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

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <loader/common-writer.h>
#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <kapp/log-xml.h>
#include <align/writer-refseq.h>

static char const option_input[] = "input";
static char const option_output[] = "output";

#define OPTION_INPUT option_input
#define OPTION_OUTPUT option_output

#define ALIAS_INPUT  "i"
#define ALIAS_OUTPUT "o"

static
char const * output_usage[] = 
{
    "Path and Name of the output database.",
    NULL
};

OptDef Options[] = 
{
    /* order here is same as in param array below!!! */               /* max#,  needs param, required */
    { OPTION_OUTPUT,        ALIAS_OUTPUT,           NULL, output_usage,     1,  true,        true },
};

const char* OptHelpParam[] =
{
    /* order here is same as in OptDef array above!!! */
    "path",
};

rc_t UsageSummary (char const * progname)
{
    return KOutMsg (
        "Usage:\n"
        "\t%s [options] <fastq-file> ...\n"
        "\n"
        "Summary:\n"
        "\tLoad VCF formatted data files\n"
        "\n"
        "Example:\n"
        "\t%s -o /tmp/SRZ123456 123456-1.vcf 123456-2.vcf\n"
        "\n"
        ,progname, progname);
}

char const UsageDefaultName[] = "vcf-load";

rc_t CC Usage (const Args * args)
{
    rc_t rc;
    int i;
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    const size_t argsQty = sizeof(Options) / sizeof(Options[0]);

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    for(i = 0; i < argsQty; i++ ) {
        if( Options[i].required && Options[i].help[0] != NULL ) {
            HelpOptionLine(Options[i].aliases, Options[i].name, OptHelpParam[i], Options[i].help);
        }
    }
    OUTMSG(("\nOptions:\n"));
    for(i = 0; i < argsQty; i++ ) {
        if( !Options[i].required && Options[i].help[0] != NULL ) {
            HelpOptionLine(Options[i].aliases, Options[i].name, OptHelpParam[i], Options[i].help);
        }
    }
    OUTMSG(("\n"));
    HelpOptionsStandard ();
    HelpVersion (fullpath, KAppVersion());
    return rc;
}

rc_t CC KMain (int argc, char * argv[])
{
    return 0;
}
