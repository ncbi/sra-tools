/*===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <kfs/directory.h>

#define FUSE_USE_VERSION 25
#include <fuse.h>

#include "xml.h"
#include "sra-fuser.h"
#include "log.h"

#include <atomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

static struct stat g_mount_point_stat;
static struct stat g_dflt_file_stat;
static atomic64_t num_open_files;

static
int ConvertRC2errno(rc_t rc)
{
    switch(GetRCState(rc)) {
        case rcNoErr:
            return 0;
        case rcNotFound:
            return ENOENT;
        case rcNull:
            return EFAULT;
        case rcInvalid:
            return EINVAL;
        case rcInsufficient:
            return ENAMETOOLONG;
        case rcReadonly:
            return EROFS;
        case rcUnauthorized:
            return EACCES;
        case rcCorrupt:
        default:
            return EBADF;
    }
}

void* UX_FUSE_init(void)
{
    atomic64_set(&num_open_files, 0);
    SRA_FUSER_Init();
    return NULL;
}

void UX_FUSE_destroy(void* x)
{
    uint64_t q = atomic64_read(&num_open_files);
    if( q > 0 ) {
        PLOGMSG(klogInfo, (klogInfo, "$(q) files still opened", PLOG_U64(q), q));
    }
    SRA_FUSER_Fini();
}

struct UX_FUSE_readdir_callback_data {
    const char *path;
    void *buf;
    fuse_fill_dir_t filler;
};

static
rc_t CC UX_FUSE_readdir_callback( const char *name, void *data )
{
    struct UX_FUSE_readdir_callback_data* d = (struct UX_FUSE_readdir_callback_data*)data;
    int r = d->filler(d->buf, name, NULL, 0);
    DEBUG_MSG(10, ("%s %s entry: '%s'\n", __func__, d->path, name));
    return r != 0 ? RC(rcExe, rcDirectory, rcReading, rcBuffer, rcInsufficient) : 0;
}

int UX_FUSE_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                     off_t offset, struct fuse_file_info *fi)
{
    rc_t rc = 0;
    struct UX_FUSE_readdir_callback_data data;

    DEBUG_MSG(8, ("%s: %s\n", __func__, path));

    data.path = path;
    data.filler = filler;
    data.buf = buf;

    if( (rc = UX_FUSE_readdir_callback(".", &data)) == 0 &&
        (rc = UX_FUSE_readdir_callback("..", &data)) == 0 ) {
        rc = SRA_FUSER_GetDir(path, UX_FUSE_readdir_callback, &data);
    }
    if( rc != 0 ) {
        errno = ConvertRC2errno(rc);
        PLOGERR(klogErr, (klogErr, rc, "$(f): $(p) - $(e)", PLOG_3(PLOG_S(f),PLOG_S(p),PLOG_S(e)), __func__, path, strerror(errno)));
        return -errno;
    }
    return 0;
}

int UX_FUSE_getattr(const char *path, struct stat *stbuf)
{
    rc_t rc = 0;

    DEBUG_MSG(8, ("%s: %s\n", __func__, path));
    if( stbuf == NULL) {
        rc = RC(rcExe, rcFileDesc, rcClassifying, rcParam, rcNull);
    } else if( strcmp(path, "/") == 0 ) {
        /* root is known as mount point */
        memcpy(stbuf, &g_mount_point_stat, sizeof(g_mount_point_stat));
    } else {
        uint32_t type = kptBadPath, access = 0;
        KTime_t ts = 0;
        uint64_t file_sz = 0, block_sz = 0;
        if( (rc = SRA_FUSER_GetAttr(path, &type, &ts, &file_sz, &access, &block_sz)) == 0 ) {
            bool symlink = (type & kptAlias);
            if( symlink ) {
                type = type & ~kptAlias;
            }
            if( type == kptDir ) {
                memcpy(stbuf, &g_mount_point_stat, sizeof(g_mount_point_stat));
                stbuf->st_mode = S_IFDIR | (0007555 & (access == 0 ? stbuf->st_mode : access));
            } else {
                memcpy(stbuf, &g_dflt_file_stat, sizeof(g_dflt_file_stat));
                if( access == 0 ) {
                    access = stbuf->st_mode;
                }
                stbuf->st_mode = 0007555 & (access == 0 ? stbuf->st_mode : access);
                if( type == kptFile ) {
                    stbuf->st_mode |= S_IFREG;
                } else if( type == kptCharDev ) {
                    stbuf->st_mode |= S_IFCHR;
                } else if( type == kptBlockDev ) {
                    stbuf->st_mode |= S_IFBLK;
                } else if( type == kptFIFO ) {
                    stbuf->st_mode |= S_IFIFO;
                } else {
                    rc = RC(rcExe, rcFileDesc, rcClassifying, rcDirEntry, rcUnknown);
                }
            }
            if( rc == 0 ) {
                if( symlink ) {
                    stbuf->st_mode = S_IFLNK | (stbuf->st_mode & 07777);
                }
                stbuf->st_size = file_sz;
                if( ts != 0 ) {
                    stbuf->st_mtime = stbuf->st_atime = stbuf->st_ctime = ts;
                }
                if( block_sz > 0 ) {
                    stbuf->st_blksize = block_sz;
                }
                DEBUG_MSG(8, ("%s: %s type: %s %lu bytes\n", __func__, path,
                    (S_ISDIR(stbuf->st_mode) ? "dir" : (S_ISLNK(stbuf->st_mode) ? " symlink" : "file")), stbuf->st_size));
            }
        }
    }
    if( rc != 0 ) {
        errno = ConvertRC2errno(rc);
        PLOGERR(klogErr, (klogErr, rc, "$(f): $(p) - $(e)", PLOG_3(PLOG_S(f),PLOG_S(p),PLOG_S(e)), __func__, path, strerror(errno)));
        return -errno;
    }
    return 0;
}

