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

#include <stdlib.h>

#include <kdb/manager.h>
#include <vdb/vdb-priv.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/num-gen.h>

rc_t CC Quitting ( void );

#define INVALID_COLUMN 0xFFFFFFFF
#define DEF_FASTA_LEN 70

typedef struct fastq_ctx
{
    const char * run_name;
    const VCursor * cursor;
    uint32_t idx_read;
    uint32_t idx_qual;
    uint32_t idx_name;    
} fastq_ctx;


static void vdb_fastq_row_error( const char * fmt, rc_t rc, int64_t row_id )
{
    PLOGERR( klogInt, ( klogInt, rc, fmt, "row_nr=%li", row_id ) );
}


static rc_t vdb_fastq_loop_with_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
    const struct num_gen_iter * iter;
    rc_t rc = num_gen_iterator_make( ctx->rows, &iter );
    DISP_RC( rc, "num_gen_iterator_make() failed" );
    if ( rc == 0 )
    {
        int64_t row_id;
        while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) )
        {
            if ( rc == 0 )
                rc = Quitting();
            if ( rc == 0 )
            {
                uint32_t elem_bits, boff, row_len, name_len;
                const char * data;
                const char * name;

                rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_name, &elem_bits,
                                            (const void**)&name, &boff, &name_len );
                if ( rc != 0 )
                    vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), NAME ) failed", rc, row_id );
                else
                {
                    rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
                                                (const void**)&data, &boff, &row_len );
                    if ( rc != 0 )
                        vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
                    else
                    {
                        rc = KOutMsg( "@%s.%li %.*s length=%u\n%.*s\n",
                                      fctx->run_name, row_id, name_len, name, row_len, row_len, data );
                        if ( rc == 0 )
                        {
                            rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_qual, &elem_bits,
                                                        (const void**)&data, &boff, &row_len );
                            if ( rc != 0 )
                                vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), QUALITY ) failed", rc, row_id );
                            else
                                rc = KOutMsg( "+%s.%li %.*s length=%u\n%.*s\n",
                                              fctx->run_name, row_id, name_len, name, row_len, row_len, data );
                        }
                    }
                }
            }
        }
        num_gen_iterator_destroy( iter );
    }
    return rc;
}


static rc_t vdb_fasta_loop_with_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
    const struct num_gen_iter * iter;
    rc_t rc = num_gen_iterator_make( ctx->rows, &iter );
    DISP_RC( rc, "num_gen_iterator_make() failed" );
    if ( rc == 0 )
    {
        int64_t row_id;
        while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) )
        {
            if ( rc == 0 )
                rc = Quitting();
            if ( rc == 0 )
            {
                uint32_t elem_bits, boff, row_len, name_len;
                const char * data;
                const char * name;

                rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_name, &elem_bits,
                                            (const void**)&name, &boff, &name_len );
                if ( rc != 0 )
                    vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), NAME ) failed", rc, row_id );
                else
                {
                    rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
                                                (const void**)&data, &boff, &row_len );
                    if ( rc != 0 )
                        vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
                    else
                    {
                        uint32_t idx = 0;
                        int32_t to_print = row_len;

                        rc = KOutMsg( ">%s.%li %.*s length=%u\n",
                                      fctx->run_name, row_id, name_len, name, row_len );
                        if ( to_print > ctx->max_line_len )
                            to_print = ctx->max_line_len;
                        while ( rc == 0 && to_print > 0 )
                        {
                            rc = KOutMsg( "%.*s\n", to_print, &data[ idx ] );
                            if ( rc == 0 )
                            {
                                idx += ctx->max_line_len;
                                to_print = ( row_len - idx );
                                if ( to_print > ctx->max_line_len )
                                    to_print = ctx->max_line_len;
                            }
                        }
                    }
                }
            }
        }
        num_gen_iterator_destroy( iter );
    }
    return rc;
}


static rc_t vdb_fastq_loop_without_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
    const struct num_gen_iter * iter;
    rc_t rc = num_gen_iterator_make( ctx->rows, &iter );
    DISP_RC( rc, "num_gen_iterator_make() failed" );
    if ( rc == 0 )
    {
        int64_t row_id;
        while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) )
        {
            if ( rc == 0 )
                rc = Quitting();
            if ( rc == 0 )
            {
                uint32_t elem_bits, boff, row_len;
                const char * data;

                rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
                                            (const void**)&data, &boff, &row_len );
                if ( rc != 0 )
                    vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
                else
                {
                    rc = KOutMsg( "@%s.%li %li length=%u\n%.*s\n",
                                  fctx->run_name, row_id, row_id, row_len, row_len, data );
                    if ( rc == 0 )
                    {
                        rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_qual, &elem_bits,
                                                    (const void**)&data, &boff, &row_len );
                        if ( rc != 0 )
                            vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), QUALITY ) failed", rc, row_id );
                        else
                            rc = KOutMsg( "+%s.%li %li length=%u\n%.*s\n",
                                          fctx->run_name, row_id, row_id, row_len, row_len, data );
                    }
                }
            }
        }
        num_gen_iterator_destroy( iter );
    }
    return rc;
}


