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
#ifndef _h_spot_position_
#define _h_spot_position_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <vdb/cursor.h>
#include <vdb/table.h>
#include <vdb/database.h>
#include "columns.h"


#define SIDX_READ_START      0
#define N_SIDX               1


typedef struct spot_pos
{
    const VTable *table;
    const VCursor *cursor;
    col columns[ N_SIDX ];
} spot_pos;


rc_t make_spot_pos( spot_pos *self, const VDatabase * db );

rc_t query_spot_pos( spot_pos *self,
                     const uint32_t seq_read_id,
                     const uint64_t spot_id,
                     uint32_t *pos_offset );

void whack_spot_pos( spot_pos *self );

#ifdef __cplusplus
}
#endif

#endif
