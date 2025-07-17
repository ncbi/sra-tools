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

#include <klib/printf.h>
#include <kfs/cacheteefile.h>
#include <sra/sraschema.h>
#include <vfs/resolver.h>

#include <os-native.h>
#include <sysalloc.h>

#include "vdb-dump-helper.h"

rc_t ErrMsg( const char * fmt, ... ) {
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;

    va_list list;
    va_start( list, fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, fmt, list );
    if ( 0 == rc ) {
        rc = pLogMsg( klogErr, "$(E)", "E=%s", buffer );
    }
    va_end( list );
    return rc;
} 

/********************************************************************
helper function to display the version of the vdb-manager
********************************************************************/
rc_t vdh_show_manager_version( const VDBManager *mgr ) {
    uint32_t version;
    rc_t rc = VDBManagerVersion( mgr, &version );
    DISP_RC( rc, "VDBManagerVersion() failed" );
    if ( 0 == rc  ) {
        PLOGMSG ( klogInfo, ( klogInfo, "manager-version = $(maj).$(min).$(rel)",
                              "vers=0x%X,maj=%u,min=%u,rel=%u",
                              version,
                              version >> 24,
                              ( version >> 16 ) & 0xFF,
                              version & 0xFFFF ));
    }
    return rc;
}

static void CC vdh_parse_1_schema( void *item, void *data ) {
    char *s = ( char* )item;
    VSchema *schema = ( VSchema* )data;
    if ( ( NULL != item ) && ( NULL != schema ) ) {
        rc_t rc = VSchemaParseFile( schema, "%s", s );
        DISP_RC( rc, "VSchemaParseFile() failed" );
    }
}

