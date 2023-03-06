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

#include "cmn_iter.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_inspector_
#include "inspector.h"      /* inspector_path_to_vpath */
#endif

#ifndef _h_klib_num_gen_
#include <klib/num-gen.h>
#endif

#ifndef _h_vdb_table_
#include <vdb/table.h>
#endif

#ifndef _h_vdb_cursor_
#include <vdb/cursor.h>
#endif

#ifndef _h_vdb_database_
#include <vdb/database.h>
#endif

#ifndef _h_vfs_manager_
#include <vfs/manager.h>
#endif

#ifndef _h_vfs_path_
#include <vfs/path.h>
#endif


bool populate_cmn_iter_params( cmn_iter_params_t * params,
                               const KDirectory * dir,
                               const VDBManager * vdb_mgr,
                               const char * accession_short,
                               const char * accession_path,
                               size_t cursor_cache,
                               int64_t first_row,
                               uint64_t row_count ) {
    bool res = false;
    if ( NULL != params && NULL != dir && NULL != vdb_mgr &&
         NULL != accession_short && NULL != accession_path ) {
        params -> dir = dir;
        params -> vdb_mgr = vdb_mgr;
        params -> accession_short = accession_short;
        params -> accession_path = accession_path;
        params -> cursor_cache = cursor_cache;
        params -> first_row = first_row;
        params -> row_count = row_count;
        res = true;
    }
    return res;
}

typedef struct cmn_iter_t {
    const VCursor * cursor;
    struct num_gen * ranges;
    const struct num_gen_iter * row_iter;
    uint64_t row_count;
    int64_t first_row, row_id;
} cmn_iter_t;

/* ------------------------------------------------------------------------------------------------------- */

