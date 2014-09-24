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
#include "ref_regions.h"

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/vector.h>
#include <klib/container.h>

#include <stdlib.h>

#include <os-native.h>
#include <sysalloc.h>
#include <strtol.h>

/* =========================================================================================== */


static int cmp_pchar( const char * a, const char * b )
{
    int res = 0;
    if ( ( a != NULL )&&( b != NULL ) )
    {
        size_t len_a = string_size( a );
        size_t len_b = string_size( b );
        res = string_cmp ( a, len_a, b, len_b, ( len_a < len_b ) ? len_b : len_a );
    }
    return res;
}


struct skip_range
{
    uint64_t start;
    uint64_t end;
} skip_range;


static struct skip_range * make_skip_range( const uint64_t start, const uint64_t end )
{
    struct skip_range *res = calloc( 1, sizeof *res );
    if ( res != NULL )
    {
        res->start = start;
        res->end = end;
    }
    return res;
}


/* =========================================================================================== */

struct reference_range
{
    uint64_t start;
    uint64_t end;
    Vector skip;
} reference_range;


static struct reference_range * make_range( const uint64_t start, const uint64_t end )
{
    struct reference_range *res = calloc( 1, sizeof *res );
    if ( res != NULL )
    {
        res->start = start;
        res->end = end;
        VectorInit ( &res->skip, 0, 5 );
    }
    return res;
}


static void CC release_skip( void * item, void * data ) { free( item ); }

static void free_range( struct reference_range * self )
{
    VectorWhack ( &self->skip, release_skip, NULL );
    free( self );
}

static int cmp_range( const struct reference_range * a, const struct reference_range * b )
{

    int64_t res = ( a->start - b->start );
    if ( res == 0 )
        res = ( a->end - b->end );
    if ( res < 0 )
        return -1;
    else if ( res >  0 )
        return 1;
    else return 0;
}


static bool range_overlapp( const struct reference_range * a, const struct reference_range * b )
{
    return ( !( ( b->end < a->start ) || ( b->start > a->end ) ) );
}


static uint64_t range_distance( const struct reference_range * a, const struct reference_range * b )
{
    return ( b->start - a->end );
}

/* =========================================================================================== */

struct reference_region
{
    BSTNode node;
    const char * name;
    Vector ranges;
} reference_region;


static struct reference_region * make_reference_region( const char *name )
{
    struct reference_region *res = calloc( sizeof *res, 1 );
    if ( res != NULL )
    {
        res->name = string_dup_measure ( name, NULL );
        VectorInit ( &res->ranges, 0, 5 );
    }
    return res;
}


static int CC cmp_range_wrapper( const void *item, const void *n )
{   return cmp_range( item, n ); }


static rc_t add_ref_region_range( struct reference_region * self, const uint64_t start, const uint64_t end )
{
    rc_t rc = 0;
    struct reference_range *r = make_range( start, end );
    if ( r == NULL )
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        rc = VectorInsert ( &self->ranges, r, NULL, cmp_range_wrapper );
        if ( rc != 0 )
            free( r );
    }
    return rc;
}


#define RR_NAME  1
#define RR_START 2
#define RR_END   3


static void put_c( char *s, size_t size, size_t *dst, char c )
{
    if ( *dst < ( size - 1 ) )
        s[ *dst ] = c;
    (*dst)++;
}

static void finish_txt( char *s, size_t size, size_t *dst )
{
    if ( *dst > size )
        s[ size - 1 ] = 0;
    else
        s[ *dst ] = 0;
    *dst = 0;
}

static uint64_t finish_num( char *s, size_t size, size_t *dst )
{
    uint64_t res = 0;
    char *endp;
    finish_txt( s, size, dst );
    res = strtou64( s, &endp, 10 );
    return res;
}


