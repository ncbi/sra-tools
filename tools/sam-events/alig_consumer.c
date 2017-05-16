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
#include "alig_consumer.h"

#include <klib/out.h>
#include <klib/printf.h>
#include <klib/namelist.h>
#include <search/ref-variation.h>
#include "allele_dict.h"

typedef struct alig_consumer
{
    /* given at constructor */
    const alig_consumer_data * config;
    
    /* to be given to the allel_dict */
    dict_data dict_data;
    
    /* detection of unsorted alignments */
    bool unsorted;
    VNamelist * seen_refs;
    uint64_t position;
    
    /* for handling references */
    int ref_index;
    const String * rname;
    const char * ref_bases;
    unsigned ref_bases_count;
    struct Allele_Dict * ad;
    struct Allele_Dict2 * ad2;
    
} alig_consumer;


/* releae an alignment-iterator */
rc_t alig_consumer_release( struct alig_consumer * self )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcNoTarg, rcReleasing, rcParam, rcNull );
    else
    {
        if ( self->ad != NULL )
            rc = allele_dict_release( self->ad );
        if ( self->ad2 != NULL )
            rc = allele_dict2_release( self->ad2 );
        if ( self->rname != NULL )
            StringWhack ( self->rname );
        if ( rc == 0 )
            rc = VNamelistRelease( self->seen_refs );
        free( ( void * ) self );
    }
    return rc;
}


static rc_t CC alig_consumer_print( const counters * count, const String * rname, size_t position,
                            uint32_t deletes, uint32_t inserts, const char * bases,
                            void * user_data )
{
    struct alig_consumer * self = user_data;
    const counters * limits = &self->config->limits;
    bool print = true;
    
    if ( limits->total > 0 )
        print = ( count->fwd + count->rev >= limits->total );
    if ( print && limits->fwd > 0 )
        print = ( count->fwd >= limits->fwd );
    if ( print && limits->rev > 0 )
        print = ( count->rev >= limits->rev );
    if ( print && limits->t_pos > 0 )
        print = ( count->t_pos >= limits->t_pos );
    if ( print && limits->t_neg > 0 )
        print = ( count->t_neg >= limits->t_neg );

    /*COUNT-FWD - COUNT-REV - COUNT-t+ - COUNT-t- - REFNAME - EVENT-POS - DELETES - INSERTS - BASES */
    
    if ( print )
    {
        if ( self->config->lookup != NULL )
        {
            char buffer[ 1024 ];
            size_t num_writ;
            if ( string_printf( buffer, sizeof buffer, &num_writ, "%S:%d:%d:%s", rname, position, deletes, bases ) == 0 )
            {
                String key;
                uint64_t values[ 2 ];
                StringInit( &key, buffer, num_writ, num_writ );
                if ( allele_lookup_perform( self->config->lookup, &key, values ) )
                {
                    return KOutMsg( "%d\t%d\t%d\t%d\t%S\t%d\t%d\t%d\t%s\t%lu\t%lX\n",
                                     count->fwd, count->rev, count->t_pos, count->t_neg,
                                     rname, position, deletes, inserts, bases, values[ 0 ], values[ 1 ] );
                }
            }
        }
        
        return KOutMsg( "%d\t%d\t%d\t%d\t%S\t%d\t%d\t%d\t%s\n",
                         count->fwd, count->rev, count->t_pos, count->t_neg,
                         rname, position, deletes, inserts, bases );
    }
    
    return 0;
}


/* construct an alignmet-iterator from an accession */
rc_t alig_consumer_make( struct alig_consumer ** self, const alig_consumer_data * config )
{
    rc_t rc = 0;
    if ( self == NULL || config == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcAllocating, rcParam, rcNull );
        log_err( "alig_consumer.alig_consumer_make() given a NULL-ptr" );
    }
    else
    {
        alig_consumer * o = calloc( 1, sizeof *o );
        *self = NULL;
        if ( o == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcAllocating, rcMemory, rcExhausted );
            log_err( "alig_consumer.alig_consumer_make() memory exhausted" );
        }
        else
        {
            o->config = config;
            
            /* fill out the data to be given to each dictionary instance */
            o->dict_data.event_func = alig_consumer_print;
            o->dict_data.purge = config->purge;
            o->dict_data.user_data = o;
            
            o->ref_index = -1;
            VNamelistMake( &o->seen_refs, 10 );
        }
        
        if ( rc == 0 )
            *self = o;
        else
            alig_consumer_release( o );
    }
    return rc;
}


