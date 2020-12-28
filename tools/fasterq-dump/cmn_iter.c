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
#include "helper.h"

#include <klib/out.h>
#include <klib/num-gen.h>

#include <sra/sraschema.h>

#include <kdb/manager.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>

#include <vfs/manager.h>
#include <vfs/path.h>

#include <insdc/sra.h>  /* platform enum */

#include <os-native.h>
#include <sysalloc.h>

typedef struct cmn_iter
{
    const VCursor * cursor;
    struct num_gen * ranges;
    const struct num_gen_iter * row_iter;
    uint64_t row_count;
    int64_t first_row, row_id;
} cmn_iter;

static rc_t cmn_iter_path_to_vpath( const char * path, VPath ** vpath )
{
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_iter_path_to_vpath().VFSManagerMake( %s ) -> %R\n", path, rc );
    }
    else
    {
        VPath * in_path = NULL;
        rc = VFSManagerMakePath( vfs_mgr, &in_path, "%s", path );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_path_to_vpath().VFSManagerMakePath( %s ) -> %R\n", path, rc );
        }
        else
        {
            rc = VFSManagerResolvePath( vfs_mgr, vfsmgr_rflag_kdb_acc, in_path, vpath );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_iter_path_to_vpath().VFSManagerResolvePath( %s ) -> %R\n", path, rc );
            }
            {
                rc_t rc2 = VPathRelease( in_path );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_iter_path_to_vpath().VPathRelease( %s ) -> %R\n", path, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VFSManagerRelease( vfs_mgr );
            if ( 0 != rc2 )
            {
                ErrMsg( "cmn_iter.c cmn_iter_path_to_vpath().VFSManagerRelease( %s ) -> %R\n", path, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

void destroy_cmn_iter( cmn_iter * self )
{
    if ( NULL != self )
    {
        if ( NULL != self -> row_iter )
        {
            num_gen_iterator_destroy( self -> row_iter );
        }
        if ( NULL != self -> ranges )
        {
            num_gen_destroy( self -> ranges );
        }
        if ( NULL != self -> cursor )
        {
            VCursorRelease( self -> cursor );
        }
        free( ( void * ) self );
    }
}

static rc_t cmn_iter_open_cursor( const VTable * tbl, size_t cursor_cache, const VCursor ** cur )
{
    rc_t rc;
    if ( cursor_cache > 0 )
    {
        rc = VTableCreateCachedCursorRead( tbl, cur, cursor_cache );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_open_cursor().VTableCreateCachedCursorRead( %lu ) -> %R\n", cursor_cache, rc );
        }
    }
    else
    {
        rc = VTableCreateCursorRead( tbl, cur );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_open_cursor().VTableCreateCursorRead() -> %R\n", rc );
        }
    }
    {
        rc_t rc2  = VTableRelease( tbl );
        if ( 0 != rc2 )
        {
            ErrMsg( "cmn_iter.c cmn_iter_open_cursor().VTableRelease() -> %R\n", rc2 );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}

static rc_t cmn_iter_open_db( const VDBManager * mgr, VSchema * schema,
                              const cmn_params * cp, const char * tblname, const VCursor ** cur )
{
    VPath * path = NULL;
    rc_t rc = cmn_iter_path_to_vpath( cp -> accession_path, &path );
    KOutMsg( "cmn_iter_open_db( '%s' )\n", cp -> accession_path );
    if ( 0 == rc )
    {
        const VDatabase * db = NULL;
        //rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", cp -> accession );
        rc = VDBManagerOpenDBReadVPath( mgr, &db, schema, path );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_open_db().VDBManagerOpenDBReadVPath( '%s' ) -> %R\n",
                    cp -> accession_short, rc );
        }
        else
        {
            const VTable * tbl = NULL;
            rc = VDatabaseOpenTableRead( db, &tbl, "%s", tblname );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_iter_open_db().VDatabaseOpenTableRead( '%s', '%s' ) -> %R\n",
                        cp -> accession_short, tblname, rc );
            }
            else
            {
                rc = cmn_iter_open_cursor( tbl, cp -> cursor_cache, cur ); /* releases tbl... */
            }
            {
                rc_t rc2 = VDatabaseRelease( db );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_iter_open_db().VDatabaseRelease( '%s' ) -> %R\n",
                            cp -> accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
            {
                rc_t rc2 = VPathRelease( path );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_iter_open_db().VPathRelease( '%s' ) -> %R\n",
                            cp -> accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return rc;
}

static rc_t cmn_iter_open_tbl( const VDBManager * mgr, VSchema * schema,
                               const cmn_params * cp, const VCursor ** cur )
{
    VPath * path = NULL;
    rc_t rc = cmn_iter_path_to_vpath( cp -> accession_path, &path );
    if ( 0 == rc )
    {
        const VTable * tbl = NULL;
        //rc_t rc = VDBManagerOpenTableRead ( mgr, &tbl, schema, "%s", cp -> accession );
        rc_t rc = VDBManagerOpenTableReadVPath( mgr, &tbl, schema, path );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_open_tbl().VDBManagerOpenTableReadVPath( '%s' ) -> %R\n",
                    cp -> accession_short, rc );
        }
        else
        {
            rc = cmn_iter_open_cursor( tbl, cp -> cursor_cache, cur );
        }
        {
            rc_t rc2 = VPathRelease( path );
            if ( 0 != rc2 )
            {
                ErrMsg( "cmn_iter.c cmn_iter_open_tbl().VPathRelease( '%s' ) -> %R\n",
                        cp -> accession_short, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }

   }
    return rc;
}

rc_t make_cmn_iter( const cmn_params * cp, const char * tblname, cmn_iter ** iter )
{
    rc_t rc = 0;
    if ( NULL == cp || NULL == cp -> dir || NULL == cp -> accession_short || 
         NULL == cp -> accession_path || NULL == iter )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c make_cmn_iter() -> %R", rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = cp -> vdb_mgr != NULL ? cp -> vdb_mgr : NULL;
        if ( NULL == mgr )
        {
            rc = VDBManagerMakeRead( &mgr, cp -> dir );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c make_cmn_iter().VDBManagerMakeRead() -> %R\n", rc );
            }
            else
            {
                release_mgr = true;
            }
        }
        if ( 0 == rc )
        {
            VSchema * schema = NULL;
            rc = VDBManagerMakeSRASchema( mgr, &schema );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c make_cmn_iter().VDBManagerMakeSRASchema() -> %R\n", rc );
            }
            else
            {
                const VCursor * cur = NULL;
                if ( NULL != tblname )
                {
                    rc = cmn_iter_open_db( mgr, schema, cp, tblname, &cur );
                }
                else
                {
                    rc = cmn_iter_open_tbl( mgr, schema, cp, &cur );
                }
                if ( 0 == rc )
                {
                    cmn_iter * i = calloc( 1, sizeof * i );
                    if ( NULL == i )
                    {
                        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                        ErrMsg( "cmn_iter.c make_cmn_iter().calloc( %d ) -> %R", ( sizeof * i ), rc );
                    }
                    else
                    {
                        i -> cursor = cur;
                        i -> first_row = cp -> first_row;
                        i -> row_count = cp -> row_count;
                        *iter = i;
                    }
                }
                else
                {
                    rc_t rc2 = VCursorRelease( cur );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "cmn_iter.c make_cmn_iter().VCursorRelease() -> %R", rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
                {
                    rc_t rc2 = VSchemaRelease( schema );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "cmn_iter.c make_cmn_iter().VSchemaRelease() -> %R", rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
            }
            if ( release_mgr )
            {
                rc_t rc2 = VDBManagerRelease( mgr );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c make_cmn_iter().VDBManagerRelease() -> %R", rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return rc;
}


rc_t cmn_iter_add_column( struct cmn_iter * self, const char * name, uint32_t * id )
{
    rc_t rc;
    if ( NULL == self )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_add_column() -> %R", rc );
    }
    else
    {
        rc = VCursorAddColumn( self -> cursor, id, name );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_add_column().VCursorAddColumn( '%s' ) -> %R", name, rc );
        }
    }
    return rc;
}


int64_t cmn_iter_row_id( const struct cmn_iter * self )
{
    return ( NULL == self ) ? 0 : self -> row_id;
}


uint64_t cmn_iter_row_count( struct cmn_iter * self )
{
    uint64_t res = 0;
    rc_t rc;
    if ( NULL == self )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_row_count() -> %R", rc );
    }
    else
    {
        rc = num_gen_iterator_count( self -> row_iter, &res );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_row_count().num_gen_iterator_count() -> %R\n", rc );
        }
    }
    return res;
}


bool cmn_iter_next( struct cmn_iter * self, rc_t * rc )
{
    if ( NULL == self )
    {
        return false;
    }
    return num_gen_iterator_next( self -> row_iter, &self -> row_id, rc );
}

static rc_t make_row_iter( struct num_gen * ranges, int64_t first, uint64_t count, 
                    const struct num_gen_iter ** iter )
{
    rc_t rc;
    if ( num_gen_empty( ranges ) )
    {
        rc = num_gen_add( ranges, first, count );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c make_row_iter().num_gen_add( %li, %ld ) -> %R", first, count, rc );
        }
    }
    else
    {
        rc = num_gen_trim( ranges, first, count );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c make_row_iter().num_gen_trim( %li, %ld ) -> %R", first, count, rc );
        }
    }
    rc = num_gen_iterator_make( ranges, iter );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c make_row_iter().num_gen_iterator_make() -> %R", rc );
    }
    return rc;
}

