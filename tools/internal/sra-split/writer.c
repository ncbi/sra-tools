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

#include "writer.h"
#include "err_msg.h"

#ifndef _h_vdb_schema_
#include <vdb/schema.h>
#endif

#ifndef _h_vdb_database_
#include <vdb/database.h>
#endif

#ifndef _h_vdb_table_
#include <vdb/table.h>
#endif

const char * schema_txt = "version 2; "
    "table SUB_TAB #1.0 { "
        "column U8 QUALITY_CACHE; "
        "column U8 READ_FILTER_CACHE; "
    " }; "
    "database MAIN_DB #1.0 { table SUB_TAB #1 SEQUENCE; };";

static rc_t make_schema( VDBManager * mgr, VSchema ** schema ) {
    rc_t rc = VDBManagerMakeSchema( mgr, schema );
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerMakeSchema() -> %R", rc );
    } else {
        size_t len = string_size( schema_txt );
        rc = VSchemaParseText( *schema, "internal", schema_txt, len );
        if ( 0 != rc ) {
            ErrMsg( "VSchemaParseText() -> %R", rc );
        }
    }
    return rc;
}

static rc_t make_database( VDBManager * mgr, VSchema * schema, const char * dst, VDatabase ** db ) {
    KCreateMode cmode = kcmInit | kcmParents | kcmMD5;
    rc_t rc = VDBManagerCreateDB( mgr, db, schema, "MAIN_DB", cmode, dst );
    VSchemaRelease( schema );
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerCreateDB( %s ) -> %R", dst, rc );
    } else {
        rc = VDatabaseColumnCreateParams( *db, cmode, kcsMD5, 0 );
        if ( 0 != rc ) {
            ErrMsg( "VDatabaseColumnCreateParams( %s ) -> %R", dst, rc );
            VDatabaseRelease( *db );
        }
    }
    return rc;
}

static rc_t make_table( VDatabase * db, VTable ** tbl, const char * dst ) {
    rc_t rc = VDatabaseCreateTable( db, tbl, "SEQUENCE", kcsMD5, "SEQUENCE" );
    VDatabaseRelease( db );
    if ( 0 != rc ) {
        ErrMsg( "VDatabaseCreateTable( %s ) -> %R", dst, rc );
    }
    return rc;
}

static rc_t make_cursor( VTable *tbl, p_writer_t writer, const char * dst ) {
    rc_t rc = VTableCreateCursorWrite( tbl, &( writer -> cur ), kcmInsert );
    VTableRelease( tbl );
    if ( 0 != rc ) {
        ErrMsg( "VTableCreateCursorWrite( %s ) -> %R", dst, rc );
    } else {
        rc = VCursorAddColumn( writer -> cur, &( writer -> quality_idx ), "QUALITY_CACHE" );
        if ( 0 != rc ) {
            ErrMsg( "VCursorAddColumn( QUALITY_CACHE ) -> %R", rc );
        } else {
            rc = VCursorAddColumn( writer -> cur, &( writer -> filter_idx ), "READ_FILTER_CACHE" );
            if ( 0 != rc ) {
                ErrMsg( "VCursorAddColumn( READ_FILTER_CACHE ) -> %R", rc );
            } else {
                rc = VCursorOpen( writer -> cur );
                if ( 0 != rc ) {
                    ErrMsg( "VCursorOpen( %s ) -> %R", dst, rc );
                }
            }
        }
    }
    return rc;
}

rc_t writer_init( p_writer_t writer, VDBManager * mgr, const char * dst ) {
    VSchema * schema;
    rc_t rc = make_schema( mgr, &schema );
    if ( 0 == rc ) {
        VDatabase * db;
        rc = make_database( mgr, schema, dst, &db );
        if ( 0 == rc ) {
            VTable * tbl;
            rc = make_table( db, &tbl, dst );
            if ( 0 == rc ) {
                rc = make_cursor( tbl, writer, dst );
            }
        }
    }
    return rc;
}

rc_t writer_write( p_writer_t writer, int64_t row_id,
                   uint8_t * quality, uint32_t quality_len,
                   uint8_t * filter, uint32_t filter_len ) {
    rc_t rc = VCursorOpenRow( writer -> cur );
    if ( 0 != rc ) {
        ErrMsg( "VCursorOpenRow( #%ld ) -> %R", row_id, rc );
    } else {
        rc = VCursorWrite( writer -> cur,
                           writer -> quality_idx,
                           8, /* elem bits */
                           quality,
                           0, /* bit-offset */
                           quality_len );
        if ( 0 != rc ) {
            ErrMsg( "VCursorWrite( #%ld QUALITY_CACHE ) -> %R", row_id, rc );
        } else {
            rc = VCursorWrite( writer -> cur,
                               writer -> filter_idx,
                               8, /* elem bits */
                               filter,
                               0, /* bit-offset */
                               filter_len );
            if ( 0 != rc ) {
                ErrMsg( "VCursorWrite( #%ld READ_FILTER_CACHE ) -> %R", row_id, rc );
            } else {
                rc = VCursorCommitRow( writer -> cur );
                if ( 0 != rc ) {
                    ErrMsg( "VCursorCommitRow() #%ld ) -> %R", row_id, rc );
                } else {
                    rc = VCursorCloseRow( writer -> cur );
                    if ( 0 != rc ) {
                        ErrMsg( "VCursorCloseRow() #%ld ) -> %R", row_id, rc );
                    }
                }
            }
        }
    }
    return rc;
}

rc_t writer_destroy( p_writer_t writer ) {
    rc_t rc = 0;
    if ( NULL != writer -> cur ) {
        rc = VCursorCommit( writer -> cur );
        if ( 0 != rc ) {
            ErrMsg( "VCursorCommit() -> %R", rc );
        } else {
            rc = VCursorRelease( writer -> cur );
            if ( 0 != rc ) {
                ErrMsg( "VCursorRelease() -> %R", rc );
            }
        }
    }
    return rc;
}
