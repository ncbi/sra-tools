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

#ifndef _h_reader_
#define _h_reader_

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif

#ifndef _h_vdb_cursor_
#include <vdb/cursor.h>
#endif

typedef struct reader_t {
    const VCursor * cur;
    uint32_t quality_idx;
    uint32_t filter_idx;
    int64_t first;
    uint64_t count;
    int64_t row_id;
    uint64_t id;
} reader_t;
typedef reader_t* p_reader_t;

rc_t reader_init( p_reader_t reader, VDBManager * mgr, const char * src );

bool reader_done( p_reader_t reader );
rc_t reader_get_quality( p_reader_t reader, uint8_t ** data, uint32_t * len );
rc_t reader_get_filter( p_reader_t reader, uint8_t ** data, uint32_t * len );
int64_t reader_get_row_id( p_reader_t reader );
void reader_next( p_reader_t reader );

rc_t reader_destroy( p_reader_t reader );

#endif
