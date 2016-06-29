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

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/rc.h>
#include <klib/printf.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kapp/log-xml.h>
#include <align/writer-refseq.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <limits.h>

#include "Globals.h"
#include "loader-imp.h"

/*: ARGS
Summary:
	Load a BAM formatted data file

Usage:
    --help                          display this text and quit
    --version                       display the version string and quit
    [global-options] [options] <file...> [ --remap [options] <file>... ]...

//:global-options
Global Options:

* options effecting logging
  log-level <level>                 logging level values: [fatal|sys|int|err|warn|info|0-5] default: info
  xml-log <filename>                produce an XML-formatted log file

* options effecting performance optimisation
  tmpfs <directory>                 where to store temparary files, default: '/tmp'
  cache-size <mbytes>               the limit in MB for temparary files

* options effecting error limits
  max-err-count <number>            the maximum number of errors to ignore
  max-warning-dup-flag <count>      the limit for number of duplicate flag mismatch warnings

//:options
Options:
  output <name>                     name of the output, required
  config <file>                     reference configuration file (See Configuration)
  header <file>                     file containing the SAM header
  remap                             special option to enable processing sets of remapped files. remap MUST be given between each set, all regular options can be respecified, in fact the output must be unique for each set. This is for when a set of reads are aligned multiple times, for example against different reference builds or with different aligners. This mode ensures that spot ids are the same across the several outputs.

Debugging Options:
  only-verify                       exit after verifying existence of references
  max-rec-count <number>            exit after processing this many records (per file)
  nomatch-log <path>                log alignments with no matching bases

Filtering Options:
  minimum-match <number>            minimum number of matches for an alignment
  no-secondary                      ignore alignments marked as secondary
  accept-dups                       accept spots with inconsistent PCR duplicate flags
  accept-nomatch                    accept alignments with no matching bases
  ref-config                        limit processing to references in the config file, ignoring all others
  ref-filter <name>                 limit processing to the given reference, ignoring all others
  min-mapq <number>                 filter secondary alignments by minimum mapping quality

Rare or Esoteric Options:
  input <directory>                 where to find fasta files
  ref-file <file>                   fasta file with references
  unsorted                          expect unsorted input (requires more memory)
  sorted                            require sorted input
  TI                                look for trace id optional tag
  unaligned <file>                  file without aligned reads

Deprecated Options:
  use-OQ                            use OQ option column for quality values instead of QUAL
  no-verify                         skip verify existence of references from the BAM file
  accept-hard-clip                  allow hard clipping in CIGAR
  allow-multi-map                   allow the same reference to be mapped to multiple names in the input files
  edit-aligned-qual <number>        convert quality at aligned positions to this value
  cs                                turn on awareness of colorspace
  qual-quant                        quality scores quantization level
  keep-mismatch-qual                don't quantized quality at mismatched positions


Example:
    bam-load -o /tmp/SRZ123456 -k analysis.bam.cfg 123456.bam
*/

/* MARK: Arguments and Usage */
static char const option_input[] = "input";
static char const option_output[] = "output";
static char const option_tmpfs[] = "tmpfs";
static char const option_config[] = "config";
static char const option_min_mapq[] = "min-mapq";
static char const option_qual_compress[] = "qual-quant";
static char const option_cache_size[] = "cache-size";
static char const option_unsorted[] = "unsorted";
static char const option_sorted[] = "sorted";
static char const option_max_err_count[] = "max-err-count";
static char const option_max_rec_count[] = "max-rec-count";
static char const option_no_verify[] = "no-verify";
static char const option_only_verify[] = "only-verify";
static char const option_use_qual[] = "use-QUAL";
static char const option_ref_filter[] = "ref-filter";
static char const option_ref_config[] = "ref-config";
static char const option_edit_aligned_qual[] = "edit-aligned-qual";
static char const option_unaligned[] = "unaligned";
static char const option_accept_dup[] = "accept-dups";
static char const option_accept_nomatch[] = "accept-nomatch";
static char const option_nomatch_log[] = "nomatch-log";
static char const option_keep_mismatch_qual[] = "keep-mismatch-qual";
static char const option_min_match[] = "minimum-match";
static char const option_header[] = "header";
static char const option_no_cs[] = "no-cs";
static char const option_no_secondary[] = "no-secondary";
static char const option_ref_file[] = "ref-file";
static char const option_TI[] = "TI";
static char const option_max_warn_dup_flag[] = "max-warning-dup-flag";
static char const option_accept_hard_clip[] = "accept-hard-clip";
static char const option_allow_multi_map[] = "allow-multi-map";
static char const option_allow_secondary[] = "make-spots-with-secondary";
static char const option_defer_secondary[] = "defer-secondary";

