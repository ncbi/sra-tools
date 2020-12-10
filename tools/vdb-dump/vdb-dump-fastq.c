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

#include "vdb-dump-fastq.h"
#include "vdb-dump-helper.h"
#include "vdb-dump-tools.h"

#include <stdlib.h>

#include <kdb/manager.h>
#include <vdb/vdb-priv.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/printf.h>
#include <klib/num-gen.h>

#include <insdc/sra.h> /* for filter/types */


rc_t CC Quitting ( void );

#define INVALID_COLUMN 0xFFFFFFFF
#define DEF_FASTA_LEN 70

typedef struct fastq_ctx
{
    const char * run_name;
    const VTable * tbl;
    const VCursor * cursor;
    const struct num_gen_iter * row_iter;
    dump_format_t format;
    size_t cur_cache_size;
    uint32_t max_line_len;
    uint32_t idx_read;
    uint32_t idx_qual;
    uint32_t idx_name;
    uint32_t idx_read_start;
    uint32_t idx_read_len;
    uint32_t idx_read_type;
} fastq_ctx;

static char * vdb_fastq_extract_run_name( const char * acc_or_path )
{
    char * delim = string_rchr ( acc_or_path, string_size( acc_or_path ), '/' );
    if ( NULL == delim )
    {
        return string_dup_measure ( acc_or_path, NULL );
    }
    else
    {
        return string_dup_measure ( delim + 1, NULL );
    }
}

static void init_fastq_ctx( const p_dump_context ctx, fastq_ctx * fctx, const char * acc_or_path )
{
    fctx -> run_name = vdb_fastq_extract_run_name( acc_or_path );
    fctx -> tbl      = NULL;    
    fctx -> cursor   = NULL;
    fctx -> row_iter = NULL;
    fctx -> max_line_len = ctx -> max_line_len;
    fctx -> format   = ctx -> format;
    fctx -> cur_cache_size = ctx -> cur_cache_size;
    fctx -> idx_read = INVALID_COLUMN;
    fctx -> idx_qual = INVALID_COLUMN;
    fctx -> idx_name = INVALID_COLUMN;
    fctx -> idx_read_start  = INVALID_COLUMN;
    fctx -> idx_read_len    = INVALID_COLUMN;
    fctx -> idx_read_type   = INVALID_COLUMN;
}

static void vdb_fastq_row_error( const char * fmt, rc_t rc, int64_t row_id )
{
    PLOGERR( klogInt, ( klogInt, rc, fmt, "row_nr=%li", row_id ) );
}

static bool is_name_in_list( KNamelist * col_names, const char * to_find )
{
    bool res = false;
    uint32_t count;
    rc_t rc = KNamelistCount( col_names, &count );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc )
    {
        uint32_t i;
        size_t to_find_len = string_size( to_find );
        for ( i = 0; i < count && 0 == rc && !res; ++i )
        {
            const char * col_name;
            rc = KNamelistGet( col_names, i, &col_name );
            DISP_RC( rc, "KNamelistGet() failed" );
            if ( 0 == rc )
            {
                size_t col_name_len = string_size( col_name );
                if ( col_name_len == to_find_len )
                {
                    res = ( 0 == string_cmp( to_find, to_find_len, col_name, col_name_len, ( uint32_t )col_name_len ) );
                }
            }
        }
    }
    return res;
}

static rc_t prepare_column( fastq_ctx * fctx, KNamelist * col_names, uint32_t * col_idx,
                            const char * to_find, const char * col_spec )
{
    rc_t rc = 0;
    if ( is_name_in_list( col_names, to_find ) )
    {
        rc = VCursorAddColumn( fctx -> cursor, col_idx, col_spec );
        if ( 0 != rc )
        {
            *col_idx = INVALID_COLUMN;
            PLOGERR( klogInt, ( klogInt, rc, "VCurosrAddColumn( '$(col)' ) failed", "col=%s", col_spec ) );
        }
    }
    return rc;
}


static rc_t vdb_prepare_cursor( fastq_ctx * fctx )
{
    KNamelist * col_names;
    rc_t rc = VTableListCol( fctx -> tbl, &col_names );
    DISP_RC( rc, "VTableListCol() failed" );
    if ( 0 == rc )
    {
        rc = VTableCreateCachedCursorRead( fctx -> tbl, &fctx -> cursor, fctx -> cur_cache_size );
        DISP_RC( rc, "VTableCreateCursorRead( fasta/fastq ) failed" );
        if ( 0 == rc )
        {
            rc = prepare_column( fctx, col_names, &fctx -> idx_read, "READ", "(INSDC:dna:text)READ" );
        }
    
        if ( 0 == rc && ( df_fastq == fctx -> format || df_fastq1 == fctx -> format ) )
        {
            rc = prepare_column( fctx, col_names, &fctx -> idx_qual, "QUALITY", "(INSDC:quality:text:phred_33)QUALITY" );
        }

        if ( 0 == rc && ( df_qual == fctx -> format || df_qual1 == fctx -> format ) )
        {
            rc = prepare_column( fctx, col_names, &fctx -> idx_qual, "QUALITY", "(INSDC:quality:phred)QUALITY" );
        }

        if ( 0 == rc )
        {
            if ( df_fasta2 == fctx -> format )
            {
                rc = prepare_column( fctx, col_names, &fctx -> idx_name, "SEQ_ID", "(ascii)SEQ_ID" );
            }
            if ( 0 == rc && INVALID_COLUMN == fctx -> idx_name )
            {
                rc = prepare_column( fctx, col_names, &fctx -> idx_name, "NAME", "(ascii)NAME" );
            }
        }

        if ( 0 == rc )
        {
            rc = prepare_column( fctx, col_names, &fctx -> idx_read_start, "READ_START", "(INSDC:coord:zero)READ_START" );
        }

        if ( 0 == rc )
        {
            rc = prepare_column( fctx, col_names, &fctx -> idx_read_len, "READ_LEN", "(INSDC:coord:len)READ_LEN" );
        }

        if ( 0 == rc )
        {
            rc = prepare_column( fctx, col_names, &fctx -> idx_read_type, "READ_TYPE", "(INSDC:SRA:xread_type)READ_TYPE" );
        }

        if ( 0 == rc )
        {
            rc = VCursorOpen ( fctx -> cursor );
            DISP_RC( rc, "VCursorOpen( fasta/fastq ) failed" );
        }
        KNamelistRelease( col_names );
    }
    return rc;
}


