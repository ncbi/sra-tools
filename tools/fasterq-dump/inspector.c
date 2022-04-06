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

#include "inspector.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_dflt_defline_
#include "dflt_defline.h"
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_vfs_manager_
#include <vfs/manager.h>
#endif

#ifndef _h_vdb_database_
#include <vdb/database.h>
#endif

#ifndef _h_vdb_table_
#include <vdb/table.h>
#endif

#ifndef _h_vdb_cursor_
#include <vdb/cursor.h>
#endif

#ifndef _h_insdc_sra_
#include <insdc/sra.h>  /* platform enum */
#endif

#ifndef _h_kdb_manager_
#include <kdb/manager.h>    /* kpt... enums */
#endif

#ifndef _h_kns_manager_
#include <kns/manager.h>
#endif

#ifndef _h_kns_http_
#include <kns/http.h>
#endif

#ifndef _h_vfs_resolver_
#include <vfs/resolver.h>
#endif

#include <limits.h> /* PATH_MAX */
#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

static const char * inspector_extract_acc_from_path_v1( const char * s ) {
    const char * res = NULL;
    if ( ( NULL != s ) && ( !ends_in_slash( s ) ) ) {
        size_t size = string_size ( s );
        char * slash = string_rchr ( s, size, '/' );
        if ( NULL == slash ) {
            if ( ends_in_sra( s ) ) {
                res = string_dup ( s, size - 4 );
            } else {
                res = string_dup ( s, size );
            }
        } else {
            char * tmp = slash + 1;
            if ( ends_in_sra( tmp ) ) {
                res = string_dup ( tmp, string_size ( tmp ) - 4 );
            } else {
                res = string_dup ( tmp, string_size ( tmp ) );
            }
        }
    }
    return res;
}

static const char * inspector_extract_acc_from_path_v2( const char * s ) {
    const char * res = NULL;
    VFSManager * mgr;
    rc_t rc = VFSManagerMake ( &mgr );
    if ( 0 != rc ) {
        ErrMsg( "extract_acc2( '%s' ).VFSManagerMake() -> %R", s, rc );
    } else {
        VPath * orig;
        rc = VFSManagerMakePath ( mgr, &orig, "%s", s );
        if ( 0 != rc ) {
            ErrMsg( "extract_acc2( '%s' ).VFSManagerMakePath() -> %R", s, rc );
        } else {
            VPath * acc_or_oid = NULL;
            rc = VFSManagerExtractAccessionOrOID( mgr, &acc_or_oid, orig );
            if ( 0 != rc ) { /* remove trailing slash[es] and try again */
                char P_option_buffer[ PATH_MAX ] = "";
                size_t l = string_copy_measure( P_option_buffer, sizeof P_option_buffer, s );
                char * basename = P_option_buffer;
                while ( l > 0 && strchr( "\\/", basename[ l - 1 ]) != NULL ) {
                    basename[ --l ] = '\0';
                }
                VPath * orig = NULL;
                rc = VFSManagerMakePath ( mgr, &orig, "%s", P_option_buffer );
                if ( 0 != rc ) {
                    ErrMsg( "extract_acc2( '%s' ).VFSManagerMakePath() -> %R", P_option_buffer, rc );
                } else {
                    rc = VFSManagerExtractAccessionOrOID( mgr, &acc_or_oid, orig );
                    if ( 0 != rc ) {
                        ErrMsg( "extract_acc2( '%s' ).VFSManagerExtractAccessionOrOID() -> %R", s, rc );
                    }
                    {
                        rc_t r2 = VPathRelease ( orig );
                        if ( 0 != r2 ) {
                            ErrMsg( "extract_acc2( '%s' ).VPathRelease().2 -> %R", P_option_buffer, rc );
                            if ( 0 == rc ) { rc = r2; }
                        }
                    }
                }
            }
            if ( 0 == rc ) {
                char buffer[ 1024 ];
                size_t num_read;
                rc = VPathReadPath ( acc_or_oid, buffer, sizeof buffer, &num_read );
                if ( 0 != rc ) {
                    ErrMsg( "extract_acc2( '%s' ).VPathReadPath() -> %R", s, rc );
                } else {
                    res = string_dup ( buffer, num_read );
                }
                rc = VPathRelease ( acc_or_oid );
                if ( 0 != rc ) {
                    ErrMsg( "extract_acc2( '%s' ).VPathRelease().1 -> %R", s, rc );
                }
            }

            rc = VPathRelease ( orig );
            if ( 0 != rc ) {
                ErrMsg( "extract_acc2( '%s' ).VPathRelease().2 -> %R", s, rc );
            }
        }

        rc = VFSManagerRelease ( mgr );
        if ( 0 != rc ) {
            ErrMsg( "extract_acc2( '%s' ).VFSManagerRelease() -> %R", s, rc );
        }
    }
    return res;
}


const char * inspector_extract_acc_from_path( const char * s ) {
    const char * res = inspector_extract_acc_from_path_v2( s );
    // in case something goes wrong with acc-extraction via VFS-manager
    if ( NULL == res ) {
        res = inspector_extract_acc_from_path_v1( s );
    }
    return res;
}

static rc_t on_visit( const KDirectory *dir, uint32_t type, const char *name, void *data ) {
    rc_t rc = 0;
    if ( kptFile == ( type & kptFile ) ) {
        uint64_t size;
        rc = KDirectoryFileSize( dir, &size, "%s", name );
        if ( 0 == rc ) {
            uint64_t * res = data;
            *res += size;
        }
    }
    return rc;
}
    
