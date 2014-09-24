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

#include "cg-dump.vers.h"

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>

#include <kdb/manager.h>    /* because of path-types */
#include <kdb/meta.h>       /* because of exploring meta-data */

#include <kfs/bzip.h>
#include <kfs/gzip.h>
#include <kfs/buffile.h>
#include <kfs/file.h>
#include <kfs/directory.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/queue-file.h>

#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/time.h>
#include <klib/printf.h>
#include <klib/container.h>
#include <klib/rc.h>

#include "num-gen.h"
#include "progressbar.h"
#include "sg_lookup.h"
#include "last_rowid.h"

#include <os-native.h>
#include <sysalloc.h>
#include <strtol.h>     /* strtou64 */
#include <string.h>     /* memset */

#define CURSOR_CACHE_SIZE 256*1024*1024
#define DEFAULT_CUTOFF 30000000

const char UsageDefaultName[] = "cg-dump";

static const char * rows_usage[]     = { "rows to dump (if ommited: all rows)", NULL };
static const char * cutoff_usage[]   = { "how many spots max. per output-file", NULL };
static const char * force_usage[]    = { "force to overwrite output directory if it already exists", NULL };
static const char * cache_usage[]    = { "size of cursor-cache", NULL };
static const char * comp_usage[]     = { "output-compression, 'none', 'gzip', 'bzip'(dflt) or 'null'", NULL };
static const char * prog_usage[]     = { "show progressbar", NULL };
static const char * lib_usage[]      = { "LIBRARY-value for output-file-header", NULL };
static const char * sample_usage[]   = { "SAMPLE-value for output-file-header", NULL };
static const char * prefix_usage[]   = { "name-prefix for output-files", NULL };
static const char * wbuf_usage[]     = { "use write-buffering of this size", NULL };
static const char * queue_usage[]    = { "use background threads for writing", NULL };
static const char * qbytes_usage[]   = { "background producer limit", NULL };
static const char * qblock_usage[]   = { "background blocksize", NULL };
static const char * qtimeout_usage[] = { "timeout for background writers", NULL };
static const char * lrowid_usage[]   = { "find the highest row-id in the out-dir", NULL };

#define OPTION_ROWS "rows"
#define OPTION_CUTOFF "cutoff"
#define OPTION_FORCE "force"
#define OPTION_CACHE "cache"
#define OPTION_COMP "compress"
#define OPTION_PROG "progress"
#define OPTION_LIB "lib"
#define OPTION_SAMPLE "sample"
#define OPTION_PREFIX "prefix"
#define OPTION_WBUF "writebuff"
#define OPTION_QUEUE "queued"
#define OPTION_QUEUE_BYTES "queue-bytes"
#define OPTION_QUEUE_BLOCK "queue-block"
#define OPTION_QUEUE_TIMEOUT "queue-timeout"
#define OPTION_LAST_ROWID "last-rowid"

#define ALIAS_ROWS "R"
#define ALIAS_CUTOFF "C"
#define ALIAS_FORCE "f"
#define ALIAS_CACHE "a"
#define ALIAS_COMP "s"
#define ALIAS_PROG "p"
#define ALIAS_WBUF "w"
#define ALIAS_QUEUE "q"

OptDef DumpOptions[] =
{
    { OPTION_ROWS,      ALIAS_ROWS,     NULL, rows_usage,   1,  true,   false },
    { OPTION_CUTOFF,    ALIAS_CUTOFF,   NULL, cutoff_usage, 1,  true,   false },
    { OPTION_FORCE,     ALIAS_FORCE,    NULL, force_usage,  1,  false,  false },
    { OPTION_CACHE,     ALIAS_CACHE,    NULL, cache_usage,  1,  true,   false },
    { OPTION_COMP,      ALIAS_COMP,     NULL, comp_usage,   1,  true,   false },
    { OPTION_PROG,      ALIAS_PROG,     NULL, prog_usage,   1,  false,  false },
    { OPTION_LIB,       NULL,           NULL, lib_usage,    1,  true,   false },
    { OPTION_SAMPLE,    NULL,           NULL, sample_usage, 1,  true,   false },
    { OPTION_PREFIX,    NULL,           NULL, prefix_usage, 1,  true,   false },
    { OPTION_WBUF,      ALIAS_WBUF,     NULL, wbuf_usage,   1,  true,   false },
    { OPTION_QUEUE,     ALIAS_QUEUE,    NULL, queue_usage,  1,  false,  false },
    { OPTION_QUEUE_BYTES, NULL,         NULL, qbytes_usage, 1,  true,   false },
    { OPTION_QUEUE_BLOCK, NULL,         NULL, qblock_usage, 1,  true,   false },
    { OPTION_QUEUE_TIMEOUT, NULL,       NULL, qtimeout_usage, 1,  true,   false },
    { OPTION_LAST_ROWID,    NULL,       NULL, lrowid_usage, 1,  false,  false }
};


rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ( "\nUsage:\n  %s <input-path or accession> <output-path> [options]\n\n",
                     progname );
}

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    uint32_t n, i;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );

    n = ( sizeof( DumpOptions ) / sizeof( DumpOptions[ 0 ] ) );
    for( i = 0; i < n; i++ )
    {
        if ( DumpOptions[ i ].help != NULL )
        {
            HelpOptionLine( DumpOptions[ i ].aliases, DumpOptions[ i ].name,
                            NULL, DumpOptions[ i ].help );
        }
    }
    KOutMsg( "\n" );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}


/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion ( void )
{
    return CG_DUMP_VERS;
}