typedef struct fastq_spot
{
    const char * name;
    const char * bases;
    const char * qual;
    const uint32_t * rd_start;
    const uint32_t * rd_len;
    const uint8_t * rd_type;
    uint32_t name_len;
    uint32_t num_bases;
    uint32_t num_qual;
    uint32_t num_rd_start;
    uint32_t num_rd_len;
    uint32_t num_rd_type;
} fastq_spot;


static rc_t read_spot( const fastq_ctx * fctx, int64_t row_id, fastq_spot * spot )
{
    rc_t rc = 0;
    uint32_t elem_bits, boff;
    if ( INVALID_COLUMN != fctx -> idx_name )
    {
        rc = VCursorCellDataDirect( fctx -> cursor, row_id, fctx -> idx_name, &elem_bits,
                                    ( const void** )&spot -> name, &boff, &spot -> name_len );
        if ( 0 != rc )
        {
            vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), NAME ) failed", rc, row_id );
        }
    }

    if ( 0 == rc && INVALID_COLUMN != fctx -> idx_read )
    {
        rc = VCursorCellDataDirect( fctx -> cursor, row_id, fctx -> idx_read, &elem_bits,
                                    ( const void** )&spot -> bases, &boff, &spot -> num_bases );
        if ( 0 != rc )
        {
            vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
        }
    }
    
    if ( 0 == rc && INVALID_COLUMN != fctx -> idx_qual )
    {
        rc = VCursorCellDataDirect( fctx -> cursor, row_id, fctx -> idx_qual, &elem_bits,
                                    ( const void** )&spot -> qual, &boff, &spot -> num_qual );
        if ( 0 != rc )
        {
            vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), QUALITY ) failed", rc, row_id );
        }
    }

    if ( 0 == rc && INVALID_COLUMN != fctx -> idx_read_start )
    {
        rc = VCursorCellDataDirect( fctx -> cursor, row_id, fctx -> idx_read_start, &elem_bits,
                                    ( const void** )&spot -> rd_start, &boff, &spot -> num_rd_start );
        if ( 0 != rc )
        {
            vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ_START ) failed", rc, row_id );
        }
    }
    
    if ( 0 == rc && INVALID_COLUMN != fctx -> idx_read_len )
    {
        rc = VCursorCellDataDirect( fctx -> cursor, row_id, fctx -> idx_read_len, &elem_bits,
                                    ( const void** )&spot -> rd_len, &boff, &spot -> num_rd_len );
        if ( 0 != rc )
        {
            vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ_LEN ) failed", rc, row_id );
        }
    }

    if ( 0 == rc && INVALID_COLUMN != fctx -> idx_read_type )
    {
        rc = VCursorCellDataDirect( fctx -> cursor, row_id, fctx -> idx_read_type, &elem_bits,
                                    ( const void** )&spot -> rd_type, &boff, &spot -> num_rd_type );
        if ( 0 != rc )
        {
            vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ_TYPE ) failed", rc, row_id );
        }
    }
    
    return rc;
}

static rc_t vdb_fastq1_frag_type_checked( fastq_spot * spot, int64_t row_id, const fastq_ctx * fctx )
{
    rc_t rc = 0;
    if ( spot -> num_bases != spot -> num_qual )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        PLOGERR( klogInt,
                 ( klogInt, rc, "invalid spot #$(row): bases.len( $(n_bases) ) != qual.len( $(n_qual)",
                    "row=%li,n_bases=%d,n_qual=%d", row_id, spot -> num_bases, spot -> num_qual ) );
    }
    else if ( spot -> num_rd_start != spot -> num_rd_len ||
               spot -> num_rd_start != spot -> num_rd_type )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        PLOGERR( klogInt,
                 ( klogInt, rc, 
                   "invalid spot #$(row): #READ_START=$(rd_start), #READ_LEN=$(rd_len), #READ_TYPE=$(rd_type)",
                   "row=%li,rd_start=%d,rd_len=%d,rd_type=%d",
                   row_id, spot -> num_rd_start, spot -> num_rd_len, spot -> num_rd_type ) );
    }
    else
    {
        uint32_t idx, frag, ofs;
        for ( idx = 0, frag = 1, ofs = 0; 0 == rc && idx < spot -> num_rd_start; ++idx )
        {
            if ( ( READ_TYPE_BIOLOGICAL == ( spot -> rd_type[ idx ] & READ_TYPE_BIOLOGICAL ) ) &&
                 spot -> rd_len[ idx ] > 0 )
            {
                rc = KOutMsg( "@%s.%li.%d %.*s length=%u\n%.*s\n+%s.%li.%d %.*s length=%u\n%.*s\n",
                              fctx -> run_name, row_id, frag, spot -> name_len, spot -> name, spot -> rd_len[ idx ],
                              spot -> rd_len[ idx ], &( spot -> bases[ ofs ] ),
                              fctx -> run_name, row_id, frag, spot -> name_len, spot -> name, spot -> rd_len[ idx ],
                              spot -> rd_len[ idx ], &( spot -> qual[ ofs ] )
                              );
                frag++;
            }
            ofs += spot -> rd_len[ idx ];
        }
    }
    return rc;
}

static rc_t vdb_fastq1_frag_not_type_checked( fastq_spot * spot, int64_t row_id, const fastq_ctx * fctx )
{
    rc_t rc = 0;
    if ( spot -> num_bases != spot -> num_qual )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        PLOGERR( klogInt,
                 ( klogInt, rc, "invalid spot #$(row): bases.len( $(n_bases) ) != qual.len( $(n_qual)",
                    "row=%li,n_bases=%d,n_qual=%d", row_id, spot -> num_bases, spot -> num_qual ) );
    }
    else if ( spot -> num_rd_start != spot -> num_rd_len )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        PLOGERR( klogInt,
                 ( klogInt, rc, 
                   "invalid spot #$(row): #READ_START=$(rd_start), #READ_LEN=$(rd_len)",
                   "row=%li,rd_start=%d,rd_len=%d",
                   row_id, spot -> num_rd_start, spot -> num_rd_len ) );
    }
    else
    {
        uint32_t idx, frag, ofs;
        for ( idx = 0, frag = 1, ofs = 0; 0 == rc && idx < spot -> num_rd_start; ++idx )
        {
            if ( spot -> rd_len[ idx ] > 0 )
            {
                rc = KOutMsg( "@%s.%li.%d %.*s length=%u\n%.*s\n+%s.%li.%d %.*s length=%u\n%.*s\n",
                              fctx -> run_name, row_id, frag, spot -> name_len, spot -> name, spot -> rd_len[ idx ],
                              spot -> rd_len[ idx ], &( spot -> bases[ ofs ] ),
                              fctx -> run_name, row_id, frag, spot -> name_len, spot -> name, spot -> rd_len[ idx ],
                              spot -> rd_len[ idx ], &( spot -> qual[ ofs ] )
                              );
                frag++;
            }
            ofs += spot -> rd_len[ idx ];
        }
    }
    return rc;
}


