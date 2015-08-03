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

#include <kapp/args.h>

#include <insdc/insdc.h>

#include <vdb/table.h>
#include <vdb/cursor.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/buffile.h>

#include <klib/text.h>
#include <klib/printf.h>
#include <klib/num-gen.h>

#include "vdb-dump-context.h"
#include "vdb-dump-coldefs.h"

#include <os-native.h>
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>

rc_t Quitting( void );

static rc_t write_to_file( KFile * f, uint64_t * pos, const void * src, uint32_t len, const char * name )
{
    size_t num_writ;
    size_t num_to_write = len;
    rc_t rc = KFileWriteAll( f, *pos, src, num_to_write, &num_writ );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc,
                 "failed to write to column $(col_name) at #%(pos)", "col_name=%s,pos=%ld", name, *pos ) );
    }
    else
        *pos += num_writ;
    return rc;
}


static rc_t read_from_file( const KFile * f, uint64_t pos, const uint8_t * dst,
                            size_t dst_size, size_t * num_read, const char * name )
{
    rc_t rc = KFileReadAll ( f, pos, ( void * )dst, dst_size, num_read );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc,
                 "failed to read from column $(col_name) at #%(pos)", "col_name=%s,pos=%ld", name, pos ) );
    }
    return rc;
}


typedef struct wr_bin_idx
{
    /* the file-handles */
    KFile * bin;
    KFile * idx;
    const char name[ 256 ];
    uint64_t bin_pos;
    uint64_t idx_pos;
    uint32_t first_len;
    bool multi_value;
} wr_bin_idx;


static void release_wr_bin_idx( wr_bin_idx * c )
{
    if ( c != NULL )
    {
        if ( c->bin != NULL )
            KFileRelease( c->bin );
        if ( c->idx != NULL )
            KFileRelease( c->idx );
    }
}


static rc_t create_wr_bin_idx( KDirectory * dir, const char * col_name, wr_bin_idx * c )
{
    rc_t rc;

    c->bin = NULL;
    c->idx = NULL;
    rc = KDirectoryCreateFile ( dir, &c->bin, false, 0664, kcmInit, "COL_%s.bin", col_name );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc,
                 "failed to create bin-file for column $(col_name)",
                 "col_name=%s", col_name ) );
    }
    else
    {
        rc = KDirectoryCreateFile ( dir, &c->idx, false, 0664, kcmInit, "COL_%s.idx", col_name );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc,
                     "failed to create index-file for column $(col_name)",
                     "col_name=%s", col_name ) );
        }
    }

    if ( rc == 0 )
    {
        KFile * f = c->bin;
        rc = KBufWriteFileMakeWrite ( &c->bin, f, 1024 * 1024 * 16 );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc,
                     "failed to create buffer for bin-file for column $(col_name)",
                     "col_name=%s", col_name ) );
        }
    }

    if ( rc == 0 )
    {
        string_copy( ( char * )c->name, sizeof c->name,
                     col_name, string_len( col_name, string_size( col_name ) ) );
        c->bin_pos = 0;
        c->idx_pos = 0;
        c->first_len = 0xFFFFFFFF;
        c->multi_value = false;
    }

    if ( rc != 0 )
        release_wr_bin_idx( c );

    return rc;
}


static rc_t write_bin_idx( wr_bin_idx * c, const void * data, uint32_t len )
{
    /* first write to index-file the position where the data will be written to */
    rc_t rc = write_to_file( c->idx, &c->idx_pos, &c->bin_pos, sizeof c->bin_pos, c->name );

    /* the write the length, it can be zero! */
    if ( rc == 0 )
        rc = write_to_file( c->idx, &c->idx_pos, &len, sizeof len, c->name );

    /* the write the data, only if we have data! */
    if ( rc == 0 && len > 0 )
        rc = write_to_file( c->bin, &c->bin_pos, data, len, c->name );

    if ( rc == 0 )
    {
        if ( c->first_len == 0xFFFFFFFF )
            c->first_len = len;
        else if( c->first_len != len )
            c->multi_value = true;
    }

    return rc;
}

/*
static rc_t set_bin_filesize( wr_bin_idx * c, uint64_t new_size )
{
    rc_t rc = KFileSetSize( c->bin, new_size );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc,
                 "failed to set size of file to >%(fsize) for column $(col_name)", "fsize=%ld,col_name=%s", new_size, c->name ) );
    }
    return rc;
}

static rc_t write_bin_at( wr_bin_idx * c, uint64_t pos, const void * data, uint32_t len )
{
    uint64_t pos1 = pos;
    rc_t rc = write_to_file( c->bin, &pos1, data, len, c->name );

    if ( rc == 0 && c->first_len == 0xFFFFFFFF )
        c->first_len = len;

    return rc;
}
*/

static rc_t truncate_idx( wr_bin_idx * c )
{
    uint64_t pos = 0;
    rc_t rc = write_to_file( c->idx, &pos, &c->first_len, sizeof c->first_len, c->name );
    if ( rc == 0 )
    {
        rc = KFileSetSize ( c->idx, pos );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc,
                     "failed to truncate the index-file for column $(col_name)",
                     "col_name=%s", c->name ) );
        }
    }
    return rc;
}