int UX_FUSE_readlink(const char *path, char *buf, size_t buf_sz)
{
    rc_t rc = 0;

    DEBUG_MSG(8, ("%s: %s\n", __func__, path));
    if( buf == NULL ) {
        rc = RC(rcExe, rcFile, rcAliasing, rcParam, rcNull);
    } else if( buf_sz < 1 ) {
        rc = RC(rcExe, rcFile, rcAliasing, rcParam, rcInvalid);
    } else {
        rc = SRA_FUSER_ResolveLink(path, buf, buf_sz);
    }
    if( rc != 0 ) {
        errno = ConvertRC2errno(rc);
        PLOGERR(klogErr, (klogErr, rc, "$(f): $(p) - $(e)", PLOG_3(PLOG_S(f),PLOG_S(p),PLOG_S(e)), __func__, path, strerror(errno)));
        return -errno;
    }
    return 0;
}

int UX_FUSE_open(const char *path, struct fuse_file_info* fi)
{
    rc_t rc = 0;
    uint64_t q;

    DEBUG_MSG(8, ("%s: %s\n", __func__, path));
    if( fi == NULL) {
        rc = RC(rcExe, rcFile, rcOpening, rcParam, rcNull);
    } else if( fi->flags & (O_CREAT | O_EXCL | O_TRUNC | O_APPEND)) { 
        rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcReadonly);
    } else {
        const void* data = NULL;
        if( (rc = SRA_FUSER_OpenNode(path, &data)) == 0 ) {
            fi->fh = (uint64_t)data;
        }
    }
    if( rc != 0 ) {
        errno = ConvertRC2errno(rc);
        PLOGERR(klogErr, (klogErr, rc, "$(f): $(p) - $(e)", PLOG_3(PLOG_S(f),PLOG_S(p),PLOG_S(e)), __func__, path, strerror(errno)));
        return -errno;
    }
    q = atomic64_add_and_read(&num_open_files, 1);
    PLOGMSG(klogInfo, (klogInfo, "opened $(n), total open $(q)", PLOG_2(PLOG_S(n),PLOG_U64(q)), path, q));
    return 0;
}

