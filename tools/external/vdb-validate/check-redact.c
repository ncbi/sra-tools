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

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/printf.h>
#include <klib/out.h>

#include <vfs/manager.h>
#include <vfs/path.h>

#include <kdb/manager.h>

#include <insdc/insdc.h>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

/* ==================================================================================== */

static rc_t ErrMsg( rc_t rc, const char * fmt, ... ) {
    rc_t rc2 = 0;
    if ( 0 != rc ) {
        char buffer[ 4096 ];
        size_t num_writ;
        
        va_list list;
        va_start( list, fmt );
        rc2 = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
        if ( 0 == rc2 ) {
            rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
        }
        va_end( list );
    }
    return rc2;
} 

#define GENERIC_RELEASE( OBJTYPE ) \
static rc_t OBJTYPE ## _Release( rc_t rc, const OBJTYPE * obj ) { \
    if ( NULL != obj ) { \
        rc_t rc2 = OBJTYPE##Release( obj ); \
        ErrMsg( rc2, "%sRelease() failed : %R", #OBJTYPE, rc2 ); \
        rc = ( 0 == rc ) ? rc2 : rc; \
    } \
    return rc; \
} \

GENERIC_RELEASE ( VFSManager )
GENERIC_RELEASE ( VPath )
GENERIC_RELEASE ( VCursor )
GENERIC_RELEASE ( VTable )
GENERIC_RELEASE ( VDatabase )
GENERIC_RELEASE ( KNamelist )
GENERIC_RELEASE ( VDBManager )
GENERIC_RELEASE ( KDirectory )

static rc_t path_to_vpath( const char * aPath, VPath ** vpath ) {
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    ErrMsg( rc, "VFSManagerMake() failed : %R", rc );
    if ( 0 == rc ) {
        VPath * in_path = NULL;
        rc = VFSManagerMakePath( vfs_mgr, &in_path, "%s", aPath );
        ErrMsg( rc, "VFSManagerMakePath() failed" );
        if ( 0 == rc ) {
            rc = VFSManagerResolvePath( vfs_mgr, vfsmgr_rflag_kdb_acc, in_path, vpath );
            ErrMsg( rc, "VFSManagerResolvePath() failed : %R", rc );
            rc = VPath_Release( rc, in_path );
        }
        rc = VFSManager_Release( rc, vfs_mgr );
    }
    return rc;
}

static void clear_recorded_errors( void ) {
    rc_t rc;
    const char * filename;
    const char * funcname;
    uint32_t line_nr;
    while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &line_nr ) ) {
    }
}

/* ==================================================================================== */

typedef struct columns_t {
    uint32_t read_filter, read, read_start, read_len;    
} columns_t;

static bool all_Ns( const uint8_t* read, uint32_t count ) {
    uint32_t N_count = 0;
    uint32_t idx;
    for ( idx = 0; idx < count; idx++ ) {
        if ( read[ idx ] == 'N' ) {
            N_count++;
        }
    }
    return ( N_count == count );
}

