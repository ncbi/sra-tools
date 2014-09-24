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

#include "reader.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const char * ridx_names[ N_RIDX ] =
{
    "RAW_READ",
    "QUALITY",
    "(bool)HAS_MISMATCH",
    "SEQ_SPOT_ID",
    "SPOT_GROUP",
    "SEQ_SPOT_GROUP",
    "REF_ORIENTATION",
    "READ_LEN",
    "SEQ_READ_ID",
    "(bool)HAS_REF_OFFSET",
    "REF_OFFSET",
    "REF_POS",
    "REF_SEQ_ID",
    "REF_LEN"
};

static rc_t make_vector( void ** ptr, uint32_t *len, uint32_t new_len )
{
    rc_t rc = 0;

    *len = new_len;
    *ptr = calloc( 1, new_len );
    if ( *ptr == NULL )
    {
        *len = 0;
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        LogErr( klogInt, rc, "failed to make large enough exclude-vector\n" );
    }
    return rc;
}


static rc_t expand_and_clear_vector( void **v, uint32_t *len, uint32_t new_len, bool clear )
{
    rc_t rc = 0;
    if ( *v != NULL )
    {
        if ( *len < new_len )
        {
            *len += new_len;
            *v = realloc( *v, *len );
            if ( *v == NULL )
            {
                rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                LogErr( klogInt, rc, "failed to expand (uint8_t)vector\n" );
            }
        }
        if ( rc == 0 && clear )
            memset( *v, 0, *len ); 
    }
    return rc;
}


rc_t make_statistic_reader( statistic_reader *self,
                            spot_pos *sequence,
                            KDirectory *dir,
                            const VCursor * cursor,
                            const char * exclude_db,
                            bool info )
{
    rc_t rc;

    memset( &self->rd_col, 0, sizeof self->rd_col );
    self->cursor = cursor;
    self->ref_exclude_used = false;
    self->exclude_vector = NULL;
    self->exclude_vector_len = 0;
    self->active_exclusions = 0;
    self->sequence = sequence;

    rc = add_columns( cursor, self->rd_col, ridx_names, N_RIDX );
    if ( rc == 0 )
    {
        rc = VCursorOpen ( cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VCursorOpen failed in reader\n" );
    }
    if ( rc == 0 )
    if ( exclude_db != NULL )
    {
        make_ref_exclude( &self->exclude, dir, exclude_db, info );
        rc = make_vector( (void**)&self->exclude_vector, &self->exclude_vector_len, 512 );
        self->ref_exclude_used = true;
    }

    return rc;
}


void whack_reader( statistic_reader *self )
{
    if ( self->ref_exclude_used )
    {
        whack_ref_exclude( &self->exclude );
        if ( self->exclude_vector != NULL )
        {
            free( self->exclude_vector );
        }
    }
}


void reader_set_spot_pos( statistic_reader *self, spot_pos * sequence )
{
    self->sequence = sequence;
}

rc_t query_reader_rowrange( statistic_reader *self, int64_t *first, uint64_t * count )
{
    rc_t rc = VCursorIdRange ( self->cursor, self->rd_col[ RIDX_READ ].idx,
                               first, count );
    if ( rc != 0 )
        LogErr( klogInt, rc, "VCursorIdRange() failed in reader\n" );
    return rc;
}


rc_t reader_get_data( statistic_reader *self, row_input * row_data,
                      uint64_t row_id )
{
    rc_t rc = read_cells( self->cursor, row_id, self->rd_col, ridx_names, N_RIDX );
    if ( rc == 0 )
    {
        bool * reverse;

        row_data->spotgroup = (char *)self->rd_col[ RIDX_SPOT_GROUP ].base;
        row_data->spotgroup_len = self->rd_col[ RIDX_SPOT_GROUP ].row_len;

        row_data->seq_spotgroup = (char *)self->rd_col[ RIDX_SEQ_SPOT_GROUP ].base;
        row_data->seq_spotgroup_len = self->rd_col[ RIDX_SEQ_SPOT_GROUP ].row_len;

        row_data->read = (char *)self->rd_col[ RIDX_READ ].base;
        row_data->read_len = self->rd_col[ RIDX_READ ].row_len;

        row_data->quality = (uint8_t *)self->rd_col[ RIDX_QUALITY ].base;
        row_data->quality_len = self->rd_col[ RIDX_QUALITY ].row_len;

        row_data->has_mismatch = (bool *)self->rd_col[ RIDX_HAS_MISMATCH ].base;
        row_data->has_mismatch_len = self->rd_col[ RIDX_HAS_MISMATCH ].row_len;

        row_data->has_roffs = (bool *)self->rd_col[ RIDX_HAS_REF_OFFSET ].base;
        row_data->has_roffs_len = self->rd_col[ RIDX_HAS_REF_OFFSET ].row_len;

        row_data->roffs = (int32_t *)self->rd_col[ RIDX_REF_OFFSET ].base;
        row_data->roffs_len = self->rd_col[ RIDX_REF_OFFSET ].row_len;

        reverse = (bool *)self->rd_col[ RIDX_REF_ORIENTATION ].base;
        row_data->reversed = *reverse;

        row_data->seq_read_id = *( ( uint32_t * )self->rd_col[ RIDX_SEQ_READ_ID ].base );
        row_data->spot_id = *( ( uint32_t * )self->rd_col[ RIDX_SEQ_SPOT_ID ].base );
        row_data->base_pos_offset = 0;
        row_data->ref_len = *( ( uint32_t *)self->rd_col[ RIDX_REF_LEN ].base );
#ifdef LOOKUP_ALL_SEQ_READ_ID
        if ( row_data->seq_read_id > 0 )
#else
        if ( row_data->seq_read_id > 1 )
#endif
        {
            if ( self->sequence != NULL )
            {
                query_spot_pos( self->sequence, row_data->seq_read_id,
                            row_data->spot_id, &(row_data->base_pos_offset) );
            }
        }

        if ( self->ref_exclude_used )
        {
            String s_ref_name;

            const char * ref_name_base = ( const char * )self->rd_col[ RIDX_REF_SEQ_ID ].base;
            uint32_t ref_name_len = self->rd_col[ RIDX_REF_SEQ_ID ].row_len;
            int32_t ref_offset = *( ( int32_t *)self->rd_col[ RIDX_REF_POS ].base );
            uint32_t ref_len = *( ( uint32_t *)self->rd_col[ RIDX_REF_LEN ].base );

            StringInit( &s_ref_name, ref_name_base, ref_name_len, ref_name_len );

            /* make the ref-exclude-vector longer if necessary */
            rc = expand_and_clear_vector( (void**)&self->exclude_vector,
                                          &self->exclude_vector_len,
                                          ref_len,
                                          true );
            if ( rc == 0 )
            {
                rc = get_ref_exclude( &self->exclude,
                                      &s_ref_name,
                                      ref_offset,
                                      ref_len,
                                      self->exclude_vector,
                                      &self->active_exclusions );
                if ( rc == 0 )
                {
                    row_data->exclude = self->exclude_vector;
                    row_data->exclude_len = ref_len;
                }
            }
        }
        else
        {
            row_data->exclude = NULL;
            row_data->exclude_len = 0;
        }
    }
    return rc;
}
