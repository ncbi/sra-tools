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

#include "copy_meta.h"

#ifndef _h_definitions_
#include "definitions.h"
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_klib_time_
#include <klib/time.h>
#endif

#ifndef _h_kapp_main_
#include <kapp/main.h>      /* for KAppVersion()*/
#endif

#ifndef _h_kdb_meta_
#include <kdb/meta.h>
#endif

#ifndef _h_kdb_namelist_
#include <kdb/namelist.h>
#endif

static rc_t copy_metadata_data ( const KMDataNode *snode, KMDataNode *dnode ) {
    char buffer [ 1024 ];
    size_t total, bytes, remaining;
    /* copy node data unless already set */
    rc_t rc = KMDataNodeRead ( dnode, 0, buffer, 0, & bytes, & total );
    DISP_RC( rc, "copy_metadata_child:KMDataNodeRead(dst) failed" );
    if ( 0 == rc && 0 == total ) {
        do {
            rc = KMDataNodeRead ( snode, total, buffer, sizeof buffer, & bytes, & remaining );
            DISP_RC( rc, "copy_metadata_child:KMDataNodeRead(src) failed" );
            if ( 0 == rc ) {
                rc = KMDataNodeAppend ( dnode, buffer, bytes );
                DISP_RC( rc, "copy_metadata_child:KMDataNodeAppend(dst) failed" );
                if ( 0 != rc ) break;
            }
            total += bytes;
        } while ( 0 != remaining );
    }
    return rc;
}

static rc_t copy_metadata_attribs ( const KMDataNode *snode, KMDataNode *dnode,
                                    const char *node_path, const bool show_meta ) {
    KNamelist *attrs;
    uint32_t i, count;
    rc_t rc = KMDataNodeListAttr ( snode, & attrs );
    DISP_RC( rc, "copy_metadata_child:KMDataNodeListAttr(src) failed" );
    if ( 0 != rc ) return rc;
    rc = KNamelistCount ( attrs, & count );
    for ( i = 0; 0 == rc && i < count; ++ i ) {
        const char *attr;
        rc = KNamelistGet ( attrs, i, & attr );
        if ( 0 == rc ) {
            char buffer [ 1024 ];
            size_t bytes;
            /* test for attr existence */
            rc = KMDataNodeReadAttr ( dnode, attr, buffer, sizeof buffer, & bytes );
            if ( 0 != rc ) {
                rc = KMDataNodeReadAttr ( snode, attr, buffer, sizeof buffer, & bytes );
                if ( 0 == rc ) {
                    if ( show_meta ) {
                        KOutMsg( "copy atr %s : %s\n", node_path, attr );
                    }
                    rc = KMDataNodeWriteAttr ( dnode, attr, buffer );
                }
            }
        }
        DISP_RC( rc, "copy_metadata_child:failed to copy attribute" );
    }
    KNamelistRelease ( attrs );
    return rc;
}

static rc_t copy_metadata_child ( const KMDataNode *src_root, KMDataNode *dst_root,
                                  const char *node_path, const bool show_meta ) {
    const KMDataNode *snode;
    KMDataNode *dnode;
    KNamelist *names;

    rc_t rc = KMDataNodeOpenNodeRead ( src_root, & snode, "%s", node_path );
    DISP_RC( rc, "copy_metadata_child:KMDataNodeOpenNodeRead(src) failed" );
    if ( 0 != rc ) return rc;
    if ( show_meta ) {
        KOutMsg( "copy child-node: %s\n", node_path );
    }
    rc = KMDataNodeOpenNodeUpdate ( dst_root, & dnode, "%s", node_path );
    DISP_RC( rc, "copy_metadata_child:KMDataNodeOpenNodeUpdate(dst) failed" );
    if ( 0 == rc ) {
        rc = copy_metadata_data ( snode, dnode );
        if ( 0 == rc ) {
            rc = copy_metadata_attribs ( snode, dnode, node_path, show_meta );
        }
        KMDataNodeRelease ( dnode );
    } else {
        PLOGMSG( klogInfo, ( klogInfo, 
                 "cannot open child-node(dst): $(node)", "node=%s", node_path ));
    }
    if ( 0 == rc || ( rcBusy == GetRCState( rc ) ) ) {
        rc = KMDataNodeListChild ( snode, & names );
        DISP_RC( rc, "copy_metadata_child:KMDataNodeListChild(src) failed" );
        if ( 0 == rc ) {
            uint32_t i, count;
            char temp_path[ 1024 ];
            size_t temp_len;

            string_copy ( temp_path, ( sizeof temp_path ) - 1, node_path, string_size( node_path ) );
            temp_len = string_size( temp_path );
            temp_path[ temp_len++ ] = '/';
            temp_path[ temp_len ] = 0;
            rc = KNamelistCount ( names, & count );
            for ( i = 0; 0 == rc && i < count; ++ i ) {
                const char *child_name;
                rc = KNamelistGet ( names, i, & child_name );
                if ( 0 == rc ) {
                    string_copy( temp_path + temp_len, ( sizeof temp_path ) - temp_len, child_name, string_size( child_name ) );
                    rc = copy_metadata_child ( src_root, dst_root, temp_path, show_meta );
                    temp_path[ temp_len ] = 0;
                }
            }
            KNamelistRelease ( names );
        }
    }
    KMDataNodeRelease ( snode );
    return rc;
}

