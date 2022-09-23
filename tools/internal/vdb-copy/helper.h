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

#ifndef _h_helper_
#define _h_helper_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_vdb_copy_includes_
#include "vdb-copy-includes.h"
#endif

#ifndef _h_vdb_redactval_
#include "redactval.h"
#endif

#ifndef _h_config_values_
#include "config_values.h"
#endif

int64_t strtoi64( const char* str, char** endp, uint32_t base );
uint64_t strtou64( const char* str, char** endp, uint32_t base );

/*
 * calls the given manager to create a new SRA-schema
 * takes the list of user-supplied schema's (which can be empty)
 * and let the created schema parse all of them
*/
rc_t helper_parse_schema( const VDBManager *my_manager,
                          VSchema **new_schema,
                          const KNamelist *schema_list );

/*
 * tries to interpret the given string in path as a accession
 * and returns the path of the found accession back in path
 * if it is possible, if not the original value of path remains unchanged
 * rc = 0 if the given path is a file-system-path or we are not
 * (weakly) linked against the sra-path-library
rc_t helper_resolve_accession( const KDirectory *my_dir,
                               char ** path );
*/

/*
 * calls VTableTypespec to discover the table-name out of the schema
 * the given table has to be opened!
*/
rc_t helper_get_schema_tab_name( const VTable *my_table, char ** name ) ;


/*
 * reads a string out of a vdb-table:
 * needs a open cursor, a row-idx and the index of the column
 * it opens the row, requests a pointer via VCursorCellData()
 * malloc's the dst-pointer and copies the string into it
 * a 0-termination get's attached
 * finally it closes the row
*/
rc_t helper_read_vdb_string( const VCursor* src_cursor,
                             const uint64_t row_idx,
                             const uint32_t col_idx,
                             char ** dst );


/*
 * reads a int64 out of a vdb-table:
 * needs a open cursor, row-id already set, row opened
 * it copies the data via memmove where to the dst-pointer
 * points to / the size in the vdb-table can be smaller
*/
rc_t helper_read_vdb_int_row_open( const VCursor* src_cursor,
                          const uint32_t col_idx, uint64_t * dst );


/*
 * reads a int64 out of a vdb-table:
 * needs a open cursor, it opens the row,
 * calls helper_read_vdb_int_row_open()
 * finally it closes the row
*/
rc_t helper_read_vdb_int( const VCursor* src_cursor,
                          const uint64_t row_idx,
                          const uint32_t col_idx,
                          uint64_t * dst );


/*
 * reads a string out of a KConfig-object:
 * needs a cfg-object, and the full name of the key(node)
 * it opens the node, discovers the size, malloc's the string,
 * reads the string and closes the node
*/
rc_t helper_read_cfg_str( const KConfig *cfg,
                          const char * key,
                          char ** value );


/*
 * reads a uint64 out of a KConfig-object:
 * needs a cfg-object, and the full name of the key(node)
 * it calls helper_read_cfg_str() to read the node as string
 * and converts it via strtoll() into a uint64
*/
rc_t helper_read_cfg_int( const KConfig *cfg,
                          const char * key,
                          uint64_t * value );


/*
 * tries to detect if the given name of a schema-table is
 * a schema-legacy-table, it needs the vdb-manager to
 * open a sra-schema and request this schema to list all
 * legacy tables, then it searches in that list for the
 * given table-name
*/
rc_t helper_is_tablename_legacy( const VDBManager *my_manager,
            const char * tabname, bool * flag );


/*
 * this function tries to read 3 strings out of cfg-nodes
 * (1) the name of the schema-file to be used to copy to
 * (2) the name of the table to be used inside the schema
 * (3) a list of columns that should not be copied
 * if (3) cannot be found it is not a error
 * it needs as input the cfg-object, the name of the 
 * (src)legacy-platform and the name of the
 * (src)schema-table
*/
rc_t helper_get_legacy_write_schema_from_config( KConfig *cfg,
    const char * platform,
    const char * src_schema,
    char ** legacy_schema_file, char ** legacy_schema_tab,
    char ** legacy_dont_copy );


/*
 * this function creates a config-manager, by passing in
 * a KDirectory-object representing the given path
 * this will include the the given path into the search
 * for *.kfg - files
*/
rc_t helper_make_config_mgr( KConfig **config_mgr, const char * path );


/*
 * this function tries to read the lossynes-score of a vdb-type
 * out of a config-object
 * it constructs a node-name-string like this:
 *      "/VDBCOPY/SCORE/INSDC/2na/bin"
 * INSDC/2na/bin is the name of the type with ":" replaced by "/"
 * it reads the value by calling helper_read_cfg_str()
*/
uint32_t helper_rd_type_score( const KConfig *cfg,
                               const char *type_name );


/*
 * this function tries to remove the given path
*/
rc_t helper_remove_path( KDirectory * directory, const char * path );


/*
 * looks into the path for chars like '/', '\\' or '.'
 * to detect that it is rather a path and not and accession
*/
bool helper_is_this_a_filesystem_path( const char * path );


/*
 * reads global config-values, from the config-manager
*/
void helper_read_redact_values( KConfig * config_mgr,
                                redact_vals * rvals );

/*
 * reads global config-values, if not found via config-manager
 * use the defines from definition.h
*/
void helper_read_config_values( KConfig * config_mgr,
                                p_config_values config );


/*
 * detects if the given table uses md5-checksum's
*/
bool helper_detect_src_md5( const VTable * src_tab );


KCreateMode helper_assemble_CreateMode( const VTable * src_tab, 
                                        bool force_init, uint8_t md5_mode );


KChecksum helper_assemble_ChecksumMode( uint8_t ctx_blob_checksum );

#ifdef __cplusplus
}
#endif

#endif
