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
#include "event_iter.h"

#define NUM_EVENTS 1024

#define STATE_PULL_ALIGNMENT_AND_EXPAND_CIGAR 0
#define STATE_EXPAND_REMAINING_CIGAR 1
#define STATE_EVENTS_AVAILABLE 2

typedef struct event_iter
{
    struct alig_iter * source;
    struct cFastaFile * fasta;
    
    uint64_t position;
    bool unsorted;
    int ref_index;
    int state;
    int num_events;
    int event_idx;
    const String * rname;
    const char * ref_bases;
    VNamelist * seen_refs;
    AlignmentT alignment;
    unsigned refLength;
    unsigned seqLength;
    struct Event events[ NUM_EVENTS ];
} event_iter;


/* releae an event-iterator */
rc_t event_iter_release( struct event_iter * self )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( self->rname != NULL )
            StringWhack ( self->rname );
        VNamelistRelease( self->seen_refs );
        free( ( void * ) self );
    }
    return rc;
}


/* construct an event-iterator from an alignment-iterator */
rc_t event_iter_make( struct event_iter ** self, struct alig_iter * source, struct cFastaFile * fasta )
{
    rc_t rc = 0;
    if ( self == NULL || source == NULL || fasta == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "event_iter_make() given a NULL-ptr" );
    }
    else
    {
        event_iter * o = calloc( 1, sizeof *o );
        *self = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "event_iter_make() memory exhausted" );
        }
        else
        {
            o->source = source;
            o->fasta = fasta;
            o->state = STATE_PULL_ALIGNMENT_AND_EXPAND_CIGAR;
            o->ref_index = -1;
            VNamelistMake( &o->seen_refs, 10 );
        }
        
        if ( rc == 0 )
            *self = o;
        else
            event_iter_release( o );
    }
    return rc;

}


static rc_t event_iter_switch_reference( event_iter * self, const String * rname )
{
    rc_t rc = 0;
    
    /* prime the current-ref-name with the ref-name of the alignment */
    if ( self->rname != NULL )
        StringWhack ( self->rname );
    rc = StringCopy( &self->rname, rname );
    
    self->ref_index = FastaFile_getNamedSequence( self->fasta, rname->size, rname->addr );
        
    if ( self->ref_index < 0 )
    {
        rc = RC( rcExe, rcNoTarg, rcVisiting, rcId, rcInvalid );
        log_err( "'%S' not found in fasta-file", rname );
    }
    else
        self->ref_bases_count = FastaFile_getSequenceData( self->fasta, self->ref_index, &self->ref_bases );
    return rc;
}


static rc_t event_iter_check_rname( event_iter * self, const String * rname, uint64_t position, bool * new_ref_entered )
{
    rc_t rc = 0;
    int cmp = 1;
    
    if ( self->ref_index >= 0 )
        cmp = StringCompare( self->rname, rname );
        
    if ( cmp != 0 )
    {
        /* we are entering a new reference! */
        int32_t idx;
        
        rc = VNamelistContainsString( self->seen_refs, rname, &idx );
        if ( rc == 0 )
        {
            if ( idx < 0 )
                rc = VNamelistAppendString( self->seen_refs, rname );
            else
            {
                self->unsorted = true;
                log_err( "unsorted: %S", rname );
            }
        }
        
        /* print all entries found in the allele-dict, and then release the whole allele-dict */
        if ( rc == 0 && new_ref_entered != NULL )
        {
            *new_ref_entered = true;
        }
        
        /* switch to the new reference!
           - store the new refname in the current-struct
           - get the index of the new reference into the fasta-file
        */
        if ( rc == 0 )
            rc = alig_consumer_switch_reference( self, rname );
            
    }
    else
    {
        if ( position < self->position )
        {
            self->unsorted = true;
            rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcInvalid );
            log_err( "event_iter ... unsorted %lu ---> %lu", self->position, position );
        }
    }
    self->position = position;
    
    return rc;
}


static void event_iter_mismatch( event_iter * self, struct Event * src,  event_t * dst )
{
    dst->refpos = ( self->alignment->pos - 1 ) + src->refPos;
    dst->deletions  = src->length;
    dst->insertions = src->length;
    dst->bases = &( self->alignment->read.addr[ src->seqPos ] );
}


static void event_iter_insertion( event_iter * self, struct Event * src,  event_t * dst )
{
    dst->refpos = ( self->alignment->pos - 1 ) + src->refPos;
    dst->deletions = 0;
    dst->insertions = src->length;
    dst->bases = &( self->alignment->read.addr[ src->seqPos ] );
}


static void event_iter_deletion( event_iter * self, struct Event * src,  event_t * dst )
{
    dst->refpos = ( self->alignment->pos - 1 ) + src->refPos;
    dst->deletions = src->length;
    dst->insertions = 0;
    dst->bases = NULL;
}


static bool event_iter_pull_and_expand( event_iter * self, event_t * dst, bool * new_ref_entered )
{
    bool res = true;
    AlignmentT * alignment = &self->alignment;
    self->num_events = 0;
    *new_ref_entered = false;
    do
    {
        /* pull one alignment from the source... */
        bool res = alig_iter_get( self->source, alignment );
        if ( res )
        {
            /* check, to see if we have entered a new reference... */
            rc_t rc = event_iter_check_rname( self, &alignment->rname, alignment->pos, new_ref_entered );
            if ( rc == 0 )
            {
                /* in expandCIGAR.h ( expandCIGAR.cpp ), we have to call it to get the refLength ! */
                int valid = validateCIGAR( alignment->cigar.len, alignment->cigar.addr, &self->refLength, &self->seqLength );
                if ( valid )
                {
                    self->offset = 0;
                    self->event_index = 0;
                    self->num_events = expandCIGAR( self->events,
                                                    NUM_EVENTS,
                                                    self->offset,
                                                    &self->remaining,
                                                    alignment->cigar.len,
                                                    alignment->cigar.addr,
                                                    alignment->read.addr,
                                                    alignment->pos - 1,
                                                    self->fasta,
                                                    self->ref_index );
                    if ( self->num_events < 0 )
                        res = false
                }
                else
                    res = false;
            }
            else
                res = false;
    } while ( res && self->num_events == 0 );
    
    if ( res && self->num_events > 0 )
    {
        /* we now have an alignment and events in self->events, we can deliver into then given event-pointer */
        struct Event * src = &self->events[ self->event_index++ ];
        switch( ev->type )
        {
            case mismatch   : event_iter_mismatch( self, src, dst ); break;
            case insertion  : event_iter_insert( self, src, dst ); break;
            case deletion   : event_iter_delete( self, src, dst ); break;
        }
        self->state = STATE_EVENTS_AVAILABLE;
    }
    return res;
}


static bool event_iter_expand_remaining( event_iter * self, event_t * event )
{

}


static bool event_iter_events_availiale( event_iter * self, event_t * event )
{

}


/* get the next event from the event-iterator */
bool event_iter_get( struct event_iter * self, event_t * event, bool * new_ref_entered )
{
    bool res = false;
    if ( self != NULL && event != NULL )
    {
        if ( new_ref_entered != NULL )
            *new_ref_entered = false;
            
        switch( self->state )
        {
            case STATE_PULL_ALIGNMENT_AND_EXPAND_CIGAR  : res = event_iter_pull_and_expand( self, event, new_ref_entered ); break;
            case STATE_EXPAND_REMAINING_CIGAR           : res = event_iter_expand_remaining( self, event ); break;
            case STATE_EVENTS_AVAILABLE                 : res = event_iter_events_availiable( self, event ); break;
        }
    }
    return res;
}