static rc_t vdb_fastq1_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    if ( INVALID_COLUMN == fctx -> idx_read || INVALID_COLUMN == fctx -> idx_name ||
         INVALID_COLUMN == fctx -> idx_qual || INVALID_COLUMN == fctx -> idx_read_start ||
         INVALID_COLUMN == fctx -> idx_read_len )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        DISP_RC( rc, "cannot generate fasta-format, at least one of these columns not found: READ, NAME, QUALITY, READ_START, READ_LEN" );
    }
    else
    {
        bool has_type = ( INVALID_COLUMN != fctx -> idx_read_type );
        int64_t row_id;
        while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
        {
            if ( 0 == rc )
            {
                rc = Quitting();
            }
            if ( 0 == rc )
            {
                fastq_spot spot;
                rc = read_spot( fctx, row_id, &spot );
                if ( 0 == rc )
                {
                    if ( has_type )
                    {
                        rc = vdb_fastq1_frag_type_checked( &spot, row_id, fctx );
                    }
                    else
                    {
                        rc = vdb_fastq1_frag_not_type_checked( &spot, row_id, fctx );
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t vdb_fastq_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    if ( INVALID_COLUMN == fctx -> idx_read || INVALID_COLUMN == fctx -> idx_qual )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        DISP_RC( rc, "cannot generate fasta-format: READ and/or QUALITY column not found" );
    }
    else
    {
        bool has_name = ( INVALID_COLUMN != fctx -> idx_name );
        int64_t row_id;
        while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
        {
            if ( 0 == rc )
            {
                rc = Quitting();
            }
            if ( 0 == rc )
            {
                fastq_spot spot;
                rc = read_spot( fctx, row_id, &spot );
                if ( rc == 0 )
                {
                    if ( has_name )
                    {
                        rc = KOutMsg( "@%s.%li %.*s length=%u\n%.*s\n+%s.%li %.*s length=%u\n%.*s\n",
                                    fctx -> run_name, row_id, spot . name_len, spot . name, spot . num_bases,
                                    spot . num_bases, spot . bases,
                                    fctx -> run_name, row_id, spot . name_len, spot . name, spot . num_qual,
                                    spot . num_qual, spot . qual );
                    }
                    else
                    {
                        rc = KOutMsg( "@%s.%li %li length=%u\n%.*s\n+%s.%li %li length=%u\n%.*s\n",
                                    fctx -> run_name, row_id, row_id, spot . num_bases,
                                    spot . num_bases, spot . bases,
                                    fctx -> run_name, row_id, row_id, spot . num_bases,
                                    spot . num_qual, spot . qual );
                    }
                }
            }
        }
    }
    return rc;
}

static rc_t print_bases( const char * bases, uint32_t num_bases, uint32_t max_line_len )
{
    rc_t rc;
    if ( 0 == max_line_len )
    {
        rc = KOutMsg( "%.*s\n", num_bases, bases );
    }
    else
    {
        uint32_t idx = 0, to_print = num_bases;
        rc = 0;
        while ( 0 == rc && idx < num_bases )
        {
            if ( to_print > max_line_len )
            {
                to_print = max_line_len;
            }
            rc = KOutMsg( "%.*s\n", to_print, &bases[ idx ] );
            if ( 0 == rc )
            {
                idx += to_print;
                to_print = ( num_bases - idx );
            }
        }
    }
    return rc;
}

static rc_t print_qual( const char * qual, uint32_t count, uint32_t max_line_len )
{
    rc_t rc = 0;
    uint32_t i = 0, on_line = 0;
    while ( 0 == rc && i < count )
    {
        char buffer[ 16 ];
        size_t num_writ;
        rc = string_printf( buffer, sizeof buffer, &num_writ, "%d", qual[ i ] );
        if ( 0 == rc )
        {
            if ( 0 == on_line )
            {
                rc = KOutMsg( "%s", buffer );
                on_line = ( uint32_t )num_writ;
            }
            else
            {
                if ( ( on_line + num_writ + 1 ) < max_line_len )
                {
                    rc = KOutMsg( " %s", buffer );
                    on_line += ( ( uint32_t )num_writ + 1 );
                }
                else
                {
                    rc = KOutMsg( "\n%s", buffer );
                    on_line = ( uint32_t )num_writ;
                }
            }
            i++;
        }
    }
    rc = KOutMsg( "\n" );
    return rc;
}

static rc_t vdb_fasta_frag_type_checked_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    bool has_name = ( INVALID_COLUMN != fctx -> idx_name );
    int64_t row_id;
    while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
    {
        if ( 0 == rc )
        {
            rc = Quitting();
        }
        if ( 0 == rc )
        {
            fastq_spot spot;
            rc = read_spot( fctx, row_id, &spot );
            if ( 0 == rc )
            {
                uint32_t idx, frag, ofs;
                for ( idx = 0, frag = 1, ofs = 0; 0 == rc && idx < spot.num_rd_start; ++idx )
                {
                    uint32_t frag_len = spot.rd_len[ idx ];
                    if ( frag_len > 0 &&
                         ( ( spot.rd_type[ idx ] & READ_TYPE_BIOLOGICAL ) == READ_TYPE_BIOLOGICAL ) )
                    {
                        if ( has_name )
                        {
                            rc = KOutMsg( ">%s.%li.%d %.*s length=%u\n",
                                    fctx -> run_name, row_id, frag, spot . name_len, spot . name, frag_len );
                        }
                        else
                        {
                            rc = KOutMsg( ">%s.%li.%d %li length=%u\n",
                                    fctx -> run_name, row_id, frag, row_id, frag_len );
                        }
                        if ( 0 == rc )
                        {
                            rc = print_bases( &( spot.bases[ ofs ] ), frag_len, fctx -> max_line_len );
                        }
                        frag++;
                    }
                    ofs += frag_len;
                }
            }
        }
    }
    return rc;
}

static rc_t vdb_fasta_frag_no_type_check_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    bool has_name = ( INVALID_COLUMN != fctx -> idx_name );
    int64_t row_id;
    while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
    {
        if ( 0 == rc )
        {
            rc = Quitting();
        }
        if ( 0 == rc )
        {
            fastq_spot spot;
            rc = read_spot( fctx, row_id, &spot );
            if ( 0 == rc )
            {
                uint32_t idx, frag, ofs;
                for ( idx = 0, frag = 1, ofs = 0; 0 == rc && idx < spot.num_rd_start; ++idx )
                {
                    uint32_t frag_len = spot.rd_len[ idx ];
                    if ( frag_len > 0 )
                    {
                        if ( has_name )
                        {
                            rc = KOutMsg( ">%s.%li.%d %.*s length=%u\n",
                                    fctx -> run_name, row_id, frag, spot . name_len, spot . name, frag_len );
                        }
                        else
                        {
                            rc = KOutMsg( ">%s.%li.%d %li length=%u\n",
                                    fctx -> run_name, row_id, frag, row_id, frag_len );
                        }
                        if ( 0 == rc )
                        {
                            rc = print_bases( &( spot.bases[ ofs ] ), frag_len, fctx -> max_line_len );
                        }
                        frag++;
                    }
                    ofs += frag_len;
                }
            }
        }
    }
    return rc;
}

static rc_t vdb_fasta_spot_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    bool has_name = ( INVALID_COLUMN != fctx -> idx_name );
    int64_t row_id;
    while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
    {
        if ( 0 == rc )
        {
            rc = Quitting();
        }
        if ( 0 == rc )
        {
            fastq_spot spot;
            rc = read_spot( fctx, row_id, &spot );
            if ( 0 == rc )
            {
                if ( has_name )
                {
                    rc = KOutMsg( ">%s.%li %.*s length=%u\n",
                            fctx -> run_name, row_id, spot . name_len, spot . name, spot . num_bases );
                }
                else
                {
                    rc = KOutMsg( ">%s.%li %li length=%u\n", fctx -> run_name, row_id, row_id, spot . num_bases );
                }   
                if ( 0 == rc )
                {
                    rc = print_bases( spot.bases, spot.num_bases, fctx -> max_line_len );
                }
            }
        }
    }
    return rc;
}

