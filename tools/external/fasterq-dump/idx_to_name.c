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
#include "idx_to_name.h"
#include <string.h>

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

/* ------------------------------------------------------------------------------ */

typedef struct idx_to_name_item_t {
    uint32_t idx;
    char * name;
} idx_to_name_item_t;

static void idx_to_name_item_release( SLNode *node, void * data ) {
    if ( NULL != node ) {
        idx_to_name_item_t *item  = ( idx_to_name_item_t * )node;
        if ( NULL != item -> name ) {
            free( item -> name );
            item -> name = NULL;
            item -> idx = 0;
        }
    }
}

static SLNode * idx_to_name_item_make( const char * name, uint32_t idx ) {
    idx_to_name_item_t * item = calloc( 1, sizeof * item );
    if ( NULL != item ) {
        item -> idx = idx;
        if ( NULL != name ) {
            size_t num_bytes = string_size( name );
            if ( num_bytes > 0 ) {
                item -> name = string_dup( name, num_bytes );
            }
        }
    }
    return ( SLNode * )item;
}

typedef struct idx_to_name_check_ctx_t {
    uint32_t idx;
    const char * name;
} idx_to_name_check_ctx_t;

static bool idx_to_name_check( SLNode * node, void * data ) {
    if ( NULL != node && NULL != data ) {
        const idx_to_name_item_t *item  = ( idx_to_name_item_t * )node;
        idx_to_name_check_ctx_t *ctx = data;
        if ( ctx -> idx == item -> idx ) {
            ctx -> name = item -> name;
            return true;
        }
    }
    return false;
}

/* ------------------------------------------------------------------------------ */

void idx_to_name_init( idx_to_name_t * self ) {
    SLListInit( &( self -> items ) );
}

void idx_to_name_release( idx_to_name_t * self ) {
    SLListWhack( &( self -> items ), idx_to_name_item_release, NULL );
}

void idx_to_name_add( struct idx_to_name_t * self, const char * name, uint32_t idx ) {
    SLNode * node = idx_to_name_item_make( name, idx );
    if ( NULL != node ) { SLListPushTail( &( self -> items ), node ); }
}

const char * idx_to_name_get( const struct idx_to_name_t * self, uint32_t idx ) {
    idx_to_name_check_ctx_t ctx = { .idx = idx, .name = NULL };
    /* linear search used here, because the list is small, no need for a search-tree */
    if ( SLListDoUntil( &( self -> items ),  idx_to_name_check, &ctx ) ) {
        if ( NULL != ctx . name ) {
            return ctx . name;
        }
    }
    return NULL;
}
