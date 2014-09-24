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
#include <sysalloc.h>
#include <stdlib.h> /* for malloc */
#include <string.h> /* for memcpy */

#define PLATFORM_COL "(ascii)PLATFORM"

static rc_t get_platform_from_table( const VTable *my_table, char ** dst, 
                                     const char pre_and_postfix )
{
    const VCursor *my_cursor;
    rc_t rc = VTableCreateCursorRead( my_table, &my_cursor );
    if ( rc == 0 )
    {
        rc = VCursorOpen( my_cursor );
        if ( rc == 0 )
        {
            uint32_t col_idx;
            rc = VCursorAddColumn( my_cursor, &col_idx, PLATFORM_COL );
            if ( rc == 0 )
            {
                rc = VCursorSetRowId( my_cursor, 1 );
                if ( rc == 0 )
                {
                    rc = VCursorOpenRow( my_cursor );
                    if ( rc == 0 )
                    {
                        const void *src_buffer;
                        uint32_t offset_in_bits;
                        uint32_t element_count;
                    
                        rc = VCursorCellData( my_cursor, col_idx, NULL,
                            &src_buffer, &offset_in_bits, &element_count );
                        if ( rc == 0 )
                        {
                            char *src_ptr = (char*)src_buffer + ( offset_in_bits >> 3 );
                            if ( pre_and_postfix != 0 )
                                *dst = malloc( element_count + 3 );
                            else
                                *dst = malloc( element_count + 1 );
                            if ( *dst != NULL )
                            {
                                if ( pre_and_postfix != 0 )
                                {
                                    (*dst)[ 0 ] = pre_and_postfix;
                                    memcpy( &(*dst)[1], src_ptr, element_count );
                                    (*dst)[ element_count + 1 ] = pre_and_postfix;
                                    (*dst)[ element_count + 2 ] = 0;
                                }
                                else
                                {
                                    memcpy( *dst, src_ptr, element_count );
                                    (*dst)[ element_count ] = 0;
                                }
                            }
                            else
                                rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
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
                         const char pre_and_postfix )
{
    rc_t rc;
    KDirectory *my_directory;
    
    if ( table_path == NULL || dst == 0 )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    rc = KDirectoryNativeDir( &my_directory );
    if ( rc == 0 )
    {
        VDBManager *my_manager;
        rc = VDBManagerMakeUpdate ( &my_manager, my_directory );
        if ( rc == 0 )
        {
            VSchema *my_schema;
            rc = VDBManagerMakeSRASchema( my_manager, &my_schema );
            if ( rc == 0 )
            {
                const VTable *my_table;
                rc = VDBManagerOpenTableRead( my_manager, &my_table, my_schema, "%s", table_path );
                if ( rc == 0 )
                {
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
                      char ** dst, const char pre_and_postfix )
{
    rc_t rc;
    KDirectory *my_directory;
    
    if ( db_path == NULL || tab_name == 0 || dst == 0 )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    rc = KDirectoryNativeDir( &my_directory );
    if ( rc == 0 )
    {
        VDBManager *my_manager;
        rc = VDBManagerMakeUpdate ( &my_manager, my_directory );
        if ( rc == 0 )
        {
            VSchema *my_schema;
            rc = VDBManagerMakeSRASchema( my_manager, &my_schema );
            if ( rc == 0 )
            {
                const VDatabase *my_database;
                rc = VDBManagerOpenDBRead( my_manager, &my_database, my_schema, "%s", db_path );
                if ( rc == 0 )
                {
                    const VTable *my_table;
                    rc = VDatabaseOpenTableRead( my_database, &my_table, "%s", tab_name );
                    if ( rc == 0 )
                    {
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
