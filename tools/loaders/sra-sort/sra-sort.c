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

#include "ctx.h"
#include "caps.h"
#include "mem.h"
#include "except.h"
#include "status.h"
#include "sra-sort.h"

#include <kapp/main.h>
#include <kapp/args.h>
#include <vdb/manager.h>
#include <vdb/vdb-priv.h>
#include <kdb/manager.h>
#include <kfg/config.h>
#include <kfs/directory.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <strtol.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

FILE_ENTRY ( sra-sort );


/* this goes away in vdb-3 */
const char UsageDefaultName [] = "sra-sort";

rc_t CC UsageSummary ( const char *prog_name )
{
    return KOutMsg ( "Usage: %s [options] src-object dst-object\n"
                     "       %s [options] src-object [src-object...] dst-dir\n"
                     "\n"
                     , prog_name
                     , prog_name
        );
}

#define OPT_IGNORE_FAILURE "ignore-failure"
#define OPT_FORCE "force"
#define OPT_MEM_LIMIT "mem-limit"
#define OPT_MAP_FILE_BSIZE "map-file-bsize"
#define OPT_MAX_IDX_IDS "max-idx-ids"
#define OPT_MAX_REF_IDX_IDS "max-ref-idx-ids"
#define OPT_MAX_LARGE_IDX_IDS "max-large-idx-ids"
#define OPT_TEMP_DIR "tempdir"
#define OPT_MMAP_DIR "mmapdir"
#define OPT_UNSORTED_OLD_NEW "unsorted-old-new"

#define OPT_COLUMN_MD5 "column-md5"
#define OPT_NO_COLUMN_CHECKSUM "no-column-checksum"

#define OPT_BLOB_CRC32 "blob-crc32"
#define OPT_BLOB_MD5 "blob-md5"
#define OPT_NO_BLOB_CHECKSUM "no-blob-checksum"

#define OPT_KEEP_IDX_FILES "keep-idx-files"
#define OPT_IDX_CONSISTENCY_CHECK "idx-cc"

static const char *hlp_ignore_failure [] = { "ignore failure when sorting multiple objects",
                                             "i.e. continue in spite of previous errors", NULL };
static const char *hlp_force [] = { "force overwrite of existing destination", NULL };
static const char *hlp_mem_limit [] = { "sets limit on dynamic memory usage", NULL };
static const char *hlp_map_file_bsize [] = { "sets id map-file cache size", NULL };
static const char *hlp_max_idx_ids [] = { "sets number of join-index ids to process at a time", NULL };
static const char *hlp_max_ref_idx_ids [] = { "sets number of join-index ids to process within REFERENCE table", NULL };
static const char *hlp_max_large_idx_ids [] = { "sets number of rows to process with large columns", NULL };
static const char *hlp_temp_dir [] = { "sets a specific directory to use for temporary files", NULL };
static const char *hlp_mmap_dir [] = { "sets a specific directory to use for memory-mapped buffers", NULL };
static const char *hlp_unsorted_old_new [] = { "write old=>new index in unsorted order", NULL };

static const char *hlp_column_md5 [] = { "generate md5sum compatible checksum files for each column [default]", NULL };
static const char *hlp_no_column_checksum [] = { "disable generation of column checksums", NULL };

static const char *hlp_blob_crc32 [] = { "generate CRC32 checksums for each blob [default]", NULL };
static const char *hlp_blob_md5 [] = { "generate MD5 checksums for each blob", NULL };
static const char *hlp_no_blob_checksum [] = { "disable generation of blob checksums", NULL };

#if _DEBUGGING
static const char *hlp_keep_idx_files [] = { "keep temporary index files for debugging", NULL };
static const char *hlp_idx_consistency_check [] = { "run consistency check on index files", NULL };
#endif