#define OPTION_INPUT option_input
#define OPTION_OUTPUT option_output
#define OPTION_TMPFS option_tmpfs
#define OPTION_CONFIG option_config
#define OPTION_MINMAPQ option_min_mapq
#define OPTION_QCOMP option_qual_compress
#define OPTION_CACHE_SIZE option_cache_size
#define OPTION_MAX_ERR_COUNT option_max_err_count
#define OPTION_MAX_REC_COUNT option_max_rec_count
#define OPTION_UNALIGNED option_unaligned
#define OPTION_ACCEPT_DUP option_accept_dup
#define OPTION_ACCEPT_NOMATCH option_accept_nomatch
#define OPTION_NOMATCH_LOG option_nomatch_log
#define OPTION_MIN_MATCH option_min_match
#define OPTION_HEADER option_header
#define OPTION_NO_CS option_no_cs
#define OPTION_NO_SECONDARY option_no_secondary
#define OPTION_REF_FILE option_ref_file
#define OPTION_TI option_TI
#define OPTION_MAX_WARN_DUP_FLAG option_max_warn_dup_flag
#define OPTION_ACCEPT_HARD_CLIP option_accept_hard_clip
#define OPTION_ALLOW_MULTI_MAP option_allow_multi_map
#define OPTION_ALLOW_SECONDARY option_allow_secondary
#define OPTION_DEFER_SECONDARY option_defer_secondary

#define ALIAS_INPUT  "i"
#define ALIAS_OUTPUT "o"
#define ALIAS_TMPFS "t"
#define ALIAS_CONFIG "k"
#define ALIAS_MINMAPQ "q"
#define ALIAS_QCOMP "Q"
#define ALIAS_MAX_ERR_COUNT "E"
#define ALIAS_UNALIGNED "u"
#define ALIAS_ACCEPT_DUP "d"
#define ALIAS_NO_SECONDARY "P"
#define ALIAS_REF_FILE "r"

static
char const * input_usage[] = 
{
    "Path where to get fasta files from.",
    NULL
};

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
char const * config_usage[] = 
{
    "Path to configuration file:",
    "maps the input BAM file's reference names to the equivalent GenBank accession.",
    "It is a tab-delimited text file with unix line endings (\\n - LF) with the following fields in this order:",
    "#1 reference name as it occurs in the BAM file's SN field of @SQ header record;",
    "#2 INSDC reference ID",
    NULL
};

static
char const * min_mapq_usage[] = 
{
    "Filter secondary alignments by minimum mapping quality.",
    NULL
};

static
char const * qcomp_usage[] = 
{
    "Quality scores quantization level, can be a number (0: none, 1: 2bit, 2: 1bit), or a string like '1:10,10:20,20:30,30:-' (which is equivalent to 1) (nb. the endpoint is exclusive).",
    NULL
};

static
char const * unsorted_usage[] = 
{
    "Tell the loader to expect unsorted input (requires more memory)",
    NULL
};

static
char const * sorted_usage[] =
{
    "Tell the loader to require sorted input",
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
    "Set the maximum number of records to process from the BAM file",
    NULL
};

static
char const * mec_usage[] = 
{
    "Set the maximum number of errors to ignore from the BAM file",
    NULL
};

static
char const * no_verify_usage[] = 
{
    "Skip verify existence of references from the BAM file",
    NULL
};

static
char const * only_verify_usage[] = 
{
    "Exit after verifying existence of references from the BAM file",
    NULL
};

static
char const * use_QUAL_usage[] = 
{
    "use QUAL column for quality values (default is to use OQ if it is available)",
    NULL
};

static
char const * use_ref_filter[] = 
{
    "Only process alignments to the given reference",
    NULL
};

static
char const * use_ref_config[] = 
{
    "Only process alignments to references in the config file",
    NULL
};

static
char const * use_edit_aligned_qual[] = 
{
    "Convert quality at aligned positions to this value",
    NULL
};