static rc_t vdb_fasta_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    if ( INVALID_COLUMN == fctx -> idx_read )
    {
        /* we actually only need a READ-column, everything else name/splitting etc. is optional... */
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        DISP_RC( rc, "cannot generate fasta-format: READ column not found" );
    }
    else
    {
        bool can_split = ( INVALID_COLUMN != fctx -> idx_read_start && INVALID_COLUMN != fctx -> idx_read_len );
        if ( can_split )
        {
            bool has_type = ( INVALID_COLUMN != fctx -> idx_read_type );
            if ( has_type )
            {
                rc = vdb_fasta_frag_type_checked_loop( fctx );
            }
            else
            {
                rc = vdb_fasta_frag_no_type_check_loop( fctx );
            }
        }
        else
        {
            rc = vdb_fasta_spot_loop( fctx );
        }
    }
    return rc;
}

/* -------------------------------------------------------------------------------------------------------------- */

static rc_t vdb_qual_frag_type_checked_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    bool has_name = ( INVALID_COLUMN != fctx -> idx_name );
    int64_t row_id;
    while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
    {
        if ( 0 == rc )
        {
            rc = Quitting();
        }
        if ( 0 == rc )
        {
            fastq_spot spot;
            rc = read_spot( fctx, row_id, &spot );
            if ( 0 == rc )
            {
                uint32_t idx, frag, ofs;
                for ( idx = 0, frag = 1, ofs = 0; 0 == rc && idx < spot.num_rd_start; ++idx )
                {
                    uint32_t frag_len = spot.rd_len[ idx ];
                    if ( frag_len > 0 &&
                         ( READ_TYPE_BIOLOGICAL == ( spot.rd_type[ idx ] & READ_TYPE_BIOLOGICAL ) ) )
                    {
                        if ( has_name )
                        {
                            rc = KOutMsg( ">%s.%li.%d %.*s length=%u\n",
                                    fctx -> run_name, row_id, frag, spot . name_len, spot . name, frag_len );
                        }
                        else
                        {
                            rc = KOutMsg( ">%s.%li.%d %li length=%u\n",
                                    fctx -> run_name, row_id, frag, row_id, frag_len );
                        }
                        if ( 0 == rc )
                            rc = print_qual( &( spot . qual[ ofs ] ), frag_len, fctx -> max_line_len );

                        frag++;
                    }
                    ofs += frag_len;
                }
            }
        }
    }
    return rc;
}

static rc_t vdb_qual_frag_no_type_check_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    bool has_name = ( INVALID_COLUMN != fctx -> idx_name );
    int64_t row_id;
    while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
    {
        if ( 0 == rc )
        {
            rc = Quitting();
        }
        if ( 0 == rc )
        {
            fastq_spot spot;
            rc = read_spot( fctx, row_id, &spot );
            if ( 0 == rc )
            {
                uint32_t idx, frag, ofs;
                for ( idx = 0, frag = 1, ofs = 0; 0 == rc && idx < spot.num_rd_start; ++idx )
                {
                    uint32_t frag_len = spot.rd_len[ idx ];
                    if ( frag_len > 0 )
                    {
                        if ( has_name )
                        {
                            rc = KOutMsg( ">%s.%li.%d %.*s length=%u\n",
                                    fctx -> run_name, row_id, frag, spot . name_len, spot . name, frag_len );
                        }
                        else
                        {
                            rc = KOutMsg( ">%s.%li.%d %li length=%u\n",
                                    fctx -> run_name, row_id, frag, row_id, frag_len );
                        }
                        if ( 0 == rc )
                        {
                            rc = print_qual( &( spot.qual[ ofs ] ), frag_len, fctx -> max_line_len );
                        }
                        frag++;
                    }
                    ofs += frag_len;
                }
            }
        }
    }
    return rc;
}

