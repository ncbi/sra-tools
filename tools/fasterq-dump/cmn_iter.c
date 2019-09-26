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


void destroy_cmn_iter( cmn_iter * self )
{
    if ( self != NULL )
    {
        if ( self -> row_iter != NULL )
            num_gen_iterator_destroy( self -> row_iter );
        if ( self -> ranges != NULL )
            num_gen_destroy( self -> ranges );
        if ( self -> cursor != NULL )
            VCursorRelease( self -> cursor );
        free( ( void * ) self );
    }
}

static rc_t cmn_iter_open_cursor( const VTable * tbl, size_t cursor_cache, const VCursor ** cur )
{
    rc_t rc;
    if ( cursor_cache > 0 )
    {
        rc = VTableCreateCachedCursorRead( tbl, cur, cursor_cache );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_iter_open_cursor().VTableCreateCachedCursorRead( %lu ) -> %R\n", cursor_cache, rc );
    }
    else
    {
        rc = VTableCreateCursorRead( tbl, cur );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_iter_open_cursor().VTableCreateCursorRead() -> %R\n", rc );
    }
    VTableRelease( tbl );
    return rc;
}

static rc_t cmn_iter_open_db( const VDBManager * mgr, VSchema * schema,
                              const cmn_params * cp, const char * tblname, const VCursor ** cur )
{
    const VDatabase * db = NULL;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", cp -> accession );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_iter_open_db().VDBManagerOpenDBRead( '%s' ) -> %R\n", cp -> accession, rc );
    else
    {
        const VTable * tbl = NULL;
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", tblname );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_iter_open_db().VDBManagerOpenDBRead( '%s', '%s' ) -> %R\n", cp -> accession, tblname, rc );
        else
            rc = cmn_iter_open_cursor( tbl, cp -> cursor_cache, cur );
        VDatabaseRelease( db );
    }
    return rc;
}

static rc_t cmn_iter_open_tbl( const VDBManager * mgr, VSchema * schema,
                               const cmn_params * cp, const VCursor ** cur )
{
    const VTable * tbl = NULL;
    rc_t rc = VDBManagerOpenTableRead ( mgr, &tbl, schema, "%s", cp -> accession );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_iter_open_tbl().VDBManagerOpenTableRead( '%s' ) -> %R\n", cp -> accession, rc );
    else
        rc = cmn_iter_open_cursor( tbl, cp -> cursor_cache, cur );
    return rc;
}

