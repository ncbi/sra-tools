/*==============================================================================
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

#include "shared.h"

#include <klib/defs.h>
#include <klib/callconv.h>

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/log.h>
#include <klib/debug.h> /* DBGMSG */
#include <klib/status.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/namelist.h>

#include <kfs/defs.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/sra.h>
#include <kfs/lockfile.h>
#include <kfs/cacheteefile.h>
#include <kfs/buffile.h>
#include <vfs/manager.h>

#include <krypto/key.h>
#include <krypto/encfile.h>
#include <krypto/wgaencrypt.h>

#include <kapp/args.h>
#include <kapp/main.h>

#include <assert.h>
#include <string.h>
#include <stdint.h>

#ifndef RIGOROUS_SRA_CHECK
#define RIGOROUS_SRA_CHECK 0
#endif

#define OPTION_FORCE   "force"
#define OPTION_SRA     "decrypt-sra-files"
#define ALIAS_FORCE    "f"
#define ALIAS_SRA      NULL

#define MY_MAX_PATH        4096


bool ForceFlag = false;
bool TmpFoundFlag = false;
bool UseStdin = false;
bool UseStdout = false;
bool IsArchive = false;

/* for wga decrypt */
char Password [4096 + 2];
size_t PasswordSize;

/* for encfile encrypt/decrypt */
KKey Key;

const char * ForceUsage[] = 
{ "Force overwrite of existing files", NULL };

/*
 * option  control flags
 */

const char EncExt[] = ".ncbi_enc";
static const char TmpExt[] = ".vdb-decrypt-tmp";
static const char TmpLockExt[] = ".vdb-decrypt-tmp.lock";
static const char CacheExt[] = ".cache";
static const char CacheLockExt[] = ".cache.lock";

/* Usage
 */
rc_t CC UsageSummary (const char * progname)
{
    rc_t rc;
    {
        rc = KOutMsg (
            /*345679012345678901234567890123456789012345678901234567890123456789012345678*/
            "\n"
            "Usage:\n"
            "  %s [options] <source-file>\n"
            "  %s [options] <source-file> <destination-file>\n"
            "  %s [options] <source-file> <destination-directory>\n"
            "  %s [options] <directory>\n",
            progname, progname, progname, progname);
    }
#if DIRECTORY_TO_DIRECTORY_SUPPORTED
    if (rc == 0)
        rc = KOutMsg (
            "  %s [options] <source-directory> <destination-directory>\n",
            progname);
#endif
        if (rc == 0)
    {
        rc = KOutMsg (
            "\n"
            "Summary:\n"
            "  %scrypt a file or all the files (recursively) in a directory\n\n",
            De);
    }
    return rc;
}

rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    const char * pline[] = {
        "file to encrypt", NULL,
        "name of resulting file", NULL,
        "directory of resulting file", NULL,
        "directory to encrypt", NULL
    };

    rc_t rc, orc;

    /* super-fragilistic molti-hacki-docious
       let's find a better way to reuse things. */
    if ( de [ 0 ] == 'd' )
    {
        pline [ 0 ] = "file to decrypt";
        pline [ 6 ] = "directory to decrypt";
    }

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    orc = UsageSummary (progname);
    if (rc == 0)
        rc = orc;

    KOutMsg ("Parameters:\n");
    HelpParamLine ("source-file"          , pline);
    HelpParamLine ("destination-file"     , pline + 2);
    HelpParamLine ("destination-directory", pline + 4);
    HelpParamLine ("directory"            , pline + 6);
    KOutMsg ("\nOptions:\n");
    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, ForceUsage);
    CryptOptionLines ();
    HelpOptionsStandard ();

    /* forcing editor alignment */
    /*   12345678901234567890123456789012345678901234567890123456789012345678901234567890*/
    KOutMsg (
        "\n"
        "Details:\n"
        "  All %scryptions are non-destructive until successful. No files are deleted or\n"
        "  replaced until the %scryptions are complete.\n"
        "\n", de, de);

    KOutMsg (
        "  The extension '.ncbi_enc' will be %s when a file is %scrypted.\n"
        "\n", Decrypting ? "removed" : "added", de);

    if (Decrypting) KOutMsg (
        "  NCBI Archive files that contain NCBI database objects will not be decrypted\n"
        "  unless the %s option is used. As these objects can be used without\n"
        "  decryption it is recommended they remain encrypted.\n"
        "\n", OPTION_SRA);
    else KOutMsg (
        "  NCBI Archive files that contain NCBI database objects will not have the\n"
        "  .ncbi_enc extension added.\n\n");


    KOutMsg (
        "  If the only parameter is a file name then it will be replaced by a file that\n"
        "  is %scrypted with a possible changed extension.\n"
        "  \n", de);

    KOutMsg (
        "  If the only parameter is a directory, all files in that directory including\n"
        "  all files in subdirectories will be replaced with a possible change\n"
        "  in the extension.\n"
        "\n");

    KOutMsg (
        "  If there are two parameters  a copy is made but the copy will be %scrypted.\n"
        "  If the second parameter is a directory the new file might have a different\n"
        "  extension. If it is not a directory, the extension will be as given in the\n"
        "  the parameter.\n"
        "\n", de);

    KOutMsg (
        "  Missing directories in the destination path will be created.\n"
        "\n");

    KOutMsg (
        "  Already existing destination files will cause the program to end with\n"
        "  an error and will be left unchanged unless the --%s option is used to\n"
        "  force the files to be overwritten.\n"
        "\n", OPTION_FORCE);

    KOutMsg (
        "Encryption key (file password):\n"
        "  The encryption key or file password is handled by configuration. If not yet\n"
        "  set, this program will fail.\n\n"
        "  Please consult configuration page at\n"
        "  http://www.ncbi.nlm.nih.gov/Traces/sra/sra.cgi?view=toolkit_doc&f=std or\n"
        "  https://github.com/ncbi/sra-tools/wiki/Toolkit-Configuration\n"        
        );

    HelpVersion (fullpath, KAppVersion());

    return rc;
}





/*
 * determine the archive type for KFile f with pathname name
 *
 * This could be extended to handle tar files with a larger head size and
 * and some less simple checks for a tar header block at the start of
 * the file.
 */
ArcScheme ArchiveTypeCheck (const KFile * f)
{
    size_t num_read;
    rc_t rc;
    char head [128];

    IsArchive = false;
    rc = KFileReadAll (f, 0, head, sizeof head, &num_read);
    if (rc)
    {
        LOGERR (klogErr, rc, "Unable to read head of decrypted file");
        return arcError;
    }

    rc = KFileIsSRA (head, num_read);
    if (rc == 0)
    {

/*         OUTMSG (("+++++ ARCHIVE\n")); */


        /* a hack... */

        IsArchive = true;
        return arcSRAFile;
    }
/*     OUTMSG (("----- not an archive\n")); */
    return arcNone;
}


/*
 * Copy a file from a const KFile * to a KFile * with the paths for the two
 * for logging purposes
 *
 * return rc_t = 0 for success
 * return rc_t != 0 for failure
 */
rc_t CopyKFile (const KFile * src, KFile * dst, const char * source, const char * dest)
{
    rc_t rc;
    uint8_t	buff	[256 * 1024];
    size_t	num_read;
    size_t      num_writ;
    uint64_t	pos;

    for (pos = 0; ; pos += num_read)
    {
        rc = Quitting ();
        if (rc)
        {
            LOGMSG (klogFatal, "Received quit");
            break;
        }

        rc = KFileReadAll (src, pos, buff, sizeof (buff), &num_read);
        if (rc)
        {
            PLOGERR (klogErr,
                     (klogErr, rc,
                      "Failed to read from file $(F) at $(P)",
                      "F=%s,P=%lu", source, pos));
            break;
        }
        
        if (num_read == 0)
            break;

        rc = KFileWriteAll (dst, pos, buff, num_read, &num_writ);
        if (rc)
        {
            PLOGERR (klogErr,
                     (klogErr, rc,
                      "Failed to write to file $(F) at $(P)",
                      "F=%s,P=%lu", dest, pos));
            break;
        }
        
        if (num_writ != num_read)
        {
            rc = RC (rcExe, rcFile, rcWriting, rcFile, rcInsufficient);
            PLOGERR (klogErr,
                     (klogErr, rc,
                      "Failed to write all to file $(F) at $(P)",
                      "F=%s,P=%lu", dest, pos));
            break;
        }
    }
    return rc;
}