static OptDef options [] =
{
    /* 1. long-name
       2. list of single character short names
       3. help-gen function
       4. list of help strings, NULL terminated
       5. max count
       6. option requires value
       7. option is required
    */
    { OPT_IGNORE_FAILURE, "i", NULL, hlp_ignore_failure, 1, false, false }
  , { OPT_FORCE, "f", NULL, hlp_force, 1, false, false }
  , { OPT_MEM_LIMIT, NULL, NULL, hlp_mem_limit, 1, true, false }
  , { OPT_MAP_FILE_BSIZE, NULL, NULL, hlp_map_file_bsize, 1, true, false }
  , { OPT_MAX_IDX_IDS, NULL, NULL, hlp_max_idx_ids, 1, true, false }
  , { OPT_MAX_REF_IDX_IDS, NULL, NULL, hlp_max_ref_idx_ids, 1, true, false }
  , { OPT_MAX_LARGE_IDX_IDS, NULL, NULL, hlp_max_large_idx_ids, 1, true, false }
  , { OPT_TEMP_DIR, NULL, NULL, hlp_temp_dir, 1, true, false }
  , { OPT_MMAP_DIR, NULL, NULL, hlp_mmap_dir, 1, true, false }
  , { OPT_UNSORTED_OLD_NEW, NULL, NULL, hlp_unsorted_old_new, 1, false, false }

  , { OPT_COLUMN_MD5, NULL, NULL, hlp_column_md5, 1, false, false }
  , { OPT_NO_COLUMN_CHECKSUM, NULL, NULL, hlp_no_column_checksum, 1, false, false }
  , { OPT_BLOB_CRC32, NULL, NULL, hlp_blob_crc32, 1, false, false }
  , { OPT_BLOB_MD5, NULL, NULL, hlp_blob_md5, 1, false, false }
  , { OPT_NO_BLOB_CHECKSUM, NULL, NULL, hlp_no_blob_checksum, 1, false, false }
#if _DEBUGGING
  , { OPT_KEEP_IDX_FILES, NULL, NULL, hlp_keep_idx_files, 1, false, false }
  , { OPT_IDX_CONSISTENCY_CHECK, NULL, NULL, hlp_idx_consistency_check, 1, false, false }
#endif
};

static const char *option_params [] =
{
    NULL
  , NULL
  , "bytes"
  , "cache-size"
  , "num-ids"
  , "num-ids"
  , "num-ids"
  , "path-to-tmp"
  , "path-to-mmaps"
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
  , NULL
#if _DEBUGGING
  , NULL
  , NULL
#endif
};

rc_t CC Usage ( const Args *args )
{
    uint32_t i;
    const char *progname, *fullpath;
    rc_t rc = ArgsProgram ( args, & fullpath, & progname );
    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );

    for ( i = 0; i < sizeof options / sizeof options [ 0 ]; ++ i )
    {
        HelpOptionLine ( options [ i ] . aliases, options [ i ] . name,
            option_params [ i ], options [ i ] . help );
    }

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion () );

    return 0;
}

static
const char *ArgsGetOptStr ( Args *self, const ctx_t *ctx, const char *optname, uint32_t *count )
{
    rc_t rc;
    const char *val = NULL;

    uint32_t dummy;
    if ( count == NULL )
        count = & dummy;

    rc = ArgsOptionCount ( self, optname, count );
    if ( rc == 0 && * count != 0 )
    {
        rc = ArgsOptionValue ( self, optname, 0, (const void **)& val );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "failed to retrieve '%s' parameter", optname );
    }

    return val;
}

static
uint64_t ArgsGetOptU64 ( Args *self, const ctx_t *ctx, const char *optname, uint32_t *count )
{
    rc_t rc;
    uint64_t val = 0;

    uint32_t dummy;
    if ( count == NULL )
        count = & dummy;

    rc = ArgsOptionCount ( self, optname, count );
    if ( rc == 0 && * count != 0 )
    {
        const char *str;
        rc = ArgsOptionValue ( self, optname, 0, (const void **)& str );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "failed to retrieve '%s' parameter", optname );
        else
        {
            char *end;
            val = strtou64 ( str, & end, 0 );
            if ( end [ 0 ] != 0 )
            {
                rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcIncorrect );
                ERROR ( rc, "bad '%s' parameter: '%s'", optname, str );
            }
        }
    }

    return val;
}

static
bool ArgsGetOptBool ( Args *self, const ctx_t *ctx, const char *optname, uint32_t *count )
{
    rc_t rc;
    bool val = false;

    uint32_t dummy;
    if ( count == NULL )
        count = & dummy;

    rc = ArgsOptionCount ( self, optname, count );
    if ( rc == 0 && * count != 0 )
        val = true;

    return val;
}

