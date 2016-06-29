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

#include "syspass-priv.h" /* get_pass */

#include <kapp/main.h> /* KMain */

#include <vfs/manager.h> /* VFSManager */
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/rc.h>

#include <string.h> /* strcmp */
#include <assert.h>
#include <limits.h> /* PATH_MAX */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)
#if 0
/******************************************************************************/
static
rc_t SetPwd(const char *password, bool no_prompt)
{
    rc_t rc = 0;

    VFSManager *mgr = NULL;

    assert(password);

    if (rc == 0) {
        rc = VFSManagerMake(&mgr);
        DISP_RC(rc, "while calling VFSManagerMake");
    }

    if (rc == 0) {
        char pwd_dir[PATH_MAX] = "";
        rc = VFSManagerUpdateKryptoPassword
            (mgr, password, strlen(password), pwd_dir, sizeof pwd_dir);
        if (rc) {
            if (no_prompt) {
                LOGERR(klogInt, rc, "while updating the password");
            }
            else if (rc == SILENT_RC(rcVFS, rcEncryptionKey, rcUpdating,
                            rcDirectory, rcExcessive))
            {
                OUTMSG((
                    "\nSecurity warning:\n"
                    "Directory \"%s\" has excessive access permissions.\n"
                    "Run \"chmod 700 %s\" to repair\n"
                    "or ask your system administrator for assistance.\n",
                    pwd_dir, pwd_dir));
                rc = 0;
            }
            else if (rc == SILENT_RC(rcVFS, rcEncryptionKey, rcUpdating,
                rcSize, rcExcessive))
            {
                OUTMSG((
                    "\nError:\n"
                    "Password is too long.\n"
                    "Maximum password size is %ld.\n",
                    VFS_KRYPTO_PASSWORD_MAX_SIZE));
            }
            else if (rc == SILENT_RC(rcVFS, rcEncryptionKey, rcUpdating,
                rcEncryptionKey, rcInvalid))
            {
                OUTMSG((
                    "\nError:\n"
                    "Invalid character in password\n"
                    "(CR/LF are not allowed)\n"));
            }
            else if (rc == SILENT_RC(rcVFS, rcEncryptionKey, rcUpdating,
                rcPath, rcExcessive))
            {
                OUTMSG((
                    "\nError:\n"
                    "Cannot set the password.\n"
                    "Path to password file is too long.\n"
                    "Your configuration should be updated.\n"));
            }
            else if (rc == SILENT_RC(rcVFS, rcEncryptionKey, rcUpdating,
                rcPath, rcIncorrect))
            {
                OUTMSG((
                    "\nError:\n"
                    "Cannot set the password.\n"
                    "Existing path to password is not a file.\n"
                    "Your configuration should be updated.\n"));
            }
            else if (rc == SILENT_RC(rcVFS, rcEncryptionKey, rcUpdating,
                rcPath, rcCorrupt))
            {
                OUTMSG((
                    "\nError:\n"
                    "Cannot set the password.\n"
                    "Unknown file type for configured path/file name.\n"
                    "Your configuration should be updated.\n"));
            }
            else if (rc == SILENT_RC(rcVFS, rcEncryptionKey, rcWriting,
                rcFile, rcInsufficient))
            {
                OUTMSG((
                    "\nError:\n"
                    "Incomplete writes to temporary password file.\n"
                    "Cannot set the password.\n"));
            }
            else {
                OUTMSG(("\nCannot set the password. Please consult\n"
                 "configuration page at\n"
                 "http://www.ncbi.nlm.nih.gov/Traces/sra/sra.cgi?view=toolkit_doc&f=std or\n"
                 "https://github.com/ncbi/sra-tools/wiki/Toolkit-Configuration\n"));
            }
        }
    }

    RELEASE(VFSManager, mgr);

    return rc;
}

#define PWD_SZ 1024