static rc_t vdi_create_dir( const char * path, KDirectory ** dir )
{
    rc_t rc = KDirectoryNativeDir ( dir );
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
    }
    else
    {
        if ( path != NULL )
        {
            rc = KDirectoryCreateDir ( *dir, 0775, kcmOpen, "%s", path );
            if ( rc != 0 )
            {
                PLOGERR( klogInt, ( klogInt, rc,
                         "failed to create directory $(dir_name)", "dir_name=%s", path ) );
                KDirectoryRelease ( *dir );
            }
            else
            {
                KDirectory * tmp = *dir;
                rc = KDirectoryOpenDirUpdate ( tmp, dir, false, "%s", path );
                if ( rc != 0 )
                {
                    PLOGERR( klogInt, ( klogInt, rc,
                             "failed to open directory $(dir_name)", "dir_name=%s", path ) );
                }
                KDirectoryRelease( tmp );
            }
        }
    }
    return rc;
}


static rc_t vdi_dump_column_rows( const char * path, const VCursor *cur, p_col_def col, struct num_gen * rows )
{
    KDirectory * dir;

    rc_t rc = vdi_create_dir( path, &dir );
    if ( rc == 0 )
    {
        wr_bin_idx wr;

        rc = create_wr_bin_idx( dir, col->name, &wr );
        if ( rc == 0 )
        {
            const struct num_gen_iter * iter;
            rc = num_gen_iterator_make( rows, &iter );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "VCursorIdRange() failed" );
            else
            {
                int64_t row_id;
                while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) )
                {
                    rc = Quitting();
                    if ( rc == 0 )
                    {
                        const void * base;
                        uint32_t elem_bits, boff, row_len;
                        rc = VCursorCellDataDirect ( cur, row_id, col->idx,
                                                     &elem_bits, &base, &boff, &row_len );
                        if ( rc != 0 )
                        {
                            PLOGERR( klogInt, ( klogInt, rc,
                                     "VCursorCellData( col:$(col_name) at row #$(row_nr) ) failed",
                                     "col_name=%s,row_nr=%ld", col->name, row_id ) );
                        }
                        else
                        {
                            uint32_t len = ( elem_bits >> 3 ) * row_len;
                            rc = write_bin_idx( &wr, base, len );
                        }
                    }
                }
                num_gen_iterator_destroy( iter );
            }

            if ( rc == 0 && !wr.multi_value )
                rc = truncate_idx( &wr );

            release_wr_bin_idx( &wr );
        }
        KDirectoryRelease ( dir );
    }
    return rc;
}


static rc_t vdi_dump_column( const p_dump_context ctx, const VCursor *cur, p_col_def col )
{
    int64_t  first;
    uint64_t count;

    rc_t rc = VCursorIdRange( cur, col->idx, &first, &count );
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "VCursorIdRange() failed" );
    }
    else if ( count > 0 )
    {
        struct num_gen * rows = NULL;


        if ( ctx->row_range == NULL )
        {
            /* the user did not give us a row-range, we take all rows of this column... */
            rc = num_gen_make_from_range( &rows, first, count );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "num_gen_make_from_range() failed" );
        }
        else
        {
            /* the gave us a row-range, we parse that string and check agains the real row-count... */
            rc = num_gen_make_from_str( &rows, ctx->row_range );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "num_gen_make_from_str() failed" );
            else
            {
                rc = num_gen_trim( rows, first, count );
                if ( rc != 0 )
                    LOGERR( klogInt, rc, "num_gen_trim() failed" );
            }
        }

        if ( rc == 0 )
        {
            if ( num_gen_empty( rows ) )
            {
                rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                LOGERR( klogInt, rc, "no row-range(s) defined" );
            }
            else
            {
                rc = vdi_dump_column_rows( ctx->output_path, cur, col, rows ); /* <---- */
            }
        }

        if ( rows != NULL )
            num_gen_destroy( rows );
    }
    return rc;
}


static rc_t vdi_dump_columns( const p_dump_context ctx, const VCursor *cur, p_col_defs col_defs )
{
    rc_t rc = 0;
    const Vector * v = &( col_defs->cols );
    uint32_t start = VectorStart( v );
    uint32_t end = start + VectorLength( v );
    uint32_t i;

    for ( i = start; rc == 0 && i < end; ++i )
    {
        p_col_def col = VectorGet ( v, i );
        if ( col != NULL )
            rc = vdi_dump_column( ctx, cur, col ); /* <---- */
    }
    return rc;
}


static uint32_t vdi_extract_or_parse_columns( const VTable * tab,
                                              p_col_defs col_defs,
                                              const char * columns,
                                              const char * excluded_columns )
{
    uint32_t count = 0;
    if ( col_defs != NULL )
    {
        bool cols_unknown = ( ( columns == NULL ) || ( string_cmp( columns, 1, "*", 1, 1 ) == 0 ) );
        if ( cols_unknown )
            /* the user does not know the column-names or wants all of them */
            count = vdcd_extract_from_table( col_defs, tab );
        else
            /* the user knows the names of the wanted columns... */
            count = vdcd_parse_string( col_defs, columns, tab );

        if ( excluded_columns != NULL )
            vdcd_exclude_these_columns( col_defs, excluded_columns );
    }
    return count;
}


