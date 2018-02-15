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

#include "csra-consumer.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_seq_iter_
#include "seq-iter.h"
#endif

#ifndef _h_klib_time_
#include <klib/time.h>
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

/* the seq-rec with values from the lookup */
typedef struct csra_rec
{
    seq_rec rec; /* seq-iter.h */
    
    lookup_entry looked_up[ 2 ];
    bool lookup_found[ 2 ];
} csra_rec;

typedef struct csra_rec_copy
{
    csra_rec rec;

    uint64_t prim_alig_id[ 2 ];
    uint8_t alig_count[ 2 ];
    uint32_t read_len[ 2 ];
    uint8_t read_type[ 2 ];
    
} csra_rec_copy;


static uint32_t validate_alig_count_vs_alig_id( const seq_rec * seq, uint32_t idx, struct logger * log )
{
    uint32_t res = 0;
    uint8_t count = seq -> alig_count[ idx ];
    if ( count == 0 )
    {
        if ( seq -> prim_alig_id[ idx ] != 0 )
        {
            log_write( log, "SEQ.#%ld : ALIGNMENT_COUNT[ %u ] == 0, PRIMARY_ALIGNMENT_ID[ %u ] = %ld",
                       seq -> row_id, idx, idx, seq -> prim_alig_id[ idx ] );
            res++;
        }
    }
    else if ( count == 1 )
    {
        if ( seq -> prim_alig_id[ idx ] == 0 )
        {
            log_write( log, "SEQ.#%ld : ALIGNMENT_COUNT[ %u ] == 1, PRIMARY_ALIGNMENT_ID[ %u ] == 0",
                       seq -> row_id, idx, idx );
            res++;
        }
    }
    else
    {
        log_write( log, "SEQ.#%ld : ALIGNMENT_COUNT[ %u ] = %u", seq -> row_id, idx, count );
        res++;
    }
    return res;
}

static uint32_t validate_csra_rec( validate_slice * slice, const csra_rec * csra )
{
    uint32_t res = 0;
    const seq_rec * seq = &csra -> rec;
    
    /* check if SEQUENCE.ALIGNMENT_COUNT and SEQUENCE.PRIMARY_ALIGNMENT_ID match */
    if ( seq -> num_alig_id != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%,ld : rowlen( PRIMARY_ALIGNMENT_ID ) != 2", seq -> row_id );
        res++;
    }
    if ( seq -> num_alig_count != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%,ld : rowlen( ALIGNMENT_COUNT ) != 2", seq -> row_id );
        res++;
    }
    res += validate_alig_count_vs_alig_id( seq, 0, slice -> vctx -> log );
    res += validate_alig_count_vs_alig_id( seq, 1, slice -> vctx -> log );

    if ( seq -> num_read_len != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%,ld : rowlen( READ_LEN ) != 2", seq -> row_id );
        res++;
    }
    if ( seq -> num_read_type != 2 )
    {
        log_write( slice -> vctx -> log, "SEQ.#%,ld : rowlen( READ_TYPE ) != 2", seq -> row_id );
        res++;
    }

    /* if we have an alignment for READ #1 */
    if ( seq -> prim_alig_id[ 0 ] > 0 )
    {
        /* does READ_LEN match? */
        if ( seq -> read_len[ 0 ] != csra -> looked_up[ 0 ] . read_len )
        {
            log_write( slice -> vctx -> log, "SEQ.#%,ld : READ_LEN[ 0 ] != PRIM.READ_LEN[ 0 ]", seq -> row_id );
            res++;
        }
        /* does the alignment refere to READ#1 ? */
        if ( csra -> looked_up[ 0 ] . seq_read_id != 1 )
        {
            log_write( slice -> vctx -> log, "SEQ.#%,ld [ 0 ] : PRIM.READ_ID != 1 ( %u )", seq -> row_id, csra -> looked_up[ 1 ] . seq_read_id );
            res++;
        }
        /* does the orientation match ? */
        if ( !csra -> looked_up[ 0 ] . ref_orient )
        {
            if ( ( seq -> read_type[ 0 ] & READ_TYPE_FORWARD ) != READ_TYPE_FORWARD )
            {
                log_write( slice -> vctx -> log, "SEQ.#%,ld [ 0 ] : ORIENTATION mismatch", seq -> row_id );
                res++;
            }
        }
        else
        {
            if ( ( seq -> read_type[ 0 ] & READ_TYPE_REVERSE ) != READ_TYPE_REVERSE )
            {
                log_write( slice -> vctx -> log, "SEQ.#%,ld [ 0 ] : ORIENTATION mismatch", seq -> row_id );
                res++;
            }
        }
    }
    
    /* if we have an alignment for READ #2 */
    if ( seq -> prim_alig_id[ 1 ] > 0 )
    {
        /* does READ_LEN match? */    
        if ( seq -> read_len[ 1 ] != csra -> looked_up[ 1 ] . read_len )
        {
            log_write( slice -> vctx -> log, "SEQ.#%,ld : READ_LEN[ 1 ] != PRIM.READ_LEN[ 1 ]", seq -> row_id );
            res++;
        }
        /* does the alignment refere to READ#2 ? */
        if ( csra -> looked_up[ 1 ] . seq_read_id != 2 )
        {
            log_write( slice -> vctx -> log, "SEQ.#%,ld [ 1 ] : PRIM.READ_ID != 2 ( %u )", seq -> row_id, csra -> looked_up[ 1 ] . seq_read_id );
            res++;
        }
        /* does the orientation match ? */
        if ( !csra -> looked_up[ 1 ] . ref_orient )
        {
            if ( ( seq -> read_type[ 1 ] & READ_TYPE_FORWARD ) != READ_TYPE_FORWARD )
            {
                log_write( slice -> vctx -> log, "SEQ.#%,ld [ 1 ] : ORIENTATION mismatch", seq -> row_id );
                res++;
            }
        }
        else
        {
            if ( ( seq -> read_type[ 1 ] & READ_TYPE_REVERSE ) != READ_TYPE_REVERSE )
            {
                log_write( slice -> vctx -> log, "SEQ.#%,ld [ 1 ] : ORIENTATION mismatch", seq -> row_id );
                res++;
            }
        }
    }
    
    {
        /* does the sum of READ_LEN add up to the length of QUALITY? */
        uint32_t n_qual = seq -> num_qual;
        uint32_t rd_len_0 = seq -> read_len[ 0 ];
        uint32_t rd_len_1 = seq -> read_len[ 1 ];
        if ( n_qual != ( rd_len_0 + rd_len_1 ) )
        {
            log_write( slice -> vctx -> log, "SEQ.#%,ld : mismatch between sum( READ_LEN %u + %u ) and len( QUALITY %u )",
                        seq -> row_id, rd_len_0, rd_len_1, n_qual );
            res++;
        }
    }
    return res;
}