/* we have a common store function because this is the place to filter... */
static rc_t alig_consumer_store_allele( struct alig_consumer * self,
                                        uint64_t position, uint32_t deletes, uint32_t inserts,
                                        const char * bases, bool fwd, bool first )
{
	bool store = true;

	if ( self->config->slice != NULL )
		store = filter_by_slice( self->config->slice, self->rname, position, inserts );

	if ( store )
    {
        if ( self->config->dict_strategy == 0 )
            return allele_dict_put( self->ad, position, deletes, inserts, bases, fwd, first );
        else
            return allele_dict2_put( self->ad2, position, deletes, inserts, bases, fwd, first );        
    }
	else
		return 0;
}

static rc_t alig_consumer_process_mismatch( struct alig_consumer * self,
                                            AlignmentT * alignment,
                                            struct Event * ev )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                              /* to be created */
                                     self->ref_bases,                       /* the reference-bases */
                                     self->ref_bases_count,                 /* length of the reference */
                                     ( alignment->pos - 1 ) + ev->refPos,  /* position of event */
                                     ev->length,                            /* length of deletion */
                                     &( alignment->read.addr[ ev->seqPos ] ),  /* inserted bases */
                                     ev->length,                            /* number of inserted bases */
                                     refvarAlgRA );
    if ( rc != 0 )
        log_err( "RefVariationIUPACMake() failed rc=%R", rc );
    else
    {
        rc_t rc2;
        INSDC_dna_text const * allele;
        size_t allele_len, allele_start;
        
        rc = RefVariationGetAllele( ref_var, &allele, &allele_len, &allele_start );
        if ( rc != 0 )
            log_err( "RefVariationGetAllele() failed rc=%R", rc );
        else
            /*                                     pos            del         ins         bases */
            rc = alig_consumer_store_allele( self, allele_start, ev->length, allele_len, allele, alignment->fwd, alignment->first );

        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


static rc_t alig_consumer_process_insert( struct alig_consumer * self,
                                          AlignmentT * alignment,
                                          struct Event * ev )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                              /* to be created */
                                     self->ref_bases,                       /* the reference-bases */
                                     self->ref_bases_count,                 /* length of the reference */
                                     ( alignment->pos - 1 ) + ev->refPos,  /* position of event */
                                     0,                                     /* length of deletion */
                                     &( alignment->read.addr[ ev->seqPos ] ),  /* inserted bases */
                                     ev->length,                            /* number of inserted bases */
                                     refvarAlgRA );
    if ( rc != 0 )
        log_err( "RefVariationIUPACMake() failed rc=%R", rc );
    else
    {
        rc_t rc2;
        INSDC_dna_text const * allele;
        size_t allele_len, allele_start;
        
        rc = RefVariationGetAllele( ref_var, &allele, &allele_len, &allele_start );
        if ( rc != 0 )
            log_err( "RefVariationGetAllele() failed rc=%R", rc );
        else
            /*                                     pos           del ins        bases */        
            rc = alig_consumer_store_allele( self, allele_start, 0, allele_len, allele, alignment->fwd, alignment->first );

        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


static rc_t alig_consumer_process_delete( struct alig_consumer * self,
                                          AlignmentT * alignment,
                                          struct Event * ev )
{
    RefVariation * ref_var;
    rc_t rc = RefVariationIUPACMake( &ref_var,                              /* to be created */
                                     self->ref_bases,                       /* the reference-bases */
                                     self->ref_bases_count,                 /* length of the reference */
                                     ( alignment->pos - 1 ) + ev->refPos,  /* position of event */
                                     ev->length,                            /* length of deletion */
                                     NULL,                                  /* inserted bases */
                                     0,                                     /* number of inserted bases */
                                     refvarAlgRA );
    if ( rc != 0 )
        log_err( "RefVariationIUPACMake() failed rc=%R", rc );
    else
    {
        rc_t rc2;
        INSDC_dna_text const * allele;
        size_t allele_len, allele_start;
        
        rc = RefVariationGetAllele( ref_var, &allele, &allele_len, &allele_start );
        if ( rc != 0 )
            log_err( "RefVariationGetAllele() failed rc=%R", rc );
        else
            /*                                     pos           del         ins bases */        
            rc = alig_consumer_store_allele( self, allele_start, ev->length, 0, NULL, alignment->fwd, alignment->first );
        
        rc2 = RefVariationRelease( ref_var );
        if ( rc2 != 0 )
            log_err( "RefVariationRelease() failed rc=%R", rc2 );
    }
    return rc;
}


static rc_t alig_consumer_process_events( struct alig_consumer * self,
                                          AlignmentT * alignment,
                                          unsigned refLength,
                                          struct Event * const events,
                                          int num_events )
{
    rc_t rc = 0;
    int idx;    
    for ( idx = 0; rc == 0 && idx < num_events; ++idx )
    {
        struct Event * ev = &events[ idx ];
        switch( ev->type )
        {
            case mismatch   : rc = alig_consumer_process_mismatch( self, alignment, ev ); break;
            case insertion  : rc = alig_consumer_process_insert( self, alignment, ev ); break;
            case deletion   : rc = alig_consumer_process_delete( self, alignment, ev ); break;
        }
    }
    return rc;
}


static rc_t alig_consumer_switch_reference( struct alig_consumer * self, const String * rname )
{
    rc_t rc = 0;
    
    /* prime the current-ref-name with the ref-name of the alignment */
    if ( self->rname != NULL )
        StringWhack ( self->rname );
    rc = StringCopy( &self->rname, rname );
    
    if ( self->config->fasta != NULL )
        self->ref_index = FastaFile_getNamedSequence( self->config->fasta, rname->size, rname->addr );
    else
        self->ref_index = -1;
        
    if ( self->ref_index < 0 )
    {
        rc = RC( rcExe, rcNoTarg, rcVisiting, rcId, rcInvalid );
        log_err( "'%S' not found in fasta-file", rname );
    }
    else
        self->ref_bases_count = FastaFile_getSequenceData( self->config->fasta, self->ref_index, &self->ref_bases );
    return rc;
}


static rc_t alig_consumer_check_rname( struct alig_consumer * self, const String * rname, uint64_t position )
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
        if ( rc == 0 )
        {
            if ( self->ad != NULL )
                rc = allele_dict_release( self->ad );
            if ( self->ad2 != NULL )
                rc = allele_dict2_release( self->ad2 );
        }
        
        /* switch to the new reference!
           - store the new refname in the current-struct
           - get the index of the new reference into the fasta-file
        */
        if ( rc == 0 )
            rc = alig_consumer_switch_reference( self, rname );
            
        /* make a new allele-dict */
        if ( rc == 0 )
        {
            if ( self->config->dict_strategy == 0 )
                rc = allele_dict_make( &self->ad, rname, &self->dict_data );
            else
                rc = allele_dict2_make( &self->ad2, rname, &self->dict_data );
        }
    }
    else
    {
        if ( position < self->position )
        {
            self->unsorted = true;
            rc = RC( rcApp, rcNoTarg, rcInserting, rcParam, rcInvalid );
            log_err( "alig_consumer_check_rname() unsorted %lu ---> %lu", self->position, position );
        }
    }
    self->position = position;
    
    return rc;
}