rc_t cmn_iter_range( struct cmn_iter * self, uint32_t col_id )
{
    rc_t rc;
    if ( NULL == self )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_range() -> %R", rc );
    }
    else
    {
        rc = VCursorOpen( self -> cursor );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_iter_range().VCursorOpen() -> %R", rc );
        }
        else
        {
            rc = num_gen_make_sorted( &self -> ranges, true );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_iter_range().num_gen_make_sorted() -> %R\n", rc );
            }
            else if ( self -> row_count > 0 )
            {
                rc = num_gen_add( self -> ranges, self -> first_row, self -> row_count );
                if ( 0 != rc )
                {
                    ErrMsg( "cmn_iter.c cmn_iter_range().num_gen_add( %ld.%lu ) -> %R\n",
                            self -> first_row, self -> row_count, rc );
                }
            }
        }
        if ( 0 == rc )
        {
            rc = VCursorIdRange( self -> cursor, col_id, &self -> first_row, &self -> row_count );
            if ( rc != 0 )
            {
                ErrMsg( "cmn_iter.c cmn_iter_range().VCursorIdRange() -> %R", rc );
            }
            else
            {
                rc = make_row_iter( self -> ranges, self -> first_row, self -> row_count, &self -> row_iter );
                if ( 0 != rc )
                {
                    ErrMsg( "cmn_iter.c cmn_iter_range().make_row_iter( %ld.%lu ) -> %R\n", self -> first_row, self -> row_count, rc );
                }
            }
        }
    }
    return rc;
}