static rc_t check_redaction( const VCursor * cur, int64_t row_id, columns_t* columns, bool *correct ) {
    rc_t rc;
    uint32_t filter_element_bits, filter_row_len;
    uint32_t spot_element_bits, spot_row_len;
    uint32_t read_start_element_bits, read_start_row_len;
    uint32_t read_len_element_bits, read_len_row_len;    
    const uint8_t *read_filters = NULL;
    const uint8_t *spot = NULL;
    const uint32_t *read_start = NULL;
    const uint32_t *read_len = NULL;
    
    *correct = false;
    
    rc = VCursorCellDataDirect( cur, row_id, columns -> read_filter, &filter_element_bits,
                                (const void**)&read_filters, NULL, &filter_row_len );
    ErrMsg( rc, "VCursorCellDataDirect( %ld , READ_FILTER ) failed : %R", row_id, rc );
    if ( 0 == rc ) {
        rc = VCursorCellDataDirect( cur, row_id, columns -> read, &spot_element_bits,
                                    (const void**)&spot, NULL, &spot_row_len );
        ErrMsg( rc, "VCursorCellDataDirect( %ld , READ ) failed : %R", row_id, rc );
    }
    if ( 0 == rc ) {
        rc = VCursorCellDataDirect( cur, row_id, columns -> read_start, &read_start_element_bits,
                                    (const void**)&read_start, NULL, &read_start_row_len );
        ErrMsg( rc, "VCursorCellDataDirect( %ld , READ_START ) failed : %R", row_id, rc );
    }
    if ( 0 == rc ) {
        rc = VCursorCellDataDirect( cur, row_id, columns -> read_len, &read_len_element_bits,
                                    (const void**)&read_len, NULL, &read_len_row_len );
        ErrMsg( rc, "VCursorCellDataDirect( %ld , READ_LEN ) failed : %R", row_id, rc );
    }
    if ( 0 == rc ) {
        /* check if we have some NULL pointers for the data */
        if ( NULL == read_filters || NULL == spot || NULL == read_start || NULL == read_len ) {
            rc = RC( rcExe, rcFileFormat, rcEvaluating, rcConstraint, rcViolated );
            ErrMsg( rc, "at row %ld : NULL pointers are read by VCursorCellDataDirect", row_id );
        }
    }
    if ( 0 == rc ) {
        /* check for different row-lengths between READ_FILTER, READ_START, and READ_LEN */
        if ( filter_row_len != read_start_row_len || filter_row_len != read_len_row_len ) {
            rc = RC( rcExe, rcFileFormat, rcEvaluating, rcConstraint, rcViolated );
            ErrMsg( rc, "at row %ld : different row-lengths of READ_FILTER vs READ_START vs READ_LEN : %u / %u / %u",
                    row_id, filter_row_len, read_start_row_len, read_len_row_len );
        }
    }
    if ( 0 == rc ) {
        /* check for unexpected element-counts for READ_FILTER, READ, READ_START, and READ_LEN */
        if ( filter_element_bits != 8 || spot_element_bits != 8 || 
            read_start_element_bits != 32 || read_len_element_bits != 32 ) {
            rc = RC( rcExe, rcFileFormat, rcEvaluating, rcConstraint, rcViolated );
        ErrMsg( rc, "at row %ld : unexpected element-sizes for READ_FILTER, READ, READ_START, and READ_LEN : %u / %u / %u / %u",
                row_id, filter_element_bits, spot_element_bits, read_start_element_bits, read_len_element_bits );            
            }
    }
    if ( 0 == rc ) {
        /* check if the sum of READ_LEN adds up to the length of READ */
        uint32_t read_id;
        uint32_t sum = 0;
        for ( read_id = 0; read_id < read_len_row_len; read_id++ ) {
            sum += read_len[ read_id ];
        }
        if ( sum != spot_row_len ) {
            rc = RC( rcExe, rcFileFormat, rcEvaluating, rcConstraint, rcViolated );
            ErrMsg( rc, "at row %ld : unexpected sum of READ_LEN vs. length of READ : %u / %u",
                    row_id, sum, spot_row_len );
        }
    }
    if ( 0 == rc ) {
        /* check if the READ_START values do not point beyond the end of READ */
        uint32_t read_id;
        uint32_t incorrect = 0;
        for ( read_id = 0; read_id < read_start_row_len; read_id++ ) {
            if ( read_start[ read_id ] > spot_row_len ||
                read_start[ read_id ] + read_len[ read_id ] > spot_row_len ) {
                incorrect++;
                }
        }
        if ( incorrect > 0 ) {
            rc = RC( rcExe, rcFileFormat, rcEvaluating, rcConstraint, rcViolated );
            ErrMsg( rc, "at row %ld : invalid READ_START / READ_LEN values", row_id );
        }
    }
    
    if ( 0 == rc ) {
        /* iterating through the READS of the SPOT */
        uint32_t read_id;
        uint32_t correct_reads = 0;
        for ( read_id = 0; read_id < filter_row_len; read_id++ ) {
            if ( read_filters[ read_id ] == SRA_READ_FILTER_REDACTED ) {
                /* this read needs to be checked */
                const uint8_t * read = &( spot[ read_start[ read_id ] ] );
                if ( all_Ns( read, read_len[ read_id ] ) ) {
                    correct_reads++;                    
                }
            } else {
                /* this read does not need to be checked */
                correct_reads++;
            }
        }
        /* the spot is correct if we have as much correct_reads as the spot has reads */
        *correct = ( correct_reads == filter_row_len );
    }
    return rc;
}

