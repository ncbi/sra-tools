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

#include <vdb/report.h> /* ReportResetTable */

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

static const char * insp_empty = ".";

static rc_t insp_release_tbl( const VTable * tbl, rc_t rc,
                              const char * fname,
                              const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != tbl ) {
        rc_t rc2 = VTableRelease( tbl );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).VTableRelease() -> %R",
                    fname, s, rc2 );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

static rc_t insp_release_db( const VDatabase * db, rc_t rc,
                             const char * fname,
                             const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != db ) {
        rc_t rc2 = VDatabaseRelease( db );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).VDatabaseRelease() -> %R",
                    fname, s, rc2 );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

static rc_t insp_release_curs( const VCursor * curs, rc_t rc,
                              const char * fname,
                              const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != curs ) {
        rc_t rc2 = VCursorRelease( curs );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).VCursorRelease() -> %R\n",
                    fname, s, rc2 );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

static rc_t insp_release_KNamelist( const struct KNamelist * lst, rc_t rc,
                                    const char * fname,
                                    const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != lst ) {
        rc_t rc2 = KNamelistRelease( lst );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).KNamelistRelease() -> %R\n",
                    fname, s, rc2 );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

static rc_t insp_release_VNamelist( const VNamelist * lst, rc_t rc,
                                    const char * fname,
                                    const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != lst ) {
        rc_t rc2 = VNamelistRelease( lst );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).VNamelistRelease() -> %R\n",
                    fname, s, rc2 );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}
                                         
static rc_t insp_release_VPath( const VPath * vpath, rc_t rc,
                                const char * fname,
                                const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != vpath ) {
        rc_t rc2 = VPathRelease( vpath );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).VPathRelease() -> %R\n",
                    fname, s, rc2 );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

static rc_t insp_release_VFSMgr( const VFSManager * mgr, rc_t rc,
                                 const char * fname,
                                 const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != mgr ) {
        rc_t rc2 = VFSManagerRelease( mgr );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).VFSManagerRelease() -> %R\n",
                    fname, s, rc );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

