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

#include <klib/out.h>
#include <klib/log.h>
#include <klib/rc.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/args-conv.h>

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))

#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = nullptr; } while (false)

#define OPTION_MODE         "mode"

#define ALIAS_MODE          "m"

static const char * mode_usage[]    = { "modes are: ...", nullptr };

OptDef ConverterOptions[] =
{
    { OPTION_MODE,          ALIAS_MODE,         nullptr, mode_usage,        1, true,    false },
};

const char UsageDefaultName[] = "sra-2-fastq";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <accession> [options]\n"
                    "\n", progname);
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == nullptr )
    {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    }
    else
    {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }

    if ( rc )
    {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );
    HelpOptionLine ( ALIAS_MODE,        OPTION_MODE,        nullptr, mode_usage );
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle( &args, argc, argv,
        1, ConverterOptions, sizeof ConverterOptions / sizeof ConverterOptions [ 0 ] );
    DISP_RC( rc, "ArgsMakeAndHandle() failed" );
    if ( rc == 0)
    {
        // the meat...

        DESTRUCT( Args, args );
    }

    return rc;
}
