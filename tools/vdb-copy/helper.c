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

#include "helper.h"
#include "definitions.h"
#include <os-native.h>

/* this is here to detect the md5-mode of the src-table */
#include <kdb/kdb-priv.h>
#include <kdb/table.h>

#include <sysalloc.h>

#include <bitstr.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>

int64_t strtoi64( const char* str, char** endp, uint32_t base )
{
    int i = 0;
    int64_t ret_value = 0;

    if ( str != NULL && base != 1 && base <= 36 )
    {
        bool negate = false;

        for ( ; isspace( str [ i ] ); ++ i )
            ( void ) 0;

        switch ( str [ i ] )
        {
        case '-':
            negate = true;
        case '+':
            ++ i;
            break;
        }

        if ( base == 0 )
        {
            if ( str [ i ] != '0' )
                base = 10;
            else if ( tolower ( str [ i + 1 ] == 'x' ) )
            {
                base = 16;
                i += 2;
            }
            else
            {
                base = 8;
                i += 1;
            }
        }

        if ( base <= 10 )
        {
            for ( ; isdigit ( str [ i ] ); ++ i )
            {
                uint32_t digit = str [ i ] - '0';
                if ( digit >= base )
                    break;
                ret_value *= base;
                ret_value += digit;
            }
        }
        else
        {
            for ( ; ; ++ i )
            {
                if ( isdigit ( str [ i ] ) )
                {
                    ret_value *= base;
                    ret_value += str [ i ] - '0';
                }
                else if ( ! isalpha ( str [ i ] ) )
                    break;
                else
                {
                    uint32_t digit = toupper ( str [ i ] ) - 'A' + 10;
                    if ( digit >= base )
                        break;
                    ret_value *= base;
                    ret_value += digit;
                }
            }
        }

        if ( negate )
            ret_value = - ret_value;
    }

    if ( endp != NULL )
        * endp = (char *)str + i;

    return ret_value;
}


uint64_t strtou64( const char* str, char** endp, uint32_t base )
{
    return strtoi64( str, endp, base );
}


/*
 * calls the given manager to create a new SRA-schema
 * takes the list of user-supplied schema's (which can be empty)
 * and let the created schema parse all of them
*/
rc_t helper_parse_schema( const VDBManager *my_manager,
                          VSchema **new_schema,
                          const KNamelist *schema_list )
{
    rc_t rc;
    if ( my_manager == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( new_schema == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );

    rc = VDBManagerMakeSRASchema( my_manager, new_schema );
    DISP_RC( rc, "VDBManagerMakeSRASchema() failed" );

    if ( ( rc == 0 )&&( schema_list != NULL ) )
    {
        uint32_t count;
        if ( KNamelistCount( schema_list, &count ) == 0 )
        {
            uint32_t idx;
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char *s;
                if ( KNamelistGet( schema_list, idx, &s ) == 0 )
                {
                    rc = VSchemaParseFile( *new_schema, "%s", s );
                    DISP_RC( rc, "VSchemaParseFile() failed" );
                }
            }
        }
    }
    return rc;
}


#if TOOLS_USE_SRAPATH != 0
static char *translate_accession( SRAPath *my_sra_path,
                           const char *accession,
                           const size_t bufsize )
{
    rc_t rc;
    char * res = calloc( 1, bufsize );
    if ( res == NULL ) return NULL;

    rc = SRAPathFind( my_sra_path, accession, res, bufsize );
    if ( GetRCState( rc ) == rcNotFound )
    {
        free( res );
        return NULL;
    }
    else if ( GetRCState( rc ) == rcInsufficient )
    {
        free( res );
        return translate_accession( my_sra_path, accession, bufsize * 2 );
    }
    else if ( rc != 0 )
    {
        free( res );
        return NULL;
    }
    return res;
}
#endif


#if TOOLS_USE_SRAPATH != 0
rc_t helper_resolve_accession( const KDirectory *my_dir, char ** path )
{
    SRAPath *my_sra_path;
    rc_t rc = 0;

    if ( strchr ( *path, '/' ) != NULL )
        return 0;

    rc = SRAPathMake( &my_sra_path, my_dir );
    if ( rc != 0 )
    {
        if ( GetRCState ( rc ) != rcNotFound || GetRCTarget ( rc ) != rcDylib )
            DISP_RC( rc, "SRAPathMake() failed" );
        else
            rc = 0;
    }
    else
    {
        if ( !SRAPathTest( my_sra_path, *path ) )
        {
            char *buf = translate_accession( my_sra_path, *path, 64 );
            if ( buf != NULL )
            {
                free( (char*)(*path) );
                *path = buf;
            }
        }
        SRAPathRelease( my_sra_path );
    }
    return rc;
}
#endif