rc_t vdi_dump_opened_table( const p_dump_context ctx, const VTable * tab )
{
    rc_t rc = 0;

    col_defs * col_defs;

    if ( !vdcd_init( &col_defs, ctx->max_line_len ) )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        LOGERR( klogInt, rc, "col_defs_init() failed" );
    }
    else
    {
        uint32_t n = vdi_extract_or_parse_columns( tab, col_defs, ctx->columns, ctx->excluded_columns );
        if ( n < 1 )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            LOGERR( klogInt, rc, "vdm_extract_or_parse_columns() failed" );
        }
        else
        {
            const VCursor * cur;

            rc = VTableCreateCachedCursorRead( tab, &cur, ctx->cur_cache_size );
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "VTableCreateCachedCursorRead() failed" );
            }
            else
            {
                n = vdcd_add_to_cursor( col_defs, cur );
                if ( n < 1 )
                {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                    LOGERR( klogInt, rc, "vdcd_add_to_cursor() failed" );
                }
                else
                {
                    rc = VCursorOpen( cur );
                    if ( rc != 0 )
                    {
                        LOGERR( klogInt, rc, "VCursorOpen() failed" );
                    }
                    else
                        rc = vdi_dump_columns( ctx, cur, col_defs );    /* <---- */
                }
            }
            VCursorRelease( cur );
        }
        vdcd_destroy( col_defs );
    }

    return rc;
}


typedef struct rd_bin_idx
{
    /* the file-handles */
    const KFile * bin;
    const KFile * idx;
    const char name[ 256 ];
    uint64_t bin_file_size;
    uint64_t idx_file_size;
    uint64_t row_count;
    uint32_t common_len;
} rd_bin_idx;


static void release_rd_bin_idx( rd_bin_idx * c )
{
    if ( c != NULL )
    {
        if ( c->bin != NULL )
            KFileRelease( c->bin );
        if ( c->idx != NULL )
            KFileRelease( c->idx );
    }
}


static rc_t create_rd_bin_idx( const KDirectory * dir, const char * col_name, rd_bin_idx * c )
{
    rc_t rc;

    rc = KDirectoryOpenFileRead( dir, &c->bin, "COL_%s.bin", col_name );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc,
                 "failed to open bin-file for column $(col_name)", "col_name=%s", col_name ) );
    }
    else
    {
        rc = KDirectoryOpenFileRead( dir, &c->idx, "COL_%s.idx", col_name );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc,
                     "failed to open idx-file for column $(col_name)", "col_name=%s", col_name ) );
        }
        else
        {
            string_copy( ( char * )c->name, sizeof c->name,
                         col_name, string_len( col_name, string_size( col_name ) ) );
            c->bin_file_size = 0;
            c->idx_file_size = 0;
            c->row_count = 0;
            c->common_len = 0;

            rc = KFileSize ( c->bin, &c->bin_file_size );
            if ( rc != 0 )
            {
                PLOGERR( klogInt, ( klogInt, rc,
                         "failed to get filesize of bin-file for $(col_name)", "col_name=%s", col_name ) );
            }
            else
            {
                rc = KFileSize ( c->idx, &c->idx_file_size );
                if ( rc != 0 )
                {
                    PLOGERR( klogInt, ( klogInt, rc,
                             "failed to get filesize of idx-file for $(col_name)", "col_name=%s", col_name ) );
                }
                else
                {
                    /* calculate row-count */
                    if ( c->idx_file_size == sizeof c->common_len )
                    {
                        size_t num_read;
                        /* read the common length out of index-file at pos 0, divide bin_file_size by common length */
                        rc = read_from_file( c->idx, 0, (void *)&c->common_len, sizeof c->common_len, &num_read, col_name );
                        if ( rc == 0 && c->common_len > 0 )
                            c->row_count = ( c->bin_file_size / c->common_len );
                    }
                    else
                    {
                        /* divide the idx_file_size by 12 ( 8 bytes offset + 4 bytes length per row ) */
                        c->row_count = ( c->idx_file_size / 12 );
                    }
                }
            }
        }

        if ( rc != 0 )
            release_rd_bin_idx( c );
    }
    return rc;
}


static rc_t rd_bin_idx_row_len( rd_bin_idx * c, uint64_t row_id, uint32_t * row_len )
{
    rc_t rc = 0;
    size_t num_read;

    if ( c->common_len > 0 )
        *row_len = c->common_len;
    else
        rc = read_from_file( c->idx, ( row_id * 12 ) + 8, (void *)row_len, sizeof *row_len, &num_read, c->name );

    return rc;
}


static rc_t rd_bin_idx_row_pos( rd_bin_idx * c, uint64_t row_id, uint64_t * row_pos )
{
    rc_t rc = 0;
    size_t num_read;

    if ( c->common_len > 0 )
        *row_pos = ( c->common_len * row_id );
    else
        rc = read_from_file( c->idx, row_id * 12, (void *)row_pos, sizeof *row_pos, &num_read, c->name );

    return rc;
}


