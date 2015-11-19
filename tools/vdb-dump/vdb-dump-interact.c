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
#include "vdb-dump-helper.h"
#include <kfs/file.h>

rc_t Quitting();

typedef struct ictx
{
    const dump_context * ctx;
    const Args * args;
    const KFile * std_in;
    bool interactive, done;
    
    String cmd;
    String S_PROMPT, S_QUIT, S_EXIT;
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
        
        CONST_STRING( &(ictx->cmd), "." );
        CONST_STRING( &(ictx->S_PROMPT), "\nvdb $" );
        CONST_STRING( &(ictx->S_QUIT), "quit" );
        CONST_STRING( &(ictx->S_EXIT), "exit" );
    }
    return rc;
}


static void release_ictx( ictx * ctx )
{
    KFileRelease( ctx->std_in );
}


static rc_t vdi_handle( ictx * ctx )
{
    rc_t rc = 0;
    
    bool is_quit = ( StringCaseCompare ( &( ctx->cmd ), &( ctx->S_QUIT ) ) == 0 );
    bool is_exit = ( StringCaseCompare ( &( ctx->cmd ), &( ctx->S_EXIT ) ) == 0 );
    ctx->done = ( is_quit || is_exit );

    if ( !ctx->done )
    {
        rc = KOutMsg( "{%S}%S", &( ctx->cmd ), &( ctx->S_PROMPT ) );
    }
    return rc;
}


static rc_t vdi_interactive_loop( ictx * ctx )
{
    char one_char[ 4 ], buffer[ 4096 ];
    uint64_t pos = 0;
    size_t buffer_idx = 0;
    rc_t rc = KOutMsg( "interactive mode%S", &( ctx->S_PROMPT ) );    

    while ( rc == 0 && !ctx->done )
    {
        rc = Quitting();
        if ( rc == 0 )
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
                    buffer[ buffer_idx ] = 0;
                    StringInit( &(ctx->cmd), buffer, buffer_idx, buffer_idx );
                    vdi_handle( ctx );
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
                        rc = KOutMsg( "\ntoo long!%s", &( ctx->S_PROMPT ) );
                        buffer_idx = 0;
                    }
                }
            }
        }
    }
    KOutMsg( "\n" );
    return rc;
}


rc_t vdi_batch_loop( struct ictx * ictx )
{
    KOutMsg( "batch loop\n" );
    return 0;
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
            rc = vdi_batch_loop( &ictx );
            
        release_ictx( &ictx );
    }
    return rc;
}