rc_t make_cmn_iter( const cmn_params * cp, const char * tblname, cmn_iter ** iter )
{
    rc_t rc = 0;
    if ( cp == NULL || cp -> dir == NULL || cp -> accession == NULL || iter == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c make_cmn_iter() -> %R", rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = cp -> vdb_mgr != NULL ? cp -> vdb_mgr : NULL;
        if ( mgr == NULL )
        {
            rc = VDBManagerMakeRead( &mgr, cp -> dir );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c make_cmn_iter().VDBManagerMakeRead() -> %R\n", rc );
            else
                release_mgr = true;
        }
        if ( rc == 0 )
        {
            VSchema * schema = NULL;
            rc = VDBManagerMakeSRASchema( mgr, &schema );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c make_cmn_iter().VDBManagerMakeSRASchema() -> %R\n", rc );
            else
            {
                const VCursor * cur = NULL;
                if ( tblname != NULL )
                    rc = cmn_iter_open_db( mgr, schema, cp, tblname, &cur );
                else
                    rc = cmn_iter_open_tbl( mgr, schema, cp, &cur );
                if ( rc == 0 )
                {
                    cmn_iter * i = calloc( 1, sizeof * i );
                    if ( i == NULL )
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
                    VCursorRelease( cur );
                VSchemaRelease( schema );   
            }
            if ( release_mgr )
                VDBManagerRelease( mgr );
        }
    }
    return rc;
}


rc_t cmn_iter_add_column( struct cmn_iter * self, const char * name, uint32_t * id )
{
    rc_t rc;
    if ( self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_add_column() -> %R", rc );
    }
    else
    {
        rc = VCursorAddColumn( self -> cursor, id, name );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_iter_add_column().VCursorAddColumn( '%s' ) -> %R", name, rc );
    }
    return rc;
}


int64_t cmn_iter_row_id( const struct cmn_iter * self )
{
    if ( self == NULL )
        return 0;
    return self -> row_id;
}


uint64_t cmn_iter_row_count( struct cmn_iter * self )
{
    uint64_t res = 0;
    rc_t rc;
    if ( self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_row_count() -> %R", rc );
    }
    else
    {
        rc = num_gen_iterator_count( self -> row_iter, &res );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_iter_row_count().num_gen_iterator_count() -> %R\n", rc );
    }
    return res;
}


bool cmn_iter_next( struct cmn_iter * self, rc_t * rc )
{
    if ( self == NULL )
        return false;
    return num_gen_iterator_next( self -> row_iter, &self -> row_id, rc );
}

static rc_t make_row_iter( struct num_gen * ranges, int64_t first, uint64_t count, 
                    const struct num_gen_iter ** iter )
{
    rc_t rc;
    if ( num_gen_empty( ranges ) )
    {
        rc = num_gen_add( ranges, first, count );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c make_row_iter().num_gen_add( %li, %ld ) -> %R", first, count, rc );
    }
    else
    {
        rc = num_gen_trim( ranges, first, count );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c make_row_iter().num_gen_trim( %li, %ld ) -> %R", first, count, rc );
    }
    rc = num_gen_iterator_make( ranges, iter );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c make_row_iter().num_gen_iterator_make() -> %R", rc );
    return rc;
}

rc_t cmn_iter_range( struct cmn_iter * self, uint32_t col_id )
{
    rc_t rc;
    if ( self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_iter_range() -> %R", rc );
    }
    else
    {
        rc = VCursorOpen( self -> cursor );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_iter_range().VCursorOpen() -> %R", rc );
        else
        {
            rc = num_gen_make_sorted( &self -> ranges, true );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_iter_range().num_gen_make_sorted() -> %R\n", rc );
            else if ( self -> row_count > 0 )
            {
                rc = num_gen_add( self -> ranges, self -> first_row, self -> row_count );
                if ( rc != 0 )
                    ErrMsg( "cmn_iter.c cmn_iter_range().num_gen_add( %ld.%lu ) -> %R\n",
                            self -> first_row, self -> row_count, rc );
            }
        }
        if ( rc == 0 )
        {
            rc = VCursorIdRange( self -> cursor, col_id, &self -> first_row, &self -> row_count );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_iter_range().VCursorIdRange() -> %R", rc );
            else
            {
                rc = make_row_iter( self -> ranges, self -> first_row, self -> row_count, &self -> row_iter );
                if ( rc != 0 )
                    ErrMsg( "cmn_iter.c cmn_iter_range().make_row_iter( %ld.%lu ) -> %R\n", self -> first_row, self -> row_count, rc );
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
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_read_uint64( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    else if ( elem_bits != 64 || boff != 0 )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint64( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( row_len > 0 )
            *value = *value_ptr;
        else
            *value = 0;
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
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_read_uint64_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    else if ( elem_bits != 64 || boff != 0 )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint64_array( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( row_len > 0 )
        {
            if ( row_len > num_values ) row_len = num_values;
            memmove( (void *)value, (void *)value_ptr, row_len * 8 );
        }
        if ( values_read != NULL )
            * values_read = row_len;
    }
    return rc;
}


rc_t cmn_read_uint32( struct cmn_iter * self, uint32_t col_id, uint32_t *value )
{
    uint32_t elem_bits, boff, row_len;
    const uint32_t * value_ptr;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value_ptr, &boff, &row_len );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_read_uint32( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    else if ( elem_bits != 32 || boff != 0 )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint32( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( row_len < 1 )
            *value = 0;
        else
            *value = *value_ptr;
    }
    return rc;
}

rc_t cmn_read_uint32_array( struct cmn_iter * self, uint32_t col_id, uint32_t ** values,
                            uint32_t * values_read )
{
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)values, &boff, &row_len );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_read_uint32_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    else if ( elem_bits != 32 || boff != 0 || row_len < 1 )
    {
        ErrMsg( "row#%ld : bits=%d, boff=%d, len=%d\n", self -> row_id, elem_bits, boff, row_len );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( values_read != NULL )
            * values_read = row_len;
    }
    return rc;
}

rc_t cmn_read_uint8_array( struct cmn_iter * self, uint32_t col_id, uint8_t ** values,
                            uint32_t * values_read )
{
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)values, &boff, &row_len );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_read_uint8_array( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    else if ( elem_bits != 8 || boff != 0 )
    {
        ErrMsg( "cmn_iter.c cmn_read_uint8_array( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
    {
        if ( values_read != NULL )
            * values_read = row_len;
    }
    return rc;
}

rc_t cmn_read_String( struct cmn_iter * self, uint32_t col_id, String * value )
{
    uint32_t elem_bits, boff;
    rc_t rc = VCursorCellDataDirect( self -> cursor, self -> row_id, col_id, &elem_bits,
                                 (const void **)&value->addr, &boff, &value -> len );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_read_String( #%ld ).VCursorCellDataDirect() -> %R\n", self -> row_id, rc );
    else if ( elem_bits != 8 || boff != 0 )
    {
        ErrMsg( "cmn_iter.c cmn_read_String( #%ld ) : bits=%d, boff=%d\n", self -> row_id, elem_bits, boff );
        rc = RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    else
        value -> size = value -> len;
    return rc;
}

static bool contains( VNamelist * tables, const char * table )
{
    uint32_t found = 0;
    rc_t rc = VNamelistIndexOf( tables, table, &found );
    return ( rc == 0 );
}

/*typedef enum acc_type_t { acc_csra, acc_sra_flat, acc_sra_db, acc_none } acc_type_t;*/
static acc_type_t cmn_get_db_type( const VDBManager * mgr, const char * accession )
{
    acc_type_t res = acc_none;
    const VDatabase * db = NULL;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", accession );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_get_db_type().VDBManagerOpenDBRead( '%s' ) -> %R\n", accession, rc );
    else
    {
        KNamelist * k_tables;
        rc = VDatabaseListTbl ( db, &k_tables );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_get_db_type().VDatabaseListTbl( '%s' ) -> %R\n", accession, rc );
        else
        {
            VNamelist * tables;
            rc = VNamelistFromKNamelist ( &tables, k_tables );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_db_type().VNamelistFromKNamelist( '%s' ) -> %R\n", accession, rc );
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
                    else if ( contains( tables, "CONSENSUS" ) )
                    {
                        res = acc_pacbio;
                    }
                }
            }
            KNamelistRelease ( k_tables );
        }
        VDatabaseRelease( db );        
    }
    return res;
}

rc_t cmn_get_acc_type( KDirectory * dir, const VDBManager * vdb_mgr,
                       const char * accession, acc_type_t * acc_type )
{
    rc_t rc = 0;
    if ( acc_type != NULL )
        *acc_type = acc_none;
    if ( dir == NULL || accession == NULL || acc_type == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s' ) -> %R", accession, rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( mgr == NULL )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s' ).VDBManagerMakeRead() -> %R\n", accession, rc );
            else
                release_mgr = true;
        }
        if ( rc == 0 )
        {
            int pt = VDBManagerPathType ( mgr, "%s", accession );
            switch( pt )
            {
                case kptDatabase    : *acc_type = cmn_get_db_type( mgr, accession ); break;
    
                case kptPrereleaseTbl:
                case kptTable       : *acc_type = acc_sra_flat; break;
            }
            if ( release_mgr )
                VDBManagerRelease( mgr );
        }
    }
    return rc;
}

static rc_t cmn_check_tbl_for_column( const VTable * tbl, const char * col_name, bool * present )
{
    struct KNamelist * columns;
    rc_t rc = VTableListReadableColumns ( tbl, &columns );
    if ( rc != 0 )
        ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VTableListReadableColumns() -> %R\n", col_name, rc );
    else
    {
        VNamelist * nl_columns;
        rc = VNamelistFromKNamelist ( &nl_columns, columns );
        if ( rc != 0 )
            ErrMsg( "cmn_iter.c cmn_check_tbl_for_column( '%s' ).VNamelistFromKNamelist() -> %R\n", col_name, rc );
        else
        {
            int32_t idx;
            rc = VNamelistContainsStr( nl_columns, col_name, &idx );
            if ( rc == 0 )
                *present = ( idx >= 0 );
            else
                *present = false;
            VNamelistRelease( nl_columns );
        }
        KNamelistRelease ( columns );
    }
    return rc;
}

rc_t cmn_check_tbl_column( KDirectory * dir, const VDBManager * vdb_mgr,
                           const char * accession, const char * col_name, bool * present )
{
    rc_t rc = 0;
    if ( present != NULL )
        *present = false;
    if ( dir == NULL || accession == NULL || col_name == NULL || present == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter.c cmn_check_column( '%s', '%s' ) -> %R", accession, col_name, rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( mgr == NULL )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s' ).VDBManagerMakeRead() -> %R\n", accession, col_name, rc );
            else
                release_mgr = true;
        }
        if ( rc == 0 )
        {
            const VTable * tbl = NULL;
            rc = VDBManagerOpenTableRead ( mgr, &tbl, NULL, "%s", accession );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s' ).VDBManagerOpenTableRead() -> %R\n", accession, col_name, rc );
            else
            {
                rc = cmn_check_tbl_for_column( tbl, col_name, present );
                VTableRelease( tbl );
            }
            if ( release_mgr )
                VDBManagerRelease( mgr );
        }
    }
    return rc;
}

