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

#include <os-native.h>
#include <sysalloc.h>

#include "line_token_iter.h"
#include <klib/log.h>
#include <klib/out.h>

#include <stdlib.h>

/* ---------------------------------------------------------------------------- */
/* >>>>> buf line iter <<<<< */

rc_t buf_line_iter_init( struct buf_line_iter * self, const char * buffer, size_t len )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
    {
        /* we do accept null buffer, empty len, we just do not hand out lines later */
        self->nxt = buffer;
        self->len = len;
        self->line_nr = 0;
    }
    return rc;
}


rc_t buf_line_iter_get( struct buf_line_iter * self, String * line, bool * valid, uint32_t * line_nr )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else if ( line == NULL || valid == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        *valid = ( self->nxt != NULL && self->len > 0 );
        if ( *valid )
        {
            char * nl = string_chr ( self->nxt, self->len, '\n' );
            if ( nl == NULL )
            {
                /* no newline found... */
                StringInit( line, self->nxt, self->len, self->len );
                self->len = 0;  /* next call will return *valid == false */
            }
            else
            {
                /* we found a newline... */
                size_t l = ( nl - self->nxt );
                StringInit( line, self->nxt, l, l );
                self->len -= ( l + 1 );
                if ( self->len > 0 )
                    self->nxt += ( l + 1 );
            }
            if ( line_nr != NULL )
                *line_nr = self->line_nr++;
        }
    }
    return rc;
}


/* ---------------------------------------------------------------------------- */
/* >>>>> token iter <<<<< */

rc_t token_iter_init( struct token_iter * self, const String * line, char delim )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
    {
        /* we do accept null buffer, empty len, we just do not hand out lines later */
        StringInit( &self->line, line->addr, line->size, line->len );
        self->delim = delim;
        self->token_nr = 0;
        self->idx = 0;
    }
    return rc;
}


rc_t token_iter_get( struct token_iter * self, String * token, bool * valid, uint32_t * token_nr )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else if ( token == NULL || valid == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        while ( ( self->idx < self->line.len ) && ( self->line.addr[ self->idx ] == self->delim ) )
            self->idx++;
        *valid = ( self->idx < self->line.len );
        if ( *valid )
        {
            char * end;
            size_t l = ( self->line.len - self->idx );
            token->addr = &( self->line.addr[ self->idx ] );
            end = string_chr ( token->addr, l, self->delim );
            if ( end == NULL )
            {
                token->size = token->len = l;
                self->idx = self->line.len;
            }
            else
            {
                token->size = token->len = ( end - token->addr );
                self->idx = ( end - self->line.addr );
            }
            if ( token_nr != NULL )
                *token_nr = self->token_nr++;
        }
    }
    return rc;
}
