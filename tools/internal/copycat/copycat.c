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

#include "copycat-priv.h"
#include "cctree-priv.h"

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/path-priv.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/nullfile.h>
#include <kfs/crc.h>
#include <klib/checksum.h>
#include <klib/writer.h>
#include <klib/log.h>
#include <klib/status.h>
#include <klib/debug.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/text.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/rc.h>
#include <klib/vector.h>

#include <strtol.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
 * some program globals
 */
const char * program_name = "copycat"; /* default it but try to set it */
const char * full_path = "copycat"; /* default it but try to set it */
int verbose = 0;
KFile *fnull;

/* cache information */
CCTree *ctree;
KDirectory *cdir;

uint32_t in_block = 0;
uint32_t out_block = 0;

CCTree *etree;
KDirectory * edir; /* extracted file base kdir */
char epath[8192];
char * ehere;
bool xml_dir = false;
bool extract_dir = false;
bool no_bzip2 = false;
bool no_md5 = false;
void * dump_out;
const char * xml_base = NULL;

char ncbi_encryption_extension[] = ".nenc";
char wga_encryption_extension[] = ".ncbi_enc";

static
KWrtWriter  log_writer;
static
KWrtWriter  log_lib_writer;
static
void * log_data;
static
void * log_lib_data;

rc_t CC copycat_log_writer (void * self, const char * buffer, size_t buffer_size,
                            size_t * num_writ)
{
    if (self)
    {
        void * bf = malloc (sizeof (SLNode) + buffer_size);
        if (bf)
        {
            const char * ps, * pc;
            size_t z;
            ps = strchr (buffer, ' ');
            if (ps)
            {
                ++ps;
                pc = strchr (ps, '-');
                if (pc == NULL)
                    pc = strchr (ps, ':');
                if (pc)
                    pc = pc + 2;
                else
                    pc = ps;
            }
            else
                pc = buffer;
            z = buffer_size - (pc - buffer);
            memmove ( (void*)(((SLNode*)bf)+1), pc, z);
            ((char*)(((SLNode*)bf)+1))[z-1] = '\0';
            SLListPushTail (self, bf);
        }
    }
    return (log_writer != NULL)
        ? log_writer (log_data, buffer, buffer_size, num_writ) : 0;
}
rc_t CC copycat_log_lib_writer  (void * self, const char * buffer, size_t buffer_size,
                                 size_t * num_writ)
{
    if (self)
    {
        void * bf = malloc (sizeof (SLNode) + buffer_size);
        if (bf)
        {
            const char * ps, * pc;
            size_t z;
            ps = strchr (buffer, ' ');
            if (ps)
            {
                ++ps;
                pc = strchr (ps, ':');
                if (pc)
                    pc = pc + 2;
                else
                    pc = ps;
            }
            else
                pc = buffer;
            z = buffer_size - (pc - buffer);
            memmove ( (void*)(((SLNode*)bf)+1), pc, z);
            ((char*)(((SLNode*)bf)+1))[z-1] = '\0';
            SLListPushTail (self, bf);
        }
    }
    if(log_lib_writer == copycat_log_lib_writer)
    {
        DEBUG_STATUS(("%s: Recursion detected in copycat_log_lib_writer!\n",__func__));
        *num_writ=buffer_size;
        return 0;
    }

    return (log_writer != NULL) ? log_writer (log_data, buffer, buffer_size, num_writ) : 0;
}

static
rc_t copycat_log_unset ()
{
    rc_t rc_l, rc_ll;

    rc_l = KLogHandlerSet (log_writer, log_data);
    rc_ll = KLogHandlerSet (log_lib_writer, log_lib_data);

    return (rc_l != 0) ? rc_l : rc_ll;
}

rc_t copycat_log_set (void * new, void ** prev)
{
    rc_t rc;

    if (prev)
        *prev = KLogDataGet();

    rc = KLogHandlerSet (copycat_log_writer, new);
    if (rc == 0)
        rc = KLogLibHandlerSet (copycat_log_lib_writer, new);

    if (rc)
        copycat_log_unset ();

    return rc;
}

/* global create mode */
KCreateMode cm = kcmParents | kcmCreate;

#define OPTION_CACHE   "cache-dir"
#define OPTION_FORCE   "force"
#define OPTION_DEST    "output"
#define OPTION_EXTRACT "extract"
#define OPTION_EXTDIR  "extract-to-dir"
#define OPTION_XMLDIR  "xml-dir"
#define OPTION_DECPWD  "decryption-password"
#define OPTION_ENCPWD  "encryption-password"
#define OPTION_XMLBASE "xml-base-node"
#define OPTION_INBLOCK "input-buffer"
#define OPTION_OUTBLOCK "output-buffer"
#define OPTION_NOBZIP2 "no-bzip2"
#define OPTION_NOMD5   "no-md5"

