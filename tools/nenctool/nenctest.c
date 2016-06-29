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

#include <kapp/main.h>

#include <vfs/manager.h>
#include <vfs/path.h>
#include <krypto/testciphermgr.h>
#include <krypto/encfile.h>
#include <kfs/file.h>

#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h>

#include <assert.h>

#define OPTION_FORCE   "force"
#define ALIAS_FORCE   "f"
static
const char * force_usage[] = 
{ "force overwrite of existing files", NULL };


#define OPTION_CIPHER  "cipher"
#define ALIAS_CIPHER   "c"
static
const char * cipher_usage[] = 
{ "choose cipher version ",
  "0-byte oritented vector", 
  "1-vector",
  "2-vector register",
  "3-aes-ni",
  "not all implementatons exist on all platforms",
  NULL };
static
OptDef Options[] = 
{
    /* name            alias max times oparam required fmtfunc help text loc */
    { OPTION_FORCE,  ALIAS_FORCE,  NULL, force_usage,  0, false, false },
    { OPTION_CIPHER, ALIAS_CIPHER, NULL, cipher_usage, 1, true,  false }
};



/* Usage
 */
const char UsageDefaultName [] = "nencvalid";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s [options] <source-file-path> <destination-file-path>\n"
        "\n"
        "Summary:\n"
        "  Copy a file from the first parameter to the second parameter\n"
        "  Encryption, decryption, or re-encryption are the expected purpose.\n"
        "  This is a version of nenctool with some testing features added.\n"
        "  The first added feature is some level of cipher implementation\n"
        "  selection.\n"
        "\n", progname);
}

static
const char * first_usage[] = 
{
    "The path to a file either in native format",
    NULL
};

static
const char * second_usage[] = 
{
    "in 'file' URI scheme or in 'ncbi-file'",
    "URI scheme.",
    "\"ncbi-file\" scheme adds a query to the \"file\" scheme",
    "where 'enc' or 'encrypt' means the file is encrypted",
    "and 'pwfile=<path>' points to a file to get the password",
    "or 'pwfd=<fd>' refers to a file descriptor from which to",
    "read a password.",
    NULL
};

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

    KOutMsg ("Parameters:\n");

    HelpParamLine ("source-file-path", first_usage);
    HelpParamLine ("destination-file-path", second_usage);

    KOutMsg ("\nOptions:\n");

    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, force_usage);
    HelpOptionLine (ALIAS_CIPHER, OPTION_CIPHER, "implementation", cipher_usage);

    HelpOptionsStandard ();

    OUTMSG (("\n"
             "ncbi-file uri syntax:\n"
             "  'ncbi-file' uris are based on a combination of the standard\n"
             "  'file' and 'http' syntax familiar to browser users.\n"
             "  Enclosing questions marks are probably necessary.\n"
             "\n"
             "  URI syntax is 'scheme:'hierarchical-part'?'query'#'fragment'\n"
             "  scheme:\n"
             "    ncbi-file\n"
             "  hierarchical-part:\n"
             "    this is the same as for scheme file and can be the native form or\n"));
    OUTMSG (("    the ncbi 'posix' format that matches most flavors of Unix.\n"
             "    Environment variables and short hands such as '~' are not interpreted.\n"
             "  query:\n"
             "    Zero or two symbols separated by the '&'.  The whole query can be omitted.\n"
             "      enc or encrypt\n"
             "      pwfile='path to a file containing a password'\n"
             "      pwfd='file descriptor where the password can be read'\n"
             "    If the encrypt is present exactly one of the other two must be present.\n"
             "  fragment:\n"
             "    No fragment is permitted.\n"));
    OUTMSG (("\n"
             "password contents:\n"
             "  The file or file descriptor containing the password is read enough to satisfy\n"
             "  the following restriction.  The password is terminated by the end of file,\n"
             "  a carriage return or a line feed.  The length must be no more than 4096 bytes.\n"
             "  The password file should have limited read access to protect it.\n"));


    OUTMSG (("\n"
             "Use examples:"
             "\n"
             "  To encrypt a create a file named 'example' with a password from a file\n"
             "  named 'password-file' to a file named 'example.nenc' all in the current\n"
             "  directory:\n"
             "\n"
             "  $ %s example \"ncbi-file:example.nenc?encrypt&pwfile=password-file\"\n",
             progname));
    OUTMSG (("\n"
             "  To decrypt a file named 'example.nenc' with a password from a file\n"
             "  named 'password-file' to a file named 'example' in directories implied\n"
             "  in the command:\n"
             "\n"
             "  $ %s \"ncbi-file:/home/user/downloads/example.nenc?encrypt&pwfile=/home/user/password-file\" example\n",
             progname));
    OUTMSG (("\n"
             "  To change the encryption of a file from one password to another:\n"
             "\n"
             "  $ %s \"ncbi-file:/home/user/downloads/example.nenc?encrypt&pwfile=old-password-file\" \\\n"
             "         \"ncbi-file:example.nenc?encrypt&pwfile=/home/user/new-password-file\"\n",
             progname));

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


