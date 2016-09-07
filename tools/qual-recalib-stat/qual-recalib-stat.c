/*==============================================================================
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

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/printf.h>
#include <klib/num-gen.h>
#include <klib/progressbar.h>

#include "context.h"
#include "stat_mod_2.h"
#include "reader.h"
#include "writer.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>

const char UsageDefaultName[] = "qual-recalib-stat";

static const char * rows_usage[] = { "set of rows to be analyzed (default = all)", NULL };
static const char * schema_usage[] = { "schema-names", NULL };
static const char * show_progress_usage[] = { "show progress in percent while analyzing", NULL };
static const char * outfile_usage[] = { "file to write output into", NULL };
static const char * outmode_usage[] = { "where to write [file,db,tab] (dflt = file)", NULL };
static const char * gcwindow_usage[] = { "how many bases are counted (dflt = 7)", NULL };
static const char * exclude_usage[] = { "path to db with ref-positions to be excluded", NULL };
static const char * info_usage[] = { "display info's after the process", NULL };
static const char * ignore_mismatch_usage[] = { "ignore mismatches", NULL };

OptDef MyOptions[] =
{
    { OPTION_ROWS, ALIAS_ROWS, NULL, rows_usage, 1, true, false },
    { OPTION_SCHEMA, ALIAS_SCHEMA, NULL, schema_usage, 5, true, false },
    { OPTION_SHOW_PROGRESS, ALIAS_SHOW_PROGRESS, NULL, show_progress_usage, 1, false, false },
    { OPTION_OUTFILE, ALIAS_OUTFILE, NULL, outfile_usage, 1, true, false },
    { OPTION_OUTMODE, ALIAS_OUTMODE, NULL, outmode_usage, 1, true, false },
    { OPTION_GCWINDOW, ALIAS_GCWINDOW, NULL, gcwindow_usage, 1, true, false },
    { OPTION_EXCLUDE, ALIAS_EXCLUDE, NULL, exclude_usage, 1, true, false },
    { OPTION_INFO, ALIAS_INFO, NULL, info_usage, 1, false, false },
    { OPTION_IGNORE_MISMATCH, ALIAS_IGNORE_MISMATCH, NULL, ignore_mismatch_usage, 1, false, false }
};


rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ( "\n"
                     "Usage:\n"
                     "  %s <path> [options]\n"
                     "\n", progname );
}


rc_t CC Usage( const Args * args  )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram( args, &fullpath, &progname );
    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary( progname );
    KOutMsg ("Options:\n");
    HelpOptionLine ( ALIAS_ROWS, OPTION_ROWS, "rows", rows_usage );
    HelpOptionLine ( ALIAS_SCHEMA, OPTION_SCHEMA, "schema", schema_usage );
    HelpOptionLine ( ALIAS_SHOW_PROGRESS, OPTION_SHOW_PROGRESS, NULL, show_progress_usage );
    HelpOptionLine ( ALIAS_OUTFILE, OPTION_OUTFILE, NULL, outfile_usage );
    HelpOptionLine ( ALIAS_OUTMODE, OPTION_OUTMODE, NULL, outmode_usage );
    HelpOptionLine ( ALIAS_GCWINDOW, OPTION_GCWINDOW, NULL, gcwindow_usage );
    HelpOptionLine ( ALIAS_EXCLUDE, OPTION_EXCLUDE, NULL, exclude_usage );
    HelpOptionLine ( ALIAS_INFO, OPTION_INFO, NULL, info_usage );
    HelpOptionLine ( ALIAS_IGNORE_MISMATCH, OPTION_IGNORE_MISMATCH, NULL, ignore_mismatch_usage );
    HelpOptionsStandard();
    HelpVersion( fullpath, KAppVersion() );
    return rc;
}

static uint8_t calc_fract_digits( const struct num_gen_iter *iter )
{
    uint8_t res = 0;
    uint64_t count;
    if ( num_gen_iterator_count( iter, &count ) == 0 )
    {
		if ( count > 10000 )
		{
			if ( count > 100000 )
				res = 2;
			else
				res = 1;
		}
    }
    return res;
}


static rc_t read_loop( statistic * data,
                       context *ctx,
                       statistic_reader *reader,
                       const VCursor *my_cursor )
{
    int64_t first;
    uint64_t count;
    rc_t rc = query_reader_rowrange( reader, &first, &count );
    if ( rc != 0 )
        LogErr( klogInt, rc, "query_statistic_rowrange() failed\n" );
    else
    {
        if ( num_gen_empty( ctx->row_generator ) )
        {
            rc = num_gen_add( ctx->row_generator, first, count );
            if ( rc != 0 )
                LogErr( klogInt, rc, "num_gen_add() failed() failed\n" );
        }
        else
        {
            rc = num_gen_trim( ctx->row_generator, first, count );
            if ( rc != 0 )
                LogErr( klogInt, rc, "num_gen_trim() failed() failed\n" );
        }

        if ( rc == 0 )
        {
            const struct num_gen_iter *iter;
            rc = num_gen_iterator_make( ctx->row_generator, &iter );
            if ( rc != 0 )
                LogErr( klogInt, rc, "num_gen_iterator_make() failed\n" );
            else
            {
                int64_t row_id;
				uint8_t fract_digits = calc_fract_digits( iter );
                struct progressbar * progress;

				
                rc = make_progressbar( &progress, fract_digits );
                if ( rc != 0 )
                    LogErr( klogInt, rc, "make_progressbar() failed\n" );
                else
                {
                    uint32_t percent;
                    row_input row_data;

                    while ( num_gen_iterator_next( iter, &row_id, &rc ) && rc == 0 )
                    {
                        rc = Quitting();
                        if ( rc == 0 )
                        {
                            /* ******************************************** */
                            rc = reader_get_data( reader, &row_data, row_id );
                            if ( rc == 0 )
                            {
                                rc = extract_statistic_from_row( data, &row_data, row_id );
                            }
                            /* ******************************************** */
                            if ( ctx->show_progress &&
                                 num_gen_iterator_percent( iter, fract_digits, &percent ) == 0 )
							{
                                    update_progressbar( progress, percent );
							}
                        }
                    }
                    destroy_progressbar( progress );
                    if ( ctx->show_progress )
                        OUTMSG(( "\n" ));
                }
                num_gen_iterator_destroy( iter );
            }
        }
    }
    return rc;
}


