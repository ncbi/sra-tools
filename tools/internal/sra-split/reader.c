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
#include "err_msg.h"

#ifndef _h_vdb_database_
#include <vdb/database.h>
#endif

#ifndef _h_vdb_table_
#include <vdb/table.h>
#endif

rc_t reader_init( p_reader_t reader, VDBManager * mgr, const char * src ) {
    const VDatabase * db;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", src );
    reader -> cur = NULL;
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerOpenDBRead( %s ) -> %R", src, rc );
    } else {
        const VTable * tbl;
        rc = VDatabaseOpenTableRead( db, &tbl, "SEQUENCE" );
        VDatabaseRelease( db );
        if ( 0 != rc ) {
            ErrMsg( "VDatabaseOpenTableRead( %s ) -> %R", src, rc );
        } else {
            rc = VTableCreateCachedCursorRead( tbl, &( reader -> cur ), 1024 * 1024 );
            VTableRelease( tbl );
            if ( 0 != rc ) {
                ErrMsg( "VTableCreateCachedCursorRead( %s ) -> %R", src, rc );
            } else {
                rc = VCursorAddColumn( reader -> cur, &( reader -> quality_idx ), "QUALITY" );
                if ( 0 != rc ) {
                    ErrMsg( "VCursorAddColumn( %s, QUALITY ) -> %R", src, rc );
                } else {
                    rc = VCursorAddColumn( reader -> cur, &( reader -> filter_idx ), "READ_FILTER" );
                    if ( 0 != rc ) {
                        ErrMsg( "VCursorAddColumn( %s, READ_FILTER ) -> %R", src, rc );
                    } else {
                        rc = VCursorOpen( reader -> cur );
                        if ( 0 != rc ) {
                            ErrMsg( "VCursorOpen( %s ) -> %R", src, rc );
                        } else {
                            rc = VCursorIdRange( reader -> cur, reader -> quality_idx,
                                    &( reader -> first ), &( reader -> count ) );
                            if ( 0 != rc ) {
                                ErrMsg( "VCursorIdRange( %s ) -> %R", src, rc );
                            } else {
                                reader ->row_id = reader -> first;
                                reader -> id = 0;
                            }
                        }
                    }
                }
            }
        }
    }
    return rc;
}

bool reader_done( p_reader_t reader ) {
    return ( reader -> id >= reader -> count );
}

int64_t reader_get_row_id( p_reader_t reader ) {
    return reader ->row_id;
}

rc_t reader_get_quality( p_reader_t reader, uint8_t ** data, uint32_t * len ) {
    uint32_t elem_bits, boff;
    rc_t rc = VCursorCellDataDirect( reader -> cur,
                                     reader -> row_id,
                                     reader -> quality_idx,
                                     &elem_bits,
                                     (const void **)data,
                                     &boff,
                                     len );
    if ( 0 != rc ) {
        ErrMsg( "VCursorCellDataDirect( QUALITY, #%u ) -> %R", reader -> row_id, rc );
    }
    return rc;
}

rc_t reader_get_filter( p_reader_t reader, uint8_t ** data, uint32_t * len ){
    uint32_t elem_bits, boff;
    rc_t rc = VCursorCellDataDirect( reader -> cur,
                                     reader -> row_id,
                                     reader -> filter_idx,
                                     &elem_bits,
                                     (const void **)data,
                                     &boff,
                                     len );
    if ( 0 != rc ) {
        ErrMsg( "VCursorCellDataDirect( FILTER, #%u ) -> %R", reader -> row_id, rc );
    }
    return rc;
}

void reader_next( p_reader_t reader ) {
    reader -> row_id += 1;
    reader -> id += 1;
}

rc_t reader_destroy( p_reader_t reader ) {
    rc_t rc = 0;
    if ( NULL != reader -> cur ) {
        rc = VCursorRelease( reader -> cur );
    }
    return rc;
}
