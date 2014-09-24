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

#include "run-stat.vers.h"

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>
#include <kfs/directory.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/container.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/debug.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <sra/srapath.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <bitstr.h>

#include "context.h"
#include "helper.h"
#include "progressbar.h"
#include "mod_cmn.h"

static const char * table_usage[] = { "table-name (if src is a database)", NULL };
static const char * rows_usage[] = { "rows (default = all)", NULL };
static const char * schema_usage[] = { "schema to be used on sourc-table", NULL };
static const char * without_accession_usage[] = { "without accession-test", NULL };
static const char * progress_usage[] = { "show a progress-bar (default: no)", NULL };
static const char * module_usage[] = { "selects a statistic-module (default all build-in)", NULL };
static const char * grafic_usage[] = { "enables output of grafic-files", NULL };
static const char * report_usage[] = { "output-format of the report-files:", 
                                       "txt..text (default), cvs..comma-sep.-format",
                                       "xml..xml-format, json...json-format",
                                       NULL };
static const char * output_usage[] = { "location of report/grafics", 
                                       "default = current directory",
                                       NULL };

static const char * prefix_usage[] = { "name-prefix for generated files", 
                                       "default = 'report'",
                                       NULL };

OptDef StatOptions[] =
{
    { OPTION_TABLE, ALIAS_TABLE, NULL, table_usage, 1, true, false },
    { OPTION_ROWS, ALIAS_ROWS, NULL, rows_usage, 1, true, false },
    { OPTION_SCHEMA, ALIAS_SCHEMA, NULL, schema_usage, 5, true, false },
    { OPTION_PROGRESS, ALIAS_PROGRESS, NULL, progress_usage, 1, false, false },
    { OPTION_MODULE, ALIAS_MODULE, NULL, module_usage, 0, true, false },
    { OPTION_WITHOUT_ACCESSION, ALIAS_WITHOUT_ACCESSION, NULL, without_accession_usage, 1, false, false },
    { OPTION_GRAFIC, ALIAS_GRAFIC, NULL, grafic_usage, 1, false, false },
    { OPTION_REPORT, ALIAS_RREPORT, NULL, report_usage, 1, true, false },
    { OPTION_OUTPUT, ALIAS_OUTPUT, NULL, output_usage, 1, true, false },
    { OPTION_PREFIX, ALIAS_PREFIX, NULL, prefix_usage, 1, true, false }
};


const char UsageDefaultName[] = "run-stat";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <path> [options]\n"
                    "\n", progname);
}

rc_t CC Usage (const Args * args)
{
    const char * progname;
    const char * fullpath;
    rc_t rc;

    if (args == NULL)
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );
    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg (( "Options:\n" ));
    HelpOptionLine ( ALIAS_TABLE, OPTION_TABLE, "table", table_usage );
    HelpOptionLine ( ALIAS_ROWS, OPTION_ROWS, "rows", rows_usage );
    HelpOptionLine ( ALIAS_SCHEMA, OPTION_SCHEMA, "schema", schema_usage );
    HelpOptionLine ( ALIAS_PROGRESS, OPTION_PROGRESS, "progress", progress_usage );
    HelpOptionLine ( ALIAS_MODULE, OPTION_MODULE, "module", module_usage );
    HelpOptionLine ( ALIAS_WITHOUT_ACCESSION, OPTION_WITHOUT_ACCESSION, NULL, without_accession_usage );
    HelpOptionLine ( ALIAS_GRAFIC, OPTION_GRAFIC, NULL, grafic_usage );
    HelpOptionLine ( ALIAS_REPORT, OPTION_REPORT, NULL, report_usage );
    HelpOptionLine ( ALIAS_OUTPUT, OPTION_OUTPUT, NULL, output_usage );
    HelpOptionLine ( ALIAS_PREFIX, OPTION_PREFIX, NULL, prefix_usage );

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

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
    return RUN_STAT_VERS;
}


