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

#include "nencvalid.vers.h"

#include <kapp/main.h>

#include <vfs/manager.h>
#include <vfs/path.h>

#include <krypto/encfile.h>
#include <krypto/wgaencrypt.h>

#include <kfs/file.h>
#include <kfs/buffile.h>

#include <klib/log.h>
#include <klib/status.h>
#include <klib/out.h>
#include <klib/debug.h> /* DBGMSG */
#include <klib/rc.h>
#include <klib/text.h>

#include <assert.h>
#include <stdlib.h>

/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion ( void )
{
    return NENCVALID_VERS;
}

#define OPTION_DECRYPT_BIN_COMPATIBILITY "compatibility-mode"
/* "decrypt.bin" */

static
OptDef Options[] = 
{
    /* private option for decrypt.bin script */
    { OPTION_DECRYPT_BIN_COMPATIBILITY, NULL,        NULL, NULL,        0, false, false }
};

enum CompatibilityModeExitCode
{
    CMEC_NO_ERROR = 0,
    CMEC_BAD_TYPE = 100,
    CMEC_BAD_HEADER = 101,
    CMEC_TRUNCATED = 102,
    CMEC_EXCESS = 103,
    CMEC_NO_CHECKSUM = 104,
    CMEC_NO_PASSWORD = 105,
    CMEC_BAD_CHECKSUM = 106
};

/* Usage
 */
const char UsageDefaultName [] = "nencvalid";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s [options] <file-path> [ <file-path> ...]\n"
        "\n"
        "Summary:\n"
        "  Validate the consistency of NCBI format encrypted files\n"
        "\n", progname);
}

static
const char * param_usage[] = 
{
    "A file in an NCBI encrypted file format.", 
    "The first version encrypted file format",
    "needs a password to validate the content",
    "while the second version does not.  If the",
    "ncbi-file uri format is used if you ask",
    "file decryption (encrypt in the query)",
    "the validation will be on the decrypted",
    "file which might not be the intention.",
    "The password if needed for validation will",
    "have to be in the standard NCBI VDB",
    "configuration as 'krypto/pwfile' or ",
    "referenced by the environment variable",
    "VDB_PWFILE or Config parameter", NULL
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

    HelpParamLine ("file-path", param_usage);

    KOutMsg ("\nOptions:\n");

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

static rc_t wga_log_error (const char * path, rc_t rc, bool decrypt_bin_compatible)
{
    switch (GetRCObject(rc))
    {
    case rcSize:
        switch (GetRCState(rc))
        {
        case rcExcessive:
            if (decrypt_bin_compatible)
                exit (CMEC_EXCESS);
            PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                              "probably mis-concatenated file '$(P)'",
                              "P=%s", path));
            return rc;

        case rcInsufficient:
            if (decrypt_bin_compatible)
                exit (CMEC_TRUNCATED);
            PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                              "probably truncated file '$(P)'",
                              "P=%s", path));
            return rc;

        default:
            break;
        }
        break;

    case rcChecksum:
        switch (GetRCState(rc))
        {
        case rcInvalid:
            if (decrypt_bin_compatible)
                exit (CMEC_BAD_CHECKSUM);
            PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                              "checksum of contents '$(P)'",
                              "P=%s", path));
            return rc;

        case rcNull:
            if (decrypt_bin_compatible)
                exit (CMEC_NO_CHECKSUM);
            PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                              "checksum of contents without password '$(P)'",
                              "P=%s", path));
            return 0;

        default:
            break;
        }
        break;

    case rcConstraint:
        switch (GetRCState(rc))
        {
        case rcCorrupt:
            if (decrypt_bin_compatible)
                exit (CMEC_BAD_CHECKSUM);
            PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                              "checksum of contents '$(P)'",
                              "P=%s", path));
            return rc;

        default:
            break;
        }
        break;

    case rcEncryption:
        switch (GetRCState(rc))
        {
        case rcNull:
            if (decrypt_bin_compatible)
                exit (CMEC_NO_PASSWORD);
            LOGERR (klogWarn, rc, "length validated but content not checked without a password");
            return 0;

        case rcNotFound:
            if (decrypt_bin_compatible)
                exit (CMEC_NO_CHECKSUM);
            LOGERR (klogWarn, rc, "length validated but content not has no checksum");
            return 0;

        default:
            break;
        }
        break;

    default:
        break;
    }
    PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                      "with non-specified error file "
                      "'$(P)'", "P=%s", path));
    return rc;
}