/* s = refname:1000-2000 */
static void parse_definition( const char *s, char * name, size_t len,
                              uint64_t *start, uint64_t *end )
{
    size_t n = string_size( s );

    *start = 0;
    *end   = 0;
    name[ 0 ] = 0;
    if ( n > 0 )
    {
        size_t i, st, dst = 0;
        char tmp[ 32 ];
        st = RR_NAME;
        for ( i = 0; i < n; ++i )
        {
            char c = s[ i ];
            switch( st )
            {
                case RR_NAME  : if ( c == ':' )
                                {
                                    finish_txt( name, len, &dst );
                                    st = RR_START;
                                }
                                else
                                {
                                    put_c( name, len, &dst, c );
                                }
                                break;

                case RR_START : if ( c == '-' )
                                {
                                    *start = finish_num( tmp, sizeof tmp, &dst );
                                    st = RR_END;
                                }
                                else if ( ( c >= '0' )&&( c <= '9' ) )
                                {
                                    put_c( tmp, sizeof tmp, &dst, c );
                                }
                                break;

                case RR_END   : if ( ( c >= '0' )&&( c <= '9' ) )
                                {
                                    put_c( tmp, sizeof tmp, &dst, c );
                                }
                                break;
            }
        }
        switch( st )
        {
            case RR_NAME  : finish_txt( name, len, &dst );
                            break;

            case RR_START : *start = finish_num( tmp, sizeof tmp, &dst );
                            break;

            case RR_END   : *end = finish_num( tmp, sizeof tmp, &dst );
                            break;
        }
    }
}


static void CC release_ranges_wrapper( void * item, void * data ) { free_range( item ); }

static void free_reference_region( struct reference_region * self )
{
    free( (void*)self->name );
    VectorWhack ( &self->ranges, release_ranges_wrapper, NULL );
    free( self );
}


static void merge_overlapping_ranges( struct reference_region * self )
{
    uint32_t n = VectorLength( &self->ranges );
    uint32_t i = 0;
    struct reference_range * a = NULL;
    while ( i < n )
    {
        struct reference_range * b = VectorGet ( &self->ranges, i );
        bool remove = false;
        if ( a != NULL )
        {
            remove = range_overlapp( a, b );
            if ( remove )
            {
                struct reference_range * r;
                a->end = b->end;
                VectorRemove ( &self->ranges, i, (void**)&r );
                free( r );
                n--;
            }
        }
        if ( !remove )
        {
            a = b;
            ++i;
        }
    }
}


static void merge_close_ranges_and_create_filter( struct reference_region * self, uint64_t merge_diff )
{
    uint32_t n = VectorLength( &self->ranges );
    uint32_t i = 0;
    struct reference_range * a = NULL;
    while ( i < n )
    {
        struct reference_range * b = VectorGet ( &self->ranges, i );
        bool remove = false;
        if ( a != NULL )
        {
            /* get the distance between a and b */
            uint64_t d = range_distance( a, b );
            remove = ( d < merge_diff );
            if ( remove )
            {
                struct reference_range * r;

                /* add the gap to the skip-vector of a */
                struct skip_range * sr = make_skip_range( a->end + 1, b->start - 1 );
                VectorAppend ( &( a->skip ), NULL, sr );

                /* expand a to merge with b */
                a->end = b->end;

                /* remove b */
                VectorRemove ( &self->ranges, i, (void**)&r );
                free( r );
                n--;
            }
        }
        if ( !remove )
        {
            a = b;
            ++i;
        }
    }
}


/* =========================================================================================== */

static int CC reference_vs_pchar_wrapper( const void *item, const BSTNode *n )
{
    const struct reference_region * r = ( const struct reference_region * )n;
    return cmp_pchar( (const char *)item, r->name );
}

static struct reference_region * find_reference_region( BSTree * regions, const char * name )
{
    return ( struct reference_region * ) BSTreeFind ( regions, name, reference_vs_pchar_wrapper );
}

static int CC ref_vs_ref_wrapper( const BSTNode *item, const BSTNode *n )
{
   const struct reference_region * a = ( const struct reference_region * )item;
   const struct reference_region * b = ( const struct reference_region * )n;
   return cmp_pchar( a->name, b->name );
}


rc_t add_region( BSTree * regions, const char * name, const uint64_t start, const uint64_t end )
{
    rc_t rc;

    struct reference_region * r = find_reference_region( regions, name );
    if ( r == NULL )
    {
        r = make_reference_region( name );
        if ( r == NULL )
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        else
            rc = add_ref_region_range( r, start, end );
        if ( rc == 0 )
            rc = BSTreeInsert ( regions, (BSTNode *)r, ref_vs_ref_wrapper );
        if ( rc != 0 )
            free_reference_region( r );
    }
    else
    {
        rc = add_ref_region_range( r, start, end );
    }
    return rc;
}


