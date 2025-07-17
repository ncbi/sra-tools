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

#include <sysalloc.h>

#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <kapp/log-xml.h>
#include <align/writer-refseq.h>
#include <insdc/sra.h>
#include <sra/sradb.h>
#include "common-writer.h"

#include "fastq-parse.h"

extern rc_t run ( char const progName[],
           CommonWriterSettings* G,
           unsigned seqFiles,
           const char *seqFile[],
           uint8_t qualityOffset,
           bool ignoreSpotGroups,
           const char* read1file,
           const char* read2file);

/* MARK: Arguments and Usage */
//static char const option_input[] = "input";
static char const option_output[] = "output";
static char const option_tmpfs[] = "tmpfs";
static char const option_qual_compress[] = "qual-quant";
static char const option_cache_size[] = "cache-size";
static char const option_max_err_count[] = "max-err-count";
static char const option_max_rec_count[] = "max-rec-count";
static char const option_platform[] = "platform";
static char const option_quality[] = "quality";
static char const option_max_err_pct[] = "max-err-pct";
static char const option_ignore_illumina_tags[] = "ignore-illumina-tags";
static char const option_no_readnames[] = "no-readnames";
static char const option_allow_duplicates[] = "allow_duplicates";
static char const option_read1[] = "read1PairFiles";
static char const option_read2[] = "read2PairFiles";

#define OPTION_INPUT option_input
#define OPTION_OUTPUT option_output
#define OPTION_TMPFS option_tmpfs
#define OPTION_QCOMP option_qual_compress
#define OPTION_CACHE_SIZE option_cache_size
#define OPTION_MAX_ERR_COUNT option_max_err_count
#define OPTION_MAX_REC_COUNT option_max_rec_count
#define OPTION_PLATFORM option_platform
#define OPTION_QUALITY option_quality
#define OPTION_MAX_ERR_PCT option_max_err_pct
#define OPTION_IGNORE_ILLUMINA_TAGS option_ignore_illumina_tags
#define OPTION_NO_READNAMES option_no_readnames
#define OPTION_ALLOW_DUPLICATES option_allow_duplicates
#define OPTION_READ1 option_read1
#define OPTION_READ2 option_read2


#define ALIAS_INPUT  "i"
#define ALIAS_OUTPUT "o"
#define ALIAS_TMPFS "t"
#define ALIAS_QCOMP "Q"
#define ALIAS_MAX_ERR_COUNT "E"
#define ALIAS_PLATFORM "p"
#define ALIAS_READ "r"
/* this alias is deprecated (conflicts with -q for --quiet): */
#define ALIAS_QUALITY "q"
#define ALIAS_DUPLICATES "a"
#define ALIAS_READ1 "1"
#define ALIAS_READ2 "2"

static
char const * output_usage[] =
{
    "Path and Name of the output database.",
    NULL
};

static
char const * tmpfs_usage[] =
{
    "Path to be used for scratch files.",
    NULL
};

static
char const * qcomp_usage[] =
{
    "Quality scores quantization level, can be a number (0: none - default, 1: 2bit, 2: 1bit), or a string like '1:10,10:20,20:30,30:-' (which is equivalent to 1).",
    NULL
};

static
char const * cache_size_usage[] =
{
    "Set the cache size in MB for the temporary indices",
    NULL
};

static
char const * mrc_usage[] =
{
    "Set the maximum number of records to process from the FASTQ file",
    NULL
};

static
char const * mec_usage[] =
{
    "Set the maximum number of errors to ignore from the FASTQ file",
    NULL
};

static
char const * use_platform[] =
{
    "Platform (ILLUMINA, LS454, SOLID, COMPLETE_GENOMICS, HELICOS, PACBIO, IONTORRENT, CAPILLARY)",
    NULL
};

static
char const * use_quality[] =
{
    "Quality encoding (PHRED_33, PHRED_64, LOGODDS)",
    NULL
};

char const * use_read1[] =
{
    "Default read number for this file is 1. Processing will be interleaved with the file specified in --read2PairFile|-r2",
    NULL
};

char const * use_read2[] =
{
    "Default read number for this file is 2. Processing will be interleaved with the file specified in --read1PairFile|-r1",
    NULL
};

static
char const * use_max_err_pct[] =
{
    "acceptable percentage of spots creation errors, default is 5",
    NULL
};

static
char const * use_ignore_illumina_tags[] =
{
    "ignore barcodes contained in Illumina-formatted names",
    NULL
};

static
char const * use_no_readnames[] =
{
    "drop original read names",
    NULL
};

static
char const * use_allow_duplicates[] =
{
    "allow duplicate read names in the same file",
    NULL
};

