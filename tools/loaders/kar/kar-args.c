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

#include "kar-args.h"

#include <klib/rc.h>
#include <kfs/sra.h>
#include <klib/log.h>
#include <klib/out.h>

#include <kapp/main.h>


static const char * create_usage[] = { "Create a new archive.", NULL };
static const char * test_usage[] = { "Check the structural validity of an archive", NULL };
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
static const char * stdout_usage[] = { "Direct output to stdout", NULL };
static const char * md5_usage[] = { "create md5sum-compatible checksum file", NULL };


OptDef Options [] =
{
    { OPTION_CREATE,    ALIAS_CREATE,    NULL, create_usage, 1, true,  false },
    { OPTION_TEST,      ALIAS_TEST,      NULL, test_usage, 1, true,  false },
    { OPTION_EXTRACT,   ALIAS_EXTRACT,   NULL, extract_usage, 1, true,  false },
    { OPTION_FORCE,     ALIAS_FORCE,     NULL, force_usage, 0, false, false },
    { OPTION_LONGLIST,  ALIAS_LONGLIST,  NULL, longlist_usage, 0, false, false },
    { OPTION_DIRECTORY, ALIAS_DIRECTORY, NULL, directory_usage, 1, true,  false },
    { OPTION_STDOUT,    ALIAS_STDOUT,    NULL, stdout_usage, 1, true,  false },
    { OPTION_MD5,       NULL,            NULL, md5_usage, 1, false,  false }
};

const char UsageDefaultName[] = "kar";


rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("Usage:\n"
                    "  %s [OPTIONS] -%s|--%s <Archive> -%s|--%s <Directory> [Filter ...]\n"
                    "  %s [OPTIONS] -%s|--%s <Archive> -%s|--%s <Directory>\n"
                    "  %s [OPTIONS] -%s|--%s|--%s <Archive>\n"
                    "\n"
                    "Summary:\n"
                    "  Create, extract from, or test an archive.\n"
                    "\n",
                    progname, ALIAS_CREATE, OPTION_CREATE, ALIAS_DIRECTORY, OPTION_DIRECTORY,
                    progname, ALIAS_EXTRACT, OPTION_EXTRACT, ALIAS_DIRECTORY, OPTION_DIRECTORY,
                    progname, ALIAS_TEST, OPTION_TEST, OPTION_LONGLIST);
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

    HelpOptionLine (ALIAS_STDOUT, OPTION_STDOUT, NULL, stdout_usage);
    HelpOptionLine ( NULL, OPTION_MD5, NULL, md5_usage);

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

static
rc_t parse_params_int ( Params *p, const Args *args )
{
    rc_t rc;

    uint32_t count;

    /* Parameters */
    rc = ArgsParamCount ( args, &count );
    if ( rc != 0 )
    {
        LogErr ( klogFatal, rc, "Failed to retrieve parameter count" );
        return rc;
    }
    else
    {
        uint32_t i;
        const char *value;

        p -> mem_count = count;

        for ( i = 0; i < count; ) /*Increment in value insertion into members*/
        {
            rc = ArgsParamValue ( args, i, ( const void ** ) &value );
            if ( rc != 0 )
            {
                LogErr ( klogFatal, rc, "Failed to retrieve parameter value" );
                return rc;
            }

            p -> members [ ++i ] = value;
        }
    }

    /* Bool  Options */
    rc = ArgsOptionCount ( args, OPTION_LONGLIST, &count );
    if ( rc == 0 && count != 0 )
        p -> long_list = true;

    rc = ArgsOptionCount ( args, OPTION_FORCE, &count );
    if ( rc == 0 && count != 0 )
        p -> force = true;

    rc = ArgsOptionCount ( args, OPTION_STDOUT, &count );
    if ( rc == 0 && count != 0 )
        p -> stdout = true;

    rc = ArgsOptionCount ( args, OPTION_MD5, &count );
    if ( rc == 0 && count != 0 )
        p -> md5sum = true;

    /* Options */
    rc = ArgsOptionCount ( args, OPTION_CREATE, & p -> c_count );
    if ( rc != 0 )
    {
        LogErr ( klogFatal, rc, "Failed to verify 'create' option" );
        return rc;
    }

    /* grab the p->archive_path as an option parameter if p->c_count > 0 */
    if ( p -> c_count > 0 )
    {
        rc = ArgsOptionValue (args, OPTION_CREATE, 0, (const void **) & p -> archive_path );
        if ( rc != 0 )
        {
            LogErr ( klogFatal, rc, "Failed to access 'create' archive path" );
            return rc;
        }
    }

    rc = ArgsOptionCount ( args, OPTION_EXTRACT, &p -> x_count );
    if ( rc != 0 )
    {
        LogErr ( klogFatal, rc, "Failed to verify 'extract' option" );
        return rc;
    }

    /* grab the p->archive_path as an option parameter if p->x_count > 0 */
    if ( p -> x_count > 0 )
    {
        rc = ArgsOptionValue (args, OPTION_EXTRACT, 0, (const void **) & p -> archive_path );
        if ( rc != 0 )
        {
            LogErr ( klogFatal, rc, "Failed to access 'extract' archive path" );
            return rc;
        }
    }

    rc = ArgsOptionCount ( args, OPTION_TEST, &p -> t_count );
    if ( rc != 0 )
    {
        LogErr ( klogFatal, rc, "Failed to verify 'test' option" );
        return rc;
    }

    /* grab the p->archive_path as an option parameter if p->t_count > 0 */
    if ( p -> t_count > 0 )
    {
        rc = ArgsOptionValue (args, OPTION_TEST, 0, (const void **) & p -> archive_path );
        if ( rc != 0 )
        {
            LogErr ( klogFatal, rc, "Failed to access 'test' archive path" );
            return rc;
        }
    }

    /* need to grab the directory option */
    rc = ArgsOptionCount ( args, OPTION_DIRECTORY, &p -> dir_count );
    if ( rc != 0 )
    {
        LogErr ( klogFatal, rc, "Failed to verify 'directory' option" );
        return rc;
    }

    if ( p -> dir_count > 0 )
    {
        rc = ArgsOptionValue ( args, OPTION_DIRECTORY, 0, ( const void ** ) &p -> directory_path );
        if ( rc != 0 )
        {
            LogErr ( klogFatal, rc, "Failed to access directory path" );
            return rc;
        }
    }

    return rc;
}

