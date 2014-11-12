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

#include "writer.h"

#include <klib/printf.h>
#include <klib/progressbar.h>
#include <kfs/file.h>

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const char * widx_names[ N_WIDX ] =
{
    "SPOT_GROUP",
    "KMER",
    "ORIG_QUAL",
    "TOTAL_COUNT",
    "MISMATCH_COUNT",
    "CYCLE",
    "HPRUN",
    "GC_CONTENT",
    "MAX_QUAL",
    "NREAD"
};


static rc_t open_writer_cursor( statistic_writer *writer )
{
    rc_t rc = 0;
    uint32_t idx;

    for ( idx = 0; idx < N_WIDX && rc == 0; ++idx )
    {
        rc = add_column( writer->cursor, &writer->wr_col[ idx ], widx_names[ idx ] );
    }
    if ( rc == 0 )
    {
        rc = VCursorOpen ( writer->cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorOpen failed\n" );
    }
    return rc;
}


static rc_t make_statistic_writer( statistic_writer *writer, VCursor * cursor )
{
    memset( &writer->wr_col, 0, sizeof writer->wr_col );
    writer->cursor = cursor;
    return open_writer_cursor( writer );
}


typedef struct writer_ctx
{
    statistic_writer *writer;
    rc_t rc;

    struct progressbar * progress;
    uint64_t entries;
    uint64_t n;
    uint8_t fract_digits;
} writer_ctx;


static uint8_t progressbar_calc_fract_digits( const uint64_t count )
{
    uint8_t res = 0;
    if ( count > 10000 )
    {
        if ( count > 100000 )
            res = 2;
        else
            res = 1;
    }
    return res;
}

static uint32_t progressbar_percent( const uint64_t count, const uint64_t value,
									 const uint8_t fract_digits )
{
    uint32_t factor = 100;
    uint32_t res = 0;

    if ( fract_digits > 0 )
    {
        if ( fract_digits > 1 )
            factor = 10000;
        else
            factor = 1000;
    }
        
    if ( count > 0 )
    {
        if ( value >= count )
            res = factor;
        else
        {
            uint64_t temp = value;
            temp *= factor;
            temp /= count;
            res = (uint16_t) temp;
        }
    }
    return res;
}


static bool CC write_cb( stat_row * row, void * data )
{
    writer_ctx * ctx = ( writer_ctx * ) data;
    col * cols = ( col * )&ctx->writer->wr_col;
    VCursor * cursor = ctx->writer->cursor;
  
    rc_t rc = VCursorOpenRow( cursor );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VCursorOpen() failed\n" );

    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_SPOT_GROUP ].idx, 8,
                              row->spotgroup, string_size( row->spotgroup ),
                              widx_names[ WIDX_SPOT_GROUP ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_KMER ].idx, 8,
                              row->dimer, string_size( row->dimer ),
                              widx_names[ WIDX_KMER ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_ORIG_QUAL ].idx, 8,
                              &row->quality, 1, widx_names[ WIDX_ORIG_QUAL ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_CYCLE ].idx, 32,
                              &row->base_pos, 1, widx_names[ WIDX_CYCLE ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_TOTAL_COUNT ].idx, 32,
                              &row->count, 1, widx_names[ WIDX_TOTAL_COUNT ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_MISMATCH_COUNT ].idx, 32,
                              &row->mismatch_count, 1,
                              widx_names[ WIDX_MISMATCH_COUNT ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_HPRUN ].idx, 32,
                              &row->hp_run, 1, widx_names[ WIDX_HPRUN ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_GC_CONTENT ].idx, 32,
                              &row->gc_content, 1, widx_names[ WIDX_GC_CONTENT ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_MAX_QUAL ].idx, 8,
                              &row->max_qual_value, 1, widx_names[ WIDX_MAX_QUAL ] );
    if ( rc == 0 )
        rc = write_to_cursor( cursor, cols[ WIDX_NREAD ].idx, 8,
                              &row->n_read, 1, widx_names[ WIDX_NREAD ] );


    if ( rc == 0 )
    {
        rc = VCursorCommitRow( cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorCommitRow() failed\n" );
    }
    if ( rc == 0 )
    {
        rc = VCursorCloseRow( cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorCloseRow() failed\n" );
    }

    ctx->rc = rc;
    if ( ctx->progress != NULL && rc == 0 )
    {
        uint32_t percent = progressbar_percent( ctx->entries, ++( ctx->n ), ctx->fract_digits );
        update_progressbar( ctx->progress, percent );
    }

    return ( rc == 0 );
}


static rc_t write_statistic( statistic_writer *writer, statistic *data,
                             uint64_t * written, bool show_progress )
{
    writer_ctx ctx;
    uint64_t count;

    ctx.writer = writer;
    ctx.rc = 0;
    ctx.progress = NULL;

    if ( show_progress )
    {
        ctx.entries = data->entries;
        ctx.fract_digits = progressbar_calc_fract_digits( ctx.entries );
		make_progressbar( &ctx.progress, ctx.fract_digits );
        ctx.n = 0;
    }

    count = foreach_statistic( data, write_cb, &ctx );

    if ( show_progress )
    {
        destroy_progressbar( ctx.progress );
        OUTMSG(( "\n" ));
    }

    if ( written != NULL ) *written = count;

    return ctx.rc;
}


static rc_t whack_statistic_writer( statistic_writer *writer )
{
    rc_t rc = VCursorCommit( writer->cursor );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VCursorCommit() failed\n" );
    return rc;
}


typedef struct write_ctx
{
    KFile *out;
    uint64_t pos;
    uint64_t lines;

    struct progressbar *progress;
    uint64_t entries;
    uint8_t fract_digits;
} write_ctx;


static bool CC write_to_file_cb( stat_row * row, void * f_data )
{
    write_ctx * wctx = ( write_ctx * ) f_data;
    char buffer[ 256 ];
    size_t num_writ;

    rc_t rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                "%s\t%u\t%u\t%s\t%u\t%u\t%u\t%u\t%u\t%u\n", 
                                row->spotgroup, 
                                row->base_pos,
                                row->n_read,
                                row->dimer,
                                row->gc_content,
                                row->hp_run,
                                row->max_qual_value,
                                row->quality,
                                row->count,
                                row->mismatch_count );

    if ( rc == 0 )
    {
        size_t f_writ;
        rc = KFileWrite ( wctx->out, wctx->pos, buffer, num_writ, &f_writ );
        if ( rc == 0 )
        {
            uint32_t percent = progressbar_percent( wctx->entries, ++wctx->lines, wctx->fract_digits );
            update_progressbar( wctx->progress, percent );
            wctx->pos += f_writ;
        }
    }
    return ( rc == 0 );
}