static rc_t nencvalidWGAEncValidate (const char * path, const KFile * file, VFSManager * mgr, bool decrypt_bin_compatible)
{
    rc_t rc;
    size_t pwz;
    char   pw [8192];

    rc = VFSManagerGetKryptoPassword (mgr, pw, sizeof (pw) - 1, &pwz);

    if (rc) 
    {
        if (GetRCObject(rc) == rcEncryptionKey && GetRCState(rc) == rcNotFound)
        {   /* krypto/pwfile not found in the configuration; will proceed without decryption */
            LOGERR (klogWarn, rc, "no password file configured file content will not be validated");

            /* configuration did not contain a password file so set it to nul */
            pw[0] = '\0';
            pwz = 0;
            rc = 0;
        }
        else
            LOGERR (klogInt, rc, "unable to read password file");
    }

    if (rc == 0)
    {
        rc = WGAEncValidate (file, pw, pwz);
        if (rc)
            rc =  wga_log_error (path, rc, decrypt_bin_compatible);
    }
    return rc;
}


/*
 * do out thing on a single file name
 */
static
rc_t HandleOneFile (VFSManager * mgr, const char * path, bool decrypt_bin_compatible)
{
    rc_t rc;
    VPath * vpath;

    rc = VFSManagerMakePath (mgr, &vpath, "%s", path);
    if (rc)
        PLOGERR (klogErr,
                 (klogErr, rc, "failed to parse path "
                  "parameter '$(P)'", "P=%s", path));

    else
    {
        const KFile * file;

        rc = VFSManagerOpenFileRead (mgr, &file, vpath);
        if (rc)
            PLOGERR (klogErr,
                     (klogErr, rc, "failed to parse path "
                      "parameter '$(P)'", "P=%s", path));

        else
        {
            size_t  buffread;
            const KFile * buffile;

            rc = KBufFileMakeRead (&buffile, file, 64*1024);
            if (rc)
                LOGERR (klogInt, rc, "unable to buffer file");
            else
            {
                /* the two current encrypted file types have an 8 byte signature (magic(
                 * at the beginning of the file.  If these 8 don't match we don't even call it
                 * an encrypted file at all */
                char buffer [8];

                rc = KFileRead (buffile, 0, buffer, sizeof buffer, &buffread);
                if (rc)
                    LOGERR (klogInt, rc, "Unable to read file signature");
                else
                {
                    if (buffread < sizeof buffer)
                    {
                        if (decrypt_bin_compatible)
                            exit (CMEC_BAD_TYPE);
                        else
                        {
                            rc = RC (rcExe, rcFile, rcValidating, rcHeader, rcWrongType);
                            LOGERR (klogErr, rc, "File too short to be encrypted file");
                        }
                    }
                    else
                    {
                        rc = KFileIsEnc (buffer, sizeof buffer);

                        switch (GetRCState(rc))
                        {
                        case 0:
                        case rcInsufficient:
                            rc = KEncFileValidate (buffile);
                            if (rc)
                            {
                                switch (GetRCObject(rc))
                                {
                                    /* unsuable header */
                                case rcHeader:
                                case rcByteOrder:
                                    if (decrypt_bin_compatible)
                                        exit (CMEC_BAD_HEADER);
                                    PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                                                      "with a bad header file '$(P)'",
                                                      "P=%s", path));
                                    break;

                                case rcFile:
                                    switch (GetRCState(rc))
                                    {
                                        /* bad block count in the footer: guess it was a truncataiont
                                         * an inconvienent point */
                                    case rcCorrupt:
                                        /* block read was neither a block nor a footer */
                                    case rcInsufficient:
                                        if (decrypt_bin_compatible)
                                            exit (CMEC_TRUNCATED);
                                        PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                                                          "probably truncated file '$(P)'",
                                                          "P=%s", path));

                                        break;
                                    default:
                                        PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                                                          "with non-specified error file "
                                                          "'$(P)'", "P=%s", path));

                                        break;
                                    }
                                    break;

                                case rcCrc:
                                    if (decrypt_bin_compatible)
                                        exit (CMEC_BAD_CHECKSUM);
                                    PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                                                      "with good length but bad checksum file "
                                                      "'$(P)'", "P=%s", path));
                                    break;

                                default:
                                    PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                                                      "with non-specified error file "
                                                      "'$(P)'", "P=%s", path));
                                    break;
                                }
                            }
                            else
                                STSMSG (1, ("validated file content and size for '%s'\n", path));
                            break;

                        case rcWrongType:
                            rc = KFileIsWGAEnc (buffer, buffread);
                            switch (GetRCState(rc))
                            {
                            case 0:
                                rc = nencvalidWGAEncValidate (path, buffile, mgr, decrypt_bin_compatible);
                                break;

                            case rcWrongType:
                                if (decrypt_bin_compatible)
                                    exit (CMEC_BAD_TYPE);
                                STSMSG (1, ("'%s' is not an encrypted file", path));
                                break;

                            default:
                                break;
                            }

                        default:
                            if (rc)
                                PLOGERR (klogErr,(klogErr, rc, "failed to validate "
                                                  "with non-specified error file "
                                                  "'$(P)'", "P=%s", path));
                            break;
                        }
                    }
                }
                KFileRelease (buffile);
            }
            KFileRelease (file);
        }
        VPathRelease (vpath);
    }
    if (rc)
        STSMSG (1, ("failed validation '%s'", path));
    else
        STSMSG (1, ("passed validation '%s'", path));

    return rc;
}


