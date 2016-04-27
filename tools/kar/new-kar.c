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

#define OPTION_CREATE    "create"
#define OPTION_EXTRACT   "extract"
#define OPTION_TEST      "test"
#define OPTION_FORCE     "force"
#define OPTION_LONGLIST  "long-list"
#define OPTION_DIRECTORY "directory"

#define ALIAS_CREATE     "c"
#define ALIAS_EXTRACT    "x"
#define ALIAS_TEST       "t"
#define ALIAS_FORCE      "f"
#define ALIAS_LONGLIST   "l"
#define ALIAS_DIRECTORY  "d"

static const char * create_usage[] = { "Create a new archive.", NULL };
static const char * test_usage[] = { "Check the structural validity of an archive",
static const char * extract_usage[] = { "Extract the contents of an archive into a directory.", NULL };
static const char * force_usage[] =
{ "(no parameter) this will cause the extract or",
  "create to over-write existing files unless",
  "they are write-protected.  Without this",
  "option the program will fail if the archive",
  "already exists for a create or the target",
  "directory exists for an extract", NULL };
static const char * longlist_usage[] =
{ "more information will be given on each file",
  "in test/list mode.", NULL };
static const char * directory_usage[] = 
{ "The next token on the command line is the",
  "name of the directory to extract to or create",
  "from", NULL };

OptDef Options [] = 
{
    { OPTION_CREATE,    ALIAS_CREATE,    NULL, create_usage, 1, true,  false },
    { OPTION_TEST,      ALIAS_TEST,      NULL, test_usage, 1, true,  false },
    { OPTION_EXTRACT,   ALIAS_EXTRACT,   NULL, extract_usage, 1, true,  false },
    { OPTION_FORCE,     ALIAS_FORCE,     NULL, force_usage, 0, false, false },
    { OPTION_LONGLIST,  ALIAS_LONGLIST,  NULL, longlist_usage, 0, false, false },
    { OPTION_DIRECTORY, ALIAS_DIRECTORY, NULL, directory_usage, 1, true,  false },
}

const char UsageDefaultName[] = "kar";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [OPTIONS] -%s|--%s <Archive> -%s|--%s <Directory> [Filter ...]\n"
                    "  %s [OPTIONS] -%s|--%s <Archive> -%s|--%s <Directory>\n"
                    "  %s [OPTIONS] -%s|--%s|--%s <Archive>\n"
                    "\n"
                    "Summary:\n"
                    "  Create, extract from, or test an archive.\n"
                    "\n",
                    progname, ALIAS_CREATE, OPTION_CREATE, ALIAS_DIRECTORY, OPTION_DIRECTORY,
                    progname, ALIAS_EXTRACT, OPTION_EXTRACT, ALIAS_DIRECTORY, OPTION_DIRECTORY,
                    progname, ALIAS_TEST, OPTION_TEST, OPTION_LIST);
}

rc_t CC Usage (const Args * args)
{
    static const char archive[] = "archive";
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    OUTMSG (("Archive Command:\n"
	     "  All of these options require the next token on the command line to be\n"
	     "  the name of the archive\n\n"));

    KOutMsg ("Options:\n");

    HelpOptionLine (ALIAS_CREATE, OPTION_CREATE, archive, create_usage);
    HelpOptionLine (ALIAS_EXTRACT, OPTION_EXTRACT, archive, extract_usage);
    HelpOptionLine (ALIAS_TEST, OPTION_TEST, archive, test_usage);
    OUTMSG (("\n"
             "Archive:\n"
             "  Path to a file that will/does hold the archive of other files.\n"
             "  This can be a full or relative path.\n"
             "\n"
             "Directory:\n"
	     "  Required for create or extract command, ignored for test command.\n"
             "  This can be a full or relative path.\n"
             "\n"
             "Filters:\n"
	     "  When present these act as include filters.\n"
	     "  Any file name will be included in the extracted files, created archive\n"
	     "  or test operation listing\n"
	     "  Any directory will be included as well as its contents\n"
             "\n"
             "Options:\n"));
    HelpOptionLine (ALIAS_DIRECTORY, OPTION_DIRECTORY, "Directory", directory_usage);
    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, force_usage);
    HelpOptionLine (ALIAS_LONGLIST, OPTION_LONGLIST, NULL, longlist_usage);

    HelpOptionsStandard ();

    OUTMSG (("\n"
             "Use examples:"
             "\n"
             "  To create an archive named 'example.sra' that contains the same\n"
             "  contents as a subdirectory 'example' of the current directory\n"
             "\n"
             "  $ %s --%s example.sra --%s example\n",
             progname, OPTION_CREATE, OPTION_DIRECTORY));

    OUTMSG (("\n"
             "  To replace an existing archive named 'example.sra' with another that contains\n"
             "  the same contents as a subdirectory 'example' of the current directory\n"
             "\n"
             "  $ %s -%s -%s example.sra -%s example\n",
             progname, ALIAS_FORCE, ALIAS_CREATE, ALIAS_DIRECTORY));

    OUTMSG (("\n"
             "  To examine in detail the contents of an archive named 'example.sra'\n"
             "\n"
             "  $ %s --%s --%s example.sra\n",
             progname, OPTION_LONGLIST, OPTION_TEST));

    OUTMSG (("\n"
             "  To extract the files from an archive named 'example.sra' into\n"
             "  a subdirectory 'example' of the current directory.\n"
             "  NOTE: all extracted files will be read only.\n"
             "\n"
             "  $ %s --%s example.sra --%s example\n",
             progname, OPTION_EXTRACT, OPTION_DIRECTORY));


    HelpVersion (fullpath, KAppVersion());

    return rc;
}

