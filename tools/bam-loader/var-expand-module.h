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
#ifndef VAR_EXPAND_MODULE_H_
#define VAR_EXPAND_MODULE_H_

#include <klib/rc.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <insdc/insdc.h>
#include <klib/container.h>

struct BAM_Alignment;
struct ReferenceSeq;

#define LOG_FILENAME "var-expand.log"
#define EV_FILENAME "var-expand.events"

typedef struct variation
{
    DLNode node;
    INSDC_coord_zero offset;
    INSDC_coord_len len;
    uint32_t count;
    char bases[ 1 ];
} variation;

#define REF_ID_LEN 4096
#define WRITE_BUFFER_LEN 4096
#define N_CIG_OPS 100

typedef struct var_expand_data
{
    KDirectory * dir;
    
    KFile * log;
    uint64_t log_pos;

    KFile * events;
    uint64_t events_pos;
    
    uint64_t alignments_seen;
    
    uint8_t * seq_buffer;
    uint32_t seq_buffer_len;
    int64_t last_seq_pos_on_ref;
    uint32_t seq_len_on_ref;

    char ref_id[ REF_ID_LEN ];
    char write_buffer[ WRITE_BUFFER_LEN ];
    
    uint8_t * ref_buffer;
    uint32_t ref_buffer_len;
    uint32_t ref_len;

    DLList variations;

    uint32_t seq_len, cigar_op_count;
    int64_t seq_pos_on_ref;
    const char * curr_ref_id;
    uint32_t cigar_op[ N_CIG_OPS ];
    uint32_t cigar_op_len[ N_CIG_OPS ];

} var_expand_data;

rc_t var_expand_init( var_expand_data ** data );

rc_t var_expand_handle( var_expand_data * data, struct BAM_Alignment const *alignment, struct ReferenceSeq const *refSequence );

rc_t var_expand_finish( var_expand_data * data );

#endif