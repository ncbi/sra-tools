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

#ifndef _h_kapp_main_
#include <kapp/main.h>
#endif

#ifndef _h_kapp_args_
#include <kapp/args.h>
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_directory_
#include <kfs/directory.h>
#endif

#include <os-native.h>
#include <sysalloc.h>

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_logger_
#include "logger.h"
#endif

#ifndef _h_result_
#include "result.h"
#endif

#ifndef _h_thread_runner_
#include "thread-runner.h"
#endif

#ifndef _h_progress_
#include "progress.h"
#endif

#ifndef _h_cmn_iter_
#include "cmn-iter.h"
#endif

#ifndef _h_csra_validator_
#include "csra-validator.h"
#endif

static const char * threads_usage[] = { "how many thread ( default=1 )", NULL };
#define OPTION_THREADS  "threads"
#define ALIAS_THREADS   "e"

static const char * detail_usage[] = { "print details", NULL };
#define OPTION_DETAILS  "details"
#define ALIAS_DETAILS    "x"

OptDef ToolOptions[] =
{
    { OPTION_THREADS,   ALIAS_THREADS,   NULL, threads_usage,    1, true,   false },
    { OPTION_DETAILS,   ALIAS_DETAILS,   NULL, detail_usage,     1, false,  false }
};

const char UsageDefaultName[] = "sra-validate";

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

typedef struct validate_ctx
{
    KDirectory * dir;
    const char accession;
    acc_info_t acc_info;
    struct logger * log;
    struct validate_result * v_res;
    struct thread_runner * threads;
    struct progress * progress;
    size_t cursor_cache;
    uint32_t num_slices;
} validate_ctx;

static rc_t clear_validate_context( validate_ctx * val_ctx )
{
    rc_t rc = 0;
    
    terminate_progress( val_ctx -> progress );
        
    if ( val_ctx -> threads != NULL )
    {
        rc = destroy_thread_runner( val_ctx -> threads );
        if ( rc != 0 )
            ErrMsg( "clear_validate_context().destroy_thread_runner() -> %R", rc );
    }
    
    if ( val_ctx -> v_res != NULL )
    {
        rc_t rc1 = print_validate_result( val_ctx -> v_res, val_ctx -> log );
        if ( rc == 0 )
            rc = rc1;
        destroy_validate_result( val_ctx -> v_res );
    }
    
    if ( val_ctx -> log != NULL )
        destroy_logger( val_ctx -> log );

    if ( val_ctx -> dir != NULL )
        KDirectoryRelease( val_ctx -> dir );

    return rc;
}

static rc_t init_validate_context( Args * args, validate_ctx * val_ctx )
{
    rc_t rc = 0;
    const char * accession;
    
    memset( val_ctx, 0, sizeof * val_ctx );
    rc = ArgsParamValue( args, 0, (const void **)&accession );
    if ( rc != 0 )
        ErrMsg( "ArgsParamValue() -> %R", rc );
        
    val_ctx -> num_slices = 6;
    val_ctx -> cursor_cache = 1024 * 1024 * 32;

    if ( rc == 0 )
    {
        rc = KDirectoryNativeDir( & val_ctx -> dir );
        if ( rc != 0 )
            ErrMsg( "sra-validate.c init_validate_context().KDirectoryNativeDir() -> %R", rc );
    }

    if ( rc == 0 )
        rc = make_logger( val_ctx -> dir, &val_ctx -> log, NULL );
    
    if ( rc == 0 )
        rc = make_validate_result( &val_ctx -> v_res );

    if ( rc == 0 )
        rc = make_thread_runner( &val_ctx -> threads );

    if ( rc == 0 )
        rc = make_progress( val_ctx -> threads, &val_ctx -> progress, 20 /* ms sleep time for updates */ );
        
    if ( rc == 0 )
        rc = cmn_get_acc_info( val_ctx -> dir, accession, &val_ctx -> acc_info ); /* cmn-iter.c */
        
    if ( rc != 0 )
        clear_validate_context( val_ctx );

    return rc;
}

static rc_t sra_validate_csra( validate_ctx * val_ctx )
{
    rc_t rc = log_write( val_ctx -> log,
                         "validating as CSRA ( PLATFORM = %s )",
                         platform_2_text( val_ctx -> acc_info . platform ) );
    if ( rc == 0 )
        rc = run_csra_validator( val_ctx -> dir,
                                 & val_ctx -> acc_info,
                                 val_ctx -> log,
                                 val_ctx -> v_res,
                                 val_ctx -> threads,
                                 val_ctx -> progress,
                                 val_ctx -> num_slices,
                                 val_ctx -> cursor_cache );
    return rc;
}

static rc_t sra_validate_sra_flat( validate_ctx * val_ctx )
{
    rc_t rc = log_write( val_ctx -> log,
                        "validating as flat SRA ( PLATFORM = %s )",
                        platform_2_text( val_ctx -> acc_info . platform ) );
    return rc;
}

static rc_t sra_validate_sra_db( validate_ctx * val_ctx )
{
    rc_t rc = log_write( val_ctx -> log,
                         "validating as db-SRA ( PLATFORM = %s )",
                         platform_2_text( val_ctx -> acc_info . platform ) );
    return rc;
}

static rc_t sra_validate_pacbio( validate_ctx * val_ctx )
{
    rc_t rc = log_write( val_ctx -> log,
                         "validating as PACBIO ( PLATFORM = %s )",
                         platform_2_text( val_ctx -> acc_info . platform ) );
    return rc;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    else
    {
        validate_ctx val_ctx;
        rc = init_validate_context( args, &val_ctx );
        if ( rc == 0 )
        {

            switch( val_ctx . acc_info . acc_type )
            {
                case acc_csra       : rc = sra_validate_csra( &val_ctx ); break;
                case acc_sra_flat   : rc = sra_validate_sra_flat( &val_ctx ); break;
                case acc_sra_db     : rc = sra_validate_sra_db( &val_ctx ); break;
                case acc_pacbio     : rc = sra_validate_pacbio( &val_ctx ); break;            
                case acc_none       : rc = log_write( val_ctx . log,
                                                "'%s' cannot be validated",
                                                val_ctx . acc_info . accession ); break;
            }
            
            {
                rc_t rc1 = clear_validate_context( &val_ctx );
                if ( rc == 0 )
                    rc = rc1;
            }
        }
    }
    return rc;
}