static uint64_t get_file_size( const KDirectory * dir, const char * path, bool remotely )
{
    rc_t rc = 0;
    uint64_t res = 0;
    
    if ( remotely ) {
        KNSManager * kns_mgr;
        rc = KNSManagerMake ( &kns_mgr );
        if ( 0 == rc )
        {
            const KFile * f = NULL;
            rc = KNSManagerMakeHttpFile ( kns_mgr, &f, NULL, 0x01010000, "%s", path );
            if ( 0 == rc ) {
                rc = KFileSize ( f, &res );
                KFileRelease( f );
            }
            KNSManagerRelease ( kns_mgr );
        }
    } else {
        /* we first have to check if the path is a directory or a file! */
        uint32_t pt = KDirectoryPathType( dir, "%s", path );
        if ( kptDir == ( pt & kptDir ) ) {
            /* the path is a directory: adding size of each file in it */
            rc = KDirectoryVisit( dir, false, on_visit, &res, "%s", path );
        } else if ( kptFile == ( pt & kptFile ) ) {
            /* the path is a file: retrieve its size */
            rc = KDirectoryFileSize( dir, &res, "%s", path );
        }
    }

    if ( 0 != rc ) {
        ErrMsg( "inspector.c get_file_size( '%s', remotely = %s ) -> %R",
                path,
                yes_or_no( remotely ),
                rc );
    }
    return res;
}

static bool starts_with( const char *a, const char *b )
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