enum output_compression
{
    oc_none = 0,    /* do not compress output */
    oc_gzip,        /* compress output with gzip */
    oc_bzip,        /* compress output with bzip2 */
    oc_null         /* do not create output at all */
};


typedef struct cg_dump_opts
{
    uint64_t cutoff;        /* how many bytes of output before new sub-file starts */
    uint64_t cursor_cache;  /* size of cursor-cache */
    uint32_t first_chunk;   /* starting number of chunks */
    size_t wbuff_size;      /* write-buffer size */
    size_t qbytes;          /* producer limit for background threads */
    size_t qblock;          /* block-size for background threads */
    uint32_t qtimeout;      /* timeout for background writers */

    bool overwrite;         /* if output dir exists, overwrite? */
    bool show_progress;     /* show progressbar */
    bool use_queue;         /* use queued writer */
    bool get_last_rowid;    /* discover the last row-id written */

    enum output_compression comp;   /* what type of output compression? */

    const char * lib;       /* LIBRARY - value for output-file-header */
    const char * sample;    /* SAMPLE - value for output-file-header */
    const char * prefix;    /* eventual prefix for output-filename */
} cg_dump_opts;


typedef struct lane
{
    BSTNode node;

    const String * name;
    KFile * reads;
/*    KFile * mappings; */
    uint64_t write_pos;
    uint64_t spot_count;
    uint32_t chunk;
} lane;


typedef struct cg_dump_ctx
{
    num_gen * rows;
    KDirectory * dir;
    KDirectory * out_dir;
    const VCursor * seq_cur;
/*    const VCursor * prim_cur; */
    BSTree lanes;
    struct sg_lookup * lookup;

    progressbar * progress;
    uint8_t fract_digits;

    uint32_t seq_read_idx;
    uint32_t seq_qual_idx;
    uint32_t seq_sg_idx;
/*    uint32_t seq_read_len_idx;
    uint32_t seq_prim_id_idx;

    uint32_t prim_cigar_idx;
    uint32_t prim_refname_idx;
    uint32_t prim_refpos_idx; */

    const char * dst;
} cg_dump_ctx;


const char * s_Database      = "Database";
const char * s_Table         = "Table";
const char * s_PrereleaseTbl = "Prerelease Table";
const char * s_Column        = "Column";
const char * s_Index         = "Index";
const char * s_NotFound      = "not found";
const char * s_BadPath       = "bad path";
const char * s_File          = "File";
const char * s_Dir           = "Dir";
const char * s_CharDev       = "CharDev";
const char * s_BlockDev      = "BlockDev";
const char * s_FIFO          = "FIFO";
const char * s_ZombieFile    = "ZombieFile";
const char * s_Dataset       = "Dataset";
const char * s_Datatype      = "Datatype";
const char * s_Unknown       = "Unknown";


const char * pathtype_2_pchar( int path_type )
{
    const char * res = s_Unknown;
    switch ( path_type )
    {
    case kptDatabase      : res = s_Database; break;
    case kptTable         : res = s_Table; break;
    case kptPrereleaseTbl : res = s_PrereleaseTbl; break;
    case kptColumn        : res = s_Column; break;
    case kptIndex         : res = s_Index; break;
    case kptNotFound      : res = s_NotFound; break;
    case kptBadPath       : res = s_BadPath; break;
    case kptFile          : res = s_File; break;
    case kptDir           : res = s_Dir; break;
    case kptCharDev       : res = s_CharDev; break;
    case kptBlockDev      : res = s_BlockDev; break;
    case kptFIFO          : res = s_FIFO; break;
    case kptZombieFile    : res = s_ZombieFile; break;
    case kptDataset       : res = s_Dataset; break;
    case kptDatatype      : res = s_Datatype; break;
    }
    return res;
}

static int CC String_lane_cmp ( const void * item, const BSTNode * n )
{
    const String * spot_group = ( const String * ) item;
    const lane * sg_lane = ( const lane * ) n;
    return StringCompare ( sg_lane->name, spot_group );
}


static int CC lane_lane_cmp ( const BSTNode * item, const BSTNode * n )
{
    const lane * lane1 = ( const lane * ) item;
    const lane * lane2 = ( const lane * ) n;
    return StringCompare ( lane2->name, lane1->name );
}


const char * bzip_ext = ".bz2";
const char * gzip_ext = ".gz";
const char * none_ext = "";


static rc_t write_header_line( lane * l, const char * line, size_t len )
{
    size_t num_writ_file;
    rc_t rc = KFileWrite ( l->reads, l->write_pos, line, len, &num_writ_file );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot write output for header" );
    }
    else
    {
        l->write_pos += num_writ_file;
    }
    return rc;
}