static rc_t copy_metadata_root ( const KMDataNode *src_root, KMDataNode *dst_root,
                                 const char * excluded_nodes,
                                 const bool show_meta ) {
    KNamelist *names;
    const VNamelist *excluded_names = NULL;
    uint32_t i, count;

    rc_t rc = KMDataNodeListChild ( src_root, & names );
    DISP_RC( rc, "copy_metadata_root:KMDataNodeListChild() failed" );
    if ( 0 != rc ) return rc;
    if ( NULL != excluded_nodes ) {
        rc = nlt_make_VNamelist_from_string( &excluded_names, excluded_nodes );
        DISP_RC( rc, "copy_metadata_root:nlt_make_namelist_from_string() failed" );
        if ( 0 != rc ) return rc;
    }
    rc = KNamelistCount ( names, & count );
    for ( i = 0; 0 == rc && i < count; ++ i ) {
        const char *node_path;
        rc = KNamelistGet ( names, i, & node_path );
        DISP_RC( rc, "copy_metadata_root:KNamelistGet() failed" );
        if ( 0 == rc ) {
            bool is_excluded = false;
            if ( NULL != excluded_names ) {
                is_excluded = nlt_is_name_in_VNamelist( excluded_names, node_path );
            }
            if ( !is_excluded ) {
                rc = copy_metadata_child ( src_root, dst_root, node_path, show_meta );
            }
        }
    }
    if ( NULL != excluded_names ) {
        VNamelistRelease( excluded_names );
    }
    KNamelistRelease ( names );
    return rc;
}

static rc_t copy_stray_metadata ( const KMetadata *src_meta, KMetadata *dst_meta,
                                  const char * excluded_nodes,
                                  const bool show_meta ) {
    /* open root node */
    const KMDataNode *src_root;
    rc_t rc = KMetadataOpenNodeRead ( src_meta, & src_root, NULL );
    DISP_RC( rc, "copy_stray_metadata:KMetadataOpenNodeRead() failed" );
    if ( 0 == rc ) {
        KMDataNode *dst_root;
        rc = KMetadataOpenNodeUpdate ( dst_meta, & dst_root, NULL );
        DISP_RC( rc, "copy_stray_metadata:KMetadataOpenNodeUpdate() failed" );
        if ( 0 == rc ) {
            /* treat the root node in a special way */
            rc = copy_metadata_root ( src_root, dst_root, excluded_nodes, show_meta );
            KMDataNodeRelease ( dst_root );
        }
        KMDataNodeRelease ( src_root );
    }
    return rc;
}

static rc_t copy_back_revisions ( const KMetadata *src_meta, VTable *dst_table,
                                  const bool show_meta ) {
    uint32_t max_revision, revision;

    rc_t rc = KMetadataMaxRevision ( src_meta, &max_revision );
    DISP_RC( rc, "copy_back_revisions:KMetadataMaxRevision() failed" );
    if ( 0 != rc ) return rc;
    if ( 0 == max_revision ) return rc;
    for ( revision = 1; revision <= max_revision && 0 == rc; ++revision ) {
        const KMetadata *src_rev_meta;
        if ( show_meta ) {
            KOutMsg( "+++copy metadata rev. #%u:\n", revision );
        }
        rc = KMetadataOpenRevision ( src_meta, &src_rev_meta, revision );
        DISP_RC( rc, "copy_back_revisions:KMetadataOpenRevision() failed" );
        if ( 0 == rc ) {
            KMetadata *dst_meta;
            rc = VTableOpenMetadataUpdate ( dst_table, & dst_meta );
            DISP_RC( rc, "copy_table_meta:VTableOpenMetadataUpdate() failed" );
            if ( 0 == rc ) {
                rc = copy_stray_metadata ( src_rev_meta, dst_meta, NULL, show_meta );
                if ( 0 == rc ) {
                    rc = KMetadataCommit ( dst_meta );
                    DISP_RC( rc, "copy_back_revisions:KMetadataCommit() failed" );
                    if ( 0 == rc ) {
                        rc = KMetadataFreeze ( dst_meta );
                        DISP_RC( rc, "copy_back_revisions:KMetadataFreeze() failed" );
                    }
                }
                KMetadataRelease ( dst_meta );
            }
            KMetadataRelease ( src_rev_meta );
        }
    }
    return rc;
}