static rc_t fill_in_lookup_values( csra_rec * csra, struct prim_lookup * lookup, bool * ready )
{
    rc_t rc = 0;

    uint64_t prim_alig_id = csra -> rec . prim_alig_id[ 0 ];
    if ( prim_alig_id > 0 )
    {
        if ( !csra -> lookup_found[ 0 ] )
            rc = prim_lookup_get( lookup, prim_alig_id, &csra -> looked_up[ 0 ], &csra -> lookup_found[ 0 ] );
    }
    else
        csra -> lookup_found[ 0 ] = true;
        
    if ( rc == 0 )
    {
        prim_alig_id = csra -> rec . prim_alig_id[ 1 ];
        
        if ( prim_alig_id > 0 )
        {
            if ( !csra -> lookup_found[ 1 ] )
                rc = prim_lookup_get( lookup, prim_alig_id, &csra -> looked_up[ 1 ], &csra -> lookup_found[ 1 ] );
        }
        else
            csra -> lookup_found[ 1 ] = true;
    }
    if ( rc == 0 )
        *ready = ( csra -> lookup_found[ 0 ] && csra -> lookup_found[ 1 ] );
    return rc;
}

static csra_rec_copy * make_copy( const csra_rec * src )
{
    /* problem: with have to make a deep-copy!!! */
    csra_rec_copy * res = malloc( sizeof * res );
    if ( res != NULL )
    {
        res -> rec . rec . row_id       = src -> rec . row_id;
        
        res -> rec . rec . num_qual     = src -> rec . num_qual;
        res -> rec . rec . num_alig_id  = src -> rec . num_alig_id;
        res -> rec . rec . prim_alig_id = &res -> prim_alig_id[ 0 ];
        res -> prim_alig_id[ 0 ]    = src -> rec . prim_alig_id[ 0 ];
        res -> prim_alig_id[ 1 ]    = src -> rec . prim_alig_id[ 1 ];
        
        res -> rec . rec . num_alig_count   = src -> rec . num_alig_count;
        res -> rec . rec. alig_count        = &res -> alig_count[ 0 ];
        res -> alig_count[ 0 ]      = src -> rec . alig_count[ 0 ];
        res -> alig_count[ 1 ]      = src -> rec . alig_count[ 1 ];

        res -> rec . rec . num_read_len     = src -> rec . num_read_len;
        res -> rec . rec . read_len         = &res -> read_len[ 0 ];
        res -> read_len[ 0 ]        = src -> rec . read_len[ 0 ];
        res -> read_len[ 1 ]        = src -> rec . read_len[ 1 ];

        res -> rec . rec . num_read_type    = src -> rec. num_read_type;
        res -> rec . rec . read_type        = &res -> read_type[ 0 ];
        res -> read_type [ 0 ]      = src -> rec . read_type[ 0 ];
        res -> read_type [ 1 ]      = src -> rec . read_type[ 1 ];
        
        res -> rec . looked_up[ 0 ] = src -> looked_up[ 0 ];
        res -> rec . looked_up[ 1 ] = src -> looked_up[ 1 ];
        
        res -> rec . lookup_found[ 0 ] = src -> lookup_found[ 0 ];
        res -> rec . lookup_found[ 1 ] = src -> lookup_found[ 1 ];
    }
    return res;
}