rc_t parse_and_add_region( BSTree * regions, const char * s )
{
    uint64_t start, end;
    char name[ 64 ];
    parse_definition( s, name, sizeof name, &start, &end );
    if ( name[ 0 ] == 0 )
        return RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
        return add_region( regions, name, start, end );
}


/* =========================================================================================== */


static void CC slice_report_wrapper( BSTNode *n, void *data )
{
    const struct reference_region * r = ( const struct reference_region * )n;
    uint32_t nr = VectorLength( &( r->ranges ) );

    KOutMsg( "\n-[%s]:\n", r->name );
    if ( nr == 0 )
        KOutMsg( " no ranges!\n" );
    else
    {
        uint32_t i, j;
        for ( i = 0; i < nr; ++i )
        {
            const struct reference_range * rr = ( const struct reference_range * ) VectorGet ( &( r->ranges ), i );
            uint32_t ns = VectorLength( &( rr->skip ) );
            KOutMsg( "  %u ... %u\n", rr->start, rr->end );
            for ( j = 0; j < ns; ++j )
            {
                const struct skip_range * sr = ( const struct skip_range * ) VectorGet ( &( rr->skip ), j );
                KOutMsg( "  ___skip %u ... %u\n", sr->start, sr->end );
            }
        }
    }
}


void slice_report( BSTree * regions )
{
    KOutMsg( "\n\nstart slice-report:\n" );
    BSTreeForEach ( regions, false, slice_report_wrapper, NULL );
    KOutMsg( "\nend slice-report\n\n" );
}

static void CC check_refrange_wrapper( BSTNode *n, void *data )
{
    struct reference_region * rr = ( struct reference_region * )n;
    uint64_t * merge_diff = data;
    merge_overlapping_ranges( rr );
    if ( *merge_diff > 0 )
        merge_close_ranges_and_create_filter( rr, *merge_diff );
}


void check_ref_regions( BSTree * regions, uint64_t merge_diff )
{
    BSTreeForEach ( regions, false, check_refrange_wrapper, &merge_diff );
}


/* =========================================================================================== */


static void CC release_ref_region_wrapper( BSTNode *n, void * data )
{
    free_reference_region( ( struct reference_region * ) n );
}


void free_ref_regions( BSTree * regions )
{    
    BSTreeWhack ( regions, release_ref_region_wrapper, NULL );
}


/* =========================================================================================== */


static void CC count_ref_region_wrapper( BSTNode *n, void *data )
{   
    struct reference_region * r = ( struct reference_region * ) n;
    uint32_t * count = ( uint32_t * ) data;
    *count += VectorLength( &(r->ranges) );
}


uint32_t count_ref_regions( BSTree * regions )
{
    uint32_t res = 0;
    BSTreeForEach ( regions, false, count_ref_region_wrapper, &res );
    return res;
}


/* =========================================================================================== */


typedef struct foreach_ref_region_func
{
    rc_t ( CC * on_region ) ( const char * name, const struct reference_range * range, void *data );
    void * data;
    rc_t rc;
} foreach_ref_region_func;


static bool CC foreach_ref_region_wrapper( BSTNode *n, void *data )
{   
    struct reference_region * r = ( struct reference_region * ) n;
    foreach_ref_region_func * func = ( foreach_ref_region_func * )data;

    if ( func->rc == 0 )
    {
        uint32_t i, v_count = VectorLength( &( r->ranges ) );

        for ( i = 0; i < v_count && func->rc == 0; ++i )
        {
            struct reference_range * rr = VectorGet ( &( r->ranges ), i );
            func->rc = func->on_region( r->name, rr, func->data );    
        }
    }
    return ( func->rc != 0 );
}


rc_t foreach_ref_region( BSTree * regions,
    rc_t ( CC * on_region ) ( const char * name, const struct reference_range * range, void *data ), 
    void *data )
{
    foreach_ref_region_func func;

    func.on_region = on_region;
    func.data = data;
    func.rc = 0;
    BSTreeDoUntil ( regions, false, foreach_ref_region_wrapper, &func );
    return func.rc;
}