static rc_t cmn_open_db( const VDBManager * mgr, const char *path, const VDatabase ** db ) {
    VPath * v_path = NULL;
    rc_t rc = inspector_path_to_vpath( path, &v_path );
    if ( 0 == rc ) {
        rc = VDBManagerOpenDBReadVPath( mgr, db, NULL, v_path );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_open_db().VDBManagerOpenDBReadVPath( '%s' ) -> %R\n", path, rc );
        }

        {
            rc_t rc2 = VPathRelease( v_path );
            if ( 0 != rc2 ) {
                ErrMsg( "cmn_iter.c cmn_open_db().VPathRelease( '%s' ) -> %R\n", path, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static rc_t cmn_open_tbl( const VDBManager * mgr, const char *path, const VTable ** tbl ) {
    VPath * v_path = NULL;
    rc_t rc = inspector_path_to_vpath( path, &v_path );
    if ( 0 == rc ) {
        rc = VDBManagerOpenTableReadVPath( mgr, tbl, NULL, v_path );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_open_tbl().VDBManagerOpenTableReadVPath( '%s' ) -> %R\n", path, rc );
        }

        {
            rc_t rc2 = VPathRelease( v_path );
            if ( 0 != rc2 ) {
                ErrMsg( "cmn_iter.c cmn_open_tbl().VPathRelease( '%s' ) -> %R\n", path, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------------------- */

static rc_t cmn_release_mgr( const VDBManager * mgr, rc_t rc, const char * function, const char * accession_short ) {
    rc_t rc2 = VDBManagerRelease( mgr );
    if ( 0 != rc2 ) {
        ErrMsg( "cmn_iter.c %s( '%s' ).VDBManagerRelease() -> %R", function, accession_short, rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t cmn_release_db( const VDatabase * db, rc_t rc, const char * function, const char * accession_short ) {
    rc_t rc2 = VDatabaseRelease( db );
    if ( 0 != rc2 ) {
        ErrMsg( "cmn_iter.c %s( '%s' ).VDatabaseRelease() -> %R", function, accession_short, rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t cmn_release_tbl( const VTable * tbl, rc_t rc, const char * function, const char * accession_short ) {
    rc_t rc2 = VTableRelease( tbl );
    if ( 0 != rc2 ) {
        ErrMsg( "cmn_iter.c %s( '%s' ).VTableRelease() -> %R", function, accession_short, rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t cmn_release_curs( const VCursor * curs, rc_t rc, const char * function, const char * accession_short ) {
    rc_t rc2 = VCursorRelease( curs );
    if ( 0 != rc2 ) {
        ErrMsg( "cmn_iter.c %s( '%s' ).VCursorRelease() -> %R", function, accession_short, rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------------------- */

void destroy_cmn_iter( cmn_iter_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> row_iter ) {
            num_gen_iterator_destroy( self -> row_iter );
        }
        if ( NULL != self -> ranges ) {
            num_gen_destroy( self -> ranges );
        }
        if ( NULL != self -> cursor ) {
            cmn_release_curs( self -> cursor, 0, "destroy_cmn_iter", "-" );
        }
        free( ( void * ) self );
    }
}

static rc_t cmn_iter_open_cursor( const VTable * tbl, size_t cursor_cache,
                                  const VCursor ** cur, const char * accession_short ) {
    rc_t rc;
    if ( cursor_cache > 0 ) {
        rc = VTableCreateCachedCursorRead( tbl, cur, cursor_cache );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_iter_open_cursor().VTableCreateCachedCursorRead( %lu ) -> %R\n", cursor_cache, rc );
        }
    } else {
        rc = VTableCreateCursorRead( tbl, cur );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_iter_open_cursor().VTableCreateCursorRead() -> %R\n", rc );
        }
    }
    return cmn_release_tbl( tbl, rc, "cmn_iter_open_cursor", accession_short );
}

static rc_t cmn_iter_open_db( const VDBManager * mgr, const cmn_iter_params_t * cp,
                              const char * tblname, const VCursor ** cur ) {
    const VDatabase * db = NULL;
    rc_t rc = cmn_open_db( mgr, cp -> accession_path, &db );
    if ( 0 == rc ) {
        const VTable * tbl = NULL;
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", tblname );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_iter_open_db().VDatabaseOpenTableRead( '%s', '%s' ) -> %R\n",
                    cp -> accession_short, tblname, rc );
        } else {
            rc = cmn_iter_open_cursor( tbl, cp -> cursor_cache, cur, cp -> accession_short ); /* releases tbl... */
        }
        rc = cmn_release_db( db, rc, "cmn_iter_open_db", cp -> accession_short );
    }
    return rc;
}

static rc_t cmn_iter_open_tbl( const VDBManager * mgr, const cmn_iter_params_t * cp,
                               const VCursor ** cur ) {
    const VTable * tbl = NULL;
    rc_t rc = cmn_open_tbl( mgr, cp -> accession_path, &tbl );
    if ( 0 == rc ) {
        rc = cmn_iter_open_cursor( tbl, cp -> cursor_cache, cur, cp -> accession_short );
    }
    return rc;
}

rc_t make_cmn_iter( const cmn_iter_params_t * cp, const char * tblname, cmn_iter_t ** iter ) {
    rc_t rc = 0;
    if ( NULL == cp || NULL == cp -> dir || NULL == cp -> accession_short ||
         NULL == cp -> accession_path || NULL == iter ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c make_cmn_iter() -> %R", rc );
    } else {
        bool release_mgr = false;
        const VDBManager * mgr = cp -> vdb_mgr != NULL ? cp -> vdb_mgr : NULL;
        if ( NULL == mgr ) {
            rc = VDBManagerMakeRead( &mgr, cp -> dir );
            if ( 0 != rc ) {
                ErrMsg( "cmn_iter.c make_cmn_iter().VDBManagerMakeRead() -> %R\n", rc );
            } else {
                release_mgr = true;
            }
        }
        if ( 0 == rc ) {
            const VCursor * cur = NULL;
            if ( NULL != tblname ) {
                rc = cmn_iter_open_db( mgr, cp, tblname, &cur );
            } else {
                rc = cmn_iter_open_tbl( mgr, cp, &cur );
            }
            if ( 0 == rc ) {
                cmn_iter_t * i = calloc( 1, sizeof * i );
                if ( NULL == i ) {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                    ErrMsg( "cmn_iter.c make_cmn_iter().calloc( %d ) -> %R", ( sizeof * i ), rc );
                } else {
                    i -> cursor = cur;
                    i -> first_row = cp -> first_row;
                    i -> row_count = cp -> row_count;
                    *iter = i;
                }
            } else {
                rc = cmn_release_curs( cur, rc, "make_cmn_iter", cp -> accession_short );
            }

        }
        if ( release_mgr ) {
            rc = cmn_release_mgr( mgr, rc, "make_cmn_iter", cp -> accession_short );
        }
    }
    return rc;
}

static rc_t make_row_iter( struct num_gen * ranges, int64_t first, uint64_t count,
                           const struct num_gen_iter ** iter ) {
    rc_t rc;
    if ( num_gen_empty( ranges ) ) {
        rc = num_gen_add( ranges, first, count );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c make_row_iter().num_gen_add( %li, %ld ) -> %R", first, count, rc );
        }
    } else {
        rc = num_gen_trim( ranges, first, count );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c make_row_iter().num_gen_trim( %li, %ld ) -> %R", first, count, rc );
        }
    }
    rc = num_gen_iterator_make( ranges, iter );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c make_row_iter().num_gen_iterator_make() -> %R", rc );
    }
    return rc;
}

rc_t cmn_iter_set_range( struct cmn_iter_t * self, int64_t start_row, uint64_t row_count ) {
    rc_t rc;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_set_range() -> %R", rc );
    } else {
        if ( NULL != self -> row_iter ) {
            num_gen_iterator_destroy( self -> row_iter );
        }
        if ( NULL != self -> ranges ) {
            num_gen_destroy( self -> ranges );
        }

        rc = num_gen_make_sorted( &self -> ranges, true );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_iter_set_range().num_gen_make_sorted() -> %R\n", rc );
        }
        else if ( row_count > 0 ) {
            rc = num_gen_add( self -> ranges, start_row, row_count );
            if ( 0 != rc ) {
                ErrMsg( "cmn_iter.c cmn_iter_set_range().num_gen_add( %ld.%lu ) -> %R\n",
                        self -> first_row, self -> row_count, rc );
            } else {
                rc = make_row_iter( self -> ranges, start_row, row_count, &self -> row_iter );
                if ( 0 != rc ) {
                    ErrMsg( "cmn_iter.c cmn_iter_set_range().make_row_iter( %ld.%lu ) -> %R\n", start_row, row_count, rc );
                } else {
                    /*
                    bool has_next = cmn_iter_next( self, &rc );
                    if ( 0 != rc ) {
                        ErrMsg( "cmn_iter.c cmn_iter_set_range().cmn_iter_next() -> %R\n", rc );                        
                    } else {
                        if ( !has_next ) {
                            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                            ErrMsg( "cmn_iter.c cmn_iter_next() returned false!" );
                        }
                    }
                    */
                }
            }
        }
    }
    return rc;
}

rc_t cmn_iter_add_column( struct cmn_iter_t * self, const char * name, uint32_t * id ) {
    rc_t rc;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_add_column() -> %R", rc );
    } else {
        rc = VCursorAddColumn( self -> cursor, id, name );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_iter_add_column().VCursorAddColumn( '%s' ) -> %R", name, rc );
        }
    }
    return rc;
}

int64_t cmn_iter_row_id( const struct cmn_iter_t * self ) {
    return ( NULL == self ) ? 0 : self -> row_id;
}

uint64_t cmn_iter_row_count( struct cmn_iter_t * self ) {
    uint64_t res = 0;
    rc_t rc;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_row_count() -> %R", rc );
    } else {
        rc = num_gen_iterator_count( self -> row_iter, &res );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_iter_row_count().num_gen_iterator_count() -> %R\n", rc );
        }
    }
    return res;
}

bool cmn_iter_next( struct cmn_iter_t * self, rc_t * rc ) {
    if ( NULL == self ) { return false; }
    return num_gen_iterator_next( self -> row_iter, &self -> row_id, rc );
}

rc_t cmn_iter_range( struct cmn_iter_t * self, uint32_t col_id ) {
    rc_t rc;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_range() -> %R", rc );
    } else {
        rc = VCursorOpen( self -> cursor );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_iter_range().VCursorOpen() -> %R", rc );
        } else {
            rc = num_gen_make_sorted( &self -> ranges, true );
            if ( 0 != rc ) {
                ErrMsg( "cmn_iter.c cmn_iter_range().num_gen_make_sorted() -> %R\n", rc );
            }
            else if ( self -> row_count > 0 ) {
                rc = num_gen_add( self -> ranges, self -> first_row, self -> row_count );
                if ( 0 != rc ) {
                    ErrMsg( "cmn_iter.c cmn_iter_range().num_gen_add( %ld.%lu ) -> %R\n",
                            self -> first_row, self -> row_count, rc );
                }
            }
        }
        if ( 0 == rc ) {
            rc = VCursorIdRange( self -> cursor, col_id, &self -> first_row, &self -> row_count );
            if ( rc != 0 ) {
                ErrMsg( "cmn_iter.c cmn_iter_range().VCursorIdRange() -> %R", rc );
            } else {
                rc = make_row_iter( self -> ranges, self -> first_row, self -> row_count, &self -> row_iter );
                if ( 0 != rc ) {
                    ErrMsg( "cmn_iter.c cmn_iter_range().make_row_iter( %ld.%lu ) -> %R\n", self -> first_row, self -> row_count, rc );
                }
            }
        }
    }
    return rc;
}

rc_t cmn_read_uint64( struct cmn_iter_t * self, uint32_t col_id, uint64_t *value ) {
    uint32_t elem_bits, boff, row_len;
    const uint64_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_read_uint64( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    } else if ( 64 != elem_bits || 0 != boff ) {
        ErrMsg( "cmn_iter.c cmn_read_uint64( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    } else {
        if ( row_len > 0 ) {
            *value = *value_ptr;
        } else {
            *value = 0;
        }
    }
    return rc;
}

rc_t cmn_read_uint64_array( struct cmn_iter_t * self, uint32_t col_id, uint64_t *value,
                            uint32_t num_values, uint32_t * values_read ) {
    uint32_t elem_bits, boff, row_len;
    const uint64_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_read_uint64_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    } else if ( 64 != elem_bits || 0 != boff ) {
        ErrMsg( "cmn_iter.c cmn_read_uint64_array( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    } else {
        if ( row_len > 0 ) {
            if ( row_len > num_values ) { row_len = num_values; }
            memmove( (void *)value, (void *)value_ptr, row_len * 8 );
        }
        if ( NULL != values_read ) {
            * values_read = row_len;
        }
    }
    return rc;
}

rc_t cmn_read_uint32( struct cmn_iter_t * self, uint32_t col_id, uint32_t *value ) {
    uint32_t elem_bits, boff, row_len;
    const uint32_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_read_uint32( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 32 != elem_bits || 0 != boff ) {
        ErrMsg( "cmn_iter.c cmn_read_uint32( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    } else {
        if ( row_len < 1 ) {
            *value = 0;
        } else {
            *value = *value_ptr;
        }
    }
    return rc;
}

rc_t cmn_read_uint32_array( struct cmn_iter_t * self, uint32_t col_id, uint32_t ** values,
                            uint32_t * values_read ) {
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)values, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_read_uint32_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    } else if ( 32 != elem_bits || 0 != boff ) {
        ErrMsg( "row#%ld : bits=%d, boff=%d, len=%d\n", self -> row_id, elem_bits, boff, row_len );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    } else {
        if ( NULL != values_read ) {
            * values_read = row_len;
        }
    }
    return rc;
}

rc_t cmn_read_uint8( struct cmn_iter_t * self, uint32_t col_id, uint8_t *value ) {
    uint32_t elem_bits, boff, row_len;
    const uint8_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                     (const void **)&value_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_read_uint8( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 8 != elem_bits || 0 != boff ) {
        ErrMsg( "cmn_iter.c cmn_read_uint8( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    } else {
        if ( row_len < 1 ) {
            *value = 0;
        } else {
            *value = *value_ptr;
        }
    }
    return rc;
}

rc_t cmn_read_uint8_array( struct cmn_iter_t * self, uint32_t col_id, uint8_t ** values,
                            uint32_t * values_read ) {
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)values, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_read_uint8_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    } else if ( 8 != elem_bits || 0 != boff ) {
        ErrMsg( "cmn_iter.c cmn_read_uint8_array( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    } else {
        if ( values_read != NULL ) { *values_read = row_len; }
    }
    return rc;
}

rc_t cmn_read_String( struct cmn_iter_t * self, uint32_t col_id, String * value ) {
    uint32_t elem_bits, boff;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value->addr, &boff, &value -> len );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_read_String( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    } else if ( 8 != elem_bits || 0 != boff ) {
        ErrMsg( "cmn_iter.c cmn_read_String( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    } else {
        value -> size = value -> len;
    }
    return rc;
}

rc_t cmn_read_bool( struct cmn_iter_t * self, uint32_t col_id, bool * value ) {
    uint8_t v8;
    rc_t rc = cmn_read_uint8( self, col_id, &v8 );
    if ( 0 == rc ) {
        *value = ( v8 != 0 );
    } else {
        *value = false;
    }
    return rc;
}

static rc_t cmn_check_tbl_for_column( const VTable * tbl, const char * col_name, bool * present ) {
    struct KNamelist * columns;
    rc_t rc = VTableListReadableColumns ( tbl, &columns );
    if ( 0 != rc ) {
        ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VTableListReadableColumns() -> %R\n", col_name, rc );
    } else {
        VNamelist * nl_columns;
        rc = VNamelistFromKNamelist ( &nl_columns, columns );
        if ( 0 != rc ) {
            ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VNamelistFromKNamelist() -> %R\n", col_name, rc );
        } else {
            int32_t idx;
            rc = VNamelistContainsStr( nl_columns, col_name, &idx );
            if ( 0 == rc ) {
                *present = ( idx >= 0 );
            } else {
                *present = false;
            }
            {
                rc_t rc2 = VNamelistRelease( nl_columns );
                if ( 0 != rc2 ) {
                    ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VNamelistRelease() -> %R\n", col_name, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = KNamelistRelease ( columns );
            if ( 0 != rc2 ) {
                ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).KNamelistRelease() -> %R\n", col_name, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

rc_t cmn_check_tbl_column( KDirectory * dir, const VDBManager * vdb_mgr,
                           const char * accession_short, const char * accession_path,
                           const char * col_name, bool * present ) {
    rc_t rc = 0;
    if ( present != NULL ) { *present = false; }
    if ( NULL == dir || NULL == accession_short || NULL == accession_path || NULL == col_name || NULL == present ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s' ) -> %R", accession_short, col_name, rc );
    } else {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( NULL == mgr ) {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( 0 != rc ) {
                ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s' ).VDBManagerMakeRead() -> %R\n",
                        accession_short, col_name, rc );
            } else {
                release_mgr = true;
            }
        }
        if ( 0 == rc ) {
            const VTable * tbl = NULL;
            rc = cmn_open_tbl( mgr, accession_path, &tbl );
            if ( 0 == rc ) {
                rc = cmn_check_tbl_for_column( tbl, col_name, present );
                {
                    rc_t rc2 = cmn_release_tbl( tbl, rc, "cmn_check_tbl_column", accession_short );
                    if ( 0 != rc2 ) {
                        ErrMsg( "cmn_iter.c cmn_check_tbl_column( '%s' ).cmn_relese_tbl() -> %R\n", col_name, rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
            }
            if ( release_mgr ) {
                rc_t rc2 = cmn_release_mgr( mgr, rc, "cmn_check_column", accession_short );
                if ( 0 != rc2 ) {
                    ErrMsg( "cmn_iter.c cmn_check_tbl_column( '%s' ).cmn_relese_mgr() -> %R\n", col_name, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return rc;
}

rc_t cmn_check_db_column( KDirectory * dir, const VDBManager * vdb_mgr,
                          const char * accession_short, const char * accession_path,
                          const char * tbl_name, const char * col_name,  bool * present ) {
    rc_t rc = 0;
    if ( NULL != present ) { *present = false; }
    if ( NULL == dir || NULL == accession_short || NULL == accession_path ||
         NULL == tbl_name || NULL == col_name || NULL == present ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter. cmn_check_db_column( '%s', '%s', '%s' ) -> %R", accession_short, tbl_name, col_name, rc );
    } else {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( NULL == mgr ) {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( 0 != rc ) {
                ErrMsg( "cmn_iter.c cmn_check_db_column( '%s', '%s', '%s' ).VDBManagerMakeRead() -> %R",
                        accession_short, tbl_name, col_name, rc );
            } else {
                release_mgr = true;
            }
        }
        if ( 0 == rc ) {
            const VDatabase * db = NULL;
            rc = cmn_open_db( mgr, accession_path, &db );
            if ( 0 == rc ) {
                const VTable * tbl = NULL;
                rc = VDatabaseOpenTableRead ( db, &tbl, "%s", tbl_name );
                if ( 0 != rc ) {
                    ErrMsg( "cmn_iter.c cmn_check_db_column( '%s', '%s', '%s' ).VDatabaseOpenTableRead() -> %R",
                            accession_short, tbl_name, col_name, rc );
                }  else {
                    rc = cmn_check_tbl_for_column( tbl, col_name, present );
                    {
                        rc_t rc2 = cmn_release_tbl( tbl, rc, "cmn_check_db_column", accession_short );
                        if ( 0 != rc2 ) {
                            ErrMsg( "cmn_iter.c cmn_check_db_column( '%s' ).cmn_relese_tbl() -> %R\n", col_name, rc2 );
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                }
                {
                    rc_t rc2 = cmn_release_db( db, rc, "cmn_check_db_column", accession_short );
                    if ( 0 != rc2 ) {
                        ErrMsg( "cmn_iter.c cmn_check_db_column( '%s' ).cmn_relese_db() -> %R\n", col_name, rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
            }
            if ( release_mgr ) {
                rc_t rc2 = cmn_release_mgr( mgr, rc, "cmn_check_db_column", accession_short );
                if ( 0 != rc2 ) {
                    ErrMsg( "cmn_iter.c cmn_check_db_column( '%s' ).cmn_relese_mgr() -> %R\n", col_name, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return rc;
}

VNamelist * cmn_get_table_names( KDirectory * dir, const VDBManager * vdb_mgr,
                                 const char * accession_short, const char * accession_path ) {
    VNamelist * res = NULL;
    if ( NULL == dir || NULL == accession_short|| NULL == accession_path ) {
        ErrMsg( "cmn_iter. cmn_get_table_names( '%s' ) -> dir || accession NULL", accession_short );
    } else {
        rc_t rc = 0;
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( NULL == mgr ) {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( 0 != rc ) {
                ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDBManagerMakeRead() -> %R",
                        accession_short, rc );
            } else {
                release_mgr = true;
            }
        }
        if ( rc == 0 ) {
            const VDatabase * db;
            rc = cmn_open_db( mgr, accession_path, &db );
            if ( 0 == rc ) {
                KNamelist * tables;
                rc = VDatabaseListTbl ( db, &tables );
                if ( 0 != rc ) {
                    ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDatabaseListTbl() -> %R",
                            accession_short, rc );
                } else {
                    rc = VNamelistFromKNamelist ( &res, tables );
                    if ( 0 != rc ) {
                        ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VNamelistFromKNamelist() -> %R",
                                accession_short, rc );
                    }
                }
                {
                    rc_t rc2 = cmn_release_db( db, rc, "cmn_get_table_names", accession_short );
                    if ( 0 != rc2 ) {
                        ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).cmn_relese_db() -> %R\n", accession_short, rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
            }
            if ( release_mgr )
            {
                rc_t rc2 = cmn_release_mgr( mgr, rc, "cmn_get_table_names", accession_short );
                if ( 0 != rc2 ) {
                    ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).cmn_relese_mgr() -> %R\n", accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return res;
}

rc_t is_column_name_present( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    const char * tbl_name,
                    bool * presence ) {
    rc_t rc;
    if ( NULL == tbl_name ) {
        rc = cmn_check_tbl_column( dir, vdb_mgr, accession_short, accession_path,
                                    "NAME", presence ); /* cmn_iter.c */
    } else {
        rc = cmn_check_db_column( dir, vdb_mgr, accession_short, accession_path, tbl_name,
                                    "NAME", presence ); /* cmn_iter.c */
    }
    return rc;
}