static rc_t write_header( cg_dump_opts * opts, struct sg_lookup * lookup, lane * l )
{
    char buffer[ 1024 ];
    size_t num_writ_buf;
    sg * entry = NULL;

    rc_t rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#GENERATED_BY\t%s\n", UsageDefaultName );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot generate GENERATED_BY for header" );
    }
    else
        rc = write_header_line( l, buffer, num_writ_buf );

    if ( rc == 0 )
    {
        KTime now;
        KTimeLocal ( &now, KTimeStamp () );
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#GENERATED_AT\t%lT\n", &now );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate GENERATED_AT for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#SOFTWARE_VERSION\t%.3V\n", KAppVersion() );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate SOFTWARE_VERSION for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#FORMAT_VERSION\t2.0\n" );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate FORMAT_VERSION for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#TYPE\tSAM_READS\n" );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate SAM_READS for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#SLIDE\t%.*s\n", ( l->name->len - 4 ), l->name->addr );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate SLIDE for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#LANE\t%.*s\n", 3, l->name->addr + ( ( l->name->len - 3 ) ) );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate LANE for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#BATCH_FILE_NUMBER\t%d\n", l->chunk );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate BATCH_FILE_NUMBER for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#LANE\t%S\n", l->name );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate LANE(2) for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        perform_sg_lookup( lookup, l->name, &entry );
        if ( entry != NULL )
            rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#LIBRARY\t%S\n", &entry->lib );
        else
            rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#LIBRARY\t%s\n", opts->lib );   /* opts->lib */
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate LIBRARY for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        if ( entry != NULL )
            rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#SAMPLE\t%S\n", &entry->sample );
        else
            rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#SAMPLE\t%s\n", opts->sample );   /* opts->sample */
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate SAMPLE for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        if ( entry != NULL )
            rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#FIELD_SIZE\t%S\n", &entry->field_size );
        else
            rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "#FIELD_SIZE\t460800\n" );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate FIELD_SIZE for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }

    if ( rc == 0 )
    {
        rc = string_printf ( buffer, sizeof buffer, &num_writ_buf, "\n>readOffset\tside\tbases\tscores\n" );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot generate columns for header" );
        }
        else
            rc = write_header_line( l, buffer, num_writ_buf );
    }
    return rc;
}


static rc_t make_read_file( cg_dump_opts * opts, struct sg_lookup * lookup, KDirectory * dir, lane * l )
{
    rc_t rc;
    if ( opts->comp == oc_null )
    {
        l->reads = NULL;
        rc = 0;
    }
    else
    {
        const char * ext;
        switch( opts->comp )
        {
            case oc_none : ext = none_ext; break;
            case oc_gzip : ext = gzip_ext; break;
            case oc_bzip : ext = bzip_ext; break;
            case oc_null : ext = none_ext; break;
        }
        rc = KDirectoryCreateFile ( dir, &l->reads, true, 0664, kcmCreate, 
                                    "%s%.*s_%03d.tsv%s", opts->prefix, l->name->len, l->name->addr, l->chunk, ext );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create reads-file for lane '$(lane)' / chunk '$(chunk)'",
                                      "lane=%S,chunk=%d", l->name, l->chunk ) );
        }
        else if ( opts->wbuff_size > 0 )
        {
            KFile * buffered;
            rc = KBufWriteFileMakeWrite ( &buffered, l->reads, opts->wbuff_size );
            if ( rc != 0 )
            {
                (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create buffered reads-file for lane '$(lane)' / chunk '$(chunk)'",
                                          "lane=%S,chunk=%d", l->name, l->chunk ) );
            }
            else
            {
                KFileRelease( l->reads );
                l->reads = buffered;
            }
        }

        if ( rc == 0 && opts->comp != oc_none )
        {

            KFile * compressed = l->reads;
            if ( opts->comp == oc_bzip )
            {
                rc = KFileMakeBzip2ForWrite ( &compressed, l->reads );
            }
            else if ( opts->comp == oc_gzip )
            {
                rc = KFileMakeGzipForWrite ( &compressed, l->reads );
            }
            if ( rc != 0 )
            {
                (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create bzip reads-file for lane '$(lane)' / chunk '$(chunk)'",
                                          "lane=%S,chunk=%d", l->name, l->chunk ) );
            }
            else
            {
                KFileRelease( l->reads );
                l->reads = compressed;
            }
        }

        if ( rc == 0 && opts->use_queue )
        {
            KFile * qf;
            rc  = KQueueFileMakeWrite ( &qf, l->reads, opts->qbytes, opts->qblock, opts->qtimeout );
            if ( rc != 0 )
            {
                (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create background writer for lane '$(lane)' / chunk '$(chunk)'",
                                          "lane=%S,chunk=%d", l->name, l->chunk ) );
            }
            else
            {
                KFileRelease( l->reads );
                l->reads = qf;
            }
        }

    }

    if ( rc == 0 )
    {
        rc = write_header( opts, lookup, l );
    }

    return rc;
}


/*
static rc_t make_mappings_file( KDirectory * dir, lane * l )
{
    rc_t rc = KDirectoryCreateFile ( dir, &l->mappings, true, 0664, kcmCreate, 
                                "mappings_%.*s_%03d.tsv.gz", l->name->len, l->name->addr, l->chunk );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create mappings-file for lane '$(lane)' / chunk '$(chunk)'",
                                  "lane=%S,chunk=%d", l->name, l->chunk ) );
    }
    else
    {
        KFile * bz;
        rc = KFileMakeBzip2ForWrite ( &bz, l->mappings );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create bzip mappings-file for lane '$(lane)' / chunk '$(chunk)'",
                                      "lane=%S,chunk=%d", l->name, l->chunk ) );
        }
        else
        {
            KFileRelease( l->mappings );
            l->mappings = bz;
        }
    }
}

*/

