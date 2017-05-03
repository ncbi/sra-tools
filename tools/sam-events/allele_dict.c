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
#include <klib/out.h>


#define NUM_ENTRY_BASES 12

typedef struct Dict_Entry
{
    uint32_t deletes, inserts;
    counters count;
    char bases[ NUM_ENTRY_BASES ];
    char * base_ptr;
} Dict_Entry;


static void release_dict_entry_no_free( Dict_Entry * e )
{
    if ( e->base_ptr != NULL && e->base_ptr != e->bases )
        free( ( void * )e->base_ptr );
}


static void release_dict_entry( void *item, void *data )
{
    Dict_Entry * e = item;
    if ( e != NULL )
    {
        if ( e->base_ptr != NULL && e->base_ptr != e->bases )
            free( ( void * )e->base_ptr );
        free( ( void * )e );
    }
}


static void count_dict_entry( counters * count, bool fwd, bool first )
{
    if ( fwd )
    {
        count->fwd += 1;
        if ( first )
            count->t_pos += 1;
        else
            count->t_neg += 1;
    }
    else
    {
        count->rev += 1;
        if ( first )
            count->t_neg += 1;
        else
            count->t_pos += 1;
    }

}


static void initialize_dict_entry( Dict_Entry * self, uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    self->deletes = deletes;
    self->inserts = inserts;   /* inserts is also the lenght of bases */
    count_dict_entry( &( self->count ), fwd, first );
    if ( inserts > 0 )
    {
        uint32_t i;
        if ( inserts >= NUM_ENTRY_BASES )
        {
            self->base_ptr = malloc( inserts + 1 );
            if ( self->base_ptr == NULL )
                log_err( "allele_dict.initialize_dict_entry() memory exhausted (2)" );
        }
        else
            self->base_ptr = self->bases;

        for ( i = 0; i < inserts; ++i ) self->base_ptr[ i ] = bases[ i ];
        self->base_ptr[ inserts ] = 0;
    }
    else
    {
        self->base_ptr = self->bases;
        self->base_ptr[ 0 ] = 0;
    }
}