static
uint64_t KConfigGetNodeU64 ( const ctx_t *ctx, const char *path, bool *found )
{
    rc_t rc;
    uint64_t val = 0;
    const KConfigNode *n;

    bool dummy;
    if ( found == NULL )
        found = & dummy;

    rc = KConfigOpenNodeRead ( ctx -> caps -> cfg, & n, "sra-sort/map_file_bsize" );
    if ( rc == 0 )
    {
        char buff [ 256 ];
        size_t num_read, remaining;

        rc = KConfigNodeRead ( n, 0, buff, sizeof buff - 1, & num_read, & remaining );
        if ( rc != 0 )
            ERROR ( rc, "failed to read KConfig node '%s'", path );
        else if ( remaining != 0 )
        {
            rc = RC ( rcExe, rcNode, rcReading, rcBuffer, rcInsufficient );
            ERROR ( rc, "failed to read KConfig node '%s'", path );
        }
        else
        {
            char *end;
            buff [ num_read ] = 0;
            val = strtou64 ( buff, & end, 0 );
            if ( end [ 0 ] != 0 )
            {
                rc = RC ( rcExe, rcNode, rcReading, rcNumeral, rcIncorrect );
                ERROR ( rc, "bad '%s' config value: '%s'", path, buff );
            }
        }

        KConfigNodeRelease ( n );
    }

    return val;
}