static rc_t has_redact_flag( const VCursor * cur, int64_t row_id, uint32_t col_idx, bool *has_flag ) {
    uint32_t filter_element_bits, filter_row_len;
    const uint8_t *read_filters = NULL;
    rc_t rc = VCursorCellDataDirect( cur, row_id, col_idx, &filter_element_bits,
                                (const void**)&read_filters, NULL, &filter_row_len );
    *has_flag = false;
    if ( 0 == rc ) {
        uint32_t idx;
        uint32_t values_found = 0;
        for ( idx = 0; idx < filter_row_len; ++idx ) {
            if ( ( read_filters[ idx ] & SRA_READ_FILTER_REDACTED ) == SRA_READ_FILTER_REDACTED ) {
                values_found++;
            }
        }
        *has_flag = values_found > 0;
    }
    return rc;
}

static rc_t check_opened_table( const VTable *tbl, const char * aPath ) {
    const VCursor * cur;
    rc_t rc = VTableCreateCachedCursorRead( tbl, &cur, 0 );
    ErrMsg( rc, "VTableCreateCachedCursorRead( '%s' ) failed : %R", aPath, rc );
    if ( 0 == rc ) {
        columns_t columns;
        int64_t  first_row;
        uint64_t row_count;
        
        rc = VCursorAddColumn( cur, &columns.read_filter, "READ_FILTER" );
        ErrMsg( rc, "VCursorAddColumn( '%s', READ_FILTER ) failed : %R", aPath, rc );
        if ( 0 == rc ) {
            rc = VCursorAddColumn( cur, &columns.read, "READ" );
            ErrMsg( rc, "VCursorAddColumn( '%s', READ ) failed : %R", aPath, rc );
        }
        if ( 0 == rc ) {
            rc = VCursorAddColumn( cur, &columns.read_start, "READ_START" );
            ErrMsg( rc, "VCursorAddColumn( '%s', READ_START ) failed : %R", aPath, rc );
        }
        if ( 0 == rc ) {
            rc = VCursorAddColumn( cur, &columns.read_len, "READ_LEN" );
            ErrMsg( rc, "VCursorAddColumn( '%s', READ_LEN ) failed : %R", aPath, rc );
        }
        if ( 0 == rc ) {
            rc = VCursorOpen( cur );
            ErrMsg( rc, "VCursorOpen( '%s' ) failed : %R", aPath, rc );
        }
        if ( 0 == rc ) {
            rc = VCursorIdRange( cur, columns . read, &first_row, &row_count );
            ErrMsg( rc, "VCursorIdRange( '%s', READ ) failed : %R", aPath, rc );
        }
        if ( 0 == rc ) {
            int64_t row = first_row;
            uint64_t row_idx = 0;
            uint64_t correct_rows = 0;
            uint64_t incorrect_rows = 0;
            while ( 0 == rc && row_idx < row_count ) {
                bool has_flag;
                rc = has_redact_flag( cur, row, columns.read_filter, &has_flag );
                if ( 0 == rc && has_flag ) {
                    bool correct = false;
                    rc = check_redaction( cur, row, &columns, &correct );
                    if ( 0 == rc ) {
                        if ( correct ) {
                            correct_rows++;
                        } else {
                            incorrect_rows++;
                        }
                    }
                } else {
                    correct_rows++;
                }
                row++;
                row_idx++;
            }
            if ( 0 != incorrect_rows ) {
                rc = SILENT_RC( rcExe, rcFileFormat, rcEvaluating, rcConstraint, rcViolated );
                ErrMsg( rc, "%lu rows with incorrectly redacted bases detected for '%s'\n", incorrect_rows, aPath );
            } else {
                ( void )PLOGMSG( klogInfo, ( klogInfo, "Redact : $(correct) of $(total) rows correct",
                                         "correct=%lu,total=%lu", correct_rows, row_count ) );
            }
        }
        rc = VCursor_Release( rc, cur );
    }
    return rc;
}

