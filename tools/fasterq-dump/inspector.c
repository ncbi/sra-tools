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

static rc_t inspector_open_db( const inspector_input_t * input, const VDatabase ** db ) {
    VPath * v_path = NULL;
    rc_t rc = inspector_path_to_vpath( input -> accession_path, &v_path );
    /* KOutMsg( "\ncmn_open_db( '%s' )\n", path ); */
    if ( 0 == rc ) {
        rc = VDBManagerOpenDBReadVPath( input -> vdb_mgr, db, NULL, v_path );
        if ( 0 != rc ) {
            ErrMsg( "inspector.c inspector_open_db().VDBManagerOpenDBReadVPath( '%s' ) -> %R\n",
                    input -> accession_path, rc );
        }
        {
            rc_t rc2 = VPathRelease( v_path );
            if ( 0 != rc2 ) {
                ErrMsg( "inspector.c inspector_open_db().VPathRelease( '%s' ) -> %R\n",
                        input -> accession_path, rc2 );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
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
                    ErrMsg( "inspector.c inspect_db_platform()().VCursorOpen( '%s' ) -> %R\n",
                            input -> accession_short, rc );
                } else {
                    int64_t first;
                    uint64_t count;
                    rc = VCursorIdRange( curs, idx, &first, &count );
                    if ( 0 != rc ) {
                        ErrMsg( "inspector.c inspect_db_platform()().VCursorIdRange( '%s' ) -> %R\n",
                                input -> accession_short, rc );
                    } else if ( count > 0 ) {
                        uint32_t elem_bits, boff, row_len;
                        uint8_t *values;
                        rc = VCursorCellDataDirect( curs, first, idx, &elem_bits, (const void **)&values, &boff, &row_len );
                        if ( 0 != rc ) {
                            ErrMsg( "inspector.c inspect_db_platform()().VCursorCellDataDirect( '%s' ) -> %R\n",
                                    input -> accession_short, rc );
                        } else if ( NULL != values && 0 == elem_bits && 0 == boff && row_len > 0 ) {
                            *pf = values[ 0 ];
                            res = true;
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

static acc_type_t inspect_db_type( const inspector_input_t * input ) {
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
                if ( contains( tables, "SEQUENCE" ) )
                {
                    res = acc_sra_db;
                    
                    /* we have at least a SEQUENCE-table */
                    if ( contains( tables, "PRIMARY_ALIGNMENT" ) &&
                         contains( tables, "REFERENCE" ) ) {
                        res = acc_csra;
                    } else {
                        if ( contains( tables, "CONSENSUS" ) ||
                             contains( tables, "ZMW_METRICS" ) ||
                             contains( tables, "PASSES" ) ) {
                            res = acc_pacbio;
                        } else {
                            /* last resort try to find out what the database-type is */
                            uint8_t pf;
                            if ( inspect_db_platform( input, db, &pf ) ) {
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

static acc_type_t inspect_path_type( const inspector_input_t * input ) {
    acc_type_t res = acc_none;
    int pt = VDBManagerPathType ( input -> vdb_mgr, "%s", input -> accession_path );
    switch( pt )
    {
        case kptDatabase    : res = inspect_db_type( input ); break;

        case kptPrereleaseTbl:
        case kptTable       : res = acc_sra_flat; break;
    }
    return res;
}

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
        output -> acc_type = inspect_path_type( input );
    }
    return rc;
}