/*
 * determine the encryption type for KFile f with pathname name
 */
rc_t EncryptionTypeCheck (const KFile * f, const char * name, EncScheme * scheme)
{
    size_t num_read;
    rc_t rc;
    char head [128];

    assert (f != NULL);
    assert (name != NULL);

    rc = KFileReadAll (f, 0, head, sizeof head, &num_read);
    if (rc)
    {
        PLOGERR (klogErr, (klogErr, rc, "Unable to read head of "
                           "'$(F)'", "F=%s", name));
        *scheme = encError;
        return rc;
    }
    
    /* looks for files with NCBInenc or NCBIsenc signatures */
    rc = KFileIsEnc (head, num_read);
    if (rc == 0)
    {
            /* looks for files with just NCBIsenc signatures */
        rc = KFileIsSraEnc (head, num_read);

        *scheme = (rc == 0) ? encSraEncFile : encEncFile;
    }
    else
    {
        rc = KFileIsWGAEnc (head, num_read);
        if (rc == 0)
            *scheme = encWGAEncFile;
        else
            *scheme = encNone;
    }
    return 0;
}


/*
 * Check a file path name for ending in the extension used by this program
 *
 * return true if it ends with the extension and false if it does not
 */
static
bool IsTmpFile (const char * path)
{
    const char * pc;

    pc = strrchr (path, '.');
    if (pc == NULL)
        return false;

    if (strcmp (pc, TmpExt) == 0)
        return true;

    pc = string_chr (path, pc - path, '.');
    if (pc == NULL)
        return false;

    return (strcmp (pc, TmpLockExt) == 0);
}

#if 0
static
bool IsCacheFile (const char * path)
{
    const char * pc;

    pc = strrchr (path, '.');
    if (pc == NULL)
        return false;

    if (strcmp (pc, CacheExt) == 0)
        return true;

    pc = string_chr (path, pc - path, '.');
    if (pc == NULL)
        return false;

    return (strcmp (pc, CacheLockExt) == 0);
}
#endif

