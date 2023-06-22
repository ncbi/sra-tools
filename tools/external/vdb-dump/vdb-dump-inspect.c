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

#ifndef _h_kdb_manager_
#include <kdb/manager.h>
#endif

#include <vdb/vdb-priv.h>
#include <kdb/kdb-priv.h>
#include <kdb/table.h>
#include <kdb/database.h>

#ifndef _h_kfs_arc_
#include <kfs/arc.h>
#endif

#ifndef _h_kfs_tar_
#include <kfs/tar.h>
#endif

#include "vdb-dump-helper.h"
#include "vdb-dump-inspect.h"

typedef struct VDI_ITEM {
    const String * name;
    uint64_t size;
    uint32_t type;
    uint32_t level;
    double percent;
    Vector items;
} VDI_ITEM;

/* -------------------------------------------------------------------------------------------------- */

static VDI_ITEM * make_vdi_Item( const KDirectory * dir, uint32_t type, const char * name, VDI_ITEM * parent );
static rc_t visit_vdi_sub( const KDirectory * dir, uint32_t type, const char *name, void *data ) {
    make_vdi_Item( dir, type, name, data );
    return 0;
}
static VDI_ITEM * make_vdi_Item( const KDirectory * dir, uint32_t type, const char * name, VDI_ITEM * parent ) {
    VDI_ITEM * self = calloc( 1, sizeof *self );
    if ( NULL != self ) {
        rc_t rc = 0;
        String tmp;
        self -> type = type;
        VectorInit( &( self -> items ), 0, 5 );
        StringInitCString( &tmp, name );
        rc = StringCopy( &( self -> name ), &tmp );
        if ( 0 == rc ) {
            if ( NULL != parent ) {
                self -> level = parent -> level + 1;                
            }
            if ( kptDir == type ) {
                const KDirectory * sub;
                rc = KDirectoryOpenDirRead( dir, &sub, false, name );
                if ( 0 == rc ) {
                    rc = KDirectoryVisit( sub, false, visit_vdi_sub, self, "." );
                    rc = vdh_kdirectory_release( rc, sub );
                }
            } else if ( kptFile == type ) {
                rc = KDirectoryFileSize( dir, &( self -> size ), name );
            }
            if ( NULL != parent ) {
                uint32_t idx;
                rc = VectorAppend( &( parent -> items ), &idx, self );
            }
        }
        if ( 0 != rc ) {
            free( self );
            self = NULL;
        }
    }
    return self;
}

/* -------------------------------------------------------------------------------------------------- */

static void print_vdi_item( VDI_ITEM * item, VNamelist * exclude );
static void print_vdi_cb( void *item, void *data ) { print_vdi_item( item, data ); }
static void print_vdi_item( VDI_ITEM * item, VNamelist * exclude ) {
    if ( kptDir == item -> type ) {
        bool print = true;
        if ( NULL != exclude ) {
            uint32_t found;
            rc_t rc = VNamelistIndexOf( exclude, item -> name -> addr, &found );
            if ( 0 == rc ) { print = false; }
        }
        if ( print ) {
            KOutMsg( "%*s %7.4f%% %S\n", ( item->level * 2 ), " ", item -> percent, item -> name );
        }
        VectorForEach( &( item -> items ), false, print_vdi_cb, exclude );
    }
}

/* -------------------------------------------------------------------------------------------------- */

static int64_t cmp_vdi_size( const void ** item1, const void ** item2, void *data ) {
    const VDI_ITEM * vitem1 = *item1;
    const VDI_ITEM * vitem2 = *item2;
    return vitem2 -> size - vitem1 -> size;
}
static void calc_vdi_percent( VDI_ITEM * item, uint64_t * total );
static void calc_vdi_percent_cb( void *item, void *data ) { calc_vdi_percent( item, data ); }    
static void calc_vdi_percent( VDI_ITEM * item, uint64_t * total ) {
    if ( ( 0 != item -> size )&&( 0 != *total ) ) {
        double p = item -> size;
        p *= 100;
        item -> percent = p / *total;
    }
    VectorForEach( &( item -> items ), false, calc_vdi_percent_cb, total );
    /* and now we can sort the vector by size */
    VectorReorder( &( item -> items ), cmp_vdi_size, NULL );
}

/* -------------------------------------------------------------------------------------------------- */

static uint64_t calc_vdi_item_size( VDI_ITEM * item );
static void calc_vdi_cb( void *item, void *data ) {
    uint64_t * sum = data;
    *sum += calc_vdi_item_size( item );
}
static uint64_t calc_vdi_item_size( VDI_ITEM * item ) {
    if ( 0 == item -> size ) {
        VectorForEach( &( item -> items ), false, calc_vdi_cb, &( item -> size ) );
    }
    return item -> size;
}