static rc_t vdb_qual_spot_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    bool has_name = ( INVALID_COLUMN != fctx -> idx_name );
    int64_t row_id;
    while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
    {
        if ( 0 == rc )
        {
            rc = Quitting();
        }
        if ( 0 == rc )
        {
            fastq_spot spot;
            rc = read_spot( fctx, row_id, &spot );
            if ( 0 == rc )
            {
                if ( has_name )
                {
                    rc = KOutMsg( ">%s.%li %.*s length=%u\n",
                            fctx -> run_name, row_id, spot . name_len, spot . name, spot . num_qual );
                }
                else
                {
                    rc = KOutMsg( ">%s.%li %li length=%u\n",
                            fctx -> run_name, row_id, row_id, spot . num_qual );
                }   
                if ( 0 == rc )
                {
                    rc = print_qual( spot.qual, spot.num_qual, fctx -> max_line_len );
                }
            }
        }
    }
    return rc;
}

static rc_t vdb_qual_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    if ( INVALID_COLUMN == fctx -> idx_qual )
    {
        /* we actually only need a QUAL-column, everything else name/splitting etc. is optional... */
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        DISP_RC( rc, "cannot generate fasta-format: READ column not found" );
    }
    else
    {
        bool can_split = ( INVALID_COLUMN != fctx -> idx_read_start && INVALID_COLUMN != fctx -> idx_read_len );
        if ( can_split )
        {
            bool has_type = ( INVALID_COLUMN != fctx -> idx_read_type );
            if ( has_type )
            {
                rc = vdb_qual_frag_type_checked_loop( fctx );
            }
            else
            {
                rc = vdb_qual_frag_no_type_check_loop( fctx );
            }
        }
        else
        {
            rc = vdb_qual_spot_loop( fctx );
        }
    }
    return rc;
}

/* -------------------------------------------------------------------------------------------------------------- */

static rc_t vdb_fasta_accumulated( const char * bases, uint32_t num_bases, 
                                   int32_t * chars_left_on_line, uint32_t max_line_len )
{
    rc_t rc = 0;
    if ( num_bases < (uint32_t)( *chars_left_on_line ) )
    {
        rc = KOutMsg( "%.*s", num_bases, bases );
        ( *chars_left_on_line ) -= num_bases;
    }
    else if ( num_bases == ( *chars_left_on_line ) )
    {
        rc = KOutMsg( "%.*s\n", num_bases, bases );
        ( *chars_left_on_line ) = max_line_len;
    }
    else
    {
        uint32_t ofs = 0;
        int32_t remaining = num_bases;
        while( 0 == rc && ofs < num_bases )
        {
            if ( remaining >= ( *chars_left_on_line ) )
            {
                rc = KOutMsg( "%.*s\n", ( *chars_left_on_line ), &bases[ ofs ] );
                ofs += ( *chars_left_on_line );
                remaining -= ( *chars_left_on_line );
                ( *chars_left_on_line ) = max_line_len;
            }
            else
            {
                rc = KOutMsg( "%.*s", remaining, &bases[ ofs ] );
                ofs += remaining;
                ( *chars_left_on_line ) -= remaining;
                remaining = 0;
            }
        }
    }
    return rc;
}

static rc_t vdb_fasta1_loop( const fastq_ctx * fctx )
{
    rc_t rc;
    if ( INVALID_COLUMN == fctx -> idx_read )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        DISP_RC( rc, "cannot generate fasta1-format: READ column not found" );
    }
    else
    {
        int64_t row_id;
        int32_t chars_left_on_line = fctx -> max_line_len;
        
        rc = KOutMsg( ">%s\n", fctx -> run_name );
        while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
        {
            if ( 0 == rc )
            {
                rc = Quitting();
            }
            if ( 0 == rc )
            {
                fastq_spot spot;
                rc = read_spot( fctx, row_id, &spot );
                if ( 0 == rc )
                {
                    rc = vdb_fasta_accumulated( spot.bases, spot.num_bases, &chars_left_on_line, fctx -> max_line_len );
                }
            }
        }
        rc = KOutMsg( "\n" );
    }
    return rc;
}

static rc_t vdb_fasta2_loop( const fastq_ctx * fctx )
{
    rc_t rc = 0;
    if ( INVALID_COLUMN == fctx -> idx_name || INVALID_COLUMN == fctx -> idx_read )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
        DISP_RC( rc, "cannot generate fasta2-format: READ and/or NAME column not found" );
    }
    else
    {
        char last_name[ 1024 ];
        size_t last_name_len = 0;
        int64_t row_id;
        int32_t chars_left_on_line = fctx -> max_line_len;
        
        while ( 0 == rc && num_gen_iterator_next( fctx -> row_iter, &row_id, &rc ) )
        {
            if ( 0 == rc )
            {
                rc = Quitting();
            }
            if ( 0 == rc )
            {
                fastq_spot spot;
                rc = read_spot( fctx, row_id, &spot );
                if ( 0 == rc )
                {
                    bool print_ref_name = ( 0 == last_name_len );
                    if ( !print_ref_name )
                    {
                        print_ref_name = ( last_name_len != spot . name_len );
                        if ( !print_ref_name )
                        {
                            print_ref_name = ( 0 != string_cmp( last_name, last_name_len,
                                    spot . name, spot . name_len, spot . name_len ) );
                        }
                    }
                    
                    if ( print_ref_name )
                    {
                        if ( chars_left_on_line == fctx -> max_line_len )
                        {
                            rc = KOutMsg( ">%.*s\n", spot . name_len, spot . name );
                        }
                        else
                        {
                            rc = KOutMsg( "\n>%.*s\n", spot . name_len, spot . name );
                            chars_left_on_line = fctx -> max_line_len;
                        }
                        last_name_len = string_copy ( last_name, sizeof last_name, spot . name, spot . name_len );
                    }
                    
                    if ( 0 == rc )
                    {
                        rc = vdb_fasta_accumulated( spot.bases, spot.num_bases, &chars_left_on_line, fctx -> max_line_len );
                    }
                }
            }
        }
        rc = KOutMsg( "\n" );
    }
    return rc;
}