static
rc_t FileInPlace (KDirectory * cwd, const char * leaf, bool try_rename)
{
    rc_t rc;
    bool is_tmp;

    STSMSG (1, ("%scrypting file in place %s",De,leaf));

    rc = 0;
    is_tmp = IsTmpFile (leaf);
    if (is_tmp)
    {
        STSMSG (1, ("%s is a vdb-decrypt/vdb-encrypt temporary file and will "
                    "be ignored", leaf));
        TmpFoundFlag = true;
        if (ForceFlag)
            ; /* LOG OVERWRITE */
        else
            ; /* LOG TMP */
    }
    if (!is_tmp || ForceFlag)
    {
        char temp [MY_MAX_PATH];


        rc = KDirectoryResolvePath (cwd, false, temp, sizeof temp, ".%s%s",
                                    leaf, TmpExt);

        if (rc)
            PLOGERR (klogErr, (klogErr, rc, "unable to resolve '.$(S)$(E)'",
                               "S=%s,E=%s",leaf,TmpExt));
        else
        {
            KPathType kpt;
            uint32_t kcm;

            kcm = kcmCreate|kcmParents;
            kpt = KDirectoryPathType (cwd, "%s", temp);
            if (kpt != kptNotFound)
            {
                /* log busy */
                if (ForceFlag)
                {
                    kcm = kcmInit|kcmParents;
                    /* log force */
                    kpt = kptNotFound;
                }
            }

            if (kpt == kptNotFound)
            {
                const KFile * infile;

                rc = KDirectoryOpenFileRead (cwd, &infile, "%s", leaf);
                if (rc)
                    PLOGERR (klogErr, (klogErr, rc, "Unable to resolve '$(F)'",
                                       "F=%s",leaf));
                else
                {
                    uint64_t fz;
                    size_t z;
                    rc_t irc;
                    uint64_t ignored;

                    rc = KFileSize (infile, &fz);
                    /* ignore rc for now? yes hack */
                    z = string_size (leaf);

                    /* vdb-decrypt and vdb-encrypt both ignore repository cache files. */
                    irc = GetCacheTruncatedSize (infile, &ignored, true);
                    if (irc == 0)
                        STSMSG (1, ("skipping cache download file %s", leaf));
                    else if ((fz == 0) &&
                             (string_cmp (leaf + (z - (sizeof (".lock")-1)), sizeof (".lock"),
                                          ".lock", sizeof (".lock") , sizeof (".lock")) == 0))
                        STSMSG (1, ("skipping cache lock download file %s", leaf));
                    else
                    {
                        EncScheme scheme;

                        rc = EncryptionTypeCheck (infile, leaf, &scheme);
                        if (rc == 0)
                        {
                            ArcScheme ascheme;
                            bool changed;
                            bool do_this_file;
                            char new_name [MY_MAX_PATH + sizeof EncExt];

                            do_this_file = DoThisFile (infile, scheme, &ascheme);
                            strcpy (new_name, leaf);
                            if (try_rename && do_this_file)
                                changed = NameFixUp (new_name);
                            else
                                changed = false;
/*                         KOutMsg ("### %d \n", changed); */

                            if (!do_this_file)
                            {
                                if (changed)
                                {
                                    STSMSG (1, ("renaming %s to %s", leaf, new_name));
                                    rc = KDirectoryRename (cwd, false, leaf, new_name);
                                }
                                else
                                    STSMSG (1, ("skipping %s",leaf));
                            }
                            else
                            {
                                KFile * outfile;

                                rc = KDirectoryCreateExclusiveAccessFile (cwd, &outfile,
                                                                          false, 0600, kcm,
                                                                          "%s", temp);
                                if (rc == 0)
                                {
                                    const KFile * Infile;
                                    KFile * Outfile;

                                    rc = CryptFile (infile, &Infile, outfile, &Outfile, scheme);

                                    if (rc == 0)
                                    {
                                        STSMSG (1, ("copying %s to %s", leaf, temp));

                                        rc = CopyKFile (Infile, Outfile, leaf, temp);

                                        if (rc == 0)
                                        {
                                            uint32_t access;
                                            KTime_t date;

                                            rc = KDirectoryAccess (cwd, &access, "%s", leaf);
                                            if (rc == 0)
                                                rc = KDirectoryDate (cwd, &date, "%s", leaf);

                                            KFileRelease (infile);
                                            KFileRelease (outfile);
                                            KFileRelease (Infile);
                                            KFileRelease (Outfile);

                                            if (rc == 0)
                                            {
                                                STSMSG (1, ("renaming %s to %s", temp, new_name));

                                                rc = KDirectoryRename (cwd, true, temp, new_name);
                                                if (rc)
                                                    LOGERR (klogErr, rc, "error renaming");
                                                else
                                                {
                                                    if (changed)
                                                        KDirectoryRemove (cwd, false, "%s", leaf);

                                                    /*rc =*/
                                                    KDirectorySetAccess (cwd, false, access,
                                                                         0777, "%s", new_name);
                                                    KDirectorySetDate (cwd, false, date, "%s", new_name);
                                                    /* gonna ignore an error here I think */
                                                    return rc;
                                                }
                                            }
                                        }
                                    }
                                    KFileRelease (outfile);
                                }
                            }
                        }
                    }
                    KFileRelease (infile);
                }
            }
        }
    }
    return rc;
}