rc_t write_output_file( KDirectory *dir, statistic * data,
                        const char * path, uint64_t * written )
{
    write_ctx wctx;
    rc_t rc;

    if ( written != NULL )
    {
        *written = 0;
    }
    wctx.out = NULL;
    wctx.pos = 0;
    wctx.lines = 0;
    rc = KDirectoryCreateFile ( dir, &wctx.out, false, 0664, kcmInit, "%s", path );
    if ( rc != 0 )
        LogErr( klogInt, rc, "KDirectoryCreateFile() failed\n" );
    else
    {
        char buffer[ 256 ];
        size_t num_writ;
        rc = string_printf ( buffer, sizeof buffer, &num_writ,
          "SPOTGROUP\tCYCLE\tNRead\tDIMER\tGC_CONTENT\tHP_RUN\tMaxQ\tQuality\tTOTAL\tMISMATCH\n" );
        if ( rc == 0 )
        {
            size_t f_writ;
            rc = KFileWrite ( wctx.out, wctx.pos, buffer, num_writ, &f_writ );
            if ( rc == 0 )
            {
                if ( written != NULL ) *written = f_writ;
                wctx.pos += f_writ;

                wctx.entries = data->entries;
                wctx.fract_digits = progressbar_calc_fract_digits( wctx.entries );
				make_progressbar( &wctx.progress, wctx.fract_digits );
				
                foreach_statistic( data, write_to_file_cb, &wctx );

                destroy_progressbar( wctx.progress );
                OUTMSG(( "\n" ));

                KFileRelease ( wctx.out );
                if ( written != NULL )
                {
                    *written = wctx.lines;
                }
            }
        }
    }
    return rc;
}


