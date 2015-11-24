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

#include "vdb-dump-interact.h"
#include "vdb-dump-repo.h"
#include "vdb-dump-helper.h"

#include <klib/vector.h>
#include <kfs/file.h>
#include <kfs/filetools.h>

rc_t Quitting();

typedef struct ictx
{
    const dump_context * ctx;
    const Args * args;
    const KFile * std_in;
    bool interactive, done;
    
    String PROMPT;
} ictx;


static rc_t init_ictx( struct ictx * ictx, const dump_context * ctx, const Args * args )
{
    rc_t rc = KFileMakeStdIn ( &( ictx->std_in ) );
    DISP_RC( rc, "KFileMakeStdIn() failed" );
    if ( rc == 0 )
    {
        ictx->ctx = ctx;
        ictx->args = args;
        ictx->interactive = ( KFileType ( ictx->std_in ) == kfdCharDev );
        ictx->done = false;
        
        CONST_STRING( &(ictx->PROMPT), "\nvdb $" );
    }
    return rc;
}


static void release_ictx( ictx * ctx )
{
    KFileRelease( ctx->std_in );
}


static rc_t vdi_test( const Vector * v )
{
    rc_t rc = 0;
    uint32_t start = VectorStart( v );
    uint32_t len = VectorLength( v );
    uint32_t idx;
    for ( idx = start; rc == 0 && idx < ( start + len ); ++idx )
        rc = KOutMsg( "{%S} ", VectorGet( v, idx ) );
    return rc;
}


static rc_t vdi_top_help()
{
    rc_t rc = KOutMsg( "help:\n" );
    if ( rc == 0 )
        rc = KOutMsg( "quit / exit : terminate program\n" );
    if ( rc == 0 )
        rc = KOutMsg( "help [cmd]  : print help [ for this topic ]\n" );
    if ( rc == 0 )
        rc = KOutMsg( "repo        : manage repositories\n" );
    return rc;
}


static rc_t vdi_help_on_help()
{
    rc_t rc = KOutMsg( "help: [help]\n" );
    return rc;
}

static rc_t vdi_help_on_repo()
{
    rc_t rc = KOutMsg( "help: [repo]\n" );
    return rc;
}

static rc_t vdi_help( const Vector * v )
{
    rc_t rc = 0;
    if ( VectorLength( v ) < 2 )
        rc = vdi_top_help();
    else
    {
        int32_t cmd_idx = index_of_match( VectorGet( v, 1 ), 2,
            "help", "repo" );
        switch( cmd_idx )
        {
            case 0 : rc = vdi_help_on_help(); break;
            case 1 : rc = vdi_help_on_repo(); break;
        }
    }
    return rc;
}


static rc_t vdi_handle_buffer( ictx * ctx, const char * buffer, size_t buffer_len )
{
    rc_t rc = 0;
    Vector v;
    uint32_t args = split_buffer( &v, buffer, buffer_len, " \t" ); /* from vdb-dump-helper.c */
    if ( args > 0 )
    {
        const String * S = VectorGet( &v, 0 );
        if ( S != NULL )
        {
            int32_t cmd_idx = index_of_match( S, 6,
                "quit", "exit", "help", "repo", "test" );
                
            ctx->done = ( cmd_idx == 0 || cmd_idx == 1 );
            if ( !ctx->done )
            {
                switch( cmd_idx )
                {
                    case 2  : rc = vdi_help( &v ); break;
                    case 3  : rc = vdi_repo( &v ); break;
                    case 4  : rc = vdi_test( &v ); break;
                    default : rc = KOutMsg( "??? {%S}", S ); break;
                }
                if ( rc == 0 )
                {
                    if ( ctx->interactive )
                        rc = KOutMsg( "%S", &( ctx->PROMPT ) );
                    else
                        rc = KOutMsg( "\n" );
                }
            }
        }
    }
    destroy_String_vector( &v ); /* from vdb-dump-helper.c */
    return rc;
}


static rc_t vdi_interactive_loop( ictx * ctx )
{
    char one_char[ 4 ], buffer[ 4096 ];
    uint64_t pos = 0;
    size_t buffer_idx = 0;
    rc_t rc = KOutMsg( "%S", &( ctx->PROMPT ) );    

    while ( rc == 0 && !ctx->done && ( 0 == Quitting() ) )
    {
        size_t num_read;
        rc = KFileRead( ctx->std_in, pos, one_char, 1, &num_read );
        if ( rc != 0 )
            LOGERR ( klogErr, rc, "failed to read stdin" );
        else
        {
            pos += num_read;
            if ( one_char[ 0 ] == '\n' )
            {
                rc = vdi_handle_buffer( ctx, buffer, buffer_idx ); /* <---- */
                buffer_idx = 0;
            }
            else
            {
                if ( buffer_idx < ( ( sizeof buffer ) - 1 ) )
                {
                    buffer[ buffer_idx++ ] = one_char[ 0 ];
                }
                else
                {
                    rc = KOutMsg( "\ntoo long!%s", &( ctx->PROMPT ) );
                    buffer_idx = 0;
                }
            }
        }
    }
    KOutMsg( "\n" );
    return rc;
}


static rc_t on_line( const String * line, void * data )
{
    ictx * ctx = data;
    rc_t rc = Quitting();
    if ( rc == 0 ) rc = vdi_handle_buffer( ctx, line->addr, line->size );
    return rc;
}


rc_t vdi_main( const dump_context * ctx, const Args * args )
{
    ictx ictx;
    rc_t rc = init_ictx( &ictx, ctx, args );
    if ( rc == 0 )
    {
        if ( ictx.interactive )
            rc = vdi_interactive_loop( &ictx );
        else
            rc = ProcessFileLineByLine( ictx.std_in, on_line, &ictx ); /* from kfs/filetools.h */
            
        release_ictx( &ictx );
    }
    return rc;
}