static
rc_t FileToFile (const KDirectory * sd, const char * source, 
                 KDirectory *dd, const char * dest_, bool try_rename,
                 char * base)
{
    const KFile * infile;
    rc_t rc;
    uint32_t access;
    KTime_t date;
    bool is_tmp;
    char dest [MY_MAX_PATH + sizeof EncExt];

    strcpy (dest, dest_);
    if (try_rename)
        NameFixUp (dest);

    if ((sd == dd) && (strcmp (source, dest) == 0))
        return FileInPlace (dd, dest, try_rename);

    if (base == NULL)
        STSMSG (1, ("%scrypting file %s to %s", De, source, dest));
    else
        STSMSG (1, ("%scrypting file %s to %s/%s", De, source, base, dest));

    /*
     * A Hack to make stdin/stout work within KFS
     */
    if (UseStdin)
    {
        const KFile * iinfile;
        rc = KFileMakeStdIn (&iinfile);
        if (rc == 0)
        {
            rc = KBufReadFileMakeRead (&infile, iinfile, 64 * 1024);
            KFileRelease (iinfile);
            if (rc == 0)
            {
                access = 0640;
                date = 0;
                goto stdin_shortcut;
            }
            LOGERR (klogErr, rc, "error wrapping stdin");
            return rc;
        }
    }
    rc = 0;
    is_tmp = IsTmpFile (source);

    if (is_tmp)
    {
        TmpFoundFlag = true;
        if (ForceFlag)
            ; /* LOG OVERWRITE */
        else
                ; /* LOG TMP */
    }
    if (!is_tmp || ForceFlag)
    {
        rc = KDirectoryAccess (sd, &access, "%s", source);
        if (rc)
            LOGERR (klogErr, rc, "Error check permission of source");

        else
        {
            rc = KDirectoryDate (sd, &date, "%s", source);
            if (rc)
                LOGERR (klogErr, rc, "Error check date of source");

            else
            {
                rc = KDirectoryOpenFileRead (sd, &infile, "%s", source);
                if (rc)
                    PLOGERR (klogErr, (klogErr, rc,
                                       "Error opening source file '$(S)'",
                                       "S=%s", source));
                else
                {
                    EncScheme scheme;

                stdin_shortcut:
                    rc = EncryptionTypeCheck (infile, source, &scheme);
                    if (rc == 0)
                    {
                        KFile * outfile;
                        uint32_t kcm;

                        /*
                         * Hack to support stdout before VFS is complete enough to use here
                         */
                        if (UseStdout)
                        {
                            rc = KFileMakeStdOut (&outfile);
                            if (rc)
                                LOGERR (klogErr, rc, "error wrapping stdout");
                        }
                        else
                        {
                            kcm = ForceFlag ? kcmInit|kcmParents : kcmCreate|kcmParents;

                            rc = KDirectoryCreateFile (dd, &outfile, false, 0600, kcm, "%s", dest);
                            if (rc)
                                PLOGERR (klogErr,(klogErr, rc, "error opening output '$(O)'",
                                                  "O=%s", dest));
                        }
                        if (rc == 0)
                        {
                            const KFile * Infile;
                            KFile * Outfile;

                            rc = CryptFile (infile, &Infile, outfile, &Outfile, scheme);
                            if (rc == 0)
                            {
                                rc = CopyKFile (Infile, Outfile, source, dest);
                                if (rc == 0)
                                {
                                    if (UseStdin || UseStdout)
                                        ;
                                    else
                                    {
                                        rc = KDirectorySetAccess (dd, false, access, 0777,
                                                                  "%s", dest);

                                        if (rc == 0 && date != 0)
                                            rc = KDirectorySetDate (dd, false, date, "%s", dest);
                                    }
                                }
                                KFileRelease (Infile);
                                KFileRelease (Outfile);
                            }
                            KFileRelease (outfile);
                        }
                    }
                    KFileRelease (infile);
                }
            }
        }
    }
    return rc;
}


