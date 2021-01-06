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

#include <klib/log.h>
#include <klib/rc.h>
#include <klib/text.h>
#include <klib/printf.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/cacheteefile.h>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <os-native.h>
#include <sysalloc.h>
#include "vdb-dump-helper.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>

rc_t ErrMsg( const char * fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( 0 == rc )
    {
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    }
    va_end( list );
    return rc;
} 

/********************************************************************
helper function to display the version of the vdb-manager
********************************************************************/
rc_t vdh_show_manager_version( const VDBManager *mgr )
{
    uint32_t version;
    rc_t rc = VDBManagerVersion( mgr, &version );
    DISP_RC( rc, "VDBManagerVersion() failed" );
    if ( 0 == rc  )
    {
        PLOGMSG ( klogInfo, ( klogInfo, "manager-version = $(maj).$(min).$(rel)",
                              "vers=0x%X,maj=%u,min=%u,rel=%u",
                              version,
                              version >> 24,
                              ( version >> 16 ) & 0xFF,
                              version & 0xFFFF ));
    }
    return rc;
}

static void CC vdh_parse_1_schema( void *item, void *data )
{
    char *s = ( char* )item;
    VSchema *schema = ( VSchema* )data;
    if ( ( NULL != item ) && ( NULL != schema ) )
    {
        rc_t rc = VSchemaParseFile( schema, "%s", s );
        DISP_RC( rc, "VSchemaParseFile() failed" );
    }
}

rc_t vdh_parse_schema( const VDBManager *mgr, VSchema **new_schema,
                        Vector *schema_list, bool with_sra_schema )
{
    rc_t rc = 0;

    if ( NULL == mgr )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }

    if ( NULL == new_schema )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }

    *new_schema = NULL;
    
    if ( with_sra_schema )
    {
        rc = VDBManagerMakeSRASchema( mgr, new_schema );
        DISP_RC( rc, "VDBManagerMakeSRASchema() failed" );
    }
    
    if ( ( 0 == rc )&&( NULL != schema_list ) )
    {
        if ( NULL == *new_schema )
        {
            rc = VDBManagerMakeSchema( mgr, new_schema );
            DISP_RC( rc, "VDBManagerMakeSchema() failed" );
        }
        if ( 0 == rc )
        {
            VectorForEach( schema_list, false, vdh_parse_1_schema, *new_schema );
        }
    }
    return rc;
}

/********************************************************************
helper function to test if a given path is a vdb-table
********************************************************************/
bool vdh_is_path_table( const VDBManager *mgr, const char *path, Vector *schema_list )
{
    bool res = false;
    const VTable *tbl;
    VSchema *schema = NULL;
    rc_t rc;
    
    vdh_parse_schema( mgr, &schema, schema_list, false );

    rc = VDBManagerOpenTableRead( mgr, &tbl, schema, "%s", path );
    if ( 0 == rc )
    {
        res = true; /* yes we are able to open the table ---> path is a table */
        rc = VTableRelease( tbl );
        DISP_RC( rc, "VTableRelease() failed" );
    }

    if ( NULL != schema )
    {
        rc = VSchemaRelease( schema );
        DISP_RC( rc, "VSchemaRelease() failed" );
    }
    return res;
}


const char * backback = "/../..";

/********************************************************************
helper function to test if a given path is a vdb-column
by testing if the parent/parent dir is a vdb-table
********************************************************************/
bool vdh_is_path_column( const VDBManager *mgr, const char *path, Vector *schema_list )
{
    bool res = false;
    size_t path_len = string_size( path );
    char *pp_path = malloc( path_len + 20 );
    if ( NULL != pp_path )
    {
        char *resolved = malloc( 1024 );
        if ( NULL != resolved )
        {
            KDirectory *dir;
            rc_t rc = KDirectoryNativeDir( &dir );
            DISP_RC( rc, "KDirectoryNativeDir() failed" );
            if ( 0 == rc )
            {
                string_copy( pp_path, path_len + 20, path, path_len );
                string_copy( &pp_path[ path_len ], 20, backback, string_size( backback ) );
                rc = KDirectoryResolvePath( dir, true, resolved, 1023, "%s", pp_path );
                DISP_RC( rc, "KDirectoryResolvePath() failed" );
                if ( 0 == rc )
                {
                    res = vdh_is_path_table( mgr, resolved, schema_list );
                }
            }
            free( resolved );
        }
        free( pp_path );
    }
    return res;
}

