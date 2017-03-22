/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnologmsgy Information
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

#include "allele_dict.h"

#include <klib/vector.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/printf.h>

#define NUM_ENTRY_BASES 12

/* ----------------------------------------------------------------------------------------------- */

static rc_t log_err( const char * t_fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;
    
    va_list args;
    va_start( args, t_fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, t_fmt, args );
    va_end( args );
    if ( rc == 0 )
        rc = LogMsg( klogErr, buffer );
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */

typedef struct Dict_Entry
{
    uint32_t deletes, inserts, count;
    char bases[ NUM_ENTRY_BASES ];
    char * base_ptr;
} Dict_Entry;


static void release_dict_entry( Dict_Entry * e )
{
    if ( e != NULL )
    {
        if ( e->base_ptr != NULL && e->base_ptr != e->bases )
            free( ( void * )e->base_ptr );
        free( ( void * )e );
    }
}


static Dict_Entry * make_dict_entry( uint32_t deletes, uint32_t inserts, const char * bases )
{
    Dict_Entry * o = calloc( 1, sizeof *o );
    if ( o == NULL )
        log_err( "allele_dict.make_dict_entry() memory exhausted" );
    else
    {
        o->count = 1;
        o->deletes = deletes;
        o->inserts = inserts;   /* inserts is also the lenght of bases */
        if ( inserts > 0 )
        {
            uint32_t i;
            
            if ( inserts >= NUM_ENTRY_BASES )
            {
                o->base_ptr = malloc( inserts + 1 );
                if ( o->base_ptr == NULL )
                    log_err( "allele_dict.make_dict_entry() memory exhausted (2)" );
            }
            else
                o->base_ptr = o->bases;

            for ( i = 0; i < inserts; ++i ) o->base_ptr[ i ] = bases[ i ];
            o->base_ptr[ inserts ] = 0;
        }
        else
        {
            o->base_ptr = o->bases;
            o->base_ptr[ 0 ] = 0;
        }
    }
    return o;
}


static int equal_dict_entry( const Dict_Entry * e, uint32_t deletes, uint32_t inserts, const char * bases )
{
    int res = 1;
    if ( e != NULL )
    {
        res = ( e->deletes - deletes );
        if ( res == 0 )
        {
            res = ( e->inserts - inserts );
            if ( res == 0 && inserts > 0 )
            {
                /* the is just to be shure that the call to sting_cmp does not get a null-ptr */
                if ( ( e->base_ptr != NULL ) && ( bases != NULL ) )
                {
                    res = string_cmp( e->base_ptr, e->inserts, bases, inserts, inserts );
                }
            }
        }
    }
    return res;
}

static rc_t visit_dict_entry( const Dict_Entry * e, const String * rname, 
                              uint64_t pos, on_ad_event f, void * user_data )
{
    return f( e->count, rname, pos, e->deletes, e->inserts, e->base_ptr, user_data );
}

/* --------------------------------------------------------------------------------------------------- */

typedef struct Pos_Entry
{
    Dict_Entry * e;
    Vector entries;
} Pos_Entry;


static void CC release_pos_entry_dict_entry( void *item, void *data )
{
    release_dict_entry( ( Dict_Entry * )item ); 
}

static void release_pos_entry( Pos_Entry * pe )
{
    if ( pe != NULL )
    {
        if ( VectorLength( &( pe->entries ) ) > 0 )
            VectorWhack( &( pe->entries ), release_pos_entry_dict_entry, NULL );
        release_dict_entry( pe->e ); 
        free( ( void * )pe );
    }
}

static Pos_Entry * make_pos_entry( uint32_t deletes, uint32_t inserts, const char * bases )
{
    Pos_Entry * o = calloc( 1, sizeof *o );
    if ( o == NULL )
        log_err( "allele_dict.make_pos_entry() memory exhausted" );
    else
    {
        o->e = make_dict_entry( deletes, inserts, bases );
        if ( o->e == NULL )
        {
            release_pos_entry( o );
            o = NULL;
        }
    }
    return o;
}


typedef struct lookup_key
{
    uint32_t deletes, inserts;
    const char * bases;
} lookup_key;

static int64_t CC key_lookup( const void *key, const void *n )
{
    const lookup_key * lk = key;
    const Dict_Entry * e = n;
    return equal_dict_entry( e, lk->deletes, lk->inserts, lk->bases );
}

static rc_t insert_event( Vector * v, uint32_t deletes, uint32_t inserts, const char * bases )
{
    rc_t rc = 0;
    Dict_Entry * e = make_dict_entry( deletes, inserts, bases );
    if ( e != NULL )
    {
        rc = VectorAppend( v, NULL, e );
        if ( rc != 0 )
        {
            log_err( "allele_dict.insert_event() VectorAppend() failed %R", rc );
            release_dict_entry( e );
        }
    }
    else
        rc = RC( rcApp, rcNoTarg, rcInserting, rcMemory, rcExhausted );
    return rc;
}

static Dict_Entry * vector_lookup( Vector * v, lookup_key * key )
{
    Dict_Entry * res = NULL;
    uint32_t i, n = VectorLength( v );
    /* if we have more of them in the vectory, visit them too */
    for ( i = 0; res == NULL && i < n; ++i )
    {
        Dict_Entry * e = VectorGet( v, i );
        if ( equal_dict_entry( e, key->deletes, key->inserts, key->bases ) == 0 )
            res = e;
    }
    return res;
}

static rc_t new_pos_event( Pos_Entry * pe, uint32_t deletes, uint32_t inserts, const char * bases )
{
    rc_t rc = 0;
    if ( pe == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
        log_err( "allele_dict.new_pos_event() given a NULL-ptr" );
    }
    else
    {
        if ( equal_dict_entry( pe->e, deletes, inserts, bases ) == 0 )
        {
            /* the key ( deletes,inserts,bases ) matches the single ( common-case ) event */
            pe->e->count += 1;
        }
        else
        {
            Dict_Entry * e;
            if ( VectorLength( &( pe->entries ) ) == 0 )
            {
                /* vector has not been used: initialize it */
                VectorInit( &( pe->entries ), 0, 5 );
                /* put new event into it */
                rc = insert_event( &( pe->entries ), deletes, inserts, bases );
            }
            else
            {
                /* vector has been used... , try to find entry */
                
                /* Vectorfind implies that the vector is sorted!!!!, let's just scan the vector... */
                lookup_key key = { .deletes = deletes, .inserts = inserts, .bases = bases };
                
                /* e = VectorFind( &( pe->entries ), &key, NULL, key_lookup ); */
                e = vector_lookup( &( pe->entries ), &key );
                if ( e != NULL )
                {
                    /* we do have a entry already, just increase count */
                    e->count += 1;        
                }
                else
                {
                    /* we do not have one already, make a new one */
                    rc = insert_event( &( pe->entries ), deletes, inserts, bases );    
                }
            }
        }
    }
    return rc;
}


static rc_t visit_pos_entry( const String * rname, uint64_t pos,
                             const Pos_Entry * pe, on_ad_event f, void * user_data )
{
    /* first we visit the mandatory pos-event */
    rc_t rc = visit_dict_entry( pe->e, rname, pos, f, user_data );
    if ( rc == 0 )
    {
        uint32_t i, n = VectorLength( &( pe->entries ) );
        /* if we have more of them in the vectory, visit them too */
        for ( i = 0; rc == 0 && i < n; ++i )
        {
            Dict_Entry * e = VectorGet( &( pe->entries ), i );
            rc = visit_dict_entry( e, rname, pos, f, user_data );
        }
    }
    return rc;
}

/* --------------------------------------------------------------------------------------------------- */
typedef struct Allele_Dict
{
    KVector * v;
    const String * rname;
    
    uint64_t min_pos;
    uint64_t max_pos;
    
    uint32_t count;    
} Allele_Dict;


static rc_t cleanup_cb( uint64_t key, const void *value, void *user_data )
{
    Pos_Entry * pe = ( Pos_Entry * )value;
    release_pos_entry( pe );
    return 0;
}


rc_t allele_dict_release( struct Allele_Dict * ad )
{
    rc_t rc = 0;
    if ( ad == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( ad->v != NULL )
        {
            rc = KVectorVisitPtr( ad->v, false, cleanup_cb, NULL );
            KVectorRelease( ad->v );
        }
        if ( ad->rname != NULL )
            StringWhack( ad->rname );
        free( ( void * ) ad );
    }
    return rc;

}


rc_t allele_dict_make( struct Allele_Dict ** ad, const String * rname )
{
    rc_t rc = 0;
    if ( ad == NULL || rname == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "allele_dict.allele_dict_make() given a NULL-ptr" );
    }
    else
    {
        Allele_Dict * o = calloc( 1, sizeof *o );
        *ad = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "allele_dict.allele_dict_make() memory exhausted" );
        }
        else
        {
            rc = KVectorMake( &o->v );
            if ( rc != 0 )
                log_err( "allele_dict.allele_dict_make() KVectorMake() failed %R", rc );
            else
            {
                rc = StringCopy( &o->rname, rname );
                if ( rc != 0  )
                    log_err( "allele_dict.allele_dict_make() StringCopy failed" );
            }
        }
        if ( rc == 0 )
            *ad = o;
        else
            allele_dict_release( o );
    }
    return rc;
}