static rc_t rd_bin_idx_64( rd_bin_idx * c, uint64_t row_id, uint64_t * values, uint32_t value_count )
{
    uint64_t pos;
    size_t num_read;

    rc_t rc = rd_bin_idx_row_pos( c, row_id, &pos );
    if ( rc == 0 )
        rc = read_from_file( c->bin, pos, (void *)values,
                             ( sizeof *values ) * value_count, &num_read, c->name );

    return rc;
}


static rc_t rd_bin_idx_32( rd_bin_idx * c, uint64_t row_id, uint32_t * values, uint32_t value_count )
{
    uint64_t pos;
    size_t num_read;

    rc_t rc = rd_bin_idx_row_pos( c, row_id, &pos );
    if ( rc == 0 )
        rc = read_from_file( c->bin, pos, (void *)values,
                             ( sizeof *values ) * value_count, &num_read, c->name );

    return rc;
}


static rc_t rd_bin_idx_8( rd_bin_idx * c, uint64_t row_id, uint8_t * values, uint32_t value_count )
{
    uint64_t pos;
    size_t num_read;

    rc_t rc = rd_bin_idx_row_pos( c, row_id, &pos );
    if ( rc == 0 )
        rc = read_from_file( c->bin, pos, (void *)values,
                             ( sizeof *values ) * value_count, &num_read, c->name );

    return rc;
}


static rc_t rd_bin_idx_char( rd_bin_idx * c, uint64_t row_id, uint32_t offset, uint32_t len,
                             char * dst, size_t dst_size, size_t * num_read )
{
    uint64_t pos;
    uint32_t row_len;

    rc_t rc = rd_bin_idx_row_pos( c, row_id, &pos );

    if ( rc == 0 )
    {
        pos += offset;
        if ( len > 0 )
            row_len = len;
        else
            rc = rd_bin_idx_row_len( c, row_id, &row_len );
    }

    if ( rc == 0 )
    {
        if ( row_len >= dst_size )
            row_len = dst_size - 1;
        rc = read_from_file( c->bin, pos, (void *)dst, row_len, num_read, c->name );
    }
    return rc;
}


/* -----------------------------------------------------------------------------------------------------------
 phase 1

 input:     COL_PRIMARY_ALIGNMENT_ID.[bin] from SEQUENCE
            COL_REF_POS.[bin] from PRIMARY_ALIGNMENT
            COL_REF_ID.[bin] from PRIMARY_ALIGNMENT
            COL_REF_ORIENTATION.[bin] from PRIMARY_ALIGNMENT
            COL_SEQ_READ_ID.[bin] from PRIMARY_ALIGNMENT

 output:    TMP_MATE_REF_POS.[bin] ... the reference position of the mate
            TMP_MATE_REF_ID.[bin]  ... the reference id ( idx to name ) of the mate
            TMP_MATE_REF_ORIENTATION.[bin] ... the orientation of the mate

 */
typedef struct p1_ctx
{
    /* the input files */

    /* originated from PRIMARY_ALIGNMENT-table */
    rd_bin_idx SEQ_SPOT_ID;
    rd_bin_idx MAPQ;
    rd_bin_idx REF_POS;
    rd_bin_idx REF_LEN;
    rd_bin_idx CIGAR;
    rd_bin_idx READ;
    rd_bin_idx REF_ID;
    rd_bin_idx REF_ORIENTATION;

    /* originated from SEQUENCE-table */
    rd_bin_idx PRIMARY_ALIGNMENT_ID;
    rd_bin_idx QUALITY;
    rd_bin_idx READ_LEN;
    rd_bin_idx READ_START;
    rd_bin_idx READ_FILTER;

    /* originated from REFERENCE-table */
    rd_bin_idx NAME;
} p1_ctx;


static void release_p1_ctx( p1_ctx * p1_ctx )
{
    release_rd_bin_idx( &p1_ctx->SEQ_SPOT_ID );
    release_rd_bin_idx( &p1_ctx->MAPQ );
    release_rd_bin_idx( &p1_ctx->REF_POS );
    release_rd_bin_idx( &p1_ctx->REF_LEN );
    release_rd_bin_idx( &p1_ctx->CIGAR );
    release_rd_bin_idx( &p1_ctx->READ );
    release_rd_bin_idx( &p1_ctx->REF_ID );
    release_rd_bin_idx( &p1_ctx->REF_ORIENTATION );

    release_rd_bin_idx( &p1_ctx->PRIMARY_ALIGNMENT_ID );
    release_rd_bin_idx( &p1_ctx->QUALITY );
    release_rd_bin_idx( &p1_ctx->READ_LEN );
    release_rd_bin_idx( &p1_ctx->READ_START );
    release_rd_bin_idx( &p1_ctx->READ_FILTER );

    release_rd_bin_idx( &p1_ctx->NAME );
}