/********************************************************************
helper function to test if a given path is a vdb-database
********************************************************************/
bool vdh_is_path_database( const VDBManager *mgr, const char *path, Vector *schema_list )
{
    bool res = false;
    const VDatabase *db;
    VSchema *schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, schema_list, false );

    rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", path );
    if ( 0 == rc )
    {
        res = true; /* yes we are able to open the database ---> path is a database */
        rc = VDatabaseRelease( db );
        DISP_RC( rc, "VDatabaseRelease() failed" );
    }

    if ( schema != NULL )
    {
        rc = VSchemaRelease( schema );
        DISP_RC( rc, "VSchemaRelease() failed" );
    }
    return res;
}

/*************************************************************************************
helper-function to extract the name of the first table of a database
and put it into the dump-context
*************************************************************************************/
bool vdh_take_1st_table_from_db( dump_context *ctx, const KNamelist * tbl_names )
{
    bool we_found_a_table = false;
    uint32_t count;
    rc_t rc = KNamelistCount( tbl_names, &count );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc && count > 0 )
    {
        const char *tbl_name;
        rc = KNamelistGet( tbl_names, 0, &tbl_name );
        DISP_RC( rc, "KNamelistGet() failed" );
        if ( 0 == rc )
        {
            vdco_set_table( ctx, tbl_name );
            we_found_a_table = true;
        }
    }
    return we_found_a_table;
}

static bool vdh_str_starts_with( const char *a, const char *b )
{
    bool res = false;
    size_t asize = string_size ( a );
    size_t bsize = string_size ( b );
    if ( asize >= bsize )
    {
        res = ( 0 == strcase_cmp ( a, bsize, b, bsize, (uint32_t)bsize ) );
    }
    return res;
}

bool list_contains_value( const KNamelist * list, const String * value )
{
    bool found = false;
    uint32_t count;
    rc_t rc = KNamelistCount( list, &count );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc && count > 0 )
    {
        uint32_t i;
        for ( i = 0; i < count && 0 == rc && !found; ++i )
        {
            const char *s;
            rc = KNamelistGet( list, i, &s );
            if ( 0 != rc )
            {
                ErrMsg( "KNamelistGet( %d ) -> %R", i, rc );
            }
            else
            {
                String item;
                StringInitCString( &item, s );
                found = ( 0 == StringCompare ( &item, value ) );
            }
        }
    }
    return found;
}


static bool list_contains_value_starting_with( const KNamelist * list, const String * value, String * found )
{
    bool res = false;
    uint32_t count;
    rc_t rc = KNamelistCount( list, &count );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc && count > 0 )
    {
        uint32_t i;
        for ( i = 0; i < count && 0 == rc && !res; ++i )
        {
            const char *s;
            rc = KNamelistGet( list, i, &s );
            if ( 0 != rc )
            {
                ErrMsg( "KNamelistGet( %d ) -> %R", i, rc );
            }
            else
            {
                String item;
                StringInitCString( &item, s );
                if ( value->len <= item.len )
                {
                    item.len = value->len;
                    item.size = value->size;
                    res = ( 0 == StringCompare ( &item, value ) );
                    if ( res )
                    {
                        StringInitCString( found, s );
                    }
                }
            }
        }
    }
    return res;
}

/*************************************************************************************
helper-function to check if a given table is in the list of tables
if found put that name into the dump-context
*************************************************************************************/
bool vdh_take_this_table_from_list( dump_context *ctx, const KNamelist * tbl_names,
                                    const char * table_to_find )
{
    bool res;
    String to_find;

    StringInitCString( &to_find, table_to_find );
    res = list_contains_value( tbl_names, &to_find );
    if ( res )
    {
        vdco_set_table_String( ctx, &to_find );
    }
    else
    {
        String found;
        res = list_contains_value_starting_with( tbl_names, &to_find, &found );
        if ( res )
        {
            vdco_set_table_String( ctx, &found );
        }
    }
    return res;
}