static
rc_t DoDir (const KDirectory * sd, KDirectory * dd)
{
    KNamelist * names;
    rc_t rc;

    rc = KDirectoryList (sd, &names, NULL, NULL, ".");
    if (rc)
        ;
    else
    {
        uint32_t count;

        rc = KNamelistCount (names, &count);
        if (rc)
            ;
        else
        {
            uint32_t idx;

            for (idx = 0; idx < count; ++idx)
            {
                const char * name;

                rc = KNamelistGet (names, idx, &name);
                if (rc)
                    ;
                else
                {
                    const KDirectory * nsd;
                    KDirectory * ndd;
                    KPathType kpt;

                    kpt = KDirectoryPathType (sd, "%s", name);

                    switch (kpt)
                    {
                    default:
                        break;

                    case kptFile:
                        if (sd == dd)
                            rc = FileInPlace (dd, name, true);
                        else
                            rc = FileToFile (sd, name, dd, name, true, NULL);
                        break;

                    case kptDir:
                        if (sd == dd)
                        {
                            rc = KDirectoryOpenDirUpdate (dd, &ndd, false, "%s", name);
                            if (rc)
                                ;
                            else
                            {
                                /* RECURSION */
                                STSMSG (1, ("%scrypting directory %s", De, name));
                                rc = DoDir (ndd, ndd);
                                STSMSG (1, ("done with directory %s", name));
                                KDirectoryRelease (ndd);
                            }
                        }
                        else
                        {
                            rc = KDirectoryOpenDirRead (sd, &nsd, false, "%s", name);
                            if (rc)
                                ;
                            else
                            {
                                rc = KDirectoryCreateDir (dd, 0600, kcmOpen, "%s", name);
                                if (rc)
                                    ;
                                else
                                {                                    
                                    rc = KDirectoryOpenDirUpdate (dd, &ndd, false, "%s", name);
                                    if (rc)
                                        ;
                                    else
                                    {
                                        /* RECURSION */
                                        STSMSG (1, ("%scrypting directory %s", De, name));
                                        rc = DoDir (nsd, ndd);
                                        STSMSG (1, ("done with directory %s", name));

                                        KDirectoryRelease (ndd);
                                    }
                                }
                                KDirectoryRelease (nsd);
                            }
                        }
                        break;
                    }
                }
            }
        }
        KNamelistRelease (names);
    }
    return rc;
}