/* =========================================================================================== */


const struct reference_region * get_first_ref_node( const BSTree * regions )
{
    return ( const struct reference_region * ) BSTreeFirst ( regions );
}


const struct reference_region * get_next_ref_node( const struct reference_region * node )
{
    return ( const struct reference_region * ) BSTNodeNext( ( const BSTNode * ) node );
}
    

const char * get_ref_node_name( const struct reference_region * node )
{
    return ( node->name );
}


uint32_t get_ref_node_range_count( const struct reference_region * node )
{
    return VectorLength( &( node->ranges ) );
}


const struct reference_range * get_ref_range( const struct reference_region * node, uint32_t idx )
{
    const struct reference_range * res = NULL;
    if ( node != NULL )
    {
        uint32_t n = VectorLength( &( node->ranges ) );
        if ( ( idx >=0 ) && ( idx < n ) )
        {
            res = ( const struct reference_range * ) VectorGet ( &( node->ranges ), idx );
        }
    }
    return res;
}


uint64_t get_ref_range_start( const struct reference_range * range )
{
    uint64_t res = 0;
    if ( range != NULL )
        res = range->start;
    return res;
}


uint64_t get_ref_range_end( const struct reference_range * range )
{
    uint64_t res = 0;
    if ( range != NULL )
        res = range->end;
    return res;
}


/* =========================================================================================== */


struct skiplist_ref_node
{
    BSTNode node;
    const char * name;
    int32_t current_id;
    const struct skip_range * current_skip_range;
    Vector skip_ranges;     /* holds skip_range structs */
} skiplist_ref_node;


struct skiplist
{
    BSTree nodes;           /* a tree of skiplist_ref_node 's */
    uint32_t node_count;
    struct skiplist_ref_node * current;
} skiplist;


/* helper func to detect if the given reference_region has ranges to be skiped */
static bool reference_region_has_skip_ranges( const struct reference_region * r )
{
    bool res = false;
    uint32_t i, n = VectorLength( &r->ranges );
    for ( i = 0; i < n && !res; ++i )
    {
        const struct reference_range * rr = VectorGet ( &( r->ranges ), i );
        if ( VectorLength( &rr->skip ) > 0 ) res = true;
    }
    return res;
}


/* helper to create a skiplist-node, walk the given the ref-region fo find and enter all skip positions */
static struct skiplist_ref_node * make_skiplist_ref_node( const struct reference_region * r )
{
    struct skiplist_ref_node * res = calloc( 1, sizeof *res );
    if ( res != NULL )
    {
        uint32_t i, n = VectorLength( &r->ranges );
        res->name = string_dup_measure ( r->name, NULL );
        VectorInit ( &res->skip_ranges, 0, 5 );
        /* walk the ranges-Vector of the reference-region */
        for ( i = 0; i < n; ++i )
        {
            const struct reference_range * rr = VectorGet ( &( r->ranges ), i );
            /* walk the skip-Vector of the reference-range */
            uint32_t j, n1 = VectorLength( &rr->skip );
            for ( j = 0; j < n1; ++j )
            {
                const struct skip_range * sr = VectorGet ( &( rr->skip ), j );
                if ( sr != NULL )
                {
                    struct skip_range * csr = make_skip_range( sr->start, sr->end );
                    if ( csr != NULL )
                        VectorAppend ( &( res->skip_ranges ), NULL, csr );
                }
            }
        }
        res->current_id = 0;
        res->current_skip_range = VectorGet ( &( res->skip_ranges ), 0 );
    }
    return res;
}


/* helper call back for BSTreeInsert into skiplist->nodes */
static int CC srn_vs_srn_wrapper( const BSTNode *item, const BSTNode *n )
{
   const struct skiplist_ref_node * a = ( const struct skiplist_ref_node * )item;
   const struct skiplist_ref_node * b = ( const struct skiplist_ref_node * )n;
   return cmp_pchar( a->name, b->name );
}