#if TOOLS_USE_SRAPATH != 0
static char *translate_accession( SRAPath *my_sra_path,
                                  const char *accession,
                                  const size_t bufsize )
{
    char *res = malloc( bufsize );
    if ( res != NULL )
    {
        rc_t rc = SRAPathFind( my_sra_path, accession, res, bufsize );
        if ( GetRCState( rc ) == rcNotFound )
        {
            free( res );
            res = NULL;
        }
        else if ( GetRCState( rc ) == rcInsufficient )
        {
            DBGMSG ( DBG_APP, 0,  ( "bufsize %lu was insufficient\n", bufsize ) );
            free( res );
            res = translate_accession( my_sra_path, accession, bufsize * 2 );
        }
        else if ( rc != 0 )
        {
            free( res );
            res = NULL;
        }
    }
    return res;
}
#endif


#if TOOLS_USE_SRAPATH != 0
static rc_t check_accession( p_stat_ctx ctx, const KDirectory *my_dir )
{
    rc_t rc = 0;
    if ( strchr ( ctx->path, '/' ) == NULL )
    {
        SRAPath *my_sra_path;
        rc = SRAPathMake( &my_sra_path, my_dir );
        if ( rc != 0 )
        {
            if ( GetRCState ( rc ) != rcNotFound || GetRCTarget ( rc ) != rcDylib )
                LOGERR( klogInt, rc, "SRAPathMake() failed" );
            else
                rc = 0;
        }
        else
        {
            if ( !SRAPathTest( my_sra_path, ctx->path ) )
            {
                char *buf = translate_accession( my_sra_path, ctx->path, 64 );
                if ( buf != NULL )
                {
                    DBGMSG ( DBG_APP, 0,  ( "sra-accession found! >%s<\n", buf ) );
                    free( (char*)ctx->path );
                    ctx->path = buf;
                }
            }
            SRAPathRelease( my_sra_path );
        }
    }
    return rc;
}
#endif


static void row_error( const char * fmt, rc_t rc, uint64_t row_id )
{
    PLOGERR( klogInt, ( klogInt, rc, fmt, "row_nr=%lu", row_id ) );
}


static rc_t run_stat_rows( const VCursor * cur,
                           p_mod_list list,
                           p_ng row_generator,
                           bool show_progress )
{
    rc_t rc = 0;
    uint64_t row_id, requested;
    uint64_t processed = 0;
    progressbar *pb = NULL;
    uint8_t digits = 0;

    requested = ng_count( row_generator );
    OUTMSG(( "we will read: %lu rows\n", requested ));

    if ( show_progress )
    {
        rc = make_progressbar( &pb );
        DISP_RC( rc, "run_stat_rows:make_progressbar() failed" );
        if ( rc == 0 )
            digits = calc_progressbar_digits( requested );
    }
    if ( rc == 0 )
    {
        ng_start( row_generator );
        while ( ( ng_next( row_generator, &row_id ) )&&( rc == 0 ) )
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                rc = VCursorSetRowId( cur, row_id );
                if ( rc != 0 )
                    row_error( "VCursorSetRowId( row#$(row_nr) ) failed", rc, row_id );
                else
                {
                    rc = VCursorOpenRow( cur );
                    if ( rc != 0 )
                        row_error( "VCursorOpenRow( row#$(row_nr) ) failed", rc, row_id );
                    else
                    {
                        /*****************************************************/
                        /* pass every row on to the list of modules          */
                        rc_t rc1 = mod_list_row( list, cur );
                        /*****************************************************/
                        if ( rc1 != 0 )
                            row_error( "stat_module_row( row#$(row_nr) ) failed", rc1, row_id );
                        else
                        {
                            if ( pb != NULL )
                            {
                                uint16_t percent = percent_progressbar( requested, 
                                                                        ++processed,
                                                                        digits );
                                update_progressbar( pb, digits, percent );
                            }
                        }
                        rc = VCursorCloseRow( cur );
                        if ( rc != 0 )
                            row_error( "VCursorCloseRow( row#$(row_nr) ) failed",  rc, row_id );
                        else
                            rc = rc1;
                    }
                }
            }
        }
        if ( pb != 0 )
        {
            rc_t rc1 = destroy_progressbar( pb );
            DISP_RC( rc1, "run_stat_rows:destroy_progressbar() failed" );
        }
    }
    if ( show_progress )
        OUTMSG(( "\n" ));
    return rc;
}