static
char const * use_keep_mismatch_qual[] = 
{
    "Don't quantized quality at mismatched positions",
    NULL
};

static
char const * use_unaligned[] = 
{
    "Specify file without aligned reads",
    NULL
};

static
char const * use_accept_dups[] = 
{
    "Accept spots inconsistent PCR duplicate flags",
    NULL
};

static
char const * use_accept_nomatch[] = 
{
    "Accept alignments with no matching bases",
    NULL
};

static
char const * use_nomatch_log[] = 
{
    "Where to write info for alignments with no matching bases",
    NULL
};

static
char const * use_min_match[] = 
{
    "minimum number of matches for an alignment",
    NULL
};

static
char const * use_header[] = 
{
    "path to a file containing the SAM header to store in the resulting cSRA, recommended in case of multiple input BAMs",
    NULL
};

static
char const * use_no_cs[] = 
{
    "turn off awareness of colorspace",
    NULL
};

static
char const * use_no_secondary[] = 
{
    "ignore alignments marked as secondary",
    NULL
};

static
char const * use_ref_file[] = 
{
    "path to a fasta file with references",
    NULL
};

static
char const * use_TI[] = 
{
    "for trace alignments",
    NULL
};

static
char const * use_max_dup_warnings[] = 
{
    "set limit for number of duplicate flag mismatch warnings",
    NULL
};

static
char const * use_accept_hard_clip[] = 
{
    "accept hard clipping in CIGAR",
    NULL
};

static
char const * use_allow_multi_map[] =
{
    "allow the same reference to be mapped to multiple names in the input files",
    "(default is disallow, old behaviors was to allow it)",
    NULL
};

static
char const * use_allow_secondary[] =
{
    "use secondary alignments for constructing spots",
    NULL
};

static
char const * use_defer_secondary[] =
{
    "defer processing of secondary alignments until the end of the file",
    NULL
};

OptDef Options[] = 
{
    /* order here is same as in param array below!!! */
    { OPTION_INPUT, ALIAS_INPUT,  NULL,  input_usage, 1, true,  false },
    { OPTION_OUTPUT, ALIAS_OUTPUT, NULL, output_usage, 1, true,  true },
    { OPTION_CONFIG, ALIAS_CONFIG,  NULL,  config_usage, 1, true, false },
    { OPTION_HEADER, NULL, NULL, use_header, 1, true, false },
    { OPTION_TMPFS, ALIAS_TMPFS, NULL, tmpfs_usage, 1, true,  false },
    { OPTION_UNALIGNED, ALIAS_UNALIGNED, NULL, use_unaligned, 256, true, false },
    { OPTION_ACCEPT_DUP, ALIAS_ACCEPT_DUP, NULL, use_accept_dups, 1, false, false },
    { OPTION_ACCEPT_NOMATCH, NULL, NULL, use_accept_nomatch, 1, false, false },
    { OPTION_NOMATCH_LOG, NULL, NULL, use_nomatch_log, 1, true, false },
    { OPTION_QCOMP, ALIAS_QCOMP, NULL, qcomp_usage, 1, true,  false },
    { OPTION_MINMAPQ, ALIAS_MINMAPQ, NULL, min_mapq_usage, 1, true,  false },
    { OPTION_CACHE_SIZE, NULL, NULL, cache_size_usage, 1, true,  false },
    { OPTION_NO_CS, NULL, NULL, use_no_cs, 1, false,  false },
    { OPTION_MIN_MATCH, NULL, NULL, use_min_match, 1, true, false },
    { OPTION_NO_SECONDARY, ALIAS_NO_SECONDARY, NULL, use_no_secondary, 1, false, false },
    { option_unsorted, NULL, NULL, unsorted_usage, 1, false,  false },
    { option_sorted, NULL, NULL, sorted_usage, 1, false,  false },
    { option_no_verify, NULL, NULL, no_verify_usage, 1, false,  false },
    { option_only_verify, NULL, NULL, only_verify_usage, 1, false,  false },
    { option_use_qual, NULL, NULL, use_QUAL_usage, 1, false,  false },
    { option_ref_config, NULL, NULL, use_ref_config, 1, false,  false },
    { option_ref_filter, NULL, NULL, use_ref_filter, 1, true,  false },
    { option_edit_aligned_qual, NULL, NULL, use_edit_aligned_qual, 1, true, false },
    { option_keep_mismatch_qual, NULL, NULL, use_keep_mismatch_qual, 1, false,  false },
    { OPTION_MAX_REC_COUNT, NULL, NULL, mrc_usage, 1, true,  false },
    { OPTION_MAX_ERR_COUNT, ALIAS_MAX_ERR_COUNT, NULL, mec_usage, 1, true,  false },
    { OPTION_REF_FILE, ALIAS_REF_FILE, NULL, use_ref_file, 0, true, false },
    { OPTION_TI, NULL, NULL, use_TI, 1, false, false },
    { OPTION_MAX_WARN_DUP_FLAG, NULL, NULL, use_max_dup_warnings, 1, true, false },
    { OPTION_ACCEPT_HARD_CLIP, NULL, NULL, use_accept_hard_clip, 1, false, false },
    { OPTION_ALLOW_MULTI_MAP, NULL, NULL, use_allow_multi_map, 1, false, false },
    { OPTION_ALLOW_SECONDARY, NULL, NULL, use_allow_secondary, 1, false, false },
    { OPTION_DEFER_SECONDARY, NULL, NULL, use_defer_secondary, 1, false, false }
};

