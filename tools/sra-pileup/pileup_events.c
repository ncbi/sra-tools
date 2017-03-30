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

#include <klib/out.h>
#include <search/ref-variation.h>

#include "ref_walker_0.h"
#include "4na_ascii.h"

#define REF_SLICE 100


const char * stars = "****************************************************************************************************";

typedef struct e_ctx
{
    char ref[ REF_SLICE ];
    char indel[ REF_SLICE ];
    size_t ref_slice_start, allele_len, allele_start, rel_event_pos;
    INSDC_coord_len ref_len, deletion_len, insertion_len;
    INSDC_dna_text const * allele;
    RefVariation * rv;
} e_ctx;


/* read the reference around the event-position -/+ 50 bases */
static rc_t read_ref( walk_data * data )
{
    e_ctx * ec = data->data;
    ec->ref_slice_start = data->ref_pos - ( ( sizeof ec->ref ) / 2 );
    rc_t rc = ReferenceObj_Read( data->ref_obj,
                                 ec->ref_slice_start,
                                 sizeof ec->ref,
                                 ( uint8_t * )&ec->ref,
                                 &ec->ref_len );
    if ( rc == 0 )
    {
        size_t i;
        for ( i = 0; i < ec->ref_len; ++i )
            ec->ref[ i ] = _4na_to_ascii( ec->ref[ i ], false );
    }
    return rc;
}


static rc_t on_insert( walk_data * data )
{
    rc_t rc = 0;
    e_ctx * ec = data->data;
    const INSDC_4na_bin *bases;
    
    ec->insertion_len = ReferenceIteratorBasesInserted ( data->ref_iter, &bases );
    if ( bases != NULL )
    {
        uint32_t i;

        /* translate the inserted bases from 4na to DNA */
        for ( i = 0; i < ec->insertion_len; ++i )
            ec->indel[ i ] = _4na_to_ascii( bases[ i ], false );
        ec->deletion_len = 0;

        rc = RefVariationIUPACMake( &ec->rv,
                                    ec->ref,
                                    ec->ref_len,
                                    ec->rel_event_pos + 1,
                                    ec->deletion_len,
                                    ec->indel,
                                    ec->insertion_len,
                                    refvarAlgRA );
        if ( rc == 0 )
        {
            rc = RefVariationGetAllele( ec->rv, &ec->allele, &ec->allele_len, &ec->allele_start );
            if ( rc == 0 )
            {

                KOutMsg( "\nINSERT: ref_start = %d, rel_allele_start = %d\n", ec->ref_slice_start, ec->allele_start );
                
                /* write the reference with a gap to showcase the insert */
                KOutMsg( "%.*s%.*s%.*s\n",
                         ec->rel_event_pos + 1, ec->ref,
                         ec->insertion_len, stars,
                         ec->ref_len - ( ec->rel_event_pos + 1 ), &ec->ref[ ec->rel_event_pos + 1 ] );
                /* write the inseted bases where the gap is */
                KOutMsg( "%*s%.*s\n",
                         ec->rel_event_pos + 1, " ",
                         ec->insertion_len, ec->indel );
                /* highlight where the allele is */
                KOutMsg( "%*s%.*s\n", ec->allele_start + 1, "*", ec->allele_len - 1, stars );

                KOutMsg( "%s\t%u\t%c\t+\t%.*s\t%.*s\t%u.%u\n",
                    data->ref_name,
                    data->ref_pos,
                    _4na_to_ascii( data->ref_base, false ),
                    ec->insertion_len, ec->indel,
                    ec->allele_len, ec->allele,
                    ec->ref_slice_start + ec->allele_start, ec->allele_len );
            }
        }
    }
    return rc;
}


static rc_t on_delete( walk_data * data )
{
    rc_t rc = 0;
    e_ctx * ec = data->data;
    const INSDC_4na_bin *bases;
    INSDC_coord_zero ref_pos;
    ec->deletion_len = ReferenceIteratorBasesDeleted ( data->ref_iter, &ref_pos, &bases );
    if ( bases != NULL )
    {
        uint32_t i;            

        /* translate the deleted bases from 4na to DNA */
        for ( i = 0; i < ec->deletion_len; ++i )
            ec->indel[ i ] = _4na_to_ascii( bases[ i ], false );
        ec->insertion_len = 0;
        
        rc = RefVariationIUPACMake( &ec->rv,
                                    ec->ref,
                                    ec->ref_len,
                                    ec->rel_event_pos + 2,
                                    ec->deletion_len,
                                    NULL,
                                    ec->insertion_len,
                                    refvarAlgRA );
        if ( rc == 0 )
        {
            rc = RefVariationGetAllele( ec->rv, &ec->allele, &ec->allele_len, &ec->allele_start );
            if ( rc == 0 )
            {

                KOutMsg( "\nDELETE: ref_start = %d, allele_start = %d\n", ec->ref_slice_start, ec->allele_start );        
                /* write the reference as is */
                KOutMsg( "%.*s\n", ec->ref_len, ec->ref );
                /* highlight where the deletion is */
                KOutMsg( "%*s%.*s\n", ec->rel_event_pos + 2, "*", ec->deletion_len - 1, stars );

                KOutMsg( "%s\t%u\t%c\t-\t%.*s\t%.*s\t%u.%u\n",
                    data->ref_name,
                    data->ref_pos,
                    _4na_to_ascii( data->ref_base, false ),
                    ec->deletion_len, ec->indel,
                    ec->allele_len, ec->allele,
                    ec->ref_slice_start + ec->allele_start, ec->allele_len );
            }
        }
        free( (void *) bases );
    }
    return rc;
}