rc_t cmn_read_uint64( struct cmn_iter * self, uint32_t col_id, uint64_t *value )
{
    uint32_t elem_bits, boff, row_len;
    const uint64_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value_ptr, &boff, &row_len );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint64( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 64 != elem_bits || 0 != boff )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint64( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( row_len > 0 )
        {
            *value = *value_ptr;
        }
        else
        {
            *value = 0;
        }
    }
    return rc;
}


rc_t cmn_read_uint64_array( struct cmn_iter * self, uint32_t col_id, uint64_t *value,
                            uint32_t num_values, uint32_t * values_read )
{
    uint32_t elem_bits, boff, row_len;
    const uint64_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value_ptr, &boff, &row_len );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint64_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 64 != elem_bits || 0 != boff )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint64_array( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( row_len > 0 )
        {
            if ( row_len > num_values )
            {
                row_len = num_values;
            }
            memmove( (void *)value, (void *)value_ptr, row_len * 8 );
        }
        if ( NULL != values_read )
        {
            * values_read = row_len;
        }
    }
    return rc;
}


rc_t cmn_read_uint32( struct cmn_iter * self, uint32_t col_id, uint32_t *value )
{
    uint32_t elem_bits, boff, row_len;
    const uint32_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value_ptr, &boff, &row_len );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint32( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 32 != elem_bits || 0 != boff )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint32( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( row_len < 1 )
        {
            *value = 0;
        }
        else
        {
            *value = *value_ptr;
        }
    }
    return rc;
}

rc_t cmn_read_uint32_array( struct cmn_iter * self, uint32_t col_id, uint32_t ** values,
                            uint32_t * values_read )
{
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)values, &boff, &row_len );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint32_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 32 != elem_bits || 0 != boff )
    {
        ErrMsg( "row#%ld : bits=%d, boff=%d, len=%d\n", self -> row_id, elem_bits, boff, row_len );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( NULL != values_read )
        {
            * values_read = row_len;
        }
    }
    return rc;
}