static rc_t vdb_fasta_loop_without_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
    const struct num_gen_iter * iter;
    rc_t rc = num_gen_iterator_make( ctx->rows, &iter );
    DISP_RC( rc, "num_gen_iterator_make() failed" );
    if ( rc == 0 )
    {
        int64_t row_id;
        while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) )
        {
            if ( rc == 0 )
                rc = Quitting();
            if ( rc == 0 )
            {
                uint32_t elem_bits, boff, row_len;
                const char * data;

                rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
                                            (const void**)&data, &boff, &row_len );
                if ( rc != 0 )
                    vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
                else
                {
                    uint32_t idx = 0;
                    int32_t to_print = row_len;

                    rc = KOutMsg( ">%s.%li %li length=%u\n",
                                  fctx->run_name, row_id, row_id, row_len );
                    if ( to_print > ctx->max_line_len )
                        to_print = ctx->max_line_len;
                    while ( rc == 0 && to_print > 0 )
                    {
                        rc = KOutMsg( "%.*s\n", to_print, &data[ idx ] );
                        if ( rc == 0 )
                        {
                            idx += ctx->max_line_len;
                            to_print = ( row_len - idx );
                            if ( to_print > ctx->max_line_len )
                                to_print = ctx->max_line_len;
                        }
                    }
                }
            }
        }
        num_gen_iterator_destroy( iter );
    }
    return rc;
}


static rc_t vdb_prepare_cursor( const p_dump_context ctx, const VTable * tbl, fastq_ctx * fctx )
{
    rc_t rc;

    /* first we try to open READ/QUALITY/NAME */
    rc = VTableCreateCachedCursorRead( tbl, &fctx->cursor, ctx->cur_cache_size );
    DISP_RC( rc, "VTableCreateCursorRead( 1st ) failed" );
    if ( rc == 0 )
    {
        rc = VCursorAddColumn( fctx->cursor, &fctx->idx_read, "(INSDC:dna:text)READ" );
        if ( rc == 0 && ctx->format == df_fastq )
            rc = VCursorAddColumn( fctx->cursor, &fctx->idx_qual, "(INSDC:quality:text:phred_33)QUALITY" );
        else
            fctx->idx_qual = INVALID_COLUMN;
        if ( rc == 0 )
            rc = VCursorAddColumn( fctx->cursor, &fctx->idx_name, "(ascii)NAME" );
        if ( rc == 0 )
            rc = VCursorPermitPostOpenAdd ( fctx->cursor );
        if ( rc == 0 )
            rc = VCursorOpen ( fctx->cursor );

        if ( rc != 0 )
        {
            VCursorRelease( fctx->cursor );
            rc = VTableCreateCachedCursorRead( tbl, &fctx->cursor, ctx->cur_cache_size );
            DISP_RC( rc, "VTableCreateCursorRead( 2nd ) failed" );
            if ( rc == 0 )
            {
                rc = VCursorAddColumn( fctx->cursor, &fctx->idx_read, "(INSDC:dna:text)READ" );
                if ( rc == 0 && ctx->format == df_fastq )
                    rc = VCursorAddColumn( fctx->cursor, &fctx->idx_qual, "(INSDC:quality:text:phred_33)QUALITY" );
                else
                    fctx->idx_qual = INVALID_COLUMN;
                if ( rc == 0 )
                    rc = VCursorOpen ( fctx->cursor );
                fctx->idx_name = INVALID_COLUMN;
            }
        }
    }
    return rc;
}


static rc_t vdb_fastq_tbl( const p_dump_context ctx,
                           const VTable * tbl,
                           fastq_ctx * fctx )
{
    rc_t rc = vdb_prepare_cursor( ctx, tbl, fctx );
    DISP_RC( rc, "the table lacks READ and/or QUALITY column" );
    if ( rc == 0 )
    {
        int64_t  first;
        uint64_t count;
        rc = VCursorIdRange( fctx->cursor, fctx->idx_read, &first, &count );
        DISP_RC( rc, "VCursorIdRange() failed" );
        if ( rc == 0 )
        {
            /* if the user did not specify a row-range, take all rows */
            if ( ctx->rows == NULL )
            {
                rc = num_gen_make_from_range( &ctx->rows, first, count );
                DISP_RC( rc, "num_gen_make_from_range() failed" );
            }
            /* if the user did specify a row-range, check the boundaries */
            else
            {
                rc = num_gen_trim( ctx->rows, first, count );
                DISP_RC( rc, "num_gen_trim() failed" );
            }

            if ( rc == 0 && !num_gen_empty( ctx->rows ) )
            {
                if ( ctx->format == df_fastq )
                {
                    if ( fctx->idx_name == INVALID_COLUMN)
                        rc = vdb_fastq_loop_without_name( ctx, fctx ); /* <--- */
                    else
                        rc = vdb_fastq_loop_with_name( ctx, fctx ); /* <--- */
                }
                else if ( ctx->format == df_fasta )
                {
                    if ( ctx->max_line_len == 0 )
                        ctx->max_line_len = DEF_FASTA_LEN;
                    if ( fctx->idx_name == INVALID_COLUMN)
                        rc = vdb_fasta_loop_without_name( ctx, fctx ); /* <--- */
                    else
                        rc = vdb_fasta_loop_with_name( ctx, fctx ); /* <--- */
                }
            }
            else
            {
                rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
            }
        }
        VCursorRelease( fctx->cursor );
    }
    return rc;
}