int UX_FUSE_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    rc_t rc = 0;
    const void* data = NULL;
    size_t num_read = 0;

    DEBUG_MSG(8, ("%s: %s from %lu %lu bytes\n", __func__, path, offset, size));
    if( fi == NULL || buf == NULL ) {
        rc = RC(rcExe, rcFile, rcReading, rcParam, rcNull);
    } else if( (data = (const void*)fi->fh) == NULL ) {
        rc = RC(rcExe, rcFile, rcReading, rcParam, rcCorrupt);
    } else {
        rc = SRA_FUSER_ReadNode(path, data, buf, size, offset, &num_read);
    }
    if( rc != 0 ) {
        errno = ConvertRC2errno(rc);
        PLOGERR(klogErr, (klogErr, rc, "$(f): $(p) - $(e)", PLOG_3(PLOG_S(f),PLOG_S(p),PLOG_S(e)), __func__, path, strerror(errno)));
        return -errno;
    }
    return num_read;
}

int UX_FUSE_release(const char *path, struct fuse_file_info *fi)
{
    rc_t rc = 0;
    const void* data = NULL;
    uint64_t q;

    DEBUG_MSG(8, ("%s: %s\n", __func__, path));
    if( fi == NULL) {
        rc = RC(rcExe, rcFile, rcReleasing, rcParam, rcNull);
    } else if( (data = (const void*)fi->fh) == NULL ) {
        rc = RC(rcExe, rcFile, rcReading, rcParam, rcCorrupt);
    } else {
        rc = SRA_FUSER_CloseNode(path, data);
    }
    fi->fh = 0;
    if( rc != 0 ) {
        errno = ConvertRC2errno(rc);
        PLOGERR(klogErr, (klogErr, rc, "$(f): $(p) - $(e)", PLOG_3(PLOG_S(f),PLOG_S(p),PLOG_S(e)), __func__, path, strerror(errno)));
        return -errno;
    }
    atomic64_dec(&num_open_files);
    q = atomic64_read(&num_open_files);
    PLOGMSG(klogInfo, (klogInfo, "closed $(n), total open $(q)", PLOG_2(PLOG_S(n),PLOG_U64(q)), path, q));
    return 0;
}

int UX_FUSE_mknod(const char *path, mode_t m, dev_t d)
{
    return -EROFS;
}

int UX_FUSE_mkdir(const char *path, mode_t m)
{
    return -EROFS;
}

int UX_FUSE_unlink(const char *path)
{
    return -EROFS;
}

int UX_FUSE_rmdir(const char *path)
{
    return -EROFS;
}

int UX_FUSE_symlink(const char *path, const char *x)
{
    return -EROFS;
}

int UX_FUSE_rename(const char *path, const char *x)
{
    return -EROFS;
}

int UX_FUSE_link(const char *path, const char *x)
{
    return -EROFS;
}

int UX_FUSE_chmod(const char *path, mode_t m)
{
    return -EROFS;
}

int UX_FUSE_chown(const char *path, uid_t u, gid_t g)
{
    return -EROFS;
}

int UX_FUSE_truncate(const char *path, off_t o)
{
    return -EROFS;
}

int UX_FUSE_utime(const char *path, struct utimbuf *b)
{
    return -EROFS;
}

int UX_FUSE_write(const char *path, const char *b, size_t s, off_t o, struct fuse_file_info *fi)
{
    return -EROFS;
}

int UX_FUSE_flush(const char *path, struct fuse_file_info *fi)
{
    return 0;
}

int UX_FUSE_create(const char *path, mode_t m, struct fuse_file_info *fi)
{
    return -EROFS;
}

int UX_FUSE_ftruncate(const char *path, off_t o, struct fuse_file_info *fi)
{
    return -EROFS;
}

