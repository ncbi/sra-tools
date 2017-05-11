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
    memset( e, 0, sizeof *e );
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
    rc_t rc = 0;
    const Dict_Entry * e = &pe->e;
    if ( e->deletes > 0 || e->inserts > 0 )
        rc = f( &( e->count ), rname, pos, e->deletes, e->inserts, e->base_ptr, user_data );
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

#if 0
static rc_t enter_pos_entry( Pos_Entry * pe,
                             uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    rc_t rc = 0;
    if ( pe->e.deletes == 0 && pe->e.inserts == 0 )
    {
        /* this pos-entry is empty, put the data into pe->e */
        initialize_dict_entry( &pe->e, deletes, inserts, bases, fwd, first );
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
#endif

/* --------------------------------------------------------------------------------------------------- */

#define NUM_PUTS_PURGE 5000

typedef struct Allele_Dict
{
    KVector * v;
    const dict_data * data;
    const String * rname;
    
    uint64_t min_pos;
    uint64_t max_pos;
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
                rc = visit_pos_entry( self->rname, key, pe, self->data->event_func, self->data->user_data );
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


static rc_t allele_dict_initialize( struct Allele_Dict * self, const String * rname, const dict_data * data )
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
            self->data = data;
            self->min_pos = 0xFFFFFFFFFFFFFFFF;
        }
    }
    return rc;
}


rc_t allele_dict_make( struct Allele_Dict ** self, const String * rname, const dict_data * data )
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
            rc = allele_dict_initialize( o, rname, data );

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
    if ( spread > ( self->data->purge * 2 ) )
    {
        rc_t rc2 = 0;
        uint64_t key;
        Pos_Entry * pe;
        uint64_t last_pos = self->min_pos + self->data->purge;
        while ( rc2 == 0 )
        {
            rc2 = KVectorGetFirstPtr( self->v, &key, ( void ** )&pe );
            if ( rc2 == 0 )
            {
                if ( key > last_pos )
                    rc2 = -1;
                else
                {
                    rc = visit_pos_entry( self->rname, key, pe, self->data->event_func, self->data->user_data );
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

#define ALLELES_PER_POS 500

typedef struct Pos_Entry2
{
    Dict_Entry e[ ALLELES_PER_POS ];
    uint32_t used;
} Pos_Entry2;


#define BLOCKSIZE 500
#define BLOCKCOUNT 3

typedef struct Allele_Dict2
{
    const String * rname;       /* the reference-name, to get handed out the the event_func */
    const dict_data * data;
    
    Pos_Entry2 entries[ BLOCKCOUNT * BLOCKSIZE ];
    Pos_Entry2 * blocks[ BLOCKCOUNT ];
    uint64_t starting_pos;
    uint64_t last_pos;
} Allele_Dict2;


static rc_t allele_dict2_enter_pos_entry( Pos_Entry2 * pe,
                     uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first )
{
    rc_t rc = 0;
    if ( pe->used == 0 )
    {
        initialize_dict_entry( &pe->e[ 0 ], deletes, inserts, bases, fwd, first );
        pe->used += 1;
    }
    else
    {
        uint32_t idx;
        Dict_Entry * found = NULL;
        
        /* we are looking for an entry that matches our allele */
        for( idx = 0; idx < pe->used; ++idx )
        {
            if ( equal_dict_entry( &pe->e[ idx ], deletes, inserts, bases ) == 0 )
            {
                found = &pe->e[ idx ];
            }
        }
        
        if ( found != NULL )
        {
            /* we found one, increase it's counters */
            count_dict_entry( &found->count, fwd, first );
        }
        else
        {
            /* we did not find one, initialize one with our allele */
            if ( pe->used < ALLELES_PER_POS )
            {
                /* we still have space for a new allele at this position */
                initialize_dict_entry( &pe->e[ pe->used ], deletes, inserts, bases, fwd, first );
                pe->used += 1;
            }
            else
            {
                /* we have a problem here: we have no space for our allele! */            
                rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcInvalid );
                log_err( "allele_dict.allele_dict2_enter_pos_entry() has not enough space" );
            }
        }
    }
    return rc;
}

static rc_t allele_dict2_visit_and_clear_block( Allele_Dict2 * self, uint64_t starting_pos, Pos_Entry2 * start_ptr )
{
    rc_t rc = 0;
    uint32_t pos_idx;
    for ( pos_idx = 0; rc == 0 && pos_idx < BLOCKSIZE; ++pos_idx )
    {
        Pos_Entry2 * pe = &start_ptr[ pos_idx ];
        uint32_t entry_idx;
        
        /* visit the entries, and release them */
        for ( entry_idx = 0; rc == 0 && entry_idx < pe->used; ++entry_idx )
        {
            Dict_Entry * de = &pe->e[ entry_idx ];
            rc = self->data->event_func( &( de->count ), self->rname, starting_pos + pos_idx,
                                    de->deletes, de->inserts, de->base_ptr, self->data->user_data );
            release_dict_entry_no_free( de );
        }
        
        /* now, no entries are used any more */
        pe->used = 0;
    }
    return rc;
}


rc_t allele_dict2_release( struct Allele_Dict2 * self )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict.allele_dict2_release() given a NULL-ptr" );
    }
    else
    {
        uint32_t idx;
        uint64_t starting_pos = self->starting_pos;
        for ( idx = 0; rc == 0 && idx < BLOCKCOUNT; ++idx )
        {
            rc = allele_dict2_visit_and_clear_block( self, starting_pos, self->blocks[ idx ] );
            starting_pos += BLOCKSIZE;
        }
        
        if ( self->rname != NULL )
            StringWhack( self->rname );
        free( ( void * ) self );
    }
    return rc;
}


static rc_t allele_dict2_initialize( struct Allele_Dict2 * self, const String * rname, const dict_data * data )
{
    rc_t rc = StringCopy( &self->rname, rname );
    if ( rc != 0  )
        log_err( "allele_dict.allele_dict2_initialize() StringCopy failed" );
    else
    {
        uint32_t idx;
        
        self->data = data;
        self->starting_pos = 0;
        self->last_pos = self->starting_pos + ( BLOCKCOUNT * BLOCKSIZE ) - 1;
        
        for ( idx = 0; idx < BLOCKCOUNT; ++idx )
        {
            self->blocks[ idx ] = &self->entries[ idx * BLOCKSIZE ];
        }
    }
    return rc;
}


rc_t allele_dict2_make( struct Allele_Dict2 ** self, const String * rname, const dict_data * data )
{
    rc_t rc = 0;
    if ( self == NULL || rname == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "allele_dict.allele_dict2_make() given a NULL-ptr" );
    }
    else
    {
        Allele_Dict2 * o = calloc( 1, sizeof *o );
        *self = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "allele_dict.allele_dict_make() memory exhausted" );
        }
        else
            rc = allele_dict2_initialize( o, rname, data );

        if ( rc == 0 )
            *self = o;
        else
            allele_dict2_release( o );
    }
    return rc;
}