rc_t cmn_read_uint8_array( struct cmn_iter * self, uint32_t col_id, uint8_t ** values,
                            uint32_t * values_read )
{
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)values, &boff, &row_len );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint8_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 8 != elem_bits || 0 != boff )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint8_array( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( values_read != NULL )
        {
            * values_read = row_len;
        }
    }
    return rc;
}

rc_t cmn_read_String( struct cmn_iter * self, uint32_t col_id, String * value )
{
    uint32_t elem_bits, boff;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value->addr, &boff, &value -> len );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_read_String( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    }
    else if ( 8 != elem_bits || 0 != boff )
    {
        ErrMsg( "cmn_iter.c cmn_read_String( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        value -> size = value -> len;
    }
    return rc;
}

static bool contains( VNamelist * tables, const char * table )
{
    uint32_t found = 0;
    rc_t rc = VNamelistIndexOf( tables, table, &found );
    return ( 0 == rc );
}

static bool cmn_get_db_platform( const VDatabase * db, const char * accession_short, uint8_t * pf )
{
    bool res = false;
    const VTable * tbl = NULL;
    rc_t rc = VDatabaseOpenTableRead ( db, &tbl, "SEQUENCE" );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_get_db_platform().VDatabaseOpenTableRead( '%s' ) -> %R\n",
                 accession_short, rc );
    }
    else
    {
        const VCursor * curs = NULL;
        rc = VTableCreateCursorRead( tbl, &curs );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_get_db_platform().VTableCreateCursor( '%s' ) -> %R\n",
                    accession_short, rc );
        }
        else
        {
            uint32_t idx;
            rc = VCursorAddColumn( curs, &idx, "PLATFORM" );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_get_db_platform().VCursorAddColumn( '%s', PLATFORM ) -> %R\n",
                        accession_short, rc );
            }
            else
            {
                rc = VCursorOpen( curs );
                if ( 0 != rc )
                {
                    ErrMsg( "cmn_iter.c cmn_get_db_platform().VCursorOpen( '%s' ) -> %R\n",
                            accession_short, rc );
                }
                else
                {
                    int64_t first;
                    uint64_t count;
                    rc = VCursorIdRange( curs, idx, &first, &count );
                    if ( 0 != rc )
                    {
                        ErrMsg( "cmn_iter.c cmn_get_db_platform().VCursorIdRange( '%s' ) -> %R\n",
                                accession_short, rc );
                    }
                    else if ( count > 0 )
                    {
                        uint32_t elem_bits, boff, row_len;
                        uint8_t *values;
                        rc = VCursorCellDataDirect( curs, first, idx, &elem_bits, (const void **)&values, &boff, &row_len );
                        if ( 0 != rc )
                        {
                            ErrMsg( "cmn_iter.c cmn_get_db_platform().VCursorCellDataDirect( '%s' ) -> %R\n",
                                    accession_short, rc );
                        }
                        else if ( NULL != values && 0 == elem_bits && 0 == boff && row_len > 0 )
                        {
                            *pf = values[ 0 ];
                            res = true;
                        }
                    }
                }
            }
            {
                rc_t rc2 = VCursorRelease( curs );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_get_db_platform().VCursorRelease( '%s' ) -> %R\n",
                            accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VTableRelease( tbl );
            if ( 0 != rc2 )
            {
                ErrMsg( "cmn_iter.c cmn_get_db_platform().VTableRelease( '%s' ) -> %R\n",
                        accession_short, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return res;
}

/*typedef enum acc_type_t { acc_csra, acc_sra_flat, acc_sra_db, acc_none } acc_type_t;*/
static acc_type_t cmn_get_db_type( const VDBManager * mgr, const char * accession_short, const char * accession_path )
{
    acc_type_t res = acc_none;
    const VDatabase * db = NULL;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", accession_path );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_get_db_type().VDBManagerOpenDBRead( '%s' ) -> %R\n", 
                accession_short, rc );
    }
    else
    {
        KNamelist * k_tables;
        rc = VDatabaseListTbl ( db, &k_tables );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_get_db_type().VDatabaseListTbl( '%s' ) -> %R\n",
                    accession_short, rc );
        }
        else
        {
            VNamelist * tables;
            rc = VNamelistFromKNamelist ( &tables, k_tables );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_get_db_type().VNamelistFromKNamelist( '%s' ) -> %R\n",
                        accession_short, rc );
            }
            else
            {
                if ( contains( tables, "SEQUENCE" ) )
                {
                    res = acc_sra_db;
                    
                    /* we have at least a SEQUENCE-table */
                    if ( contains( tables, "PRIMARY_ALIGNMENT" ) &&
                         contains( tables, "REFERENCE" ) )
                    {
                        res = acc_csra;
                    }
                    else
                    {
                        if ( contains( tables, "CONSENSUS" ) ||
                             contains( tables, "ZMW_METRICS" ) ||
                             contains( tables, "PASSES" ) )
                        {
                            res = acc_pacbio;
                        }
                        else
                        {
                            /* last resort try to find out what the database-type is */
                            uint8_t pf;
                            if ( cmn_get_db_platform( db, accession_short, &pf ) )
                            {
                                if ( SRA_PLATFORM_PACBIO_SMRT == pf )
                                {
                                    res = acc_pacbio;
                                }
                            }
                        }
                    }
                }
            }
            {
                rc_t rc2 = KNamelistRelease ( k_tables );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_get_db_type().KNamelistRelease( '%s' ) -> %R\n",
                            accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }

            }
        }
        {
            rc_t rc2 = VDatabaseRelease( db );
            if ( 0 != rc2 )
            {
                ErrMsg( "cmn_iter.c cmn_get_db_type().VDatabaseRelease( '%s' ) -> %R\n",
                        accession_short, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return res;
}

rc_t cmn_get_acc_type( KDirectory * dir, const VDBManager * vdb_mgr,
                       const char * accession_short, const char * accession_path,
                       acc_type_t * acc_type )
{
    rc_t rc = 0;
    if ( NULL != acc_type )
    {
        *acc_type = acc_none;
    }
    if ( NULL == dir || NULL == accession_short || NULL == accession_path || NULL == acc_type )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s' ) -> %R", accession_short, rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( NULL == mgr )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( 0 == rc )
            {
                release_mgr = true;
            }
            else
            {
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s' ).VDBManagerMakeRead() -> %R\n",
                        accession_short, rc );
            }
        }
        if ( rc == 0 )
        {
            int pt = VDBManagerPathType ( mgr, "%s", accession_path );
            switch( pt )
            {
                case kptDatabase    : *acc_type = cmn_get_db_type( mgr, accession_short, accession_path ); break;
    
                case kptPrereleaseTbl:
                case kptTable       : *acc_type = acc_sra_flat; break;
            }
            if ( release_mgr )
            {
                rc_t rc2 = VDBManagerRelease( mgr );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_get_acc_type().VDBManagerRelease( '%s' ) -> %R\n",
                            accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return rc;
}

static rc_t cmn_check_tbl_for_column( const VTable * tbl, const char * col_name, bool * present )
{
    struct KNamelist * columns;
    rc_t rc = VTableListReadableColumns ( tbl, &columns );
    if ( 0 != rc )
    {
        ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VTableListReadableColumns() -> %R\n", col_name, rc );
    }
    else
    {
        VNamelist * nl_columns;
        rc = VNamelistFromKNamelist ( &nl_columns, columns );
        if ( 0 != rc )
        {
            ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VNamelistFromKNamelist() -> %R\n", col_name, rc );
        }
        else
        {
            int32_t idx;
            rc = VNamelistContainsStr( nl_columns, col_name, &idx );
            if ( 0 == rc )
            {
                *present = ( idx >= 0 );
            }
            else
            {
                *present = false;
            }
            {
                rc_t rc2 = VNamelistRelease( nl_columns );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VNamelistRelease() -> %R\n", col_name, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = KNamelistRelease ( columns );
            if ( 0 != rc2 )
            {
                ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).KNamelistRelease() -> %R\n", col_name, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

rc_t cmn_check_tbl_column( KDirectory * dir, const VDBManager * vdb_mgr,
                           const char * accession_short, const char * accession_path,
                           const char * col_name, bool * present )
{
    rc_t rc = 0;
    if ( present != NULL )
    {
        *present = false;
    }
    if ( NULL == dir || NULL == accession_short || NULL == accession_path || NULL == col_name || NULL == present )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s' ) -> %R", accession_short, col_name, rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( NULL == mgr )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s' ).VDBManagerMakeRead() -> %R\n",
                        accession_short, col_name, rc );
            }
            else
            {
                release_mgr = true;
            }
        }
        if ( 0 == rc )
        {
            const VTable * tbl = NULL;
            rc = VDBManagerOpenTableRead ( mgr, &tbl, NULL, "%s", accession_path );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s' ).VDBManagerOpenTableRead() -> %R\n",
                        accession_short, col_name, rc );
            }
            else
            {
                rc = cmn_check_tbl_for_column( tbl, col_name, present );
                {
                    rc_t rc2 = VTableRelease( tbl );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s' ).VTableRelease() -> %R\n",
                                accession_short, col_name, rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
            }
            if ( release_mgr )
            {
                rc_t rc2 = VDBManagerRelease( mgr );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s' ).VDBManagerRelease() -> %R\n",
                            accession_short, col_name, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return rc;
}

rc_t cmn_check_db_column( KDirectory * dir, const VDBManager * vdb_mgr,
                          const char * accession_short, const char * accession_path,
                          const char * tbl_name, const char * col_name,  bool * present )
{
    rc_t rc = 0;
    if ( NULL != present )
    {
        *present = false;
    }
    if ( NULL == dir || NULL == accession_short || NULL == accession_path ||
         NULL == tbl_name || NULL == col_name || NULL == present )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter. cmn_check_column( '%s', '%s', '%s' ) -> %R", accession_short, tbl_name, col_name, rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( NULL == mgr )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s', '%s' ).VDBManagerMakeRead() -> %R",
                        accession_short, tbl_name, col_name, rc );
            }
            else
            {
                release_mgr = true;
            }
        }
        if ( 0 == rc )
        {
            const VDatabase * db = NULL;
            rc = VDBManagerOpenDBRead ( mgr, &db, NULL, "%s", accession_path );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s', '%s' ).VDBManagerOpenDBRead() -> %R",
                        accession_short, tbl_name, col_name, rc );
            }
            else
            {
                const VTable * tbl = NULL;
                rc = VDatabaseOpenTableRead ( db, &tbl, "%s", tbl_name );
                if ( 0 != rc )
                {
                    ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s', '%s' ).VDatabaseOpenTableRead() -> %R",
                            accession_short, tbl_name, col_name, rc );
                }
                else
                {
                    rc = cmn_check_tbl_for_column( tbl, col_name, present );
                    {
                        rc_t rc2 = VTableRelease( tbl );
                        if ( 0 != rc2 )
                        {
                            ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s', '%s' ).VTableRelease() -> %R",
                                    accession_short, tbl_name, col_name, rc2 );
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                }
                {
                    rc_t rc2 = VDatabaseRelease( db );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s', '%s' ).VDatabaseRelease() -> %R",
                                accession_short, tbl_name, col_name, rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
            }
            if ( release_mgr )
            {
                rc_t rc2 = VDBManagerRelease( mgr );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s', '%s' ).VDBManagerRelease() -> %R",
                            accession_short, tbl_name, col_name, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return rc;
}

VNamelist * cmn_get_table_names( KDirectory * dir, const VDBManager * vdb_mgr,
                                 const char * accession_short, const char * accession_path )
{
    VNamelist * res = NULL;
    if ( NULL == dir || NULL == accession_short|| NULL == accession_path )
    {
        ErrMsg( "cmn_iter. cmn_get_table_names( '%s' ) -> dir || accession NULL", accession_short );
    }
    else
    {
        rc_t rc = 0;
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( NULL == mgr )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDBManagerMakeRead() -> %R",
                        accession_short, rc );
            }
            else
            {
                release_mgr = true;
            }
        }
        if ( rc == 0 )
        {
            const VDatabase * db;
            rc = VDBManagerOpenDBRead ( mgr, &db, NULL, "%s", accession_path );
            if ( 0 != rc )
            {
                ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDBManagerOpenDBRead() -> %R",
                        accession_short, rc );
            }
            else
            {
                KNamelist * tables;
                rc = VDatabaseListTbl ( db, &tables );
                if ( 0 != rc )
                {
                    ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDatabaseListTbl() -> %R",
                            accession_short, rc );
                }
                else
                {
                    rc = VNamelistFromKNamelist ( &res, tables );
                    if ( 0 != rc )
                    {
                        ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VNamelistFromKNamelist() -> %R",
                                accession_short, rc );
                    }
                }
                {
                    rc_t rc2 = VDatabaseRelease ( db );
                    if ( 0 != rc2 )
                    {
                        ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDatabaseRelease() -> %R",
                                accession_short, rc2 );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
            }
            if ( release_mgr )
            {
                rc_t rc2 = VDBManagerRelease( mgr );
                if ( 0 != rc2 )
                {
                    ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDBManagerRelease() -> %R",
                            accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
    }
    return res;
}