static rc_t run_stat_check_range( p_ng row_generator, const VCursor * cur )
{
    int64_t  first;
    uint64_t count;
    rc_t rc = VCursorIdRange( cur, 0, &first, &count );
    DISP_RC( rc, "run_stat_check_range:VCursorIdRange() failed" );
    if ( rc == 0 )
    {
        /* if the user did not specify a row-range, take all rows */
        if ( ng_range_defined( row_generator ) == false )
        {
            ng_set_range( row_generator, first, count );
        }
        /* if the user did specify a row-range, check the boundaries */
        else
        {
            ng_check_range( row_generator, first, count );
        }

        if ( !ng_range_defined( row_generator ) )
        {
            rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
            DISP_RC( rc, "run_stat_check_range:ng_range_defined() failed" );
        }

    }
    return rc;
}


static rc_t run_stat_tab( p_stat_ctx ctx,
                          const VTable * tab,
                          p_mod_list m_list )
{
    const VCursor *cur;
    rc_t rc = VTableCreateCursorRead( tab, &cur );
    DISP_RC( rc, "run_stat_tab:VTableCreateCursorRead() failed" );
    if ( rc == 0 )
    {
        rc = mod_list_pre_open( m_list, cur );
        if ( rc == 0 )
        {
                rc = VCursorOpen( cur );
                DISP_RC( rc, "run_stat_tab:VCursorOpen() failed" );
                if ( rc == 0 )
                {
                    rc = run_stat_check_range( ctx->row_generator, cur );
                    if ( rc == 0 )
                    {
                        /***************************************/
                        rc = run_stat_rows( cur,
                                            m_list,
                                            ctx->row_generator,
                                            ctx->show_progress );

                        if ( rc == 0 )
                            rc = mod_list_post_rows( m_list );
                        /***************************************/
                    }
                }
        }
        {
            rc_t rc1 = VCursorRelease( cur );
            DISP_RC( rc1, "run_stat_tab:VCursorRelease() failed" );
        }
    }
    return 0;
}


static rc_t run_stat_db( p_stat_ctx ctx,
                         const VDatabase * db,
                         p_mod_list m_list )
{
    rc_t rc = 0;
    bool table_defined = ( ctx->table != NULL );
    if ( !table_defined )
    {
        table_defined = helper_take_this_table_from_db( db, ctx, "SEQUENCE" );
        if ( !table_defined )
            table_defined = helper_take_1st_table_from_db( db, ctx );
    }
    if ( table_defined )
    {
        const VTable *tab;
        rc = VDatabaseOpenTableRead( db, &tab, "%s", ctx->table );
        DISP_RC( rc, "run_stat_db:VDatabaseOpenTableRead() failed" );
        if ( rc == 0 )
        {
            /************************************/
            rc = run_stat_tab( ctx, tab, m_list );
            /************************************/
            {
                rc_t rc1 = VTableRelease( tab );
                DISP_RC( rc1, "run_stat_db:VTableRelease() failed" );
            }
        }
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcCopying, rcItem, rcNotFound );
        LOGMSG( klogInfo, "opened as vdb-dabase, but no table found" );
    }
    return rc;
}