static rc_t insp_release_KNSMgr( const KNSManager * mgr, rc_t rc,
                                 const char * fname,
                                 const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != mgr ) {
        rc_t rc2 = KNSManagerRelease( mgr );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;
            ErrMsg( "%s( '%s' ).KNSManagerRelease() -> %R\n",
                    fname, s, rc );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

static rc_t insp_release_VResolver( const VResolver * resolver, rc_t rc,
                                    const char * fname,
                                    const char * acc ) {
    rc_t rc1 = rc;
    if ( NULL != resolver ) {
        rc_t rc2 = VResolverRelease( resolver );
        if ( 0 != rc2 ) {
            const char * s = ( NULL == acc ) ? insp_empty : acc;            
            ErrMsg( "%s( '%s' ).VResolverRelease() -> %R\n",
                    fname, s, rc2 );
            rc1 = ( 0 == rc1 ) ? rc2 : rc1;
        }
    }
    return rc1;
}

/* ------------------------------------------------------------------------------ */

static const char * ACC_TYPE_CSRA_STR = "cSRA";
static const char * ACC_TYPE_SRA_FLAT_STR = "SRA-flat";
static const char * ACC_TYPE_SRA_DB_STR = "SRA-db";
static const char * ACC_TYPE_SRA_PACBIO_BAM_STR = "SRA-pacbio-bam";
static const char * ACC_TYPE_SRA_PACBIO_NATIVE_STR = "SRA-pacbio-native";
static const char * ACC_TYPE_SRA_NONE_STR = "SRA-none";

static const char * insp_acc_type_to_string( acc_type_t acc_type ) {
    const char * res;
    switch( acc_type ) {
        case acc_csra          : res = ACC_TYPE_CSRA_STR; break;
        case acc_sra_flat      : res = ACC_TYPE_SRA_FLAT_STR; break;
        case acc_sra_db        : res = ACC_TYPE_SRA_DB_STR; break;
        case acc_pacbio_bam    : res = ACC_TYPE_SRA_PACBIO_BAM_STR; break;
        case acc_pacbio_native : res = ACC_TYPE_SRA_PACBIO_NATIVE_STR; break;
        case acc_none          : res = ACC_TYPE_SRA_NONE_STR;break;
    }
    return res;
}

/* ------------------------------------------------------------------------------ */

static rc_t insp_extract_column_list( const VTable * tbl,
                                      const insp_input_t * input,
                                      VNamelist ** columns ) {
    struct KNamelist * k_columns;
    rc_t rc = VTableListReadableColumns( tbl, &k_columns );
    if ( 0 != rc ) {
        ErrMsg( "insp_extract_column_list( '%s' ).VTableListReadableColumns() -> %R\n",
                input -> accession_short, rc );
    } else {
        rc = VNamelistFromKNamelist( columns, k_columns );
        if ( 0 != rc ) {
            ErrMsg( "insp_extract_column_list( '%s' ).VNamelistFromKNamelist() -> %R\n",
                    input -> accession_short, rc );
        }
        rc = insp_release_KNamelist( k_columns, rc, "inspect_extract_column_list",
                                     input -> accession_short );
    }
    return rc;
}

static const char * insp_extract_acc_from_path_v1( const char * s ) {
    const char * res = NULL;
    if ( ( NULL != s ) && ( !hlp_ends_in_slash( s ) ) ) {
        size_t size = string_size ( s );
        char * slash = string_rchr ( s, size, '/' );
        if ( NULL == slash ) {
            if ( hlp_ends_in_sra( s ) ) {
                res = string_dup ( s, size - 4 );
            } else {
                res = string_dup ( s, size );
            }
        } else {
            char * tmp = slash + 1;
            if ( hlp_ends_in_sra( tmp ) ) {
                res = string_dup ( tmp, string_size ( tmp ) - 4 );
            } else {
                res = string_dup ( tmp, string_size ( tmp ) );
            }
        }
    }
    return res;
}

static const char * insp_extract_from_vpath( const VPath * vpath ) {
    const char * res = NULL;
    char buffer[ 1024 ];
    size_t num_read;
    rc_t rc = VPathReadPath( vpath, buffer, sizeof buffer, &num_read );
    if ( 0 != rc ) {
        ErrMsg( "insp_extract_from_vpath().VPathReadPath() -> %R", rc );
    } else {
        res = string_dup( buffer, num_read );
    }
    return res;
}

static const char * insp_extract_from_path( const VFSManager * mgr, const char * path ) {
    const char * res = NULL;    
    VPath * vpath;
    rc_t rc = VFSManagerMakePath ( mgr, &vpath, "%s", path );
    if ( 0 == rc ) {
        VPath * acc_or_oid = NULL;
        rc = VFSManagerExtractAccessionOrOID( mgr, &acc_or_oid, vpath );
        if ( 0 == rc ) {
            res = insp_extract_from_vpath( acc_or_oid );
            insp_release_VPath( acc_or_oid, rc, "insp_extract_from_path", path );
        }
        insp_release_VPath( vpath, rc, "insp_extract_from_path", path );
    }
    return res;
}

static const char * insp_extract_acc_from_path_v2( const char * path ) {
    const char * res = NULL;
    VFSManager * mgr;
    rc_t rc = VFSManagerMake( &mgr );
    if ( 0 != rc ) {
        ErrMsg( "insp_extract_acc_from_path_v2( '%s' ).VFSManagerMake() -> %R",
                path, rc );
    } else {
        res = insp_extract_from_path( mgr, path );
        if ( NULL == res ) {
            /* remove trailing slash[es] and try again */
            char buffer[ PATH_MAX ] = "";
            size_t l = string_copy_measure( buffer, sizeof buffer, path );
            char * basename = buffer;
            while ( l > 0 && strchr( "\\/", basename[ l - 1 ]) != NULL ) {
                basename[ --l ] = '\0';
            }
            res = insp_extract_from_path( mgr, buffer );
        }
        insp_release_VFSMgr( mgr, rc, "insp_extract_acc_from_path_v2", path );
    }
    return res;
}


const char * insp_extract_acc_from_path( const char * s ) {
    const char * res = insp_extract_acc_from_path_v2( s );
    // in case something goes wrong with acc-extraction via VFS-manager
    if ( NULL == res ) {
        res = insp_extract_acc_from_path_v1( s );
    }
    return res;
}

static rc_t insp_on_file_visit( const KDirectory *dir, uint32_t type, const char *name, void *data ) {
    rc_t rc = 0;
    if ( kptFile == type ) {
        uint64_t size;
        rc = KDirectoryFileSize( dir, &size, "%s", name );
        if ( 0 == rc ) {
            uint64_t * res = data;
            *res += size;
        }
    }
    return rc;
}

static uint64_t insp_get_file_size( const KDirectory * dir, const char * path, bool remotely )
{
    rc_t rc = 0;
    uint64_t res = 0;

    if ( remotely ) {
        KNSManager * kns_mgr;
        rc = KNSManagerMake( &kns_mgr );
        if ( 0 == rc )
        {
            const KFile * f = NULL;
            rc = KNSManagerMakeHttpFile( kns_mgr, &f, NULL, 0x01010000, "%s", path );
            if ( 0 == rc ) {
                rc = KFileSize ( f, &res );
                KFileRelease( f );
            }
            rc = insp_release_KNSMgr( kns_mgr, rc, "get_file_size", path );
        }
    } else {
        /* we first have to check if the path is a directory or a file! */
        uint32_t pt = KDirectoryPathType( dir, "%s", path );
        if ( kptDir == pt ) {
            /* the path is a directory: adding size of each file in it */
            rc = KDirectoryVisit( dir, true, insp_on_file_visit, &res, "%s", path );
        } else if ( kptFile == pt ) {
            /* the path is a file: retrieve its size */
            rc = KDirectoryFileSize( dir, &res, "%s", path );
        }
    }

    if ( 0 != rc ) {
        ErrMsg( "insp_get_file_size( '%s', remotely = %s ) -> %R",
                path,
                hlp_yes_or_no( remotely ),
                rc );
    }
    return res;
}

static bool insp_starts_with( const char *a, const char *b )
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

static rc_t insp_resolve_accession( const char * acc, char * dst, size_t dst_size, bool * remotely )
{
    VFSManager * vfs_mgr;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    dst[ 0 ] = 0;
    if ( 0 == rc ) {
        const VPath * vpath = NULL;
        rc = VFSManagerResolve(vfs_mgr, acc, &vpath);
        if ( 0 == rc && ( NULL != vpath ) )
        {
                    const String * path = NULL;
                    rc = VPathMakeString(vpath, &path);

                    if ( 0 == rc && NULL != path ) {
                        string_copy ( dst, dst_size, path -> addr, path -> size );
                        dst[ path -> size ] = 0;
                        StringWhack ( path );
                    }

                    if (0 == rc) {
                        assert(remotely);
                        *remotely = VPathIsRemote(vpath);
                    }
                    rc = insp_release_VPath( vpath, rc,
                        "insp_resolve_accession", acc );
        }
        else
            ErrMsg("cannot find '%s' -> %R", acc, rc);
        rc = insp_release_VFSMgr( vfs_mgr, rc, "insp_resolve_accession", acc );
    }

    if ( 0 == rc && insp_starts_with( dst, "ncbi-acc:" ) )
    {
        size_t l = string_size ( dst );
        memmove( dst, &( dst[ 9 ] ), l - 9 );
        dst[ l - 9 ] = 0;
    }
    return rc;
}

rc_t insp_path_to_vpath( const char * path, VPath ** vpath ) {
    VFSManager * vfs_mgr = NULL;
    rc_t rc = VFSManagerMake( &vfs_mgr );
    if ( 0 != rc ) {
        ErrMsg( "insp_path_to_vpath().VFSManagerMake( %s ) -> %R\n", path, rc );
    } else {
        VPath * in_path = NULL;
        rc = VFSManagerMakePath( vfs_mgr, &in_path, "%s", path );
        if ( 0 != rc ) {
            ErrMsg( "insp_path_to_vpath().VFSManagerMakePath( %s ) -> %R\n", path, rc );
        } else {
            rc = VFSManagerResolvePath( vfs_mgr, vfsmgr_rflag_kdb_acc, in_path, vpath );
            if ( 0 != rc ) {
                ErrMsg( "insp_path_to_vpath().VFSManagerResolvePath( %s ) -> %R\n", path, rc );
            }
            rc = insp_release_VPath( in_path, rc, "inspector_path_to_vpath", path );
        }
        rc = insp_release_VFSMgr( vfs_mgr, rc, "inspector_path_to_vpath", path );
    }
    return rc;
}

static rc_t insp_open_db( const insp_input_t * input, const VDatabase ** db ) {
    VPath * v_path = NULL;
    rc_t rc = insp_path_to_vpath( input -> accession_path, &v_path );
    if ( 0 == rc ) {
        rc = VDBManagerOpenDBReadVPath( input -> vdb_mgr, db, NULL, v_path );
        if ( 0 != rc ) {
            ErrMsg( "insp_open_db().VDBManagerOpenDBReadVPath( '%s' ) -> %R\n",
                    input -> accession_path, rc );
        }
        else {
            assert( db );
            ReportResetDatabase( input -> accession_path, *db );
        }
        rc = insp_release_VPath( v_path, rc, "inspector_open_db", input -> accession_short );
    }
    return rc;
}

static rc_t insp_open_tbl( const insp_input_t * input, const VTable ** tbl ) {
    VPath * v_path = NULL;
    rc_t rc = insp_path_to_vpath( input -> accession_path, &v_path );
    if ( 0 == rc ) {
        rc = VDBManagerOpenTableReadVPath( input -> vdb_mgr, tbl, NULL, v_path );
        if ( 0 != rc ) {
            ErrMsg( "insp_open_tbl().VDBManagerOpenTableReadVPath( '%s' ) -> %R\n",
                    input -> accession_path, rc );
        }
        else {
            assert( tbl );
            ReportResetTable( input -> accession_path, *tbl );
        }
        rc = insp_release_VPath( v_path, rc, "insp_open_tbl", input -> accession_short );
    }
    return rc;
}

static bool insp_contains( VNamelist * tables, const char * table ) {
    uint32_t found = 0;
    rc_t rc = VNamelistIndexOf( tables, table, &found );
    return ( 0 == rc );
}

static bool insp_db_platform( const insp_input_t * input, const VDatabase * db, uint8_t * pf ) {
    bool res = false;
    const VTable * tbl = NULL;
    rc_t rc = VDatabaseOpenTableRead ( db, &tbl, "SEQUENCE" );
    if ( 0 != rc ) {
        ErrMsg( "insp_db_platform().VDatabaseOpenTableRead( '%s' ) -> %R\n",
                 input -> accession_short, rc );
    } else {
        const VCursor * curs = NULL;
        rc = VTableCreateCursorRead( tbl, &curs );
        if ( 0 != rc ) {
            ErrMsg( "insp_db_platform().VTableCreateCursor( '%s' ) -> %R\n",
                    input -> accession_short, rc );
        } else {
            uint32_t idx;
            rc = VCursorAddColumn( curs, &idx, "PLATFORM" );
            if ( 0 != rc ) {
                ErrMsg( "insp_db_platform().VCursorAddColumn( '%s', PLATFORM ) -> %R\n",
                        input -> accession_short, rc );
            } else {
                rc = VCursorOpen( curs );
                if ( 0 != rc ) {
                    ErrMsg( "insp_db_platform().VCursorOpen( '%s' ) -> %R\n",
                            input -> accession_short, rc );
                } else {
                    int64_t first;
                    uint64_t count;
                    rc = VCursorIdRange( curs, idx, &first, &count );
                    if ( 0 != rc ) {
                        ErrMsg( "insp_db_platform().VCursorIdRange( '%s' ) -> %R\n",
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
                            ErrMsg( "insp_db_platform().VCursorCellDataDirect( '%s' ) -> %R\n",
                                    input -> accession_short, rc );
                        } else if ( NULL != values && 8 == elem_bits && 0 == boff && row_len > 0 ) {
                            *pf = values[ 0 ];
                            res = true;
                        } else {
                            ErrMsg( "insp_db_platform().VCursorCellDataDirect( '%s' ) -> unexpected\n",
                                    input -> accession_short );
                        }
                    }
                }
            }
            rc = insp_release_curs( curs, rc, "insp_db_platform", input -> accession_short );
        }
        rc = insp_release_tbl( tbl, rc, "insp_db_platform", input -> accession_short );
    }
    return res;
}

static const char * PRIM_TBL_NAME = "PRIMARY_ALIGNMENT";
static const char * REF_TBL_NAME  = "REFERENCE";
static const char * SEQ_TBL_NAME  = "SEQUENCE";
static const char * CONS_TBL_NAME = "CONSENSUS";
static const char * ZMW_TBL_NAME  = "ZMW_METRICS";
static const char * PASS_TBL_NAME = "PASSES";

static acc_type_t insp_db_type( const insp_input_t * input,
                                insp_output_t * output ) {
    acc_type_t res = acc_none;
    const VDatabase * db = NULL;
    rc_t rc = insp_open_db( input, &db );
    if ( 0 == rc ) {
        KNamelist * k_tables;
        rc = VDatabaseListTbl ( db, &k_tables );
        if ( 0 != rc ) {
            ErrMsg( "insp_db_type().VDatabaseListTbl( '%s' ) -> %R\n",
                    input -> accession_short, rc );
        } else {
            VNamelist * tables;
            rc = VNamelistFromKNamelist ( &tables, k_tables );
            if ( 0 != rc ) {
                ErrMsg( "insp_db_type.VNamelistFromKNamelist( '%s' ) -> %R\n",
                        input -> accession_short, rc );
            } else {
                if ( insp_contains( tables, SEQ_TBL_NAME ) )
                {
                    bool has_prim_tbl = insp_contains( tables, PRIM_TBL_NAME );
                    bool has_ref_tbl = insp_contains( tables, REF_TBL_NAME );

                    res = acc_sra_db;
                    output -> seq . tbl_name = SEQ_TBL_NAME;
                    /* we have at least a SEQUENCE-table */
                    if ( has_prim_tbl && has_ref_tbl ) {
                        /* we have a SEQUENCE-, PRIMARY_ALIGNMENT-, and REFERENCE-table */
                        res = acc_csra;
                    } else {
                        uint8_t pf = SRA_PLATFORM_UNDEFINED;
                        if ( !insp_db_platform( input, db, &pf ) ) { /* above */
                            pf = SRA_PLATFORM_UNDEFINED;
                        }

                        if ( SRA_PLATFORM_OXFORD_NANOPORE == pf ) {
                            bool has_cons_tbl = insp_contains( tables, CONS_TBL_NAME );
                            if ( has_cons_tbl ) {
                                output -> seq . tbl_name = CONS_TBL_NAME;
                            }
                        }  else {
                            bool has_cons_tbl = insp_contains( tables, CONS_TBL_NAME );
                            bool has_zmw_tbl = insp_contains( tables, ZMW_TBL_NAME );
                            bool has_pass_tbl = insp_contains( tables, PASS_TBL_NAME );

                            if ( has_cons_tbl || has_zmw_tbl || has_pass_tbl ) {
                                if ( has_cons_tbl ) {
                                    if ( NULL == input -> requested_seq_tbl_name ) {
                                        output -> seq . tbl_name = CONS_TBL_NAME;
                                    } else {
                                        output -> seq . tbl_name = input -> requested_seq_tbl_name;                                        
                                    }
                                }
                                res = acc_pacbio_native;
                            } else {
                                /* last resort try to find out what the database-type is
                                * ... the enums are in ncbi-vdb/interfaces/insdc/sra.h
                                */
                                if ( SRA_PLATFORM_PACBIO_SMRT == pf ) {
                                    res = acc_pacbio_bam;
                                }
                            }
                        }
                    }
                }
				VNamelistRelease( tables );
            }
            rc = insp_release_KNamelist( k_tables, rc, "insp_db_type", input -> accession_short );
        }
        rc = insp_release_db( db, rc, "insp_db_type", input -> accession_short );
    }
    return res;
}

static acc_type_t insp_tbl_type( const insp_input_t * input,
                                 insp_output_t * output ) {
    acc_type_t res = acc_none;
    const VTable * tbl = NULL;
    rc_t rc = insp_open_tbl( input, &tbl );
    if ( 0 == rc ) {
        VNamelist * columns;
        rc = insp_extract_column_list( tbl, input, &columns );
        if ( 0 == rc ) {

            res = acc_sra_flat;

            rc = insp_release_VNamelist( columns, rc, "insp_tbl_type",
                                         input -> accession_short );
        }
        rc = insp_release_tbl( tbl, rc, "insp_tbl_type",
                               input -> accession_short );
    }
    return res;
}

static acc_type_t insp_path_type_and_seq_tbl_name( const insp_input_t * input,
                                                   insp_output_t * output ) {
    acc_type_t res = acc_none;
    int pt = VDBManagerPathType ( input -> vdb_mgr, "%s", input -> accession_path );
    output -> seq . tbl_name = NULL;  /* might be changed in case of PACBIO ( in case we handle PACBIO! ) */
    switch( pt )
    {
        case kptDatabase    :   res = insp_db_type( input, output );
                                break;

        case kptPrereleaseTbl:
        case kptTable       :   res = insp_tbl_type( input, output );
                                break;
    }
    return res;
}

static rc_t insp_location_and_size( const insp_input_t * input,
                                    insp_output_t * output ) {
    rc_t rc;
    char resolved[ PATH_MAX ];
    bool remotely = false;

    /* try to resolve the path locally first */
    rc = insp_resolve_accession( input -> accession_path, resolved, sizeof resolved, &remotely ); /* above */
    if ( 0 == rc )
    {
      if (!remotely) {
          /* found locally */
        output -> is_remote = false;
        output -> acc_size = insp_get_file_size( input -> dir, resolved, false );
        if ( 0 == output -> acc_size ) {
            // it could be the user specified a directory instead of a path to a file...
            char p[ PATH_MAX ];
            size_t written;
            rc = string_printf( p, sizeof p, &written, "%s/%s", resolved, input -> accession_short );
            if ( 0 == rc ) {
                output -> acc_size = insp_get_file_size( input -> dir, p, false );
            }
        }
      }
      else
      {
        /* found remotely */
        hlp_unread_rc_info( false ); /* get rid of stored rc-messages... */
        if ( 0 == rc )
        {
            output -> is_remote = true;
            output -> acc_size = insp_get_file_size( input -> dir, resolved, true );
        }
      }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

static bool insp_list_contains( const VNamelist * lst, const char * item ) {
    int32_t idx;
    rc_t rc = VNamelistContainsStr( lst, item, &idx );
    return ( rc == 0 && idx >= 0 );
}

static rc_t insp_seq_columns( const VTable * tbl, const insp_input_t * input, insp_seq_data_t * seq ) {
    VNamelist * columns;
    rc_t rc = insp_extract_column_list( tbl, input, &columns );
    if ( 0 == rc ) {
        seq -> has_name_column = insp_list_contains( columns, "NAME" );
        seq -> has_spot_group_column = insp_list_contains( columns, "SPOT_GROUP" );
        seq -> has_read_type_column = insp_list_contains( columns, "READ_TYPE" );
        seq -> has_quality_column = insp_list_contains( columns, "QUALITY" );
        seq -> has_read_column = insp_list_contains( columns, "READ" );
        rc = insp_release_VNamelist( columns, rc, "inspect_seq_columns",
                                     input -> accession_short );
    }
    return rc;
}

static rc_t insp_add_column( const VCursor * cur, uint32_t * id, const char * name ) {
    rc_t rc = VCursorAddColumn( cur, id, name );
    if ( 0 != rc ) {
        ErrMsg( "insp_add_column( '%s' ) -> %R", name, rc );
    }
    return rc;
}

static rc_t insp_read_u64( const VCursor * cur, int64_t row_id, uint32_t col_id, const char * name, uint64_t * dst ) {
    const uint64_t * u64_ptr;
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( cur, row_id, col_id, &elem_bits, (const void **)&u64_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "insp_read_u64().VCursorCellDataDirect( %s ) -> %R", name, rc );
    } else {
        if ( row_len > 0 && 0 == boff && 64 == elem_bits ) { *dst = *u64_ptr; }
    }
    return rc;
}

static rc_t insp_read_len( const VCursor * cur, int64_t row_id, uint32_t col_id, const char * name, uint32_t * dst ) {
    const char * c_ptr;
    uint32_t elem_bits, boff, row_len;
    rc_t rc = VCursorCellDataDirect( cur, row_id, col_id, &elem_bits, (const void **)&c_ptr, &boff, &row_len );
    if ( 0 != rc ) {
        ErrMsg( "insp_read_len().VCursorCellDataDirect( %s ) -> %R", name, rc );
    } else {
        *dst = row_len;
    }
    return rc;
}

static rc_t insp_avg_len( const VCursor * cur, int64_t first_row, uint64_t row_count,
                          uint32_t col_id, const char * name, uint32_t * dst ) {
    rc_t rc = 0;
    uint32_t n = 10;
    uint32_t sum = 0;
    if ( row_count < n ) { n = (uint32_t)row_count; }
    if ( n > 0 ) {
        uint32_t i;
        for ( i = 0; 0 == rc && i < n; i++ ) {
            uint32_t value;
            rc = insp_read_len( cur, first_row + i, col_id, name, &value );
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

static rc_t insp_avg_read_type( const VCursor * cur, int64_t first_row, uint64_t row_count,
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
                ErrMsg( "insp_avg_read_type().VCursorCellDataDirect() -> %R", rc );
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

static rc_t insp_seq_data( const VTable * tbl, const insp_input_t * input, insp_seq_data_t * seq ) {
    const VCursor * cur;
    rc_t rc = VTableCreateCursorRead( tbl, &cur );
    if ( 0 != 0 ) {
        ErrMsg( "insp_seq_data().VTableCreateCursorRead( '%s' ) -> %R", seq -> tbl_name, rc );
    } else {
        uint32_t id_read, id_name, id_spot_group, id_base_count, id_bio_base_count, id_spot_count, id_read_type;

        /* we need this, because all other columns may by static - and because of that do not reveal the row-count! */
        rc = insp_add_column( cur, &id_read, "READ" );
        if ( 0 == rc && seq -> has_name_column ) { rc = insp_add_column( cur, &id_name, "NAME" ); }
        if ( 0 == rc && seq -> has_spot_group_column ) { rc = insp_add_column( cur, &id_spot_group, "SPOT_GROUP" ); }
        if ( 0 == rc ) { rc = insp_add_column( cur, &id_base_count, "BASE_COUNT" ); }
        if ( 0 == rc ) { rc = insp_add_column( cur, &id_bio_base_count, "BIO_BASE_COUNT" ); }
        if ( 0 == rc ) { rc = insp_add_column( cur, &id_spot_count, "SPOT_COUNT" ); }
        if ( 0 == rc && seq -> has_read_type_column ) { rc = insp_add_column( cur, &id_read_type, "READ_TYPE" ); }
        if ( 0 == rc ) {
            rc = VCursorOpen( cur );
            if ( 0 != rc ) {
                ErrMsg( "insp_seq_data().VCursorOpen( '%s' ) -> %R", seq -> tbl_name, rc );
            }
        }
        if ( 0 == rc ) {
            rc = VCursorIdRange( cur, id_read, &( seq -> first_row ), &( seq -> row_count ) );
            if ( 0 != rc ) {
                ErrMsg( "insp_seq_data().VCursorIdRange( '%s' ) -> %R", seq -> tbl_name, rc );
            }
        }
        if ( 0 == rc ) {
            rc = insp_read_u64( cur, seq -> first_row, id_base_count, "BASE_COUNT",
                                &( seq -> total_base_count ) );
        }
        if ( 0 == rc ) {
            rc = insp_read_u64( cur, seq -> first_row, id_bio_base_count, "BIO_BASE_COUNT",
                                &( seq -> bio_base_count ) );
        }
        if ( 0 == rc ) {
            rc = insp_read_u64( cur, seq -> first_row, id_spot_count, "SPOT_COUNT",
                                &( seq -> spot_count ) );
        }
        if ( 0 == rc && seq -> has_name_column ) {
            rc = insp_avg_len( cur, seq -> first_row, seq -> row_count,
                               id_name, "NAME", &( seq -> avg_name_len ) );
        }
        if ( 0 == rc && seq -> has_spot_group_column ) {
            rc = insp_avg_len( cur, seq -> first_row, seq -> row_count,
                               id_spot_group, "SPOT_GROUP", &( seq -> avg_spot_group_len ) );
        }
        if ( 0 == rc && seq -> has_read_type_column ) {
            rc = insp_avg_read_type( cur, seq -> first_row, seq -> row_count,
                        id_read_type, &( seq -> avg_bio_reads ), &( seq -> avg_tech_reads ) );
        }
        rc = insp_release_curs( cur, rc, "inspect_seq_data", seq -> tbl_name );
    }
    return rc;
}

static rc_t insp_seq_tbl( const VTable * tbl, const insp_input_t * input, insp_seq_data_t * seq ) {
    rc_t rc = insp_seq_columns( tbl, input, seq );
    if ( 0 == rc ) {
        rc = insp_seq_data( tbl, input, seq );
    }
    return rc;
}

static rc_t insp_align_tbl( const VTable * tbl, const insp_input_t * input, insp_align_data_t * align ) {
    const VCursor * cur;
    rc_t rc = VTableCreateCursorRead( tbl, &cur );
    if ( 0 != 0 ) {
        ErrMsg( "insp_align_tbl().VTableCreateCursorRead() -> %R", rc );
    } else {
        uint32_t id_read, id_base_count, id_bio_base_count, id_spot_count;

        /* we need this, because all other columns may by static - and because of that do not reveal the row-count! */
        rc = insp_add_column( cur, &id_read, "READ" );
        if ( 0 == rc ) { rc = insp_add_column( cur, &id_base_count, "BASE_COUNT" ); }
        if ( 0 == rc ) { rc = insp_add_column( cur, &id_bio_base_count, "BIO_BASE_COUNT" ); }
        if ( 0 == rc ) { rc = insp_add_column( cur, &id_spot_count, "SPOT_COUNT" ); }
        if ( 0 == rc ) {
            rc = VCursorOpen( cur );
            if ( 0 != rc ) {
                ErrMsg( "insp_align_tbl().VCursorOpen() -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            rc = VCursorIdRange( cur, id_read, &( align -> first_row ), &( align -> row_count ) );
            if ( 0 != rc ) {
                ErrMsg( "insp_align_tbl().VCursorIdRange() -> %R", rc );
            }
        }
        if ( 0 == rc ) {
            rc = insp_read_u64( cur, align -> first_row, id_base_count, "BASE_COUNT",
                                   &( align -> total_base_count ) );
        }
        if ( 0 == rc ) {
            rc = insp_read_u64( cur, align -> first_row, id_bio_base_count, "BIO_BASE_COUNT",
                                   &( align -> bio_base_count ) );
        }
        if ( 0 == rc ) {
            rc = insp_read_u64( cur, align -> first_row, id_spot_count, "SPOT_COUNT",
                                   &( align -> spot_count ) );
        }
        rc = insp_release_curs( cur, rc, "insp_align_data", input -> accession_short );
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

/* it is a cSRA with alignments! */
static rc_t insp_csra( const insp_input_t * input, insp_output_t * output ) {
    const VDatabase * db;
    rc_t rc = insp_open_db( input, &db );
    if ( 0 == rc ) {
        const VTable * tbl;

        /* inspecting the SEQUENCE-table */
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", output -> seq . tbl_name );
        if ( 0 != rc ) {
            ErrMsg( "insp_csra().VDatabaseOpenTableRead( '%s' ) -> %R",
                    output -> seq . tbl_name, rc );
        }
        else {
            rc = insp_seq_tbl( tbl, input, &( output -> seq ) );
            rc = insp_release_tbl( tbl, rc, "insp_csra (seq)", input -> accession_short );
        }

        /* inspecting the PRIMARY_ALIGNMENT-table */
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
        if ( 0 != rc ) {
            ErrMsg( "insp_csra().VDatabaseOpenTableRead( PRIMARY_ALIGNMENT ) -> %R", rc );
        }
        else {
            rc = insp_align_tbl( tbl, input, &( output -> align ) );
            rc = insp_release_tbl( tbl, rc, "insp_csra (align)", input -> accession_short );
        }
        rc = insp_release_db( db, rc, "insp_csra", input -> accession_short );
    }
    return rc;
}

/* it is a database without alignments! */
static rc_t insp_sra_db( const insp_input_t * input, insp_output_t * output ) {
    const VDatabase * db;
    rc_t rc = insp_open_db( input, &db );
    if ( 0 == rc ) {
        const VTable * tbl;
        rc = VDatabaseOpenTableRead( db, &tbl, "%s", output -> seq . tbl_name );
        if ( 0 != rc ) {
            ErrMsg( "insp_sra_db().VDatabaseOpenTableRead( '%s' ) -> %R",
                    output -> seq . tbl_name, rc );
        }
        else {
            rc = insp_seq_tbl( tbl, input, &( output -> seq ) );
            rc = insp_release_tbl( tbl, rc, "insp_sra_db", input -> accession_short );
        }

        rc = insp_release_db( db, rc, "insp_sra_db", input -> accession_short );
    }
    return rc;
}

/* it is a flat table! */
static rc_t insp_sra_flat( const insp_input_t * input, insp_output_t * output ) {
    const VTable * tbl;
    rc_t rc = insp_open_tbl( input, &tbl );
    if ( 0 == rc ) {
        rc = insp_seq_tbl( tbl, input, &( output -> seq ) );
        rc = insp_release_tbl( tbl, rc, "insp_sra_flat", input -> accession_short );
    }
    return 0;
}


/* ------------------------------------------------------------------------------------------- */

rc_t inspect( const insp_input_t * input, insp_output_t * output ) {
    rc_t rc = 0;
    if ( NULL == input ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspect() : input is NULL -> %R", rc );
    } else if ( NULL == output ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspect() : output is NULL -> %R", rc );
    } else if ( NULL == input -> dir ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspect() : input->dir is NULL -> %R", rc );
    } else if ( NULL == input -> vdb_mgr ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspect() : input->vdb_mgr is NULL -> %R", rc );
    } else if ( NULL == input -> accession_short ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspect() : input->accession_short is NULL -> %R", rc );
    } else if ( NULL == input -> accession_path ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "inspect() : input->accession_path is NULL -> %R", rc );
    } else {
        memset( output, 0, sizeof *output );
        rc = insp_location_and_size( input, output );
        if ( 0 == rc ) {
            output -> acc_type = insp_path_type_and_seq_tbl_name( input, output ); /* db or table? */

            switch( output -> acc_type ) {
                case acc_csra          : rc = insp_csra( input, output ); break; /* above */
                case acc_sra_flat      : rc = insp_sra_flat( input, output ); break; /* above */
                case acc_sra_db        : ; /* break intentionally omited! */
                case acc_pacbio_bam    : rc = insp_sra_db( input, output ); break; /* above */
                case acc_pacbio_native : rc = insp_sra_db( input, output ); break; /* above */
                default                : break;
            }

        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

static rc_t insp_report_seq( const insp_seq_data_t * seq ) {
    rc_t rc = KOutMsg( "... SEQ has NAME column = %s\n", hlp_yes_or_no( seq -> has_name_column ) );
    if ( 0 == rc ) {
        rc = KOutMsg( "... SEQ has SPOT_GROUP column = %s\n", hlp_yes_or_no( seq -> has_spot_group_column ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "... SEQ has READ_TYPE column = %s\n", hlp_yes_or_no( seq -> has_read_type_column ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "... SEQ has QUALITY column = %s\n", hlp_yes_or_no( seq -> has_quality_column ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "... SEQ has READ column = %s\n", hlp_yes_or_no( seq -> has_read_column ) );
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

static rc_t insp_report_align( const insp_align_data_t * align ) {
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

rc_t insp_report( const insp_input_t * input, const insp_output_t * output ) {
    rc_t rc = KOutMsg( "%s is %s\n",
                       input -> accession_short,
                       output -> is_remote ? "remote" : "local" );
    if ( 0 == rc ) {
        rc = KOutMsg( "... has a size of %,lu bytes\n", output -> acc_size );
    }
    if ( 0 == rc ) {
        const char * s_acc_type = insp_acc_type_to_string( output -> acc_type ); /* above */
        rc = KOutMsg( "... is %s\n", s_acc_type );
    }
    if ( 0 == rc ) {
        rc = insp_report_seq( &( output -> seq ) );
    }
    if ( 0 == rc ) {
        rc = insp_report_align( &( output -> align ) );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "\n" );
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------- */

static size_t insp_est_base_count( const insp_estimate_input_t * input ) {
    /* if we are skipping technical reads : we take the bio_base_count, otherwise the total_base_count 
       ( these 2 numbers can be the same for cSRA objects, they have no technical reads ) */
    if ( input -> skip_tech ) {
        return input -> insp -> seq . bio_base_count;
    }
    return input -> insp -> seq . total_base_count;
}

static uint32_t insp_est_reads_per_spot( const insp_estimate_input_t * input ) {
    uint32_t res = input -> insp -> seq . avg_bio_reads;
    if ( !input -> skip_tech ) {
        res += input -> insp -> seq . avg_tech_reads;
    }
    return res;
}

static size_t insp_est_seq_defline_len( const insp_estimate_input_t * input ) {
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

static size_t insp_est_qual_defline_len( const insp_estimate_input_t * input ) {
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

static size_t insp_est_out_size_whole_spot( const insp_estimate_input_t * input, bool fasta ) {
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

static size_t insp_est_out_size_split_spot( const insp_estimate_input_t * input, bool fasta ) {
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

static size_t insp_est_out_size_ref_tbl( const insp_estimate_input_t * input, bool fasta ) {
    /* size_t res = insp_est_ref_base_count( input );  TBD */
    size_t res = 1000000;
    return res;
}

static size_t insp_est_out_size_ref_report( const insp_estimate_input_t * input, bool fasta ) {
    /* size_t res = insp_est_ref_base_count( input );  TBD */
    size_t res = 1000000;
    return res;
}

size_t insp_estimate_output_size( const insp_estimate_input_t * input ) {
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
        case ft_fasta_ref_tbl           : res = insp_est_out_size_ref_tbl( input, true ); break;
        case ft_fasta_concat            : res = insp_est_out_size_ref_tbl( input, true ); break;        
        case ft_ref_report              : res = insp_est_out_size_ref_report( input, true ); break;
    }
    return res;
}
