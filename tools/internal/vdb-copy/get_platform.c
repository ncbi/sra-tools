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
#include "get_platform.h"

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif

#ifndef _h_vdb_database_
#include <vdb/database.h>
#endif

#ifndef _h_vdb_table_
#include <vdb/table.h>
#endif

#ifndef _h_vdb_cursor_
#include <vdb/cursor.h>
#endif

#ifndef _h_vdb_schema_
#include <vdb/schema.h>
#endif

#ifndef _h_sra_sraschema_
#include <sra/sraschema.h>
#endif

#define PLATFORM_COL "(ascii)PLATFORM"

static rc_t get_platform_from_table( const VTable *my_table, char ** dst, 
                                     const char pre_and_postfix ) {
    const VCursor *my_cursor;
    rc_t rc = VTableCreateCursorRead( my_table, &my_cursor );
    if ( 0 == rc ) {
        rc = VCursorOpen( my_cursor );
        if ( 0 == rc ) {
            uint32_t col_idx;
            rc = VCursorAddColumn( my_cursor, &col_idx, PLATFORM_COL );
            if ( 0 == rc ) {
                rc = VCursorSetRowId( my_cursor, 1 );
                if ( 0 == rc ) {
                    rc = VCursorOpenRow( my_cursor );
                    if ( 0 == rc ) {
                        const void *src_buffer;
                        uint32_t offset_in_bits;
                        uint32_t element_count;
                    
                        rc = VCursorCellData( my_cursor, col_idx, NULL,
                            &src_buffer, &offset_in_bits, &element_count );
                        if ( 0 == rc ) {
                            char *src_ptr = (char*)src_buffer + ( offset_in_bits >> 3 );
                            if ( 0 != pre_and_postfix ) {
                                *dst = malloc( element_count + 3 );
                            } else {
                                *dst = malloc( element_count + 1 );
                            }
                            if ( NULL != *dst ) {
                                if ( 0 != pre_and_postfix ) {
                                    ( *dst )[ 0 ] = pre_and_postfix;
                                    memmove( &( *dst )[ 1 ], src_ptr, element_count );
                                    ( *dst )[ element_count + 1 ] = pre_and_postfix;
                                    ( *dst )[ element_count + 2 ] = 0;
                                } else {
                                    memmove( *dst, src_ptr, element_count );
                                    ( *dst )[ element_count ] = 0;
                                }
                            } else {
                                rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                            }
                        }
                        VCursorCloseRow( my_cursor );
                    }
                }
            }
        }
        VCursorRelease( my_cursor );
    }
    return rc;
}

rc_t get_table_platform( const char * table_path, char ** dst, 
                         const char pre_and_postfix ) {
    rc_t rc;
    KDirectory *my_directory;
    
    if ( NULL == table_path || 0 == dst ) {
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    rc = KDirectoryNativeDir( &my_directory );
    if ( 0 == rc ) {
        VDBManager *my_manager;
        rc = VDBManagerMakeUpdate ( &my_manager, my_directory );
        if ( 0 == rc ) {
            VSchema *my_schema;
            rc = VDBManagerMakeSRASchema( my_manager, &my_schema );
            if ( 0 == rc ) {
                const VTable *my_table;
                rc = VDBManagerOpenTableRead( my_manager, &my_table, my_schema, "%s", table_path );
                if ( 0 == rc ) {
                    rc = get_platform_from_table( my_table, dst, pre_and_postfix );
                    VTableRelease( my_table );
                }
                VSchemaRelease( my_schema );
            }
            VDBManagerRelease( my_manager );
        }
        KDirectoryRelease( my_directory );
    }
    return rc;
}

rc_t get_db_platform( const char * db_path, const char * tab_name, 
                      char ** dst, const char pre_and_postfix ) {
    rc_t rc;
    KDirectory *my_directory;
    
    if ( NULL == db_path || 0 == tab_name || 0 == dst ) {
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    rc = KDirectoryNativeDir( &my_directory );
    if ( 0 == rc ) {
        VDBManager *my_manager;
        rc = VDBManagerMakeUpdate ( &my_manager, my_directory );
        if ( 0 == rc ) {
            VSchema *my_schema;
            rc = VDBManagerMakeSRASchema( my_manager, &my_schema );
            if ( 0 == rc ) {
                const VDatabase *my_database;
                rc = VDBManagerOpenDBRead( my_manager, &my_database, my_schema, "%s", db_path );
                if ( 0 == rc ) {
                    const VTable *my_table;
                    rc = VDatabaseOpenTableRead( my_database, &my_table, "%s", tab_name );
                    if ( 0 == rc ) {
                        rc = get_platform_from_table( my_table, dst, pre_and_postfix );
                        VTableRelease( my_table );
                    }
                    VDatabaseRelease( my_database );
                }
                VSchemaRelease( my_schema );
            }
            VDBManagerRelease( my_manager );
        }
        KDirectoryRelease( my_directory );
    }
    return rc;
}