static rc_t resolve_accession( const char * accession, char * dst, size_t dst_size, bool remotely )
{
    VFSManager * vfs_mgr;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    dst[ 0 ] = 0;
    if ( 0 == rc ) {
        VResolver * resolver;
        rc = VFSManagerGetResolver( vfs_mgr, &resolver );
        if ( 0 == rc ) {
            VPath * vpath;
            rc = VFSManagerMakePath( vfs_mgr, &vpath, "%s", accession );
            if ( 0 == rc )
            {
                const VPath * local = NULL;
                const VPath * remote = NULL;
                if ( remotely ) {
                    rc = VResolverQuery ( resolver, 0, vpath, &local, &remote, NULL );
                } else {
                    rc = VResolverQuery ( resolver, 0, vpath, &local, NULL, NULL );
                }
                if ( 0 == rc && ( NULL != local || NULL != remote ) )
                {
                    const String * path;
                    if ( NULL != local ) {
                        rc = VPathMakeString( local, &path );
                    } else {
                        rc = VPathMakeString( remote, &path );
                    }

                    if ( 0 == rc && NULL != path ) {
                        string_copy ( dst, dst_size, path -> addr, path -> size );
                        dst[ path -> size ] = 0;
                        StringWhack ( path );
                    }

                    if ( NULL != local ) {
                        VPathRelease ( local );
                    }
                    if ( NULL != remote ) {
                        VPathRelease ( remote );
                    }
                }
                {
                    rc_t rc2 = VPathRelease ( vpath );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
            {
                rc_t rc2 = VResolverRelease( resolver );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
        {
            rc_t rc2 = VFSManagerRelease ( vfs_mgr );
            rc = ( 0 == rc ) ? rc2 : rc;
        }
    }

    if ( 0 == rc && starts_with( dst, "ncbi-acc:" ) )
    {
        size_t l = string_size ( dst );
        memmove( dst, &( dst[ 9 ] ), l - 9 );
        dst[ l - 9 ] = 0;
    }
    return rc;
}

rc_t inspector_path_to_vpath( const char * path, VPath ** vpath ) {
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    if ( 0 != rc ) {
        ErrMsg( "inspector.c inspector_path_to_vpath().VFSManagerMake( %s ) -> %R\n", path, rc );
    } else {
        VPath * in_path = NULL;
        rc = VFSManagerMakePath( vfs_mgr, &in_path, "%s", path );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspector_path_to_vpath().VFSManagerMakePath( %s ) -> %R\n", path, rc );
        } else {
            rc = VFSManagerResolvePath( vfs_mgr, vfsmgr_rflag_kdb_acc, in_path, vpath );
            if ( 0 != rc ) {
                ErrMsg( "inspector.c inspector_path_to_vpath().VFSManagerResolvePath( %s ) -> %R\n", path, rc );
            }
            {
                rc_t rc2 = VPathRelease( in_path );
                if ( 0 != rc2 ) {
                    ErrMsg( "inspector.c inspector_path_to_vpath().VPathRelease( %s ) -> %R\n", path, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VFSManagerRelease( vfs_mgr );
            if ( 0 != rc2 ) {
                ErrMsg( "inspector.c inspector_path_to_vpath().VFSManagerRelease( %s ) -> %R\n", path, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static rc_t inspector_release_vpath( VPath * v_path, rc_t rc ) {
    rc_t rc2 = VPathRelease( v_path );
    if ( 0 != rc2 ) {
        ErrMsg( "inspector.c inspector_release_vpath() -> %R\n", rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t inspector_open_db( const inspector_input_t * input, const VDatabase ** db ) {
    VPath * v_path = NULL;
    rc_t rc = inspector_path_to_vpath( input -> accession_path, &v_path );
    if ( 0 == rc ) {
        rc = VDBManagerOpenDBReadVPath( input -> vdb_mgr, db, NULL, v_path );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspector_open_db().VDBManagerOpenDBReadVPath( '%s' ) -> %R\n",
                    input -> accession_path, rc );
        }
        rc = inspector_release_vpath( v_path, rc );
    }
    return rc;
}

static rc_t inspector_open_tbl( const inspector_input_t * input, const VTable ** tbl ) {
    VPath * v_path = NULL;
    rc_t rc = inspector_path_to_vpath( input -> accession_path, &v_path );
    if ( 0 == rc ) {
        rc = VDBManagerOpenTableReadVPath( input -> vdb_mgr, tbl, NULL, v_path );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspector_open_tbl().VDBManagerOpenTableReadVPath( '%s' ) -> %R\n",
                    input -> accession_path, rc );
        }
        rc = inspector_release_vpath( v_path, rc );
    }
    return rc;
}

static rc_t inspector_release_db( const VDatabase * db, rc_t rc, const char * function, const char * accession_short ) {
    rc_t rc2 = VDatabaseRelease( db );
    if ( 0 != rc2 ) {
        ErrMsg( "inspector.c %s( '%s' ).VDatabaseRelease() -> %R", function, accession_short, rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static rc_t inspector_release_tbl( const VTable * tbl, rc_t rc, const char * function, const char * accession_short ) {
    rc_t rc2 = VTableRelease( tbl );
    if ( 0 != rc2 ) {
        ErrMsg( "inspector.c %s( '%s' ).VDatabaseTable() -> %R", function, accession_short, rc2 );
        rc = ( 0 == rc ) ? rc2 : rc;
    }
    return rc;
}

static bool contains( VNamelist * tables, const char * table ) {
    uint32_t found = 0;
    rc_t rc = VNamelistIndexOf( tables, table, &found );
    return ( 0 == rc );
}

static bool inspect_db_platform( const inspector_input_t * input, const VDatabase * db, uint8_t * pf ) {
    bool res = false;
    const VTable * tbl = NULL;
    rc_t rc = VDatabaseOpenTableRead ( db, &tbl, "SEQUENCE" );
    if ( 0 != rc ) {
        ErrMsg( "inspector.c inspect_db_platform().VDatabaseOpenTableRead( '%s' ) -> %R\n",
                 input -> accession_short, rc );
    } else {
        const VCursor * curs = NULL;
        rc = VTableCreateCursorRead( tbl, &curs );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspect_db_platform().VTableCreateCursor( '%s' ) -> %R\n",
                    input -> accession_short, rc );
        } else {
            uint32_t idx;
            rc = VCursorAddColumn( curs, &idx, "PLATFORM" );
            if ( 0 != rc ) {
                ErrMsg( "inspector.c inspect_db_platform().VCursorAddColumn( '%s', PLATFORM ) -> %R\n",
                        input -> accession_short, rc );
            } else {
                rc = VCursorOpen( curs );
                if ( 0 != rc ) {
                    ErrMsg( "inspector.c inspect_db_platform().VCursorOpen( '%s' ) -> %R\n",
                            input -> accession_short, rc );
                } else {
                    int64_t first;
                    uint64_t count;
                    rc = VCursorIdRange( curs, idx, &first, &count );
                    if ( 0 != rc ) {
                        ErrMsg( "inspector.c inspect_db_platform().VCursorIdRange( '%s' ) -> %R\n",
                                input -> accession_short, rc );
                    } else if ( count == 0 ) {
                        /* most likely the PLATFORM-column is static, that means count==0 */
                        first = 1;
                    }
                    if ( 0 == rc ) {
                        uint32_t elem_bits, boff, row_len;
                        uint8_t *values;
                        rc = VCursorCellDataDirect( curs, first, idx, &elem_bits, (const void **)&values, &boff, &row_len );
                        if ( 0 != rc ) {
                            ErrMsg( "inspector.c inspect_db_platform().VCursorCellDataDirect( '%s' ) -> %R\n",
                                    input -> accession_short, rc );
                        } else if ( NULL != values && 8 == elem_bits && 0 == boff && row_len > 0 ) {
                            *pf = values[ 0 ];
                            res = true;
                        } else {
                            ErrMsg( "inspector.c inspect_db_platform().VCursorCellDataDirect( '%s' ) -> unexpected\n",
                                    input -> accession_short );
                            
                        }
                    }
                }
            }
            {
                rc_t rc2 = VCursorRelease( curs );
                if ( 0 != rc2 ) {
                    ErrMsg( "inspector.c inspect_db_platform( '%s' ).VCursorRelease() -> %R\n",
                            input -> accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VTableRelease( tbl );
            if ( 0 != rc2 ) {
                ErrMsg( "inspector.c inspect_db_platform( '%s' ).VTableRelease() -> %R\n",
                        input -> accession_short, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return res;
}

static const char * PRIM_TBL_NAME = "PRIMARY_ALIGNMENT";
static const char * REF_TBL_NAME  = "REFERENCE";
static const char * SEQ_TBL_NAME  = "SEQUENCE";
static const char * CONS_TBL_NAME = "CONSENSUS";
static const char * ZMW_TBL_NAME  = "ZMW_METRICS";
static const char * PASS_TBL_NAME = "PASSES";

static acc_type_t inspect_db_type( const inspector_input_t * input,
                                   inspector_output_t * output ) {
    acc_type_t res = acc_none;
    const VDatabase * db = NULL;
    rc_t rc = inspector_open_db( input, &db );
    if ( 0 == rc ) {
        KNamelist * k_tables;
        rc = VDatabaseListTbl ( db, &k_tables );
        if ( 0 != rc ) {
            ErrMsg( "inspector.inspect_db_type().VDatabaseListTbl( '%s' ) -> %R\n",
                    input -> accession_short, rc );
        } else {
            VNamelist * tables;
            rc = VNamelistFromKNamelist ( &tables, k_tables );
            if ( 0 != rc ) {
                ErrMsg( "inspector.inspect_db_type.VNamelistFromKNamelist( '%s' ) -> %R\n",
                        input -> accession_short, rc );
            } else {
                if ( contains( tables, SEQ_TBL_NAME ) )
                {
                    bool has_prim_tbl = contains( tables, PRIM_TBL_NAME );
                    bool has_ref_tbl = contains( tables, REF_TBL_NAME );

                    res = acc_sra_db;
                    output -> seq . tbl_name = SEQ_TBL_NAME;
                    /* we have at least a SEQUENCE-table */
                    if ( has_prim_tbl && has_ref_tbl ) {
                        /* we have a SEQUENCE-, PRIMARY_ALIGNMENT-, and REFERENCE-table */
                        res = acc_csra;
                    } else {
                        uint8_t pf = SRA_PLATFORM_UNDEFINED;
                        if ( !inspect_db_platform( input, db, &pf ) ) {
                            pf = SRA_PLATFORM_UNDEFINED;                            
                        }
                        if ( SRA_PLATFORM_OXFORD_NANOPORE != pf )
                        {
                            bool has_cons_tbl = contains( tables, CONS_TBL_NAME );
                            bool has_zmw_tbl = contains( tables, ZMW_TBL_NAME );
                            bool has_pass_tbl = contains( tables, PASS_TBL_NAME );

                            if ( has_cons_tbl || has_zmw_tbl || has_pass_tbl ) {
                                if ( has_cons_tbl ) {
                                    output -> seq . tbl_name = CONS_TBL_NAME;                             
                                }
                                res = acc_pacbio;
                            } else {
                                /* last resort try to find out what the database-type is 
                                * ... the enums are in ncbi-vdb/interfaces/insdc/sra.h
                                */
                                if ( SRA_PLATFORM_PACBIO_SMRT == pf ) {
                                    res = acc_pacbio;
                                }
                            }
                        }
                    }
                }
            }
            {
                rc_t rc2 = KNamelistRelease ( k_tables );
                if ( 0 != rc2 ) {
                    ErrMsg( "inspector.inspect_db_type.KNamelistRelease( '%s' ) -> %R\n",
                            input -> accession_short, rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
        }
        {
            rc_t rc2 = VDatabaseRelease( db );
            if ( 0 != rc2 ) {
                ErrMsg( "inspector.inspect_db_type( '%s' ).VDatabaseRelease() -> %R\n",
                        input -> accession_short, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return res;
}

static acc_type_t inspect_path_type_and_seq_tbl_name( const inspector_input_t * input,
                                                      inspector_output_t * output ) {
    acc_type_t res = acc_none;
    int pt = VDBManagerPathType ( input -> vdb_mgr, "%s", input -> accession_path );
    output -> seq . tbl_name = NULL;  /* might be changed in case of PACBIO ( in case we handle PACBIO! ) */
    switch( pt )
    {
        case kptDatabase    :   res = inspect_db_type( input, output );
                                break;

        case kptPrereleaseTbl:
        case kptTable       :   res = acc_sra_flat;
                                break;
    }
    return res;
}

void unread_rc_info( bool show ) {
    rc_t rc;
    const char *filename;
    const char * funcname;
    uint32_t lineno;
    while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &lineno ) ) {
        if ( show ) {
            KOutMsg( "unread: %R %s %s %u\n", rc, filename, funcname, lineno );
        }
    }
}

static rc_t inspect_location_and_size( const inspector_input_t * input,
                                       inspector_output_t * output ) {
    rc_t rc;
    char resolved[ PATH_MAX ];
    
    /* try to resolve the path locally first */
    rc = resolve_accession( input -> accession_path, resolved, sizeof resolved, false ); /* above */
    if ( 0 == rc )
    {
        output -> is_remote = false;
        output -> acc_size = get_file_size( input -> dir, resolved, false );
        if ( 0 == output -> acc_size ) {
            // it could be the user specified a directory instead of a path to a file...
            char p[ PATH_MAX ];
            size_t written;
            rc = string_printf( p, sizeof p, &written, "%s/%s", resolved, input -> accession_short );
            if ( 0 == rc ) {
                output -> acc_size = get_file_size( input -> dir, p, false );
            }
        }
    }
    else
    {
        unread_rc_info( false ); /* get rid of stored rc-messages... */
        /* try to resolve the path remotely */
        rc = resolve_accession( input -> accession_path, resolved, sizeof resolved, true ); /* above */
        if ( 0 == rc )
        {
            output -> is_remote = true;
            output -> acc_size = get_file_size( input -> dir, resolved, true );
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

static rc_t inspect_extract_column_list( const VTable * tbl, const inspector_input_t * input, VNamelist ** columns ) {
    struct KNamelist * k_columns;
    rc_t rc = VTableListReadableColumns( tbl, &k_columns );
    if ( 0 != rc ) {
        ErrMsg( "inspector.c inspect_extract_column_list( '%s' ).VTableListReadableColumns() -> %R\n",
                input -> accession_short, rc );
    } else {
        rc = VNamelistFromKNamelist( columns, k_columns );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspect_extract_column_list( '%s' ).VNamelistFromKNamelist() -> %R\n",
                    input -> accession_short, rc );
        }
        {
            rc_t rc2 = KNamelistRelease ( k_columns );
            if ( 0 != rc2 ) {
                ErrMsg( "inspector.c inspect_extract_column_list( '%s' ).KNamelistRelease() -> %R\n",
                        input -> accession_short, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static bool inspect_list_contains( const VNamelist * lst, const char * item ) {
    int32_t idx;
    rc_t rc = VNamelistContainsStr( lst, item, &idx );
    return ( rc == 0 && idx >= 0 );
}

static rc_t inspect_seq_columns( const VTable * tbl, const inspector_input_t * input, inspector_seq_data_t * seq ) {
    VNamelist * columns;
    rc_t rc = inspect_extract_column_list( tbl, input, &columns );
    if ( 0 == rc ) {
        seq -> has_name_column = inspect_list_contains( columns, "NAME" );
        seq -> has_spot_group_column = inspect_list_contains( columns, "SPOT_GROUP" );
        VNamelistRelease( columns );
    }
    return rc;
}

static rc_t inspect_add_column( const VCursor * cur, uint32_t * id, const char * name ) {
    rc_t rc = VCursorAddColumn( cur, id, name );    
    if ( 0 != rc ) {
        ErrMsg( "inspector.c inspect_add_column( '%s' ) -> %R", name, rc );
    }
    return rc;
}

static rc_t inspect_read_u64( const VCursor * cur, int64_t row_id, uint32_t col_id, const char * name, uint64_t * dst ) {
    const uint64_t * u64_ptr;
    uint32_t elem_bits, boff, row_len;        
    rc_t rc = VCursorCellDataDirect( cur, row_id, col_id, &elem_bits, (const void **)&u64_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "inspector.c inspect_read_u64().VCursorCellDataDirect( %s ) -> %R", name, rc );
    } else {
        if ( row_len > 0 && 0 == boff && 64 == elem_bits ) { *dst = *u64_ptr; }
    }
    return rc;
}

static rc_t inspect_read_len( const VCursor * cur, int64_t row_id, uint32_t col_id, const char * name, uint32_t * dst ) {
    const char * c_ptr;
    uint32_t elem_bits, boff, row_len;        
    rc_t rc = VCursorCellDataDirect( cur, row_id, col_id, &elem_bits, (const void **)&c_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "inspector.c inspect_read_len().VCursorCellDataDirect( %s ) -> %R", name, rc );
    } else {
        *dst = row_len;
    }
    return rc;
}

static rc_t inspect_avg_len( const VCursor * cur, int64_t first_row, uint64_t row_count,
                            uint32_t col_id, const char * name, uint32_t * dst ) {
    rc_t rc = 0;
    uint32_t n = 10;
    uint32_t sum = 0;
    if ( row_count < n ) { n = (uint32_t)row_count; }
    if ( n > 0 ) {
        uint32_t i;
        for ( i = 0; 0 == rc && i < n; i++ ) {
            uint32_t value;
            rc = inspect_read_len( cur, first_row + i, col_id, name, &value );
            if ( 0 == rc ) { sum += value; }
        }
        *dst = ( sum / n );
    }
    return rc;
}

/* for SRA_READ_TYPE_TECHNICAL and SRA_READ_TYPE_BIOLOGICAL */
#ifndef _h_insdc_insdc_
#include <insdc/insdc.h>
#endif

static rc_t inspect_avg_read_type( const VCursor * cur, int64_t first_row, uint64_t row_count,
                            uint32_t col_id, uint32_t * bio, uint32_t * tech ) {
    rc_t rc = 0;
    uint32_t n = 10;
    uint32_t n_bio = 0;
    uint32_t n_tech = 0;
    if ( row_count < n ) { n = (uint32_t)row_count; }
    if ( n > 0 ) {
        uint32_t i;
        const uint8_t * data_ptr;
        uint32_t elem_bits, boff, row_len;        
        for ( i = 0; 0 == rc && i < n; i++ ) {
            rc = VCursorCellDataDirect( cur, first_row + i, col_id, &elem_bits, (const void **)&data_ptr, &boff, &row_len );
            if ( 0 != rc ) {
                ErrMsg( "inspector.c inspect_avg_read_type().VCursorCellDataDirect() -> %R", rc );
            } else {
                /* now we can count how many bio- and tech-reads we have in this row */
                uint32_t j;
                for ( j = 0; j < row_len; ++j ) {
                    uint8_t t = data_ptr[ j ];
                    if ( ( t & SRA_READ_TYPE_BIOLOGICAL ) == SRA_READ_TYPE_BIOLOGICAL ) {
                        n_bio++;
                    } else if ( ( t & SRA_READ_TYPE_TECHNICAL ) == SRA_READ_TYPE_TECHNICAL ) {
                        n_tech++;
                    }
                }
            }
        }
        *bio = ( n_bio / n );
        *tech = ( n_tech / n );
    }
    return rc;
}

static rc_t inspect_seq_data( const VTable * tbl, const inspector_input_t * input, inspector_seq_data_t * seq ) {
    const VCursor * cur;
    rc_t rc = VTableCreateCursorRead( tbl, &cur );    
    if ( 0 != 0 ) {
        ErrMsg( "inspector.c inspect_seq_data().VTableCreateCursorRead( '%s' ) -> %R", seq -> tbl_name, rc );
    } else {
        uint32_t id_read, id_name, id_spot_group, id_base_count, id_bio_base_count, id_spot_count, id_read_type;
       
        /* we need this, because all other columns may by static - and because of that do not reveal the row-count! */
        rc = inspect_add_column( cur, &id_read, "READ" );
        if ( 0 == rc && seq -> has_name_column ) { rc = inspect_add_column( cur, &id_name, "NAME" ); }
        if ( 0 == rc && seq -> has_spot_group_column ) { rc = inspect_add_column( cur, &id_spot_group, "SPOT_GROUP" ); }
        if ( 0 == rc ) { rc = inspect_add_column( cur, &id_base_count, "BASE_COUNT" ); }
        if ( 0 == rc ) { rc = inspect_add_column( cur, &id_bio_base_count, "BIO_BASE_COUNT" ); }
        if ( 0 == rc ) { rc = inspect_add_column( cur, &id_spot_count, "SPOT_COUNT" ); }
        if ( 0 == rc ) { rc = inspect_add_column( cur, &id_read_type, "READ_TYPE" ); }
        if ( 0 == rc ) {
            rc = VCursorOpen( cur );
            if ( 0 != rc ) {
                ErrMsg( "inspector.c inspect_seq_data().VCursorOpen( '%s' ) -> %R", seq -> tbl_name, rc );
            }
        }
        if ( 0 == rc ) {
            rc = VCursorIdRange( cur, id_read, &( seq -> first_row ), &( seq -> row_count ) );
            if ( 0 != rc ) {
                ErrMsg( "inspector.c inspect_seq_data().VCursorIdRange( '%s' ) -> %R", seq -> tbl_name, rc );
            }
        }
        if ( 0 == rc ) {
            rc = inspect_read_u64( cur, seq -> first_row, id_base_count, "BASE_COUNT",
                                   &( seq -> total_base_count ) );
        }
        if ( 0 == rc ) {
            rc = inspect_read_u64( cur, seq -> first_row, id_bio_base_count, "BIO_BASE_COUNT",
                                   &( seq -> bio_base_count ) );
        }
        if ( 0 == rc ) {
            rc = inspect_read_u64( cur, seq -> first_row, id_spot_count, "SPOT_COUNT",
                                   &( seq -> spot_count ) );
        }
        if ( 0 == rc && seq -> has_name_column ) {
            rc = inspect_avg_len( cur, seq -> first_row, seq -> row_count,
                                  id_name, "NAME", &( seq -> avg_name_len ) );
        }
        if ( 0 == rc && seq -> has_spot_group_column ) {
            rc = inspect_avg_len( cur, seq -> first_row, seq -> row_count,
                                  id_spot_group, "SPOT_GROUP", &( seq -> avg_spot_group_len ) );
        }
        if ( 0 == rc ) {
            rc = inspect_avg_read_type( cur, seq -> first_row, seq -> row_count,
                                  id_read_type, &( seq -> avg_bio_reads ), &( seq -> avg_tech_reads ) );                                        
        }
        {
            rc_t rc2 = VCursorRelease( cur );
            if ( rc2 != 0 ) {
                ErrMsg( "inspector.c inspect_seq_data().VCursorRelease( '%s' ) -> %R", seq -> tbl_name, rc2 );
                rc = ( rc == 0 ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

static rc_t inspect_seq_tbl( const VTable * tbl, const inspector_input_t * input, inspector_seq_data_t * seq ) {
    rc_t rc = inspect_seq_columns( tbl, input, seq );
    if ( 0 == rc ) {
        rc = inspect_seq_data( tbl, input, seq );
    }
    return rc;
}

static rc_t inspect_align_tbl( const VTable * tbl, const inspector_input_t * input, inspector_align_data_t * align ) {
    const VCursor * cur;
    rc_t rc = VTableCreateCursorRead( tbl, &cur );    
    if ( 0 != 0 ) {
        ErrMsg( "inspector.c inspect_align_tbl().VTableCreateCursorRead() -> %R", rc );
    } else {
        uint32_t id_read, id_base_count, id_bio_base_count, id_spot_count;
       
        /* we need this, because all other columns may by static - and because of that do not reveal the row-count! */
        rc = inspect_add_column( cur, &id_read, "READ" );
        if ( 0 == rc ) { rc = inspect_add_column( cur, &id_base_count, "BASE_COUNT" ); }
        if ( 0 == rc ) { rc = inspect_add_column( cur, &id_bio_base_count, "BIO_BASE_COUNT" ); }
        if ( 0 == rc ) { rc = inspect_add_column( cur, &id_spot_count, "SPOT_COUNT" ); }
        if ( 0 == rc ) {
            rc = VCursorOpen( cur );
            if ( 0 != rc ) {
                ErrMsg( "inspector.c inspect_align_tbl().VCursorOpen() -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            rc = VCursorIdRange( cur, id_read, &( align -> first_row ), &( align -> row_count ) );
            if ( 0 != rc ) {
                ErrMsg( "inspector.c inspect_align_tbl().VCursorIdRange() -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            rc = inspect_read_u64( cur, align -> first_row, id_base_count, "BASE_COUNT",
                                   &( align -> total_base_count ) );
        }
        if ( 0 == rc ) {
            rc = inspect_read_u64( cur, align -> first_row, id_bio_base_count, "BIO_BASE_COUNT",
                                   &( align -> bio_base_count ) );
        }
        if ( 0 == rc ) {
            rc = inspect_read_u64( cur, align -> first_row, id_spot_count, "SPOT_COUNT",
                                   &( align -> spot_count ) );
        }
        {
            rc_t rc2 = VCursorRelease( cur );
            if ( rc2 != 0 ) {
                ErrMsg( "inspector.c inspect_align_tbl().VCursorRelease() -> %R", rc2 );
                rc = ( rc == 0 ) ? rc2 : rc;
            }
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

/* it is a cSRA with alignments! */
static rc_t inspect_csra( const inspector_input_t * input, inspector_output_t * output ) {
    const VDatabase * db;
    rc_t rc = inspector_open_db( input, &db );
    if ( 0 == rc ) {
        const VTable * tbl;

        /* inspecting the SEQUENCE-table */
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", output -> seq . tbl_name );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspect_csra().VDatabaseOpenTableRead( '%s' ) -> %R",
                    output -> seq . tbl_name, rc );
        }
        else {
            rc = inspect_seq_tbl( tbl, input, &( output -> seq ) );
            rc = inspector_release_tbl( tbl, rc, "inspect_csra (seq)", input -> accession_short );
        }

        /* inspecting the PRIMARY_ALIGNMENT-table */
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspect_csra().VDatabaseOpenTableRead( PRIMARY_ALIGNMENT ) -> %R", rc );
        }
        else {
            rc = inspect_align_tbl( tbl, input, &( output -> align ) );
            rc = inspector_release_tbl( tbl, rc, "inspect_csra (align)", input -> accession_short );
        }
        rc = inspector_release_db( db, rc, "inspect_csra", input -> accession_short );
    }
    return rc;
}

/* it is a database without alignments! */
static rc_t inspect_sra_db( const inspector_input_t * input, inspector_output_t * output ) {
    const VDatabase * db;
    rc_t rc = inspector_open_db( input, &db );
    if ( 0 == rc ) {
        const VTable * tbl;
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", output -> seq . tbl_name );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspect_sra_db().VDatabaseOpenTableRead( '%s' ) -> %R",
                    output -> seq . tbl_name, rc );
        }
        else {
            rc = inspect_seq_tbl( tbl, input, &( output -> seq ) );
            rc = inspector_release_tbl( tbl, rc, "inspect_sra_db", input -> accession_short );
        }

        rc = inspector_release_db( db, rc, "inspect_sra_db", input -> accession_short );
    }
    return rc;
}

/* it is a flat table! */
static rc_t inspect_sra_flat( const inspector_input_t * input, inspector_output_t * output ) {
    const VTable * tbl;
    rc_t rc = inspector_open_tbl( input, &tbl );
    if ( 0 == rc ) {
        rc = inspect_seq_tbl( tbl, input, &( output -> seq ) );
        rc = inspector_release_tbl( tbl, rc, "inspect_sra_flat", input -> accession_short );
    }
    return 0;
}


/* ------------------------------------------------------------------------------------------- */

rc_t inspect( const inspector_input_t * input, inspector_output_t * output ) {
    rc_t rc = 0;
    if ( NULL == input ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspector.c inspect() : input is NULL -> %R", rc );
    } else if ( NULL == output ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspector.c inspect() : output is NULL -> %R", rc );
    } else if ( NULL == input -> dir ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspector.c inspect() : input->dir is NULL -> %R", rc );
    } else if ( NULL == input -> vdb_mgr ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspector.c inspect() : input->vdb_mgr is NULL -> %R", rc );
    } else if ( NULL == input -> accession_short ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspector.c inspect() : input->accession_short is NULL -> %R", rc );
    } else if ( NULL == input -> accession_path ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspector.c inspect() : input->accession_path is NULL -> %R", rc );
    } else {
        memset( output, 0, sizeof *output );
        rc = inspect_location_and_size( input, output );
        if ( 0 == rc ) {
            output -> acc_type = inspect_path_type_and_seq_tbl_name( input, output ); /* db or table? */

            switch( output -> acc_type ) {
                case acc_csra       : rc = inspect_csra( input, output ); break; /* above */
                case acc_sra_flat   : rc = inspect_sra_flat( input, output ); break; /* above */
                case acc_sra_db     : rc = inspect_sra_db( input, output ); break; /* above */
                default            : break;
            }

        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

static rc_t inspection_report_seq( const inspector_seq_data_t * seq ) {
    rc_t rc = KOutMsg( "... SEQ has NAME column = %s\n", yes_or_no( seq -> has_name_column ) );
    if ( 0 == rc ) {
        rc = KOutMsg( "... SEQ has SPOT_GROUP column = %s\n", yes_or_no( seq -> has_spot_group_column ) );
    }
    if ( 0 == rc && NULL != seq -> tbl_name ) {
        rc = KOutMsg( "... uses '%s' as sequence-table\n", seq -> tbl_name );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.first_row = %,ld\n", seq -> first_row );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.row_count = %,lu\n", seq -> row_count );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.spot_count = %,lu\n", seq -> spot_count );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.total_base_count = %,lu\n", seq -> total_base_count );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.bio_base_count = %,lu\n", seq -> bio_base_count );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.avg_name_len = %u\n", seq -> avg_name_len );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.avg_spot_group_len = %u\n", seq -> avg_spot_group_len );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.avg_bio_reads_per_spot = %u\n", seq -> avg_bio_reads );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "SEQ.avg_tech_reads_per_spot = %u\n", seq -> avg_tech_reads );
    }
    return rc;
}

static rc_t inspection_report_align( const inspector_align_data_t * align ) {
    rc_t rc = KOutMsg( "ALIGN.first_row = %,ld\n", align -> first_row );
    if ( 0 == rc ) {
        rc = KOutMsg( "ALIGN.row_count = %,lu\n", align -> row_count );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "ALIGN.spot_count = %,lu\n", align -> spot_count );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "ALIGN.total_base_count = %,lu\n", align -> total_base_count );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "ALIGN.bio_base_count = %,lu\n", align -> bio_base_count );
    }
    return rc;
}

rc_t inspection_report( const inspector_input_t * input, const inspector_output_t * output ) {
    rc_t rc = KOutMsg( "%s is %s\n",
                       input -> accession_short,
                       output -> is_remote ? "remote" : "local" );
    if ( 0 == rc ) {
        rc = KOutMsg( "... has a size of %,lu bytes\n", output -> acc_size );
    }
    if ( 0 == rc ) {
        switch( output -> acc_type ) {
            case acc_csra       : rc = KOutMsg( "... is cSRA with alignments\n" ); break;
            case acc_sra_flat   : rc = KOutMsg( "... is unaligned\n" ); break;
            case acc_sra_db     : rc = KOutMsg( "... is cSRA without alignments\n" ); break;
            case acc_pacbio     : rc = KOutMsg( "... is PACBIO\n" ); break;
            case acc_none       : rc = KOutMsg( "... is unknown\n" ); break;
        }
    }
    if ( 0 == rc ) {
        rc = inspection_report_seq( &( output -> seq ) );
    }
    if ( 0 == rc ) {
        rc = inspection_report_align( &( output -> align ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "\n" );
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

static size_t insp_est_base_count( const inspector_estimate_input_t * input ) {
    /* if we are skipping technical reads : we take the bio_base_count, otherwise the total_base_count 
       ( these 2 numbers can be the same for cSRA objects, they have no technical reads ) */
    if ( input -> skip_tech ) {
        return input -> insp -> seq . bio_base_count;
    }
    return input -> insp -> seq . total_base_count;
}

static uint32_t insp_est_reads_per_spot( const inspector_estimate_input_t * input ) {
    uint32_t res = input -> insp -> seq . avg_bio_reads;
    if ( !input -> skip_tech ) {
        res += input -> insp -> seq . avg_tech_reads;
    }
    return res;
}

static size_t insp_est_seq_defline_len( const inspector_estimate_input_t * input ) {
    defline_estimator_input_t defl_est_inp;

    defl_est_inp . acc = input -> acc;
    defl_est_inp . avg_name_len = input -> avg_name_len;
    defl_est_inp . row_count = input -> insp -> seq . row_count;
    if ( input -> insp -> seq .row_count > 0 ) {
        defl_est_inp . avg_seq_len = insp_est_base_count( input ) / input -> insp -> seq . row_count;
    } else {
        defl_est_inp . avg_seq_len = insp_est_base_count( input );
    }
    defl_est_inp . defline = input -> seq_defline;

    return estimate_defline_length( &defl_est_inp ); /* dflt_defline.c */
}

static size_t insp_est_qual_defline_len( const inspector_estimate_input_t * input ) {
    defline_estimator_input_t defl_est_inp;

    defl_est_inp . acc = input -> acc;
    defl_est_inp . avg_name_len = input -> avg_name_len;
    defl_est_inp . row_count = input -> insp -> seq . row_count;
    if ( input -> insp -> seq . row_count > 0 ) {
        defl_est_inp . avg_seq_len = insp_est_base_count( input ) / input -> insp -> seq . row_count;
    } else {
        defl_est_inp . avg_seq_len = insp_est_base_count( input );
    }
    defl_est_inp . defline = input -> qual_defline;    
    
    return estimate_defline_length( &defl_est_inp ); /* dflt_defline.c */
}

static size_t insp_est_out_size_whole_spot( const inspector_estimate_input_t * input, bool fasta ) {
    size_t res = input -> insp -> seq . total_base_count;   /* whole spot: always the total count */
    size_t l_seq_dl = insp_est_seq_defline_len( input );
    size_t l_qual_dl = 0;
    size_t row_count = input -> insp -> seq . row_count;

    if ( fasta ) {
        res += ( l_seq_dl * row_count );    /* add the length of the seq-defline for each row */
        res += ( row_count * 2 );           /* add the new-lines */
        return res;
    }
    
    l_qual_dl = insp_est_qual_defline_len( input );
    res *= 2;                               /* double the bases, because of quality */
    res += ( l_seq_dl * row_count );        /* add the length of the seq-defline for each row */
    res += ( l_qual_dl * row_count );       /* and the qual-defline for each row */
    res += ( row_count * 4  );              /* add the new-lines */
    return res;
}

static size_t insp_est_out_size_split_spot( const inspector_estimate_input_t * input, bool fasta ) {
    size_t res = insp_est_base_count( input );  /* depends on with/without technical reads */

    size_t l_seq_dl = insp_est_seq_defline_len( input );
    size_t l_qual_dl = 0;
    size_t row_count = input -> insp -> seq . row_count;

    size_t read_count = insp_est_reads_per_spot( input ); /* this depends on skip_tech:
                                                            we need to know how many tech. and bio. reads per spot */
    
    if ( fasta ) {
        res += ( l_seq_dl * row_count * read_count );    /* add the length of the seq-defline for each row and read */
        res += ( row_count * read_count * 2 );           /* add the new-lines */
        return res;
    }
    
    l_qual_dl = insp_est_qual_defline_len( input );
    res *= 2;                                           /* double the bases, because of quality */
    res += ( l_seq_dl * row_count * read_count );       /* add the length of the seq-defline for each row and read */
    res += ( l_qual_dl * row_count * read_count );      /* and the qual-defline for each row */
    res += ( row_count * read_count * 4  );             /* add the new-lines */
    return res;
}

size_t inspector_estimate_output_size( const inspector_estimate_input_t * input ) {
    size_t res = 0;
    switch( input -> fmt ) {
        case ft_unknown                 : break;
        case ft_fastq_whole_spot        : res = insp_est_out_size_whole_spot( input, false ); break;
        case ft_fastq_split_spot        : res = insp_est_out_size_split_spot( input, false ); break;
        case ft_fastq_split_file        : res = insp_est_out_size_split_spot( input, false ); break;
        case ft_fastq_split_3           : res = insp_est_out_size_split_spot( input, false ); break;
        case ft_fasta_whole_spot        : res = insp_est_out_size_whole_spot( input, true ); break;
        case ft_fasta_split_spot        : res = insp_est_out_size_split_spot( input, true ); break;
        case ft_fasta_split_file        : res = insp_est_out_size_split_spot( input, true ); break;
        case ft_fasta_split_3           : res = insp_est_out_size_split_spot( input, true ); break;
        case ft_fasta_us_split_spot     : res = insp_est_out_size_split_spot( input, false ); break;
    }
    return res;
}
