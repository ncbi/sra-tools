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

#include <kfs/directory.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/text.h>

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

/********************************************************************
helper function to display the version of the vdb-manager
********************************************************************/
rc_t vdh_show_manager_version( const VDBManager *my_manager )
{
    uint32_t version;
    rc_t rc = VDBManagerVersion( my_manager, &version );
    DISP_RC( rc, "VDBManagerVersion() failed" );
    if ( rc == 0 )
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
    char *s = (char*)item;
    VSchema *my_schema = (VSchema*)data;
    if ( ( item != NULL )&&( my_schema != NULL ) )
    {
        rc_t rc = VSchemaParseFile( my_schema, "%s", s );
        DISP_RC( rc, "VSchemaParseFile() failed" );
    }
}

rc_t vdh_parse_schema( const VDBManager *my_manager,
                       VSchema **new_schema,
                       Vector *schema_list )
{
    rc_t rc;

    if ( my_manager == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    if ( new_schema == NULL )
    {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }

    rc = VDBManagerMakeSRASchema( my_manager, new_schema );
    DISP_RC( rc, "VDBManagerMakeSRASchema() failed" );

    if ( ( rc == 0 )&&( schema_list != NULL ) )
    {
        VectorForEach( schema_list, false, vdh_parse_1_schema, *new_schema );
    }

/*    rc = VDBManagerMakeSchema( my_manager, new_schema );
    display_rescode( rc, "failed to make a schema", NULL ); */
    return rc;
}

/********************************************************************
helper function to test if a given path is a vdb-table
********************************************************************/
bool vdh_is_path_table( const VDBManager *my_manager, const char *path,
                        Vector *schema_list )
{
    bool res = false;
    const VTable *my_table;
    VSchema *my_schema = NULL;
    rc_t rc;

    rc = vdh_parse_schema( my_manager, &my_schema, schema_list );
    DISP_RC( rc, "helper_parse_schema() failed" );

    rc = VDBManagerOpenTableRead( my_manager, &my_table, my_schema, "%s", path );
    DISP_RC( rc, "VDBManagerOpenTableRead() failed" );
    if ( rc == 0 )
        {
        res = true; /* yes we are able to open the table ---> path is a table */
        VTableRelease( my_table );
        }

    if ( my_schema != NULL )
    {
        rc = VSchemaRelease( my_schema );
        DISP_RC( rc, "VSchemaRelease() failed" );
    }

    return res;
}


const char * backback = "/../..";

/********************************************************************
helper function to test if a given path is a vdb-column
by testing if the parent/parent dir is a vdb-table
********************************************************************/
bool vdh_is_path_column( const VDBManager *my_manager, const char *path,
                         Vector *schema_list )
{
    bool res = false;
    size_t path_len = string_size( path );
    char *pp_path = malloc( path_len + 20 );
    if ( pp_path )
    {
        char *resolved = malloc( 1024 );
        if ( resolved )
        {
            KDirectory *my_directory;
            rc_t rc = KDirectoryNativeDir( &my_directory );
            DISP_RC( rc, "KDirectoryNativeDir() failed" );
            if ( rc == 0 )
            {
                string_copy( pp_path, path_len + 20, path, path_len );
                string_copy( &pp_path[ path_len ], 20, backback, string_size( backback ) );
                rc = KDirectoryResolvePath( my_directory, true, resolved, 1023, "%s", pp_path );
                if ( rc == 0 )
                    res = vdh_is_path_table( my_manager, resolved, schema_list );
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
bool vdh_is_path_database( const VDBManager *my_manager, const char *path,
                           Vector *schema_list )
{
    bool res = false;
    const VDatabase *my_database;
    VSchema *my_schema = NULL;
    rc_t rc;

    rc = vdh_parse_schema( my_manager, &my_schema, schema_list );
    DISP_RC( rc, "helper_parse_schema() failed" );

    rc = VDBManagerOpenDBRead( my_manager, &my_database, my_schema, "%s", path );
    if ( rc == 0 )
        {
        res = true; /* yes we are able to open the database ---> path is a database */
        VDatabaseRelease( my_database );
        }

    if ( my_schema != NULL )
        VSchemaRelease( my_schema );

    return res;
}


/*************************************************************************************
helper-function to extract the name of the first table of a database
and put it into the dump-context
*************************************************************************************/
bool vdh_take_1st_table_from_db( dump_context *ctx, const VDatabase *my_database )
{
    bool we_found_a_table = false;
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( my_database, &tbl_names );
    DISP_RC( rc, "VDatabaseListTbl() failed" );
    if ( rc == 0 )
    {
        uint32_t n;
        rc = KNamelistCount( tbl_names, &n );
        DISP_RC( rc, "KNamelistCount() failed" );
        if ( ( rc == 0 )&&( n > 0 ) )
        {
            const char *tbl_name;
            rc = KNamelistGet( tbl_names, 0, &tbl_name );
            DISP_RC( rc, "KNamelistGet() failed" );
            if ( rc == 0 )
            {
                vdco_set_table( ctx, tbl_name );
                we_found_a_table = true;
            }
        }
        rc = KNamelistRelease( tbl_names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    return we_found_a_table;
}


static int vdh_str_cmp( const char *a, const char *b )
{
    size_t asize = string_size ( a );
    size_t bsize = string_size ( b );
    return strcase_cmp ( a, asize, b, bsize, ( asize > bsize ) ? asize : bsize );
}

static bool vdh_str_starts_with( const char *a, const char *b )
{
    bool res = false;
    size_t asize = string_size ( a );
    size_t bsize = string_size ( b );
    if ( asize >= bsize )
    {
        int cmp = strcase_cmp ( a, bsize, b, bsize, bsize );
        res = ( cmp == 0 );
    }
    return res;
}

/*************************************************************************************
helper-function to check if a given table is in the list of tables
if found put that name into the dump-context
*************************************************************************************/
bool vdh_take_this_table_from_db( dump_context *ctx, const VDatabase *my_database,
                                  const char * table_to_find )
{
    bool we_found_a_table = false;
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( my_database, &tbl_names );
    DISP_RC( rc, "VDatabaseListTbl() failed" );
    if ( rc == 0 )
    {
        uint32_t n;
        rc = KNamelistCount( tbl_names, &n );
        DISP_RC( rc, "KNamelistCount() failed" );
        if ( ( rc == 0 )&&( n > 0 ) )
        {
            uint32_t i;
            for ( i = 0; i < n && rc == 0 && !we_found_a_table; ++i )
            {
                const char *tbl_name;
                rc = KNamelistGet( tbl_names, i, &tbl_name );
                DISP_RC( rc, "KNamelistGet() failed" );
                if ( rc == 0 )
                {
                    if ( vdh_str_cmp( tbl_name, table_to_find ) == 0 )
                    {
                        vdco_set_table( ctx, tbl_name );
                        we_found_a_table = true;
                    }
                }
            }
        }
        rc = KNamelistRelease( tbl_names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    return we_found_a_table;
}


const char * s_unknown_tab = "unknown table";

static rc_t vdh_print_full_col_info( dump_context *ctx,
                                     const p_col_def col_def,
                                     const VSchema *my_schema )
{
    rc_t rc = 0;
    char * s_domain = vdcd_make_domain_txt( col_def->type_desc.domain );
    if ( s_domain != NULL )
    {
        if ( ctx->table == NULL )
        {
            ctx->table = string_dup_measure( s_unknown_tab, NULL ); /* will be free'd when ctx get free'd */
        }

        if ( ( ctx->table != NULL )&&( col_def->name != NULL ) )
        {
            rc = KOutMsg( "%s.%.02d : (%.3d bits [%.02d], %8s)  %s",
                    ctx->table,
                    ctx->generic_idx++,
                    col_def->type_desc.intrinsic_bits,
                    col_def->type_desc.intrinsic_dim,
                    s_domain,
                    col_def->name );
            if ( rc == 0 && my_schema )
            {
                char buf[64];
                rc = VTypedeclToText( &(col_def->type_decl), my_schema,
                                      buf, sizeof(buf) );
                DISP_RC( rc, "VTypedeclToText() failed" );
                if ( rc == 0 )
                    rc = KOutMsg( "\n      (%s)", buf );
            }
            if ( rc == 0 )
                rc = KOutMsg( "\n" );
        }
        else
        {
            if ( ctx->table == NULL )
            {
                rc = KOutMsg( "error: no table-name in print_column_info()" );
            }
            if ( col_def->name == NULL )
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


static rc_t vdh_print_short_col_info( const p_col_def col_def,
                                      const VSchema *my_schema )
{
    rc_t rc = 0;
    if ( col_def->name != NULL )
    {
        rc = KOutMsg( "%s", col_def->name );
        if ( my_schema )
        {
            char buf[64];
            rc = VTypedeclToText( &(col_def->type_decl), my_schema,
                                  buf, sizeof(buf) );
            DISP_RC( rc, "VTypedeclToText() failed" );
            if ( rc == 0 )
            {
                rc = KOutMsg( " (%s)", buf );
            }
        }
        if ( rc == 0 )
            rc = KOutMsg( "\n" );
    }
    return rc;
}


rc_t vdh_print_col_info( dump_context *ctx,
                         const p_col_def col_def,
                         const VSchema *my_schema )
{
    rc_t rc = 0;
    if ( ctx != NULL && col_def != NULL )
    {
        if ( ctx->column_enum_requested )
            rc = vdh_print_full_col_info( ctx, col_def, my_schema );
        else
            rc = vdh_print_short_col_info( col_def, my_schema );
    }
    return rc;
}



rc_t resolve_accession( const char * accession, char * dst, size_t dst_size, bool remotely )
{
    VFSManager * vfs_mgr;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    dst[ 0 ] = 0;
    if ( rc == 0 )
    {
        VResolver * resolver;
        rc = VFSManagerGetResolver( vfs_mgr, &resolver );
        if ( rc == 0 )
        {
            VPath * vpath;
            rc = VFSManagerMakePath( vfs_mgr, &vpath, "ncbi-acc:%s", accession );
            if ( rc == 0 )
            {
                const VPath * local = NULL;
                const VPath * remote = NULL;
                if ( remotely )
                    rc = VResolverQuery ( resolver, eProtocolHttp, vpath, &local, &remote, NULL );
                else
                    rc = VResolverQuery ( resolver, eProtocolHttp, vpath, &local, NULL, NULL );
                if ( rc == 0 && ( local != NULL || remote != NULL ) )
                {
                    const String * path;
                    if ( local != NULL )
                        rc = VPathMakeString( local, &path );
                    else
                        rc = VPathMakeString( remote, &path );

                    if ( rc == 0 && path != NULL )
                    {
                        string_copy ( dst, dst_size, path->addr, path->size );
                        dst[ path->size ] = 0;
                        StringWhack ( path );
                    }

                    if ( local != NULL )
                        VPathRelease ( local );
                    if ( remote != NULL )
                        VPathRelease ( remote );
                }
                VPathRelease ( vpath );
            }
            VResolverRelease( resolver );
        }
        VFSManagerRelease ( vfs_mgr );
    }

    if ( rc == 0 && vdh_str_starts_with( dst, "ncbi-acc:" ) )
    {
        size_t l = string_size ( dst );
        memmove( dst, &( dst[ 9 ] ), l - 9 );
        dst[ l - 9 ] = 0;
    }
    return rc;
}