#define ALIAS_CACHE   "x"
#define ALIAS_FORCE   "f"
#define ALIAS_DEST    "o"
#define ALIAS_EXTRACT "e"
#define ALIAS_EXTDIR  "E"
#define ALIAS_XMLDIR  "X"
#define ALIAS_DECPWD  ""
#define ALIAS_ENCPWD  ""
#define ALIAS_XMLBASE ""
#define ALIAS_INBLOCK ""
#define ALIAS_OUTBLOCK ""
#define ALIAS_NOBZIP2 ""
#define ALIAS_NOMD5   ""



static
const char * extract_usage[] =
{ "location of extracted files", NULL };
static
const char * cache_usage[] =
{ "location of output cached files", NULL };
static
const char * force_usage[] =
{ "force overwrite of existing files", NULL };
static
const char * dest_usage[] =
{ "location of output", NULL };
static
const char * xmldir_usage[] =
{ "XML matches extracted files", NULL };
static
const char * extdir_usage[] =
{ "extracted directories match normal XML", NULL };
static
const char * xmlbase_usage[] =
{ "use this to base the XML not destination; can only be used with a single source", NULL };
static
const char * inblock_usage[] =
{ "system file reads are of blocks of this size", NULL };
static
const char * outblock_usage[] =
{ "system file writes are of blocks of this size", NULL };
static
const char * no_bzip2_usage[] =
{ "do not decompress files compressed with bzip2", NULL };
const char * no_md5_usage[] =
{ "do not calculate md5 hashes", NULL };


const char UsageDefaultName [] = "copycat";


rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s [options] src-file dst-file\n"
        "  %s [options] src-file [src-file...] dst-dir\n"
        "  %s [options] -o dst-dir src-file [src-file...]\n"
        "\n"
        "Summary:\n"
        "  Copies files and/or directories, creating a catalog of the copied files.\n",
        progname, progname, progname);
}

rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    HelpOptionLine (ALIAS_CACHE, OPTION_CACHE, "dir-path", cache_usage);
    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, force_usage);
    HelpOptionLine (ALIAS_DEST, OPTION_DEST, "file-path", dest_usage);
    HelpOptionLine (ALIAS_EXTRACT, OPTION_EXTRACT, "dir-path", extract_usage);
    HelpOptionLine (ALIAS_EXTDIR, OPTION_EXTDIR, NULL, extdir_usage);
    HelpOptionLine (ALIAS_XMLDIR, OPTION_XMLDIR, NULL, xmldir_usage);
    HelpOptionLine (ALIAS_INBLOCK, OPTION_INBLOCK, "size-in-KB", inblock_usage);
    HelpOptionLine (ALIAS_OUTBLOCK,OPTION_OUTBLOCK, "size-in-KB", outblock_usage);
    HelpOptionLine (ALIAS_NOBZIP2,OPTION_NOBZIP2, NULL, no_bzip2_usage);
    HelpOptionLine (ALIAS_NOMD5,OPTION_NOMD5, NULL, no_md5_usage);
    HelpOptionsStandard ();