#define NUM_EVENTS 1024

rc_t alig_consumer_consume_alignment( struct alig_consumer * self, AlignmentT * alignment )
{
    rc_t rc = alig_consumer_check_rname( self, &alignment->rname, alignment->pos );
    if ( rc == 0 )
    {
        unsigned refLength;
        unsigned seqLength;
        /* in expandCIGAR.h ( expandCIGAR.cpp ), we have to call it to get the refLength ! */
        int valid = validateCIGAR( alignment->cigar.len, alignment->cigar.addr, &refLength, &seqLength );

        if ( valid == 0 )
        {
            int remaining = 1;
            int offset = 0;
            struct Event events[ NUM_EVENTS ];
         
            while ( rc == 0 && remaining > 0 )
            {
                /* in expandCIGAR.h ( expandCIGAR.cpp ) */
                int num_events = expandCIGAR( events,
                                              NUM_EVENTS,
                                              offset,
                                              &remaining,
                                              alignment->cigar.len,
                                              alignment->cigar.addr,
                                              alignment->read.addr,
                                              alignment->pos - 1,
                                              self->config->fasta,
                                              self->ref_index );
                if ( num_events < 0 )
                    log_err( "expandCIGRAR failed for cigar '%S'", &alignment->cigar );
                else
                {
                    rc = alig_consumer_process_events( self, alignment, refLength, events, num_events );
                    offset += num_events;
                }
            }
        }
    }
    return rc;
}


bool alig_consumer_get_unsorted( struct alig_consumer * self )
{
    if ( self != NULL ) return self->unsorted;
    return false;
}

