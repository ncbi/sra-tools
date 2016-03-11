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

#include "fastdump.vers.h"

#include "cmn_iter.h"
#include "file_printer.h"
#include "raw_read_iter.h"
#include "special_iter.h"
#include "fastq_iter.h"
#include "lookup_writer.h"
#include "lookup_reader.h"
#include "sorter.h"
#include "helper.h"

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/out.h>
#include <kfs/directory.h>

#include <os-native.h>
#include <sysalloc.h>

static const char * lookup_usage[] = { "lookup file", NULL };
#define OPTION_LOOKUP   "lookup"
#define ALIAS_LOOKUP    "l"

static const char * range_usage[] = { "row-range", NULL };
#define OPTION_RANGE    "range"
#define ALIAS_RANGE     "R"

static const char * format_usage[] = { "format (special,fastq,lookup)", NULL };
#define OPTION_FORMAT   "format"
#define ALIAS_FORMAT     "f"

static const char * output_usage[] = { "output-file", NULL };
#define OPTION_OUTPUT   "out"
#define ALIAS_OUTPUT    "o"

static const char * progress_usage[] = { "show progress", NULL };
#define OPTION_PROGRESS "progress"
#define ALIAS_PROGRESS  "p"

static const char * bufsize_usage[] = { "size of file-buffer ( default=1MB )", NULL };
#define OPTION_BUFSIZE  "bufsize"
#define ALIAS_BUFSIZE   "b"

static const char * curcache_usage[] = { "size of cursor-cache ( default=10MB )", NULL };
#define OPTION_CURCACHE "curcache"
#define ALIAS_CURCACHE  "c"

static const char * nsub_usage[] = { "number of sub-files for lookup creation ( default=8 )", NULL };
#define OPTION_NSUB     "nsub"
#define ALIAS_NSUB      "n"

static const char * temp_usage[] = { "where to put temp. files ( default=curr. dir )", NULL };
#define OPTION_TEMP     "temp"
#define ALIAS_TEMP      "t"

static const char * threads_usage[] = { "how many thread ( default=1 )", NULL };
#define OPTION_THREADS  "threads"
#define ALIAS_THREADS   "e"

OptDef ToolOptions[] =
{
    { OPTION_RANGE,     ALIAS_RANGE,     NULL, range_usage,      1, true,   false },
    { OPTION_LOOKUP,    ALIAS_LOOKUP,    NULL, lookup_usage,     1, true,   false },
    { OPTION_FORMAT,    ALIAS_FORMAT,    NULL, format_usage,     1, true,   false },
    { OPTION_OUTPUT,    ALIAS_OUTPUT,    NULL, output_usage,     1, true,   false },
    { OPTION_BUFSIZE,   ALIAS_BUFSIZE,   NULL, bufsize_usage,    1, true,   false },
    { OPTION_CURCACHE,  ALIAS_CURCACHE,  NULL, curcache_usage,   1, true,   false },
    { OPTION_NSUB,      ALIAS_NSUB,      NULL, nsub_usage,       1, true,   false },
    { OPTION_TEMP,      ALIAS_TEMP,      NULL, temp_usage,       1, true,   false },
    { OPTION_THREADS,   ALIAS_THREADS,   NULL, threads_usage,    1, true,   false },
    { OPTION_PROGRESS,  ALIAS_PROGRESS,  NULL, progress_usage,   1, false,  false }
};

const char UsageDefaultName[] = "fastdump";

rc_t CC UsageSummary( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [options]\n"
                     "\n", progname );
}