/*                     1         2         3         4         5         6         7         8 */
/*            12345678901234567890123456789012345678901234567890123456789012345678901234567890 */
    OUTMSG (("Use:\n"
             " Copy and catalog:\n"
             "  Some quick examples:\n"
             "    %s dir/file.tar other-dir/file.tar\n"
             "      copy file.tar from dir to other-dir and write the catalog to stdout\n"
             "    %s dir/file.tar otherdir/\n"
             "      the same\n", progname,progname));
    OUTMSG (("    %s \"ncbi-file:dir/file.tar.nenc?encrypt&pwfile=pw other-dir.file.tar\n"
             "      copy and decrypt file.tar.nenc from dir to other-dir and catalog\n"
             "    %s dir/file.tar \"ncbi-file:other-dir/file.tar.nenc?encrypt&pwfile=pw\n"
             "      copy and encrypt file.tar from dir to other-dir/file.tar.nenc and catalog\n"
             "    %s \"ncbi-file:dir/file.tar.nenc?encrypt&pwfile=pw1 \\\n"
             "                   \"ncbi-file:other-dir/file.tar.nenc?encrypt&pwfile=pw2\n"
             "      copy the file as above while changing the encryption\n"
             "\n", progname, progname, progname));
    OUTMSG (("Use:\n"
             "  Copy source file[s] to a destination file or directory.\n"
             "  File names can either be typical path names or they can be URLs (IRLs) using\n"
             "  the standard \"file\" or extended \"ncbi-file\" schemes.\n"
             "  The catalog is XML output sent by default to stdout.\n"
             "  As UTF-8 is accepted in the paths they are IRLs for International Resource\n"
             "  Locators.\n"
             "\n"));
    OUTMSG (("  If the specified destination does not exist, there could be an ambiguity\n"
             "  whether the destination is supposed to be a file or directory.  If the\n"
             "  entered path ends in a '/' character or if there is more than one source\n"
             "  it is assumed to mean a directory and is created as such.  If neither of\n"
             "  of those apply it is assumed to be a file.\n"
             "\n"));
    OUTMSG (("  The sources or destination may also be special Unix devices:\n"
             "    /dev/stdin is supported as a source.\n"
             "    /dev/stdout and /dev/stderr is supported as a destination.\n"
             "  Other file descriptor devices can be used in the form:\n"
             "    /dev/fd/<fd-number>\n"));
    OUTMSG (("  For example /dev/stdin is synonymous with /dev/fd/0 as a source.\n"
             "  If /dev/stdout or /dev/fd/1 is used as the destination then the XML\n"
             "  output is redirected to /dev/stderr (/dev/fd/2).\n"
             "  Device /dev/null as the destination is treated as a file with only one\n"
             "  source but as a directory if more than one source.  Using this device\n"
             "  means no actual file will be copied but the cataloging will be done but\n"
             "  " OPTION_XMLBASE " must be used.\n"
             "\n"));
    OUTMSG (("  These special devices can be entered using the URL (IRL) schemes if\n"
             "  desired.  This allows the use of 'query' decorators.\n"
             "  \n"));
    OUTMSG (("  If a query is added to the URL it will need to be enclosed within '\"\'\n"
             "  characters on a command line to prevent premature interpretation.\n"
             "  The query for the 'ncbi-file' extension to the 'file' scheme allows\n"
             "  encryption and decryption.  The supported query is introduced by the\n"
             "  standard URI/IRI syntax of a '?' character with a '&' character\n"
             "  separating individual query-entries.\n"
             "\n"));
    OUTMSG (("  The supported query entries are:\n"
             "    'encrypt' or 'enc' to mean the input may be encrypted or the output\n"
             "      will be encrypted,\n"
             "    'pwfile=<path>' gives the path to file containing the password.\n"
             "    'pwfd=<FD>' gives the numerical file descriptor from which to read\n"
             "      the password,\n"
             "\n"));
    OUTMSG (("  In this program the encrypted input can apply to a file contained within\n"
             "  the source rather than just the source file itself.  The tool is fully\n"
             "  compatible with all versions of NCBI encryption.\n"
             "\n"
             "  If the output is to be encrypted only the newer FIPS compliant encryption\n"
             "  will be used and applies to the whole file.\n"
             "\n"));
    OUTMSG (("NOTE: Not all combinations of URL specifications will work at this point.\n"
             "\n"
             "NOTE: using the same file descriptor for multiple sources or overlapping with\n"
             "      stdin/stdout/stderr may cause undefined behavior including hanging the\n"
             "      the program.\n"
             "\n"));
    OUTMSG (("  The '-x' option allows small files that are typed as eligible for\n"
             "  caching to be copied to the cache directory provided. the directory\n"
             "  will be created if necessary.\n"
             "  the intent is to capture top-level files, such that files are copied\n"
             "  into the flat cache directory without regard to where they were found\n"
             "  in the input hierarchy. in the case of name conflict, output files will\n"
             "  be renamed.\n"
             "\n"));
    OUTMSG (("  To prevent internal decompression of bzipped files, use the option\n"
             "    '--no-bzip2'\n"
             "\n"));
    OUTMSG (("  To prevent calculation of MD5 hashes, use the option\n"
             "    '--no-md5'\n"
             "\n"));

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

static
OptDef Options[] =
{
    /* name            alias max times oparam required fmtfunc help text loc */
    { OPTION_EXTRACT, ALIAS_EXTRACT, NULL, extract_usage, 1, true,  false },
    { OPTION_EXTDIR,  ALIAS_EXTDIR,  NULL, extdir_usage,  0, false, false },
    { OPTION_XMLDIR,  ALIAS_XMLDIR,  NULL, xmldir_usage,  0, false, false },
    { OPTION_CACHE,   ALIAS_CACHE,   NULL, cache_usage,   1, true,  false },
    { OPTION_FORCE,   ALIAS_FORCE,   NULL, force_usage,   0, false, false },
    { OPTION_DEST,    ALIAS_DEST,    NULL, dest_usage,    1, true,  false },
    { OPTION_XMLBASE, ALIAS_XMLBASE, NULL, xmlbase_usage, 1, true,  false },
    { OPTION_INBLOCK, ALIAS_OUTBLOCK,NULL, inblock_usage, 1, true,  false },
    { OPTION_OUTBLOCK,ALIAS_OUTBLOCK,NULL, outblock_usage,1, true,  false },
    { OPTION_NOBZIP2, ALIAS_NOBZIP2, NULL, no_bzip2_usage,0, false, false },
    { OPTION_NOMD5,   ALIAS_NOMD5,   NULL, no_md5_usage,  0, false, false }
};

/* file2file
 */