static rc_t check_table( const VDBManager *mgr, const char * aPath ) {
    VPath * path = NULL;
    rc_t rc = path_to_vpath( aPath, &path );
    if ( 0 == rc ) {
        const VTable *tbl;
        rc = VDBManagerOpenTableReadVPath( mgr, &tbl, NULL, path );
        ErrMsg( rc, "VDBManagerOpenTableReadVPath( '%R' ) -> %R", aPath, rc );
        if ( 0 == rc ) {
            rc = check_opened_table( tbl, aPath ); /* <=== above */
            rc = VTable_Release( rc, tbl );
        }
        rc = VPath_Release( rc, path );
    }
    return rc;
}

static rc_t check_database( const VDBManager *mgr, const char * aPath ) {
    VPath * path = NULL;
    rc_t rc = path_to_vpath( aPath, &path );
    if ( 0 == rc ) {
        const VDatabase *db;
        rc = VDBManagerOpenDBReadVPath( mgr, &db, NULL, path );
        ErrMsg( rc, "VDBManagerOpenDBReadVPath( '%s' ) -> %R", aPath, rc );
        if ( 0 == rc ) {
            KNamelist *tbl_names;
            rc = VDatabaseListTbl( db, &tbl_names );
            ErrMsg( rc, "VDatabaseListTbl( '%s' ) -> %R", aPath, rc );
            if ( 0 == rc ) {
                if ( KNamelistContains( tbl_names, "SEQUENCE" ) ) {
                    /* we do have a SEQUENCE-table! */
                    const VTable * tbl;
                    rc = VDatabaseOpenTableRead( db, &tbl, "SEQUENCE" );
                    ErrMsg( rc, "VDatabaseOpenTableRead() failed : %R", rc );
                    if ( 0 == rc ) {
                        rc = check_opened_table( tbl, aPath ); /* <=== above */
                        rc = VTable_Release( rc, tbl );
                    }
                } else {
                    rc = RC( rcExe, rcDatabase, rcOpening, rcTable, rcNotFound );
                    ErrMsg( rc, "opened as vdb-database, but no SEQUENCE-table found" );
                }
                rc = KNamelist_Release( rc, tbl_names );
            }
            rc = VDatabase_Release( rc, db );
        }
        rc = VPath_Release( rc, path );
    }
    return rc;
}

static rc_t check_by_pathtype( const VDBManager *mgr, const char * aPath ) {
    rc_t rc;
    int path_type = ( VDBManagerPathType( mgr, "%s", aPath ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type ) {
        case kptDatabase    :   rc = check_database( mgr, aPath ); break; /* <=== above */
        case kptPrereleaseTbl:
        case kptTable       :   rc = check_table( mgr, aPath ); break; /* <=== above */
        default             :   rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        PLOGERR( klogInt, ( klogInt, rc,
                            "the path '$(p)' cannot be opened as database or table",
                            "p=%s", aPath ) );
        break;
    }
    return rc;
}

rc_t check_redact( const char *aPath ) {
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    ErrMsg( rc, "KDirectoryNativeDir() failed : %R", rc );
    if ( 0 == rc ) {
        const VDBManager *mgr;
        rc = VDBManagerMakeRead( &mgr, dir );
        ErrMsg( rc, "VDBManagerMakeRead() failed : %R", rc );
        if( 0 == rc ) {
            rc = check_by_pathtype( mgr, aPath );    /* <=== above */
            rc = VDBManager_Release( rc, mgr );
        }
        rc = KDirectory_Release( rc, dir );
    }
    clear_recorded_errors();
    return rc;
}