static rc_t read_statistic_from_table( statistic * data,
                                       KDirectory *dir,
                                       context *ctx,
                                       const VDatabase *my_database,
                                       const char *table_name )
{
    const VTable *my_table;
    rc_t rc = VDatabaseOpenTableRead( my_database, &my_table, "%s", table_name );
    if ( rc != 0 )
    {
        LogErr( klogInt, rc, "VDatabaseOpenTableRead() failed\n" );
    }
    else
    {
        const VCursor *my_cursor;
        rc = VTableCreateCursorRead( my_table, &my_cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VTableCreateCursorRead() failed\n" );
        else
        {
/*
            spot_pos sequence;

            rc = make_spot_pos( &sequence, my_database );
            if ( rc == 0 )
            {
*/
                statistic_reader reader;

/*
                rc = make_statistic_reader( &reader, &sequence, dir, my_cursor,
                            ctx->exclude_file_path, ctx->info );
*/
                rc = make_statistic_reader( &reader, NULL, dir, my_cursor,
                            ctx->exclude_file_path, ctx->info );

                if ( rc == 0 )
                {
                    /* ******************************************************* */
                    rc = read_loop( data, ctx, &reader, my_cursor );
                    /* ******************************************************* */
                    whack_reader( &reader );
                }
/*
                whack_spot_pos( &sequence );
            }
*/
            VCursorRelease( my_cursor );
        }
        VTableRelease( my_table );
    }
    return rc;
}


static rc_t gather_statistic( statistic * data,
                              KDirectory *dir,
                              context *ctx )
{
    VDBManager *my_manager;
    /* because this tool is linked against the write-version
       of vdb and kdb, there is no Read-Manager available... */
    rc_t rc = VDBManagerMakeUpdate ( &my_manager, dir );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VDBManagerMakeUpdate() failed\n" );
    else
    {
        const VDatabase *my_database;
        rc = VDBManagerOpenDBRead( my_manager, &my_database, NULL, "%s", ctx->src_path );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VDBManagerOpenDBRead() failed\n" );
        else
        {
            /* ******************************************************* */
            rc = read_statistic_from_table( data, dir, ctx, my_database,
                                            "PRIMARY_ALIGNMENT" );
            /* ******************************************************* */
            VDatabaseRelease( my_database );
        }
        VDBManagerRelease( my_manager );
    }
    return rc;
}


static rc_t gater_and_write( context *ctx )
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
        LogErr( klogInt, rc, "KDirectoryNativeDir() failed\n" );
    else
    {
        statistic data;
        rc = make_statistic( &data, ctx->gc_window, ctx->ignore_mismatch );
        if ( rc == 0 )
        {
            rc = gather_statistic( &data, dir, ctx ); /* <--- the meat */
            if ( rc == 0 )
            {
                uint64_t written;

                if ( ctx->show_progress )
                {
                    OUTMSG(( "%lu statistic-entries gathered\n", data.entries ));
                    OUTMSG(( "max. cycles per read = %u\n", data.max_cycle ));
                }

                switch( ctx->output_mode[ 0 ] )
                {
                case 'f' :  if ( ctx->output_file_path != NULL )
                            {
                                rc = write_output_file( dir, &data,
                                        ctx->output_file_path, &written );
                                if ( rc == 0 && ctx->info )
                                {
                                    OUTMSG(( "%lu lines written to '%s'\n", 
                                            written, ctx->output_file_path ));
                                }
                            }
                            else
                                OUTMSG(( "the output-path is missing!\n" ));
                            break;

                case 'd' :  rc = write_statistic_into_db( dir, &data,
                                    ctx->src_schema_list, ctx->src_path, &written,
                                    ctx->show_progress );
                            if ( rc == 0 && ctx->info )
                            {
                                OUTMSG(( "%lu rows written to database\n", written ));
                            }
                            break;

                case 't' :  if ( ctx->output_file_path != NULL )
                            {
                                rc = write_statistic_into_tab( dir, &data, 
                                        ctx->src_schema_list, ctx->output_file_path, &written,
                                        ctx->show_progress );
                                if ( rc == 0 && ctx->info )
                                {
                                    OUTMSG(( "%lu rows written to table\n", written ));
                                }
                            }
                            else
                            {
                                OUTMSG(( "the output-path is missing!\n" ));
                            }
                            break;
                }
            }
            whack_statistic( &data );
        }
        KDirectoryRelease( dir );
    }
    return rc;
}