/*
 * params are file names so retrienve them one by one and process them
 */
static __inline__
rc_t HandleParams (VFSManager * mgr, Args * args, uint32_t pcount)
{
    rc_t rc;
    rc_t orc;
    uint32_t ix;

    rc = orc = 0;
    for (ix = 0; (ix < pcount); ++ix)
    {
        const char * pc;

        rc = ArgsParamValue (args, ix, &pc);
        if (rc)
            PLOGERR (klogErr, 
                     (klogErr, rc, "failed to retrieve "
                      "parameter '$(P)'", "P=%u", ix));
        else
        {
            /*
             * we will return rc but use orc to handle
             * the return error code form the first found error on a parameter
             * though we will continue to process all file parameters
             */
            orc = HandleOneFile (mgr, pc, false);
            if (rc == 0)
                rc = orc;
        }
    }
    return rc;
}


/*
 * handle the command line options then handle the file name parameters
 */
static __inline__
rc_t HandleOptions (Args * args)
{
    uint32_t pcount;
    rc_t rc;

    rc = ArgsParamCount (args, &pcount);
    if (rc)
        LOGERR (klogInt, rc, "failed to count parameters");

    else
    {
        if (pcount == 0)
            MiniUsage(args);

        else
        {
            uint32_t ocount;

            rc = ArgsOptionCount (args, OPTION_QUIET, &ocount);
            if (rc)
                LOGERR (klogInt, rc, "failed to retrieve quiet option");

            else
            {
                bool quiet;

                quiet = (ocount != 0);

                rc = ArgsOptionCount (args, OPTION_DECRYPT_BIN_COMPATIBILITY, &ocount);
                if (rc)
                    LOGERR (klogInt, rc, "failed to retrieve compatibility option");

                else
                {
                    VFSManager * mgr;

                    rc = VFSManagerMake (&mgr);
                    if (rc)
                        LOGERR (klogErr, rc, "failed to open file system manager");

                    else
                    {
                        bool decrypt_bin_compatible;

                        if (ocount == 0)
                            decrypt_bin_compatible = false;

                        else
                            decrypt_bin_compatible = true;


                        if (quiet)
                        {
                            KStsLevelSet (0);
                            KLogLevelSet (0);
                        }
                        else if (!decrypt_bin_compatible)
                        {
                            if (KStsLevelGet() == 0)
                                KStsLevelSet(1);
                        }


                        if (decrypt_bin_compatible)
                        {
                            if (pcount > 1)
                            {
                                rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
                                LOGERR (0, rc, "compatibility mode can only handle one file parameter");
                            }
                            else
                            {
                                const char * pc;

                                rc = ArgsParamValue (args, 0, &pc);
                                if (rc)
                                    LOGERR (0, rc, "unable to retrieve file parameter");

                                if (rc == 0)
                                    rc = HandleOneFile (mgr, pc, true); 
                            }
                        }
                        else
                        {
                            rc = HandleParams (mgr, args, pcount);
                        }
                    
                        VFSManagerRelease (mgr);
                    }
                }
            }
        }
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

    rc = ArgsMakeAndHandle(&args, argc, argv, 1, Options, sizeof Options / sizeof (Options[0]) );
    if (rc)
        LOGERR (klogInt, rc, "failed to parse command line parameters");
    if (rc == 0)
    {
        rc = HandleOptions (args);

        ArgsWhack (args);
    }
    STSMSG (2,("exiting: %R (%u)", rc, rc));
    return rc;
}

/* EOF */