static rc_t fill_timestring( char * s, size_t size ) {
    KTime tr;
    rc_t rc;
    KTimeLocal ( &tr, KTimeStamp() );
    rc = string_printf ( s, size, NULL, "%lT", &tr );
    DISP_RC( rc, "fill_timestring:string_printf( date/time ) failed" );
    return rc;
}

static rc_t enter_time( KMDataNode *node, const char * key ) {
    char timestring[ 160 ];
    rc_t rc = fill_timestring( timestring, sizeof timestring );
    if ( 0 == rc ) {
        rc = KMDataNodeWriteAttr ( node, key, timestring );
        DISP_RC( rc, "enter_time:KMDataNodeWriteAttr( timestring ) failed" );
    }
    return rc;
}

static rc_t enter_version( KMDataNode *node, const char * key ) {
    char buff[ 32 ];
    rc_t rc;
    rc = string_printf ( buff, sizeof( buff ), NULL, "%.3V", KAppVersion() );
    assert ( 0 == rc );
    rc = KMDataNodeWriteAttr ( node, key, buff );
    DISP_RC( rc, "enter_version:KMDataNodeWriteAttr() failed" );
    return rc;
}

static rc_t enter_date_name_vers( KMDataNode *node ) {
    rc_t rc = enter_time( node, "run" );
    DISP_RC( rc, "enter_date_name_vers:enter_time() failed" );
    if ( 0 == rc ) {
        rc = KMDataNodeWriteAttr ( node, "tool", "vdb-copy" );
        DISP_RC( rc, "enter_date_name_vers:KMDataNodeWriteAttr(tool=vdb-copy) failed" );
        if ( 0 == rc ) {
            rc = enter_version ( node, "vers" );
            DISP_RC( rc, "enter_date_name_vers:enter_version() failed" );
            if ( 0 == rc ) {
                rc = KMDataNodeWriteAttr ( node, "build", __DATE__ );
                DISP_RC( rc, "enter_date_name_vers:KMDataNodeWriteAttr(build=_DATE_) failed" );
            }
        }
    }
    return rc;
}

static rc_t enter_schema_update( KMetadata *dst_meta, const bool show_meta ) {
    rc_t rc;
    KMDataNode *sw_node;
    if ( show_meta ) {
        KOutMsg( "--- entering schema-update\n" );
    }
    rc = KMetadataOpenNodeUpdate ( dst_meta, &sw_node, "SOFTWARE" );
    DISP_RC( rc, "enter_schema_update:KMetadataOpenNodeUpdate('SOFTWARE') failed" );
    if ( 0 == rc ) {
        KMDataNode *update_node;
        rc = KMDataNodeOpenNodeUpdate ( sw_node, &update_node, "update" );
        DISP_RC( rc, "enter_schema_update:KMDataNodeOpenNodeUpdate('update') failed" );
        if ( 0 == rc ) {
            rc = enter_date_name_vers( update_node );
            KMDataNodeRelease ( update_node );
        }
        KMDataNodeRelease ( sw_node );
    }
    return rc;
}

static uint32_t get_child_count( KMDataNode *node ) {
    uint32_t res = 0;
    KNamelist *names;
    rc_t rc = KMDataNodeListChild ( node, &names );
    DISP_RC( rc, "get_child_count:KMDataNodeListChild() failed" );
    if ( 0 == rc ) {
        rc = KNamelistCount ( names, &res );
        DISP_RC( rc, "get_child_count:KNamelistCount() failed" );
        KNamelistRelease ( names );
    }
    return res;
}