static rc_t make_lane( cg_dump_opts * opts, struct sg_lookup * lookup, KDirectory * dir, String * spot_group, lane ** sg_lane )
{
    rc_t rc = 0;
    ( *sg_lane ) = malloc( sizeof ** sg_lane );
    if ( *sg_lane == NULL )
    {
        rc = RC( rcExe, rcDatabase, rcReading, rcMemory, rcExhausted );
        (void)LOGERR( klogErr, rc, "memory exhausted when creating new lane" );
    }
    else
    {
        (*sg_lane)->chunk = opts->first_chunk;
        (*sg_lane)->write_pos = 0;
        (*sg_lane)->spot_count = 0;
        rc = StringCopy ( &( (*sg_lane)->name ), spot_group );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot copy name for new lane" );
            free( *sg_lane );
        }
        else
        {
            rc = make_read_file( opts, lookup, dir, *sg_lane );
            if ( rc != 0 )
            {
                StringWhack ( (*sg_lane)->name );
                free( *sg_lane );
            }
/*
            else
            {
                rc = make_mappings_file( dir, *sg_lane );
                if ( rc != 0 )
                {
                    KFileRelease( (*sg_lane)->reads );
                    StringWhack ( (*sg_lane)->name );
                    free( *sg_lane );
                }
            }
*/
        }
    }
    return rc;
}


static void whack_lane( lane * l )
{
/*    KFileRelease( l->mappings ); */
    KFileRelease( l->reads );
    StringWhack ( l->name );
    free( l );
}


static rc_t cg_dump_write_spot( cg_dump_opts * opts, cg_dump_ctx * cg_ctx, uint64_t row_id, lane * l )
{
    uint32_t elem_bits, boff, read_len;
    const char * read;

    rc_t rc = VCursorCellDataDirect( cg_ctx->seq_cur, row_id, cg_ctx->seq_read_idx, &elem_bits, (const void**)&read, &boff, &read_len );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot read READ in row #$(row_id)", "row_id=%lu", row_id ) );
    }
    else
    {
        uint32_t qual_len;
        const char * qual;
        rc = VCursorCellDataDirect( cg_ctx->seq_cur, row_id, cg_ctx->seq_qual_idx, &elem_bits, (const void**)&qual, &boff, &qual_len );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot read QUALITY in row #$(row_id)", "row_id=%lu", row_id ) );
        }
        else
        {
            if ( ( read_len != 70 ) && ( qual_len != 70 ) )
            {
                rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcInvalid );
                (void)LOGERR( klogErr, rc, "len of read/quality columns do not match cg-length of 2 x 35" );
            }
            else
            {
                char buffer[ 1024 ];
                size_t num_writ_buf;
                rc = string_printf ( buffer, sizeof buffer, &num_writ_buf,
                        "%lu\t0\t%.35s\t%.35s\n%lu\t1\t%.35s\t%.35s\n",
                        row_id, read, qual, row_id, &(read[35]), &(qual[35]) );
                if ( rc != 0 )
                {
                    (void)PLOGERR( klogErr, ( klogErr, rc, "cannot generate output in row #$(row_id)", "row_id=%lu", row_id ) );
                }
                else
                {
                    if ( opts->comp != oc_null )
                    {
                        if ( l->spot_count >= opts->cutoff )
                        {
                            KFileRelease( l->reads );
                            l->chunk++;
                            l->spot_count = 0;
                            l->write_pos = 0;
                            rc = make_read_file( opts, cg_ctx->lookup, cg_ctx->out_dir, l );
                        }
                        if ( rc == 0 )
                        {
                            size_t num_writ_file;
                            rc = KFileWrite ( l->reads, l->write_pos, buffer, num_writ_buf, &num_writ_file );
                            if ( rc != 0 )
                            {
                                (void)PLOGERR( klogErr, ( klogErr, rc, "cannot write output in row #$(row_id)", "row_id=%lu", row_id ) );
                            }
                            else
                            {
                                l->write_pos += num_writ_file;
                                l->spot_count ++;
                            }
                        }
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t cg_dump_row( cg_dump_opts * opts, cg_dump_ctx * cg_ctx, uint64_t row_id )
{
    uint32_t elem_bits, boff, sg_len;
    const char * sg;
    rc_t rc = VCursorCellDataDirect( cg_ctx->seq_cur, row_id, cg_ctx->seq_sg_idx, &elem_bits, (const void**)&sg, &boff, &sg_len );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot read spot-group in row #$(row_id)", "row_id=%lu", row_id ) );
    }
    else
    {
        String spot_group;
        lane * sg_lane;

        StringInit( &spot_group, sg, sg_len, sg_len );
        sg_lane = ( lane * )BSTreeFind ( &cg_ctx->lanes, &spot_group, String_lane_cmp );
        if ( sg_lane == NULL )
        {
            /* KOutMsg( "row %lu (%S) not found, create it\n", row_id, &spot_group ); */
            rc = make_lane( opts, cg_ctx->lookup, cg_ctx->out_dir, &spot_group, &sg_lane );
            if ( rc == 0 )
            {
                rc = BSTreeInsert ( &cg_ctx->lanes, ( BSTNode * )sg_lane, lane_lane_cmp );
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot insert new lane" );
                    whack_lane( sg_lane );
                }
            }
        }
        else
        {
            /* KOutMsg( "row %lu (%S) found, use it\n", row_id, &spot_group ); */
        }
        if ( rc == 0 )
        {
            cg_dump_write_spot( opts, cg_ctx, row_id, sg_lane ); /* <================== */
        }
    }
    return rc;
}


static rc_t cg_dump_loop( cg_dump_opts * opts, cg_dump_ctx * cg_ctx )
{
    const num_gen_iter * iter;
    rc_t rc = num_gen_iterator_make( cg_ctx->rows, &iter );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot make num-gen-iterator" );
    }
    else
    {
        uint64_t row_id;
        rc_t rc1 = num_gen_iterator_next( iter, &row_id );
        while ( rc == 0 && rc1 == 0 )
        {
            rc = Quitting();    /* to be able to cancel the loop by signal */
            if ( rc == 0 )
            {
                rc = cg_dump_row( opts, cg_ctx, row_id ); /* <================== */
                if ( rc == 0 )
                {
                    rc1 = num_gen_iterator_next( iter, &row_id );
                    if ( rc1 == 0 )
                    {
                        if ( opts->show_progress )
                        {
                            uint32_t percent;
                            rc = num_gen_iterator_percent( iter, cg_ctx->fract_digits, &percent );
                            if ( rc == 0 )
                                update_progressbar( cg_ctx->progress, cg_ctx->fract_digits, percent );
                        }
                    }
                }
            }
        }
        if ( opts->show_progress )
            KOutMsg( "\n" );
        num_gen_iterator_destroy( iter );
    }
    return rc;
}