static rc_t run_stat_collect_data( p_stat_ctx ctx,
                              const VDBManager * mgr,
                              p_mod_list m_list )
{
    VSchema * dflt_schema;
    rc_t rc = helper_parse_schema( mgr, &dflt_schema, ctx->schema_list );
    DISP_RC( rc, "run_stat_collect_data:helper_parse_schema() failed" );
    if ( rc == 0 )
    {
        const VDatabase * db;
        /* try to open it as a database */
        rc = VDBManagerOpenDBRead ( mgr, &db, dflt_schema, "%s", ctx->path );
        if ( rc == 0 )
        {
            /* if it succeeds it is a database, continue ... */
            /**********************************/
            rc = run_stat_db( ctx, db, m_list );
            /**********************************/
            VDatabaseRelease( db );
        }
        else
        {
            const VTable * tab;
            /* try to open it as a table */
            rc = VDBManagerOpenTableRead( mgr, &tab, dflt_schema, "%s", ctx->path );
            /* if it succeeds it is a table, continue ... */
            if ( rc == 0 )
            {
                /************************************/
                rc = run_stat_tab( ctx, tab, m_list );
                /************************************/
                VTableRelease( tab );
            }
            else
            {
                rc = RC( rcVDB, rcNoTarg, rcCopying, rcItem, rcNotFound );
                PLOGERR( klogInt, ( klogInt, rc,
                         "\nthe path '$(path)' cannot be opened as vdb-database or vdb-table",
                         "path=%s", ctx->path ));
            }
        }
        {
            rc_t rc1 = VSchemaRelease( dflt_schema );
            DISP_RC( rc1, "run_stat_perform:VSchemaRelease(dflt) failed" );
        }
    }
    return rc;
}

static const char * ext_txt = "txt";
static const char * ext_csv = "csv";
static const char * ext_xml = "xml";
static const char * ext_jso = "json";

static const char * run_stat_ext( const uint32_t report_type )
{
    const char * res = ext_txt;
    switch( report_type )
    {
    case RT_CSV : res = ext_csv; break;
    case RT_XML : res = ext_xml; break;
    case RT_JSO : res = ext_jso; break;
    }
    return res;
}

static rc_t run_stat_assemble_filename( p_char_buffer buf, p_stat_ctx ctx,
                                 const char * module_name,
                                 const char * report_name,
                                 const char * ext )
{
    rc_t rc;
    size_t size = string_size( ctx->output_path );
    size += string_size( ctx->name_prefix );
    size += string_size( module_name );
    size += string_size( report_name );
    size += string_size( ext );
    size += 6;

    if ( ctx->output_path == NULL )
        rc = char_buffer_printf( buf, size, 
            "%s_%s_%s.%s", ctx->name_prefix, module_name, report_name, ext );
    else
        rc = char_buffer_printf( buf, size, 
            "%s/%s_%s_%s.%s", ctx->output_path, ctx->name_prefix, module_name, 
                              report_name, ext );
    return rc;
}


static rc_t run_stat_module_sub_report( p_stat_ctx ctx, p_mod_list m_list,
    const char * module_name, uint32_t m_idx, uint32_t r_idx, p_char_buffer buffer )
{
    char * report_name;
    rc_t rc = mod_list_subreport_name( m_list, m_idx, r_idx, &report_name );
    if ( rc == 0 )
    {
        buffer->len = 0;
        rc = mod_list_subreport( m_list, m_idx, r_idx, buffer, ctx->report_type );
        if ( rc == 0 )
        {
            char_buffer fn;
            rc = char_buffer_init( &fn, 1024 );
            if ( rc == 0 )
            {
                const char * ext = run_stat_ext( ctx->report_type );
                rc = run_stat_assemble_filename( &fn, ctx,
                                    module_name, report_name, ext );
                if ( rc == 0 )
                {
                    if ( buffer->len > 0 )
                    {
                        rc = char_buffer_saveto( buffer, fn.ptr );
                        if ( rc == 0 )
                            OUTMSG(( "written: %s\n", fn.ptr ));
                    }
                    fn.len = 0;
                }

                if ( ctx->produce_grafic )
                {
                    rc = run_stat_assemble_filename( &fn, ctx,
                                            module_name, report_name, "svg" );
                    if ( rc == 0 )
                    {
                        rc_t rc1 = mod_list_graph( m_list, m_idx, r_idx, fn.ptr );
                        if ( rc1 == 0 )
                            OUTMSG(( "written: %s\n", fn.ptr ));
                    }
                }
                char_buffer_destroy( &fn );
            }
        }
        free( report_name );
    }
    return rc;
}


