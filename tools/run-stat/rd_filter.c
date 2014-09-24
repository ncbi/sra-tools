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

#include "rd_filter.h"

#include <os-native.h>
/* #include <insdc/sra.h> */

#include <sysalloc.h>
#include <bitstr.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


static rc_t add_2_cur( p_rd_filter self, const VCursor * cur )
{
    rc_t rc = VCursorAddColumn( cur, &self->idx_READ, "(INSDC:dna:text)READ" );
    if ( rc == 0 )
        rc = VCursorAddColumn( cur, &self->idx_QUALITY, "(INSDC:quality:phred)QUALITY" );
    if ( rc == 0 )
        rc = VCursorAddColumn( cur, &self->idx_TRIM_START, "TRIM_START" );
    if ( rc == 0 )
        rc = VCursorAddColumn( cur, &self->idx_TRIM_LEN, "TRIM_LEN" );
    if ( rc == 0 )
        rc = VCursorAddColumn( cur, &self->idx_READ_START, "READ_START" );
    if ( rc == 0 )
        rc = VCursorAddColumn( cur, &self->idx_READ_LEN, "READ_LEN" );
    if ( rc == 0 )
        rc = VCursorAddColumn( cur, &self->idx_READ_FILTER, "READ_FILTER" );
    if ( rc == 0 )
        rc = VCursorAddColumn( cur, &self->idx_READ_TYPE, "READ_TYPE" );

    return rc;
}


rc_t rd_filter_init( p_rd_filter self )
{
    rc_t rc = char_buffer_init( &self->bases, 0 );
    if ( rc == 0 )
        rc = char_buffer_init( &self->filtered_bases, 5 );
    if ( rc == 0 )
        rc = char_buffer_init( &self->quality, 0 );
    if ( rc == 0 )
        rc = char_buffer_init( &self->filtered_quality, 0 );
    if ( rc == 0 )
        rc = char_buffer_init( &self->quality, 5 );
    if ( rc == 0 )
        rc = char_buffer_init( &self->filtered_quality, 5 );
    if ( rc == 0 )
        rc = int_buffer_init( &self->read_start, 5 );
    if ( rc == 0 )
        rc = int_buffer_init( &self->read_len, 5 );
    if ( rc == 0 )
        rc = int_buffer_init( &self->read_filter, 5 );
    if ( rc == 0 )
        rc = int_buffer_init( &self->read_type, 5 );
    return rc;
}


void rd_filter_destroy( p_rd_filter self )
{
    char_buffer_destroy( &self->bases );
    char_buffer_destroy( &self->filtered_bases );
    char_buffer_destroy( &self->quality );
    char_buffer_destroy( &self->filtered_quality );

    int_buffer_destroy( &self->read_start );
    int_buffer_destroy( &self->read_len );
    int_buffer_destroy( &self->read_filter );
    int_buffer_destroy( &self->read_type );
}


rc_t rd_filter_pre_open( p_rd_filter self, const VCursor * cur )
{
    return add_2_cur( self, cur );
    /* return get_cur_idx( self, cur ); */
}

#define SRA_READ_TYPE_BIOLOGICAL 1

static bool rd_filter_restrict( p_rd_filter self, uint32_t ridx,
                                uint32_t * start, uint32_t * end )
{
    bool res = true;

    if ( self->bio )
        res = ( self->read_type.trans_ptr[ ridx ] == SRA_READ_TYPE_BIOLOGICAL );
    if ( res )
    {
        *start = self->read_start.trans_ptr[ ridx ];
        *end   = *start + self->read_len.trans_ptr[ ridx ];
        if ( self->trim )
        {
            if ( *end < self->trim_start || *start > self->trim_end )
                res = false;
            else
            {
                if ( *start < self->trim_start )
                    *start = self->trim_start;
                if ( *end > self->trim_end )
                    *end = self->trim_end;
            }
        }
        if ( res && self->cut )
        {
            res = ( ( *end - *start ) > 25 );
        }
    }

    return res;
}


static rc_t rd_filter_apply( p_rd_filter self )
{
    p_char_buffer b_src = &self->bases;
    p_char_buffer b_dst = &self->filtered_bases;
    p_char_buffer q_src = &self->quality;
    p_char_buffer q_dst = &self->filtered_quality;

    rc_t rc = char_buffer_realloc( b_dst, b_src->len );
    if ( rc == 0 )
    {
        b_dst->len = 0;
        rc = char_buffer_realloc( q_dst, q_src->len );
        if ( rc == 0 )
        {
            uint32_t ridx, n = self->read_start.len;
            q_dst->len = 0;
            for ( ridx = 0; ridx < n; ++ridx )
            {
                /* walk the READS */
                uint32_t start, end;
                if ( rd_filter_restrict( self, ridx, &start, &end ) )
                {
                    uint32_t pos;
                    for ( pos = start; pos < end; ++pos )
                    {
                        b_dst->ptr[ b_dst->len++ ] = b_src->trans_ptr[ pos ];
                        q_dst->ptr[ q_dst->len++ ] = q_src->trans_ptr[ pos ];
                    }
                }
            }
        }
    }
    return rc;
}


rc_t rd_filter_read( p_rd_filter self, const VCursor * cur )
{
    /* first read the trim-values */
    rc_t rc = helper_read_uint32( cur, self->idx_TRIM_START, &self->trim_start );
    if ( rc == 0 )
    {
        rc = helper_read_uint32( cur, self->idx_TRIM_LEN, &self->trim_end );
        if ( rc == 0 )
            self->trim_end += self->trim_start;
    }

    /* then read the READ-SEGMENT-COLUMNs */
    if ( rc == 0 )
        rc = helper_read_int32_values( cur, self->idx_READ_START, &self->read_start );
    if ( rc == 0 )
        rc = helper_read_int32_values( cur, self->idx_READ_LEN, &self->read_len );
    if ( rc == 0 )
        rc = helper_read_int32_values( cur, self->idx_READ_FILTER, &self->read_filter );
    if ( rc == 0 )
        rc = helper_read_int32_values( cur, self->idx_READ_TYPE, &self->read_type );

    /* then read the BASES */
    if ( rc == 0 )
        rc = helper_read_char_values( cur, self->idx_READ, &self->bases );

    /* the read the QULAITY */
    if ( rc == 0 )
        rc = helper_read_char_values( cur, self->idx_QUALITY, &self->quality );

    if ( rc == 0 )
        rc = rd_filter_apply( self );

    return rc;
}


rc_t rd_filter_set_flags( p_rd_filter self, bool bio, bool trim, bool cut )
{
    self->bio  = bio;
    self->trim = trim;
    self->cut  = cut;
    return 0;
}


uint32_t filter_get_read_len( p_rd_filter self )
{
    return self->filtered_bases.len;
}


char filter_get_base( p_rd_filter self, const uint32_t idx )
{
    if ( idx < self->filtered_bases.len )
        return self->filtered_bases.ptr[ idx ];
    else
        return 0;
}


uint8_t filter_get_quality( p_rd_filter self, const uint32_t idx )
{
    if ( idx < self->filtered_quality.len )
        return (uint8_t)self->filtered_quality.ptr[ idx ];
    else
        return 0;
}