rc_t parse_params ( Params *p, Args **args, int argc, char * argv [] )
{
    SetUsage( Usage );
    SetUsageSummary( UsageSummary );
        
    rc_t rc;

    p -> members = ( const char ** ) argv;
    p -> archive_path = "";
    p -> directory_path = "";
    p -> mem_count = 0;
    p -> dir_count = 0;
    p -> c_count = 0;
    p -> x_count = 0;
    p -> t_count = 0;
    p -> long_list = false;
    p -> force = false;
    p -> stdout = false;

    rc = ArgsMakeAndHandle ( args, argc, argv, 1,
        Options, sizeof Options / sizeof ( Options [ 0 ] ) );
    if ( rc == 0 )
    {
        rc = parse_params_int ( p, *args );

        if ( rc == 0 )
            rc = validate_params ( p );

    }

    return rc;
}

rc_t validate_params ( Params *p )
{
    rc_t rc = 0;
    uint32_t i;

    /* must have a valid mode */
    uint32_t cxt_count = p -> c_count + p -> x_count + p -> t_count;
    if ( cxt_count == 0 )
    {
        rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
        LogErr ( klogErr, rc, "Require at least one option of create|extract|test" );
        return rc;
    }
    else if ( cxt_count > 1 )
    {
        rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcExcessive );
        LogErr ( klogErr, rc, "Too many option parameters" );
        return rc;
    }

    /* if member count > 0, must be in create mode */
    if ( p -> mem_count > 0 )
    {
        if ( p -> c_count == 0 )
        {
            rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInvalid );
            LogErr ( klogErr, rc, "Must use create option" );
            return rc;
        }
    }

    /* if creating, must have a directory OR member count > 0 */
    if ( p -> c_count != 0 )
    {
        if ( ! ( p -> mem_count != 0 || p -> dir_count != 0 ) )
        {
            rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
            LogErr ( klogErr, rc, "Must provide an input directory or file paths when creating " );
            return rc;
        }
    }

    /* if extracting, must have a directory */
    if ( p -> x_count != 0 )
    {
        if ( p -> dir_count == 0 )
        {
            rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInvalid );
            LogErr ( klogErr, rc, "Must provide a directory path when extracting" );
            return rc;
        }
    }

    /* test the archive path */


    /* TBD stdout and md5 */

    /* if we have parameter list, check each parameter for non-NULL and non-empty */
    for ( i = 1; i <= p -> mem_count; ++ i )
    {
        const char * mbr = p -> members [ i ];
        if ( mbr == NULL )
        {
            rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
            LogErr ( klogErr, rc, " " );
            return rc;
        }

        if ( mbr [ 0 ] == 0 )
        {
            rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
            LogErr ( klogErr, rc, " " );
            return rc;
        }
    }

    /* if we have a directory, check non-NULL and non-empty */
    if ( p -> dir_count != 0 )
    {
        if ( p -> directory_path == NULL )
        {
            rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
            LogErr ( klogErr, rc, " " );
            return rc;
        }

        if ( p -> directory_path [ 0 ] == 0 )
        {
            rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
            LogErr ( klogErr, rc, " " );
            return rc;
        }
    }

    return rc;
}