rc_t allele_dict2_put( struct Allele_Dict2 * self,
                       uint64_t position, uint32_t deletes, uint32_t inserts,
                       const char * bases, bool fwd, bool first )
{
    rc_t rc = 0;
    if ( self == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcNull );
        log_err( "allele_dict2.allele_dict_put() given a NULL-ptr" );
    }
    else
    {
        bool inserting = true;
        while ( rc == 0 && inserting )
        {
            if ( position < self->starting_pos )
            {
                /* this is bad, we are going backwards on position, input is unsorted! */
                rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcInvalid );
                log_err( "allele_dict.allele_dict2_put() inserted pos of %lu before dict.starting_pos of %lu",
                         position, self->starting_pos );
            }
            else if ( position < self->last_pos )
            {
                /* this is the common case, we insert into the current window... */
                uint64_t relative_pos = ( position - self->starting_pos );
                uint64_t block_idx = ( relative_pos / BLOCKSIZE );
                if ( block_idx >= BLOCKCOUNT )
                {
                    /* this should not happen, if it happens then starting_pos/last_pos are invalid */
                    rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcInvalid );
                    log_err( "allele_dict.allele_dict2_put() inserted pos of %lu not between %lu and %lu",
                             position, self->starting_pos, self->last_pos );
                }
                else
                {
                    /* we have found the block where our allele goes in */
                    Pos_Entry2 * pe = self->blocks[ block_idx ];
                    /* now let as adjust the corrent position */
                    pe += ( relative_pos - ( BLOCKSIZE * block_idx ) );
                    
                    /**************************************************************/
                    rc = allele_dict2_enter_pos_entry( pe, deletes, inserts, bases, fwd, first );
                    /**************************************************************/
                    
                    /* now we can terminate the loop! */
                    inserting = false;
                }
            }
            else
            {
                /* we are advancing out of the window, parts of it has to be purged... */
                
                /* what is the difference between the last_pos and the allele-position */
                int64_t gap = ( position - self->last_pos );
                uint32_t idx;
                
                if ( gap < ( BLOCKCOUNT * BLOCKSIZE ) )
                {
                    /* the gap is small enough to advance one block at a time, we purge the fist block */
                    rc = allele_dict2_visit_and_clear_block( self, self->starting_pos, self->blocks[ 0 ] );
                    if ( rc == 0 )
                    {
                        /* we rotate the blocks... */
                        Pos_Entry2 * temp = self->blocks[ 0 ];
                        for ( idx = 0; idx < ( BLOCKCOUNT - 1 ); ++idx )
                            self->blocks[ idx ] = self->blocks[ idx + 1 ];
                        self->blocks[ BLOCKCOUNT - 1 ] = temp;
                        
                        /* we move starting_pos and last_pos forward by BLOCKSIZE */
                        self->starting_pos += BLOCKSIZE;
                        self->last_pos += BLOCKSIZE;
                    }
                }
                else
                {
                    /* the gap is wider then all of our blocks, we purge all blocks */
                    for ( idx = 0; rc == 0 && idx < BLOCKCOUNT; ++idx )
                        rc = allele_dict2_visit_and_clear_block( self, self->starting_pos + ( idx * BLOCKSIZE ), self->blocks[ idx ] );    
                    
                    /* let us start one block before the allele-pos ( because allele-positions can fluctuate ) */
                    self->starting_pos = position - ( BLOCKSIZE * BLOCKCOUNT );
                    self->last_pos = self->starting_pos + ( BLOCKCOUNT * BLOCKSIZE ) - 1;
                }
                /* we re-enter the inserting-loop again to try to place our allele */
            }
        }
    }
    return rc;
}
