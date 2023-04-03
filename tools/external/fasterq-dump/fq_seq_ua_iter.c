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

#include "fq_seq_ua_iter.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_klib_data_buffer_
#include <klib/data-buffer.h>
#endif

/* this is for unaligned FASTQ ( used by tbl_join.c ) */

typedef struct fq_seq_ua_iter_t {
    struct cmn_iter_t * cmn;
    fq_seq_ua_opt_t opt;
    KDataBuffer qual_buffer;  /* klib/databuffer.h */
    uint32_t name_id, read_id, quality_id, read_len_id, read_type_id, spot_group_id;
    char qual_2_ascii_lut[ 256 ];
} fq_seq_ua_iter_t;

rc_t fq_seq_ua_iter_create( const cmn_iter_params_t * params,
                            fq_seq_ua_opt_t opt,
                            const char * tbl_name,
                            fq_seq_ua_iter_t ** iter ) {
    rc_t rc = 0;
    fq_seq_ua_iter_t * self = calloc( 1, sizeof * self );
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "fq_seq_ua_iter_create.calloc( %d ) -> %R", ( sizeof * self ), rc );
    } else {
        rc = KDataBufferMakeBytes( &self -> qual_buffer, 4096 );
        if ( 0 != rc ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "fq_seq_ua_iter_create().KDataBufferMakeBytes() -> %R", rc );
        } else {
            self -> opt = opt;
            rc = cmn_iter_make( params, tbl_name, &( self -> cmn ) );
            
            if ( 0 == rc && opt . with_name ) {
                rc = cmn_iter_add_column( self -> cmn, "NAME", &( self -> name_id ) );
            }
            
            if ( 0 == rc ) {
                rc = cmn_iter_add_column( self -> cmn, "READ", &( self -> read_id ) );
            }
            
            if ( 0 == rc && opt . with_quality ) {
                rc = cmn_iter_add_column( self -> cmn, "QUALITY", &( self -> quality_id ) );
            }
            
            if ( 0 == rc && opt . with_read_len ) {
                rc = cmn_iter_add_column( self -> cmn, "READ_LEN", &( self -> read_len_id ) );
            }
            
            if ( 0 == rc && opt . with_read_type ) {
                rc = cmn_iter_add_column( self -> cmn, "READ_TYPE", &( self -> read_type_id ) );
            }
            
            if ( 0 == rc && opt . with_spotgroup ) {
                rc = cmn_iter_add_column( self -> cmn, "SPOT_GROUP", &( self -> spot_group_id ) );
            }
            
            if ( 0 == rc ) {
                rc = cmn_iter_detect_range( self -> cmn, self -> read_id );
            }
            
            if ( 0 == rc ) {
                hlp_init_qual_to_ascii_lut( &( self -> qual_2_ascii_lut[ 0 ] ),
                                        sizeof( self -> qual_2_ascii_lut ) );
            }
            
            if ( 0 != rc ) {
                fq_seq_ua_iter_release( self );
            } else {
                *iter = self;
            }
        }
    }
    return rc;
}

void fq_seq_ua_iter_release( fq_seq_ua_iter_t * self ) {
    if ( NULL != self ) {
        cmn_iter_release( self -> cmn );
        if ( NULL != self -> qual_buffer . base ) {
            KDataBufferWhack( &self -> qual_buffer );
        }
        free( ( void * ) self );
    }
}

static rc_t fq_seq_ua_iter_read_bounded_quality( fq_seq_ua_iter_t * self,
                                  String * quality ) {
    uint8_t * qual_values = NULL;
    uint32_t num_qual = 0;
    rc_t rc = cmn_iter_read_uint8_array( self -> cmn, self -> quality_id, &qual_values, &num_qual );
    if ( 0 == rc ) {
        if ( num_qual > 0 && NULL != qual_values ) {
            if ( num_qual > self -> qual_buffer . elem_count ) {
                rc = KDataBufferResize ( &( self -> qual_buffer ), num_qual );
            }
            if ( 0 == rc ) {
                uint32_t idx;
                uint8_t * b = self -> qual_buffer . base;
                for ( idx = 0; idx < num_qual; idx++ ) {
                    b[ idx ] = self -> qual_2_ascii_lut[ qual_values[ idx ] ];
                }
                StringInit( quality, self -> qual_buffer . base, num_qual, num_qual );
            }
        } else {
            StringInit( quality, NULL, 0, 0 );
        }
    }
    if ( 0 != rc ) {
        StringInit( quality, NULL, 0, 0 );
        rc = 0;
    }
    return rc;
}
                                  