rc_t allele_dict_put( struct Allele_Dict * ad,
                      size_t position, uint32_t deletes, uint32_t inserts, const char * bases )
{
    rc_t rc = 0;
    if ( ad == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict.allele_dict_put() given a NULL-ptr" );
    }
    else
    {
        Pos_Entry * pe = NULL;
        rc = KVectorGetPtr( ad->v, position, ( void ** )&pe );
        if ( rc == 0 && pe != NULL )
        {
            /* entry for position found */
            rc = new_pos_event( pe, deletes, inserts, bases );
        }
        else
        {
            /* entry for position not found */
            pe = make_pos_entry( deletes, inserts, bases );
            if ( pe != NULL )
                rc = KVectorSetPtr( ad->v, position, pe );
            else
                rc = RC( rcApp, rcNoTarg, rcInserting, rcMemory, rcExhausted );
        }
        if ( rc == 0 )
        {
            if ( ad->count == 0 )
            {
                ad->min_pos = position;
                ad->max_pos = position;
            }
            else
            {
                if ( position < ad->min_pos )
                    ad->min_pos = position;
                else if ( position > ad->max_pos )
                    ad->max_pos = position;                
            }
            ad->count += 1;
        }
    }
    return rc;
}


typedef struct visit_pos_entry_ctx
{
    on_ad_event f;
    const String * rname;
    void * user_data;
    uint64_t filter_pos;
} visit_pos_entry_ctx;


