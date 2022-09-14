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

#include "vdb-diff-context.h"

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/status.h>
#include <klib/text.h>
#include <kapp/args.h>

#include <os-native.h>
#include <sysalloc.h>

#include <strtol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

void init_diff_ctx( struct diff_ctx * dctx )
{
    dctx -> src1 = NULL;
    dctx -> src2 = NULL;
	dctx -> columns = NULL;
	dctx -> excluded = NULL;	
	dctx -> table = NULL;
	
    dctx -> rows = NULL;
	dctx -> show_progress = false;
	dctx -> intersect = false;
    dctx -> columnwise = false;
}

void release_diff_ctx( struct diff_ctx * dctx )
{
	/* do not release the src1,src2,columns,table pointers! they are owned by the Args-obj. */
	if ( dctx -> rows != 0 )
	{
		num_gen_destroy( dctx -> rows );
	}
}

static const char * get_str_option( const Args *args, const char * option_name )
{
    const char * res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsOptionCount() failed" );
	}
	else if ( count > 0 )
	{
		rc = ArgsOptionValue( args, option_name, 0, (const void **)&res );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "ArgsOptionValue( 0 ) failed" );
		}
	}
    return res;
}

static bool get_bool_option( const Args *args, const char * option_name, bool dflt )
{
    bool res = dflt;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsOptionCount() failed" );
	}
	else if ( count > 0 )
	{
		res = true;
	}
    return res;
}

static uint32_t get_uint32t_option( const Args *args, const char * option_name, uint32_t dflt )
{
    uint32_t res = dflt;
	const char * s = get_str_option( args, option_name );
	if ( s != NULL ) res = atoi( s );
    return res;
}

rc_t gather_diff_ctx( struct diff_ctx * dctx, Args * args )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsParamCount() failed" );
	}
    else if ( count < 2 )
    {
        rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcIncorrect );
		LOGERR ( klogInt, rc, "This tool needs 2 arguments: source1 and source2" );
    }
    else
    {
        rc = ArgsParamValue( args, 0, (const void **)&dctx->src1 );
        if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "ArgsParamValue( 1 ) failed" );
		}
        else
        {
            rc = ArgsParamValue( args, 1, (const void **)&dctx->src2 );
            if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "ArgsParamValue( 2 ) failed" );
			}
        }
    }

    if ( rc == 0 )
    {
        const char * s = get_str_option( args, OPTION_ROWS );
        if ( s != NULL )
		{
            rc = num_gen_make_from_str( &dctx->rows, s );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "num_gen_make_from_str() failed" );
			}
		}
	}
	
	if ( rc == 0 )
	{
        dctx -> table = get_str_option( args, OPTION_TABLE );
		dctx -> columns = get_str_option( args, OPTION_COLUMNS );
		dctx -> excluded = get_str_option( args, OPTION_EXCLUDE );		
		dctx -> show_progress = get_bool_option( args, OPTION_PROGRESS, false );
		dctx -> intersect = get_bool_option( args, OPTION_INTERSECT, false );
		dctx -> max_err = get_uint32t_option( args, OPTION_MAXERR, 1 );
        dctx -> columnwise = get_bool_option( args, OPTION_COLUMNWISE, false );
    }

    return rc;
}

rc_t report_diff_ctx( struct diff_ctx * dctx )
{
    rc_t rc = KOutMsg( "src[ 1 ] : %s\n", dctx -> src1 );
	if ( rc == 0 )
		rc = KOutMsg( "src[ 2 ] : %s\n", dctx -> src2 );

	if ( rc == 0 )
	{
		if ( dctx -> rows != NULL )
		{
			char buffer[ 4096 ];
			rc = num_gen_as_string( dctx -> rows, buffer, sizeof buffer, NULL, false );
			if ( rc == 0 )
				rc = KOutMsg( "- rows : %s\n", buffer );
		}
		else
			rc = KOutMsg( "- rows : all\n" );
	}
		
	if ( rc == 0 && dctx -> columns != NULL )
		rc = KOutMsg( "- columns : %s\n", dctx -> columns );
	if ( rc == 0 && dctx -> excluded != NULL )
		rc = KOutMsg( "- exclude : %s\n", dctx -> excluded );
	if ( rc == 0 && dctx -> table != NULL )
		rc = KOutMsg( "- table : %s\n", dctx -> table );
	if ( rc == 0 )
		rc = KOutMsg( "- progress : %s\n", dctx -> show_progress ? "show" : "hide" );
	if ( rc == 0 )
		rc = KOutMsg( "- intersect: %s\n", dctx -> intersect ? "yes" : "no" );
	if ( rc == 0 )
		rc = KOutMsg( "- max err : %u\n", dctx -> max_err );
	if ( rc == 0 )
		rc = KOutMsg( "- col-by-col: %s\n", dctx -> columnwise ? "yes" : "no" );

	if ( rc == 0 )
		rc = KOutMsg( "\n" );
	return rc;
}