rc_t cmn_check_db_column( KDirectory * dir, const VDBManager * vdb_mgr, const char * accession,
                          const char * tbl_name, const char * col_name,  bool * present )
{
    rc_t rc = 0;
    if ( present != NULL )
        *present = false;
    if ( dir == NULL || accession == NULL || tbl_name == NULL || col_name == NULL || present == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "cmn_iter. cmn_check_column( '%s', '%s', '%s' ) -> %R", accession, tbl_name, col_name, rc );
    }
    else
    {
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( mgr == NULL )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s', '%s' ).VDBManagerMakeRead() -> %R", accession, tbl_name, col_name, rc );
            else
                release_mgr = true;
        }
        if ( rc == 0 )
        {
            const VDatabase * db = NULL;
            rc = VDBManagerOpenDBRead ( mgr, &db, NULL, "%s", accession );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s', '%s' ).VDBManagerOpenDBRead() -> %R", accession, tbl_name, col_name, rc );
            else
            {
                const VTable * tbl = NULL;
                rc = VDatabaseOpenTableRead ( db, &tbl, "%s", tbl_name );
                if ( rc != 0 )
                    ErrMsg( "cmn_iter.c cmn_get_acc_type( '%s', '%s', '%s' ).VDatabaseOpenTableRead() -> %R", accession, tbl_name, col_name, rc );
                else
                {
                    rc = cmn_check_tbl_for_column( tbl, col_name, present );
                    VTableRelease( tbl );
                }
                VDatabaseRelease( db );
            }
            if ( release_mgr )
                VDBManagerRelease( mgr );
        }
    }
    return rc;
}