static rc_t cg_dump_add_column( const VCursor * cur, uint32_t * idx, const char * col_name, const char * cur_name )
{
    rc_t rc = VCursorAddColumn ( cur, idx, "%s", col_name );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot add $(col_name) to $(cur_name)",
                                  "col_name=%s,cur_name=%s", col_name, cur_name ) );
    }
    return rc;
}


static rc_t cg_dump_open_cursor( const VCursor * cur, const char * cur_name )
{
    rc_t rc = VCursorOpen ( cur );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot open $(cur_name)", "cur_name=%s", cur_name ) );
    }
    return rc;
}


static rc_t cg_dump_prepare_seq_tab( cg_dump_ctx * cg_ctx )
{
    const VCursor * cur = cg_ctx->seq_cur;
    const char * cur_name = "SEQ-cursor";
    rc_t rc = cg_dump_add_column( cur, &cg_ctx->seq_read_idx, "(INSDC:dna:text)READ", cur_name );
/*
    if ( rc == 0 )
        rc = cg_dump_add_column( cur, &cg_ctx->seq_read_len_idx, "(INSDC:coord:len)READ_LEN", cur_name );
*/
    if ( rc == 0 )
        rc = cg_dump_add_column( cur, &cg_ctx->seq_qual_idx, "(INSDC:quality:text:phred_33)QUALITY", cur_name );
/*
    if ( rc == 0 )
        rc = cg_dump_add_column( cur, &cg_ctx->seq_prim_id_idx, "(I64)PRIMARY_ALIGNMENT_ID", cur_name );
*/
    if ( rc == 0 )
        rc = cg_dump_add_column( cur, &cg_ctx->seq_sg_idx, "(ascii)SPOT_GROUP", cur_name );
    if ( rc == 0 )
        rc = cg_dump_open_cursor( cur, cur_name );
    return rc;
}


/*
static rc_t cg_dump_prepare_prim_tab( cg_dump_ctx * cg_ctx )
{
    const VCursor * cur = cg_ctx->prim_cur;
    const char * cur_name = "PRIM-cursor";
    rc_t rc = cg_dump_add_column( cur, &cg_ctx->prim_cigar_idx, "(ascii)CIGAR_SHORT", cur_name );
    if ( rc == 0 )
        rc = cg_dump_add_column( cur, &cg_ctx->prim_refname_idx, "(ascii)REF_NAME", cur_name );
    if ( rc == 0 )
        rc = cg_dump_add_column( cur, &cg_ctx->prim_refpos_idx, "(INSDC:coord:zero)REF_POS", cur_name );
    if ( rc == 0 )
        rc = cg_dump_open_cursor( cur, cur_name );
    return rc;
}
*/

static rc_t cg_dump_adjust_rowrange( cg_dump_ctx * cg_ctx )
{
    int64_t  first;
    uint64_t count;
    rc_t rc = VCursorIdRange( cg_ctx->seq_cur, 0, &first, &count );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot detect Id-Range for SEQ-cursor" );
    }
    else
    {
        rc = num_gen_range_check( ( num_gen * )cg_ctx->rows, first, count );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot define range of rows" );
        }
    }
    return rc;
}


static rc_t cg_dump_setup_progressbar( cg_dump_ctx * cg_ctx )
{
    rc_t rc = make_progressbar( &cg_ctx->progress );
    if ( rc == 0 )
    {
        uint64_t count;
        const num_gen_iter * iter;
        cg_ctx->fract_digits = 0;
        rc = num_gen_iterator_make( cg_ctx->rows, &iter );
        if ( rc == 0 )
        {
            if ( num_gen_iterator_count( iter, &count ) == 0 )
            {
                if ( count > 10000 )
                {
                    if ( count > 100000 )
                        cg_ctx->fract_digits = 2;
                    else
                        cg_ctx->fract_digits = 1;
                }
            }
            num_gen_iterator_destroy( iter );
        }
    }
    return rc;
}


static rc_t cg_dump_create_output_dir( cg_dump_ctx * cg_ctx )
{
    rc_t rc = KDirectoryCreateDir ( cg_ctx->dir, 0775, ( kcmCreate | kcmParents ), "%s", cg_ctx->dst );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create directory '$(dir)'", "dir=%s", cg_ctx->dst ) );
    }
    return rc;
}


static rc_t cg_dump_clear_output_dir( cg_dump_opts * opts, cg_dump_ctx * cg_ctx )
{
    rc_t rc;
    if ( opts->overwrite )
    {
        rc = KDirectoryClearDir ( cg_ctx->dir, true, "%s", cg_ctx->dst );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot clear directory '$(dt)'",
                                      "dt=%s", cg_ctx->dst ) );
        }
    }
    else
    {
        rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcInvalid );
        (void)PLOGERR( klogErr, ( klogErr, rc, "output-directory exitst already '$(dt)', use force-switch",
                                  "dt=%s", cg_ctx->dst ) );
        KOutMsg( "output-directory exitst already '%s', use force-switch", cg_ctx->dst );
    }
    return rc;
}