static
rc_t copy_file (const char * src, const char * dst, const KFile * fin, KFile *fout)
{
    rc_t rc;
    uint8_t	buff	[64 * 1024];
    size_t	num_read;
    uint64_t	inpos;
    uint64_t	outpos;

    assert (src);
    assert (dst);
    assert (fin);
    assert (fout);

    inpos = 0;
    outpos = 0;

#if 1
    for (inpos = 0; ; inpos += num_read)
    {
        rc = Quitting ();
        if (rc)
        {
            LOGMSG (klogFatal, "Received quit");
            break;
        }
        else
        {
            rc = KFileReadAll (fin, inpos, buff, sizeof (buff), &num_read);
            if (rc)
            {
                PLOGERR (klogErr,
                         (klogErr, rc,
                          "Failed to read from file $(F) at $(P)",
                          "F=%s,P=%lu", src, inpos));
                break;
            }
            else if (num_read)
            {
                size_t num_writ;

                rc = KFileWriteAll (fout, inpos, buff, num_read, &num_writ);
                if (rc)
                {
                    PLOGERR (klogErr,
                             (klogErr, rc,
                              "Failed to write to file $(F) at $(P)",
                              "F=%s,P=%lu", dst, outpos));
                    break;
                }
                else if (num_writ != num_read)
                {
                    rc = RC (rcExe, rcFile, rcWriting, rcFile, rcInsufficient);
                    PLOGERR (klogErr,
                             (klogErr, rc,
                              "Failed to write all to file $(F) at $(P)",
                              "F=%s,P=%lu", dst, outpos));
                    break;
                }
            }
            else 
                break;
        }
    }
#else
    do
    {
        rc = Quitting ();
        if (rc)
        {
            LOGMSG (klogFatal, "Received quit");
            break;
        }
        rc = KFileRead (fin, inpos, buff, sizeof (buff), &num_read);
        if (rc)
        {
            PLOGERR (klogErr,
                     (klogErr, rc,
                      "Failed to read from file $(F) at $(P)",
                      "F=%s,P=%lu", src, inpos));
            break;
        }
        else if (num_read > 0)
        {
            size_t to_write;

            inpos += (uint64_t)num_read;

            STSMSG (2, ("Read %zu bytes to %lu", num_read, inpos));

            to_write = num_read;
            while (to_write > 0)
            {
                size_t num_writ;
                rc = KFileWrite (fout, outpos, buff, num_read, &num_writ);
                if (rc)
                {
                    PLOGERR (klogErr,
                             (klogErr, rc,
                              "Failed to write to file $(F) at $(P)",
                              "F=%s,P=%lu", dst, outpos));
                    break;
                }
                outpos += num_writ;
                to_write -= num_writ;
            }
        }
        if (rc)
            break;
    } while (num_read != 0);
#endif
    return rc;
}