static Dict_Entry * make_dict_entry( uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    Dict_Entry * o = calloc( 1, sizeof *o );
    if ( o == NULL )
        log_err( "allele_dict.make_dict_entry() memory exhausted" );
    else
        initialize_dict_entry( o, deletes, inserts, bases, fwd, first );
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

/* --------------------------------------------------------------------------------------------------- */

typedef struct Pos_Entry
{
    Dict_Entry e;     /* a dictionary-entry ( because a lot of positions have just 1 entry ) */
    Vector entries;     /* vector of dictionary-entries ( for the rarer case of multiple alleles per position )*/
} Pos_Entry;


static void release_pos_entry( Pos_Entry * pe )
{
    if ( pe != NULL )
    {
        if ( VectorLength( &( pe->entries ) ) > 0 )
            VectorWhack( &( pe->entries ), release_dict_entry, NULL );
        release_dict_entry_no_free( &pe->e );
        free( ( void * )pe );
    }
}


static Pos_Entry * make_pos_entry( uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    Pos_Entry * o = calloc( 1, sizeof *o );
    if ( o == NULL )
        log_err( "allele_dict.make_pos_entry() memory exhausted" );
    else
        initialize_dict_entry( &o->e, deletes, inserts, bases, fwd, first );
    return o;
}


typedef struct lookup_key
{
    uint32_t deletes, inserts;
    const char * bases;
} lookup_key;


static rc_t insert_event( Vector * v, uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    rc_t rc = 0;
    Dict_Entry * e = make_dict_entry( deletes, inserts, bases, fwd, first );
    if ( e != NULL )
    {
        rc = VectorAppend( v, NULL, e );
        if ( rc != 0 )
        {
            log_err( "allele_dict.insert_event() VectorAppend() failed %R", rc );
            release_dict_entry( e, NULL );
        }
    }
    else
        rc = RC( rcApp, rcNoTarg, rcInserting, rcMemory, rcExhausted );
    return rc;
}


static Dict_Entry * vector_lookup( Vector * v, uint32_t deletes, uint32_t inserts, const char * bases )
{
    Dict_Entry * res = NULL;
    uint32_t i, n = VectorLength( v );
    /* if we have more of them in the vectory, visit them too */
    for ( i = 0; res == NULL && i < n; ++i )
    {
        Dict_Entry * e = VectorGet( v, i );
        if ( equal_dict_entry( e, deletes, inserts, bases ) == 0 )
            res = e;
    }
    return res;
}

static rc_t new_pos_event( Pos_Entry * pe, uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    rc_t rc = 0;
    if ( pe == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
        log_err( "allele_dict.new_pos_event() given a NULL-ptr" );
    }
    else
    {
        if ( equal_dict_entry( &pe->e, deletes, inserts, bases ) == 0 )
        {
            /* the key ( deletes,inserts,bases ) matches the single ( common-case ) event */
            count_dict_entry( &( pe->e.count ) , fwd, first );
        }
        else
        {
            Dict_Entry * e;
            if ( VectorLength( &( pe->entries ) ) == 0 )
            {
                /* vector has not been used: initialize it */
                VectorInit( &( pe->entries ), 0, 5 );
                /* put new event into it */
                rc = insert_event( &( pe->entries ), deletes, inserts, bases, fwd, first );
            }
            else
            {
                /* vector has been used... , try to find entry */
                e = vector_lookup( &( pe->entries ), deletes, inserts, bases );
                if ( e != NULL )
                {
                    /* we do have an entry already, just increase count */
                    count_dict_entry( &( e->count ), fwd, first );
                }
                else
                {
                    /* we do not have one already, make a new one */
                    rc = insert_event( &( pe->entries ), deletes, inserts, bases, fwd, first );
                }
            }
        }
    }
    return rc;
}


static rc_t visit_pos_entry( const String * rname, uint32_t pos,
                             const Pos_Entry * pe, on_ad_event f, void * user_data )
{
    /* first we visit the mandatory pos-event */
    const Dict_Entry * e = &pe->e;
    rc_t rc = f( &( e->count ), rname, pos, e->deletes, e->inserts, e->base_ptr, user_data );
    if ( rc == 0 )
    {
        uint32_t i, n = VectorLength( &( pe->entries ) );
        /* if we have more of them in the vectory, visit them too */
        for ( i = 0; rc == 0 && i < n; ++i )
        {
            e = VectorGet( &( pe->entries ), i );
            rc = f( &( e->count ), rname, pos, e->deletes, e->inserts, e->base_ptr, user_data );
        }
    }
    return rc;
}

/* --------------------------------------------------------------------------------------------------- */

#define NUM_PUTS_PURGE 5000

typedef struct Allele_Dict
{
    KVector * v;
    const String * rname;
    void * user_data;
    on_ad_event event_func;
    
    uint64_t min_pos;
    uint64_t max_pos;
    uint32_t purge;
    uint32_t num_puts;
} Allele_Dict;


static rc_t cleanup_cb( uint64_t key, const void *value, void *user_data )
{
    Pos_Entry * pe = ( Pos_Entry * )value;
    release_pos_entry( pe );
    return 0;
}


rc_t allele_dict_release( struct Allele_Dict * self )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict.allele_dict_purge() given a NULL-ptr" );
    }
    else
    {
        rc_t rc2 = 0;
        while ( rc2 == 0 )
        {
            uint64_t key;
            Pos_Entry * pe;
            rc2 = KVectorGetFirstPtr( self->v, &key, ( void ** )&pe );
            if ( rc2 == 0 )
            {
                rc = visit_pos_entry( self->rname, key, pe, self->event_func, self->user_data );
                if ( rc == 0 )
                {
                    rc = KVectorUnset( self->v, key );
                    if ( rc == 0 )
                        release_pos_entry( pe );
                }
            }
        }

        if ( self->v != NULL )
        {
            rc = KVectorVisitPtr( self->v, false, cleanup_cb, NULL );
            KVectorRelease( self->v );
        }

        if ( self->rname != NULL )
            StringWhack( self->rname );

        free( ( void * ) self );
    }
    return rc;
}