static
rc_t run(bool quiet)
{
    rc_t rc = 0;

    char password[PWD_SZ] = "";

    bool empty = false;

    KWrtHandler handler;
    handler.writer = KOutWriterGet();
    handler.data = KOutDataGet();

    KOutHandlerSetStdErr();
    KLogLibHandlerSet(NULL, NULL);

    OUTMSG(("Changing password\n"));

#if 1
    if (rc == 0) {
        int i = 0;
        for (i = 0; i < 3 && rc == 0; ++i) {
            rc = get_pass("New password:", password, sizeof password);
            DISP_RC(rc, "cannot read the password");
            if (rc != 0) {
                break;
            }
            if (password[0] == '\0') {
                OUTMSG(("BAD PASSWORD: too short\n"));
            }
            else {
                break;
            }
        }
        if (rc == 0 && password[0] == '\0') {
            rc = RC(rcExe, rcString, rcReading, rcString, rcIncorrect);
            empty = true;
         /* LOGERR(klogErr, rc, "failed to set password"); */
        }
    }

    if (rc == 0) {
        char repassword[PWD_SZ] = "";
        rc = get_pass("Retype new password:", repassword, sizeof repassword);
        DISP_RC(rc, "cannot read the password");
        if (rc == 0) {
            if (strcmp(password, repassword)) {
                OUTMSG(("Sorry, passwords do not match.\n"));
                rc = RC(rcExe, rcString, rcReading, rcString, rcInconsistent);
             /* LOGERR(klogErr, rc, "failed to set password"); */
            }
        }
    }

#else
    strcpy(password, "1n2");
#endif

    if (rc == 0) {
        rc = SetPwd(password, quiet);
    }

    KOutHandlerSet(handler.writer, handler.data);

    if (empty)
    {   rc = 0; }

    return rc;
}
#endif
#define SL_OPTION "noprompt"
#define SL_ALIAS "n"
static const char *SL_USAGE[]
    = { "Do not suggest consulting configuration page at http://www.ncbi.nlm.nih.gov/Traces/sra/sra.cgi?view=toolkit_doc&f=std or https://github.com/ncbi/sra-tools/wiki/Toolkit-Configuration", NULL };

OptDef Options[] = { { SL_OPTION, SL_ALIAS, NULL, SL_USAGE, 1, false, false } };

const char UsageDefaultName[] = "vdb-passwd";

rc_t CC UsageSummary(const char *progname) {
    KOutMsg("Update user's NCBI VDB crypto password\n"
            "If not run with --quiet recommendations\n"
            "are given if errors are detected.\n"
            "\n");

    return 0;
}

rc_t CC Usage(const Args *args) {
    UsageSummary(UsageDefaultName);

    KOutMsg("Options:\n");
    HelpOptionsStandard ();

    HelpVersion(UsageDefaultName, KAppVersion());

    return 0;
}

#if 0
rc_t CC KMain(int argc, char *argv[]) {
    Args *args;
    rc_t rc; /*, orc; */

    rc = ArgsMakeAndHandle(&args, argc, argv, 0);
    if (rc == 0)
    {
        uint32_t pcount;
        rc_t orc;

        /* non standard use of --quiet */
        rc = ArgsOptionCount(args, OPTION_QUIET, &pcount);
        if (rc)
            LOGERR(klogErr, rc, "error check " OPTION_QUIET " option");

        else
            rc = run (pcount != 0);

        orc = ArgsWhack(args);
        if (rc == 0)
            rc = orc;
    }

    return rc;
}
#endif
rc_t CC KMain(int argc, char *argv[]) {
    OUTMSG((
"WARNING: vdb-passwd IS OBSOLETE AND SHOULD NOT BE USED.\n"
"\n"
"\n"
"To access dbGaP data you need:\n"
"\n"
"- Make sure you have the latest version of SRA Toolkit installed:\n"
"https://github.com/ncbi/sra-tools/wiki/Downloads\n"
"\n"
"- Have permission to access controlled-access data for a dbGaP project;\n"
"\n"
"- Get dbGaP repository key (ngc file);\n"
"\n"
"- Import the dbGaP repository key to SRA Toolkit.\n"
"  It will set up the project's workspace directory.\n"
"\n"
"- Change directory to the project's workspace.\n"
"\n"
"N.B. MAKE SURE YOU DO NOT HAVE A VDB_PWFILE ENVIRONMENT VARIABLE SET !!!\n"
"\n"
"Now you should be able to work with encrypted data.\n"
"The SRA Toolkit will work with encrypted SRA data files,\n"
"there is no need to decrypt the read data.\n"
"\n"
"The complete instructions are:\n"
"http://www.ncbi.nlm.nih.gov/Traces/sra/sra.cgi?view=toolkit_doc&f=dbgap_use\n"
"\n"
"Send questions/bug reports to sra-tools@ncbi.nlm.nih.gov\n"));
    return RC(rcExe, rcProcess, rcExecuting, rcProcess, rcUnsupported);
}