static
rc_t nenctest (const char * srcstr, const char * dststr, bool force)
{
    VFSManager * mgr;
    rc_t rc;

    rc = VFSManagerMake (&mgr);
    if (rc)
        LOGERR (klogInt, rc, "failed to create file system manager");
    else
    {
        VPath * srcpath;

        rc = VFSManagerMakePath (mgr, &srcpath, "%s", srcstr);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "Failed to parse source path '$(path)'",
                      "path=%s", srcstr));
        else
        {
            VPath * dstpath;

            rc = VFSManagerMakePath (mgr, &dstpath, "%s", dststr);
            if (rc)
                PLOGERR (klogErr,
                         (klogErr, rc, "Failed to parse destination path '$(path)'",
                          "path=%s", dststr));
            else
            {
                const KFile * srcfile;

                rc = VFSManagerOpenFileRead (mgr, &srcfile, srcpath);
                if (rc)
                    PLOGERR (klogErr,
                             (klogErr, rc, "Failed to open source path '$(path)'",
                              "path=%s", srcstr));
                else
                {
                    KFile * dstfile;

                    rc = VFSManagerCreateFile (mgr, &dstfile, false, 0666,
                                               kcmParents | (force ? kcmInit : kcmCreate),
                                               dstpath);
                    if (rc)
                        PLOGERR (klogErr,
                                 (klogErr, rc, "failed to open destination path '$(path)'",
                                  "path=%s", dststr));
                    else
                    {
                        rc = copy_file (srcstr, dststr, srcfile, dstfile);
                        if (rc)
                        {
                            PLOGERR (klogErr,
                                     (klogErr, rc, "failed to copy '$(S)' to '$(D)'",
                                      "S=%s,D=%s", srcstr, dststr));

                            VFSManagerRemove (mgr, true, dstpath);
                        }

                        KFileRelease (dstfile);
                    }
                    KFileRelease (srcfile);
                }
                VPathRelease (dstpath);
            }
            VPathRelease (srcpath);
        }
        VFSManagerRelease (mgr);
    }
    return rc;
}


/* KMain - EXTERN
 *  executable entrypoint "main" is implemented by
 *  an OS-specific wrapper that takes care of establishing
 *  signal handlers, logging, etc.
 *
 *  in turn, OS-specific "main" will invoke "KMain" as
 *  platform independent main entrypoint.
 *
 *  "argc" [ IN ] - the number of textual parameters in "argv"
 *  should never be < 0, but has been left as a signed int
 *  for reasons of tradition.
 *
 *  "argv" [ IN ] - array of NUL terminated strings expected
 *  to be in the shell-native character set: ASCII or UTF-8
 *  element 0 is expected to be executable identity or path.
 */
rc_t CC KMain ( int argc, char *argv [] )
{
    Args* args = NULL;
    rc_t rc;

    rc = ArgsMakeAndHandle(&args, argc, argv, 1, Options, sizeof Options / sizeof (OptDef));
    if (rc)
        LOGERR (klogInt, rc, "failed to parse command line parameters");
    if (rc == 0)
    {
        uint32_t pcount;
        bool force;

        rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
        if (rc)
            LOGERR (klogInt, rc, "failed to examine force option");
        else
        {
            force = (pcount > 0);
 
            rc = ArgsOptionCount (args, OPTION_CIPHER, &pcount);
            if (rc)
                LOGERR (klogInt, rc, "failed to examine cipher option");
            else
            {
                if (pcount)
                {
                    const char * value;

                    rc = ArgsOptionValue (args, OPTION_CIPHER, 0, (const void **)&value);
                    if (rc)
                        LOGERR (klogInt, rc, "failed to examine cipher value");
                    else
                    {
                        if (value[1])
                            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
                        else
                            switch (value[0])
                            {
                            case '0':
                            case '1':
                            case '2':
                            case '3':
                                KCipherSubType = value[0] - '0';
                                break;
                            default:
                                rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
                                break;
                            }
                        if (rc)
                            LOGERR (klogErr, rc, "bad value for cipher implmentation");
                    }
                }
                if (rc == 0)
                {
                    rc = ArgsParamCount (args, &pcount);
                    if (rc)
                        LOGERR (klogInt, rc, "failed to count parameters");
                    else
                    {
                        if (pcount != 2)
                            MiniUsage (args);
                        else
                        {
                            const char * src;

                            rc = ArgsParamValue (args, 0, (const void **)&src);
                            if (rc)
                                LOGERR (klogInt, rc, "failed to get source parameter");
                            else
                            {
                                const char * dst;
                                rc = ArgsParamValue (args, 1, (const void **)&dst);
                                if (rc)
                                    LOGERR (klogInt, rc, "failed to get destination parameter");
                                else
                                {
                                    rc = nenctest (src, dst, force);
                                }
                            }
                        }
                    }
                }
            }
        }
        ArgsWhack (args);
    }
    STSMSG (1, ("exiting: %R (%u)", rc, rc));
    return rc;
}

/* EOF */
