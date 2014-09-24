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

#ifndef _h_sra_sort_glob_poslen_
#define _h_sra_sort_glob_poslen_

#ifndef _h_sra_sort_col_pair_
#include "col-pair.h"
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct TablePair;


/*--------------------------------------------------------------------------
 * inline functions
 */
#define MAX_POS_BITS 34
#define MAX_LEN_BITS ( 64 - MAX_POS_BITS )


static __inline__
uint64_t encode_pos_len ( uint64_t pos, uint32_t len )
{
    assert ( pos < ( ( ~ ( uint64_t ) 0 ) >> MAX_LEN_BITS ) );
    assert ( 0 < len && len <= ( ( ~ ( uint32_t ) 0 ) >> ( 32 - MAX_LEN_BITS ) ) );

    /* position ascending, length descending */
    return ( ( pos + 1 ) << MAX_LEN_BITS ) - len;
}

static __inline__
uint64_t decode_pos_len ( uint64_t pos_len )
{
    return pos_len >> MAX_LEN_BITS;
}

static __inline__
uint32_t poslen_to_len ( uint64_t pos_len )
{
    return ( 1U << MAX_LEN_BITS ) - ( ( uint32_t ) pos_len & ( ( ~ ( uint32_t ) 0 ) >> ( 32 - MAX_LEN_BITS ) ) );
}

static __inline__
uint64_t local_to_global ( int64_t row_id, uint32_t chunk_size, uint32_t offset )
{
    assert ( row_id > 0 );
    return ( row_id - 1 ) * chunk_size + offset;
}

static __inline__
int64_t global_to_row_id ( uint64_t pos, uint32_t chunk_size )
{
    return ( pos / chunk_size ) + 1;
}


/*--------------------------------------------------------------------------
 * GlobalPosLenReader
 */
typedef struct GlobalPosLenReader GlobalPosLenReader;


/* MakeGlobalPosLenReader
 */
ColumnReader *TablePairMakeGlobalPosLenReader ( struct TablePair *self, const ctx_t *ctx, uint32_t chunk_size );


#endif /* _h_sra_sort_glob_poslen_ */
