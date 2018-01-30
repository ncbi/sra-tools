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

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_klib_data_buffer_
#include <klib/data-buffer.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

#ifndef _h_klib_directory_
#include <kfs/directory.h>
#endif

#include <os-native.h>
#include <sysalloc.h>

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

static rc_t ErrMsg( const char * fmt, ... )
{
    KDataBuffer data_buffer;
    rc_t rc = KDataBufferMakeBytes( &data_buffer, 1024 );
    if ( rc == 0 )
    {
        va_list list;
        
        va_start( list, fmt );
        rc = KDataBufferVPrintf ( &data_buffer, fmt, list );
        if ( rc == 0 )
            rc = pLogMsg( klogErr, "$(E)", "E=%s", data_buffer . base );
        va_end( list );
    
        KDataBufferWhack( &data_buffer );
    }
    return rc;
} 

static rc_t sra_validate( const char * accession )
{
    return ErrMsg( "validating: %s\n", accession );
}

rc_t CC KMain ( int argc, char *argv [] )
{
    rc_t rc;
    Args * args;
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];

    rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
    if ( rc == 0 )
    {
        const char * accession;
        rc = ArgsParamValue( args, 0, (const void **)&accession );
        if ( rc != 0 )
            ErrMsg( "ArgsParamValue() -> %R", rc );
        else
            rc = sra_validate( accession );
    }
    return rc;
}