static
rc_t Start (KDirectory * cwd, const char * src, const char * dst)
{
    KPathType dtype;
    KPathType stype;
    char dpath [MY_MAX_PATH];
    char spath [MY_MAX_PATH];
    rc_t rc;
    bool using_stdin, using_stdout, try_rename;

    /* limited anti oops checks */
    try_rename = (dst == NULL);
    if (!try_rename)
    {
        /* try to prevent file to file clash */
        if (strcmp (src,dst) == 0)
            dst = NULL;

        /* try to prevent file to dir clash */
        else
        {
            size_t s,d;

            s = string_size (src);
            d = string_size (dst);

            if (s > d)
            {
                if (string_cmp (src, s, dst, d, d) == 0)
                {
                    if ((strchr (src+d+1, '/') == NULL) &&
                        ((src[d] == '/') ||
                         (src[d-1] == '/')))
                    {
                        try_rename = true;
                        dst = NULL;
                    }
                }
            }
        }
    }

    /*
     * This is a quick fix "hack"
     * A fully built out VFS should replace the KFS in use and eliminate this
     */
    using_stdin = (strcmp (src, "/dev/stdin") == 0);

    if (using_stdin)
    {
        if (dst == NULL)
        {
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcNull);
            LOGERR (klogErr, rc, "Unable to handle stdin in place");
            return rc;
        }
        stype = kptFile;
        strcpy (spath, src);
        UseStdin = true;
        STSMSG (1, ("reading console / stdin as input"));
        goto stdin_shortcut;
    }

    rc = KDirectoryResolvePath (cwd, false, spath, sizeof spath, "%s", src);
    if (rc)
    {
        LOGERR (klogErr, rc, "can't resolve source");
        return rc;
    }

    stype = KDirectoryPathType (cwd, "%s", spath);

    switch (stype)
    {
    case kptNotFound:
        rc = RC (rcExe, rcArgv, rcResolving, rcPath, rcNotFound);
        break;

    default:
    case kptBadPath:
        rc = RC (rcExe, rcArgv, rcResolving, rcPath, rcInvalid);
        break;

    case kptCharDev:
    case kptBlockDev:
    case kptFIFO:
    case kptZombieFile:
    case kptDataset:
    case kptDatatype:
        rc = RC (rcExe, rcArgv, rcResolving, rcPath, rcIncorrect);
        break;

    case kptFile:
    case kptDir:
        break;
    }
    if (rc)
    {
        PLOGERR (klogErr, (klogErr, rc, "can not use source '$(S)'", "S=%s", src));
        return rc;
    }

    /*
     * In Place Operation
     */
    if (dst == NULL)
    {

        /*
         * Input is a file
         */
        if (stype == kptFile)
        {
            KDirectory * ndir;
            char * pc;

            pc = strrchr (spath, '/');
            if (pc == NULL)
            {
                pc = spath;
                ndir = cwd;
                rc = KDirectoryAddRef (cwd);
            }
            else if (pc == spath)
            {
                ++pc;
                ndir = cwd;
                rc = KDirectoryAddRef (cwd);
            }
            else
            {
                *pc++ = '\0';
                rc = KDirectoryOpenDirUpdate (cwd, &ndir, false, "%s", spath);
            }

            if (rc == 0)
            {
                rc = FileInPlace (ndir, pc, try_rename);
                KDirectoryRelease (ndir);
            }
        }
        /*
         * Input is a directory
         */
        else
        {
            KDirectory * ndir;

            rc = KDirectoryOpenDirUpdate (cwd, &ndir, false, "%s", spath);
            if (rc)
                ;
            else
            {
                STSMSG (1, ("%scrypting directory %s", De, spath));
                rc = DoDir (ndir, ndir);
                STSMSG (1, ("done with directory %s", spath));
                KDirectoryRelease (ndir);
            }
        }
    }

    /*
     * 'Copy' Operation
     */
    else
    {
    stdin_shortcut:
        using_stdout = (strcmp (dst, "/dev/stdout") == 0);
        if (using_stdout == true)
        {
            dtype = kptFile;
            strcpy (dpath, dst);
            UseStdout = true;
            STSMSG (1, ("writing console / stdout as output"));
            goto do_file;
        }
        rc = KDirectoryResolvePath (cwd, false, dpath, sizeof dpath, "%s", dst);
        if (rc)
        {
            LOGERR (klogErr, rc, "can't resolve destination");
            return rc;
        }
        dtype = KDirectoryPathType (cwd, "%s", dpath);
        switch (dtype)
        {
        default:
        case kptBadPath:
            rc = RC (rcExe, rcArgv, rcResolving, rcPath, rcInvalid);
            PLOGERR (klogErr, (klogErr, rc, "can not use destination  '$(S)'", "S=%s", dst));
            break;

        case kptCharDev:
        case kptBlockDev:
        case kptFIFO:
        case kptZombieFile:
        case kptDataset:
        case kptDatatype:
            rc = RC (rcExe, rcArgv, rcResolving, rcPath, rcIncorrect);
            PLOGERR (klogErr, (klogErr, rc, "can not use destination parameter '$(S)'", "S=%s", dst));
            break;

        case kptNotFound:
        {
            size_t z;

            z = strlen (dst) - 1;
            if ((dst[z] == '/') || (stype == kptDir))
                goto do_dir;
            else
                goto do_file;
        }

        case kptFile:
            if (!ForceFlag)
            {
                rc = RC (rcExe, rcArgv, rcParsing, rcFile, rcExists);
                PLOGERR (klogErr, (klogErr, rc, "can not over-write '$(F)' without --force",
                                   "F=%s", dpath));
                break;
            }
        do_file:
            if (stype == kptFile)
            {
                rc = FileToFile (cwd, spath, cwd, dpath, try_rename, NULL);
            }
            else
            {
                rc = RC (rcExe, rcArgv, rcResolving, rcPath, rcIncorrect);
                LOGERR (klogErr, rc, "Can't do directory to file");
            }
            break;

        do_dir:
        case kptDir:
            /*
             * Input is a directory
             */
            if (stype == kptDir)
            {
#if DIRECTORY_TO_DIRECTORY_SUPPORTED
                const KDirectory * sdir;
                KDirectory * ddir;

                rc = KDirectoryOpenDirRead (cwd, &sdir, false, "%s", spath);
                if (rc)
                    ;
                else
                {
                    if (dtype == kptNotFound)
                    {
                        STSMSG (1, ("creating output directory %s", dpath));
                        rc = KDirectoryCreateDir (cwd, 0775, kcmCreate|kcmParents,
                                                  "%s", dpath);
                    }
                    if (rc == 0)
                    {
                        rc = KDirectoryOpenDirUpdate (cwd, &ddir, false, "%s", dpath);
                        if (rc)
                            ;
                        else
                        {
                            STSMSG (1, ("%scrypting directory %s to %s", De, spath, dpath));
                            rc = DoDir (sdir, ddir);
                            STSMSG (1, ("done with directory %s to %s", spath, dpath));
                            KDirectoryRelease (ddir);
                        }
                    }
                    KDirectoryRelease (sdir);
                }
#else
                rc = RC (rcExe, rcArgv, rcResolving, rcPath, rcIncorrect);
                LOGERR (klogErr, rc, "Can't do directory to directory");
#endif
            }
            /*
             * Input is a file
             */
            else
            {
                KDirectory * ndir;
                const char * pc;

                if (dtype == kptNotFound)
                {
                    STSMSG (1, ("creating output directory %s", dpath));
                    rc = KDirectoryCreateDir (cwd, 0775, kcmCreate|kcmParents,
                                              "%s", dpath);
                }
                if (rc == 0)
                {

                    STSMSG (1, ("opening output directory %s", dpath));
                    rc = KDirectoryOpenDirUpdate (cwd, &ndir, false, "%s", dpath);
                    if (rc)
                        ;
                    else
                    {
                        pc = strrchr (spath, '/');
                        if (pc == NULL)
                            pc = spath;
                        else
                            ++pc;

                        rc = FileToFile (cwd, spath, ndir, pc, true, dpath);

                        KDirectoryRelease (ndir);
                    }
                }
            }
            break;
        }
    }
    return rc;
}