OptDef Options[] =
{
    /* order here is same as in param array below!!! */                                 /* max#,  needs param, required */
    { OPTION_OUTPUT,                ALIAS_OUTPUT,           NULL, output_usage,             1,  true,        true },
    { OPTION_TMPFS,                 ALIAS_TMPFS,            NULL, tmpfs_usage,              1,  true,        false },
    { OPTION_QCOMP,                 ALIAS_QCOMP,            NULL, qcomp_usage,              1,  true,        false },
    { OPTION_CACHE_SIZE,            NULL,                   NULL, cache_size_usage,         1,  true,        false },
    { OPTION_MAX_REC_COUNT,         NULL,                   NULL, mrc_usage,                1,  true,        false },
    { OPTION_MAX_ERR_COUNT,         ALIAS_MAX_ERR_COUNT,    NULL, mec_usage,                1,  true,        false },
    { OPTION_PLATFORM,              ALIAS_PLATFORM,         NULL, use_platform,             1,  true,        false },
    { OPTION_QUALITY,               ALIAS_QUALITY,          NULL, use_quality,              1,  true,        true },
    { OPTION_MAX_ERR_PCT,           NULL,                   NULL, use_max_err_pct,          1,  true,        false },
    { OPTION_IGNORE_ILLUMINA_TAGS,  NULL,                   NULL, use_ignore_illumina_tags, 1,  false,       false },
    { OPTION_NO_READNAMES,          NULL,                   NULL, use_no_readnames,         1,  false,       false },
    { OPTION_ALLOW_DUPLICATES,      ALIAS_DUPLICATES,       NULL, use_allow_duplicates,     1,  false,       false },
    { OPTION_READ1,                 ALIAS_READ1,            NULL, use_read1,                1,  true,        false },
    { OPTION_READ2,                 ALIAS_READ2,            NULL, use_read2,                1,  true,        false },
};

const char* OptHelpParam[] =
{
    /* order here is same as in OptDef array above!!! */
    "path",
    "path-to-file",
    "phred-score",
    "mbytes",
    "count",
    "count",
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    "path-to-file",
    "path-to-file"
};

rc_t UsageSummary (char const * progname)
{
    return KOutMsg (
        "Usage:\n"
        "\t%s [options] <fastq-file> ...\n"
        "\n"
        "Summary:\n"
        "\tLoad FASTQ formatted data files\n"
        "\n"
        "Example:\n"
        "\t%s -p 454 -o /tmp/SRZ123456 123456-1.fastq 123456-2.fastq\n"
        "\n"
        ,progname, progname);
}

char const UsageDefaultName[] = "fastq-load";

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
    XMLLogger_Usage();
    OUTMSG(("\n"));
    HelpOptionsStandard ();
    HelpVersion (fullpath, KAppVersion());
    return rc;
}

/* MARK: Definitions and Globals */

#define SCHEMAFILE "align/align.vschema"

CommonWriterSettings G;

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
static void set_pid(void)
{
    G.pid = getpid();
}

static bool platform_cmp(char const platform[], char const test[])
{
    unsigned i;

    for (i = 0; ; ++i) {
        int ch1 = test[i];
        int ch2 = toupper(platform[i]);

        if (ch1 != ch2)
            break;
        if (ch1 == 0)
            return true;
    }
    return false;
}

static INSDC_SRA_platform_id PlatformToId(const char* name)
{
    if (name != NULL)
    {
        switch (toupper(name[0])) {
        case 'C':
            if (platform_cmp(name, "CAPILLARY"))
                return SRA_PLATFORM_CAPILLARY;
            if (platform_cmp(name, "COMPLETE GENOMICS") || platform_cmp(name, "COMPLETE_GENOMICS"))
                return SRA_PLATFORM_COMPLETE_GENOMICS;
            break;
        case 'H':
            if (platform_cmp(name, "HELICOS"))
                return SRA_PLATFORM_HELICOS;
            break;
        case 'I':
            if (platform_cmp(name, "ILLUMINA"))
                return SRA_PLATFORM_ILLUMINA;
            if (platform_cmp(name, "IONTORRENT"))
                return SRA_PLATFORM_ION_TORRENT;
            break;
        case 'L':
            if (platform_cmp(name, "LS454"))
                return SRA_PLATFORM_454;
            break;
        case 'N':
            if (platform_cmp(name, "NANOPORE"))
                return SRA_PLATFORM_OXFORD_NANOPORE;
            break;
        case 'O':
            if (platform_cmp(name, "OXFORD_NANOPORE"))
                return SRA_PLATFORM_OXFORD_NANOPORE;
            break;
        case 'P':
            if (platform_cmp(name, "PACBIO"))
                return SRA_PLATFORM_PACBIO_SMRT;
            break;
        case 'S':
            if (platform_cmp(name, "SOLID"))
                return SRA_PLATFORM_ABSOLID;
            if (platform_cmp(name, "SANGER"))
                return SRA_PLATFORM_CAPILLARY;
            break;
        default:
            break;
        }
    }
    return SRA_PLATFORM_UNDEFINED;
}

