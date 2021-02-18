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

#include <kfs/file.h>

#define ITER_DONE 0x01
#define ITER_EOF  0x02

typedef struct line_iter
{
    const struct KFile * f;
    String buffer, content, line;
    uint64_t pos_in_file;
    uint32_t state;
} line_iter;


void release_line_iter( struct line_iter * iter )
{
    if ( NULL != iter )
    {
        if ( iter -> f != NULL )
        {
            rc_t rc = KFileRelease( iter -> f );
            if ( 0 != rc )
            {
                ErrMsg( "release_line_iter().KFileRelease() -> %R", filename, rc );
            }
        }
        if ( NULL != iter -> buffer . addr )
        {
            free( ( void * ) iter -> buffer . addr );
        }
        free( ( void * ) iter );
    }
}


static void read_line_iter( struct line_iter * iter )
{
    if ( iter->content.size > 0 )
    {
        memmove( (void *) iter -> buffer . addr, iter -> content . addr, iter -> content.size );
    }
    iter -> content.addr = iter -> buffer . addr;
    {
        rc_t rc;
        char * dst = ( char * )iter -> buffer . addr + iter -> content . size;
        size_t num_read;
        size_t to_read = ( ( iter -> buffer.size - 1 ) - iter -> content.size );
        rc = KFileRead ( iter -> f, iter -> pos_in_file, dst, iter -> buffer.size - iter -> content . size, &num_read );
        if ( 0 == rc )
        {
            iter -> pos_in_file += num_read;
            iter -> content.size += num_read;
            if ( num_read < to_read )
            {
                iter -> state |= ITER_EOF;
            }
        }
        else
        {
            iter -> state |= ITER_EOF;
        }
    }
}


static bool slice_iter_content( struct line_iter * iter, size_t by )
{
    size_t l;
    iter -> line . addr = iter -> content . addr;
    iter -> line . len  = by;
    iter -> line . size = by;
    l = ( by + 1 );
    iter -> content . addr += l;
    if ( l < iter -> content . size )
    {
        iter -> content . size -= l;
    }
    else
    {
        iter -> content . size = 0;
    }
    return true;
}


bool advance_line_iter( struct line_iter * iter )
{
    bool res = ( 0 == ( iter -> state & ITER_DONE ) );
    if ( res )
    {
        if ( 0 == iter -> content.size )
        {
            read_line_iter( iter );
        }
        if ( 0 == iter -> content . size && ( iter -> state & ITER_EOF ) )
        {
            iter -> state |= ITER_DONE;
            res = false;
        }
        else
        {
            char * newline = string_chr( iter -> content . addr, iter -> content . size, '\n' );
            if ( NULL == newline )
            {
                if ( iter -> state & ITER_EOF )
                {
                    res = slice_iter_content( iter, iter -> content . size );
                }
                else
                {
                    read_line_iter( iter );
                    res = advance_line_iter( iter ); /* recursion! */
                }
            }
            else
            {
                res = slice_iter_content( iter, newline - iter -> content . addr );
            }
        }
    }
    return res;
}


String * get_line_iter( struct line_iter * iter )
{
    String * res = NULL;
    if ( NULL != iter )
    {
        if ( 0 == ( iter -> state & ITER_DONE ) )
        {
            res = &( iter -> line );
        }
    }
    return res;
}


bool is_line_iter_done( const struct line_iter * iter )
{
    if ( NULL != iter )
    {
        return ( iter -> state & ITER_DONE );
    }
    return true;
}


rc_t make_line_iter( const KDirectory *dir, line_iter ** iter,
                     const char * filename, size_t buffer_size )
{
    const struct KFile * f;
    rc_t rc = KDirectoryOpenFileRead( dir, &f, "%s", filename );
    if ( 0 != rc )
    {
        ErrMsg( "make_line_iter().KDirectoryOpenFileRead( '%s' ) -> %R", filename, rc );
    }
    else
    {
        line_iter * l = calloc( 1, sizeof * l );
        if ( NULL == l )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "calloc( %d ) -> %R", ( sizeof * l ), rc );
            {
                rc_t rc2 = KFileRelease( f );
                if ( 0 != rc2 )
                {
                    ErrMsg( "make_line_iter().KFileRelease().1 -> %R", rc2 );
                }
            }
        }
        else
        {
            l -> f = f;
            l -> buffer.addr = malloc( buffer_size );
            if ( NULL == l -> buffer . addr )
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                ErrMsg( "malloc( %d ) -> %R", ( buffer_size ), rc );
                {
                    rc_t rc2 = KFileRelease( f );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "make_line_iter().KFileRelease().2 -> %R", rc2 );
                    }
                }
                free( ( void * ) l );
            }
            else
            {
                l -> buffer . size = buffer_size;
                l -> buffer . len = buffer_size;
                l -> content . addr = l -> buffer . addr;
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
    }
    return rc;
}