rc_t CC KMain( int argc, char * argv[] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                             MyOptions, sizeof MyOptions / sizeof ( OptDef ) );
    if ( rc != 0 )
        LogErr( klogInt, rc, "ArgsMakeAndHandle() failed\n" );
    else
    {
        context *ctx;
        KLogHandlerSetStdErr();
        rc = context_init( &ctx );
        if ( rc != 0 )
            LogErr( klogInt, rc, "context_init() failed\n" );
        else
        {
            rc = context_capture_arguments_and_options( args, ctx );
            if ( rc != 0 )
                LogErr( klogInt, rc, "context_capture_arguments_and_options() failed\n" );
            else
            {
                if ( ctx->usage_requested ) {
                    MiniUsage( args );
                    rc = RC(rcApp, rcArgv, rcParsing, rcParam, rcInsufficient);
                }
                else
                {
                    switch( ctx->output_mode[ 0 ] )
                    {
                        case 'd' :
                        case 't' : if ( context_schema_count( ctx ) == 0 )
                                    {
                                        OUTMSG(( "cannot write, schema-file is missing:\n" ));
                                        Usage( args  );
                                        rc = RC( rcApp, rcNoTarg, rcConstructing, rcSelf, rcNull );
                                    }
                    }
                    if ( rc == 0 )
                    {
                        /* ************************* */
                        rc = gater_and_write( ctx );
                        /* ************************* */
                    }
                }
            }
            context_destroy ( ctx );
        }
        ArgsWhack ( args );
    }
    return rc;
}