static
rc_t copycat_file2file (CCTree * tree,
                        SLList * logs,
                        VFSManager * mgr,
                        VPath * _src,
                        VPath * _dst,
                        const char * leaf)
{
    size_t sz;
    rc_t rc;
    bool do_encrypt;
    bool do_decrypt;
    char spath [8192];

    do_decrypt = (VPathOption (_src, vpopt_encrypted, spath, sizeof spath, &sz) == 0);
    do_encrypt = (VPathOption (_dst, vpopt_encrypted, spath, sizeof spath, &sz) == 0);

    /* we can't use the automagical nature of the VPath and its query part
     * because copycat needs to peek under the hood; but we want the automagical
     * ability to handle it's path part.
     */

    rc = VPathReadPath (_src, spath, sizeof spath, &sz);
    if (rc)
        LOGERR (klogInt, rc, "error rereading built source path");
    else
    {
        char dpath [8192];
        size_t dz;

        rc = VPathReadPath (_dst, dpath, sizeof dpath, &dz);
        if (rc)
            LOGERR (klogInt, rc, "error rereading built source path");
        else
        {
            KDirectory * cwd;

            rc = VFSManagerGetCWD (mgr, &cwd);
            if (rc)
                LOGERR (klogInt, rc, "error pulling directory out of manager");
            else
            {
                KTime_t mtime = 0;
                bool src_dev = false;
                bool dst_dev = false;

                if (strncmp (spath, "/dev/", sizeof "/dev/"-1) == 0)
                {
                    /* get date from file system
                       [this won't be either the submitter original date
                       nor the mod-date within the file system, unless
                       the date gets reset...] */
                    mtime = time (NULL);
                    src_dev = true;
                }
                else
                {
                    rc = KDirectoryDate (cwd, &mtime, "%s", spath);
                    if (rc)
                    {
                        PLOGERR (klogErr,
                                 (klogErr, rc,
                                  "failed to determine modtime for '$(path)' continuing", "path=%s", spath ));
                        mtime = time (NULL);
                        rc = 0;
                    }
                }
                if (strncmp (dpath, "/dev/", sizeof "/dev/" - 1) == 0)
                {
                    if (strcmp(dpath, "/dev/stdout") == 0 ||
                        strcmp(dpath, "/dev/fd/1") == 0)
                    {
                        dump_out = stderr;
                    }
                    dst_dev = true;

                    if (src_dev && (xml_base == NULL))
                    {
                        rc = RC (rcExe, rcArgv, rcAccessing, rcParam, rcNull);
                        LOGERR (klogErr, rc, "Must provide " OPTION_XMLBASE
                                " when using a device stream as output");
                    }
                }
                if (rc == 0)
                {
                    char * sleaf;
                    char * dleaf;
                    char * ext;
                    VPath * src;
                    size_t xz;
                    char xpath [8192]; /* way over sized - its a leaf only */

                    sleaf = strrchr (spath, '/');
                    if (sleaf++ == NULL)
                        sleaf = spath;

                    dleaf = strrchr (dpath, '/');
                    if (dleaf++ == NULL)
                        dleaf = dpath;

                    xz = strlen (leaf);
                    memmove (xpath, leaf, xz + 1);

                    /* if we are encrypting the output make sure we have an encryption
                     * extension on the destination.
                     */
                    if (do_decrypt)
                    {
                        ext = strrchr (xpath, '.');
                        if (ext == NULL)
                            ext = xpath;
                        if ((strcmp (ext, ncbi_encryption_extension) == 0) ||
                            (strcmp (ext, wga_encryption_extension) == 0))
                            *ext = '\0';
                    }
                    else
                        ext = xpath + strlen (xpath);

                    if (do_encrypt)
                    {
                        strcpy (ext, ncbi_encryption_extension);

                        if (!dst_dev)
                        {
                            ext = strrchr (dleaf, '.');
                            if (ext == NULL)
                                ext = dleaf + strlen (dleaf);

                            if (strcmp (ext, ncbi_encryption_extension) != 0)
                                strcat (ext, ncbi_encryption_extension);
                        }
                    }

                    rc = VFSManagerMakePath (mgr, &src, "%s", spath);
                    if (rc)
                        LOGERR (klogErr, rc, "error rebuilding source path");
                    else
                    {
                        VPath * dst;

                        rc = VFSManagerMakePath (mgr, &dst, "%s", dpath);
                        if (rc)
                            LOGERR (klogErr, rc, "error rebuilding source path");
                        else
                        {

                            /* never allow overwrite of something already there */
                            if (CCTreeFind (tree, xpath) != NULL ) /* dleaf? xpath? */
                            {
                                rc = RC ( rcExe, rcFile, rcCopying, rcPath, rcExists );
                                PLOGERR ( klogInt,  (klogInt, rc, "will not overwrite "
                                                     "just-created '$(path)'", "path=%s", xpath ));
                            }
                            else
                            {
                                const KFile * sf;

                                rc = VFSManagerOpenFileRead (mgr, &sf, src);
                                if (rc)
                                    PLOGERR (klogFatal,
                                             (klogFatal, rc,
                                              "error opening input '$(P)'", "P=%s", spath));
                                else
                                {
                                    uint64_t expected;

                                    rc = KFileSize (sf, &expected);
                                    if (rc)
                                    {
                                        if (GetRCState (rc) == rcUnsupported)
                                        {
                                            expected = rcUnsupported;
                                            rc = 0;
                                        }
                                    }
                                    if (rc == 0)
                                    {
                                        KFile * df;

                                        rc = VFSManagerCreateFile (mgr, &df, false, 0640, cm, dst);
                                        if (GetRCState (rc) == rcUnauthorized)
                                        {
                                            uint32_t access;
                                            rc_t orc;

                                            orc = KDirectoryAccess (cwd, &access, "%s", dpath);
                                            if (orc == 0)
                                            {
                                                orc = KDirectorySetAccess (cwd, false, 0640, 0777, "%s", dpath);
                                                if (orc == 0)
                                                {
                                                    rc = VFSManagerCreateFile (mgr, &df, false, 0640, cm, dst);
                                                    if (rc)
                                                        KDirectorySetAccess (cwd, false, access, 0777, "%s", dpath);
                                                }
                                            }
                                        }
                                        if (rc)
                                            PLOGERR (klogErr,
                                                     (klogErr, rc, "failed to creat destination file '$(path)'",
                                                      "path=%s", dpath));
                                        else
                                        {
                                            rc_t orc;

                                            log_writer = KLogWriterGet();
                                            log_lib_writer = KLogLibWriterGet();
                                            log_data = KLogDataGet();
                                            log_lib_data = KLogLibDataGet();

                                            rc = copycat_log_set (logs, NULL);
                                            if (rc == 0)
                                            {
                                                DEBUG_STATUS (("\n-----\n%s: call copycat (tree(%p), mtime(%lu),"
                                                               " cwd(%p), _src(%p), sf(%p), _dst(%p), df(%p), "
                                                               "spath(%s), leaf(%s), expected(%lu), do_decrypt(%d)"
                                                               " do_encrypt(%d))\n\n", __func__,
                                                               tree, mtime, cwd, _src, sf, _dst, df, spath,
                                                               xpath, expected, do_decrypt, do_encrypt));
                                                rc = copycat (tree, mtime, cwd, _src, sf, _dst, df, spath,
                                                              xpath, expected, do_decrypt, do_encrypt);

                                                orc = copycat_log_unset();
                                            }

                                            if (rc)
                                                LOGERR (klogFatal, rc, "copycat function failed");
                                            else
                                                rc = orc;

                                            KFileRelease (df);
                                        }
                                    }
                                    KFileRelease (sf);
                                }
                            }
                            VPathRelease (dst);
                        }
                        VPathRelease (src);
                    }
                }
                KDirectoryRelease (cwd);
            }
        }
    }
    return rc;
}


