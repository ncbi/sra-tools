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

#include "coldefs.h"
#include "helper.h"
#include "definitions.h"

#include <klib/text.h>
#include <klib/printf.h>

#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* allocate a column-definition */
static p_col_def col_defs_init_col( const char* name )
{
    p_col_def res = NULL;
    if ( name == NULL ) return res;
    if ( name[0] == 0 ) return res;
    res = calloc( 1, sizeof( col_def ) );
    /* because of calloc all members are zero! */
    if ( res != NULL )
        res->name = string_dup_measure ( name, NULL );
    return res;
}


/*
 * helper-function for: col_defs_destroy
 * - free's everything a node owns
 * - free's the node
*/
static void CC col_defs_destroy_node( void* node, void* data )
{
    p_col_def p_node = (p_col_def)node;
    if ( p_node != NULL )
    {
        if ( p_node->name != NULL )
            free( p_node->name );
        if ( p_node->src_cast )
            free( p_node->src_cast );
        if ( p_node->dst_cast )
            free( p_node->dst_cast );
        free( p_node );
    }
}


/*
 * initializes a column-definitions-list
*/
rc_t col_defs_init( col_defs** defs )
{
    if ( defs == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    (*defs) = calloc( 1, sizeof( col_defs ) );
    if ( *defs == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    VectorInit( &((*defs)->cols), 0, 5 );
    (*defs)->filter_idx = -1;
    return 0;
}


/*
 * destroys the column-definitions-list
*/
rc_t col_defs_destroy( col_defs* defs )
{
    if ( defs == NULL )
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
    VectorWhack( &(defs->cols), col_defs_destroy_node, NULL );
    free( defs );
    return 0;
}


/*
 * helper-function for: col_defs_parse_string / col_defs_extract_from_table
 * - creates a column-definition by the column-name
 * - adds the definition to the column-definition-vector
*/
static rc_t col_defs_append_col( col_defs* defs, const char* name )
{
    rc_t rc;
    p_col_def new_col = col_defs_init_col( name );
    if ( new_col == NULL )
        rc = RC( rcVDB, rcNoTarg, rcParsing, rcMemory, rcExhausted );
    else
        rc = VectorAppend( &(defs->cols), NULL, new_col );
    return rc;
}


/*
 * calls VTableListReadableColumns to get a list of all column-names of the table
 * creates a temporary read-cursor to test and get the column-type
 * walks the KNamelist with the available column-names
 * tries to add every one of them to the temp. cursor
 * if the column can be added to the cursor, it is appended
 * to the column-definition list
*/
rc_t col_defs_extract_from_table( col_defs* defs, const VTable *table )
{
    KNamelist *names;
    rc_t rc;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( table == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    rc = VTableListReadableColumns( table, &names );
    DISP_RC( rc, "col_defs_extract_from_table:VTableListReadableColumns() failed" );
    if ( rc == 0 )
    {
        const VCursor *cursor;
        rc = VTableCreateCursorRead( table, &cursor );
        DISP_RC( rc, "col_defs_extract_from_table:VTableCreateCursorRead() failed" );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = KNamelistCount( names, &count );
            DISP_RC( rc, "col_defs_extract_from_table:KNamelistCount() failed" );
            if ( rc == 0 )
            {
                uint32_t idx;
                for ( idx = 0; idx < count && rc == 0; ++idx )
                {
                    const char *name;
                    rc = KNamelistGet( names, idx, &name );
                    DISP_RC( rc, "col_defs_extract_from_table:KNamelistGet() failed" );
                    if ( rc == 0 )
                    {
                        uint32_t temp_idx;
                        rc_t rc1 = VCursorAddColumn( cursor, &temp_idx, "%s", name );
                        DISP_RC( rc1, "col_defs_extract_from_table:VCursorAddColumn() failed" );
                        if ( rc1 == 0 )
                        {
                            rc = col_defs_append_col( defs, name );
                            DISP_RC( rc, "col_defs_extract_from_table:col_defs_append_col() failed" );
                        }
                    }
                }
            }
            rc = VCursorRelease( cursor );
            DISP_RC( rc, "col_defs_extract_from_table:VCursorRelease() failed" );
        }
        rc = KNamelistRelease( names );
        DISP_RC( rc, "col_defs_extract_from_table:KNamelistRelease() failed" );
    }
    return rc;
}


rc_t col_defs_clear_to_copy( col_defs* defs )
{
    rc_t rc = 0;
    uint32_t idx, len;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    len = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < len && rc == 0; ++idx )
    {
        p_col_def item = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( item != NULL )
            item->to_copy = false;
    }
    return rc;
}


/*
 * walks the list of column-definitions and tries to add all off them
 * to the given cursor, it stops if one of them fails to be added
 * for every column it detects type_decl and type_desc
 * if both calls are successful the column-def is marked as valid
*/
rc_t col_defs_add_to_rd_cursor( col_defs* defs, const VCursor *cursor, bool show )
{
    rc_t rc = 0;
    uint32_t idx, len;
    
    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( cursor == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    len = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < len && rc == 0; ++idx )
    {
        p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( col != NULL )
        {
            if ( col->src_cast != NULL )
            {
                rc = VCursorAddColumn( cursor, &(col->src_idx), "%s", col->src_cast );
                DISP_RC( rc, "col_defs_add_to_cursor:VCursorAddColumn() failed" );
                if ( rc == 0 )
                {
                    rc = VCursorDatatype( cursor, col->src_idx,
                              &(col->type_decl), &(col->type_desc) );
                    DISP_RC( rc, "col_defs_add_to_cursor:VCursorDatatype() failed" );
                    col->src_valid = ( rc == 0 );
                    if ( show && col->src_valid )
                        KOutMsg( "added to rd-cursor: >%s<\n", col->name );
                }

            }
        }
    }
    return rc;
}


/*
 * retrieves a pointer to a column-definition
 * name defines the name of the wanted column-definition
*/
p_col_def col_defs_find( col_defs* defs, const char * name )
{
    p_col_def res = NULL;
    uint32_t idx, len;
    if ( defs == NULL || name == NULL || name[0] == 0 )
        return res;

    len = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < len && res == NULL; ++idx )
    {
        p_col_def item = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( nlt_strcmp( item->name, name ) == 0 )
            res = item;
    }
    return res;
}