/* -------------------------------------------------------------------------------------------------- */

static void destroy_vdi_item( void * self, void * data ) {
    if ( NULL != self ) {
        VDI_ITEM * item = self;
        if ( NULL != item -> name ) {
            StringWhack( item -> name );
        }
        VectorWhack( &( item -> items ), destroy_vdi_item, NULL );
        free( self );
    }
}

/* -------------------------------------------------------------------------------------------------- */

static rc_t vdb_dump_inspect_dir( const KDirectory * dir ) {
    rc_t rc = 0;
    VDI_ITEM * root = make_vdi_Item( dir, kptDir, ".", NULL );
    if ( NULL != root ) {
        uint64_t sum = calc_vdi_item_size( root );
        VNamelist * exclude;
        rc = VNamelistMake( &exclude, 5 );
        if ( 0 == rc ) {
            calc_vdi_percent( root, &sum );
            VNamelistAppend( exclude, "idx" );
            VNamelistAppend( exclude, "md" );
            VNamelistAppend( exclude, "." );
            VNamelistAppend( exclude, "col" );
            VNamelistAppend( exclude, "tbl" );
            print_vdi_item( root, exclude );
            VNamelistRelease( exclude );
        }
        destroy_vdi_item( root, NULL );
    }
    return vdh_kdirectory_release( rc, dir );
}

static rc_t vdb_dump_inspect_db( const VDBManager * mgr, const char * object, const VPath * path ) {
    const VDatabase *db;
    rc_t rc = VDBManagerOpenDBReadVPath( mgr, &db, NULL, path );
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerOpenDBReadVPath( '%s' ) -> %R", object, rc );
    } else {
        const KDatabase *k_db;
        rc = VDatabaseOpenKDatabaseRead( db, &k_db );
        if ( 0 != rc ) {
            ErrMsg( "VDatabaseOpenKDatabaseRead( '%s' ) -> %R", object, rc );
        } else {
            const KDirectory * dir;
            rc = KDatabaseOpenDirectoryRead( k_db, &dir );
            if ( 0 != rc ) {
                ErrMsg( "KDatabaseOpenDirectoryRead( '%s' ) -> %R", object, rc );                
            } else {
                rc = vdb_dump_inspect_dir( dir );
            }
            rc = vdh_kdatabase_release( rc, k_db );            
        }
        rc = vdh_vdatabase_release( rc, db );
    }
    return rc;
}

static rc_t vdb_dump_inspect_tbl( const VDBManager * mgr, const char * object, const VPath * path ) {
    const VTable *tbl;
    rc_t rc = VDBManagerOpenTableReadVPath( mgr, &tbl, NULL, path );
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerOpenTableReadVPath( '%s' ) -> %R", object, rc );
    } else {
        const KTable *k_tbl;
        rc = VTableOpenKTableRead( tbl, &k_tbl );
        if ( 0 != rc ) {
            ErrMsg( "VTableOpenKTableRead( '%s' ) -> %R", object, rc );
        } else {
            const KDirectory * dir;
            rc = KTableOpenDirectoryRead( k_tbl, &dir );
            if ( 0 != rc ) {
                ErrMsg( "KTableOpenDirectoryRead( '%s' ) -> %R", object, rc );                
            } else {
                rc = vdb_dump_inspect_dir( dir );
            }
            rc = vdh_ktable_release( rc, k_tbl );
        }
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}

rc_t vdb_dump_inspect( const KDirectory * dir, const VDBManager * mgr, const char * object ) {
    VPath * path = NULL;
    rc_t rc = vdh_path_to_vpath( object, &path );
    if ( 0 != rc ) {
        ErrMsg( "vdh_path_to_vpath( '%s' ) -> %R", object, rc );            
    } else {
        /* let's check if we can use the object */
        uint32_t path_type = ( VDBManagerPathType ( mgr, "%s", object ) & ~ kptAlias );
        /* types defined in <kdb/manager.h> */
        switch ( path_type ) {
            case kptDatabase        : rc = vdb_dump_inspect_db( mgr, object, path ); break;
            case kptPrereleaseTbl   :
            case kptTable           : rc = vdb_dump_inspect_tbl( mgr, object, path ); break;
            default                 : rc = KOutMsg( "\tis not a vdb-object\n" ); break;
        }
        rc = vdh_vpath_release( rc, path );
    }
    return rc;
}
