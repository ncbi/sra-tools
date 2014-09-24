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

#include "spot_position.h"
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static const char * sidx_names[ N_SIDX ] =
{
    "READ_START"
};


rc_t make_spot_pos( spot_pos *self,
                    const VDatabase * db )
{
    rc_t rc = VDatabaseOpenTableRead( db, &self->table, "SEQUENCE" );
    if ( rc != 0 )
    {
        LogErr( klogInt, rc, "VDatabaseOpenTableRead(SEQUENCE) failed\n" );
        self->table = NULL;
        self->cursor = NULL;
    }
    else
    {
        rc = VTableCreateCursorRead( self->table, &self->cursor );
        if ( rc != 0 )
            LogErr( klogInt, rc, "VTableCreateCursorRead(SEQUENCE) failed\n" );
        else
        {
            rc = add_columns( self->cursor, self->columns, sidx_names, N_SIDX );
            if ( rc == 0 )
            {
                rc = VCursorOpen ( self->cursor );
                if ( rc != 0 )
                {
                    LogErr( klogInt, rc, "cannot open cursor on SEQUENCE\n" );
                }
            }
        }
    }
    return rc;
}


rc_t query_spot_pos( spot_pos *self,
                     const uint32_t seq_read_id,
                     const uint64_t spot_id,
                     uint32_t *pos_offset )
{
    rc_t rc;
    if ( self->cursor == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
        LogErr( klogInt, rc, "cannot query spot-position, cursor is NULL\n" );
    }
    else
    {
        rc = read_cells( self->cursor, spot_id, self->columns, sidx_names, N_SIDX );
        if ( rc == 0 )
        {
            const uint32_t * rd_start   = self->columns[ SIDX_READ_START ].base;
            uint32_t rd_start_len = self->columns[ SIDX_READ_START ].row_len;
            if ( seq_read_id > rd_start_len )
            {
                rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcInvalid );
                PLOGERR( klogInt, ( klogInt, rc, 
                     "asking for read_start of read #$(read_nr) but we only have $(n_read) reads at row #$(row_nr)",
                     "read_nr=%u,n_read=%u,row_nr=%lu", seq_read_id, rd_start_len, spot_id ) );
            }
            else
            {
                *pos_offset = rd_start[ seq_read_id - 1 ];
                /*
                OUTMSG(( "SPOT_ID %lu / SEQ_READ_ID %u ---> %u\n",
                         spot_id, seq_read_id, *pos_offset ));
                */
            }
        }
        else
        {
            PLOGERR( klogInt, ( klogInt, rc, 
                 "cannot read sequence row #$(row_nr)", "row_nr=%lu", spot_id ) );
        }
    }
    return rc;
}


void whack_spot_pos( spot_pos *self )
{
    if ( self->cursor != NULL )
        VCursorRelease( self->cursor );
    if ( self->table != NULL )
        VTableRelease( self->table );
}