static rc_t cg_dump_remove_file( cg_dump_opts * opts, cg_dump_ctx * cg_ctx )
{
    rc_t rc;
    if ( opts->overwrite )
    {
        rc = KDirectoryRemove ( cg_ctx->dir, true, "%s", cg_ctx->dst );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot remove file '$(dt)'",
                                      "dt=%s", cg_ctx->dst ) );
        }
    }
    else
    {
        rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcInvalid );
        (void)PLOGERR( klogErr, ( klogErr, rc, "output-directory-name exists as file '$(dt)', use force-switch",
                                  "dt=%s", cg_ctx->dst ) );
        KOutMsg( "output-directory-name exists as file '%s', use force-switch", cg_ctx->dst );

    }
    return rc;
}


static rc_t cg_dump_prepare_output( cg_dump_opts * opts, cg_dump_ctx * cg_ctx )
{
    rc_t rc = 0;

    uint32_t pt = ( KDirectoryPathType ( cg_ctx->dir, "%s", cg_ctx->dst ) & ~ kptAlias );
    switch ( pt )
    {
        case kptNotFound :  rc = cg_dump_create_output_dir( cg_ctx ); break;

        case kptDir  :  rc = cg_dump_clear_output_dir( opts, cg_ctx ); break;

        case kptFile :  rc = cg_dump_remove_file( opts, cg_ctx ); 
                        if ( rc == 0 )
                            rc = cg_dump_create_output_dir( cg_ctx );
                        break;

        default :   rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcInvalid );
                    (void)PLOGERR( klogErr, ( klogErr, rc, "invalid output-directory-type '$(dt)'",
                                              "dt=%s", pathtype_2_pchar( pt ) ) );
                    break;
    }
    if ( rc == 0 )
    {
        rc = KDirectoryOpenDirUpdate ( cg_ctx->dir, &cg_ctx->out_dir, false, "%s", cg_ctx->dst );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot open output-directory for update" );
        }
        else
        {
            BSTreeInit( &cg_ctx->lanes );
        }
    }
    return rc;
}


static void CC whack_lanes_nodes( BSTNode *n, void *data )
{
    whack_lane( ( lane * )n );
}


static rc_t cg_dump_src_dst_rows_cur( cg_dump_opts * opts, cg_dump_ctx * cg_ctx )
{
    /* preparations */
    rc_t rc = cg_dump_prepare_seq_tab( cg_ctx );
/*
    if ( rc == 0 )
        rc = cg_dump_prepare_prim_tab( cg_ctx );
*/
    if ( rc == 0 )
        rc = cg_dump_adjust_rowrange( cg_ctx );
    if ( rc == 0 )
        rc = cg_dump_setup_progressbar( cg_ctx );
    if ( rc == 0 )
        rc = cg_dump_prepare_output( opts, cg_ctx );

    /* loop through the SEQUENCE-table */
    if ( rc == 0 )
    {
        rc = cg_dump_loop( opts, cg_ctx ); /* <================== */

        if ( cg_ctx->progress != NULL )
            destroy_progressbar( cg_ctx->progress );
        BSTreeWhack ( &cg_ctx->lanes, whack_lanes_nodes, NULL );
        KDirectoryRelease( cg_ctx->out_dir );
    }
    return rc;
}


static rc_t cg_dump_src_dst_rows_vdb( cg_dump_opts * opts, const VDBManager * mgr, const char * src, cg_dump_ctx * cg_ctx )
{
    const VDatabase * db;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", src );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot open database" );
    }
    else
    {
        rc = parse_sg_lookup( cg_ctx->lookup, db ); /* in sg_lookup.c */
        if ( rc == 0 )
        {
            const VTable * seq;
            rc = VDatabaseOpenTableRead( db, &seq, "SEQUENCE" );
            if ( rc != 0 )
            {
                (void)LOGERR( klogErr, rc, "cannot open SEQUENCE-table" );
            }
            else
            {
                const VTable * prim;
                rc = VDatabaseOpenTableRead( db, &prim, "PRIMARY_ALIGNMENT" );
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot open PRIMARY-ALIGNMENT-table" );
                }
                else
                {
                    if ( opts->cursor_cache > 0 )
                        rc = VTableCreateCachedCursorRead( seq, &cg_ctx->seq_cur, opts->cursor_cache );
                    else
                        rc = VTableCreateCursorRead( seq, &cg_ctx->seq_cur );
                    if ( rc != 0 )
                    {
                        (void)LOGERR( klogErr, rc, "cannot create cursor for SEQUENCE-table" );
                    }
                    else
                    {
    /*
                        if ( opts->cursor_cache > 0 )
                            rc = VTableCreateCachedCursorRead( prim, &cg_ctx->prim_cur, opts->cursor_cache );
                        else
                            rc = VTableCreateCursorRead( prim, &cg_ctx->prim_cur );
                        if ( rc != 0 )
                        {
                            (void)LOGERR( klogErr, rc, "cannot create cursor for PRIMARY_ALIGNMENT-table" );
                        }
    */

                        if ( rc == 0 )
                        {
                            rc = cg_dump_src_dst_rows_cur( opts, cg_ctx );  /* <================== */
                            /* VCursorRelease( cg_ctx->prim_cur ); */
                        }
                        VCursorRelease( cg_ctx->seq_cur );
                    }
                    /* VTableRelease( prim ); */
                }
                VTableRelease( seq );
            }
        }
        VDatabaseRelease( db );
    }
    return rc;
}