rc_t CC Usage ( const Args * args )
{
    rc_t rc;
    uint32_t idx, count = ( sizeof ToolOptions ) / ( sizeof ToolOptions[ 0 ] );
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;

    if ( args == NULL )
        rc = RC( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary( progname );

    KOutMsg( "Options:\n" );
    for ( idx = 1; idx < count; ++idx ) /* start with 1, do not advertize row-range-option*/
        HelpOptionLine( ToolOptions[ idx ].aliases, ToolOptions[ idx ].name, NULL, ToolOptions[ idx ].help );
    
    HelpOptionsStandard();
    HelpVersion( fullpath, KAppVersion() );
    return rc;
}


/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion( void )
{
    return FASTDUMP_VERS;
}


/* -------------------------------------------------------------------------------------------- */

static rc_t print_special( special_rec * rec, struct lookup_reader * lookup,
            SBuffer * B1, SBuffer * B2, struct file_printer * printer )
{
    rc_t rc = 0;
    int64_t row_id = rec->row_id;
    
    if ( rec->prim_alig_id[ 0 ] == 0 )
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* both unaligned, print what is in row->cmp_read ( !!! no lookup !!! )*/
            if ( printer != NULL )
                rc = file_print( printer, "%ld\t%S\t%S\n", row_id, &rec->cmp_read, &rec->spot_group );
            else
                rc = KOutMsg( "%ld\t%S\t%S\n", row_id, &rec->cmp_read, &rec->spot_group );
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            rc = lookup_bases( lookup, row_id, 2, B2 );
            if ( rc == 0 )
            {
                if ( printer != NULL )
                    rc = file_print( printer, "%ld\t%S%S\t%S\n", row_id, &rec->cmp_read, &B2->S, &rec->spot_group );
                else
                    rc = KOutMsg( "%ld\t%S%S\t%S\n", row_id, &rec->cmp_read, &B2->S, &rec->spot_group );
            }
        }
    }
    else
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( lookup, row_id, 1, B1 );
            if ( rc == 0 )
            {
                if ( printer != NULL )
                    rc = file_print( printer, "%ld\t%S%S\t%S\n", row_id, &B1->S, &rec->cmp_read, &rec->spot_group );
                else
                    rc = KOutMsg( "%ld\t%S%S\t%S\n", row_id, &B1->S, &rec->cmp_read, &rec->spot_group );
            }
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( lookup, row_id, 1, B1 );
            if ( rc == 0 )
                rc = lookup_bases( lookup, row_id, 2, B2 );
            if ( rc == 0 )
            {
                if ( printer != NULL )
                    rc = file_print( printer, "%ld\t%S%S\t%S\n", row_id, &B1->S, &B2->S, &rec->spot_group );
                else
                    rc = KOutMsg( "%ld\t%S%S\t%S\n", row_id, &B1->S, &B2->S, &rec->spot_group );
            }
        }

    }
    return rc;
}


/* -------------------------------------------------------------------------------------------- */

static rc_t print_fastq( const char * acc, fastq_rec * rec, struct lookup_reader * lookup,
            SBuffer * B1, SBuffer * B2, struct file_printer * printer )
{
    rc_t rc = 0;
    int64_t row_id = rec->row_id;

    if ( rec->prim_alig_id[ 0 ] == 0 )
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* both unaligned, print what is in row->cmp_read (no lookup)*/
            const char * fmt = "@%s.%ld %ld length=%d\n%S\n+%s.%ld %ld length=%d\n%S\n";
            if ( printer != NULL )
                rc = file_print( printer, fmt,
                    acc, row_id, row_id, rec->cmp_read.len, &rec->cmp_read,
                    acc, row_id, row_id, rec->quality.len, &rec->quality );
            else
                rc = KOutMsg( fmt,
                    acc, row_id, row_id, rec->cmp_read.len, &rec->cmp_read,
                    acc, row_id, row_id, rec->quality.len, &rec->quality );
                
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            rc = lookup_bases( lookup, row_id, 2, B2 );
            if ( rc == 0 )
            {
                const char * fmt = "@%s.%ld %ld length=%d\n%S%S\n+%s.%ld %ld length=%d\n%S\n";
                if ( printer != NULL )
                    rc = file_print( printer, fmt,
                        acc, row_id, row_id, rec->cmp_read.len + B2->S.len, &rec->cmp_read, &B2->S,
                        acc, row_id, row_id, rec->quality.len, &rec->quality );
                else
                    rc = KOutMsg( fmt,
                        acc, row_id, row_id, rec->cmp_read.len + B2->S.len, &rec->cmp_read, &B2->S,
                        acc, row_id, row_id, rec->quality.len, &rec->quality );
            }
        }
    }
    else
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( lookup, row_id, 1, B1 );
            if ( rc == 0 )
            {
                const char * fmt = "@%s.%ld %ld length=%d\n%S%S\n+%s.%ld %ld length=%d\n%S\n";
                if ( printer != NULL )
                    rc = file_print( printer, fmt,
                        acc, row_id, row_id, rec->cmp_read.len + B1->S.len, &B1->S, &rec->cmp_read,
                        acc, row_id, row_id, rec->quality.len, &rec->quality );
                else
                    rc = KOutMsg( fmt,
                        acc, row_id, row_id, rec->cmp_read.len + B1->S.len, &B1->S, &rec->cmp_read,
                        acc, row_id, row_id, rec->quality.len, &rec->quality );
            }
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( lookup, row_id, 1, B1 );
            if ( rc == 0 )
                rc = lookup_bases( lookup, row_id, 2, B2 );
            if ( rc == 0 )
            {
                const char * fmt = "@%s.%ld %ld length=%d\n%S%S\n+%s.%ld %ld length=%d\n%S\n";
                if ( printer != NULL )
                    rc = file_print( printer, fmt,
                        acc, row_id, row_id, B1->S.len + B2->S.len, &B1->S, &B2->S,
                        acc, row_id, row_id, rec->quality.len, &rec->quality );
                else
                    rc = KOutMsg( fmt,
                        acc, row_id, row_id, B1->S.len + B2->S.len, &B1->S, &B2->S,
                        acc, row_id, row_id, rec->quality.len, &rec->quality );
            }
        }
    }
    return rc;
}