const char* OptHelpParam[] =
{
    /* order here is same as in OptDef array above!!! */
    "path",				/* input */
    "path",				/* output */
    "path-to-file",		/* config */
    "path-to-file",		/* header */
    "path",				/* tmpfs */
    "path-to-file",		/* unaligned */
    NULL,				/* accept dups */
    NULL,				/* accept no-match */
    "path-to-file",		/* no-match log */
    "level",			/* quality compression */
    "phred-score",		/* min. mapq */
    "mbytes",			/* cache size */
    NULL,				/* no colorspace */
    "count",			/* min. match count */
    NULL,				/* no secondary */
    NULL,				/* unsorted */
    NULL,				/* sorted */
    NULL,				/* no verify ref's */
    NULL,				/* quit after verify ref's */
    NULL,				/* force QUAL */
    NULL,				/* ref's from config */
    "name",				/* only this ref */
    "new-value",		/* value for aligned qualities */
    NULL,				/* no quantize mismatch qualities */
    "number",			/* max. record count to process */
    "number",			/* max. error count */
    "path-to-file",		/* reference fasta file */
    NULL,				/* use XT->TI */
    "count",			/* max. duplicate warning count */
    NULL,				/* allow hard clipping */
    NULL,				/* allow multimapping */
    NULL,				/* allow secondary */
    NULL				/* defer secondary */
};

rc_t UsageSummary (char const * progname)
{
    return KOutMsg (
        "Usage:\n"
        "\t%s [options] <bam-file>\n"
        "\n"
        "Summary:\n"
        "\tLoad a BAM formatted data file\n"
        "\n"
        "Example:\n"
        "\t%s -o /tmp/SRZ123456 -k analysis.bam.cfg 123456.bam\n"
        "\n"
        ,progname, progname);
}

char const UsageDefaultName[] = "bam-load";

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

Globals G;

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
static void set_pid(void)
{
    G.pid = getpid();
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
    else if (plen < sz) {
        strncpy(rslt, path, sz);
        return 0;
    }
    {
        rc_t const rc = RC(rcApp, rcArgv, rcAccessing, rcBuffer, rcInsufficient);
        (void)LOGERR(klogErr, rc, "The path to the file is too long");
        return rc;
    }
}

static rc_t OpenFile(KFile const **kf, char const path[], char const base[])
{
    char fname[4096];
    rc_t rc = PathWithBasePath(fname, sizeof(fname), path, base);
    
    if (rc == 0) {
        KDirectory *dir;
        
        rc = KDirectoryNativeDir(&dir);
        if (rc == 0) {
            rc = KDirectoryOpenFileRead(dir, kf, "%s", fname);
            KDirectoryRelease(dir);
        }
    }
    return rc;
}