static rc_t PathWithBasePath(char rslt[], size_t sz, char const path[], char const base[])
{
    size_t const plen = strlen(path);
    bool const hasBase = base && base[0];
    bool const isBareName = strchr(path, '/') == NULL;

    if (isBareName && hasBase) {
        if (string_printf(rslt, sz, NULL, "%s/%s", base, path) == 0)
            return 0;
    }
    else if ( string_copy ( rslt, sz, path, plen ) < sz )
            return 0;

    {
        rc_t const rc = RC(rcApp, rcArgv, rcAccessing, rcBuffer, rcInsufficient);
        (void)LOGERR(klogErr, rc, "The path to the file is too long");
        return rc;
    }
}

MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
        return VDB_INIT_FAILED;

    Args * args;
    rc_t rc;
    char *files[256];
    char *name_buffer = NULL;
    size_t next_name = 0;
    size_t nbsz = 0;
    char const *value;
    char *dummy;
    const XMLLogger* xml_logger = NULL;
    enum FASTQQualityFormat qualityFormat;
    bool ignoreSpotGroups;
    const char * read1file = NULL;
    const char * read2file = NULL;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    memset(&G, 0, sizeof(G));

    G.maxSeqLen = TableWriterRefSeq_MAX_SEQ_LEN;
    G.schemaPath = SCHEMAFILE;
    G.omit_aligned_reads = true;
    G.omit_reference_reads = true;
    G.minMapQual = 0; /* accept all */
    G.tmpfs = "/tmp";
#if _ARCH_BITS == 32
    G.cache_size = ( size_t ) 1 << 30;
#else
    G.cache_size = ( size_t ) 10 << 30;