static rc_t init_p1_ctx( KDirectory * dir, p1_ctx * p1_ctx )
{
    rc_t rc;

    memset( p1_ctx, 0, sizeof * p1_ctx );

    rc = create_rd_bin_idx( dir, "SEQ_SPOT_ID", &p1_ctx->SEQ_SPOT_ID );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "MAPQ", &p1_ctx->MAPQ );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "REF_POS", &p1_ctx->REF_POS );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "REF_LEN", &p1_ctx->REF_LEN );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "CIGAR_SHORT", &p1_ctx->CIGAR );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "READ", &p1_ctx->READ );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "REF_ID", &p1_ctx->REF_ID );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "REF_ORIENTATION", &p1_ctx->REF_ORIENTATION );

    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "PRIMARY_ALIGNMENT_ID", &p1_ctx->PRIMARY_ALIGNMENT_ID );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "QUALITY", &p1_ctx->QUALITY );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "READ_LEN", &p1_ctx->READ_LEN );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "READ_START", &p1_ctx->READ_START );
    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "READ_FILTER", &p1_ctx->READ_FILTER );

    if ( rc == 0 )
        rc = create_rd_bin_idx( dir, "NAME", &p1_ctx->NAME );


    if ( rc != 0 )
        release_p1_ctx( p1_ctx );
    return rc;
}


static uint32_t vdi_calc_flag( bool each_fragment_aligned,
                               bool this_fragment_not_aligned,
                               bool mate_not_aligned,
                               bool this_fragment_reversed,
                               bool mate_reversed,
                               bool this_fragment_is_first,
                               bool this_fragment_is_last,
                               bool this_fragment_is_secondary,
                               bool this_fragment_not_passing_quality_control,
                               bool this_fragment_is_pcr_or_duplicate )
{
    uint32_t res = 0x001;
    if ( each_fragment_aligned ) res |= 0x002;
    if ( this_fragment_not_aligned ) res |= 0x004;
    if ( mate_not_aligned ) res |= 0x008;
    if ( this_fragment_reversed ) res |= 0x010;
    if ( mate_reversed ) res |= 0x020;
    if ( this_fragment_is_first ) res |= 0x040;
    if ( this_fragment_is_last ) res |= 0x080;
    if ( this_fragment_is_secondary ) res |= 0x100;
    if ( this_fragment_not_passing_quality_control ) res |= 0x200;
    if ( this_fragment_is_pcr_or_duplicate ) res |= 0x400;
    return res;
}


static int32_t vdi_calc_tlen( const uint32_t self_ref_pos,
                              const uint32_t mate_ref_pos,
                              const uint32_t self_ref_len,
                              const uint32_t mate_ref_len,
                              const bool on_same_ref,
                              const bool first_read )
{
    int32_t res = 0;
    if ( on_same_ref && ( self_ref_pos > 0 ) && ( mate_ref_pos > 0 ) )
    {
        const uint32_t self_right = self_ref_pos + self_ref_len;
        const uint32_t mate_right = mate_ref_pos + mate_ref_len;
        const uint32_t leftmost   = ( self_ref_pos < mate_ref_pos ) ? self_ref_pos : mate_ref_pos;
        const uint32_t rightmost  = ( self_right > mate_right ) ? self_right : mate_right;
        const uint32_t tlen = rightmost - leftmost;

        /* The standard says, "The leftmost segment has a plus sign and the rightmost has a minus sign." */
        if ( ( self_ref_pos <= mate_ref_pos && self_right >= mate_right ) || /* mate fully contained within self or */
             ( mate_ref_pos <= self_ref_pos && mate_right >= self_right ) )  /* self fully contained within mate; */
        {
            if ( self_ref_pos < mate_ref_pos || ( first_read && self_ref_pos == mate_ref_pos ) )
                res = tlen;
            else
                res = -( ( int32_t )tlen );
        }
        else if ( ( self_right == mate_right && mate_ref_pos == leftmost ) || /* both are rightmost, but mate is leftmost */
                   ( self_right == rightmost ) )
        {
            res = -( ( int32_t )tlen );
        }
        else
            res = tlen;
    }
    return res;
}


static rc_t vdi_get_SEQ_SPOT_ID( p1_ctx * p1_ctx, uint64_t alignment_id, uint64_t * dst )
{
    rc_t rc = rd_bin_idx_64( &p1_ctx->SEQ_SPOT_ID, alignment_id, dst, 1 );
    return rc;
}


static rc_t vdi_get_MATE_ID( p1_ctx * p1_ctx, uint64_t seq_spot_id_1_based, uint64_t alignment_id_1_based,
                            uint64_t * dst, bool * first )
{
    uint64_t AL_IDS[ 2 ];
    rc_t rc = rd_bin_idx_64( &p1_ctx->PRIMARY_ALIGNMENT_ID, seq_spot_id_1_based - 1, AL_IDS, 2 );
    if ( rc == 0 )
    {
        if ( AL_IDS[ 0 ] == alignment_id_1_based )
        {
            *dst = AL_IDS[ 1 ];
            *first = true;
        }
        else if ( AL_IDS[ 1 ] == alignment_id_1_based )
        {
            *dst = AL_IDS[ 0 ];
            *first = false;
        }
        else
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            PLOGERR( klogInt, ( klogInt, rc,
                     "given ALIGNMENT_ID #$(al_id) not found in SEQUENCE-ID #$(seq_id)",
                     "al_id=%lu,seq_id=%lu", alignment_id_1_based, seq_spot_id_1_based ) );
        }
    }
    return rc;
}