/*
 * retrieves the index of a column-definition
 * name defines the name of the wanted column-definition
*/
int32_t col_defs_find_idx( col_defs* defs, const char * name )
{
    int32_t res = -1;
    uint32_t idx, len;
    if ( defs == NULL || name == NULL || name[0] == 0 )
        return res;

    len = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < len && res == -1; ++idx )
    {
        p_col_def item = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( nlt_strcmp( item->name, name ) == 0 )
            res = (int32_t)idx;
    }
    return res;
}


/*
 * retrieves a pointer to a column-definition
 * idx defines the Vector-Index of the wanted column-definition
 * !!! not the read or write cursor index !!!
*/
p_col_def col_defs_get( col_defs* defs, const uint32_t idx )
{
    p_col_def res = NULL;
    if ( defs != NULL )
        res = (p_col_def) VectorGet ( &(defs->cols), idx );
    return res;
}


/*
 * adds all columns marked with to_copy == true to the cursor
*/
rc_t col_defs_add_to_wr_cursor( col_defs* defs, const VCursor* cursor, bool show )
{
    uint32_t idx, len;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( cursor == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    len = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < len; ++idx )
    {
        p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( col != NULL )
            if ( col->to_copy && col->dst_cast != NULL )
            {
                rc_t rc = VCursorAddColumn( cursor, &(col->dst_idx), "%s", col->dst_cast );
                col->to_copy = ( rc == 0 );
                if ( show )
                {
                    if ( col->to_copy )
                        KOutMsg( "added to wr-cursor: >%s<\n", col->name );
                    else
                        KOutMsg( "cannot add >%s<\n", col->name );
                }
            }
    }
    return 0;
}