/* files2dir
 */
static
rc_t copycat_files2dir (CCTree * tree, SLList * logs, VFSManager * mgr, Vector * v, VPath * dst)
{
    size_t dz;
    uint32_t ix;
    rc_t rc;
    char dbuff [8192];

    /* xml-base only works for a single file */
    if ((VectorLength (v) > 1) && (xml_base != NULL))
    {
        rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcIncorrect);
        LOGERR (klogErr, rc, "Can only use " OPTION_XMLBASE " with a single source file");
        return rc;
    }

    /* get the path out of the destination VPath */
    rc = VPathReadPath (dst, dbuff, sizeof dbuff, &dz);
    if (rc)
        return rc;

    for (ix = 0; ix < VectorLength (v); ++ix)
    {
        VPath * new_dst;
        VPath * src;
        char * sleaf;
        size_t sz;
        char sbuff [8192];

        src = (VPath*) VectorGet (v, ix);
        if (src == NULL) /* warn? error? abort? */
            continue;


        rc = VPathReadPath (src, sbuff, sizeof sbuff, &sz);
        if (rc)
            return rc;

        sleaf = strrchr (sbuff, '/');
        if (sleaf++ == NULL)
            sleaf = sbuff;

        /* the special case destination is the null device which we treat
         * as if it was a directory at first and then as a file
         */
        if (strcmp (dbuff, "/dev/null") == 0)
        {
            rc = VPathAddRef (dst);
            if (rc != 0)
                break;
            new_dst = dst;
        }
        else
        {
            DEBUG_STATUS(("%s: %s (%lu)\n", __func__, dbuff, dz));

            /* fix up the destination path if it's missing a final '/'
             * this is inside the loop because of the null device special case
             */
            if (dbuff [dz-1] != '/')
            {
                dbuff [dz++] = '/';
                dbuff [dz] = '\0';
            }

            /* append source leaf to destination path */
            string_copy (dbuff + dz, sizeof dbuff - dz, sleaf, strlen (sleaf));

            DEBUG_STATUS(("%s: %s\n", __func__, dbuff));

            /* make a new VPath - no URI stuff gets transferred here */
            rc = VFSManagerMakePath (mgr, &new_dst, "%s", dbuff);
            if (rc)
                break;
        }

        /* do this one file copy and catalog now */
        rc = copycat_file2file (tree, logs, mgr, src, new_dst, xml_base ? xml_base : sleaf);

        VPathRelease (new_dst);
    }
    return rc;
}