static rc_t vdi_get_REF_ID( p1_ctx * p1_ctx, uint64_t alignment_id_zero_based, uint64_t * dst )
{
    rc_t rc = rd_bin_idx_64( &p1_ctx->REF_ID, alignment_id_zero_based, dst, 1 );
    return rc;
}


static rc_t vdi_get_REF_POS( p1_ctx * p1_ctx, uint64_t alignment_id, uint32_t * dst )
{
    rc_t rc = rd_bin_idx_32( &p1_ctx->REF_POS, alignment_id, dst, 1 );
    if ( rc == 0 ) *dst += 1;
    return rc;
}


static rc_t vdi_get_REF_LEN( p1_ctx * p1_ctx, uint64_t alignment_id, uint32_t * dst )
{
    rc_t rc = rd_bin_idx_32( &p1_ctx->REF_LEN, alignment_id, dst, 1 );
    return rc;
}


static rc_t vdi_get_REF_ORIENTATION( p1_ctx * p1_ctx, uint64_t alignment_id, bool * dst )
{
    uint8_t orientation;
    rc_t rc = rd_bin_idx_8( &p1_ctx->REF_ORIENTATION, alignment_id, &orientation, 1 );
    if ( rc == 0 ) *dst = ( orientation != 0 );
    return rc;
}


static rc_t vdi_generate_QNAME( p1_ctx * p1_ctx, uint64_t id, char * dst, size_t dst_size )
{
    rc_t rc = string_printf ( dst, dst_size, NULL, "%lu", id );
    return rc;
}


static rc_t vdi_generate_RNAME( p1_ctx * p1_ctx, uint64_t ref_id_1_based, char * dst, size_t dst_size )
{
    size_t num_read;
    rc_t rc = rd_bin_idx_char( &p1_ctx->NAME, ref_id_1_based - 1, 0, 0, dst, dst_size, &num_read );
    if ( rc == 0 && num_read < dst_size )
        dst[ num_read ] = 0;
    return rc;
}


static rc_t vdi_generate_MAPQ( p1_ctx * p1_ctx, uint64_t alignment_id, uint32_t * dst )
{
    rc_t rc = rd_bin_idx_32( &p1_ctx->MAPQ, alignment_id, dst, 1 );
    return rc;
}


static rc_t vdi_generate_CIGAR( p1_ctx * p1_ctx, uint64_t alignment_id, char * dst, size_t dst_size )
{
    size_t num_read;
    rc_t rc = rd_bin_idx_char( &p1_ctx->CIGAR, alignment_id, 0, 0, dst, dst_size, &num_read );
    if ( rc == 0 && num_read < dst_size )
        dst[ num_read ] = 0;
    return rc;
}


static rc_t vdi_generate_SEQ( p1_ctx * p1_ctx, uint64_t alignment_id, char * dst, size_t dst_size )
{
    size_t num_read;
    rc_t rc = rd_bin_idx_char( &p1_ctx->READ, alignment_id, 0, 0, dst, dst_size, &num_read );
    if ( rc == 0 && num_read < dst_size )
        dst[ num_read ] = 0;
    return rc;
}


static rc_t vdi_generate_FRAG( p1_ctx * p1_ctx, uint64_t seq_spot_id_1_based, bool first,
                               uint32_t * frag_start, uint32_t * frag_len, uint8_t * filter )
{
    uint32_t rd_start[ 2 ];
    rc_t rc = rd_bin_idx_32( &p1_ctx->READ_START, seq_spot_id_1_based - 1, rd_start, 2 );
    if ( rc == 0 )
    {
        uint32_t rd_len[ 2 ];
        rc = rd_bin_idx_32( &p1_ctx->READ_LEN, seq_spot_id_1_based - 1, rd_len, 2 );
        if ( rc == 0 )
        {
            uint8_t rd_filter[ 2 ];
            rc = rd_bin_idx_8( &p1_ctx->READ_FILTER, seq_spot_id_1_based - 1, rd_filter, 2 );
            if ( rc == 0 )
            {
                uint32_t idx = first ? 0 : 1;
                *frag_start = rd_start[ idx ];
                *frag_len = rd_len[ idx ];
                *filter = rd_filter[ idx ];
            }
        }
    }
    return rc;
}


static rc_t vdi_generate_QUAL( p1_ctx * p1_ctx, uint64_t seq_spot_id_1_based,
                               uint32_t start, uint32_t len, char * dst, size_t dst_size )
{
    /* how many entries in READ_START and READ_LEN do we have? ( size is 32 bit ) */
    size_t num_read;
    rc_t rc = rd_bin_idx_char( &p1_ctx->QUALITY, seq_spot_id_1_based - 1, start, len, dst, dst_size, &num_read );
    if ( rc == 0 && num_read < dst_size )
    {
        uint32_t i;
        for ( i = 0; i < num_read; ++i )
            dst[ i ] += 33; 
        dst[ num_read ] = 0;
    }
    return rc;
}


static void reverse_buffer( char * dst, const char * src, size_t len )
{
    size_t i, j;
    for ( i = 0, j = len - 1; i < len; ++i, --j )
        dst[ i ] = src[ j ];
}


typedef struct alignment
{
    char REF_NAME[ 256 ];
    uint64_t id_one_based;
    uint64_t ref_id_one_based;
    uint32_t REF_POS_one_based;
    uint32_t REF_LEN;
    bool aligned;
    bool first;
    bool reversed;
} alignment;