static rc_t cg_dump_src_dst_rows( cg_dump_opts * opts, const char * src, cg_dump_ctx * cg_ctx )
{
    rc_t rc = KDirectoryNativeDir( &cg_ctx->dir );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot create internal directory object" );
    }
    else
    {
        const VDBManager * mgr;
        rc = VDBManagerMakeRead ( &mgr, cg_ctx->dir );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot create vdb-manager" );
        }
        else
        {
            int path_type = ( VDBManagerPathType ( mgr, "%s", src ) & ~ kptAlias );
            if ( path_type != kptDatabase )
            {
                rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcInvalid );
                if ( path_type == kptNotFound )
                {
                    (void)LOGERR( klogErr, rc, "the source object cannot be found" );
                }
                else
                {
                    (void)LOGERR( klogErr, rc, "source cannot be used" );
                    KOutMsg( "it is instead: '%s'\n", pathtype_2_pchar( path_type ) );
                }
            }
            else
            {
                rc = cg_dump_src_dst_rows_vdb( opts, mgr, src, cg_ctx ); /* <================== */
            }
            VDBManagerRelease( mgr );
        }
        KDirectoryRelease( cg_ctx->dir );
    }
    return rc;
}


static rc_t cg_dump_discover_last_rowid( const char * dst )
{
    rc_t rc = KOutMsg( "\n***** discover last processed row-id *****\n" );
    if ( rc == 0 )
    {
        int64_t last_row_id = 0;
        rc = discover_last_rowid( dst, &last_row_id );
        if ( rc == 0 )
        {
            rc = KOutMsg( "last row-id: %ld\n", last_row_id );
        }
    }
    return rc;
}


static rc_t cg_dump_get_uint64t_option( Args * args, const char * name, uint64_t * value, bool * found  )
{
    uint32_t count;

    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect count of $(name) option", "name=%s", name ) );
    }
    else
    {
        *found = ( count > 0 );
    }

    if ( rc == 0 && ( *found ) )
    {
        const char * s;
        rc = ArgsOptionValue( args, name, 0,  &s );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect value of $(name) option", "name=%s", name ) );
        }
        else
        {
            char * endp;
            *value = strtou64( s, &endp, 10 );
        }
    }
    return rc;
}


static rc_t cg_dump_get_size_option( Args * args, const char * name, size_t * value, bool * found  )
{
    uint32_t count;

    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect count of $(name) option", "name=%s", name ) );
    }
    else
    {
        *found = ( count > 0 );
    }

    if ( rc == 0 && ( *found ) )
    {
        const char * s;
        rc = ArgsOptionValue( args, name, 0,  &s );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect value of $(name) option", "name=%s", name ) );
        }
        else
        {
            *value = atoi( s );
        }
    }
    return rc;
}


static rc_t cg_dump_get_uint32t_option( Args * args, const char * name, uint32_t * value, bool * found  )
{
    uint32_t count;

    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect count of $(name) option", "name=%s", name ) );
    }
    else
    {
        *found = ( count > 0 );
    }

    if ( rc == 0 && ( *found ) )
    {
        const char * s;
        rc = ArgsOptionValue( args, name, 0,  &s );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect value of $(name) option", "name=%s", name ) );
        }
        else
        {
            *value = atoi( s );
        }
    }
    return rc;
}


static rc_t cg_dump_get_bool_option( Args * args, const char * name, bool * value )
{
    uint32_t count;

    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect count of $(name) option", "name=%s", name ) );
    }
    else
    {
        *value = ( count > 0 );
    }
    return rc;
}


static rc_t cg_dump_get_string_option( Args * args, const char * name, const char ** value, bool * found  )
{
    uint32_t count;

    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect count of $(name) option", "name=%s", name ) );
    }
    else
    {
        *found = ( count > 0 );
    }

    if ( rc == 0 && ( *found ) )
    {
        rc = ArgsOptionValue( args, name, 0, value );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot detect value of $(name) option", "name=%s", name ) );
        }
    }
    return rc;
}


const char * value_comp_none = "none";
const char * value_comp_bzip = "bzip";
const char * value_comp_gzip = "gzip";
const char * value_comp_null = "null";


static rc_t cg_dump_gather_output_compression( Args * args, cg_dump_opts * opts )
{
    const char * value;
    bool found;
    rc_t rc = cg_dump_get_string_option( args, OPTION_COMP, &value, &found );
    if ( rc == 0 )
    {
        opts->comp = oc_bzip;
        if ( found )
        {
            if ( string_cmp ( value, string_size( value ),
                              value_comp_none, string_size( value_comp_none ), string_size( value ) ) == 0 )
            {
                opts->comp = oc_none;
            }
            else if ( string_cmp ( value, string_size( value ),
                                   value_comp_bzip, string_size( value_comp_bzip ), string_size( value ) ) == 0 )
            {
                opts->comp = oc_bzip;
            }
            else if ( string_cmp ( value, string_size( value ),
                                   value_comp_gzip, string_size( value_comp_gzip ), string_size( value ) ) == 0 )
            {
                opts->comp = oc_gzip;
            }
            else  if ( string_cmp ( value, string_size( value ),
                                   value_comp_null, string_size( value_comp_null ), string_size( value ) ) == 0 )
            {
                opts->comp = oc_null;
            }
        }
    }
    return rc;
}