/* call back for each reference-region to generate eventually a skiplist_ref_node */
static void CC visit_region_node_for_skiplist( BSTNode *n, void *data )
{
    const struct reference_region * r = ( const struct reference_region * ) n;
    struct skiplist * skl = ( struct skiplist * ) data;
    if ( r != NULL && skl != NULL )
    {
        /* walk the reference-region, detect if we even have something to skip in here */
        if ( reference_region_has_skip_ranges( r ) )
        {
            struct skiplist_ref_node * srn = make_skiplist_ref_node( r );
            if ( srn != NULL )
            {
                BSTreeInsert ( &(skl->nodes), ( BSTNode * )srn, srn_vs_srn_wrapper );
                skl->node_count++;
            }
        }
    }
}

struct skiplist * skiplist_make( BSTree * regions )
{
    struct skiplist *res = calloc( 1, sizeof *res );
    if ( res != NULL )
    {
        BSTreeInit( &(res->nodes) );
        res->current = NULL;
        res->node_count = 0;

        /* walk the given regions-tree to generate the skip-list */
        BSTreeForEach ( regions, false, visit_region_node_for_skiplist, res );
        if ( res->node_count == 0 )
        {
            skiplist_release( res );
            res = NULL;
        }
    }
    return res;
}


static void CC release_skiplist_entry( BSTNode * n, void * data )
{
    struct skiplist_ref_node * node = ( struct skiplist_ref_node * )n;
    if ( node->name != NULL ) free( ( void * ) node->name );
    VectorWhack ( &node->skip_ranges, release_skip, NULL );     /* wrapper callback reused from above */
    free( ( void * ) node );
}


void skiplist_release( struct skiplist * list )
{
    if ( list != NULL )
    {
        BSTreeWhack ( &(list->nodes), release_skiplist_entry, NULL );
        free( ( void * ) list );
    }
}


static int CC pchar_vs_srn_cmp( const void * item, const BSTNode * n )
{
   const char * name = item;
   const struct skiplist_ref_node * b = ( const struct skiplist_ref_node * )n;
   return cmp_pchar( name, b->name );

}


void skiplist_enter_ref( struct skiplist * list, const char * name )
{
    if ( list != NULL )
    {
        if ( name == NULL )
            list->current = NULL;
        else
        {
            struct skiplist_ref_node * cur_node = ( struct skiplist_ref_node * )BSTreeFind ( &( list->nodes ), name, pchar_vs_srn_cmp );
            list->current = cur_node;
            cur_node->current_id = 0;
            cur_node->current_skip_range = VectorGet ( &( cur_node->skip_ranges ), 0 );
        }
    }
}


bool skiplist_is_skip_position( struct skiplist * list, uint64_t pos )
{
    if ( list != NULL )
    {
        struct skiplist_ref_node * cur_node = list->current;
        if ( cur_node != NULL )
        {
            const struct skip_range * curr_skip_range = cur_node->current_skip_range;
            if ( curr_skip_range != NULL )
            {
                if ( pos < curr_skip_range->start ) return false;
                if ( pos <= curr_skip_range->end ) return true;
                cur_node->current_id++;
                cur_node->current_skip_range = VectorGet ( &( cur_node->skip_ranges ), cur_node->current_id );
            }
        }
    }
    return false;
}


static void CC skiplist_report_cb( BSTNode *n, void *data )
{
    const struct skiplist_ref_node * node = ( const struct skiplist_ref_node * )n;
    uint32_t nr = VectorLength( &( node->skip_ranges ) );

    KOutMsg( "\n-[%s]:\n", node->name );
    if ( n == 0 )
        KOutMsg( " no ranges!\n" );
    else
    {
        uint32_t i;
        for ( i = 0; i < nr; ++i )
        {
            const struct skip_range * sr = ( const struct skip_range * ) VectorGet ( &( node->skip_ranges ), i );
            KOutMsg( "  %u ... %u\n", sr->start, sr->end );
        }
    }
}


void skiplist_report( const struct skiplist * list )
{
    if ( list != NULL )
    {
        KOutMsg( "\n\nstart skiplist-report:\n" );
        BSTreeForEach ( &( list->nodes ), false, skiplist_report_cb, NULL );
        KOutMsg( "\nend skiplist-report\n\n" );
    }
}