static
void CoreUsage(int fd, const char *progName, bool showHelp, bool showVersion, bool fail)
{
    /* used only for FUSE built-in help and version printing */
    struct fuse_operations ops;
    struct fuse_args args;
    memset(&args, 0, sizeof(struct fuse_args));
    fuse_opt_add_arg(&args, progName); /* fake mount point */


    if( fd != STDOUT_FILENO ) {
        /* redirect usage to log file if it was specified */
        dup2(fd, STDOUT_FILENO);
    }
    if( showHelp ) {
        const char* p = strrchr(progName, '/');
        if( p++ == NULL ) {
            p = progName;
        }
        UsageSummary(p);
        if( !fail ) {
            fuse_opt_add_arg(&args, "-ho");
            KOutMsg("\n"
                "    -x|--xml-dir <path>                XML file with virtual directory structure\n"
                "    -m|--mount-point <path>            path to a mount directory \n"
                "    -u|--unmount                       Unmount only and exit (only -m required)\n"
                );
            KOutMsg("\nOptions:\n"
                "    -c|--xml-check <secs>              Check XML for update every <arg> seconds,\n"
                "                                       default: 0 - never.\n"
                "    -r|--xml-root  <path>              Base directory for a 'path' attributes in XML.\n"
                "                                       default: '.'\n"
                );
            KOutMsg(
                "    -i|--xml-validate <nocheck|ignore> XML validation on load:\n"
                "                                       nocheck - do not check presence of dir/file in path attribute;\n"
                "                                       ignore - only report missing dir/file in path attribute;\n"
                "                                       default behaivour is to fail loading XML if dir/file is not found.\n"
                );
            KOutMsg(
                "    --SRA-check <secs>                 Check SRA config and runs for update\n"
                "                                       every <arg> seconds, default: 0 - never.\n"
                "    --SRA-cache <path>                 Write SRA update info to a file.\n"
                "                                       Must have --SRA-check option value of non-zero.\n"
                );
            KOutMsg(
                "    -L|--log-level                     Logging level as number or enum string. One\n"
                "                                       of (fatal|sys|int|err|warn|info) or (0-5)\n"
                "                                       Current/default is warn.\n"
                "    -l|--log-file <path>               Use log file specified by path.\n"
                "    -g|--log-reopen <secs>             Reopened log file every <arg> seconds\n"
                "                                       (external log rotation), default: 0 - never.\n"
                );
            KOutMsg(
    #if _DEBUGGING
                "    -+|--debug <Module[-Flag]>         Turn on debug output for module. All flags\n"
                "                                       if not specified.\n"
    #endif
                "    -v|--verbose                       Increase the verbosity level of the program.\n"
                "                                       Use multiple times for more verbosity.\n"
                "    -V|--version                       Display the version of the program then quit.\n"
                "    -h|--help                          Output brief explantion for the program.\n"
                "\n"
                );
        }
    }
    if( showVersion && !fail ) {
        HelpVersion(progName, KAppVersion());
        fuse_opt_add_arg(&args, "--version");
    }
    /* force help preceed fuse help */
    fflush(stdout);
    /* hack to force fuse lib to output to stdout */
    dup2(fd, STDERR_FILENO);
    if( !fail ) {
        memset(&ops, 0, sizeof(struct fuse_operations));
        fuse_main(args.argc, args.argv, &ops);
    }
    exit(fail ? rcArgv : 0);
}

/*******************************************************************************
 * KMain - defined for use with kapp library
 *******************************************************************************/