static rc_t make_schema( const KNamelist * schema_list,
                  VDBManager *my_manager, VSchema ** schema )
{
    rc_t rc = VDBManagerMakeSchema ( my_manager, schema );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VDBManagerMakeSchema() failed\n" );
    else
    {
        uint32_t count;
        rc = KNamelistCount ( schema_list, &count );
        if ( rc !=0 )
            LogErr( klogInt, rc, "KNamelistCount(schema-list) failed\n" );
        else
        {
            uint32_t i;
            for ( i = 0; i < count && rc == 0; ++i )
            {
                const char * name;
                rc = KNamelistGet ( schema_list, i, &name );
                if ( rc !=0 )
                    LogErr( klogInt, rc, "KNamelistGet(schema-list) failed\n" );
                else
                {
                    rc = VSchemaParseFile ( *schema, "%s", name );
                    if ( rc !=0 )
                        LogErr( klogInt, rc, "VSchemaParseFile() failed\n" );
                }
            }
        }
    }
    return rc;
}


static rc_t write_statistic_cmn( VTable * my_table, statistic * data,
                          uint64_t * written, bool show_progress )
{
    VCursor *my_cursor;
    rc_t rc = VTableCreateCursorWrite( my_table, &my_cursor, kcmInsert );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VTableCreateCursorWrite() failed\n" );
    else
    {
        statistic_writer writer;
        rc = make_statistic_writer( &writer, my_cursor );
        if ( rc == 0 )
        {
            rc = write_statistic( &writer, data, written, show_progress );
            if ( rc == 0 )
                rc = whack_statistic_writer( &writer );
        }
        VCursorRelease( my_cursor );
    }
    return rc;
}


rc_t write_statistic_into_tab( KDirectory *dir, statistic * data,
        const KNamelist *schema_list, const char *output_file_path,
        uint64_t * written, bool show_progress )
{
    VDBManager *my_manager;
    rc_t rc;

    if ( written != NULL ) *written = 0;
    rc = VDBManagerMakeUpdate ( &my_manager, dir );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VDBManagerMakeUpdate() failed\n" );
    else
    {
        VSchema * my_schema;
        rc = make_schema( schema_list, my_manager, &my_schema );
        if ( rc == 0 )
        {
            VTable * my_table;
            rc = VDBManagerCreateTable( my_manager, &my_table, my_schema, 
                                        "NCBI:align:tbl:qstat",
                                        kcmInit | kcmParents,
                                        "%s", output_file_path );
            if ( rc != 0 )
                LogErr( klogInt, rc, "VDBManagerCreateTable() failed\n" );
            else
            {
                rc = write_statistic_cmn( my_table, data, written, show_progress );
                VTableRelease( my_table );
            }
            VSchemaRelease( my_schema );
        }
        VDBManagerRelease( my_manager );
    }
    return rc;
}


rc_t write_statistic_into_db( KDirectory *dir, statistic * data,
         const KNamelist *schema_list, const char *src_path,
        uint64_t * written, bool show_progress )
{
    VDBManager *my_manager;
    rc_t rc;

    if ( written != NULL ) *written = 0;
    rc = VDBManagerMakeUpdate ( &my_manager, dir );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VDBManagerMakeUpdate() failed\n" );
    else
    {
        VSchema * my_schema;
        rc = make_schema( schema_list, my_manager, &my_schema );
        if ( rc == 0 )
        {
            VDatabase *my_database;
            rc = VDBManagerOpenDBUpdate( my_manager, &my_database, 
                                         my_schema, "%s", src_path );
            if ( rc != 0 )
                LogErr( klogInt, rc, "VDBManagerOpenDBUpdate() failed\n" );
            else
            {
                VTable * my_table;
                rc = VDatabaseCreateTable( my_database, &my_table,
                                           "QUAL_STAT",
                                           kcmOpen | kcmParents,
                                           "QUAL_STAT" );
                if ( rc !=0 )
                    LogErr( klogInt, rc, "VDatabaseCreateTable() failed\n" );
                else
                {
                    rc = write_statistic_cmn( my_table, data, written, show_progress );
                    VTableRelease( my_table );
                }
                VDatabaseRelease( my_database );
            }
            VSchemaRelease( my_schema );
        }
        VDBManagerRelease( my_manager );
    }
    return rc;
}
