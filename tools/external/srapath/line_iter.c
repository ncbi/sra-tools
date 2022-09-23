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

#include "line_iter.h"
#include "helper.h"

#include <kapp/main.h> /* Quitting */
#include <kfs/file.h>
#include <klib/out.h>
#include <klib/time.h>
#include <kproc/timeout.h>

#include "os-native.h"

#define ITER_DONE 0x01
#define ITER_EOF  0x02

typedef struct line_iter
{
    const struct KStream * stream;
    String buffer, content, line;
    uint32_t state, timeout_ms;
} line_iter;


void release_line_iter( struct line_iter * iter )
{
    if ( iter != NULL )
    {
        if ( iter->buffer.addr != NULL )
            free( ( void * ) iter->buffer.addr );
        free( ( void * ) iter );
    }
}


static uint32_t read_line_iter( struct line_iter * iter )
{
    uint32_t res = 0;
    if ( iter->content.size > 0 )
        memmove( (void *)iter->buffer.addr, iter->content.addr, iter->content.size );
    iter->content.addr = iter->buffer.addr;
    {
        char * dst = ( char * )iter->buffer.addr + iter->content.size;
        size_t num_read;
        size_t to_read = ( ( iter->buffer.size - 1 ) - iter->content.size );
        timeout_t tm;
        
        rc_t rc = TimeoutInit( &tm, iter->timeout_ms );
        if ( rc != 0 )
            ErrMsg( "TimeoutInit( 5000 ) -> %R", rc );
        else
        {
            rc = TimeoutPrepare( &tm );
            if ( rc != 0 )
                ErrMsg( "TimeoutPrepare() -> %R", rc );
            else
            {
                rc = KStreamTimedRead( iter->stream, dst, iter->buffer.size - iter->content.size, &num_read, &tm );
                if ( rc == 0 )
                {
                    iter->content.size += num_read;
                    res = num_read;
                    if ( num_read < to_read )
                        iter->state |= ITER_EOF;
                    else
                        iter->state &= ~ITER_EOF;
                }
                else
                {
                    KOutMsg( "KStreamTimedRead() = %R, (%d)\n", rc, num_read );
                    iter->state |= ITER_DONE;
                }
            }
        }
    }
    return res;
}


static bool slice_iter_content( struct line_iter * iter, size_t by )
{
    size_t l;                
    iter->line.addr = iter->content.addr;
    iter->line.len  = by;
    iter->line.size = by;
    l = ( by + 1 );
    iter->content.addr += l;
    if ( l < iter->content.size )
        iter->content.size -= l;
    else
        iter->content.size = 0;
    return true;
}


bool advance_line_iter( struct line_iter * iter )
{
    bool res = ( 0 == ( iter->state & ITER_DONE ) );
    if ( res )
    {
        if ( iter->content.size == 0 )
            read_line_iter( iter );

        if ( iter->content.size == 0 && ( iter->state & ITER_EOF ) )
        {
            iter->state |= ITER_DONE;
            res = false;
        }
        else
        {
            char * newline = string_chr( iter->content.addr, iter->content.size, '\n' );
            if ( newline == NULL )
            {
                if ( read_line_iter( iter ) == 0 )
                    KSleepMs( 100 );
                if ( Quitting () )
                    res = false;
                else
                    res = advance_line_iter( iter ); /* recursion! */
            }
            else
                res = slice_iter_content( iter, newline - iter->content.addr );
        }
    }
    return res;
}


String * get_line_iter( struct line_iter * iter )
{
    String * res = NULL;
    if ( iter != NULL )
    {
        if ( 0 == ( iter->state & ITER_DONE ) )
            res = &iter->line;
    }
    return res;
}


bool is_line_iter_done( const struct line_iter * iter )
{
    if ( iter != NULL )
        return ( iter->state & ITER_DONE );
    return true;
}


rc_t make_line_iter( struct line_iter ** iter,
                     const struct KStream * stream,
                     size_t buffer_size,
                     uint32_t timeout_ms )
{
    rc_t rc = 0;
    line_iter * l = calloc( 1, sizeof * l );
    if ( l == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "calloc( %d ) -> %R", ( sizeof * l ), rc );
    }
    else
    {
        l->stream = stream;
        l->buffer.addr = malloc( buffer_size );
        if ( l->buffer.addr == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "malloc( %d ) -> %R", ( buffer_size ), rc );
            free( ( void * ) l );
        }
        else
        {
            l->buffer.size = buffer_size;
            l->buffer.len = buffer_size;
            l->content.addr = l->buffer.addr;
            l->timeout_ms = timeout_ms;
            read_line_iter( l );
            if ( advance_line_iter( l ) )
            {
                *iter = l;
            }
            else
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcRange, rcInvalid );
                release_line_iter( l );
            }
        }
    }
    return rc;
}


rc_t stream_line_read( const struct KStream * stream, stream_line_handler_t on_line,
                       size_t buffer_size, uint32_t timeout_ms, void * data )
{
    struct line_iter * iter;
    rc_t rc = make_line_iter( &iter, stream, buffer_size, timeout_ms );
    if ( rc == 0 )
    {
        while ( rc == 0 && !is_line_iter_done( iter ) )
        {
            String * line = get_line_iter( iter );
            rc = on_line( line, data );
            advance_line_iter( iter );
            if ( rc == 0 )
                rc = Quitting ();
        }
        release_line_iter( iter );
    }
    return rc;
}


static rc_t print_stream_line( const String * line, void * data )
{
    return KOutMsg( "%S\n", line );
}

rc_t print_stream( const struct KStream * stream, size_t buffer_size,
        uint32_t timeout_ms, void * data )
{
    return stream_line_read( stream, print_stream_line, buffer_size, timeout_ms, data );
}