static rc_t allele_dict_initialize( struct Allele_Dict * self, const String * rname, uint32_t purge,
                                    on_ad_event event_func, void * user_data )
{
    rc_t rc = KVectorMake( &self->v );
    if ( rc != 0 )
        log_err( "allele_dict.allele_dict_make() KVectorMake() failed %R", rc );
    else
    {
        rc = StringCopy( &self->rname, rname );
        if ( rc != 0  )
            log_err( "allele_dict.allele_dict_make() StringCopy failed" );
        else
        {
            self->purge = purge;
            self->min_pos = 0xFFFFFFFFFFFFFFFF;
            self->event_func = event_func;
            self->user_data = user_data;
        }
    }
    return rc;
}


rc_t allele_dict_make( struct Allele_Dict ** self, const String * rname, uint32_t purge,
                       on_ad_event event_func, void * user_data )
{
    rc_t rc = 0;
    if ( self == NULL || rname == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "allele_dict.allele_dict_make() given a NULL-ptr" );
    }
    else
    {
        Allele_Dict * o = calloc( 1, sizeof *o );
        *self = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "allele_dict.allele_dict_make() memory exhausted" );
        }
        else
            rc = allele_dict_initialize( o, rname, purge, event_func, user_data );

        if ( rc == 0 )
            *self = o;
        else
            allele_dict_release( o );
    }
    return rc;
}


static rc_t allele_dict_visit_and_purge( struct Allele_Dict * self )
{
    rc_t rc = 0;
    int64_t spread = self->max_pos - self->min_pos;
    if ( spread > ( self->purge * 2 ) )
    {
        rc_t rc2 = 0;
        uint64_t key;
        Pos_Entry * pe;
        uint64_t last_pos = self->min_pos + self->purge;
        while ( rc2 == 0 )
        {
            rc2 = KVectorGetFirstPtr( self->v, &key, ( void ** )&pe );
            if ( rc2 == 0 )
            {
                if ( key > last_pos )
                    rc2 = -1;
                else
                {
                    rc = visit_pos_entry( self->rname, key, pe, self->event_func, self->user_data );
                    if ( rc == 0 )
                    {
                        rc = KVectorUnset( self->v, key );
                        if ( rc == 0 )
                            release_pos_entry( pe );
                    }
                }
            }
        }
        /* set the new min_pos... */
        rc2 = KVectorGetFirstPtr( self->v, &key, ( void ** )&pe );
        if ( rc2 == 0 )
            self->min_pos = key;
    }
    return rc;
}


rc_t allele_dict_put( struct Allele_Dict * self,
                      uint64_t position, uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict.allele_dict_put() given a NULL-ptr" );
    }
    else
    {
        Pos_Entry * pe = NULL;
        rc = KVectorGetPtr( self->v, position, ( void ** )&pe );
        if ( rc == 0 && pe != NULL )
        {
            /* entry for position found */
            rc = new_pos_event( pe, deletes, inserts, bases, fwd, first );
        }
        else
        {
            /* entry for position not found */
            pe = make_pos_entry( deletes, inserts, bases, fwd, first );
            if ( pe != NULL )
                rc = KVectorSetPtr( self->v, position, pe );
            else
                rc = RC( rcApp, rcNoTarg, rcInserting, rcMemory, rcExhausted );
        }
        if ( rc == 0 )
        {
            if ( position < self->min_pos )
                self->min_pos = position;
            else if ( position > self->max_pos )
                self->max_pos = position;                
        }
        
        /* run the purge after a specific number or put's */
        if ( rc == 0 )
        {
            self->num_puts++;
            if ( self->num_puts > NUM_PUTS_PURGE )
            {
                self->num_puts = 0;
                rc = allele_dict_visit_and_purge( self );
            }
        }
    }
    return rc;
}


/* --------------------------------------------------------------------------------------------------- */
#define BLOCKSIZE 1024

static rc_t make_entry_block( Pos_Entry ** block, uint32_t blocksize )
{
    rc_t rc = 0;
    if ( block == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "allele_dict.make_entry_block() given a NULL-ptr" );
    }
    else
    {
        Pos_Entry * o = calloc( blocksize, sizeof * o );
        *block = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "allele_dict.make_entry_block() memory exhausted" );
        }
        else
            *block = o;
    }
    return rc;
}


typedef struct Allele_Dict2
{
    const String * rname;
    void * user_data;
    on_ad_event event_func;
    
    uint32_t blocksize;
} Allele_Dict2;
