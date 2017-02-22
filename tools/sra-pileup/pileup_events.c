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

typedef struct event_counters
{
    uint32_t deletes;
    uint32_t inserts;
    uint32_t mismatches;
} event_counters;

static rc_t line( void )
{
    return KOutMsg( "\n-------------------------------------------------------------\n" );
}

static void from_4na_to_DNA( char * buffer, size_t len )
{
    size_t i;
    for ( i = 0; i < len; ++i )
        buffer[ i ] = _4na_to_ascii( buffer[ i ], false );
}

static rc_t on_insert( walk_data * data )
{
    rc_t rc = 0;
    const INSDC_4na_bin *bases;
    uint32_t n = ReferenceIteratorBasesInserted ( data->ref_iter, &bases );
    if ( bases != NULL )
    {
        uint32_t i;
        line();
        rc = KOutMsg( "%s\t%u\tI\t", data->ref_name, data->ref_pos );
        for ( i = 0; i < n && rc == 0; ++i )
            rc = KOutMsg( "%c", _4na_to_ascii( bases[ i ], false ) );
        if ( rc == 0 )
            rc = KOutMsg( "\n" );
    }
    return rc;
}

static rc_t on_delete( walk_data * data )
{
    rc_t rc = 0;
    const INSDC_4na_bin *bases;
    INSDC_coord_zero ref_pos;
    uint32_t n = ReferenceIteratorBasesDeleted ( data->ref_iter, &ref_pos, &bases );
    if ( bases != NULL )
    {
        uint32_t i;            
        line();
        rc = KOutMsg( "%s\t%u\tD\t", 
                 data->ref_name, data->ref_pos );
        for ( i = 0; i < n && rc == 0; ++i )
            rc = KOutMsg( "%c", _4na_to_ascii( bases[ i ], false ) );
        if ( rc == 0 )
            rc = KOutMsg( "\n" );
        free( (void *) bases );
    }
    return rc;
}

#define REF_SLICE 100

static rc_t on_mismatch( walk_data * data )
{
    RefVariation * rv;
    char ref[ REF_SLICE ];
    size_t deletion_pos = REF_SLICE / 2;
    size_t ref_start = data->ref_pos - deletion_pos;
    INSDC_coord_len written;
    
    rc_t rc = ReferenceObj_Read( data->ref_obj, ref_start, REF_SLICE, ( uint8_t * )&ref, &written );
    if ( rc == 0 )
    {
        size_t ref_len = written;
        char insertion[ 2 ];
        size_t insertion_len = 1;
        size_t deletion_len = 1;
        insertion[ 0 ] = _4na_to_ascii( data->state, false ); /* we have to translate to DNA! */

        line();
        from_4na_to_DNA( ref, ref_len ); /* we have to translate to DNA! */
        rc = RefVariationIUPACMake( &rv, ref, ref_len, deletion_pos-1, deletion_len, insertion, insertion_len, refvarAlgRA );
        if ( rc == 0 )
        {
            INSDC_dna_text const * allele;
            size_t allele_len, allele_start;
            rc = RefVariationGetAllele( rv, &allele, &allele_len, &allele_start );
            if ( rc == 0 )
            {
                size_t allele_pos = ( data->ref_pos + deletion_pos ) - allele_start;
                
                KOutMsg( "%.*s\n", ref_len, ref );
                rc = KOutMsg( "%s\t%u\tM\t%c\t%.*s\t%u.%u\n",
                    data->ref_name,
                    data->ref_pos,
                    insertion[ 0 ],
                    allele_len, allele,
                    allele_pos,
                    allele_len );
            }
            RefVariationRelease ( rv );
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
        /* INSERT-event */
        if ( ( state & align_iter_insert ) == align_iter_insert )
            rc = on_insert( data );

        /* DELETE-event */
        if ( ( state & align_iter_skip ) == align_iter_skip )
            rc = on_delete( data );
        
        /* MISMATCH-event */
        if ( ( state & align_iter_match ) != align_iter_match )
            rc = on_mismatch( data );
    }
    return rc;
}


rc_t walk_events( ReferenceIterator *ref_iter, pileup_options * options )
{
    walk_data data;
    walk_funcs funcs;

    event_counters v_counters;

    data.ref_iter = ref_iter;
    data.options = options;
    data.data = &v_counters;

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