static
void initialize_params ( const ctx_t *ctx, Caps *caps, Tool *tp, Selection *sel, Args *args )
{
    bool found=false;
    uint64_t val;
    uint32_t count;
    const char *str;

    int col_md5 = kcmMD5;

    memset ( sel, 0, sizeof * sel );
    memset ( tp, 0, sizeof * tp );
    tp -> sel = sel;

    /* default to host tmp */
    tp -> tmpdir = "/tmp";

    /* default to mmap dir */
    tp -> mmapdir = NULL;

    /* default buffer size for map cache */
    tp -> map_file_bsize = 64 * 1024 * 1024;
    tp -> map_file_random_bsize = tp -> map_file_bsize;

    /* default max index ids to gather at a time */
    tp -> max_ref_idx_ids = tp -> max_large_idx_ids = tp -> max_idx_ids = 256 * 1024 * 1024;
    tp -> max_poslen_ids = 64 * 1024 * 1024;
    tp -> min_idx_ids =  64 * 1024 * 1024;
    tp -> max_missing_ids = tp -> max_idx_ids;

#if 0
    /* refpos cache size */
    tp -> refpos_cache_capacity = 100 * 1024 * 1024;
#endif

    /* for creating mapping files */
    tp -> pid = getpid ();

    /* db create defaults */
    tp -> db . cmode = kcmCreate;

    /* tbl create defaults */
    tp -> tbl . cmode = kcmCreate;

    /* column create mode defaults */
    tp -> col . pgsize = 1;
    tp -> col . cmode = kcmInit;
    tp -> col . checksum = kcsCRC32;

    /* normally do not ignore failures */
    tp -> ignore = false;

    /* normally do not force overwrite */
    tp -> force = false;

    /* not needed under normal circumstances */
    tp -> write_new_to_old = false;
tp->write_new_to_old=true;

    /* default is to sort on old */
    tp -> sort_before_old2new = true;

    /* when debugging, this can be turned off */
    tp -> unlink_idx_files = true;
    tp -> idx_consistency_check = false;


    /* record them as caps */
    caps -> tool = tp;

    /* look in config */
    /* TBD - COMPLETE THIS */
    ON_FAIL ( val = KConfigGetNodeU64 ( ctx, "sra-sort/map_file_bsize", & found ) )
        return;
    if ( found )
        tp -> map_file_bsize = tp -> map_file_random_bsize = ( size_t ) val;

    ON_FAIL ( val = KConfigGetNodeU64 ( ctx, "sra-sort/map_file_random_bsize", & found ) )
        return;
    if ( found )
        tp -> map_file_random_bsize = ( size_t ) val;

    ON_FAIL ( val = KConfigGetNodeU64 ( ctx, "sra-sort/max_idx_ids", & found ) )
        return;
    if ( found )
        tp -> max_idx_ids = ( size_t ) val;

    ON_FAIL ( val = KConfigGetNodeU64 ( ctx, "sra-sort/max_ref_idx_ids", & found ) )
        return;
    if ( found )
        tp -> max_ref_idx_ids = ( size_t ) val;

    /* finally look in args */
    ON_FAIL ( str = ArgsGetOptStr ( args, ctx, OPT_TEMP_DIR, & count ) )
        return;
    if ( count != 0 && str [ 0 ] != 0 )
        tp -> tmpdir = str;

    ON_FAIL ( str = ArgsGetOptStr ( args, ctx, OPT_MMAP_DIR, & count ) )
        return;
    if ( count != 0 && str [ 0 ] != 0 )
        tp -> mmapdir = str;

    ON_FAIL ( val = ArgsGetOptU64 ( args, ctx, OPT_MAP_FILE_BSIZE, & count ) )
        return;
    if ( count != 0 )
        tp -> map_file_random_bsize = ( size_t ) val;

    ON_FAIL ( val = ArgsGetOptU64 ( args, ctx, OPT_MAX_IDX_IDS, & count ) )
        return;
    if ( count != 0 )
        tp -> max_idx_ids = tp -> max_ref_idx_ids = ( size_t ) val;

    ON_FAIL ( val = ArgsGetOptU64 ( args, ctx, OPT_MAX_REF_IDX_IDS, & count ) )
        return;
    if ( count != 0 )
        tp -> max_ref_idx_ids = ( size_t ) val;

    ON_FAIL ( val = ArgsGetOptU64 ( args, ctx, OPT_MAX_LARGE_IDX_IDS, & count ) )
        return;
    if ( count != 0 )
        tp -> max_large_idx_ids = ( size_t ) val;

    ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_IGNORE_FAILURE, & count ) )
        return;
    if ( count != 0 )
        tp -> ignore = true;

    ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_FORCE, & count ) )
        return;
    if ( count != 0 )
        tp -> force = true;

    ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_UNSORTED_OLD_NEW, & count ) )
        return;
    if ( count != 0 )
        tp -> sort_before_old2new = false;

    ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_COLUMN_MD5, & count ) )
        return;
    if ( count != 0 )
        col_md5 = kcmMD5;
    else
    {
        ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_NO_COLUMN_CHECKSUM, & count ) )
            return;
        if ( count != 0 )
            col_md5 = 0;
    }

    tp -> db . cmode |= col_md5;
    tp -> tbl . cmode |= col_md5;
    tp -> col . cmode |= col_md5;

    ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_BLOB_CRC32, & count ) )
        return;
    if ( count != 0 )
        tp -> col . checksum = kcsCRC32;
    else
    {
        ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_BLOB_MD5, & count ) )
            return;
        if ( count != 0 )
            tp -> col . checksum = kcsMD5;
        else
        {
            ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_NO_BLOB_CHECKSUM, & count ) )
                return;
            if ( count != 0 )
                tp -> col . checksum = kcsNone;
        }
    }

#if _DEBUGGING
    ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_KEEP_IDX_FILES, & count ) )
        return;
    if ( count != 0 )
        tp -> unlink_idx_files = false;

    ON_FAIL ( found = ArgsGetOptBool ( args, ctx, OPT_IDX_CONSISTENCY_CHECK, & count ) )
        return;
    if ( count != 0 )
        tp -> idx_consistency_check = tp -> write_new_to_old = true;
#endif
}

static
void initialize_caps ( const ctx_t *ctx, Caps *caps, Args *args )
{
    FUNC_ENTRY ( ctx );

    /* get specified limit */
    uint32_t count;
    size_t mem_limit = ( size_t ) ArgsGetOptU64 ( args, ctx, OPT_MEM_LIMIT, & count );
    if ( ! FAILED () && count != 0 )
    {
        MemBank *mem;
        TRY ( mem = MemBankMake ( ctx, mem_limit ) )
        {
            MemBankRelease ( caps -> mem, ctx );
            caps -> mem = mem;
        }
    }

    if ( ! FAILED () )
    {
        /* here's a chance to pick up special config */
        /* TBD */

        /* open up KConfig */
        rc_t rc = KConfigMake ( ( KConfig** ) & caps -> cfg, NULL );
        if ( rc != 0 )
            ERROR ( rc, "KConfigMake failed" );
    }
}