VNamelist * cmn_get_table_names( KDirectory * dir, const VDBManager * vdb_mgr, const char * accession )
{
    VNamelist * res = NULL;
    if ( dir == NULL || accession == NULL )
        ErrMsg( "cmn_iter. cmn_get_table_names( '%s' ) -> dir || accession NULL", accession );
    else
    {
        rc_t rc = 0;
        bool release_mgr = false;
        const VDBManager * mgr = vdb_mgr != NULL ? vdb_mgr : NULL;
        if ( mgr == NULL )
        {
            rc = VDBManagerMakeRead( &mgr, dir );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDBManagerMakeRead() -> %R", accession, rc );
            else
                release_mgr = true;
        }
        if ( rc == 0 )
        {
            const VDatabase * db;
            rc = VDBManagerOpenDBRead ( mgr, &db, NULL, "%s", accession );
            if ( rc != 0 )
                ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDBManagerOpenDBRead() -> %R", accession, rc );
            else
            {
                KNamelist * tables;
                rc = VDatabaseListTbl ( db, &tables );
                if ( rc != 0 )
                    ErrMsg( "cmn_iter.c cmn_get_table_names( '%s' ).VDatabaseListTbl() -> %R", accession, rc );
                else
                    rc = VNamelistFromKNamelist ( &res, tables );
                VDatabaseRelease ( db );
            }
            if ( release_mgr )
                VDBManagerRelease( mgr );
        }
    }
    return res;
}
