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

#include "align_iter.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_klib_data_buffer_
#include <klib/data-buffer.h>
#endif

#include <klib/out.h>

typedef struct alit_t {
    struct cmn_iter_t * cmn;
    uint32_t cur_idx_raw_read;
    uint32_t cur_idx_spot_id;
    uint32_t cur_idx_seq_read_id;
    bool uses_read_id;
} alit_t;


rc_t alit_create( const cmn_iter_params_t * params, alit_t ** iter, bool uses_read_id ) {
    rc_t rc = 0;
    alit_t * self = calloc( 1, sizeof * self );
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_fastq_tbl_iter.calloc( %d ) -> %R", ( sizeof * self ), rc );
    } else {
        self -> uses_read_id = uses_read_id;
        rc = cmn_iter_make( params, "PRIMARY_ALIGNMENT", &( self -> cmn ) );
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( self -> cmn, "RAW_READ", &( self -> cur_idx_raw_read ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_add_column( self -> cmn, "SEQ_SPOT_ID", &( self -> cur_idx_spot_id ) );
        }
        if ( 0 == rc && uses_read_id ) {
            rc = cmn_iter_add_column( self -> cmn, "SEQ_READ_ID", &( self -> cur_idx_seq_read_id ) );
        }
        if ( 0 == rc ) {
            rc = cmn_iter_detect_range( self -> cmn, self -> cur_idx_raw_read );
        }
        if ( 0 != rc ) {
            alit_release( self );
        } else {
            *iter = self;
        }
    }
    return rc;
}

void alit_release( alit_t * self ) {
    if ( NULL != self ) {
        cmn_iter_release( self -> cmn );
        free( ( void * ) self );
    }
}

bool alit_get_rec( alit_t * self, alit_rec_t * rec, rc_t * rc ) {
    rc_t rc2;
    bool res = cmn_iter_get_next( self -> cmn, &rc2 );
    if ( res ) {
        rc_t rc1 = 0;
        
        rec -> row_id = cmn_iter_get_row_id( self -> cmn );
        rc1 = cmn_iter_read_String( self -> cmn, self -> cur_idx_raw_read, &( rec -> read ) );
        
        if ( 0 == rc1 ) {
            rc1 = cmn_iter_read_uint64( self -> cmn, self -> cur_idx_spot_id, &( rec -> spot_id ) );
        } else {
            rec -> spot_id = 0;
        }
        
        if ( 0 == rc1 && self -> uses_read_id ) {
            rc1 = cmn_iter_read_uint32( self -> cmn, self -> cur_idx_seq_read_id, &( rec -> read_id ) );
        } else {
            rec -> read_id = 0;
        }
        
        if ( NULL != rc ) { *rc = rc1; }
    }
    return res;
}

uint64_t alit_get_row_count( struct alit_t * self ) {
    return cmn_iter_get_row_count( self -> cmn );
}

rc_t alit_extract_row_count( KDirectory * dir,
                             const VDBManager * vdb_mgr,
                             const char * accession_short,
                             const char * accession_path,
                             size_t cur_cache,
                             uint64_t * res ) {
    rc_t rc;
    cmn_iter_params_t cp;
    struct alit_t * iter;
    cmn_iter_populate_params( &cp,
                              dir,
                              vdb_mgr,
                              accession_short,
                              accession_path,
                              cur_cache,
                              0,
                              0 );
    rc = alit_create( &cp, &iter, false );
    if ( 0 == rc ) {
        *res = alit_get_row_count( iter );
        alit_release( iter );
    }
    return rc;
}