#endif
    G.maxErrCount = 1000;
    G.maxErrPct = 5;
    G.acceptNoMatch = true;
    G.minMatchCount = 0;
    G.QualQuantizer="0";
    G.allowDuplicateReadNames = false;

    set_pid();

    rc = ArgsMakeAndHandle (&args, argc, argv, 2, Options,
                            sizeof Options / sizeof (OptDef), XMLLogger_Args, XMLLogger_ArgsQty);

    while (rc == 0) {
        uint32_t pcount;

        if( (rc = XMLLogger_Make(&xml_logger, NULL, args)) != 0 ) {
            break;
        }

        rc = ArgsOptionCount (args, OPTION_TMPFS, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_TMPFS, 0, (const void **)&G.tmpfs);
            if (rc)
                break;
        }
        else if (pcount > 1)
        {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcExcessive);
            OUTMSG (("Single parameter required\n"));
            MiniUsage (args);
            break;
        }

        rc = ArgsOptionCount (args, OPTION_OUTPUT, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_OUTPUT, 0, (const void **)&G.outpath);
            if (rc)
                break;
        }
        else if (pcount > 1)
        {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcExcessive);
            OUTMSG (("Single output parameter required\n"));
            MiniUsage (args);
            break;
        }
        else {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcInsufficient);
            OUTMSG (("Output parameter required\n"));
            MiniUsage (args);
            break;
        }

        rc = ArgsOptionCount (args, OPTION_QCOMP, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_QCOMP, 0, (const void **)&G.QualQuantizer);
            if (rc)
                break;
        }

        rc = ArgsOptionCount (args, OPTION_CACHE_SIZE, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_CACHE_SIZE, 0, (const void **)&value);
            if (rc)
                break;
            G.cache_size = strtoul(value, &dummy, 0) * 1024UL * 1024UL;
            if (G.cache_size == 0  || G.cache_size == ULONG_MAX) {
                rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcIncorrect);
                OUTMSG (("cache-size: bad value\n"));
                MiniUsage (args);
                break;
            }
        }

        G.expectUnsorted = true;

        rc = ArgsOptionCount (args, OPTION_MAX_REC_COUNT, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_MAX_REC_COUNT, 0, (const void **)&value);
            if (rc)
                break;
            G.maxAlignCount = strtoul(value, &dummy, 0);
        }

        rc = ArgsOptionCount (args, OPTION_MAX_ERR_COUNT, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_MAX_ERR_COUNT, 0, (const void **)&value);
            if (rc)
                break;
            G.maxErrCount = strtoul(value, &dummy, 0);
        }

        rc = ArgsOptionCount (args, OPTION_MAX_ERR_PCT, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_MAX_ERR_PCT, 0, (const void **)&value);
            if (rc)
                break;
            G.maxErrPct = strtoul(value, &dummy, 0);
        }

        rc = ArgsOptionCount (args, OPTION_PLATFORM, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_PLATFORM, 0, (const void **)&value);
            if (rc)
                break;
            G.platform = PlatformToId(value);
            if (G.platform == SRA_PLATFORM_UNDEFINED)
            {
                rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcIncorrect);
                (void)PLOGERR(klogErr, (klogErr, rc, "Invalid platform $(v)",
                            "v=%s", value));
                break;
            }
        }
        else
            G.platform = SRA_PLATFORM_UNDEFINED;

        rc = ArgsOptionCount (args, OPTION_QUALITY, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_QUALITY, 0, (const void **)&value);
            if (rc)
                break;
            if (strcmp(value, "PHRED_33") == 0)
                qualityFormat = FASTQphred33;
            else if (strcmp(value, "PHRED_64") == 0)
                qualityFormat = FASTQphred64;
            else if (strcmp(value, "LOGODDS") == 0)
                qualityFormat = FASTQlogodds;
            else
            {
                rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcIncorrect);
                (void)PLOGERR(klogErr, (klogErr, rc, "Invalid quality encoding $(v)",
                            "v=%s", value));
                break;
            }
        }
        else
            qualityFormat = 0;

        rc = ArgsOptionCount (args, OPTION_IGNORE_ILLUMINA_TAGS, &pcount);
        if (rc)
            break;
        ignoreSpotGroups = pcount > 0;

        rc = ArgsOptionCount (args, OPTION_NO_READNAMES, &pcount);
        if (rc)
            break;
        G.dropReadnames = pcount > 0;

        rc = ArgsOptionCount (args, OPTION_ALLOW_DUPLICATES, &pcount);
        if (rc)
            break;
        G.allowDuplicateReadNames = pcount > 0;

        rc = ArgsOptionCount (args, OPTION_READ1, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_READ1, 0, (const void **) & read1file);
            if (rc)
                break;
        }

        rc = ArgsOptionCount (args, OPTION_READ2, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_READ2, 0, (const void **) & read2file);
            if (rc)
                break;
        }

        rc = ArgsParamCount (args, &pcount);
        if (rc) break;
        if (pcount == 0 && read1file == NULL && read2file == NULL)
        {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcInsufficient);
            MiniUsage (args);
            break;
        }
        else if (pcount > sizeof(files)/sizeof(files[0])) {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcExcessive);
            (void)PLOGERR(klogErr, (klogErr, rc, "$(count) input files is too many, $(max) is the limit",
                        "count=%u,max=%u", (unsigned)pcount, (unsigned)(sizeof(files)/sizeof(files[0]))));
            break;
        }
        else {
            size_t need = G.inpath ? (strlen(G.inpath) + 1) * pcount : 0;
            unsigned i;

            for (i = 0; i < pcount; ++i) {
                rc = ArgsParamValue(args, i, (const void **)&value);
                if (rc) break;
                need += strlen(value) + 1;
            }
            nbsz = need;
        }

        name_buffer = malloc(nbsz);
        if (name_buffer == NULL) {
            rc = RC(rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted);
            break;
        }

        rc = ArgsParamCount (args, &pcount);
        if (rc == 0) {
            unsigned i;

            for (i = 0; i < pcount; ++i) {
                rc = ArgsParamValue(args, i, (const void **)&value);
                if (rc) break;

                files[i] = name_buffer + next_name;
                rc = PathWithBasePath(name_buffer + next_name, nbsz - next_name, value, G.inpath);
                if (rc) break;
                next_name += strlen(name_buffer + next_name) + 1;
            }
        }
        else
            break;

        rc = run(argv[0], &G, pcount, (char const **)files, qualityFormat, ignoreSpotGroups, read1file, read2file);
        break;
    }
    free(name_buffer);

    value = G.outpath ? strrchr(G.outpath, '/') : "/???";
    if( value == NULL ) {
        value = G.outpath;
    } else {
        value++;
    }
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "load failed",
                "severity=total,status=failure,accession=%s,errors=%u", value, G.errCount));
    } else {
        (void)PLOGMSG(klogInfo, (klogInfo, "loaded",
                "severity=total,status=success,accession=%s,errors=%u", value, G.errCount));
    }
    ArgsWhack(args);
    XMLLogger_Release(xml_logger);
    return VdbTerminate( rc );
}
