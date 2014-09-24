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

#ifndef _h_vdb_coldefs_
#define _h_vdb_coldefs_

#ifndef _h_vdb_copy_includes_
#include "vdb-copy-includes.h"
#endif

#ifndef _h_type_matcher_
#include "type_matcher.h"
#endif

#ifndef _h_vdb_redactval_
#include "redactval.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/********************************************************************
col-def is the definition of a single column: name/index/type
********************************************************************/
typedef struct col_def
{
    char *name;
    uint32_t src_idx;       /* index of this column relative to the
                               read-cursor */

    uint32_t dst_idx;       /* index of this column relative to the
                               write-cursor */

    bool src_valid;         /* this column was successfully addes to
                               the read-cursor */

    bool requested;         /* in case of no columns given to the commandline
                               all existing columns are marked as requested
                               ( even if not all of them can be copied,
                                 they are filtered out later... )
                               if columns are given at the commandline
                               only these are marked as requested */

    bool to_copy;           /* this column has a correspondig column 
                               in the list of writable columns of the
                               destination-schema with writable types */

    bool redactable;        /* this column is in the list of redactable
                               columns */

    VTypedecl type_decl;    /* type-decl of this column via read-schema */
    VTypedesc type_desc;    /* type-desc of this column via read-schema */

    char * src_cast;        /* typecast to be used by the source */
    char * dst_cast;        /* typecast to be used by the destination */

    p_redact_val r_val;     /* pointer to a redact-value entry if exists*/
} col_def;
typedef col_def* p_col_def;

/********************************************************************
the col-defs are a vector of single column-definitions
********************************************************************/
typedef struct col_defs
{
    Vector cols;
    int32_t filter_idx; /* index of READ_FILTER-column (-1 not in set...)*/
} col_defs;
typedef col_defs* p_col_defs;


/*
 * initializes a column-definitions-list
*/
rc_t col_defs_init( col_defs** defs );


/*
 * destroys the column-definitions-list
*/
rc_t col_defs_destroy( col_defs* defs );


/*
 * calls VTableListCol to get a list of all column-names of the table
 * creates a temporary read-cursor to test and get the column-type
 * walks the KNamelist with the available column-names
 * tries to add every one of them to the temp. cursor
 * if the column can be added to the cursor, it is appended
 * to the column-definition list, the type of the column is detected
*/
rc_t col_defs_extract_from_table( col_defs* defs, const VTable *table );


/*
 * walks the list of column-definitions and calls
 * VTableColumnDatatypes() for every column, fills the types
 * into the src-types-Vector of every column
*/
rc_t col_defs_read_src_types( col_defs* defs, const VTable *table,
                              const VSchema *schema );


/*
 * walks the list of column-definitions and calls
 * VTableListWritableColumnTypes() for every column
 * if the returned namelist is not empty, it marks the column as
 * to be copied, and fills it's types into the dst-types-Vector 
 * of every column
*/
rc_t col_defs_read_dst_columns( col_defs* defs, const VTable *table,
                                const VSchema *schema );


rc_t col_defs_clear_to_copy( col_defs* defs );


/*
 * walks the list of column-definitions and reports on every one
*/
rc_t col_defs_report( const col_defs* defs, const bool only_copy_columns );


/*
 * walks the list of column-definitions and tries to add all off them
 * to the given cursor, it stops if one of them fails to be added
 * for every column it detects type_decl and type_desc
 * if both calls are successful the column-def is marked as valid
*/
rc_t col_defs_add_to_rd_cursor( col_defs* defs, const VCursor *cursor, bool show );


/*
 * retrieves a pointer to a column-definition
 * name defines the name of the wanted column-definition
*/
p_col_def col_defs_find( col_defs* defs, const char * name );


/*
 * retrieves the index of a column-definition
 * name defines the name of the wanted column-definition
*/
int32_t col_defs_find_idx( col_defs* defs, const char * name );


/*
 * retrieves a pointer to a column-definition
 * idx defines the Vector-Index of the wanted column-definition
 * !!! not the read or write cursor index !!!
*/
p_col_def col_defs_get( col_defs* defs, const uint32_t idx );


/*
 * adds all columns marked with to_copy == true to the cursor
*/
rc_t col_defs_add_to_wr_cursor( col_defs* defs, const VCursor* cursor, bool show );


/*
 * searches in the column-vector for a successful string-comparison
 * with the hardcoded name of the filter-column
 * stores it's vector-idx in defs->filter_idx
 * does not require an open cursor.
*/
rc_t col_defs_detect_filter_col( col_defs* defs, const char *col_name );


/*
 * walks through the column-names and marks every column thats
 * name is in "redactable_cols"  as redactable
 * sets the redact-value to zero
 * if the default-type of the column is in the list of dna-types
 * (a hardcoded list) it sets the redact-value to 'N'
 * does not require an open cursor. 
*/
rc_t col_defs_detect_redactable_cols_by_name( col_defs* defs,
                    const char * redactable_cols );

rc_t col_defs_detect_redactable_cols_by_type( col_defs* defs,
                        const VSchema * s, const matcher * m,
                        const char * redactable_types );


/*
 * walks through the column-names and clear the redactable-flag 
 * for every column thats name is in "do_not_redact_cols"
 * does not require an open cursor. 
*/
rc_t col_defs_unmark_do_not_redact_columns(  col_defs* defs,
                    const char * do_not_redact_cols );


/*
 * walks through the column-definitions and counts how
 * many of them have the to-copy-flag
 * does not require an open cursor. 
*/
uint32_t col_defs_count_copy_cols( col_defs* defs );


/*
 * walks through the column-definitions and if a column
 * has the to_copy-flag set, and it's name is in the
 * given list of column-names the to copy-flag is cleared
 * does not require an open cursor.
*/
rc_t col_defs_exclude_these_columns( col_defs* defs,
                const char * prefix, const char * column_names );

rc_t col_defs_as_string( col_defs* defs, char ** dst, bool only_requested );

rc_t col_defs_apply_casts( col_defs* defs, const matcher * m );

rc_t col_defs_find_redact_vals( col_defs* defs, const redact_vals * rvals );

rc_t col_defs_mark_writable_columns( col_defs* defs, VTable *tab, bool show );

rc_t col_defs_mark_requested_columns( col_defs* defs, const char * columns );

#ifdef __cplusplus
}
#endif

#endif