rc_t copy_stats_metadata( const char * src_path, const char * dst_path );

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    DECLARE_CTX_INFO ();

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    /* initialize context */
    Caps caps;
    char cp_src_path [ 4096 ];
    char cp_dst_path [ 4096 ];
    ctx_t main_ctx = { & caps, NULL, & ctx_info };
    const ctx_t *ctx = & main_ctx;
    CapsInit ( & caps, NULL );

    cp_src_path[ 0 ] = 0;
    cp_dst_path[ 0 ] = 0;

    if ( argc <= 1 )
        main_ctx . rc = UsageSummary ( "sra-sort" );
    else
    {
        /* create MemBank with unlimited quota */
        TRY ( caps . mem = MemBankMake (  ctx, -1 ) )
        {
            rc_t rc = VDBManagerMakeUpdate ( & caps . vdb, NULL );
            if ( rc != 0 )
                ERROR ( rc, "failed to create VDBManager" );
            else
            {
                rc = VDBManagerOpenKDBManagerUpdate ( caps . vdb, & caps . kdb );
                if ( rc != 0 )
                    ERROR ( rc, "failed to create KDBManager" );
                else
                {
                    Args * args;
                    rc = ArgsMakeAndHandle ( & args, argc, argv, 1,
                        options, sizeof options / sizeof options [ 0 ] );
                    if ( rc != 0 )
                        ERROR ( rc, "failed to parse command line" );
                    else
                    {
                        TRY ( initialize_caps ( ctx, & caps, args ) )
                        {
                            Tool tp;
                            Selection sel;
                            TRY ( initialize_params ( ctx, & caps, & tp, & sel, args ) )
                            {
                                uint32_t count;
                                rc = ArgsParamCount ( args, & count );
                                if ( rc != 0 )
                                    ERROR ( rc, "ArgsParamCount failed" );
                                else if ( count < 2 )
                                {
                                    rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
                                    ERROR ( rc, "expected source and destination parameters" );
                                }
                                else
                                {
                                    /* check type of last parameter */
                                    const char *dst;
                                    rc = ArgsParamValue ( args, count - 1, (const void **)& dst );
                                    if ( rc != 0 )
                                        ERROR ( rc, "ArgsParamValue [ %u ] failed", count - 1 );
                                    else
                                    {
                                        enum RCTarget targ = rcNoTarg;

                                        bool dst_is_dir = false;
                                        int dst_type = KDBManagerPathType ( caps . kdb, "%s", dst ) & ~ kptAlias;
                                        if ( dst_type == kptDir )
                                            dst_is_dir = true;
                                        else if ( count != 2 )
                                        {
                                            rc = RC ( rcExe, rcArgv, rcParsing, rcArgv, rcIncorrect );
                                            ERROR ( rc, "multiple source objects require last parameter to be a directory" );
                                        }
                                        else switch ( dst_type )
                                        {
                                        case kptNotFound:
                                            tp . db . cmode |= kcmParents;
                                            tp . tbl . cmode |= kcmParents;
                                            break;
                                        case kptFile:
                                        case kptFile | kptAlias:
                                            rc = RC ( rcExe, rcArgv, rcParsing, rcArgv, rcIncorrect );
                                            ERROR ( rc, "destination object is a file - remove first or rename before trying again" );
                                            break;
                                        case kptDatabase:
                                            targ = rcDatabase;
                                            break;
                                        case kptTable:
                                            targ = rcTable;
                                            break;
                                        case kptPrereleaseTbl:
                                            dst_type = kptTable;
                                            break;
                                        default:
                                            rc = RC ( rcExe, rcArgv, rcParsing, rcArgv, rcIncorrect );
                                            ERROR ( rc, "destination object type cannot be overwritten" );
                                            break;
                                        }

                                        if ( ! FAILED () )
                                        {
                                            if ( dst_is_dir )
                                            {
                                                uint32_t i;
                                                rc_t first_rc = 0;
                                                bool issue_divider_line;

                                                if ( tp . force )
                                                {
                                                    tp . db . cmode = kcmInit | ( tp . db . cmode & ~ kcmValueMask );
                                                    tp . tbl . cmode = kcmInit | ( tp . tbl . cmode & ~ kcmValueMask );
                                                }

                                                for ( issue_divider_line = false, i = 0; i < count - 1; issue_divider_line = true, ++ i )
                                                {
                                                    rc = ArgsParamValue ( args, i, (const void **)& tp . src_path );
                                                    if ( rc != 0 )
                                                        ERROR ( rc, "ArgParamValue [ %u ] failed", i );
                                                    else
                                                    {
                                                        char dst_path [ 4096 ];
                                                        const char *leaf = strrchr ( tp . src_path, '/' );
                                                        if ( leaf ++ == NULL )
                                                            leaf = tp . src_path;
                                                        rc = string_printf ( dst_path, sizeof dst_path, NULL, "%s/%s", dst, leaf );
                                                        if ( rc != 0 )
                                                            ERROR ( rc, "string_printf on param [ %u ] failed", i );
                                                        else
                                                        {
                                                            size_t size = string_size ( dst_path );
                                                            char *ext = string_rchr ( dst_path, size, '.' );
                                                            if ( ext != NULL )
                                                            {
                                                                size -= ext - dst_path;
                                                                switch ( size )
                                                                {
                                                                case 4:
                                                                    if ( strcase_cmp ( ext, 4, ".sra", 4, 4 ) == 0 )
                                                                        * ext = 0;
                                                                    break;
                                                                case 5:
                                                                    if ( strcase_cmp ( ext, 5, ".csra", 5, 5 ) == 0 )
                                                                        * ext = 0;
                                                                    break;
                                                                }
                                                            }

                                                            if ( issue_divider_line )
                                                                STATUS ( 1, "################################################################" );

                                                            tp . dst_path = dst_path;
                                                            ON_FAIL ( run ( ctx ) )
                                                            {
                                                                if ( ! tp . ignore )
                                                                    break;

                                                                if ( first_rc == 0 )
                                                                    first_rc = ctx -> rc;

                                                                CLEAR ();
                                                            }
                                                        }
                                                    }
                                                }

                                                if ( first_rc != 0 )
                                                    main_ctx . rc = first_rc;
                                            }
                                            else
                                            {
                                                tp . dst_path = dst;
                                                rc = ArgsParamValue ( args, 0, (const void **)& tp . src_path );
                                                if ( rc != 0 )
                                                    ERROR ( rc, "ArgParamValue [ 0 ] failed" );
                                                else
                                                {
                                                    int src_type;

                                                    switch ( dst_type )
                                                    {
                                                    case kptDatabase:
                                                    case kptTable:
                                                        src_type = KDBManagerPathType ( caps . kdb, "%s", tp . src_path ) & ~ kptAlias;
                                                        if ( src_type == kptPrereleaseTbl )
                                                            src_type = kptTable;
                                                        if ( src_type != dst_type )
                                                        {
                                                            rc = RC ( rcExe, rcArgv, rcParsing, rcArgv, rcIncorrect );
                                                            ERROR ( rc, "source and destination object types are not compatible" );
                                                        }
                                                        else if ( ! tp . force )
                                                        {
                                                            rc = RC ( rcExe, targ, rcCopying, targ, rcExists );
                                                            ERROR ( rc, "destination object cannot be overwritten - try again with '-f'" );
                                                        }
                                                        else
                                                        {
                                                            tp . db . cmode = kcmInit | ( tp . db . cmode & ~ kcmValueMask );
                                                            tp . tbl . cmode = kcmInit | ( tp . tbl . cmode & ~ kcmValueMask );
                                                        }
                                                    }

                                                    if ( ! FAILED () ) {
                                                        string_copy( cp_src_path, sizeof cp_src_path,
                                                                     tp . src_path, string_size( tp . src_path ) );
                                                        string_copy( cp_dst_path, sizeof cp_dst_path,
                                                                     tp . dst_path, string_size( tp . dst_path ) );
                                                        run ( ctx );
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        ArgsWhack ( args );
                    }
                }
            }

            CapsWhack ( & caps, ctx );
        }
    }

    if ( 0 == main_ctx . rc && 0 != cp_src_path[ 0 ] && 0 != cp_dst_path[ 0 ] ) {
        main_ctx . rc = copy_stats_metadata( cp_src_path, cp_dst_path );
    }
    return VDB_TERMINATE( main_ctx . rc );
}