const char * dflt_lib = "no library";
const char * dflt_sample = "no sample";
const char * dflt_prefix = "";

static rc_t cg_dump_gather_opts( Args * args, cg_dump_opts * opts )
{
    bool found;

    rc_t rc = cg_dump_get_uint64t_option( args, OPTION_CUTOFF, &opts->cutoff, &found );
    if ( rc == 0 && !found )
        opts->cutoff = DEFAULT_CUTOFF;

    if ( rc == 0 )
        rc = cg_dump_get_bool_option( args, OPTION_FORCE, &opts->overwrite  );

    if ( rc == 0 )
        rc = cg_dump_get_uint64t_option( args, OPTION_CACHE, &opts->cursor_cache, &found );
    if ( rc == 0 && !found )
        opts->cursor_cache = CURSOR_CACHE_SIZE;

    if ( rc == 0 )
        rc = cg_dump_gather_output_compression( args, opts );

    if ( rc == 0 )
        rc = cg_dump_get_bool_option( args, OPTION_PROG, &opts->show_progress  );

    opts->first_chunk = 1;

    if ( rc == 0 )
        rc = cg_dump_get_string_option( args, OPTION_LIB, &opts->lib, &found );
    if ( rc == 0 && !found )
        opts->lib = dflt_lib;

    if ( rc == 0 )
        rc = cg_dump_get_string_option( args, OPTION_SAMPLE, &opts->sample, &found );
    if ( rc == 0 && !found )
        opts->sample = dflt_sample;

    if ( rc == 0 )
        rc = cg_dump_get_string_option( args, OPTION_PREFIX, &opts->prefix, &found );
    if ( rc == 0 && !found )
        opts->prefix = dflt_prefix;

    if ( rc == 0 )
        rc = cg_dump_get_size_option( args, OPTION_WBUF, &opts->wbuff_size, &found );

    if ( rc == 0 )
        rc = cg_dump_get_bool_option( args, OPTION_QUEUE, &opts->use_queue );

    if ( rc == 0 )
        rc = cg_dump_get_size_option( args, OPTION_QUEUE_BYTES, &opts->qbytes, &found );
    if ( rc == 0 && !found )
        opts->qbytes = ( 2 * 64 * 1024 );

    if ( rc == 0 )
        rc = cg_dump_get_size_option( args, OPTION_QUEUE_BLOCK, &opts->qblock, &found );
    if ( rc == 0 && !found )
        opts->qblock = ( 64 * 1024 );

    if ( rc == 0 )
        rc = cg_dump_get_uint32t_option( args, OPTION_QUEUE_TIMEOUT, &opts->qtimeout, &found );
    if ( rc == 0 && !found )
        opts->qtimeout = 10000;

    if ( rc == 0 )
        rc = cg_dump_get_bool_option( args, OPTION_LAST_ROWID, &opts->get_last_rowid );

    return rc;
}


static rc_t cg_dump_src_dst( Args * args, const char * src, cg_dump_ctx * cg_ctx )
{
    rc_t rc = num_gen_make( &cg_ctx->rows );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "cannot create internal object" );
    }
    else
    {
        uint32_t count;
        rc = ArgsOptionCount( args, OPTION_ROWS, &count );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot detect count of program option : 'rows'" );
        }
        else
        {
            if ( count > 0 )
            {
                const char * s;
                rc = ArgsOptionValue( args, OPTION_ROWS, 0,  &s );
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot detect value of program option : 'rows'" );
                }
                else
                {
                    num_gen_parse( cg_ctx->rows, s );
                }
            }
            if ( rc == 0 )
            {
                rc = make_sg_lookup( &cg_ctx->lookup ); /* in sg_lookup.c */
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot create spot-group lookup table" );
                }
                else
                {
                    cg_dump_opts opts;
                    memset( &opts, 0, sizeof opts );
                    rc = cg_dump_gather_opts( args, &opts );
                    if ( rc == 0 )
                    {
                        if ( opts.get_last_rowid )
                        {
                            rc = cg_dump_discover_last_rowid( cg_ctx->dst );
                        }
                        else
                        {
                            rc = cg_dump_src_dst_rows( &opts, src, cg_ctx ); /* <================== */
                        }
                    }

                    destroy_sg_lookup( cg_ctx->lookup ); /* in sg_lookup.c */
                }
            }
        }
        num_gen_destroy( cg_ctx->rows );
    }
    return rc;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
            DumpOptions, sizeof DumpOptions / sizeof DumpOptions [ 0 ] );
    if ( rc == 0 )
    {
        uint32_t count;
        rc = ArgsParamCount( args, &count );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot detect count of program arguments" );
        }
        else
        {
            if ( count != 2 )
            {
                Usage ( args );
            }
            else
            {
                cg_dump_ctx cg_ctx;
                const char * src;

                memset( &cg_ctx, 0, sizeof cg_ctx );
                rc = ArgsParamValue( args, 0, &src );
                if ( rc != 0 )
                {
                    (void)LOGERR( klogErr, rc, "cannot detect source - argument" );
                }
                else
                {
                    rc = ArgsParamValue( args, 1, &cg_ctx.dst );
                    if ( rc != 0 )
                    {
                        (void)LOGERR( klogErr, rc, "cannot detect destination - argument" );
                    }
                    else
                    {
                        rc = cg_dump_src_dst( args, src, &cg_ctx ); /* <================== */
                    }
                }
            }
        }
        ArgsWhack ( args );
    }
    return rc;
}