/* -------------------------------------------------------------------------------------------- */

typedef struct fd_ctx
{
    cmn_params cmn;
    const char * lookup_file;
    const char * output_file;
    const char * temp_path;
    size_t buf_size, sort_split;
    uint64_t num_threads;
} fd_ctx;


/* --------------------------------------------------------------------------------------------
    produce special-output ( SPOT_ID,READ,SPOT_GROUP ) by iterating over the SEQUENCE - table:
   -------------------------------------------------------------------------------------------- 
   
-------------------------------------------------------------------------------------------- */
static rc_t fastdump_special( fd_ctx * fd_ctx )
{
    struct lookup_reader * lookup;
    rc_t rc = make_lookup_reader( fd_ctx->cmn.dir, &lookup, fd_ctx->buf_size, "%s", fd_ctx->lookup_file );
    if ( rc == 0 )
    {
        struct special_iter * iter;
        rc = make_special_iter( &fd_ctx->cmn, &iter );
        if ( rc == 0 )
        {
            SBuffer B1;
            rc = make_SBuffer( &B1, 4096 );
            if ( rc == 0 )
            {
                SBuffer B2;
                rc = make_SBuffer( &B2, 4096 );
                if ( rc == 0 )
                {
                    special_rec rec;
                    struct file_printer * printer = NULL;
                    
                    if ( fd_ctx->output_file != NULL )
                        rc = make_file_printer( fd_ctx->cmn.dir, &printer, fd_ctx->buf_size, 4096 * 4, "%s", fd_ctx->output_file );
                    
                    while ( get_from_special_iter( iter, &rec, &rc ) && rc == 0 )
                    {
                        rc = print_special( &rec, lookup, &B1, &B2, printer );
                    }
                    
                    destroy_file_printer( printer );
                    release_SBuffer( &B2 );
                }
                release_SBuffer( &B1 );
            }
            destroy_special_iter( iter );
        }
        release_lookup_reader( lookup );
    }
    return rc;
}


/* --------------------------------------------------------------------------------------------
    produce fastq-output by iterating over the SEQUENCE - table:
   -------------------------------------------------------------------------------------------- 
   
-------------------------------------------------------------------------------------------- */
static rc_t fastdump_fastq( fd_ctx * fd_ctx )
{
    struct lookup_reader * lookup;
    rc_t rc = make_lookup_reader( fd_ctx->cmn.dir, &lookup, fd_ctx->buf_size, "%s", fd_ctx->lookup_file );
    if ( rc == 0 )
    {
        struct fastq_iter * iter;
        rc = make_fastq_iter( &fd_ctx->cmn, &iter );
        if ( rc == 0 )
        {
            SBuffer B1;
            rc = make_SBuffer( &B1, 4096 );
            if ( rc == 0 )
            {
                SBuffer B2;
                rc = make_SBuffer( &B2, 4096 );
                if ( rc == 0 )
                {
                    fastq_rec rec;
                    struct file_printer * printer = NULL;

                    if ( fd_ctx->output_file != NULL )
                        rc = make_file_printer( fd_ctx->cmn.dir, &printer, fd_ctx->buf_size, 4096 * 4, "%s", fd_ctx->output_file );

                    while ( get_from_fastq_iter( iter, &rec, &rc ) && rc == 0 )
                    {
                        rc = print_fastq( fd_ctx->cmn.acc, &rec, lookup, &B1, &B2, printer );
                    }
                    
                    destroy_file_printer( printer );
                    release_SBuffer( &B2 );
                }
                release_SBuffer( &B1 );
            }
            destroy_fastq_iter( iter );
        }
        release_lookup_reader( lookup );
    }
    return rc;
}