/*
 * calls VTableTypespec to discover the table-name out of the schema
 * the given table has to be opened!
*/
#define MAX_SCHEMA_TABNAME_LENGHT 80
rc_t helper_get_schema_tab_name( const VTable *my_table, char ** name )
{
    rc_t rc;
    char *s;
    
    if ( my_table == NULL || name == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );

    *name = NULL;
    s = malloc( MAX_SCHEMA_TABNAME_LENGHT + 1 );
    if ( s == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );

    rc = VTableTypespec ( my_table, s, MAX_SCHEMA_TABNAME_LENGHT );
    if ( rc == 0 )
        *name = s;
    else
        free( s );

    return rc;
}


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
                             char ** dst )
{
    rc_t rc;

    if ( dst == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );

    *dst = NULL;
    rc = VCursorSetRowId( src_cursor, row_idx );
    DISP_RC( rc, "helper_read_string:VCursorSetRowId() failed" );
    if ( rc == 0 )
    {
        rc = VCursorOpenRow( src_cursor );
        DISP_RC( rc, "helper_read_string:VCursorOpenRow() failed" );
        if ( rc == 0 )
        {
            const void *src_buffer;
            uint32_t offset_in_bits;
            uint32_t element_bits;
            uint32_t element_count;

            rc = VCursorCellData( src_cursor, col_idx, &element_bits,
                  &src_buffer, &offset_in_bits, &element_count );
            DISP_RC( rc, "helper_read_string:VCursorCellData() failed" );
            if ( rc == 0 )
            {
                char *src_ptr = (char*)src_buffer + ( offset_in_bits >> 3 );
                *dst = malloc( element_count + 1 );
                if ( *dst != NULL )
                {
                    memcpy( *dst, src_ptr, element_count );
                    (*dst)[ element_count ] = 0;
                }
                else
                    rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            }
            VCursorCloseRow( src_cursor );
            DISP_RC( rc, "helper_read_string:VCursorClose() failed" );
        }
    }
    return rc;
}

uint8_t BitLength2Bytes[65] =
{
         /* 0  1  2  3  4  5  6  7  8  9*/
   /* 0 */  0, 1, 1, 1, 1, 1, 1, 1, 1, 2,
   /* 1 */  2, 2, 2, 2, 2, 2, 2, 3, 3, 3,
   /* 2 */  3, 3, 3, 3, 3, 4, 4, 4, 4, 4,
   /* 3 */  4, 4, 4, 5, 5, 5, 5, 5, 5, 5,
   /* 4 */  5, 6, 6, 6, 6, 6, 6, 6, 6, 7,
   /* 5 */  7, 7, 7, 7, 7, 7, 7, 8, 8, 8,
   /* 6 */  8, 8, 8, 8, 8
};

/*************************************************************************************
n_bits   [IN] ... number of bits

calculates the number of bytes that have to be copied to contain the given
number of bits
*************************************************************************************/
static uint16_t bitlength_2_bytes( const size_t n_bits )
{
    if ( n_bits > 64 )
        return 8;
    else
        return BitLength2Bytes[ n_bits ];
}


/*
 * reads a int64 out of a vdb-table:
 * needs a open cursor, row-id already set, row opened
 * it copies the data via memmove where to the dst-pointer
 * points to / the size in the vdb-table can be smaller
*/
rc_t helper_read_vdb_int_row_open( const VCursor* src_cursor,
                          const uint32_t col_idx, uint64_t * dst )
{
    const void *src_buffer;
    uint32_t offset_in_bits;
    uint32_t element_bits;
    uint32_t element_count;

    rc_t rc = VCursorCellData( src_cursor, col_idx, &element_bits,
                  &src_buffer, &offset_in_bits, &element_count );
    DISP_RC( rc, "helper_read_int_intern:VCursorCellData() failed" );
    if ( rc == 0 )
    {
        uint64_t value = 0;
        char *src_ptr = (char*)src_buffer + ( offset_in_bits >> 3 );
        if ( ( offset_in_bits & 7 ) == 0 )
            memmove( &value, src_ptr, bitlength_2_bytes( element_bits ) );
        else
            bitcpy ( &value, 0, src_ptr, offset_in_bits, element_bits );
        *dst = value;
    }
    return rc;
}