static rc_t vdb_fastq_table( const p_dump_context ctx,
                             const VDBManager *mgr,
                             fastq_ctx * fctx )
{
    const VTable * tbl;
    VSchema * schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &(ctx->schema_list) );

    rc = VDBManagerOpenTableRead( mgr, &tbl, schema, "%s", ctx->path );
    DISP_RC( rc, "VDBManagerOpenTableRead() failed" );
    if ( rc == 0 )
    {
        rc = vdb_fastq_tbl( ctx, tbl, fctx );
        VTableRelease( tbl );
    }
    VSchemaRelease( schema );

    return rc;
}


static rc_t vdb_fastq_database( const p_dump_context ctx,
                                const VDBManager *mgr,
                                fastq_ctx * fctx )
{
    const VDatabase * db;
    VSchema *schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &(ctx->schema_list) );

    rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", ctx->path );
    DISP_RC( rc, "VDBManagerOpenDBRead() failed" );
    if ( rc == 0 )
    {
        bool table_defined = ( ctx->table != NULL );
        if ( !table_defined )
            table_defined = vdh_take_this_table_from_db( ctx, db, "SEQUENCE" );

        if ( table_defined )
        {
            const VTable * tbl;

            rc = VDatabaseOpenTableRead( db, &tbl, "%s", ctx->table );
            DISP_RC( rc, "VDatabaseOpenTableRead() failed" );
            if ( rc == 0 )
            {
                rc = vdb_fastq_tbl( ctx, tbl, fctx );
                VTableRelease( tbl );
            }
        }
        else
        {
            LOGMSG( klogInfo, "opened as vdb-database, but no table found/defined" );
            ctx->usage_requested = true;
        }
        VDatabaseRelease( db );
    }

    VSchemaRelease( schema );
    return rc;
}


static rc_t vdb_fastq_by_pathtype( const p_dump_context ctx,
                                   const VDBManager *mgr,
                                   fastq_ctx * fctx )                                   
{
    rc_t rc;
    int path_type = ( VDBManagerPathType ( mgr, "%s", ctx->path ) & ~ kptAlias );
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
                                "p=%s", ctx->path ) );
                            if ( vdco_schema_count( ctx ) == 0 )
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
    if ( vdh_is_path_database( mgr, ctx->path, &(ctx->schema_list) ) )
    {
        rc = vdb_fastq_database( ctx, mgr, fctx );
        DISP_RC( rc, "dump_database() failed" );
    }
    else if ( vdh_is_path_table( mgr, ctx->path, &(ctx->schema_list) ) )
    {
        rc = vdb_fastq_table( ctx, mgr, fctx );
        DISP_RC( rc, "dump_table() failed" );
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        PLOGERR( klogInt, ( klogInt, rc,
                 "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                 "p=%s", ctx->path ) );
        if ( vdco_schema_count( ctx ) == 0 )
        {
            LOGERR( klogInt, rc, "Maybe it is a legacy table. If so, specify a schema with the -S option" );
        }
    }
    return rc;
}


static char * vdb_fastq_extract_run_name( const char * acc_or_path )
{
    char * delim = string_rchr ( acc_or_path, string_size( acc_or_path ), '/' );
    if ( delim == NULL )
        return string_dup_measure ( acc_or_path, NULL );
    else
        return string_dup_measure ( delim + 1, NULL );    
}


rc_t vdf_main( const p_dump_context ctx, const VDBManager * mgr, const char * acc_or_path )
{
    rc_t rc = 0;
    fastq_ctx fctx;
    fctx.run_name = vdb_fastq_extract_run_name( acc_or_path );
    
    ctx->path = string_dup_measure ( acc_or_path, NULL );

    if ( USE_PATHTYPE_TO_DETECT_DB_OR_TAB ) /* in vdb-dump-context.h */
    {
        rc = vdb_fastq_by_pathtype( ctx, mgr, &fctx );
    }
    else
    {
        rc = vdb_fastq_by_probing( ctx, mgr, &fctx );
    }

    free( (char*)ctx->path );
    free( (void*)fctx.run_name );
    ctx->path = NULL;

    return rc;
}