/* --------------------------------------------------------------------------------------------
    produce the lookup-table by iterating over the PRIMARY_ALIGNMENT - table:
   -------------------------------------------------------------------------------------------- 
    reading SEQ_SPOT_ID, SEQ_READ_ID and RAW_READ
    SEQ_SPOT_ID and SEQ_READ_ID is merged into a 64-bit-key
    RAW_READ is read as 4na-unpacked ( Schema does not provide 4na-packed for this column )
    these key-pairs are temporarely stored in a KVector until a limit is reached
    after that limit is reached they are writen sorted into the file-system as sub-files
    this repeats until the requested row-range is exhausted ( row_range ... NULL -> all rows )
    These sub-files are than merge-sorted into the final output-file.
    This output-file is a binary data-file:
    content: [KEY][RAW_READ]
    KEY... 64-bit value as SEQ_SPOT_ID shifted left by 1 bit, zero-bit contains SEQ_READ_ID
    RAW_READ... 16-bit binary-chunk-lenght, followed by n bytes of packed 4na
-------------------------------------------------------------------------------------------- */
static rc_t single_threaded_make_lookup( fd_ctx * fd_ctx, struct raw_read_iter * iter )
{
    rc_t rc;
    sorter_params params;
    struct sorter * sorter;
    uint64_t row_count = get_row_count_of_raw_read( iter );

    params.dir = fd_ctx->cmn.dir;
    params.output_filename = fd_ctx->output_file;
    params.temp_path = fd_ctx->temp_path;
    params.use_subfiles = ( fd_ctx->sort_split > 1 );        
    if ( row_count > 500000 && params.use_subfiles )
        params.store_limit = ( row_count / fd_ctx->sort_split ) + 1;
    else
        params.store_limit = row_count;
    params.buf_size = fd_ctx->buf_size;

    rc = make_sorter( &params, &sorter );
    if ( rc == 0 )
    {
        raw_read_rec rec;
        while ( rc == 0 && get_from_raw_read_iter( iter, &rec, &rc ) )
        {
            if ( rc == 0 )
                rc = write_to_sorter( sorter, rec.seq_spot_id, rec.seq_read_id, &rec.raw_read );
        }
        rc = finalize_sorter( sorter );
        release_sorter( sorter );
    }
    destroy_raw_read_iter( iter );
    return rc;
}


static rc_t multi_threaded_make_lookup( fd_ctx * fd_ctx, struct raw_read_iter * iter )
{
    rc_t rc = 0;
    uint64_t row_count = get_row_count_of_raw_read( iter );
    
    destroy_raw_read_iter( iter );
    return rc;
}

/* --------------------------------------------------------------------------------------------
    produce the lookup-table by iterating over the PRIMARY_ALIGNMENT - table:
   -------------------------------------------------------------------------------------------- 
    reading SEQ_SPOT_ID, SEQ_READ_ID and RAW_READ
    SEQ_SPOT_ID and SEQ_READ_ID is merged into a 64-bit-key
    RAW_READ is read as 4na-unpacked ( Schema does not provide 4na-packed for this column )
    these key-pairs are temporarely stored in a KVector until a limit is reached
    after that limit is reached they are writen sorted into the file-system as sub-files
    this repeats until the requested row-range is exhausted ( row_range ... NULL -> all rows )
    These sub-files are than merge-sorted into the final output-file.
    This output-file is a binary data-file:
    content: [KEY][RAW_READ]
    KEY... 64-bit value as SEQ_SPOT_ID shifted left by 1 bit, zero-bit contains SEQ_READ_ID
    RAW_READ... 16-bit binary-chunk-lenght, followed by n bytes of packed 4na
-------------------------------------------------------------------------------------------- */
static rc_t fastdump_make_lookup( fd_ctx * fd_ctx )
{
    struct raw_read_iter * iter;
    rc_t rc = make_raw_read_iter( &fd_ctx->cmn, &iter );
    if ( rc == 0 )
    {
        if ( fd_ctx->num_threads > 1 )
            rc = multi_threaded_make_lookup( fd_ctx, iter );
        else
            rc = single_threaded_make_lookup( fd_ctx, iter );
    }
    return rc;
}