static rc_t on_mismatch( walk_data * data )
{
    rc_t rc;
    e_ctx * ec = data->data;
    
    ec->indel[ 0 ] = _4na_to_ascii( data->state, false ); /* we have to translate to DNA! */
    ec->indel[ 1 ] = 0;
    ec->insertion_len = 1;
    ec->deletion_len = 1;
    
    rc = RefVariationIUPACMake( &ec->rv,
                                ec->ref,
                                ec->ref_len,
                                ec->rel_event_pos,
                                ec->deletion_len,
                                ec->indel,
                                ec->insertion_len,
                                refvarAlgRA );
    if ( rc == 0 )
    {
        rc = RefVariationGetAllele( ec->rv, &ec->allele, &ec->allele_len, &ec->allele_start );
        if ( rc == 0 )
        {
            KOutMsg( "\nMISMATCH: ref_start = %d, rel_allele_start = %d\n", ec->ref_slice_start, ec->allele_start );
            /* write the reference as is */
            KOutMsg( "%.*s\n", ec->ref_len, ec->ref );
            /* write the base that is the mismatch at its position */
            KOutMsg( "%*s\n", ec->rel_event_pos + 1, ec->indel );
            /* highlight where the allele is */
            KOutMsg( "%*s%.*s\n", ec->allele_start + 1, "*", ec->allele_len - 1, stars );
            
            rc = KOutMsg( "%s\t%u\t%c\t!=\t%c\t%.*s\t%u.%u\n",
                data->ref_name,
                data->ref_pos,
                _4na_to_ascii( data->ref_base, false ),
                ec->indel[ 0 ],
                ec->allele_len, ec->allele,
                ec->ref_slice_start + ec->allele_start, ec->allele_len );
        }
    }
    return rc;
}

static rc_t CC walk_events_placement( walk_data * data )
{
    rc_t rc = 0;
    int32_t state = data->state;
    if ( ( state & align_iter_invalid ) != align_iter_invalid )
    {
        uint32_t event_type = 0;
        /* the event types are defined in <align/iterator.h> */
        
        /* INSERT-event */
        if ( ( state & align_iter_insert ) == align_iter_insert )
            event_type = align_iter_insert;

        /* DELETE-event */
        else if ( ( state & align_iter_skip ) == align_iter_skip )
            event_type = align_iter_skip;
        
        /* MISMATCH-event */
        else  if ( ( state & align_iter_match ) != align_iter_match )
            event_type = align_iter_match;
        
        if ( event_type != 0 )
        {
            rc = read_ref( data );
            if ( rc == 0 )
            {
                e_ctx * ec = data->data;

                ec->rv = NULL;
                switch( event_type )
                {
                    case align_iter_insert :    if ( data->options->process_inserts )
                                                    rc = on_insert( data );
                                                break;
                                                
                    case align_iter_skip :      if ( data->options->process_deletes )
                                                    rc = on_delete( data );
                                                break;

                    case align_iter_match :     if ( data->options->process_mismatches )
                                                    rc = on_mismatch( data );
                                                break;

                }
                if ( ec->rv != NULL )
                    RefVariationRelease( ec->rv );
            }
        }
    }
    return rc;
}


rc_t walk_events( ReferenceIterator *ref_iter, pileup_options * options )
{
    walk_data data;
    walk_funcs funcs;

    e_ctx ec;

    ec.rel_event_pos = ( ( sizeof ec.ref ) / 2 );
    
    data.ref_iter = ref_iter;
    data.options = options;
    data.data = &ec;

    funcs.on_enter_ref = NULL;
    funcs.on_exit_ref = NULL;

    funcs.on_enter_ref_window = NULL;
    funcs.on_exit_ref_window = NULL;

    funcs.on_enter_ref_pos = NULL;
    funcs.on_exit_ref_pos = NULL;

    funcs.on_enter_spotgroup = NULL;
    funcs.on_exit_spotgroup = NULL;

    funcs.on_placement = walk_events_placement;

    return walk_0( &data, &funcs );
}