/* ----------------------------------------------------------------------------------------------------------- */
static rc_t vdi_bin_phase_1_row( const p_dump_context ctx, p1_ctx * p1_ctx, int64_t row_id )
{
    rc_t rc;

    alignment self, mate;

    char QNAME[ 256 ];
    char CIGAR[ 512 ];
    char SEQ[ 2048 ];
    char QUAL[ 2048 ];
    char QUALR[ 2048 ];

    uint64_t SEQ_SPOT_ID_1_based;
    uint32_t FLAG, MAPQ;
    int32_t TLEN;
    uint32_t frag_start, frag_len;
    uint8_t filter;
    bool on_same_reference = false;

    /* first we have to collect a lot of helper values... */
    memset( &self, 0, sizeof self );
    memset( &mate, 0, sizeof mate );

    /* -------------------------------------------------------------------------------------------------------- */
    self.id_one_based = (  row_id + 1 );
    self.aligned = true;

    /* get the row-id (1-based) of the sequence this alignment belongs to */
    rc = vdi_get_SEQ_SPOT_ID( p1_ctx, row_id, &SEQ_SPOT_ID_1_based );
    
    /* get the row-id (1-based) of the mate of this alignment ( 0...not mated )*/
    if ( rc == 0 )
        rc = vdi_get_MATE_ID( p1_ctx, SEQ_SPOT_ID_1_based, self.id_one_based, &mate.id_one_based, &self.first );
    if ( rc == 0 )
    {
        mate.first = !self.first;
        mate.aligned = ( mate.id_one_based > 0 );
    }

    /* get the row-id of the Reference this alignment belongs to */
    if ( rc == 0 )
        rc = vdi_get_REF_ID( p1_ctx, row_id, &self.ref_id_one_based );

    /* get the row-id of the Reference the mate belongs to */
    if ( rc == 0 && mate.aligned )
        rc = vdi_get_REF_ID( p1_ctx, mate.id_one_based - 1, &mate.ref_id_one_based );

    if ( rc == 0 && mate.aligned )
        on_same_reference = ( self.ref_id_one_based == mate.ref_id_one_based );

    /* get the position on the reference ( 0 based ) of this alignment */
    if ( rc == 0 )
        rc = vdi_get_REF_POS( p1_ctx, row_id, &self.REF_POS_one_based );

    /* get the position on the reference ( 0 based ) of the mate */
    if ( rc == 0 && mate.aligned )
        rc = vdi_get_REF_POS( p1_ctx, mate.id_one_based - 1, &mate.REF_POS_one_based );

    /* get the length of the alignment on the reference */
    if ( rc == 0 )
        rc = vdi_get_REF_LEN( p1_ctx, self.id_one_based - 1, &self.REF_LEN );

    /* get the length of the mate on the reference */
    if ( rc == 0 && mate.aligned )
        rc = vdi_get_REF_LEN( p1_ctx, mate.id_one_based - 1, &mate.REF_LEN );

    /* get the reference-name of this alignment */
    if ( rc == 0 )
        rc = vdi_generate_RNAME( p1_ctx, self.ref_id_one_based, self.REF_NAME, sizeof self.REF_NAME );
    
    /* get the reference-name of the mate */
    if ( rc == 0 && mate.aligned && !on_same_reference )
        rc = vdi_generate_RNAME( p1_ctx, mate.ref_id_one_based, mate.REF_NAME, sizeof mate.REF_NAME );

    if ( rc == 0 )
        rc = vdi_get_REF_ORIENTATION( p1_ctx, self.id_one_based - 1, &self.reversed );

    if ( rc == 0 && mate.aligned )
        rc = vdi_get_REF_ORIENTATION( p1_ctx, mate.id_one_based - 1, &mate.reversed );

    /* if the mate is aligned and the ref-ids dont match, compare the strings to find out if they are
       on the same reference */
    if ( rc == 0 && mate.aligned && !on_same_reference )
    {
        size_t l1 = string_size( self.REF_NAME );
        size_t l2 = string_size( mate.REF_NAME );
        if ( l1 == l2 )
        {
            int diff = string_cmp ( self.REF_NAME, l1, mate.REF_NAME, l2, l1 );
            on_same_reference = ( diff == 0 );
        }
    }

    TLEN = vdi_calc_tlen( self.REF_POS_one_based, mate.REF_POS_one_based,
                          self.REF_LEN, mate.REF_LEN, on_same_reference, self.first );

    /* -------------------------------------------------------------------------------------------------------- */
    if ( rc == 0 )
        rc = vdi_generate_QNAME( p1_ctx, SEQ_SPOT_ID_1_based, QNAME, sizeof QNAME );

    if ( rc == 0 )
        rc = vdi_generate_MAPQ( p1_ctx, row_id, &MAPQ );

    if ( rc == 0 )
        rc = vdi_generate_CIGAR( p1_ctx, row_id, CIGAR, sizeof CIGAR );

    if ( rc == 0 )
        rc = vdi_generate_SEQ( p1_ctx, row_id, SEQ, sizeof SEQ );

    if ( rc == 0 )
        rc = vdi_generate_FRAG( p1_ctx, SEQ_SPOT_ID_1_based, self.first,
                                &frag_start, &frag_len, &filter );

    if ( rc == 0 )
        rc = vdi_generate_QUAL( p1_ctx, SEQ_SPOT_ID_1_based, frag_start, frag_len, QUAL, sizeof QUAL );

    if ( rc == 0 && self.reversed )
        reverse_buffer( QUALR, QUAL, string_size( QUAL ) );
        
    if ( rc == 0 )
    {
        bool each_fragment_aligned = ( self.aligned && mate.aligned );
        bool this_fragment_not_aligned = false;
        bool mate_not_aligned = !mate.aligned;
        bool this_fragment_reversed = self.reversed;
        bool mate_reversed = mate.reversed;
        bool this_fragment_is_first = self.first;
        bool this_fragment_is_last = !self.first;
        bool this_fragment_is_secondary = false;
        bool this_fragment_not_passing_quality_control = ( ( filter & READ_FILTER_REJECT ) > 0 );
        bool this_fragment_is_pcr_or_duplicate = ( ( filter & READ_FILTER_CRITERIA ) > 0 );

        FLAG = vdi_calc_flag( each_fragment_aligned,
                              this_fragment_not_aligned,
                              mate_not_aligned,
                              this_fragment_reversed,
                              mate_reversed,
                              this_fragment_is_first,
                              this_fragment_is_last,
                              this_fragment_is_secondary,
                              this_fragment_not_passing_quality_control,
                              this_fragment_is_pcr_or_duplicate );
    }

    if ( rc == 0 )
        rc = KOutMsg( "%s\t%u\t%s\t%u\t%d\t%s\t",
                      QNAME, FLAG, self.REF_NAME, self.REF_POS_one_based, MAPQ, CIGAR );

    if ( rc == 0 )
    {
        if ( mate.aligned )
        {
            if ( on_same_reference )
                rc = KOutMsg( "=\t%u\t%d\t", mate.REF_POS_one_based, TLEN );
            else
                rc = KOutMsg( "%s\t%u\t%d\t", mate.REF_NAME, mate.REF_POS_one_based, TLEN );
        }
        else
            rc = KOutMsg( "*\t0\t0\t" );
    }

    if ( rc == 0 )
    {
        if ( self.reversed )
            rc = KOutMsg( "%s\t%s\n", SEQ, QUALR );
        else
            rc = KOutMsg( "%s\t%s\n", SEQ, QUAL );
    }
    return rc;
}

