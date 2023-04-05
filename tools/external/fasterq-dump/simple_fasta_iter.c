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

#include "simple_fasta_iter.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

struct simple_fasta_iter_t;
typedef struct simple_fasta_iter_t {
    struct cmn_iter_t * cmn;
    uint32_t read_id;
} simple_fasta_iter_t;

rc_t sfai_create( const cmn_iter_params_t * params, const char * tbl_name,
                  struct simple_fasta_iter_t ** iter ) {
    rc_t rc = 0;
    simple_fasta_iter_t * self = calloc( 1, sizeof * self );
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_fastq_csra_iter.calloc( %d ) -> %R", ( sizeof * self ), rc );
    } 
    if ( 0 == rc ) {
        rc = cmn_iter_make( params, tbl_name, &( self -> cmn ) ); /* cmn_iter.h */
    }
    if ( 0 == rc ) {
        rc = cmn_iter_add_column( self -> cmn, "READ", &( self -> read_id ) ); /* cmn_iter.h */
    }

    if ( 0 == rc ) {
        rc = cmn_iter_detect_range( self -> cmn, self -> read_id );
    }
    
    if ( 0 != rc ) {
        sfai_destroy( self );
    } else {
        *iter = self;
    }
    return rc;
}

void sfai_destroy( struct simple_fasta_iter_t * self ) {
    if ( NULL != self ) {
        cmn_iter_release( self -> cmn ); /* cmn_iter.h */
        free( ( void * ) self );
    }
}

bool sfai_get( struct simple_fasta_iter_t * self, String * read, rc_t * rc ) {
    rc_t rc2 = 0;
    bool res = cmn_iter_get_next( self -> cmn, &rc2 );
    if ( res ) {
        rc_t rc1 = cmn_iter_read_String( self -> cmn, self -> read_id, read );
        if ( NULL != rc ) { *rc = rc1; }
    }
    return res;
}

uint64_t sfai_get_row_count( struct simple_fasta_iter_t * self ) {
    return cmn_iter_get_row_count( self -> cmn );    
}