static rc_t LoadHeader(char const **rslt, char const path[], char const base[])
{
    KFile const *kf;
    rc_t rc = OpenFile(&kf, path, base);
    
    *rslt = NULL;
    if (rc == 0) {
        uint64_t fsize;
        rc = KFileSize(kf, &fsize);
        if (rc == 0) {
            char *fdata = malloc(fsize+1);
            
            if (fdata) {
                size_t nread;
                rc = KFileRead(kf, 0, fdata, fsize, &nread);
                if (rc == 0) {
                    if (nread) {
                        fdata[nread] = '\0';
                        *rslt = fdata;
                    }
                    else {
                        free(fdata);
                        rc = RC(rcApp, rcArgv, rcAccessing, rcFile, rcEmpty);
                        (void)PLOGERR(klogErr, (klogErr, rc, "File '$(file)' is empty", "file=%s", path));
                    }
                }
                else {
                    (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read file '$(file)'", "file=%s", path));
                }
            }
            else {
                rc = RC(rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted);
                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read file '$(file)'", "file=%s", path));
            }
        }
        KFileRelease(kf);
    }
    else {
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open file '$(file)'", "file=%s", path));
    }
    return rc;
}

static rc_t main_help_vers(int argc, char * argv[])
{
    Args *args = NULL;
    rc_t const rc = ArgsMakeAndHandle (&args, argc, argv, 2, Options,
        sizeof Options / sizeof (OptDef), XMLLogger_Args, XMLLogger_ArgsQty);
    ArgsWhack(args);
    return rc;
}

static rc_t getArgValue(Args *const args, char const *name, int index, char const **result)
{
    void const *value;
    rc_t const rc = ArgsOptionValue(args, name, index, &value);
    if (rc) return rc;
    free((void *)*result);
    *result = strdup(value);
    assert(*result);
    return 0;
}

static rc_t main_1(int argc, char *argv[], bool const continuing, unsigned const load)
{
    Args *args;
    rc_t rc;
    unsigned n_aligned = 0;
    unsigned n_unalgnd = 0;
    char *aligned[256];
    char *unalgnd[256];
    char *name_buffer = NULL;
    unsigned next_name = 0;
    unsigned nbsz = 0;
    char const *value;
    char *dummy;

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof(Options)/sizeof(Options[0]));
    while (rc == 0) {
        uint32_t pcount;

        rc = ArgsOptionCount(args, option_only_verify, &pcount);
        if (rc)
            break;
        G.onlyVerifyReferences |= (pcount > 0);
        
        rc = ArgsOptionCount(args, option_no_verify, &pcount);
        if (rc)
            break;
        G.noVerifyReferences |= (pcount > 0);
        
        rc = ArgsOptionCount(args, option_use_qual, &pcount);
        if (rc)
            break;
        G.useQUAL |= (pcount > 0);
        
        rc = ArgsOptionCount(args, option_ref_config, &pcount);
        if (rc)
            break;
        G.limit2config |= (pcount > 0);
        
        rc = ArgsOptionCount(args, OPTION_REF_FILE, &pcount);
        if (rc)
            break;
        if (pcount && G.refFiles) {
            int i;

            for (i = 0; G.refFiles[i]; ++i)
                free((void *)G.refFiles[i]);
            free((void *)G.refFiles);
        }
        G.refFiles = calloc(pcount + 1, sizeof(*(G.refFiles)));
        if (!G.refFiles) {
            rc = RC(rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted);
            break;
        }
        while(pcount-- > 0) {
            rc = getArgValue(args, OPTION_REF_FILE, pcount, &G.refFiles[pcount]);
            if (rc)
                break;
        }

        rc = ArgsOptionCount (args, OPTION_TMPFS, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = getArgValue(args, OPTION_TMPFS, 0, &G.tmpfs);
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
        
        rc = ArgsOptionCount (args, OPTION_INPUT, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = getArgValue(args, OPTION_INPUT, 0, &G.inpath);
            if (rc)
                break;
        }
        else if (pcount > 1)
        {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcExcessive);
            OUTMSG (("Single input parameter required\n"));
            MiniUsage (args);
            break;
        }
        
        rc = ArgsOptionCount (args, option_ref_filter, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = getArgValue(args, option_ref_filter, 0, &G.refFilter);
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
        
        rc = ArgsOptionCount (args, OPTION_CONFIG, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = getArgValue(args, OPTION_CONFIG, 0, &G.refXRefPath);
            if (rc)
                break;
        }
        else if (pcount > 1)
        {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcExcessive);
            OUTMSG (("Single input parameter required\n"));
            MiniUsage (args);
            break;
        }
        
        rc = ArgsOptionCount (args, OPTION_OUTPUT, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = getArgValue(args, OPTION_OUTPUT, 0, &G.outpath);
            if (rc)
                break;
            if (load == 0) {
                G.firstOut = strdup(G.outpath);
            }
            value = strrchr(G.outpath, '/');
            G.outname = value ? (value + 1) : G.outpath;
        }
        else if (pcount > 1)
        {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcExcessive);
            OUTMSG (("Single output parameter required\n"));
            MiniUsage (args);
            break;
        }
        else if (!G.onlyVerifyReferences) {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcInsufficient);
            OUTMSG (("Output parameter required\n"));
            MiniUsage (args);
            break;
        }
        
        rc = ArgsOptionCount (args, OPTION_MINMAPQ, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue(args, OPTION_MINMAPQ, 0, (const void **)&value);
            if (rc)
                break;
            G.minMapQual = strtoul(value, &dummy, 0);
        }
        
        rc = ArgsOptionCount (args, OPTION_QCOMP, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = getArgValue(args, OPTION_QCOMP, 0, &G.QualQuantizer);
            if (rc)
                break;
        }

        rc = ArgsOptionCount (args, option_edit_aligned_qual, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, option_edit_aligned_qual, 0, (const void **)&value);
            if (rc)
                break;
            G.alignedQualValue = strtoul(value, &dummy, 0);
            if (G.alignedQualValue == 0) {
                rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcIncorrect);
                OUTMSG (("edit-aligned-qual: bad value\n"));
                MiniUsage (args);
                break;
            }
            G.editAlignedQual = true;
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
            if (G.cache_size == 0) {
                rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcIncorrect);
                OUTMSG (("cache-size: bad value\n"));
                MiniUsage (args);
                break;
            }
        }
        
        rc = ArgsOptionCount (args, OPTION_MAX_WARN_DUP_FLAG, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_MAX_WARN_DUP_FLAG, 0, (const void **)&value);
            if (rc)
                break;
            G.maxWarnCount_DupConflict = strtoul(value, &dummy, 0);
        }
        
        rc = ArgsOptionCount (args, option_unsorted, &pcount);
        if (rc)
            break;
        G.expectUnsorted |= (pcount > 0);
        
        rc = ArgsOptionCount (args, option_sorted, &pcount);
        if (rc)
            break;
        G.requireSorted |= (pcount > 0);
        
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
        
        rc = ArgsOptionCount (args, OPTION_MIN_MATCH, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            rc = ArgsOptionValue (args, OPTION_MIN_MATCH, 0, (const void **)&value);
            if (rc)
                break;
            G.minMatchCount = strtoul(value, &dummy, 0);
        }
        
        rc = ArgsOptionCount (args, OPTION_ACCEPT_DUP, &pcount);
        if (rc)
            break;
        G.acceptBadDups |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_ACCEPT_NOMATCH, &pcount);
        if (rc)
            break;
        G.acceptNoMatch |= (pcount > 0);
        
        rc = ArgsOptionCount (args, option_keep_mismatch_qual, &pcount);
        if (rc)
            break;
        G.keepMismatchQual |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_NO_CS, &pcount);
        if (rc)
            break;
        G.noColorSpace |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_NO_SECONDARY, &pcount);
        if (rc)
            break;
        G.noSecondary |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_TI, &pcount);
        if (rc)
            break;
        G.hasTI |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_ACCEPT_HARD_CLIP, &pcount);
        if (rc)
            break;
        G.acceptHardClip |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_ALLOW_MULTI_MAP, &pcount);
        if (rc)
            break;
        G.allowMultiMapping |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_ALLOW_SECONDARY, &pcount);
        if (rc)
            break;
        G.assembleWithSecondary |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_DEFER_SECONDARY, &pcount);
        if (rc)
            break;
        G.deferSecondary |= (pcount > 0);
        
        rc = ArgsOptionCount (args, OPTION_NOMATCH_LOG, &pcount);
        if (rc)
            break;
        if (pcount == 1)
        {
            KDirectory *dir;
            
            rc = ArgsOptionValue (args, OPTION_NOMATCH_LOG, 0, (const void **)&value);
            if (rc) break;
            rc = KDirectoryNativeDir(&dir);
            if (rc) break;
            rc = KDirectoryCreateFile(dir, &G.noMatchLog, 0, 0664, kcmInit, "%s", value);
            KDirectoryRelease(dir);
            if (rc) break;
        }
        
        rc = ArgsOptionCount (args, OPTION_HEADER, &pcount);
        if (rc)
            break;
        if (pcount == 1) {
            rc = ArgsOptionValue (args, OPTION_HEADER, 0, (const void **)&value);
            if (rc) break;
            free((void *)G.headerText);
            rc = LoadHeader(&G.headerText, value, G.inpath);
            if (rc) break;
        }
        
        rc = ArgsParamCount (args, &pcount);
        if (rc) break;
        if (pcount == 0)
        {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcInsufficient);
            MiniUsage (args);
            break;
        }
        else if (pcount > sizeof(aligned)/sizeof(aligned[0])) {
            rc = RC(rcApp, rcArgv, rcAccessing, rcParam, rcExcessive);
            (void)PLOGERR(klogErr, (klogErr, rc, "$(count) input files is too many, $(max) is the limit",
                        "count=%u,max=%u", (unsigned)pcount, (unsigned)(sizeof(aligned)/sizeof(aligned[0]))));
            break;
        }
        else {
            unsigned need = G.inpath ? (strlen(G.inpath) + 1) * pcount : 0;
            unsigned i;
            
            for (i = 0; i < pcount; ++i) {
                rc = ArgsParamValue(args, i, (const void **)&value);
                if (rc) break;
                need += strlen(value) + 1;
            }
            nbsz = need;
        }
        
        rc = ArgsOptionCount (args, OPTION_UNALIGNED, &pcount);
        if (rc)
            break;
        if (pcount > 0)
        {
            unsigned need = G.inpath ? (strlen(G.inpath) + 1) * pcount : 0;
            unsigned i;
            
            for (i = 0; i < pcount; ++i) {
                rc = ArgsOptionValue(args, OPTION_UNALIGNED, i, (const void **)&value);
                if (rc) break;
                need += strlen(value) + 1;
            }
            if (rc) break;
            nbsz += need;
        }
        
        name_buffer = malloc(nbsz);
        if (name_buffer == NULL) {
            rc = RC(rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted);
            break;
        }
        
        rc = ArgsOptionCount (args, OPTION_UNALIGNED, &pcount);
        if (rc == 0) {
            unsigned i;
            
            for (i = 0; i < pcount; ++i) {
                rc = ArgsOptionValue(args, OPTION_UNALIGNED, i, (const void **)&value);
                if (rc) break;
                
                unalgnd[n_unalgnd++] = name_buffer + next_name;
                rc = PathWithBasePath(name_buffer + next_name, nbsz - next_name, value, G.inpath);
                if (rc) break;
                next_name += strlen(name_buffer + next_name) + 1;
            }
            if (rc) break;
        }
        else
            break;
        
        rc = ArgsParamCount (args, &pcount);
        if (rc == 0) {
            unsigned i;
            
            for (i = 0; i < pcount; ++i) {
                rc = ArgsParamValue(args, i, (const void **)&value);
                if (rc) break;
                
                aligned[n_aligned++] = name_buffer + next_name;
                rc = PathWithBasePath(name_buffer + next_name, nbsz - next_name, value, G.inpath);
                if (rc) break;
                next_name += strlen(name_buffer + next_name) + 1;
            }
        }
        else
            break;

        rc = run(argv[0], n_aligned, (char const **)aligned, n_unalgnd, (char const **)unalgnd, continuing);
        break;
    }
    free(name_buffer);

    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "load failed",
                "severity=total,status=failure,accession=%s,errors=%u", G.outname, G.errCount));
    } else {
        (void)PLOGMSG(klogInfo, (klogInfo, "loaded",
                "severity=total,status=success,accession=%s,errors=%u", G.outname, G.errCount));
    }
    ArgsWhack(args);
    return rc;
}