static rc_t vdb_fastq_tbl( const p_dump_context ctx, fastq_ctx * fctx )
{
    rc_t rc = vdb_prepare_cursor( fctx );
    DISP_RC( rc, "the table lacks READ and/or QUALITY column" );
    if ( 0 == rc )
    {
        int64_t  first;
        uint64_t count;
        /* READ is the colum we have in all cases... */
        rc = VCursorIdRange( fctx -> cursor, fctx -> idx_read, &first, &count );
        DISP_RC( rc, "VCursorIdRange() failed" );
        if ( 0 == rc )
        {
            if ( 0 == count )
            {
                KOutMsg( "this table is empty\n" );
            }
            else
            {
                /* if the user did not specify a row-range, take all rows */
                if ( NULL == ctx -> rows )
                {
                    rc = num_gen_make_from_range( &ctx -> rows, first, count );
                    DISP_RC( rc, "num_gen_make_from_range() failed" );
                }
                /* if the user did specify a row-range, check the boundaries */
                else
                {
                    rc = num_gen_trim( ctx -> rows, first, count );
                    DISP_RC( rc, "num_gen_trim() failed" );
                }

                if ( 0 == rc && !num_gen_empty( ctx -> rows ) )
                {
                    rc = num_gen_iterator_make( ctx -> rows, &fctx -> row_iter );
                    DISP_RC( rc, "num_gen_iterator_make() failed" );
                    if ( 0 == rc )
                    {
                        if ( 0 == fctx -> max_line_len )
                        {
                            fctx -> max_line_len = DEF_FASTA_LEN;
                        }    
                        switch( fctx -> format )
                        {
                            /* one FASTQ-record ( 4 liner ) per READ/SPOT */
                            case df_fastq : rc = vdb_fastq_loop( fctx ); /* <--- */
                                             break;

                            /* one FASTQ-record ( 4 liner ) per FRAGMENT/ALIGNMENT */
                            case df_fastq1 : rc = vdb_fastq1_loop( fctx ); /* <--- */
                                              break;

                            /* one FASTA-record ( 2 liner ) per READ/SPOT */
                            case df_fasta :  rc = vdb_fasta_loop( fctx ); /* <--- */
                                             break;

                             /* one FASTA-record ( many lines ) for the whole accession ( REFSEQ-accession )  */
                            case df_fasta1 : rc = vdb_fasta1_loop( fctx ); /* <--- */
                                             break;

                             /* one FASTA-record ( many lines ) for each REFERENCE used in a cSRA-database  */
                            case df_fasta2 : rc = vdb_fasta2_loop( fctx ); /* <--- */
                                             break;

                            /* one QUAL-record ( 2 liner ) per whole READ/SPOT */
                            case df_qual :  rc = vdb_qual_spot_loop( fctx ); /* <--- */
                                             break;

                            /* one QUAL-record ( 2 liner ) per FRAGMENT/ALIGNMENTT */
                            case df_qual1 :  rc = vdb_qual_loop( fctx ); /* <--- */
                                             break;

                            default : break;
                        }
                        num_gen_iterator_destroy( fctx -> row_iter );
                    }
                }
                else
                {
                    rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                }
            }
        }
        {
            rc_t rc2 = VCursorRelease( fctx -> cursor );
            DISP_RC( rc2, "VCursorRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}


static rc_t vdb_fastq_table( const p_dump_context ctx,
                             const VDBManager *mgr,
                             fastq_ctx * fctx )
{
    VSchema * schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &( ctx -> schema_list ), true /* ctx -> force_sra_schema */ );

    rc = VDBManagerOpenTableRead( mgr, &fctx -> tbl, schema, "%s", ctx -> path );
    DISP_RC( rc, "VDBManagerOpenTableRead() failed" );
    if ( 0 == rc )
    {
        rc = vdb_fastq_tbl( ctx, fctx );
        {
            rc_t rc2 = VTableRelease( fctx -> tbl );
            DISP_RC( rc2, "VTableRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    
    if ( NULL != schema )
    {
        rc_t rc2 = VSchemaRelease( schema );
        DISP_RC( rc2, "VSchemaRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}


static rc_t vdb_fastq_database( const p_dump_context ctx,
                                const VDBManager *mgr,
                                fastq_ctx * fctx )
{
    const VDatabase * db;
    VSchema *schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &( ctx -> schema_list ), true /* ctx -> force_sra_schema */ );

    rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", ctx -> path );
    DISP_RC( rc, "VDBManagerOpenDBRead() failed" );
    if ( 0 == rc )
    {
        KNamelist *tbl_names;
        rc = VDatabaseListTbl( db, &tbl_names );
        if ( rc != 0 )
        {
            ErrMsg( "VDatabaseListTbl( '%s' )  ->  %R", ctx -> path, rc );
        }
        else
        {
            if ( NULL == ctx -> table )
            {
                /* the user DID NOT not specify a table: by default assume the SEQUENCE-table */
                bool table_found = vdh_take_this_table_from_list( ctx, tbl_names, "SEQUENCE" );
                /* if there is no SEQUENCE-table, just pick the first table available... */
                if ( !table_found )
                {
                    vdh_take_1st_table_from_db( ctx, tbl_names );
                }
            }
            else
            {
                /* the user DID specify a table: check if the database has a table with this name,
                   if not try with a sub-string */
                String value;
                StringInitCString( &value, ctx -> table );
                if ( !list_contains_value( tbl_names, &value ) )
                {
                    vdh_take_this_table_from_list( ctx, tbl_names, ctx -> table );
                }
            }
            rc = KNamelistRelease( tbl_names );
            if ( 0 != rc )
            {
                ErrMsg( "KNamelistRelease()  ->  %R", rc );
            }
        }
        if ( 0 == rc )
        {
            rc = open_table_by_path( db, ctx -> table, &fctx -> tbl ); /* vdb-dump-tools.c */
            if ( 0 == rc )
            {
                rc = vdb_fastq_tbl( ctx, fctx );
                {
                    rc_t rc2 = VTableRelease( fctx -> tbl );
                    DISP_RC( rc2, "VTableRelease() failed" );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VDatabaseRelease( db );
            DISP_RC( rc2, "VDatabaseRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }

    if ( NULL != schema )
    {
        rc_t rc2 = VSchemaRelease( schema );
        DISP_RC( rc2, "VSchemaRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t vdb_fastq_by_pathtype( const p_dump_context ctx,
                                   const VDBManager *mgr,
                                   fastq_ctx * fctx )                                   
{
    rc_t rc;
    int path_type = ( VDBManagerPathType ( mgr, "%s", ctx -> path ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type )
    {
    case kptDatabase    :  rc = vdb_fastq_database( ctx, mgr, fctx );
                            DISP_RC( rc, "dump_database() failed" );
                            break;

    case kptPrereleaseTbl:
    case kptTable       :  rc = vdb_fastq_table( ctx, mgr, fctx );
                            DISP_RC( rc, "dump_table() failed" );
                            break;

    default             :  rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
                            PLOGERR( klogInt, ( klogInt, rc,
                                "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                                "p=%s", ctx -> path ) );
                            if ( 0 == vdco_schema_count( ctx ) )
                            {
                                LOGERR( klogInt, rc, "Maybe it is a legacy table. If so, specify a schema with the -S option" );
                            }
                            break;
    }
    return rc;
}


static rc_t vdb_fastq_by_probing( const p_dump_context ctx,
                                  const VDBManager *mgr,
                                  fastq_ctx * fctx )
{
    rc_t rc;
    if ( vdh_is_path_database( mgr, ctx -> path, &( ctx -> schema_list ) ) )
    {
        rc = vdb_fastq_database( ctx, mgr, fctx );
        DISP_RC( rc, "dump_database() failed" );
    }
    else if ( vdh_is_path_table( mgr, ctx -> path, &( ctx -> schema_list ) ) )
    {
        rc = vdb_fastq_table( ctx, mgr, fctx );
        DISP_RC( rc, "dump_table() failed" );
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        PLOGERR( klogInt, ( klogInt, rc,
                 "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                 "p=%s", ctx -> path ) );
    }
    return rc;
}

rc_t vdf_main( const p_dump_context ctx, const VDBManager * mgr, const char * acc_or_path )
{
    rc_t rc = 0;
    fastq_ctx fctx;
    init_fastq_ctx( ctx, &fctx, acc_or_path );
    ctx -> path = string_dup_measure ( acc_or_path, NULL );
    
    if ( USE_PATHTYPE_TO_DETECT_DB_OR_TAB ) /* in vdb-dump-context.h */
    {
        rc = vdb_fastq_by_pathtype( ctx, mgr, &fctx );
    }
    else
    {
        rc = vdb_fastq_by_probing( ctx, mgr, &fctx );
    }
    free( ( char* )ctx -> path );
    free( ( void* )fctx.run_name );
    ctx -> path = NULL;

    return rc;
}

/* ---------------------------------------------------------------------------------------------------- */

#define NUM_COUNTERS 1024

static rc_t vdf_len_spread_loop( const p_dump_context ctx,
                                const VCursor * curs,
                                bool has_read_len, bool has_ref_len,
                                uint32_t read_len_idx, uint32_t ref_len_idx,
                                const char * path )
{
    const struct num_gen_iter * row_iter;
    rc_t rc = num_gen_iterator_make( ctx -> rows, &row_iter );
    DISP_RC( rc, "num_gen_iterator_make() failed" );
    if ( rc == 0 )
    {
        int64_t row_id;
        uint64_t read_len_counters[ NUM_COUNTERS ];
        uint64_t ref_len_counters[ NUM_COUNTERS ];
        uint32_t idx;


        for ( idx = 0; idx < NUM_COUNTERS; ++idx )
        {
            read_len_counters[ idx ] = 0;
            ref_len_counters[ idx ] = 0;
        }

        while ( 0 == rc && num_gen_iterator_next( row_iter, &row_id, &rc ) )
        {
            if ( 0 == rc )
            {
                rc = Quitting();
            }
            if ( 0 == rc )
            {
                uint32_t elem_bits, boff, row_len;
                uint32_t * ptr;
                if ( has_read_len )
                {
                    rc = VCursorCellDataDirect( curs, row_id, read_len_idx, &elem_bits, (const void**)&ptr, &boff, &row_len );
                    if ( 0 == rc && row_len > 0 )
                    {
                        if ( *ptr < NUM_COUNTERS )
                        {
                            read_len_counters[ *ptr ]++;
                        }
                        else
                        {
                            read_len_counters[ NUM_COUNTERS - 1 ]++;
                        }
                    }
                }
                if ( has_ref_len )
                {
                    rc = VCursorCellDataDirect( curs, row_id, ref_len_idx, &elem_bits, (const void**)&ptr, &boff, &row_len );
                    if ( 0 == rc && row_len > 0 )
                    {
                        if ( *ptr < NUM_COUNTERS )
                        {
                            ref_len_counters[ *ptr ]++;
                        }
                        else
                        {
                            ref_len_counters[ NUM_COUNTERS - 1 ]++;
                        }
                    }
                }
            }
        }

        num_gen_iterator_destroy( row_iter );
        
        for ( idx = 0; 0 == rc && idx < NUM_COUNTERS; ++idx )
        {
            if ( read_len_counters[ idx ] > 0 )
            {
                rc = KOutMsg( "READ_LEN[ %d ] = %,lu\n", idx, read_len_counters[ idx ] );
            }
        }
        for ( idx = 0; 0 == rc && idx < NUM_COUNTERS; ++idx )
        {
            if ( ref_len_counters[ idx ] > 0 )
            {
                rc = KOutMsg( "REF_LEN[ %d ] = %,lu\n", idx, ref_len_counters[ idx ] );
            }
        }
    }
    return rc;
}

static rc_t vdf_len_spread_vdbtbl( const p_dump_context ctx, const VTable * tbl, const char * path )
{
    KNamelist * col_names;
    rc_t rc = VTableListCol( tbl, &col_names );
    DISP_RC( rc, "VTableListCol() failed" );
    if ( 0 == rc )
    {
        const VCursor * curs;
        rc = VTableCreateCachedCursorRead( tbl, &curs, 1024 * 1024 * 32 );
        DISP_RC( rc, "VTableCreateCursorRead( fasta/fastq ) failed" );
        if ( 0 == rc )
        {
            bool has_read_len = is_name_in_list( col_names, "READ_LEN" );
            bool has_ref_len = is_name_in_list( col_names, "REF_LEN" );
            if ( has_read_len || has_ref_len )
            {
                uint32_t read_len_idx, ref_len_idx;
                if ( has_read_len )
                {
                    rc = VCursorAddColumn( curs, &read_len_idx, "READ_LEN" );
                    if ( 0 != rc )
                    {
                        PLOGERR( klogInt, ( klogInt, rc, "VCurosrAddColumn( '$(col)' ) failed", "col=%s", "READ_LEN" ) );
                    }
                }
                if ( 0 == rc && has_ref_len )
                {
                    rc = VCursorAddColumn( curs, &ref_len_idx, "REF_LEN" );
                    if ( 0 != rc )
                    {
                        PLOGERR( klogInt, ( klogInt, rc, "VCurosrAddColumn( '$(col)' ) failed", "col=%s", "REF_LEN" ) );
                    }
                }
                if ( 0 == rc )
                {
                    rc = VCursorOpen( curs );
                    DISP_RC( rc, "VCursorOpen( len-spread ) failed" );
                }
                if ( 0 == rc )
                {
                    int64_t  first;
                    uint64_t count;
                    
                    if ( has_read_len )
                    {
                        rc = VCursorIdRange( curs, read_len_idx, &first, &count );
                    }
                    else
                    {
                        rc = VCursorIdRange( curs, ref_len_idx, &first, &count );
                    }
                    DISP_RC( rc, "VCursorIdRange() failed" );
                    if ( 0 == rc )
                    {
                        if ( 0 == count )
                        {
                            rc = KOutMsg( "this table is empty\n" );
                        }
                        else
                        {
                            /* if the user did not specify a row-range, take all rows */
                            if ( NULL == ctx -> rows )
                            {
                                rc = num_gen_make_from_range( &ctx -> rows, first, count );
                                DISP_RC( rc, "num_gen_make_from_range() failed" );
                            }
                            /* if the user did specify a row-range, check the boundaries */
                            else
                            {
                                rc = num_gen_trim( ctx -> rows, first, count );
                                DISP_RC( rc, "num_gen_trim() failed" );
                            }
                        }
                    }
                }
                if ( 0 == rc && !num_gen_empty( ctx -> rows ) )
                {
                    rc = vdf_len_spread_loop( ctx, curs, has_read_len, has_ref_len, read_len_idx, ref_len_idx, path ); /* <=== the meat */
                }
            }
            else
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
                PLOGERR( klogInt, ( klogInt, rc,
                        "neither READ_LEN nor REF_LEN found in '$(p)'", "p=%s", path ) );
            }
            {
                rc_t rc2 = VCursorRelease( curs );
                DISP_RC( rc2, "VCursorRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        {
            rc_t rc2 = KNamelistRelease( col_names );
            DISP_RC( rc2, "KNamelistRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}


static rc_t vdf_len_spread_db( const p_dump_context ctx, const VDBManager * mgr, const char * path )
{
    const VDatabase * db;
    VSchema *schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &(ctx -> schema_list), true /* ctx -> force_sra_schema */ );

    rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", path );
    DISP_RC( rc, "VDBManagerOpenDBRead() failed" );
    if ( 0 == rc )
    {
        KNamelist *tbl_names;
        rc = VDatabaseListTbl( db, &tbl_names );
        if ( 0 != rc )
        {
            ErrMsg( "VDatabaseListTbl( '%s' )  ->  %R", path, rc );
        }
        else
        {
            if ( ctx -> table == NULL )
            {
                /* the user DID NOT not specify a table: by default assume the SEQUENCE-table */
                bool table_found = vdh_take_this_table_from_list( ctx, tbl_names, "SEQUENCE" );
                /* if there is no SEQUENCE-table, just pick the first table available... */
                if ( !table_found )
                {
                    vdh_take_1st_table_from_db( ctx, tbl_names );
                }
            }
            else
            {
                /* the user DID specify a table: check if the database has a table with this name,
                   if not try with a sub-string */
                String value;
                StringInitCString( &value, ctx -> table );
                if ( !list_contains_value( tbl_names, &value ) )
                {
                    vdh_take_this_table_from_list( ctx, tbl_names, ctx -> table );
                }
            }
            rc = KNamelistRelease( tbl_names );
            if ( 0 != rc )
            {
                ErrMsg( "KNamelistRelease()  ->  %R", rc );
            }
        }

        if ( rc == 0 )
        {
            const VTable * tbl;
            rc = open_table_by_path( db, ctx -> table, &tbl ); /* vdb-dump-tools.c */
            if ( 0 == rc )
            {
                rc = vdf_len_spread_vdbtbl( ctx, tbl, path );
                {
                    rc_t rc2 = VTableRelease( tbl );
                    DISP_RC( rc2, "VTableRelease() failed" );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VDatabaseRelease( db );
            DISP_RC( rc2, "VDatabaseRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    if ( schema != NULL )
    {
        rc_t rc2 = VSchemaRelease( schema );
        DISP_RC( rc2, "VSchemaRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t vdf_len_spread_tbl( const p_dump_context ctx, const VDBManager * mgr, const char * path )
{
    VSchema * schema = NULL;
    const VTable * tbl;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &(ctx -> schema_list), true /* ctx -> force_sra_schema */ );

    rc = VDBManagerOpenTableRead( mgr, &tbl, schema, "%s", path );
    DISP_RC( rc, "VDBManagerOpenTableRead() failed" );
    if ( 0 == rc )
    {
        rc = vdf_len_spread_vdbtbl( ctx, tbl, path );
        {
            rc_t rc2 = VTableRelease( tbl );
            DISP_RC( rc2, "VTableRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    if ( schema != NULL )
    {
        rc_t rc2 = VSchemaRelease( schema );
        DISP_RC( rc2, "VSchemaRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t vdf_len_spread_by_pathtype( const p_dump_context ctx, const VDBManager * mgr, const char * path )
{
    rc_t rc;
    int path_type = ( VDBManagerPathType ( mgr, "%s", path ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type )
    {
        case kptDatabase    :  rc = vdf_len_spread_db( ctx, mgr, path ); break;

        case kptPrereleaseTbl:
        case kptTable       :  rc = vdf_len_spread_tbl( ctx, mgr, path ); break;

        default             :  rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
                                PLOGERR( klogInt, ( klogInt, rc,
                                        "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                                        "p=%s", ctx -> path ) );
                                break;
    }
    return rc;

}

static rc_t vdf_len_spread_by_probing( const p_dump_context ctx, const VDBManager * mgr, const char * path )
{
    rc_t rc;
    if ( vdh_is_path_database( mgr, path, &(ctx -> schema_list) ) )
    {
        rc = vdf_len_spread_db( ctx, mgr, path );
    }
    else if ( vdh_is_path_table( mgr, path, &(ctx -> schema_list) ) )
    {
        rc = vdf_len_spread_tbl( ctx, mgr, path );
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        PLOGERR( klogInt, ( klogInt, rc,
                 "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                 "p=%s", path ) );
    }
    return rc;
}

rc_t vdf_len_spread( const p_dump_context ctx, const VDBManager * mgr, const char * path )
{
    rc_t rc = 0;
    if ( USE_PATHTYPE_TO_DETECT_DB_OR_TAB ) /* in vdb-dump-context.h */
    {
        rc = vdf_len_spread_by_pathtype( ctx, mgr, path );
    }
    else
    {
        rc = vdf_len_spread_by_probing( ctx, mgr, path );
    }
    return rc;
}