static rc_t run_stat_module_report( p_stat_ctx ctx, p_mod_list m_list,
                                uint32_t m_idx, p_char_buffer buffer )
{
    char * module_name;
    rc_t rc = mod_list_name( m_list, m_idx, &module_name );
    if ( rc == 0 )
    {
        uint32_t r_count;
        rc = mod_list_subreport_count( m_list, m_idx, &r_count );
        if ( rc == 0 )
        {
            uint32_t r_idx;
            for ( r_idx = 0; r_idx < r_count && rc == 0; ++r_idx )
            {
                rc = run_stat_module_sub_report( ctx, m_list,
                            module_name, m_idx, r_idx, buffer );
            }
        }
        free( module_name );
    }
    return rc;
}


static rc_t run_stat_report( p_stat_ctx ctx, p_mod_list m_list )
{
    char_buffer buffer;
    rc_t rc = char_buffer_init( &buffer, 4096 ); /* preallocate */
    DISP_RC( rc, "run_stat_report:char_buffer_init(finalized) failed" );
    if ( rc == 0 )
    {
        uint32_t m_count;
        rc = mod_list_count( m_list, &m_count );
        if ( rc ==  0 )
        {
            uint32_t m_idx;
            for ( m_idx = 0; m_idx < m_count && rc == 0; ++m_idx )
            {
                rc = run_stat_module_report( ctx, m_list, m_idx, &buffer );
            }
        }
        /* free the collected output... */
        char_buffer_destroy( &buffer );
    }
    return rc;
}


static rc_t run_stat_main( p_stat_ctx ctx )
{
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    DISP_RC( rc, "run_stat_main:KDirectoryNativeDir() failed" );
    if ( rc == 0 )
    {
        const VDBManager *mgr;

#if TOOLS_USE_SRAPATH != 0
        if ( ctx->dont_check_accession == false )
        {
            rc_t rc1 = check_accession( ctx, dir );
            DISP_RC( rc1, "run_stat_main:check_accession() failed" );
        }
#endif

        rc = VDBManagerMakeRead ( &mgr, dir );
        DISP_RC( rc, "run_stat_main:VDBManagerMakeRead() failed" );
        if ( rc == 0 )
        {
            p_mod_list m_list;
            rc = mod_list_init( &m_list, ctx->module_list );
            if ( rc == 0 )
            {
                /********************************/
                rc = run_stat_collect_data( ctx, mgr, m_list );
                /********************************/

                if ( rc == 0 )
                    rc = run_stat_report( ctx, m_list );

                mod_list_destroy( m_list );
            }
            {
                rc_t rc1 = VDBManagerRelease( mgr );
                DISP_RC( rc1, "run_stat_main:VDBManagerRelease() failed" );
            }
        }
        {
            rc_t rc1 = KDirectoryRelease( dir );
            DISP_RC( rc1, "run_stat_main:KDirectoryRelease() failed" );
        }
    }
    return rc;
}


/* for KOutHandlerSet... */
static
rc_t CC write_to_FILE ( void *f, const char *buffer, size_t bytes, size_t *num_writ )
{
    * num_writ = fwrite ( buffer, 1, bytes, f );
    if ( * num_writ != bytes )
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;

    rc_t rc = KOutHandlerSet ( write_to_FILE, stdout );
    DISP_RC( rc, "KMain:KOutHandlerSet() failed" );
    if ( rc == 0 )
        rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
            StatOptions, sizeof StatOptions / sizeof StatOptions [ 0 ] );
    if ( rc == 0 )
    {
        stat_ctx *ctx;

        rc = ctx_init( &ctx );
        DISP_RC( rc, "KMain:ctx_init() failed" );

        if ( rc == 0 )
        {
            rc = ctx_capture_arguments_and_options( args, ctx );
            DISP_RC( rc, "KMain:ctx_capture_arguments_and_options() failed" );
            if ( rc == 0 )
            {
                if ( ctx->usage_requested )
                {
                    MiniUsage(args);
                }
                else
                {
                    KLogHandlerSetStdErr();
                    if ( ctx->module_list == NULL )
                    {
                        helper_make_namelist_from_string( &(ctx->module_list), 
                                        "bases,qualities", ',' );
                    }
                    if ( rc == 0 )
                    {
                        /************************/
                        rc = run_stat_main( ctx );
                        /************************/
                    }
                }
            }
            ctx_destroy( ctx );
        }
        ArgsWhack (args);
    }

    return rc;
}