static rc_t visit_pos_entry_cb( uint64_t key, const void *value, void *user_data )
{
    const Pos_Entry * pe = value;
    visit_pos_entry_ctx * vpec = user_data;
    if ( key < vpec->filter_pos )
        return visit_pos_entry( vpec->rname, key, pe, vpec->f, vpec->user_data );
    else
        return 0;
}


/* call a callback for each event in the allele_dictionary */
rc_t allele_dict_visit( struct Allele_Dict * ad, uint64_t pos, on_ad_event f, void * user_data )
{
    rc_t rc = 0;
    if ( ad == NULL || f == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict.allele_dict_visit() given a NULL-ptr" );
    }
    else
    {
        visit_pos_entry_ctx vpec;
        vpec.f = f;
        vpec.rname = ad->rname;
        vpec.user_data = user_data;
        vpec.filter_pos = pos;
        KVectorVisitPtr( ad->v, false, visit_pos_entry_cb, &vpec );        
    }
    return rc;
}


rc_t allele_get_min_max( struct Allele_Dict * ad, uint64_t * min_pos, uint64_t * max_pos )
{
    rc_t rc = 0;
    if ( ad == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict.allele_get_min_max() given a NULL-ptr" );
    }
    else
    {
        if ( min_pos != NULL ) *min_pos = ad->min_pos;
        if ( max_pos != NULL ) *max_pos = ad->max_pos;
    }
    return rc;
}


rc_t allele_dict_purge( struct Allele_Dict * ad, uint64_t pos )
{
    rc_t rc = 0;
    if ( ad == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict.allele_dict_purge() given a NULL-ptr" );
    }
    else
    {
        uint64_t p = ad->min_pos;
        while ( rc == 0 && p < pos )
        {
            Pos_Entry * pe;
            rc = KVectorGetFirstPtr( ad->v, &p, ( void ** )&pe );
            if ( rc == 0 && p < pos )
            {
                rc = KVectorUnset( ad->v, p );
                if ( rc == 0 )
                    release_pos_entry( pe );
                ad->count -= 1;
            }
        }
        ad->min_pos = p;
    }
    return rc;
}
