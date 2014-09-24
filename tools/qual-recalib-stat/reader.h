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

#ifndef _h_stat_reader_
#define _h_stat_reader_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <vdb/cursor.h>
#include "columns.h"
#include "ref_exclude.h"
#include "stat_mod_2.h"
#include "spot_position.h"

#define RIDX_READ            0
#define RIDX_QUALITY         1
#define RIDX_HAS_MISMATCH    2
#define RIDX_SEQ_SPOT_ID     3
#define RIDX_SPOT_GROUP      4
#define RIDX_SEQ_SPOT_GROUP  5
#define RIDX_REF_ORIENTATION 6
#define RIDX_READ_LEN        7
#define RIDX_SEQ_READ_ID     8
#define RIDX_HAS_REF_OFFSET  9
#define RIDX_REF_OFFSET     10
#define RIDX_REF_POS        11
#define RIDX_REF_SEQ_ID     12
#define RIDX_REF_LEN        13
#define N_RIDX              14

typedef struct statistic_reader
{
    ref_exclude exclude;
    uint8_t *exclude_vector;
    uint32_t exclude_vector_len;
    uint32_t active_exclusions;
    bool ref_exclude_used;
    const VCursor *cursor;
    col rd_col[ N_RIDX ];
    spot_pos * sequence;
} statistic_reader;

rc_t make_statistic_reader( statistic_reader *self,
                            spot_pos *sequence,
                            KDirectory *dir,
                            const VCursor * cursor,
                            const char * exclude_db,
                            bool info );

void whack_reader( statistic_reader *self );

void reader_set_spot_pos( statistic_reader *self, spot_pos * sequence );

rc_t query_reader_rowrange( statistic_reader *self, int64_t *first, uint64_t * count );

rc_t reader_get_data( statistic_reader *self, row_input * row_data, uint64_t row_id );

#ifdef __cplusplus
}
#endif

#endif