/*
 * searches in the column-vector for a successful string-comparison
 * with the hardcoded name of the filter-column
 * stores it's vector-idx in defs->filter_idx
 * does not require an open cursor.
*/
rc_t col_defs_detect_filter_col( col_defs* defs, const char *name )
{
    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( name == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    defs->filter_idx = col_defs_find_idx( defs, name );
    if ( defs->filter_idx < 0 )
        return RC( rcExe, rcNoTarg, rcResolving, rcItem, rcNotFound );
    return 0;
}


/*
 * walks through the column-names and marks every column thats
 * name is in "redactable_cols" as redactable
 * sets the redact-value to zero
 * if the default-type of the column is in the list of dna-types
 * (a hardcoded list) it sets the redact-value to 'N'
 * does not require an open cursor. 
*/
rc_t col_defs_detect_redactable_cols_by_name( col_defs* defs,
                                const char * redactable_cols )
{
    const KNamelist *r_columns;
    rc_t rc;
    
    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( defs == NULL || redactable_cols == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    rc = nlt_make_namelist_from_string( &r_columns, redactable_cols );
    if ( rc == 0 )
    {
        uint32_t idx, len = VectorLength( &(defs->cols) );
        for ( idx = 0;  idx < len && rc == 0; ++idx )
        {
            p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( col != NULL )
                if ( !(col->redactable) )
                    col->redactable = nlt_is_name_in_namelist( r_columns, col->name );
        }
        KNamelistRelease( r_columns );
    }
    return rc;
}


static rc_t redactable_types_2_type_id_vector( const VSchema * s,
                                               const char * redactable_types,
                                               Vector * id_vector )
{
    const KNamelist *r_types;
    rc_t rc;
    if ( redactable_types == NULL || s == NULL || id_vector == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    rc = nlt_make_namelist_from_string( &r_types, redactable_types );
    if ( rc == 0 )
    {
        uint32_t count, idx;

        rc = KNamelistCount( r_types, &count );
        if ( rc == 0 && count > 0 )
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char *name;
                rc = KNamelistGet( r_types, idx, &name );
                if ( rc == 0 )
                {
                    VTypedecl td;
                    rc = VSchemaResolveTypedecl ( s, &td, "%s", name );
                    if ( rc == 0 )
                    {
                        uint32_t *id = malloc( sizeof *id );
                        if ( id != NULL )
                        {
                            *id = td.type_id;
                            rc = VectorAppend ( id_vector, NULL, id );
                        }
                        else
                            rc = RC( rcExe, rcNoTarg, rcResolving, rcMemory, rcExhausted );
                    }
                }
            }
        KNamelistRelease( r_types );
    }
    return rc;
}


static void CC type_id_ptr_whack ( void *item, void *data )
{
    free( item );
}


/*
 * walks through the columns and checks every column against
 * a list of types, if one of the src-types of the column
 * is in the list it is marked as redactable
*/
rc_t col_defs_detect_redactable_cols_by_type( col_defs* defs,
                        const VSchema * s, const matcher * m,
                        const char * redactable_types )
{
    Vector id_vector;
    rc_t rc;
    
    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( redactable_types == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
    
    VectorInit ( &id_vector, 0, 20 );
    rc = redactable_types_2_type_id_vector( s, redactable_types, &id_vector );
    if ( rc == 0 )
    {
        uint32_t idx, len = VectorLength( &(defs->cols) );
        for ( idx = 0;  idx < len; ++idx )
        {
            p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( col != NULL )
                if ( ! col->redactable )
                    col->redactable = matcher_src_has_type( m, s, col->name, &id_vector );
        }
        VectorWhack ( &id_vector, type_id_ptr_whack, NULL );
    }

/*
    rc = nlt_make_namelist_from_string( &r_types, redactable_types );
    if ( rc == 0 )
    {
        uint32_t idx, len = VectorLength( &(defs->cols) );
        for ( idx = 0;  idx < len; ++idx )
        {
            p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( col != NULL )
                if ( ! col->redactable )
                    col->redactable = matcher_src_has_type( m, col->name, r_types );
        }
        KNamelistRelease( r_types );
    }
*/
    return rc;
}


rc_t col_defs_unmark_do_not_redact_columns(  col_defs* defs,
                    const char * do_not_redact_cols )
{
    const KNamelist *names;
    rc_t rc;
    
    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( do_not_redact_cols == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    rc = nlt_make_namelist_from_string( &names, do_not_redact_cols );
    if ( rc == 0 )
    {
        uint32_t idx, len = VectorLength( &(defs->cols) );
        for ( idx = 0;  idx < len; ++idx )
        {
            p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( col != NULL )
                if ( col->redactable )
                    if ( nlt_is_name_in_namelist( names, col->name ) )
                        col->redactable = false;
        }
        KNamelistRelease( names );
    }
    return rc;
}

/*
 * walks through the column-definitions and counts how
 * many of them have the to-copy-flag
 * does not require an open cursor. 
*/
uint32_t col_defs_count_copy_cols( col_defs* defs )
{
    uint32_t res = 0;
    if ( defs != NULL )
    {
        uint32_t idx, len = VectorLength( &(defs->cols) );
        for ( idx = 0;  idx < len; ++idx )
        {
            p_col_def item = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( item != NULL )
                if ( item->to_copy )
                    res++;
        }
    }
    return res;
}


/*
 * walks through the column-definitions and if a column
 * has the to_copy-flag set, and it's name is in the
 * given list of column-names the to copy-flag is cleared
 * does not require an open cursor.
*/
rc_t col_defs_exclude_these_columns( col_defs* defs, const char * prefix, const char * column_names )
{
    rc_t rc = 0;
    const KNamelist *names;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    /* it is OK if we have not column-names to exclude */
    if ( column_names == NULL )
        return 0;
    if ( column_names[0] == 0 )
        return 0;

    rc = nlt_make_namelist_from_string( &names, column_names );
    DISP_RC( rc, "col_defs_parse_string:nlt_make_namelist_from_string() failed" );
    if ( rc == 0 )
    {
        uint32_t idx, len = VectorLength( &(defs->cols) );
        size_t prefix_len = 0;
        if ( prefix != 0 )
            prefix_len = string_size( prefix ) + 2;
        for ( idx = 0;  idx < len; ++idx )
        {
            p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( col != NULL )
            {
                if ( col->requested )
                {
                    if ( nlt_is_name_in_namelist( names, col->name ) )
                        col->requested = false;
                    else
                    {
                        if ( prefix != NULL )
                        {
                            size_t len1 = string_size ( col->name ) + prefix_len;
                            char * s = malloc( len1 );
                            if ( s != NULL )
                            {
                                size_t num_writ;
                                rc_t rc1 = string_printf ( s, len1, &num_writ, "%s:%s", prefix, col->name );
                                if ( rc1 == 0 )
                                {
                                    if ( nlt_is_name_in_namelist( names, s ) )
                                        col->requested = false;
                                }
                                free( s );
                            }
                        }
                    }
                }
            }
        }
        KNamelistRelease( names );
    }
    return rc;
}


rc_t col_defs_as_string( col_defs* defs, char ** dst, bool only_requested )
{
    uint32_t idx, count, total = 0, dst_idx = 0;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( dst == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
    *dst = NULL;
    count = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < count; ++idx )
    {
        p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( col != NULL )
        {
            bool use_this = ( only_requested ) ? col->requested : true;
            if ( use_this )
                total += ( string_measure ( col->name, NULL ) + 1 );
        }
    }
    *dst = malloc( total + 1 );
    if ( *dst == NULL )
        return RC( rcVDB, rcNoTarg, rcResolving, rcMemory, rcExhausted );
    *dst[0] = 0;
    for ( idx = 0;  idx < count; ++idx )
    {
        p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( col != NULL )
        {
            bool use_this = ( only_requested ) ? col->requested : true;
            if ( use_this )
            {
                dst_idx += string_copy_measure ( &((*dst)[dst_idx]), total-dst_idx, col->name );
                (*dst)[dst_idx++] = ',';
            }
        }
    }
    if ( dst_idx > 0 )
        (*dst)[dst_idx-1] = 0;
    return 0;
}


rc_t col_defs_apply_casts( col_defs* defs, const matcher * m )
{
    rc_t rc = 0;
    uint32_t idx, count;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );

    count = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < count && rc == 0; ++idx )
    {
        p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( col != NULL )
        {
            if ( m == NULL )
            {
                col->src_cast = string_dup_measure ( col->name, NULL );
                col->dst_cast = string_dup_measure ( col->name, NULL );
            }
            else
            {
                rc = matcher_src_cast( m, col->name, &(col->src_cast) );
                if ( rc == 0 )
                    rc = matcher_dst_cast( m, col->name, &(col->dst_cast) );
            }
        }
    }
    return rc;
}


rc_t col_defs_find_redact_vals( col_defs* defs, 
                                const redact_vals * rvals )
{
    uint32_t idx, count;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( rvals == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    count = VectorLength( &(defs->cols) );
    for ( idx = 0;  idx < count; ++idx )
    {
        p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
        if ( col != NULL )
            if ( col->to_copy )
                col->r_val = redact_vals_get_by_cast( rvals, col->dst_cast );
    }
    return 0;
}


rc_t col_defs_mark_writable_columns( col_defs* defs, VTable *tab, bool show )
{
    KNamelist *writables;
    rc_t rc;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    if ( tab == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );

    rc = VTableListWritableColumns ( tab, &writables );
    if ( rc == 0 )
    {
        uint32_t idx, count = VectorLength( &(defs->cols) );
        for ( idx = 0;  idx < count; ++idx )
        {
            p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( col != NULL )
                if ( col->requested )
                    if ( nlt_is_name_in_namelist( writables, col->name ) )
                    {
                        if ( show )
                            KOutMsg( "writable column: >%s<\n", col->name );
                        col->to_copy = true;
                    }
        }
        KNamelistRelease( writables );
    }
    return rc;
}


rc_t col_defs_mark_requested_columns( col_defs* defs, const char * columns )
{
    const KNamelist *requested;
    rc_t rc;
    uint32_t idx, len;

    if ( defs == NULL )
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );

    if ( columns == NULL )
    {
        /* no specific columns are provided --> mark all of them */
        len = VectorLength( &(defs->cols) );
        for ( idx = 0;  idx < len; ++idx )
        {
            p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
            if ( col != NULL )
                col->requested = true;
        }
        rc = 0;
    }
    else
    {
        /* specific columns are provided --> mark all of these */
        rc = nlt_make_namelist_from_string( &requested, columns );
        DISP_RC( rc, "col_defs_mark_requested_columns:nlt_make_namelist_from_string() failed" );
        if ( rc == 0 )
        {
            uint32_t idx, len = VectorLength( &(defs->cols) );
            for ( idx = 0;  idx < len; ++idx )
            {
                p_col_def col = (p_col_def) VectorGet ( &(defs->cols), idx );
                if ( col != NULL )
                    col->requested = nlt_is_name_in_namelist( requested, col->name );
            }
            KNamelistRelease( requested );
        }
    }
    return rc;
}