static void cleanupGlobal(void)
{
    if (G.refFiles) {
        int i;

        for (i = 0; G.refFiles[i]; ++i)
            free((void *)G.refFiles[i]);
        free((void *)G.refFiles);
    }
    free((void *)G.tmpfs);
    free((void *)G.inpath);
    free((void *)G.refFilter);
    free((void *)G.refXRefPath);
    free((void *)G.outpath);
    free((void *)G.firstOut);
    free((void *)G.headerText);
    free((void *)G.QualQuantizer);
    free((void *)G.schemaPath);
}

static int find_arg(char const *const *const query, int const first, int const argc, char **const argv)
{
    int i;

    for (i = first; i < argc; ++i) {
        int j;

        for (j = 0; query[j] != NULL; ++j) {
            if (strcmp(argv[i], query[j]) == 0)
                return i;
        }
    }
    return 0;
}

static bool has_arg(char const *const *const query, int const argc, char **const argv)
{
    return find_arg(query, 1, argc, argv) > 0;
}

static const char *logger_options[] = { "--xml-log-fd", "--xml-log", "-z" };
static XMLLogger const *make_logger(int *argc, char *argv[])
{
    XMLLogger const *rslt = NULL;
    char *argf[4];
    int i;

    argf[0] = argv[0];
    argf[1] = NULL;
    argf[2] = NULL;
    argf[3] = NULL;

    for (i = 1; i < *argc; ++i) {
        int remove = 0;

        if (strcmp(argv[i], logger_options[2]) == 0) {
            argf[1] = argv[i];
            argf[2] = argv[i + 1];
            remove = 2;
        }
        else {
            int j;

            for (j = 0; j < 2; ++j) {
                if (strstr(argv[i], logger_options[j]) == argv[i]) {
                    int const n = strlen(logger_options[j]);

                    if (argv[i][n] == '\0') {
                        argf[1] = argv[i];
                        argf[2] = argv[i + 1];
                        remove = 2;
                    }
                    else if (argv[i][n] == '=') {
                        argv[i][n] = '\0';
                        argf[1] = argv[i];
                        argf[2] = argv[i] + n + 1;
                        remove = 1;
                    }
                    break;
                }
            }
        }
        if (argf[1] != NULL) {
            Args *args = NULL;

            ArgsMakeAndHandle(&args, 3, argf, 1, XMLLogger_Args, XMLLogger_ArgsQty);
            if (args) {
                XMLLogger_Make(&rslt, NULL, args);
                ArgsWhack(args);
            }
        }
        if (remove) {
            *argc -= remove;
            memmove(argv + i, argv + i + remove, (*argc + 1) * sizeof(argv[0]));
            break;
        }
    }
    return rslt;
}