static rc_t enter_vdbcopy_node( KMetadata *dst_meta, const bool show_meta ) {
    rc_t rc;
    KMDataNode *hist_node;

    if ( show_meta ) {
        KOutMsg( "--- entering Copy entry...\n" );
    }
    rc = KMetadataOpenNodeUpdate ( dst_meta, &hist_node, "HISTORY" );
    DISP_RC( rc, "enter_vdbcopy_node:KMetadataOpenNodeUpdate('HISTORY') failed" );
    if ( 0 == rc ) {
        char event_name[ 32 ];
        uint32_t index = get_child_count( hist_node ) + 1;
        rc = string_printf ( event_name, sizeof( event_name ), NULL, "EVENT_%u", index );
        DISP_RC( rc, "enter_vdbcopy_node:string_printf(EVENT_NR) failed" );
        if ( 0 == rc ) {
            KMDataNode *event_node;
            rc = KMDataNodeOpenNodeUpdate ( hist_node, &event_node, "%s", event_name );
            DISP_RC( rc, "enter_vdbcopy_node:KMDataNodeOpenNodeUpdate('EVENT_NR') failed" );
            if ( 0 == rc ) {
                rc = enter_date_name_vers( event_node );
                KMDataNodeRelease ( event_node );
            }
        }
        KMDataNodeRelease ( hist_node );
    }
    return rc;
}

rc_t copy_table_meta ( const VTable *src_table, VTable *dst_table,
                       const char * excluded_nodes,
                       const bool show_meta, const bool schema_updated ) {
    const KMetadata *src_meta;
    rc_t rc;

    if ( NULL == src_table || NULL == dst_table ) {
        return RC( rcExe, rcNoTarg, rcCopying, rcParam, rcNull );
    }
    /* it is OK if excluded_nodes is NULL */
    rc = VTableOpenMetadataRead ( src_table, & src_meta );
    DISP_RC( rc, "copy_table_meta:VTableOpenMetadataRead() failed" );
    if ( 0 == rc ) {
        rc = copy_back_revisions ( src_meta, dst_table, show_meta );
        if ( 0 == rc ) {
            KMetadata *dst_meta;
            rc = VTableOpenMetadataUpdate ( dst_table, & dst_meta );
            DISP_RC( rc, "copy_table_meta:VTableOpenMetadataUpdate() failed" );
            if ( 0 == rc ) {
                if ( show_meta ) {
                    KOutMsg( "+++copy current metadata\n" );
                }
                rc = copy_stray_metadata ( src_meta, dst_meta, excluded_nodes,
                                           show_meta );
                if ( show_meta ) {
                    KOutMsg( "+++end of copy current metadata\n" );
                }
                /* enter a attribute "vdb-copy" under '/SOFTWARE/update'
                   *if the schema was updated ! */
                if ( 0 == rc && schema_updated ) {
                    rc = enter_schema_update( dst_meta, show_meta );
                }
                /* enter a unconditional node under '/SOFTWARE/Copy'
                    <%TIMESTAMP%>
                        <Application date="%DATE%" name="vdb-copy" vers="%VERSION%"/>
                    </%TIMESTAMP%>
                */
                if ( 0 == rc ) {
                    rc = enter_vdbcopy_node( dst_meta, show_meta );
                }
                KMetadataRelease ( dst_meta );
            }
        }
        KMetadataRelease ( src_meta );
    }
    return rc;
}


rc_t copy_database_meta ( const VDatabase *src_db, VDatabase *dst_db,
                          const char * excluded_nodes,
                          const bool show_meta ) {
    const KMetadata *src_meta;
    rc_t rc;

    if ( NULL == src_db || NULL == dst_db ) {
        return RC( rcExe, rcNoTarg, rcCopying, rcParam, rcNull );
    }
    /* it is OK if excluded_nodes is NULL */
    rc = VDatabaseOpenMetadataRead ( src_db, & src_meta );
    DISP_RC( rc, "copy_database_meta:VDatabaseOpenMetadataRead() failed" );
    if ( 0 == rc ) {
        KMetadata *dst_meta;
        rc = VDatabaseOpenMetadataUpdate ( dst_db, & dst_meta );
        DISP_RC( rc, "copy_database_meta:VDatabaseOpenMetadataUpdate() failed" );
        if ( 0 == rc ) {
            if ( show_meta ) {
                KOutMsg( "+++copy current db-metadata\n" );
            }
            rc = copy_stray_metadata ( src_meta, dst_meta, excluded_nodes,
                                       show_meta );
            if ( show_meta ) {
                KOutMsg( "+++end of copy db-current metadata\n" );
            }
            KMetadataRelease ( dst_meta );
        }
       KMetadataRelease ( src_meta );
    }
    return rc;
}