rc_t vdh_parse_schema( const VDBManager *mgr, VSchema **new_schema, Vector *schema_list ) {
    rc_t rc = 0;
    if ( NULL == mgr ) {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    if ( NULL == new_schema ) {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    *new_schema = NULL;
    /*
    if ( with_sra_schema ) {
        rc = VDBManagerMakeSRASchema( mgr, new_schema );
        DISP_RC( rc, "VDBManagerMakeSRASchema() failed" );
    }
    */
    if ( ( 0 == rc )&&( NULL != schema_list ) ) {
        if ( NULL == *new_schema ) {
            rc = VDBManagerMakeSchema( mgr, new_schema );
            DISP_RC( rc, "VDBManagerMakeSchema() failed" );
        }
        if ( 0 == rc ) {
            VectorForEach( schema_list, false, vdh_parse_1_schema, *new_schema );
        }
    }
    return rc;
}

rc_t vdh_parse_schema_add_on ( const VDBManager *my_manager,
                               VSchema *base_schema,
                               Vector *schema_list ) {
    if ( NULL == my_manager ) {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    if ( NULL == base_schema ) {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    if ( NULL != schema_list ) {
       VectorForEach( schema_list, false, vdh_parse_1_schema, base_schema );
    }
    return 0;
}

/********************************************************************
helper function to test if a given path is a vdb-table
********************************************************************/
bool vdh_is_path_table( const VDBManager *mgr, const char *path, Vector *schema_list ) {
    bool res = false;
    VSchema *schema = NULL;
    rc_t rc = vdh_parse_schema( mgr, &schema, schema_list );
    if ( 0 == rc ) {
        const VTable *tbl;
        rc = VDBManagerOpenTableRead( mgr, &tbl, schema, "%s", path );
        if ( 0 == rc ) {
            res = true; /* yes we are able to open the table ---> path is a table */
            vdh_vtable_release( rc, tbl );
        }
        vdh_vschema_release( rc, schema );
    }
    return res;
}

const char * backback = "/../..";

/********************************************************************
helper function to test if a given path is a vdb-column
by testing if the parent/parent dir is a vdb-table
********************************************************************/
bool vdh_is_path_column( const VDBManager *mgr, const char *path, Vector *schema_list ) {
    bool res = false;
    size_t path_len = string_size( path );
    char *pp_path = malloc( path_len + 20 );
    if ( NULL != pp_path ) {
        char *resolved = malloc( 1024 );
        if ( NULL != resolved ) {
            KDirectory *dir;
            rc_t rc = KDirectoryNativeDir( &dir );
            DISP_RC( rc, "KDirectoryNativeDir() failed" );
            if ( 0 == rc ) {
                string_copy( pp_path, path_len + 20, path, path_len );
                string_copy( &pp_path[ path_len ], 20, backback, string_size( backback ) );
                rc = KDirectoryResolvePath( dir, true, resolved, 1023, "%s", pp_path );
                DISP_RC( rc, "KDirectoryResolvePath() failed" );
                if ( 0 == rc ) {
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
bool vdh_is_path_database( const VDBManager *mgr, const char *path, Vector *schema_list ) {
    bool res = false;
    VSchema *schema = NULL;
    rc_t rc = vdh_parse_schema( mgr, &schema, schema_list );
    if ( 0 == rc ) {
        const VDatabase *db;        
        rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", path );
        if ( 0 == rc ) {
            res = true; /* yes we are able to open the database ---> path is a database */
            rc = vdh_vdatabase_release( rc, db );
        }
        vdh_vschema_release( rc, schema );
    }
    return res;
}

/*************************************************************************************
helper-function to extract the name of the first table of a database
and put it into the dump-context
*************************************************************************************/
bool vdh_take_1st_table_from_db( dump_context *ctx, const KNamelist * tbl_names ) {
    bool we_found_a_table = false;
    uint32_t count;
    rc_t rc = KNamelistCount( tbl_names, &count );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc && count > 0 ) {
        const char *tbl_name;
        rc = KNamelistGet( tbl_names, 0, &tbl_name );
        DISP_RC( rc, "KNamelistGet() failed" );
        if ( 0 == rc ) {
            vdco_set_table( ctx, tbl_name );
            we_found_a_table = true;
        }
    }
    return we_found_a_table;
}

static bool vdh_str_starts_with( const char *a, const char *b ) {
    bool res = false;
    size_t asize = string_size ( a );
    size_t bsize = string_size ( b );
    if ( asize >= bsize ) {
        res = ( 0 == strcase_cmp ( a, bsize, b, bsize, (uint32_t)bsize ) );
    }
    return res;
}

bool vdh_list_contains_value( const KNamelist * list, const String * value ) {
    bool found = false;
    uint32_t count;
    rc_t rc = KNamelistCount( list, &count );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc && count > 0 ) {
        uint32_t i;
        for ( i = 0; i < count && 0 == rc && !found; ++i ) {
            const char *s;
            rc = KNamelistGet( list, i, &s );
            if ( 0 != rc ) {
                ErrMsg( "KNamelistGet( %d ) -> %R", i, rc );
            } else {
                String item;
                StringInitCString( &item, s );
                found = ( 0 == StringCompare ( &item, value ) );
            }
        }
    }
    return found;
}

static bool list_contains_value_starting_with( const KNamelist * list, 
                                               const String * value, String * found ) {
    bool res = false;
    uint32_t count;
    rc_t rc = KNamelistCount( list, &count );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc && count > 0 ) {
        uint32_t i;
        for ( i = 0; i < count && 0 == rc && !res; ++i ) {
            const char *s;
            rc = KNamelistGet( list, i, &s );
            if ( 0 != rc ) {
                ErrMsg( "KNamelistGet( %d ) -> %R", i, rc );
            } else {
                String item;
                StringInitCString( &item, s );
                if ( value->len <= item.len ) {
                    item.len = value->len;
                    item.size = value->size;
                    res = ( 0 == StringCompare ( &item, value ) );
                    if ( res ) {
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
                                    const char * table_to_find ) {
    bool res;
    String to_find;

    StringInitCString( &to_find, table_to_find );
    res = vdh_list_contains_value( tbl_names, &to_find );
    if ( res ) {
        vdco_set_table_String( ctx, &to_find );
    } else {
        String found;
        res = list_contains_value_starting_with( tbl_names, &to_find, &found );
        if ( res ) {
            vdco_set_table_String( ctx, &found );
        }
    }
    return res;
}

bool vdh_take_this_table_from_db( dump_context *ctx, const VDatabase * db,
                                  const char * table_to_find ) {
    bool we_found_the_table = false;
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( db, &tbl_names );
    DISP_RC( rc, "VDatabaseListTbl() failed" );
    if ( 0 == rc ) {
        we_found_the_table = vdh_take_this_table_from_list( ctx, tbl_names, table_to_find );
        rc = vdh_knamelist_release( rc, tbl_names );
    }
    return we_found_the_table;
}

const char * s_unknown_tab = "unknown table";

static rc_t vdh_print_full_col_info( dump_context *ctx,
                                     const p_col_def col_def,
                                     const VSchema *schema ) {
    rc_t rc = 0;
    char * s_domain = vdcd_make_domain_txt( col_def -> type_desc . domain );
    if ( NULL != s_domain ) {
        if ( NULL == ctx -> table ) {
            ctx -> table = string_dup_measure( s_unknown_tab, NULL ); /* will be free'd when ctx get free'd */
        }
        if ( ( NULL != ctx -> table ) && ( NULL != col_def -> name ) ) {
            rc = KOutMsg( "%s.%.02d : (%.3d bits [%.02d], %8s)  %s",
                    ctx -> table,
                    ctx -> generic_idx++,
                    col_def -> type_desc . intrinsic_bits,
                    col_def -> type_desc . intrinsic_dim,
                    s_domain,
                    col_def -> name );
            if ( 0 == rc && NULL != schema ) {
                char buf[ 64 ];
                rc = VTypedeclToText( &( col_def -> type_decl ), schema, buf, sizeof( buf ) );
                DISP_RC( rc, "VTypedeclToText() failed" );
                if ( 0 == rc ) {
                    rc = KOutMsg( "\n      (%s)", buf );
                }
            } 
            if ( 0 == rc ) {
                rc = KOutMsg( "\n" );
            }
        } else {
            if ( NULL == ctx -> table ) {
                rc = KOutMsg( "error: no table-name in print_column_info()" );
            }
            if ( NULL == col_def -> name ) {
                rc = KOutMsg( "error: no column-name in print_column_info()" );
            }
        }
        free( s_domain );
    } else {
        rc = KOutMsg( "error: making domain-text in print_column_info()" );
    }
    return rc;
}

static rc_t vdh_print_short_col_info( const p_col_def col_def, const VSchema *schema ) {
    rc_t rc = 0;
    if ( NULL != col_def -> name ) {
        rc = KOutMsg( "%s", col_def -> name );
        if ( 0 == rc && NULL != schema ) {
            char buf[ 64 ];
            rc = VTypedeclToText( &( col_def -> type_decl ), schema, buf, sizeof( buf ) );
            DISP_RC( rc, "VTypedeclToText() failed" );
            if ( 0 == rc ) {
                rc = KOutMsg( " (%s)", buf );
            }
        }
        if ( 0 == rc ) {
            rc = KOutMsg( "\n" );
        }
    }
    return rc;
}

rc_t vdh_print_col_info( dump_context *ctx, const p_col_def col_def, const VSchema *schema ) {
    rc_t rc = 0;
    if ( NULL != ctx && NULL != col_def ) {
        if ( ctx -> column_enum_requested ) {
            rc = vdh_print_full_col_info( ctx, col_def, schema );
        } else {
            rc = vdh_print_short_col_info( col_def, schema );
        }
    }
    return rc;
}

rc_t vdh_resolve(const char * accession,
    const VPath ** local, const VPath ** remote, const VPath ** cache)
{
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake(&vfs_mgr);
    * local = * remote = * cache = NULL;
    if (rc == 0)
        rc = VFSManagerResolveAll(vfs_mgr,
            accession, local, remote, cache);
    rc = vdh_vfsmanager_release(rc, vfs_mgr);
    return rc;
}

rc_t vdh_set_VPath_to_str(const VPath * path,
    char * dst, size_t dst_size)
{
    rc_t rc = 0;
    dst[0] = '\0';
    if (path != NULL) {
        const String * s;
        rc = VPathMakeString(path, &s);
        if (0 == rc && NULL != s) {
            string_copy(dst, dst_size, s->addr, s->size);
            dst[s->size] = 0;
            StringWhack(s);
        }
    }
    return rc;
}

rc_t vdh_set_local_or_remote_to_str(const VPath * local, const VPath * remote,
    char * dst, size_t dst_size)
{
    rc_t rc = 0;
    const VPath * path = local != NULL ? local : remote;
    dst[0] = '\0';
    if (path != NULL) {
        const String * s;
        rc = VPathMakeString(path, &s);
        if (0 == rc && NULL != s) {
            string_copy(dst, dst_size, s->addr, s->size);
            dst[s->size] = 0;
            StringWhack(s);
        }
    }
    return rc;
}

rc_t vdh_check_cache_comleteness( const char * path, float * percent, uint64_t * bytes_in_cache ) {
    rc_t rc = 0;
    if ( NULL != percent ) {
        ( * percent ) = 0.0;
    }
    if ( NULL != bytes_in_cache ) {
        ( * bytes_in_cache ) = 0;
    }
    if ( NULL != path && 0 != path[ 0 ] ) {
        KDirectory * dir;
        rc_t rc = KDirectoryNativeDir( &dir );
        DISP_RC( rc, "KDirectoryNativeDir() failed" );
        if ( 0 == rc ) {
            const KFile * f = NULL;
            rc = KDirectoryOpenFileRead( dir, &f, "%s.cache", path );
            if ( 0 == rc ) {
                rc = GetCacheCompleteness( f, percent, bytes_in_cache );
            } else {
                rc = KDirectoryOpenFileRead( dir, &f, "%s", path );
                if ( 0 == rc ) {
                    if ( NULL != percent ) {
                        ( * percent ) = 100.0;
                    }
                    if ( NULL != bytes_in_cache ) {
                        rc = KFileSize ( f, bytes_in_cache );
                    }
                }
            }
            rc = vdh_kfile_release( rc, f );
            rc = vdh_kdirectory_release( rc, dir );
        }
    }
    return rc;
}
            
rc_t vdh_path_to_vpath( const char * path, VPath ** vpath ) {
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    DISP_RC( rc, "VFSManagerMake() failed" );
    if ( 0 == rc ) {
        VPath * in_path = NULL;
        rc = VFSManagerMakePath( vfs_mgr, &in_path, "%s", path );
        DISP_RC( rc, "VFSManagerMakePath() failed" );
        if ( 0 == rc ) {
            rc = VFSManagerResolvePath( vfs_mgr, vfsmgr_rflag_kdb_acc, in_path, vpath );
            DISP_RC( rc, "VFSManagerResolvePath() failed" );
            rc = vdh_vpath_release( rc, in_path );
        }
        rc = vdh_vfsmanager_release( rc, vfs_mgr );
    }
    return rc;
}

void vdh_clear_recorded_errors( void ) {
    rc_t rc;
    const char * filename;
    const char * funcname;
    uint32_t line_nr;
    while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &line_nr ) ) {
    }
}

rc_t vdh_check_table_empty( const VTable * tab ) {
    bool empty;
    rc_t rc = VTableIsEmpty( tab, &empty );
    DISP_RC( rc, "VTableIsEmpty() failed" );
    if ( 0 == rc && empty ) {
        vdh_clear_recorded_errors();
        KOutMsg( "the requested table is empty!\n" );
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTable, rcEmpty );
    }
    return rc;
}

/* ================================================================================= */

static rc_t vdh_walk_sections( const VDatabase * base_db, const VDatabase ** sub_db,
                    const VNamelist * sections, uint32_t count ) {
    rc_t rc = 0;
    const VDatabase * parent_db = base_db;
    if ( 0 == count ) {
        rc = VDatabaseAddRef ( parent_db );
        DISP_RC( rc, "VDatabaseAddRef() failed" );
    } else {
        uint32_t idx;
        for ( idx = 0; 0 == rc && idx < count; ++idx ) {
            const char * dbname;
            rc = VNameListGet ( sections, idx, &dbname );
            DISP_RC( rc, "VNameListGet() failed" );
            if ( 0 == rc ) {
                const VDatabase * temp;
                rc = VDatabaseOpenDBRead ( parent_db, &temp, "%s", dbname );
                DISP_RC( rc, "VDatabaseOpenDBRead() failed" );
                if ( 0 == rc && idx > 0 ) {
                    rc = vdh_vdatabase_release( rc, parent_db );
                }
                if ( 0 == rc ) {
                    parent_db = temp;
                }
            }
        }
    }
    if ( 0 == rc ) {
        *sub_db = parent_db;
    }
    return rc;
}

rc_t vdh_open_table_by_path( const VDatabase * db, const char * inner_db_path, const VTable ** tab ) {
    VNamelist * sections;
    rc_t rc = vds_path_to_sections( inner_db_path, '.', &sections );
    DISP_RC( rc, "vds_path_to_sections() failed" );
    if ( 0 == rc ) {
        uint32_t count;
        rc = VNameListCount ( sections, &count );
        DISP_RC( rc, "VNameListCount() failed" );
        if ( 0 == rc && count > 0 )         {
            const VDatabase * sub_db;
            rc = vdh_walk_sections( db, &sub_db, sections, count - 1 );
            if ( 0 == rc ) {
                const char * tabname;
                rc = VNameListGet ( sections, count - 1, &tabname );
                DISP_RC( rc, "VNameListGet() failed" );
                if ( 0 == rc ) {
                    rc = VDatabaseOpenTableRead( sub_db, tab, "%s", tabname );
                    DISP_RC( rc, "VDatabaseOpenTableRead() failed" );
                    if ( 0 == rc ) {
                        rc = vdh_check_table_empty( *tab );
                        if ( 0 != rc ) {
                            vdh_vtable_release( rc, *tab );
                            tab = NULL;
                        }
                    }
                }
                rc = vdh_vdatabase_release( rc, sub_db );
            }
        }
        rc = vdh_vnamelist_release( rc, sections );
    }
    return rc;
}

rc_t vdh_open_vpath_as_file( const KDirectory * dir, const VPath * vpath, const KFile ** f ) {
    rc_t rc;
    if ( NULL == dir || NULL == vpath || NULL ==f ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    } else {
        char syspath[ 4096 ];
        size_t syspath_len;
        rc = VPathReadSysPath( vpath, syspath, sizeof syspath, &syspath_len );
        if ( 0 == rc ) {
            KOutMsg( "syspath: >%s<\n", syspath );
            uint32_t path_type = KDirectoryPathType( dir, "%s", syspath );
            if ( kptFile == path_type || ( kptFile | kptAlias ) == path_type ) {
                KOutMsg( "...it is a file!\n" );
                rc = KDirectoryOpenFileRead( dir, f, "%s", syspath );
            } else {
                rc = SILENT_RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                vdh_clear_recorded_errors();
            }
        }
    }
    return rc;
}

rc_t vdh_vfsmanager_release( rc_t rc, const VFSManager * mgr ) {
    if ( NULL != mgr ) {
        rc_t rc2 = VFSManagerRelease( mgr );
        DISP_RC( rc2, "VFSManagerRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_vpath_release( rc_t rc, const VPath * path ) {
    if ( NULL != path ) {
        rc_t rc2 = VPathRelease( path );
        DISP_RC( rc2, "VPathRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_knamelist_release( rc_t rc, const KNamelist * namelist ) {
    if ( NULL != namelist ) {
        rc_t rc2 = KNamelistRelease( namelist );
        DISP_RC( rc2, "KNamelistRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_vnamelist_release( rc_t rc, const VNamelist * namelist ) {
    if ( NULL != namelist ) {
        rc_t rc2 = VNamelistRelease( namelist );
        DISP_RC( rc2, "VNamelistRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_vschema_release( rc_t rc, const VSchema * schema ) {
    if ( NULL != schema ) {
        rc_t rc2 = VSchemaRelease( schema );
        DISP_RC( rc2, "VSchemaRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_vdatabase_release( rc_t rc, const VDatabase * db ) {
    if ( NULL != db ) {
        rc_t rc2 = VDatabaseRelease( db );
        DISP_RC( rc2, "VDatabaseRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_kdatabase_release( rc_t rc, const KDatabase * db ) {
    if ( NULL != db ) {
        rc_t rc2 = KDatabaseRelease( db );
        DISP_RC( rc2, "KDatabaseRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_vtable_release( rc_t rc, const VTable * tbl ) {
    if ( NULL != tbl ) {
        rc_t rc2 = VTableRelease( tbl );
        DISP_RC( rc2, "VTableRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_ktable_release( rc_t rc, const KTable * tbl ) {
    if ( NULL != tbl ) {
        rc_t rc2 = KTableRelease( tbl );
        DISP_RC( rc2, "KTableRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_vcursor_release( rc_t rc, const VCursor * cursor ) {
    if ( NULL != cursor ) {
        rc_t rc2 = VCursorRelease( cursor );
        DISP_RC( rc2, "VCursorRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_kcolumnblob_release( rc_t rc, const KColumnBlob * blob ) {
    if ( NULL != blob ) {
        rc_t rc2 = KColumnBlobRelease( blob );
        DISP_RC( rc2, "KColumnBlobRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_kcolumn_release( rc_t rc, const KColumn * col ) {
    if ( NULL != col ) {
        rc_t rc2 = KColumnRelease( col );
        DISP_RC( rc2, "KColumnRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_kindex_release( rc_t rc, const KIndex * idx ) {
    if ( NULL != idx ) {
        rc_t rc2 = KIndexRelease( idx );
        DISP_RC( rc2, "KIndexRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_datanode_release( rc_t rc, const KMDataNode * node ) {
    if ( NULL != node ) {
        rc_t rc2 = KMDataNodeRelease( node );
        DISP_RC( rc2, "KMDataNodeRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_kmeta_release( rc_t rc, const KMetadata * meta ) {
    if ( NULL != meta ) {
        rc_t rc2 = KMetadataRelease( meta );
        DISP_RC( rc2, "KMetadataRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_view_release( rc_t rc, const VView * view ) {
    if ( NULL != view ) {
        rc_t rc2 = VViewRelease( view );
        DISP_RC( rc2, "VViewRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_kdirectory_release( rc_t rc, const KDirectory * dir ) {
    if ( NULL != dir ) {
        rc_t rc2 = KDirectoryRelease( dir );
        DISP_RC( rc2, "KDirectoryRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_kfile_release( rc_t rc, const KFile * f ) {
    if ( NULL != f ) {
        rc_t rc2 = KFileRelease( f );
        DISP_RC( rc2, "KFileRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

rc_t vdh_vmanager_release( rc_t rc, const VDBManager * mgr ) {
    if ( NULL != mgr ) {
        rc_t rc2 = VDBManagerRelease( mgr );
        DISP_RC( rc2, "VDBManagerRelease() failed" );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}