rc_t CC KMain(int argc, char *argv[])
{
    int i;
    rc_t rc;

    bool showHelp = argc < 2, showVersion = false, unmount = false, foreground = false;
    const char* mount_point = NULL, *xml_path = NULL, *log_file = NULL;
    const char* sra_cache = NULL, *xml_root = ".";
    char** fargs = (char**)calloc(argc, sizeof(char*));
    uint32_t xml_sync = 0, log_sync = 0, sra_sync = 0;
    EXMLValidate xml_validate = eXML_Full;
    int log_fd = STDOUT_FILENO;

#ifdef SRAFUSER_LOGLOCALTIME
    KLogFmtFlagsSet(klogFmtLocalTimestamp);
    KLogLibFmtFlagsSet(klogFmtLocalTimestamp);
    KStsFmtFlagsSet(kstsFmtLocalTimestamp);
    KStsLibFmtFlagsSet(kstsFmtLocalTimestamp);
#endif

    for(i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "-x") || !strcmp(argv[i], "--xml-dir")) {
            xml_path = argv[++i];
        } else if(!strcmp(argv[i], "-m") || !strcmp(argv[i], "--mount-point")) {
            mount_point = argv[++i];
        } else if(!strcmp(argv[i], "-xs") || !strcmp(argv[i], "-c") || !strcmp(argv[i], "--xml-check")) {
            xml_sync = AsciiToU32(argv[++i], NULL, NULL);
        } else if(!strcmp(argv[i], "-r") || !strcmp(argv[i], "--xml-root")) {
            xml_root = argv[++i];
        } else if(!strcmp(argv[i], "-i") || !strcmp(argv[i], "--xml-validate")) {
            if( i++ == argc - 1 ) {
                rc = RC(rcExe, rcArgv, rcValidating, rcParam, rcInsufficient);
                LOGERR(klogErr, rc, "XML validation setting value");
                CoreUsage(log_fd, argv[0], true, false, true);
            } else if( !strcmp(argv[i], "ignore") ) {
                xml_validate = eXML_NoFail;
            } else if( !strcmp(argv[i], "nocheck") ) {
                xml_validate = eXML_NoCheck;
            } else {
                rc = RC(rcExe, rcArgv, rcValidating, rcParam, rcUnrecognized);
                PLOGERR(klogErr, (klogErr, rc, "XML validation setting value '$(lvl)'", PLOG_S(lvl), argv[i]));
                CoreUsage(log_fd, argv[0], true, false, true);
            }
        } else if(!strcmp(argv[i], "-ds") || !strcmp(argv[i], "--SRA-check")) {
            sra_sync = AsciiToU32(argv[++i], NULL, NULL);
        } else if(!strcmp(argv[i], "-df") || !strcmp(argv[i], "--SRA-cache")) {
            sra_cache = argv[++i];
        } else if(!strcmp(argv[i], "-u") || !strcmp (argv[i], "--unmount")) {
            unmount = true;
        } else if(!strcmp(argv[i], "-L") || !strcmp (argv[i], "--log-level")) {
            if( i == argc - 1 ) {
                rc = RC(rcExe, rcArgv, rcValidating, rcParam, rcInsufficient);
                LOGERR(klogErr, rc, "missing log level");
                CoreUsage(log_fd, argv[0], true, false, true);
            } else if( (rc = LogLevelSet(argv[++i])) != 0 ) {
                PLOGERR(klogErr, (klogErr, rc, "log level $(lvl)", PLOG_S(lvl), argv[i]));
                CoreUsage(log_fd, argv[0], true, false, true);
            }
        } else if(!strcmp(argv[i], "-+") || !strcmp (argv[i], "--debug")) {
#ifdef _DEBUGGING
            if( i == argc - 1 ) {
                rc = RC(rcExe, rcArgv, rcValidating, rcParam, rcInsufficient);
                LOGERR(klogErr, rc, "missing debug level");
                CoreUsage(log_fd, argv[0], true, false, true);
            } else if( (rc = KDbgSetString(argv[++i])) != 0 ) {
                PLOGERR(klogErr, (klogErr, rc, "debug level $(lvl)", PLOG_S(lvl), argv[i]));
                CoreUsage(log_fd, argv[0], true, false, true);
            }
#else
            i++;
#endif
        } else if(!strcmp(argv[i], "-lf") || !strcmp(argv[i], "-l") || !strcmp (argv[i], "--log-file")) {
            log_file = argv[++i];
        } else if(!strcmp(argv[i], "-ls") || !strcmp(argv[i], "-g") || !strcmp(argv[i], "--log-reopen")) {
            log_sync = AsciiToU32(argv[++i], NULL, NULL);
        } else if(!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
            showVersion = true;
        } else if(!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            KStsLevel l = KStsLevelGet();
            KStsLevelSet(++l);
        } else if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "-ho") || !strcmp(argv[i], "--help")) {
            showHelp = true;
        } else {
            /* save arg for FUSE */
            fargs[i] = argv[i];
            if( !strcmp(argv[i], "-d") || !strcmp(argv[i], "-f") ||
                (!strcmp(argv[i], "-o") && (i > argc - 1) && !strcmp(argv[i + 1], "debug")) ) {
                foreground = true;
            }
        }
    }
    if( showHelp || showVersion ) {
        CoreUsage(log_fd, argv[0], showHelp, showVersion, false);
    }
    if( (rc = LogFile_Init(log_file, log_sync, foreground, &log_fd)) != 0 ) {
        LOGERR(klogErr, rc, log_file ? log_file : "no log");
        CoreUsage(log_fd, argv[0], true, false, true);
    }
    if( mount_point == NULL ) {
        LOGERR(klogErr, RC(rcExe, rcArgv, rcValidating, rcParam, rcInsufficient), "mountpoint");
        CoreUsage(log_fd, argv[0], true, false, true);
    }
    if( unmount ) {
        fuse_unmount(mount_point);
        exit(0);
    }
    if( xml_path == NULL ) {
        LOGERR(klogErr, RC(rcExe, rcArgv, rcValidating, rcParam, rcInsufficient), "virtual directory XML");
        CoreUsage(log_fd, argv[0], true, false, true);
    }
    if( i != argc ) {
        LOGERR(klogErr, RC(rcExe, rcArgv, rcValidating, rcParam, rcExcessive), argv[i]);
        CoreUsage(log_fd, argv[0], true, false, true);
    }
    if( stat(mount_point, &g_mount_point_stat) < 0 ) {
        PLOGMSG(klogErr, (klogErr, "$(p): $(e)", PLOG_2(PLOG_S(p),PLOG_S(e)), mount_point, strerror(errno)));
        CoreUsage(log_fd, argv[0], true, false, true);
    }
    g_mount_point_stat.st_dev = 0;
    g_mount_point_stat.st_ino = 0;
    g_mount_point_stat.st_mode = S_IFDIR | 0555; /* execute read-only */
     /* find needs more links to search in subdir */
    g_mount_point_stat.st_nlink = 1024 * 1024 * 1024;
    
    if( stat(xml_path, &g_dflt_file_stat) < 0 ) {
        PLOGMSG(klogErr, (klogErr, "$(p): $(e)", PLOG_2(PLOG_S(p),PLOG_S(e)), xml_path, strerror(errno)));
        CoreUsage(log_fd, argv[0], true, false, true);
    }
    g_dflt_file_stat.st_dev = 0;
    g_dflt_file_stat.st_ino = 0;
    g_dflt_file_stat.st_mode = S_IFREG | 0444; /* read-only */
    g_dflt_file_stat.st_nlink = 1;
    g_dflt_file_stat.st_rdev = 0;
    g_dflt_file_stat.st_size = 0;
    g_dflt_file_stat.st_blksize = 0;
    g_dflt_file_stat.st_blocks = 0;

    if( (rc = Initialize(sra_sync, xml_path, xml_sync, sra_cache, xml_root, xml_validate)) != 0 ) {
        LOGERR(klogErr, rc, "at initialization");
        CoreUsage(log_fd, argv[0], true, false, true);
    }
    DEBUG_MSG(8, ("Mount point set to '%s'\n", mount_point));

    {{ /* FUSE start */
        struct fuse_operations ops;
        struct fuse_args args;

        memset(&args, 0, sizeof(struct fuse_args));
        fuse_opt_add_arg(&args, argv[0]);
        /* mount point for fuse_main */
        fuse_opt_add_arg(&args, mount_point);
        /* save mopunt point dir and program stat */
        for(i = 0; i < argc; i++) {
            if( fargs[i] ) {
                fuse_opt_add_arg(&args, fargs[i]);
            }
        }
        free(fargs);
        memset(&ops, 0, sizeof(struct fuse_operations));
        ops.init     = UX_FUSE_init;
        ops.destroy  = UX_FUSE_destroy;
        ops.getattr  = UX_FUSE_getattr;
        ops.readdir  = UX_FUSE_readdir;
        ops.readlink = UX_FUSE_readlink;
        ops.open     = UX_FUSE_open;
        ops.read     = UX_FUSE_read;
        ops.release  = UX_FUSE_release;
        ops.mknod = UX_FUSE_mknod;
        ops.mkdir = UX_FUSE_mkdir;
        ops.unlink = UX_FUSE_unlink;
        ops.rmdir = UX_FUSE_rmdir;
        ops.symlink = UX_FUSE_symlink;
        ops.rename = UX_FUSE_rename;
        ops.link = UX_FUSE_link;
        ops.chmod = UX_FUSE_chmod;
        ops.chown = UX_FUSE_chown;
        ops.truncate = UX_FUSE_truncate;
        ops.utime = UX_FUSE_utime;
        ops.write = UX_FUSE_write;
        ops.flush = UX_FUSE_flush;
        ops.create = UX_FUSE_create;
        ops.ftruncate = UX_FUSE_ftruncate;
        rc = fuse_main(args.argc, args.argv, &ops);
    }}
    return rc;
}