rc_t CC KMain(int argc, char *argv[])
{
    static const char *help[] = { "--help", "-h", "-?", NULL };
    static const char *vers[] = { "--version", "-V", NULL };

    bool const has_help = has_arg(help, argc, argv);
    bool const has_vers = has_arg(vers, argc, argv);
    XMLLogger const *logger = NULL;
    int argfirst = 0;
    int arglast = 0;
    rc_t rc = 0;
    unsigned load = 0;

    if (has_help) {
        argc = 2;
        argv[1] = "--help";
        return main_help_vers(argc, argv);
    }
    if (has_vers) {
        argc = 2;
        argv[1] = "--vers";
        return main_help_vers(argc, argv);
    }

    logger = make_logger(&argc, argv);

    memset(&G, 0, sizeof(G));
    G.mode = mode_Archive;
    G.globalMode = mode_Archive;
    G.maxSeqLen = TableWriterRefSeq_MAX_SEQ_LEN;
    G.schemaPath = strdup(SCHEMAFILE);
    G.omit_aligned_reads = true;
    G.omit_reference_reads = true;
    G.minMapQual = 0; /* accept all */
    G.tmpfs = strdup("/tmp");
    G.cache_size = ((size_t)16) << 30;
    G.maxErrCount = 1000;
    G.minMatchCount = 10;
    
    set_pid();

    for (arglast = 1; arglast < argc; ++arglast) {
        if (strcmp(argv[arglast], "--remap") == 0) {
            argv[arglast] = argv[0];
            G.globalMode = mode_Remap;
            rc = main_1(arglast - argfirst, argv + argfirst, true, load);
            if (rc)
                break;
            G.mode = mode_Remap;
            argfirst = arglast;
            ++load;
        }
    }
    rc = main_1(arglast - argfirst, argv + argfirst, false, load);
    XMLLogger_Release(logger);
    cleanupGlobal();
    return rc;
}