static void CC on_backlog_free( void * item, void * data )
{
    free( item );
}

static rc_t handle_backlog( validate_slice * slice,
                            Vector * backlog,
                            uint32_t * count )
{
    rc_t rc = 0;
    uint32_t n = VectorLength( backlog );
    uint32_t idx = 0;
    while ( rc == 0 && idx < n )
    {
        csra_rec_copy * c = VectorGet ( backlog, idx );
        bool found;
        rc = fill_in_lookup_values( &c -> rec, slice -> lookup, &found );
        if ( rc == 0 && found )
        {
            void * ptr;
            
            uint32_t errors = validate_csra_rec( slice, &c -> rec );
            rc = update_seq_validate_result( slice -> vctx -> v_res, errors );
            if ( rc == 0 )
                update_progress( slice -> vctx -> progress, 1 );
                
            rc = VectorRemove ( backlog, idx, &ptr );
            n--;
        }
        else
            idx++;
    }
    if ( count != NULL )
        *count = VectorLength( backlog );
    return rc;
}

rc_t CC csra_consumer_thread( const KThread *self, void *data )
{
    validate_slice * slice = data;
    cmn_params p = { slice -> vctx -> dir,
                     slice -> vctx -> acc_info . accession,
                     slice -> first_row,
                     slice -> row_count,
                     slice -> vctx -> cursor_cache }; /* cmn-iter.h */
    struct seq_iter * iter;
        
    rc_t rc = make_seq_iter( &p, &iter );
    if ( rc == 0 )
    {
        csra_rec csra;
        Vector backlog;
        bool running = true;
        
        VectorInit ( &backlog, 0, 512 );
        
        while( rc == 0 && running )
        {
            /* get the SEQ-record */
            running = get_from_seq_iter( iter, &csra . rec, &rc );
            if ( running )
            {
                bool found;
                
                /* try to get the lookup-data */
                csra . lookup_found[ 0 ] = false;
                csra . lookup_found[ 1 ] = false;
                
                rc = fill_in_lookup_values( &csra, slice -> lookup, &found );
                if ( rc == 0 )
                {
                    if ( found )
                    {
                        /* all joins ( if any ) could be made, do the validation */
                        uint32_t errors = validate_csra_rec( slice, &csra );
                        rc = update_seq_validate_result( slice -> vctx -> v_res, errors );
                        if ( rc == 0 )
                            update_progress( slice -> vctx -> progress, 1 );
                    }
                    else
                        /* some/all joins are not read( yet ) put this one on the back-log */
                        rc = VectorAppend ( &backlog, NULL, make_copy( &csra ) );
                }
            }
            
            /* try to make progress on the backlog */
            rc = handle_backlog( slice, &backlog, NULL );
        }
        
        if ( rc == 0 )
        {
            uint32_t count = VectorLength( &backlog );
            while ( rc == 0 && count > 0 )
                rc = handle_backlog( slice, &backlog, &count );
        }
        
        VectorWhack ( &backlog, on_backlog_free, NULL );

        destroy_seq_iter( iter );
    }
    
    finish_validate_result( slice -> vctx -> v_res );
    free( data );
    return rc;
}