bool vdh_take_this_table_from_db( dump_context *ctx, const VDatabase * db,
                                  const char * table_to_find )
{
    bool we_found_the_table = false;
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( db, &tbl_names );
    DISP_RC( rc, "VDatabaseListTbl() failed" );
    if ( 0 == rc )
    {
        we_found_the_table = vdh_take_this_table_from_list( ctx, tbl_names, table_to_find );
        rc = KNamelistRelease( tbl_names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    return we_found_the_table;
}

const char * s_unknown_tab = "unknown table";

static rc_t vdh_print_full_col_info( dump_context *ctx,
                                     const p_col_def col_def,
                                     const VSchema *schema )
{
    rc_t rc = 0;
    char * s_domain = vdcd_make_domain_txt( col_def -> type_desc . domain );
    if ( NULL != s_domain )
    {
        if ( NULL == ctx -> table )
        {
            ctx -> table = string_dup_measure( s_unknown_tab, NULL ); /* will be free'd when ctx get free'd */
        }

        if ( ( NULL != ctx -> table ) && ( NULL != col_def -> name ) )
        {
            rc = KOutMsg( "%s.%.02d : (%.3d bits [%.02d], %8s)  %s",
                    ctx -> table,
                    ctx -> generic_idx++,
                    col_def -> type_desc . intrinsic_bits,
                    col_def -> type_desc . intrinsic_dim,
                    s_domain,
                    col_def -> name );
            if ( 0 == rc && NULL != schema )
            {
                char buf[ 64 ];
                rc = VTypedeclToText( &( col_def -> type_decl ), schema, buf, sizeof( buf ) );
                DISP_RC( rc, "VTypedeclToText() failed" );
                if ( 0 == rc )
                {
                    rc = KOutMsg( "\n      (%s)", buf );
                }
            }
            if ( 0 == rc )
            {
                rc = KOutMsg( "\n" );
            }
        }
        else
        {
            if ( NULL == ctx -> table )
            {
                rc = KOutMsg( "error: no table-name in print_column_info()" );
            }
            if ( NULL == col_def -> name )
            {
                rc = KOutMsg( "error: no column-name in print_column_info()" );
            }
        }
        free( s_domain );
    }
    else
    {
        rc = KOutMsg( "error: making domain-text in print_column_info()" );
    }
    return rc;
}

static rc_t vdh_print_short_col_info( const p_col_def col_def, const VSchema *schema )
{
    rc_t rc = 0;
    if ( NULL != col_def -> name )
    {
        rc = KOutMsg( "%s", col_def -> name );
        if ( 0 == rc && NULL != schema )
        {
            char buf[ 64 ];
            rc = VTypedeclToText( &( col_def -> type_decl ), schema, buf, sizeof( buf ) );
            DISP_RC( rc, "VTypedeclToText() failed" );
            if ( 0 == rc )
            {
                rc = KOutMsg( " (%s)", buf );
            }
        }
        if ( 0 == rc )
        {
            rc = KOutMsg( "\n" );
        }
    }
    return rc;
}

rc_t vdh_print_col_info( dump_context *ctx, const p_col_def col_def, const VSchema *schema )
{
    rc_t rc = 0;
    if ( NULL != ctx && NULL != col_def )
    {
        if ( ctx -> column_enum_requested )
        {
            rc = vdh_print_full_col_info( ctx, col_def, schema );
        }
        else
        {
            rc = vdh_print_short_col_info( col_def, schema );
        }
    }
    return rc;
}

rc_t resolve_remote_accession( const char * accession, char * dst, size_t dst_size )
{
    VFSManager * vfs_mgr;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    dst[ 0 ] = 0;
    DISP_RC( rc, "VFSManagerMake() failed" );    
    if ( 0 == rc )
    {
        VResolver * resolver;
        rc = VFSManagerGetResolver( vfs_mgr, &resolver );
        DISP_RC( rc, "VFSManagerGetResolver() failed" );
        if ( 0 == rc )
        {
            VPath * vpath;
            rc = VFSManagerMakePath( vfs_mgr, &vpath, "ncbi-acc:%s", accession );
            DISP_RC( rc, "VFSManagerMakePath() failed" );
            if ( 0 == rc )
            {
                const VPath * remote = NULL;
                VResolverRemoteEnable( resolver, vrAlwaysEnable );
                rc = VResolverQuery ( resolver, 0, vpath, NULL, &remote, NULL );
                if ( rc == 0 &&  remote != NULL )
                {
                    const String * path;
                    rc = VPathMakeString( remote, &path );
                    if ( 0 == rc && NULL != path )
                    {
                        string_copy ( dst, dst_size, path -> addr, path -> size );
                        dst[ path -> size ] = 0;
                        StringWhack( path );
                    }
                    if ( NULL != remote )
                    {
                        rc_t rc2 = VPathRelease ( remote );
                        DISP_RC( rc2, "VPathRelease() failed" );
                        rc = ( 0 == rc ) ? rc2 : rc;
                    }
                }
                {
                    rc_t rc2 = VPathRelease ( vpath );
                    DISP_RC( rc2, "VPathRelease() failed" );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
            {
                rc_t rc2 = VResolverRelease( resolver );
                DISP_RC( rc2, "VResolverRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        {
            rc_t rc2 = VFSManagerRelease ( vfs_mgr );
            DISP_RC( rc2, "VFSManagerRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }

    if ( 0 == rc && vdh_str_starts_with( dst, "ncbi-acc:" ) )
    {
        size_t l = string_size ( dst );
        memmove( dst, &( dst[ 9 ] ), l - 9 );
        dst[ l - 9 ] = 0;
    }
    return rc;
}

rc_t resolve_accession( const char * accession, char * dst, size_t dst_size, bool remotely )
{
    VFSManager * vfs_mgr;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    dst[ 0 ] = 0;
    DISP_RC( rc, "VFSManagerMake() failed" );
    if ( 0 == rc )
    {
        VResolver * resolver;
        rc = VFSManagerGetResolver( vfs_mgr, &resolver );
        DISP_RC( rc, "VFSManagerGetResolver() failed" );
        if ( 0 == rc )
        {
            VPath * vpath;
            rc = VFSManagerMakePath( vfs_mgr, &vpath, "ncbi-acc:%s", accession );
            DISP_RC( rc, "VFSManagerMakePath() failed" );
            if ( 0 == rc )
            {
                const VPath * local = NULL;
                const VPath * remote = NULL;
                if ( remotely )
                {
                    rc = VResolverQuery ( resolver, 0, vpath, &local, &remote, NULL );
                }
                else
                {
                    rc = VResolverQuery ( resolver, 0, vpath, &local, NULL, NULL );
                }
                if ( 0 == rc && ( NULL != local || NULL != remote ) )
                {
                    const String * path;
                    if ( NULL != local )
                    {
                        rc = VPathMakeString( local, &path );
                    }
                    else
                    {
                        rc = VPathMakeString( remote, &path );
                    }

                    if ( 0 == rc && NULL != path )
                    {
                        string_copy ( dst, dst_size, path -> addr, path -> size );
                        dst[ path -> size ] = 0;
                        StringWhack ( path );
                    }

                    if ( NULL != local )
                    {
                        VPathRelease ( local );
                    }
                    if ( NULL != remote )
                    {
                        VPathRelease ( remote );
                    }
                }
                {
                    rc_t rc2 = VPathRelease ( vpath );
                    DISP_RC( rc2, "VPathRelease() failed" );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
            {
                rc_t rc2 = VResolverRelease( resolver );
                DISP_RC( rc2, "VResolverRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        {
            rc_t rc2 = VFSManagerRelease ( vfs_mgr );
            DISP_RC( rc2, "VFSManagerRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }

    if ( 0 == rc && vdh_str_starts_with( dst, "ncbi-acc:" ) )
    {
        size_t l = string_size ( dst );
        memmove( dst, &( dst[ 9 ] ), l - 9 );
        dst[ l - 9 ] = 0;
    }
    return rc;
}

rc_t resolve_cache( const char * accession, char * dst, size_t dst_size )
{
    VFSManager * vfs_mgr;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    dst[ 0 ] = 0;
    DISP_RC( rc, "VFSManagerMake() failed" );
    if ( 0 == rc )
    {
        VResolver * resolver;
        rc = VFSManagerGetResolver( vfs_mgr, &resolver );
        DISP_RC( rc, "VFSManagerGetResolver() failed" );
        if ( 0 == rc )
        {
            VPath * vpath;
            rc = VFSManagerMakePath( vfs_mgr, &vpath, "ncbi-acc:%s", accession );
            DISP_RC( rc, "VFSManagerMakePath() failed" );
            if ( 0 == rc )
            {
                const VPath * local = NULL;
                const VPath * remote = NULL;
                const VPath * cache = NULL;
                rc = VResolverQuery ( resolver, 0, vpath, &local, &remote, &cache );
                if ( 0 == rc && cache != NULL )
                {
                    const String * path;
                    rc = VPathMakeString( cache, &path );

                    if ( 0 == rc && NULL != path )
                    {
                        string_copy ( dst, dst_size, path -> addr, path -> size );
                        dst[ path -> size ] = 0;
                        StringWhack ( path );
                    }

                    if ( NULL != local )
                    {
                        VPathRelease ( local );
                    }
                    if ( NULL != remote )
                    {
                        VPathRelease ( remote );
                    }
                    if ( NULL != remote )
                    {
                        VPathRelease ( cache );
                    }
                }
                {
                    rc_t rc2 = VPathRelease ( vpath );
                    DISP_RC( rc2, "VPathRelease() failed" );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
            {
                rc_t rc2 = VResolverRelease( resolver );
                DISP_RC( rc2, "VResolverRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        {
            rc_t rc2 = VFSManagerRelease ( vfs_mgr );
            DISP_RC( rc2, "VFSManagerRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}

rc_t check_cache_comleteness( const char * path, float * percent, uint64_t * bytes_in_cache )
{
    rc_t rc = 0;
    if ( NULL != percent)
    {
        ( * percent ) = 0.0;
    }
    if ( NULL != bytes_in_cache )
    {
        ( * bytes_in_cache ) = 0;
    }
    if ( NULL != path && 0 != path[ 0 ] )
    {
        KDirectory * dir;
        rc_t rc = KDirectoryNativeDir( &dir );
        DISP_RC( rc, "KDirectoryNativeDir() failed" );
        if ( 0 == rc )
        {
            const KFile * f = NULL;
            rc = KDirectoryOpenFileRead( dir, &f, "%s.cache", path );
            if ( 0 == rc )
            {
                rc = GetCacheCompleteness( f, percent, bytes_in_cache );
            }
            else
            {
                rc = KDirectoryOpenFileRead( dir, &f, "%s", path );
                if ( 0 == rc )
                {
                    if ( NULL != percent )
                    {
                        ( * percent ) = 100.0;
                    }
                    if ( NULL != bytes_in_cache )
                    {
                        rc = KFileSize ( f, bytes_in_cache );
                    }
                }
            }
            if ( NULL != f )
            {
                rc_t rc2 = KFileRelease( f );
                DISP_RC( rc2, "KFileRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
            {
                rc_t rc2 = KDirectoryRelease( dir );
                DISP_RC( rc2, "KDirectoryRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static bool matches( const String * cmd, const String * pattern )
{
    char buffer[ 256 ];
    String match;
    uint32_t matching;
    
    StringInit( &match, buffer, sizeof buffer, 0 );
    matching = StringMatch( &match, cmd, pattern );
    return ( matching == pattern->len && matching == cmd -> len );
}

int32_t index_of_match( const String * word, uint32_t num, ... )
{
    int32_t res = -1;
    if ( NULL != word )
    {
        uint32_t idx;
        va_list args;
        
        va_start ( args, num );
        for ( idx = 0; idx < num && res < 0; ++idx )
        {
            const char * arg = va_arg ( args, const char * );
            if ( arg != NULL )
            {
                String S;
                StringInitCString( &S, arg );
                if ( matches( word, &S ) )
                {
                    res = idx;
                }
            }
        }
        va_end ( args );
    }
    return res;
}

static void CC destroy_String( void * item, void * data ) { free( item ); }
void destroy_String_vector( Vector * v ) { VectorWhack( v, destroy_String, NULL ); }

uint32_t copy_String_2_vector( Vector * v, const String * S )
{
    uint32_t res = 0;
    if ( S -> len > 0 && NULL != S->addr )
    {
        String * S1 = malloc( sizeof * S1 );
        if ( NULL != S1 )
        {
            rc_t rc;
            StringInit( S1, S->addr, S->size, S->len );
            rc = VectorAppend( v, NULL, S1 );
            if ( 0 == rc )
            {
                res++;
            }
            else
            {
                free( S1 );
            }
        }
    }
    return res;
}

uint32_t split_buffer( Vector * v, const String * S, const char * delim )
{
    uint32_t i, res = 0;
    size_t delim_len = string_size( delim );
    String temp;
    
    StringInit( &temp, NULL, 0, 0 );
    VectorInit( v, 0, 10 );
    for( i = 0; i < S -> len; ++i )
    {
        if ( NULL != string_chr( delim, delim_len, S->addr[ i ] ) )
        {
            /* delimiter found */
            res += copy_String_2_vector( v, &temp );
            StringInit( &temp, NULL, 0, 0 );
        }
        else
        {
            /* normal char in line */
            if ( NULL == temp . addr )
            {
                temp . addr = &( S -> addr[ i ] );
            }
            temp . size++;
            temp . len++;
        }
    }
    res += copy_String_2_vector( v, &temp );
    return res;
}

rc_t vdh_path_to_vpath( const char * path, VPath ** vpath )
{
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    DISP_RC( rc, "VFSManagerMake() failed" );
    if ( 0 == rc )
    {
        VPath * in_path = NULL;
        rc = VFSManagerMakePath( vfs_mgr, &in_path, "%s", path );
        DISP_RC( rc, "VFSManagerMakePath() failed" );
        if ( 0 == rc )
        {
            rc = VFSManagerResolvePath( vfs_mgr, vfsmgr_rflag_kdb_acc, in_path, vpath );
            DISP_RC( rc, "VFSManagerResolvePath() failed" );
            {
                rc_t rc2 = VPathRelease( in_path );
                DISP_RC( rc2, "VPathRelease() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        {
            rc_t rc2 = VFSManagerRelease( vfs_mgr );
            DISP_RC( rc2, "VFSManagerRelease() failed" );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }
    return rc;
}