/* run
 *
 * dest will be set if the -o option was used.
 *
 */
static
rc_t copycat_run ( CCTree *tree, SLList * logs, VFSManager * mgr,
                   const char *cache, VPath * _dest, const char *extract,
                   Vector * v)
{
    rc_t rc = 0;
    int dest_type = kptNotFound;
    KDirectory * cwd = NULL;
    VPath * dest = NULL;
    size_t sz = 0;
    const char * pleaf = NULL;
    char pbuff [4096];

    /* =====
     * directories aren't yet using the VFSManager to open them
     * because we have to get more under the covers for our cataloging
     */

    rc = VFSManagerGetCWD (mgr, &cwd);
    if (rc)
        goto CLEANUP;

    /* if there's a cache path, create directory */
    if ( cache != NULL )
    {
        rc = KDirectoryCreateDir ( cwd, 0775, kcmParents | kcmOpen, "%s", cache );
        if ( rc == 0 )
            rc = KDirectoryOpenDirUpdate ( cwd, & cdir, true, "%s", cache );
        if ( rc != 0 )
        {
            PLOGERR (klogErr,
                     (klogErr, rc, "failed to open cache directory '$(path)'",
                      "path=%s", cache ));
            goto CLEANUP;
        }
    }
    else
        cdir = NULL;

    /* if there's a extract path, create directory */
    if ( extract != NULL )
    {
        rc = KDirectoryCreateDir (cwd, 0775, kcmParents | kcmOpen, "%s", extract);
        if ( rc == 0 )
            rc = KDirectoryOpenDirUpdate (cwd, & edir, true, "%s", extract);
        if ( rc != 0 )
        {
            PLOGERR (klogErr,
                     (klogErr, rc,
                      "failed to open extract directory '$(path)'",
                      "path=%s", extract ));
            goto CLEANUP;
        }
    }
    else
        edir = NULL;

    dest = _dest;
    if (dest == NULL)
    {
        rc = VectorRemove (v, VectorLength(v) - 1, (void**)&dest);
        if (rc)
            goto CLEANUP;
    }
    else
        VPathAddRef(dest);

    rc = VPathReadPath (dest, pbuff, sizeof pbuff, &sz);
    if (rc)
        goto CLEANUP;

    if (xml_base)
        pleaf = xml_base;
    else
    {
        pleaf = strrchr (pbuff, '/');
        if (pleaf++ == NULL)
            pleaf = pbuff;
    }


    /* check destination type */
    dest_type = KDirectoryPathType (cwd, "%s", pbuff);
    DEBUG_STATUS(("%s: checked destination type for '%s' got '%u'\n", __func__, pbuff, dest_type));
    switch (dest_type & ~ kptAlias)
    {
    case kptNotFound:
        /* this is the potentially ambiguous situation
         * if only two arguments and the last isn't definitively a directory
         * we assume its supposed to be a file.
         *
         * If the target does not exist but it's path ends in '/' or if
         * there is more than one source we know it is supposed to be a
         * directory.
         */
        if ((pbuff[sz-1] != '/')
/*              ((pbuff[sz-1] != '.') */



/*              ( */
/* )) */
            && (VectorLength (v) == 1))
        {
            rc = copycat_file2file (tree, logs, mgr, VectorGet (v, 0), dest, pleaf);
            goto CLEANUP;
        }

        /* create a directory at the given path */
        rc = KDirectoryCreateDir ( cwd, 0775, kcmParents | kcmOpen, "%s", pbuff );
        if ( rc != 0 )
            goto CLEANUP;

        /* fall through */
    case kptDir:
        rc = copycat_files2dir (tree, logs, mgr, v, dest);
        goto CLEANUP;


    case kptCharDev:
        /*
         * special case NULL device can act like a directory here
         * all other 'devices' we treat as a file
         */
        if ( strcmp ( pbuff, "/dev/null" ) == 0 )
        {
#if 0
            if (VectorLength (v) > 1)
#endif
            {
                rc = copycat_files2dir (tree, logs, mgr, v, dest);
                goto CLEANUP;
            }
        }
        /* fall through */
    case kptBlockDev:
    case kptFIFO:
    case kptFile:
        if (VectorLength (v) == 1) {
            rc = copycat_file2file (tree, logs, mgr, VectorGet (v, 0), dest, pleaf);
            goto CLEANUP;
        }
        rc = RC (rcExe, rcDirectory, rcAccessing, rcPath, rcNotFound);
        PLOGERR (klogFatal,
                 (klogFatal, rc, "copying multiple files, but target argument "
                  "[$(D)] is not a directory", "D=%s", pbuff));
        goto CLEANUP;
    }

    fprintf ( stderr, "%s: '%s': specified destination path is not a directory\n", program_name, pbuff );
    rc = RC ( rcExe, rcDirectory, rcAccessing, rcPath, rcIncorrect );
    
CLEANUP:
    VPathRelease(dest);
    return rc;
}
/* dump
 */