static rc_t fq_seq_ua_iter_read_bounded_quality_fixed( fq_seq_ua_iter_t * self,
                                                       String * quality,
                                                       uint32_t fixed_len ) {
    uint8_t * qual_values = NULL;
    uint32_t num_qual;
    rc_t rc = cmn_iter_read_uint8_array( self -> cmn, self -> quality_id, &qual_values, &num_qual );
    num_qual = fixed_len;
    if ( 0 == rc && num_qual > 0 && NULL != qual_values ) {
        if ( num_qual > self -> qual_buffer . elem_count ) {
            rc = KDataBufferResize( &( self -> qual_buffer ), num_qual );
        }
        if ( 0 == rc ) {
            uint32_t idx;
            uint8_t * b = self -> qual_buffer . base;
            for ( idx = 0; idx < num_qual; idx++ ) {
                b[ idx ] = self -> qual_2_ascii_lut[ qual_values[ idx ] ];
            }
            StringInit( quality, self -> qual_buffer . base, num_qual, num_qual );
        }
    }
    if ( 0 != rc ) {
        StringInit( quality, NULL, 0, 0 );
        rc = 0;
    }
    return rc;
}

bool fq_seq_ua_iter_get_data( fq_seq_ua_iter_t * self, fq_seq_ua_rec_t * rec, rc_t * rc ) {
    rc_t rc2;
    bool res = cmn_iter_get_next( self -> cmn, &rc2 );
    if ( res ) {
        rc_t rc1 = 0;
        
        rec -> row_id = cmn_iter_get_row_id( self -> cmn );
        
        if ( self -> opt . with_name ) {
            rc1 = cmn_iter_read_String( self -> cmn, self -> name_id, &( rec -> name ) );
        }
        
        if ( 0 == rc1 ) {
            rc1 = cmn_iter_read_String( self -> cmn, self -> read_id, &( rec -> read ) );
        }
        
        if ( 0 == rc1 ) {
            if ( self -> opt . with_read_len ) {
                rc1 = cmn_iter_read_uint32_array( self -> cmn, self -> read_len_id,
                                                  &rec -> read_len, &( rec -> num_read_len ) );
            } else {
                rec -> num_read_len = 1;
            }
        }
        
        if ( rc1 == 0 && self -> opt . with_quality ) {
            rc1 = fq_seq_ua_iter_read_bounded_quality( self, &( rec -> quality ) );
        }
        
        if ( 0 == rc1 ) {
            if ( self -> opt . with_read_type ) {
                rc1 = cmn_iter_read_uint8_array( self -> cmn, self -> read_type_id,
                                                 &rec -> read_type, &( rec -> num_read_type ) );
            } else {
                rec -> num_read_type = 0;
            }
        }
        
        if ( 0 == rc1 && self -> opt . with_read_len ) {
            uint32_t sum_read_len = 0;
            uint32_t i;
            for ( i = 0; i < rec -> num_read_len; ++i ) {
                sum_read_len += rec -> read_len[ i ];
            }
            if ( rec -> read . len != sum_read_len ) {
                rec -> read . len  = sum_read_len;
                rec -> read . size = sum_read_len;
                rc1 = fq_seq_ua_iter_read_bounded_quality_fixed( self,
                                                &( rec -> quality ),
                                                sum_read_len );
            }
        }
        
        if ( 0 == rc1 && self -> opt . with_spotgroup ) {
            rc1 = cmn_iter_read_String( self -> cmn, self -> spot_group_id, &( rec -> spotgroup ) );
        } else {
            StringInit( &( rec -> spotgroup ), NULL, 0, 0 );
        }
        
        if ( NULL != rc ) { *rc = rc1; }
    }
    return res;
}