static
rc_t StartFileSystem (const char * src, const char * dst)
{
    VFSManager * vmanager;
    rc_t rc;
 
    rc = VFSManagerMake (&vmanager);
    if (rc)
        LOGERR (klogErr, rc, "Failed to open file system");

    else
    {
        rc = VFSManagerGetKryptoPassword (vmanager, Password, sizeof Password,
                                          &PasswordSize);
        if (rc != 0)
            LOGERR (klogErr, rc, "unable to obtain a password");

        else
        {
            rc = KKeyInitRead (&Key, kkeyAES128, Password, PasswordSize);
            if (rc)
                LOGERR (klogErr, rc, "Unable to make encryption/decryption key");

            else
            {
                KDirectory * cwd;

                rc = VFSManagerGetCWD (vmanager, &cwd);
                if (rc)
                    LOGERR (klogInt, rc, "unable to access current directory");

                else
                {
                    rc = Start (cwd, src, dst);

                    KDirectoryRelease (cwd);
                }
            }
        }
        VFSManagerRelease (vmanager);
    }
    return rc;
}


rc_t CommonMain (Args * args)
{
    rc_t rc;
    uint32_t ocount; /* we take the address of ocount but not pcount. */
    uint32_t pcount; /* does that help the compiler optimize? */

    rc = ArgsParamCount (args, &ocount);
    if (rc)
        LOGERR (klogInt, rc, "failed to count parameters");

    else if ((pcount = ocount) == 0)
        MiniUsage (args);

    else if (pcount > 2)
    {
        LOGERR (klogErr, rc, "too many parameters");
        MiniUsage(args);
    }
            
    else
    {
        const char * dst; /* we only take the address of one of these */
        const char * src;

        rc = ArgsOptionCount (args, OPTION_FORCE, &ocount);
        if (rc)
            LOGERR (klogInt, rc, "failed to examine force option");

        else
        {
            ForceFlag = (ocount > 0);

            /* -----
             * letting comp put src in register
             * only if it wants
             */
            rc = ArgsParamValue (args, 0, &dst);
            if (rc)
                LOGERR (klogInt, rc, "Failure to fetch "
                        "source parameter");

            else
            {
                src = dst;

                if (pcount == 1)
                    dst = NULL;

                else
                {
                    rc = ArgsParamValue (args, 1, &dst);
                    if (rc)
                        LOGERR (klogInt, rc, "Failure to fetch "
                                "destination parameter");
                }

                if (rc == 0)
                    rc = StartFileSystem (src, dst);
            }
        }
    }
    return rc;
}

/* EOF */
