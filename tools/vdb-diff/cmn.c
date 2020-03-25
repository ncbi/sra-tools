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

#include "cmn.h"
#include <klib/log.h>
#include <klib/out.h>

rc_t cmn_diff_column( const col_pair * pair,
                      const VCursor * cur_1, const VCursor * cur_2,
                      int64_t row_id,  bool * res )
{
    uint32_t elem_bits_1, boff_1, row_len_1;
    const void * base_1;
    rc_t rc = VCursorCellDataDirect ( cur_1, row_id, pair->idx[ 0 ], 
                                    &elem_bits_1, &base_1, &boff_1, &row_len_1 );
    if ( rc != 0 )
    {
        PLOGERR( klogInt, ( klogInt, rc, 
                    "VCursorCellDataDirect( #1 [$(col)].$(row) ) failed",
                    "col=%s,row=%ld", pair->name, row_id ) );
    }
    else
    {
        uint32_t elem_bits_2, boff_2, row_len_2;
        const void * base_2;
        rc = VCursorCellDataDirect ( cur_2, row_id, pair->idx[ 1 ], 
                                    &elem_bits_2, &base_2, &boff_2, &row_len_2 );
        if ( rc != 0 )
        {
            PLOGERR( klogInt, ( klogInt, rc, 
                        "VCursorCellDataDirect( #2 [$(col)].$(row) ) failed",
                        "col=%s,row=%ld", pair->name, row_id ) );
        }
        else
        {
            *res = true;
            
            if ( elem_bits_1 != elem_bits_2 )
            {
                *res = false;
                rc = KOutMsg( "%s[ %ld ].elem_bits %u != %u\n", pair->name, row_id, elem_bits_1, elem_bits_2 );
            }

            if ( row_len_1 != row_len_2 )
            {
                *res = false;
                if ( rc == 0 )
                    rc = KOutMsg( "%s[ %ld ].row_len %u != %u\n", pair->name, row_id, row_len_1, row_len_2 );
            }

            if ( boff_1 != 0 || boff_2 != 0 )
            {
                *res = false;
                if ( rc == 0 )
                    rc = KOutMsg( "%s[ %ld ].bit_offset: %u, %u\n", pair->name, row_id, boff_1, boff_2 );
            }
            
            if ( *res )
            {
                size_t num_bits = ( row_len_1 * elem_bits_1 );
                if ( num_bits & 0x07 )
                {
                    if ( rc == 0 )
                        rc = KOutMsg( "%s[ %ld ].bits_total %% 8 = %u\n", pair->name, row_id, ( num_bits % 8 ) );
                }
                else
                {
                    size_t num_bytes = ( num_bits >> 3 );
                    int cmp = memcmp ( base_1, base_2, num_bytes );
                    if ( cmp != 0 )
                    {
                        if ( rc == 0 )
                            rc = KOutMsg( "%s[ %ld ] differ\n", pair->name, row_id );
                        *res = false;
                    }
                }
            }
        }
    }

    return rc;
}

rc_t cmn_make_num_gen( const VCursor * cur_1, const VCursor * cur_2,
                    int idx_1, int idx_2,
                    const struct num_gen * src, struct num_gen ** dst )
{
    int64_t  first_1;
    uint64_t count_1;
    rc_t rc = VCursorIdRange( cur_1, idx_1, &first_1, &count_1 );
    *dst = NULL;
    if ( rc != 0 )
    {
        LOGERR ( klogInt, rc, "VCursorIdRange( acc #1 ) failed" );
    }
    else
    {
        int64_t  first_2;
        uint64_t count_2;
        rc = VCursorIdRange( cur_2, idx_2, &first_2, &count_2 );
        if ( rc != 0 )
        {
            LOGERR ( klogInt, rc, "VCursorIdRange( acc #2 ) failed" );
        }
        else
        {
            /* trick for static columns, they have only one value - so count=1 is OK */
            if ( count_1 == 0 ) count_1 = 1;
            if ( count_2 == 0 ) count_2 = 1;
            
            if ( src == NULL )
            {
                /* no row-range given ( src == NULL ) create the number generator from the discovered range, if it is the same */
                if ( first_1 != first_2 || count_1 != count_2 )
                {
                    rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
                    PLOGERR( klogInt, ( klogInt, rc, "row-ranges differ: $(first1).$(count1) != $(first2).$(count2)",
                            "first1=%ld,count1=%lu,first2=%ld,count2=%lu",
                            first_1, count_1, first_2, count_2 ) );
                }
                else
                {
                    rc = num_gen_make_from_range( dst, first_1, count_1 );
                    if ( rc != 0 )
                    {
                        LOGERR ( klogInt, rc, "num_gen_make_from_range() failed" );
                    }
                }
            }
            else
            {
                /* row-range given, clip the rows be the 2 ranges ( even if they are not the same ) */
                num_gen_copy( src, dst );
                KOutMsg( "cmn_make_num_gen: count_1=%lu, count2=%lu\n", count_1, count_2 );
                rc = num_gen_trim( *dst, first_1, count_1 );
                if ( rc != 0 )
                {
                    LOGERR ( klogInt, rc, "num_gen_trim( acc #1 ) failed" );
                }
                else if ( first_1 != first_2 || count_1 != count_2 )
                {
                    rc = num_gen_trim( *dst, first_2, count_2 );
                    if ( rc != 0 )
                    {
                        LOGERR ( klogInt, rc, "num_gen_trim( acc #2 ) failed" );
                    }
                }
            }
            
            if ( rc != 0 && *dst != NULL )
            {
                num_gen_destroy( *dst );
            }
        }
    }
    return rc;
}