/*
 * reads a int64 out of a vdb-table:
 * needs a open cursor, it opens the row,
 * calls helper_read_vdb_int_row_open()
 * finally it closes the row
*/
rc_t helper_read_vdb_int( const VCursor* src_cursor,
                          const uint64_t row_idx,
                          const uint32_t col_idx,
                          uint64_t * dst )
{
    rc_t rc;

    if ( src_cursor == NULL || dst == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );

    *dst = 0;
    rc = VCursorSetRowId( src_cursor, row_idx );
    DISP_RC( rc, "helper_read_int:VCursorSetRowId() failed" );
    if ( rc == 0 )
    {
        rc = VCursorOpenRow( src_cursor );
        DISP_RC( rc, "helper_read_int:VCursorOpenRow() failed" );
        if ( rc == 0 )
        {
            rc = helper_read_vdb_int_row_open( src_cursor, col_idx, dst );
            VCursorCloseRow( src_cursor );
            DISP_RC( rc, "helper_read_int:VCursorClose() failed" );
        }
    }
    return rc;
}


/*
 * reads a string out of a KConfig-object:
 * needs a cfg-object, and the full name of the key(node)
 * it opens the node, discovers the size, malloc's the string,
 * reads the string and closes the node
*/
rc_t helper_read_cfg_str( const KConfig *cfg, const char * key,
                          char ** value )
{
    const KConfigNode *node;
    rc_t rc;
    
    if ( value == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
 
    *value = NULL;
    rc = KConfigOpenNodeRead ( cfg, &node, "%s", key );
    /* it is OK if we do not find it, so no DISP_RC here */
    if ( rc == 0 )
    {
        size_t num_read, remaining;
        /* first we ask about the size to be read */
        rc = KConfigNodeRead ( node, 0, NULL, 0, &num_read, &remaining );
        DISP_RC( rc, "helper_read_config_str:KConfigNodeRead(1) failed" );
        if ( rc == 0 )
        {
            *value = malloc( remaining + 1 );
            if ( *value )
            {
                size_t to_read = remaining;
                rc = KConfigNodeRead ( node, 0, *value, to_read, &num_read, &remaining );
                DISP_RC( rc, "helper_read_config_str:KConfigNodeRead(2) failed" );
                if ( rc == 0 )
                    (*value)[ num_read ] = 0;
            }
            else
                rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        KConfigNodeRelease( node );
    }
    return rc;
}


/*
 * reads a uint64 out of a KConfig-object:
 * needs a cfg-object, and the full name of the key(node)
 * it calls helper_read_cfg_str() to read the node as string
 * and converts it via strtou64() into a uint64
*/
rc_t helper_read_cfg_int( const KConfig *cfg, const char * key,
                          uint64_t * value )
{
    char *str_value;
    rc_t rc;

    if ( value == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    *value = 0;
    
    rc = helper_read_cfg_str( cfg, key, &str_value );
    DISP_RC( rc, "helper_read_config_int:helper_read_config_str() failed" );
    if ( rc == 0 )
    {
        *value = strtou64( str_value, NULL, 0 );
        free( str_value );
    }
    return rc;
}


/*
 * tries to detect if the given name of a schema-table is
 * a schema-legacy-table, it needs the vdb-manager to
 * open a sra-schema and request this schema to list all
 * legacy tables, then it searches in that list for the
 * given table-name
*/
rc_t helper_is_tablename_legacy( const VDBManager *my_manager,
            const char * tabname, bool * flag )
{
    VSchema *schema;
    rc_t rc;

    if ( flag == NULL )
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    *flag = false;

    rc = VDBManagerMakeSRASchema ( my_manager, &schema );
    DISP_RC( rc, "helper_is_tablename_legacy:VDBManagerMakeSRASchema() failed" );
    if ( rc == 0 )
    {
        KNamelist *list;
        rc = VSchemaListLegacyTables ( schema, &list );
        DISP_RC( rc, "helper_is_tablename_legacy:VSchemaListLegacyTables() failed" );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = KNamelistCount ( list, &count );
            DISP_RC( rc, "helper_is_tablename_legacy:KNamelistCoun() failed" );
            if ( rc == 0 )
            {
                uint32_t idx = 0;
                size_t tabname_len = string_size ( tabname );
                while( idx < count && rc == 0 && *flag == false )
                {
                    const char * name;
                    rc = KNamelistGet ( list, idx, &name );
                    if ( rc == 0 )
                    {
                        size_t len = string_size ( name );
                        if ( string_cmp ( name, len, tabname, tabname_len,
                             ( len < tabname_len ) ? len : tabname_len ) == 0 )
                            *flag = true;
                    }
                    idx++;
                }
            }
            KNamelistRelease( list );
        }
        VSchemaRelease( schema );
    }
    return rc;
}


/*
 * helper-function to replace all ':' and '#' chars
 * with '_' chars
*/
static void helper_process_c_str( char * s )
{
    size_t idx, len = string_measure ( s, NULL );
    for ( idx = 0; idx < len; ++idx )
    {
        if ( s[ idx ] == ':' || s[ idx ] == '#' )
            s[ idx ] = '_';
    }
}


/*
 * helper-function to replace all ':' and '#' chars
 * with '_' chars
*/
static void helper_process_cfg_key( String *s )
{
    uint32_t idx;
    for ( idx = 0; idx < s->len; ++idx )
    {
        if ( s->addr[ idx ] == ':' || s->addr[ idx ] == '#' )
            ( (char *)(s->addr) )[ idx ] = '_';
    }
}


/*
 * helper-function to read the value of cfg-node
 * if key_sel = xxx and s_sub = yyy
 * the node-name is formed like that "/VDBCOPY/xxx/yyy"
 * to read the value helper_read_cfg_str() is called
*/
static rc_t helper_get_legacy_node( KConfig *cfg,
        String *key_sel, const char * s_sub, char ** dst )
{
    String key_prefix, key_sub;
    const String *temp;
    const String *key;
    rc_t rc;

    StringInitCString( &key_prefix, VDB_COPY_PREFIX );
    rc = StringConcat ( &temp, &key_prefix, key_sel );
    if ( rc != 0 ) return rc;
    
    StringInitCString( &key_sub, s_sub );
    rc = StringConcat ( &key, temp, &key_sub );
    StringWhack ( temp );
    if ( rc != 0 ) return rc;
    
    rc = helper_read_cfg_str( cfg, key->addr, dst );
    StringWhack ( key );
    return rc;
}


/*
 * helper-function to read the value of a cfg-node
 * if pf = "_454_", schema = "NCBI_SRA__454__tbl_v0_1"
 * and postfix = "tab" it first tries to read:
 *      "/VDBCOPY/NCBI_SRA__454__tbl_v0_1/tab"
 * if this node is not found it tries to read:
 *      "/VDBCOPY/_454_/tab"
 * if this node is not found it is a error
*/
static rc_t helper_get_legacy_value( KConfig *cfg,
        String *pf, String *schema, const char * postfix, char ** dst )
{
    /* first try to find the value under the src-schema-key */
    rc_t rc = helper_get_legacy_node( cfg, schema, postfix, dst );
    /* if this fails try to find it mapped to the platform */
    if ( rc != 0 )
        rc = helper_get_legacy_node( cfg, pf, postfix, dst );
    return rc;
}


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
    char ** legacy_dont_copy )
{
    rc_t rc;
    String key_pf, key_schema;

    StringInitCString( &key_pf, platform );
    helper_process_cfg_key( &key_pf );
    StringInitCString( &key_schema, src_schema );
    helper_process_cfg_key( &key_pf );

    rc = helper_get_legacy_value( cfg, &key_pf, &key_schema, 
                LEGACY_SCHEMA_KEY, legacy_schema_file );
    if ( rc != 0 ) return rc;
    
    rc = helper_get_legacy_value( cfg, &key_pf, &key_schema,
                LEGACY_TAB_KEY, legacy_schema_tab );
    if ( rc != 0 ) return rc;

    /* dont_use is optional, no error if not found! */
    helper_get_legacy_value( cfg, &key_pf, &key_schema,
                LEGACY_EXCLUDE_KEY, legacy_dont_copy );
    return 0;
}


/*
 * this function creates a config-manager, by passing in
 * a KDirectory-object representing the given path
 * this will include the the given path into the search
 * for *.kfg - files
*/
rc_t helper_make_config_mgr( KConfig **config_mgr, const char * path )
{
#if ALLOW_EXTERNAL_CONFIG
    KDirectory *directory;
    const KDirectory *config_sub_dir;

    rc_t rc = KDirectoryNativeDir( &directory );
    if ( rc != 0 ) return rc;
    rc = KDirectoryOpenDirRead ( directory, &config_sub_dir, false, "%s", path );
    if ( rc == 0 )
    {
        rc = KConfigMake ( config_mgr, config_sub_dir );
        KDirectoryRelease( config_sub_dir );
    }
    KDirectoryRelease( directory );
#else
    rc_t rc = KConfigMake ( config_mgr, NULL );
#endif
    return rc;
}


/*
 * this function tries to read the lossynes-score of a vdb-type
 * out of a config-object
 * it constructs a node-name-string like this:
 *      "/VDBCOPY/SCORE/INSDC_2na_bin"
 * INSDC/2na/bin is the name of the type with ":" replaced by "_"
 * it reads the value by calling helper_read_cfg_str()
*/
uint32_t helper_rd_type_score( const KConfig *cfg, 
                               const char *type_name )
{
    uint32_t res = 0;
    if ( cfg != NULL && type_name != NULL && type_name[0] != 0 )
    {
        uint32_t prefix_len = string_measure ( TYPE_SCORE_PREFIX, NULL ) + 1;
        uint32_t len = string_measure ( type_name, NULL ) + prefix_len;
        char *s = malloc( len );
        if ( s != NULL )
        {
            char *value;
            size_t n = string_copy_measure ( s, len, TYPE_SCORE_PREFIX );
            string_copy_measure( &(s[n]), len, type_name );
            helper_process_c_str( s );
            if ( helper_read_cfg_str( cfg, s, &value ) == 0 )
            {
                if ( value != NULL )
                {
                    res = strtol( value, NULL, 0 );
                    free( value );
                }
            }
            free( s );
        }
    }
    return res;
}


/* remove the target-table if something went wrong */
rc_t helper_remove_path( KDirectory * directory, const char * path )
{
    rc_t rc;

    PLOGMSG( klogInfo, ( klogInfo, "removing '$(path)'", "path=%s", path ));
    rc = KDirectoryRemove ( directory, true, "%s", path );
    DISP_RC( rc, "vdb_copy_remove_table:KDirectoryRemove() failed" );
    return rc;
}


bool helper_is_this_a_filesystem_path( const char * path )
{
    bool res = false;
    size_t i, n = string_size ( path );
    for ( i = 0; i < n && !res; ++i )
    {
        char c = path[ i ];
        res = ( c == '.' || c == '/' || c == '\\' );
    }
    return res;
}


static void helper_read_redact_value( KConfig * config_mgr, redact_vals * rvals,
                               const char * type_name )
{
    rc_t rc;
    String key_prefix, key_type, key_postfix;
    const String * p1;

    StringInitCString( &key_prefix, REDACTVALUE_PREFIX );
    StringInitCString( &key_type, type_name );
    StringInitCString( &key_postfix, REDACTVALUE_VALUE_POSTFIX );
    rc = StringConcat ( &p1, &key_prefix, &key_type );
    if ( rc == 0 )
    {
        const String * key;
        rc = StringConcat ( &key, p1, &key_postfix );
        if ( rc == 0 )
        {
            char * value_str;
            rc = helper_read_cfg_str( config_mgr, key->addr, &value_str );
            StringWhack ( key );
            if ( rc == 0 )
            {
                StringInitCString( &key_postfix, REDACTVALUE_LEN_POSTFIX );
                rc = StringConcat ( &key, p1, &key_postfix );
                if ( rc == 0 )
                {
                    uint32_t idx, l, len = 1;
                    char * endp, * len_str, * real_type;

                    rc = helper_read_cfg_str( config_mgr, key->addr, &len_str );
                    StringWhack ( key );
                    if ( rc == 0 )
                    {
                        len = strtol( len_str, &endp, 0 );
                        free( len_str );
                    }

                    real_type = string_dup_measure ( type_name, NULL );
                    l = string_size ( real_type );
                        
                    /* transform the typename back into '_' ---> ':' real typename */
                    for ( idx = 0; idx < l; ++idx )
                        if ( real_type[idx] == '_' )
                            real_type[idx] = ':';

                    /* finally insert the redact-value-pair into the list */
                    redact_vals_add( rvals, real_type, len, value_str );
                    free( real_type );
                }
                free( value_str );
            }
        }
        StringWhack ( p1 );
    }
}


void helper_read_redact_values( KConfig * config_mgr, redact_vals * rvals )
{
    rc_t rc;
    char * type_list_str;
    const KNamelist *type_list;

    /* first we read the list of redactable types */
    helper_read_cfg_str( config_mgr, 
            REDACTABLE_LIST_KEY, &type_list_str );
    if ( type_list_str == NULL ) return;
    rc = nlt_make_namelist_from_string( &type_list, type_list_str );
    if ( rc == 0 && type_list != NULL )
    {
        uint32_t idx, count;
        rc = KNamelistCount( type_list, &count );
        if ( rc == 0 && count > 0 )
            for ( idx = 0; idx < count; ++idx )
            {
                const char *type_name;
                rc = KNamelistGet( type_list, idx, &type_name );
                if ( rc == 0 )
                    helper_read_redact_value( config_mgr, rvals, type_name );
            }
        KNamelistRelease( type_list );
    }
    free( type_list_str );
}


void helper_read_config_values( KConfig * config_mgr, p_config_values config )
{
    /* key's and default-values in definitions.h */

    /* look for the name of the filter-column */
    helper_read_cfg_str( config_mgr, 
            READ_FILTER_COL_NAME_KEY, &(config->filter_col_name) );
    if ( config->filter_col_name == NULL )
        config->filter_col_name = string_dup_measure ( READ_FILTER_COL_NAME, NULL );

    /* look for the comma-separated list of meta-nodes to be ignored */
    helper_read_cfg_str( config_mgr, 
            META_IGNORE_NODES_KEY, &(config->meta_ignore_nodes) );
    if ( config->meta_ignore_nodes == NULL )
        config->meta_ignore_nodes = string_dup_measure ( META_IGNROE_NODES_DFLT, NULL );

    /* look for the comma-separated list of redactable types */
    helper_read_cfg_str( config_mgr, 
            REDACTABLE_TYPES_KEY, &(config->redactable_types) );
    if ( config->redactable_types == NULL )
        config->redactable_types = string_dup_measure ( REDACTABLE_TYPES, NULL );

    /* look for the comma-separated list of columns which are protected from redaction */
    helper_read_cfg_str( config_mgr, 
            DO_NOT_REDACT_KEY, &(config->do_not_redact_columns) );
}


bool helper_detect_src_md5( const VTable * src_tab )
{
    rc_t rc;
    const struct KTable * ktab;
    bool res = false;

    rc = VTableOpenKTableRead ( src_tab, &ktab );
    if ( rc == 0 )
    {
        const struct KDirectory *dir;
        rc = KTableOpenDirectoryRead ( ktab, &dir );
        if ( rc == 0 )
        {
            /* ask for pathtype of "md5" */
            uint32_t pt = KDirectoryPathType ( dir, "md5" );
            if ( ( pt == kptFile )||( pt == ( kptFile | kptAlias ) ) )
                res = true;
            KDirectoryRelease( dir );
        }
        KTableRelease( ktab );
    }
    return res;
}


KCreateMode helper_assemble_CreateMode( const VTable * src_tab, 
                                        bool force_init, uint8_t md5_mode )
{
    KCreateMode res = kcmParents;
 
    if ( md5_mode == MD5_MODE_AUTO )
    {
        bool src_md5_mode = helper_detect_src_md5( src_tab );
        md5_mode = ( src_md5_mode ? MD5_MODE_ON : MD5_MODE_OFF );
    }

    if ( force_init )
        res |= kcmInit;
    else
        res |= kcmCreate;

    switch( md5_mode )
    {
    case MD5_MODE_ON :
    case MD5_MODE_AUTO : res |= kcmMD5; break;
    }
    return res;
}


KChecksum helper_assemble_ChecksumMode( uint8_t ctx_blob_checksum )
{
    KChecksum res = kcsCRC32; /* from interfaces/kdb/column.h */
    switch( ctx_blob_checksum )
    {
        case BLOB_CHECKSUM_OFF   : res = kcsNone; break;
        case BLOB_CHECKSUM_CRC32 : res = kcsCRC32; break;
        case BLOB_CHECKSUM_MD5   : res = kcsMD5; break;
        case BLOB_CHECKSUM_AUTO  : res = kcsCRC32; break;
    }
    return res;
}