static rc_t vdi_bin_phase_1( KDirectory * dir, const p_dump_context ctx )
{
    p1_ctx p1_ctx;
    rc_t rc = init_p1_ctx( dir, &p1_ctx );
    if ( rc == 0 )
    {
        struct num_gen * rows = NULL;
        uint64_t row_count = p1_ctx.REF_POS.row_count;

        if ( ctx->row_range == NULL )
        {
            /* the user did not give us a row-range, we take all rows of this column... */
            rc = num_gen_make_from_range( &rows, 0, row_count );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "num_gen_make_from_range() failed" );
        }
        else
        {
            /* the gave us a row-range, we parse that string and check agains the real row-count... */
            rc = num_gen_make_from_str( &rows, ctx->row_range );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "num_gen_make_from_str() failed" );
            else
            {
                rc = num_gen_trim( rows, 0, row_count );
                if ( rc != 0 )
                    LOGERR( klogInt, rc, "num_gen_trim() failed" );
            }
        }

        if ( rc == 0 && num_gen_empty( rows ) )
        {
            rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
            LOGERR( klogInt, rc, "no row-range(s) defined" );
        }

        if ( rc == 0 )
        {
            const struct num_gen_iter * iter;
            rc = num_gen_iterator_make( rows, &iter );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "num_gen_iterator_make() failed" );
            else
            {
                int64_t row_id;
                while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) )
                {
                    rc = Quitting();
                    if ( rc == 0 )
                        rc = vdi_bin_phase_1_row( ctx, &p1_ctx, row_id );
                }
                num_gen_iterator_destroy( iter );
            }
        }

        if ( rows != NULL )
            num_gen_destroy( rows );

        release_p1_ctx( &p1_ctx );
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------------------- */


/* ----------------------------------------------------------------------------------------------------------- */

rc_t vdi_bin_phase( const p_dump_context ctx, Args * args )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "VCursorOpen() failed" );
    }
    else if ( count < 1 )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        LOGERR( klogInt, rc, "parameter missing ( path to bin-files )" );
    }
    else
    {
        const char *bin_path = NULL;
        rc = ArgsParamValue( args, 0, &bin_path );
        if ( rc != 0 )
        {
            LOGERR( klogInt, rc, "ArgsParamValue() failed" );
        }
        else
        {
            KDirectory *dir;
            rc = vdi_create_dir( bin_path, &dir );
            if ( rc == 0 )
            {
                switch( ctx->phase )
                {
                    case 1  : vdi_bin_phase_1( dir, ctx ); break;
                    default : KOutMsg( "phase %d unknown\n", ctx->phase );
                }
                KDirectoryRelease( dir );
            }
        }
    }
    return rc;
}