/* --------------------------------------------------------------------------------------------
    produces the binaray lookup-table as readable ascii-file
   -------------------------------------------------------------------------------------------- 
    exercises the lookup-reader, writes each alignment as one line
    SEQ_READ_ID,SEQ_SPOT_ID,RAW_READ ( tab-separated )
    used to be compared to the output of
    'vdb-dump SRRXXX -T PRIMARY_ALIGNMENT -C SEQ_SPOT_ID,SEQ_READ_ID,RAW_READ -f tab > XXX.txt'
    xxx.txt has to be numerically sorted! 'sort xxx.txt -n -T scratch -o yyy.txt'
    don't forget to give the sort-command a scratch-path that has enough space!
    yyy.txt can the be compared agains the output of this function
-------------------------------------------------------------------------------------------- */
static rc_t fastdump_test( fd_ctx * fd_ctx )
{
    struct lookup_reader * reader;
    rc_t rc = make_lookup_reader( fd_ctx->cmn.dir, &reader, fd_ctx->buf_size, "%s", fd_ctx->lookup_file );
    if ( rc == 0 )
    {
        SBuffer bases;
        int64_t  seq_spot_id;
        uint32_t seq_read_id;
        rc = make_SBuffer( &bases, 4096 );
        if ( rc == 0 )
        {
            struct file_printer * printer = NULL;
            if ( fd_ctx->output_file != NULL )
                rc = make_file_printer( fd_ctx->cmn.dir, &printer, fd_ctx->buf_size, 4096 * 4, "%s", fd_ctx->output_file );
            
            while ( rc == 0 )
            {
                rc = get_bases_from_lookup_reader( reader, &seq_spot_id, &seq_read_id, &bases );
                if ( rc == 0 )
                {
                
                    if ( printer != NULL )
                        rc = file_print( printer, "%lu\t%u\t%S\n", seq_spot_id, seq_read_id, &bases.S );
                    else
                        rc = KOutMsg( "%lu\t%u\t%S\n", seq_spot_id, seq_read_id, &bases.S );                    
                }
            }
            destroy_file_printer( printer );
            
            release_SBuffer( &bases );
        }
        release_lookup_reader( reader );
    }
    return rc;
}


/* -------------------------------------------------------------------------------------------- */

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    if ( rc == 0 )
    {
        fd_ctx fd_ctx;
        rc = ArgsParamValue( args, 0, (const void **)&fd_ctx.cmn.acc );
        if ( rc != 0 )
            ErrMsg( "ArgsParamValue() -> %R", rc );
        else
        {
            fd_ctx.cmn.row_range = get_str_option( args, OPTION_RANGE, NULL );
            fd_ctx.cmn.cursor_cache = get_size_t_option( args, OPTION_CURCACHE, 10 * 1024 * 1024 );            
            fd_ctx.cmn.show_progress = get_bool_option( args, OPTION_PROGRESS );
            
            fd_ctx.output_file = get_str_option( args, OPTION_OUTPUT, NULL );
            fd_ctx.temp_path = get_str_option( args, OPTION_TEMP, NULL );
            fd_ctx.lookup_file = get_str_option( args, OPTION_LOOKUP, NULL );
            fd_ctx.buf_size = get_size_t_option( args, OPTION_BUFSIZE, 1024 * 1024 );
            fd_ctx.sort_split = get_uint64_t_option( args, OPTION_NSUB, 8 );
            fd_ctx.num_threads = get_uint64_t_option( args, OPTION_THREADS, 1 );
            
            rc = KDirectoryNativeDir( &fd_ctx.cmn.dir );
            if ( rc != 0 )
                ErrMsg( "KDirectoryNativeDir() -> %R\n", rc );
            else
            {
                const char * format = get_str_option( args, OPTION_FORMAT, NULL );
                format_t f = get_format_t( format );
                switch( f )
                {
                    case ft_special : rc = fastdump_special( &fd_ctx ); break;
                    case ft_fastq   : rc = fastdump_fastq( &fd_ctx ); break;
                    case ft_lookup  : rc = fastdump_make_lookup( &fd_ctx ); break;
                    case ft_test    : rc = fastdump_test( &fd_ctx ); break;
                }
                KDirectoryRelease( fd_ctx.cmn.dir );
            }
        }
    }
    return rc;
}