static
rc_t copycat_fwrite ( void *out, const void *buffer, size_t bytes )
{
    size_t writ = fwrite ( buffer, 1, bytes, out );
    if ( writ != bytes )
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}

static
rc_t copycat_dump ( const CCTree *tree, SLList * logs )
{
    return CCTreeDump ( tree, copycat_fwrite, dump_out, logs );
}

static
void param_whack (void * path, void * ignored)
{
    (void)VPathRelease ((const VPath*)path);
}

MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
         return VDB_INIT_FAILED;

    Args * args;
    rc_t rc, orc;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    KStsHandlerSetStdErr();
    KStsLibHandlerSetStdErr();

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof Options / sizeof (OptDef));
    if (rc == 0)
    {
        do
        {
            const char * dest;
            const char * cache;
            const char * extract = NULL;

            uint32_t pcount;
            CCTree * tree;
            VFSManager * mgr = NULL;
            VPath * dp = NULL;
            Vector params = { NULL, 0, 0, 0};
            uint32_t ix;

            rc = ArgsProgram (args, &full_path, &program_name);
            if (rc)
                break;

            extract_dir = false;
            xml_dir = false;
            memset (epath, 0, sizeof (epath));
            ehere = epath;

            rc = ArgsOptionCount (args, OPTION_CACHE, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                rc = ArgsOptionValue (args, OPTION_CACHE, 0, (const void **)&cache);
                if (rc)
                    break;
            }
            else
                cache = NULL;

            rc = ArgsOptionCount (args, OPTION_DEST, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                rc = ArgsOptionValue (args, OPTION_DEST, 0, (const void **)&dest);
                if (rc)
                    break;
            }
            else
            {
                dest = NULL;
            }

            rc = ArgsOptionCount (args, OPTION_EXTRACT, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                rc = ArgsOptionValue (args, OPTION_EXTRACT, 0, (const void **)&extract);
                if (rc)
                    break;
            }

            rc = ArgsOptionCount (args, OPTION_EXTDIR, &pcount);
            if (rc)
                break;
            extract_dir = pcount > 0;

            rc = ArgsOptionCount (args, OPTION_XMLDIR, &pcount);
            if (rc)
                break;
            xml_dir = pcount > 0;

            rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
            if (rc)
                break;
            if (pcount)
                cm = kcmParents | kcmInit;

            rc = ArgsOptionCount (args, OPTION_XMLBASE, &pcount);
            if (pcount == 1)
            {
                rc = ArgsOptionValue (args, OPTION_XMLBASE, 0, (const void **)&xml_base);
                if (rc)
                    break;

                /* we might want a few more checks here... */
                if (strchr (xml_base, '/') != NULL)
                {
                    rc = RC (rcExe, rcArgv, rcAccessing, rcParam, rcInvalid);
                    break;
                }
            }

            rc = ArgsOptionCount (args, OPTION_INBLOCK, &pcount);
            if (pcount == 1)
            {
                const char * start;
                char * end;
                uint32_t val;

                rc = ArgsOptionValue (args, OPTION_INBLOCK, 0, (const void **)&start);
                if (rc)
                    break;

                val = strtou32 (start, &end, 10);

                if (*end != '\0')
                {
                    rc = RC (rcExe, rcArgv, rcAccessing, rcParam, rcInvalid);
                    break;
                }
                in_block = val * 1024;
            }

            rc = ArgsOptionCount (args, OPTION_OUTBLOCK, &pcount);
            if (pcount == 1)
            {
                const char * start;
                char * end;
                uint32_t val;

                rc = ArgsOptionValue (args, OPTION_OUTBLOCK, 0, (const void **)&start);
                if (rc)
                    break;

                val = strtou32 (start, &end, 10);

                if (*end != '\0')
                {
                    rc = RC (rcExe, rcArgv, rcAccessing, rcParam, rcInvalid);
                    break;
                }
                out_block = val * 1024;
            }

            rc = ArgsOptionCount ( args, OPTION_NOBZIP2, & pcount );
            if ( pcount > 0 )
            {
                no_bzip2 = true;
            }

            rc = ArgsOptionCount ( args, OPTION_NOMD5, & pcount );
            if ( pcount > 0 )
            {
                no_md5 = true;
            }

            /* all parameters plus the possible dest option parameter */
            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;

            if (pcount == 0)
            {
                rc = RC ( rcExe, rcArgv, rcReading, rcParam, rcInsufficient );
                MiniUsage (args);
                break;
            }

            if ((dest == NULL) && (extract == NULL) && (pcount < 2))
            {
                rc = RC ( rcExe, rcArgv, rcReading, rcParam, rcInvalid );
                if (pcount)
                    LOGERR (klogFatal, rc, "missing source and destination arguments\n");
                else
                    LOGERR (klogFatal, rc, "missing destination argument[s]\n");
                break;
            }

            VectorInit (&params, 0, 8); /* 8 is arbirary - seems long enough for no realloc */

            rc = VFSManagerMake (&mgr);
            if (rc)
            {
                LOGERR (klogFatal, rc,
                        "unable to build file system manager");
                break;
            }

            for (ix = 0; ix < pcount; ++ix)
            {
                VPath * kp;
                const char * pc;

                rc = ArgsParamValue (args, ix, (const void **)&pc);
                if (rc)
                {
                    LOGERR (klogFatal, rc, "unable to extract path parameter");
                    break;
                }

                rc = VFSManagerMakePath (mgr, &kp, "%s", pc);
                if (rc)
                {
                    LOGERR (klogFatal, rc, "unable to build path parameter");
                    break;
                }

                rc = VectorSet (&params, ix, kp);
                if (rc)
                {
                    LOGERR (klogFatal, rc, "unable to stow path parameter");
                    break;
                }
            }
            if (rc == 0)
            {
                if (dest)
                {
                    rc = VFSManagerMakePath (mgr, &dp, "%s", dest);
                    if (rc)
                    {
                        LOGERR (klogFatal, rc, "unable to build dest parameter");
                        break;
                    }
                }
                DEBUG_STATUS(("%s: Create file tree\n", __func__));
                rc = CCTreeMake (&tree);
                if (rc)
                {
                    LOGERR ( klogInt, rc, "failed to create parse tree" );
                }
                else
                {
                    DEBUG_STATUS(("%s: Create cache file tree\n", __func__));

                    rc = CCTreeMake (&ctree);
                    if (rc)
                    {
                        LOGERR ( klogInt, rc, "failed to create cache tree" );
                    }
                    else
                    {
                        DEBUG_STATUS(("%s: Create extracted file tree\n",
                                      __func__));

                        rc = CCTreeMake (&etree);
                        if (rc)
                        {
                            LOGERR ( klogInt, rc,
                                     "failed to create extract tree" );
                        }
                        else
                        {
                            DEBUG_STATUS(("%s: Create  NULL output file\n",
                                          __func__));
                            rc = KFileMakeNullUpdate (&fnull);
                            if (rc)
                                LOGERR (klogInt, rc,
                                        "failed to create null output");
                            else
                            {
                                DEBUG_STATUS(("%s: Open File Format Tester\n",
                                              __func__));

                                rc = CCFileFormatMake ( & filefmt );
                                if ( rc != 0 )
                                    LOGERR (klogInt, rc,
                                            "failed to create file format" );
                                else
                                {
                                    SLList logs;

                                    DEBUG_STATUS(("%s: Initialize CRC32\n",
                                                  __func__));

                                    SLListInit (&logs);
                                    CRC32Init ();

                                    DEBUG_STATUS(("%s: Copy and catalog\n",
                                                  __func__));

                                    dump_out = stdout; /* kludge */

                                    rc = copycat_run (tree, &logs, mgr, cache,
                                                      dp, extract, &params);
                                    if ( rc == 0 )
                                        rc = copycat_dump ( xml_dir ? etree : tree, &logs );
                                    DEBUG_STATUS(("%s: Output XML\n", __func__));


                                    CCFileFormatRelease ( filefmt );
                                }

                                DEBUG_STATUS(("%s: Release NULL output file\n", __func__));

                                orc = KFileRelease ( fnull ), fnull = NULL;
                                if (rc == 0)
                                    rc = orc;
                            }
                            DEBUG_STATUS(("%s: Whack extracted file tree;\n", __func__));
                            CCTreeWhack (etree);
                        }
                        DEBUG_STATUS(("%s: Whack cache file tree;\n", __func__));
                        CCTreeWhack (ctree);
                    }
                    DEBUG_STATUS(("%s: Whack file tree;\n", __func__));
                    CCTreeWhack (tree);
                }

                VPathRelease (dp);
            }
            VFSManagerRelease (mgr);
            VectorWhack (&params, param_whack, NULL);
        } while (0);
    }
    ArgsWhack (args);
    orc = KDirectoryRelease (cdir); /* class extren should be NULL if never used */
    if (orc)
    {
        LOGERR (klogInt, rc, "Error shutting file system access");
        if (rc == 0)
            rc = orc;
    }
    DEBUG_STATUS(("%s: exit rc %R(%x);\n", __func__, rc, rc));
    return VdbTerminate( rc );
}