typedef enum op_mode
{
    OM_NONE = 0,
    OM_CREATE,
    OM_EXTRACT,
    OM_TEST,
    OM_ERROR
} op_mode;

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc;

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof Options / sizeof (OptDef));
    if (rc == 0)
    {
        uint32_t pcount;
        const char * pc;
        const char * directory;
        const char * archive;

        do
        {
            op_mode mode;
            uint32_t ix;

            BSTreeInit (&pnames);

            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;
            for (ix = 0; ix < pcount; ++ix)
            {
                rc = ArgsParamValue (args, ix, (const void **)&pc);
                if (rc)
                    break;
                rc = pnamesInsert (pc);
                if (rc)
                {
                    PLOGERR (klogInt, (klogInt, rc, "failure to process filter parameter [%s]", pc));
                    break;
                }
            }
            if (rc)
                break;

            rc = ArgsOptionCount (args, OPTION_LONGLIST, &pcount);
            if (rc)
                break;
            long_list = (pcount != 0);

            rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
            if (rc)
                break;
            force = (pcount != 0);

            mode = OM_NONE;

            rc = ArgsOptionCount (args, OPTION_DIRECTORY, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                rc = ArgsOptionValue (args, OPTION_DIRECTORY, 0, (const void **)&directory);
                if (rc)
                    break;

            }
            else
                directory = NULL;

            rc = ArgsOptionCount (args, OPTION_CREATE, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                rc = ArgsOptionValue (args, OPTION_CREATE, 0, (const void **)&archive);

                if (rc)
                    break;
                mode = OM_CREATE;
            }

            rc = ArgsOptionCount (args, OPTION_TEST, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                if (mode == OM_NONE)
                {
                    mode = OM_TEST;
                    rc = ArgsOptionValue (args, OPTION_TEST, 0, (const void **)&archive);
                    if (rc)
                        break;
                }
                else
                    mode = OM_ERROR;
           }

            rc = ArgsOptionCount (args, OPTION_EXTRACT, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                if (mode == OM_NONE)
                {
                    mode = OM_EXTRACT;
                    rc = ArgsOptionValue (args, OPTION_EXTRACT, 0, (const void **)&archive);
                    if (rc)
                        break;
                }
                else
                    mode = OM_ERROR;
            }


            if (mode == OM_NONE)
            {
                MiniUsage(args);
                rc = RC(rcApp, rcArgv, rcParsing, rcParam, rcInsufficient);
                break;
            }


            /* -----
             * Mount the native filesystem as root
             */
            rc = KDirectoryNativeDir (&kdir);
            if (rc != 0)
            {
                LOGMSG (klogFatal, "Failed to open native KDirectory");
            }
            else
            {
                switch (mode)

                { 
                case OM_ERROR:
                    rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcAmbiguous);
                    LOGERR (klogFatal, rc, "Must provide a single operating mode and archive: Create, Test (list) or eXtract");
                    break;
                case OM_CREATE:
                    STSMSG (2, ("Create Mode %s %s", archive, directory));
                    if (directory == NULL)
                    {
                        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcNull);
                        LOGERR (klogFatal, rc, "Must provide a directory for create mode");
                    }
                    else
                        rc = run_kar_create (archive, directory);
                    break;
                case OM_EXTRACT:
                    STSMSG (2, ("Extract Mode %s %s", archive, directory));
                    if (directory == NULL)
                    {
                        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcNull);
                        LOGERR (klogFatal, rc, "Must provide a directory for extract mode");
                    }
                    else
                        rc = run_kar_extract (archive, directory);
                    break;
                case OM_TEST:
                    STSMSG (2, ("Test Mode %s", archive));
                    rc = run_kar_test (archive);
                    break;
                default: /* OM_NONE presumably */
/*                     assert (mode == OM_NONE); */
/*                     rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcEmpty); */
/*                     LOGERR (klogFatal, rc, "Must provide a single operating mode and archive: Create, Test (list) or eXtract"); */
                    pc = NULL;
                    break;

                }

                KDirectoryRelease (kdir);
            }

        } while (0);

        ArgsWhack (args);
    }
    if (rc)
        LOGERR (klogWarn, rc, "Failure exiting kar");
    else
        STSMSG (1, ("Success: Exiting kar\n"));
    return rc;
}
