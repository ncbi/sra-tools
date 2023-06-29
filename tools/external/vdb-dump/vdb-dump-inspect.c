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

/* -------------------------------------------------------------------------------------------------- */

static uint64_t vdi_count_total_bytes_of_column( const VTable *tbl, const String * column_name ) {
    uint64_t total_bytes = 0;
    const VCursor * cur;
    rc_t rc = VTableCreateCachedCursorRead( tbl, &cur, 1024 * 1024 * 10 );
    if ( 0 == rc ) {
        uint32_t idx;
        rc = VCursorAddColumn( cur, &idx, "%s", column_name -> addr );
        if ( 0 == rc ) {
            rc = VCursorOpen( cur );
            if ( 0 == rc ) {
                int64_t first;
                uint64_t count;
                rc = VCursorIdRange( cur, idx, &first, &count );
                if ( 0 == rc ) {
                    while ( 0 == rc && count > 0 ) {
                        const void * data;
                        uint32_t elem_bits;
                        uint32_t bit_offset;
                        uint32_t row_len;
                        rc = VCursorCellDataDirect( cur, first++, idx, &elem_bits, &data, &bit_offset, &row_len );
                        if ( 0 == rc ) {
                            uint64_t row_bits = row_len;
                            row_bits *= elem_bits;
                            total_bytes += ( row_bits / 8 );
                            count--;
                        }
                    }
                }
            }
        }
        rc = vdh_vcursor_release( rc, cur );
    }
    return total_bytes;
}

/* -------------------------------------------------------------------------------------------------- */

static bool vdi_cmp_name( const String * S, const char * s ) {
    bool res = false;
    if ( NULL != S && NULL != s ) {
        String S2;
        StringInitCString( &S2, s );
        res = ( 0 == StringCompare( S, &S2 ) );
    }
    return res;
}

static double percent_of( uint64_t value, uint64_t total ) {
    if ( value > 0 && total > 0 ) {
        double p = value;
        p *= 100;
        return p / total;
    }
    return 0.0;
}

/* -------------------------------------------------------------------------------------------------- */

typedef enum VDI_ITEM_TYPE {
    VDI_ITEM_TYPE_OTHER,
    VDI_ITEM_TYPE_COL,
    VDI_ITEM_TYPE_TBL
} VDI_ITEM_TYPE;

typedef struct VDI_ITEM {
    const String * name;
    const struct VDI_ITEM * parent;
    Vector items;
    uint64_t compressed_size;   /* the stored, compressed size */
    uint64_t uncompressed_size; /* the reported, uncompressed size */
    uint32_t level;             /* nesting level for print */
    bool first;                 /* is this item the first one in its parents items? */
    bool last;                  /* is this item the last one in its parents items? */
    VDI_ITEM_TYPE item_type;    /* other | column | table */
    double percent_of_item;     /* percentage of size against size of whole archive */
    double compression;         /* precentage of compressed vs. un-compressed */
} VDI_ITEM;

static VDI_ITEM * vdi_make_item( const KDirectory * dir, uint32_t type, const char * name, VDI_ITEM * parent );
static rc_t vdi_visit_sub( const KDirectory * dir, uint32_t type, const char *name, void *data ) {
    vdi_make_item( dir, type, name, data );
    return 0;
}

static bool vdi_is_col( const String * S ) { return vdi_cmp_name( S, "col" ); }
static bool vdi_is_tbl( const String * S ) { return vdi_cmp_name( S, "tbl" ); }

static VDI_ITEM * vdi_make_item( const KDirectory * dir, uint32_t type, const char * name, VDI_ITEM * parent ) {
    VDI_ITEM * self = calloc( 1, sizeof *self );
    if ( NULL != self ) {
        rc_t rc = 0;
        String tmp;
        self -> parent = parent;
        self -> item_type = VDI_ITEM_TYPE_OTHER;
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
                    rc = KDirectoryVisit( sub, false, vdi_visit_sub, self, "." );
                    rc = vdh_kdirectory_release( rc, sub );
                }
                if ( NULL != parent ) {
                    if ( vdi_is_col( parent -> name ) ) {
                        self -> item_type = VDI_ITEM_TYPE_COL;
                    } else if ( vdi_is_tbl( parent -> name ) ) {
                        self -> item_type = VDI_ITEM_TYPE_TBL;
                    }
                }
            } else if ( kptFile == type ) {
                rc = KDirectoryFileSize( dir, &( self -> compressed_size ), name );
            }
            if ( NULL != parent ) {
                uint32_t idx;
                /* append itself as a child into the items of its parent */
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

static void vdi_destroy_item( void * self, void * data ) {
    if ( NULL != self ) {
        VDI_ITEM * item = self;
        if ( NULL != item -> name ) {
            StringWhack( item -> name );
        }
        VectorWhack( &( item -> items ), vdi_destroy_item, NULL );
        free( self );
    }
}

static bool vdi_item_is_col( const VDI_ITEM * self ) {
    return ( VDI_ITEM_TYPE_COL == self -> item_type );
}
static bool vdi_item_is_tbl( const VDI_ITEM * self ) {
    return ( VDI_ITEM_TYPE_TBL == self -> item_type );
}
static bool vdi_item_is_col_or_tbl( const VDI_ITEM * self ) {
    return vdi_item_is_col( self ) || vdi_item_is_tbl( self );
}

/* -------------------------------------------------------------------------------------------------- */

typedef enum VDI_FOR_EACH_ORDER {
    VDI_FOR_EACH_ORDER_SELF_FIRST,  /* former true */
    VDI_FOR_EACH_ORDER_SELF_LAST    
} VDI_FOR_EACH_ORDER;

typedef struct VDI_FOR_EACH_CTX {
    VDI_FOR_EACH_ORDER order;
    void ( CC * f ) ( VDI_ITEM *item, void * data );
    void * data;
} VDI_FOR_EACH_CTX;

static void vdi_for_each_item_2( VDI_ITEM * self, const VDI_FOR_EACH_CTX * ctx );
static void vdi_for_each_item_handle_self( VDI_ITEM * self, const VDI_FOR_EACH_CTX * ctx ) {
    if ( NULL != ctx -> f ) {
        ctx -> f( self, ctx -> data );
    }
}
static void vdi_for_each_item_handle_list( VDI_ITEM * self, const VDI_FOR_EACH_CTX * ctx ) {
    uint32_t count = VectorLength( &( self -> items ) );
    if ( count > 0 ) {
        uint32_t idx = 0;
        while ( count > 0 ) {
            VDI_ITEM * child = VectorGet( &( self -> items ), idx );
            if ( NULL != child ) {
                vdi_for_each_item_2( child, ctx );
            }
            count--;
            idx++;
        }
    }
}
static void vdi_for_each_item_2( VDI_ITEM * self, const VDI_FOR_EACH_CTX * ctx ) {
    switch ( ctx -> order ) {
        case VDI_FOR_EACH_ORDER_SELF_FIRST : {
            vdi_for_each_item_handle_self( self, ctx );
            vdi_for_each_item_handle_list( self, ctx );
        } break;
        case VDI_FOR_EACH_ORDER_SELF_LAST : {
            vdi_for_each_item_handle_list( self, ctx );                
            vdi_for_each_item_handle_self( self, ctx );
        } break;
    }
}

static void vdi_for_each_item( VDI_ITEM * self,
                               VDI_FOR_EACH_ORDER order,
                               void ( CC * f ) ( VDI_ITEM *item, void * data ),
                               void * data ) {
    VDI_FOR_EACH_CTX ctx;
    ctx . order = order;
    ctx . f = f;
    ctx . data = data;
    vdi_for_each_item_2( self, &ctx );
}
    
/* -------------------------------------------------------------------------------------------------- */

typedef struct VDI_PRINT_CTX {
    bool with_compression;
    dump_format_t format;
} VDI_PRINT_CTX;

static const String * vdi_parent_parent_name( VDI_ITEM *self ) {
    const String * res = NULL;
    if ( NULL != self -> parent ) {
        if ( NULL != self -> parent -> parent ) {
            res = self -> parent -> parent -> name;
        }
    }
    return res;
}

static bool vdi_name_valid( const String * name ) {
    bool res = false;
    if ( NULL != name ) { res = !vdi_cmp_name( name, "." ); }
    return res;
}

static void vdi_print_cb_default( VDI_ITEM *self, void * data ) {
    if ( vdi_item_is_col( self ) ) {
        const VDI_PRINT_CTX * ctx = data;        
        if ( ctx -> with_compression ) {
            KOutMsg( "%*s %7.4f%% %,20zu\t%,20zu\t%7.4f%%\t%S\n", 
                    ( self -> level * 2 ), " ", self -> percent_of_item,
                    self -> compressed_size, self -> uncompressed_size,
                    self -> compression, self -> name );
        } else {
            KOutMsg( "%*s %7.4f%% %,20zu\t%S\n", 
                    ( self -> level * 2 ), " ", self -> percent_of_item,
                    self -> compressed_size,
                    self -> name );
        }
    }
}

static void vdi_print_cb_json( VDI_ITEM *self, void * data ) {
    if ( vdi_item_is_col( self ) ) {
        const VDI_PRINT_CTX * ctx = data;
        if ( self -> first ) {
            const String * name = vdi_parent_parent_name( self );
            if ( vdi_name_valid( name ) ) {
                KOutMsg( "{ \"%S\": [ \n", name );                
            } else {
                KOutMsg( "{ \"columns\": [ \n" );
            }
        }
        if ( ctx -> with_compression ) {
            KOutMsg( "{ \"%S\":{ \"size\":%lu, \"uncompressed\":%lu, \"percent\":%.4f, \"compression\":%.4f } }", 
                    self -> name,
                    self -> compressed_size, self -> uncompressed_size,                 
                    self -> percent_of_item, self -> compression );
        } else {
            KOutMsg( "{ \"%S\":{ \"size\":%lu, \"percent\":%.4f } }",
                    self -> name,
                    self -> compressed_size,
                    self -> percent_of_item );
        }
        if ( self -> last ) {
            KOutMsg( " ] }\n" );
        } else {
            KOutMsg( ",\n" );
        }
    }
}

static void vdi_print_cb_xml( VDI_ITEM *self, void * data ) {
    if ( vdi_item_is_col( self ) ) {
        const VDI_PRINT_CTX * ctx = data;
        if ( self -> first ) {
            KOutMsg( "<table>\n" );            
            const String * name = vdi_parent_parent_name( self );
            if ( vdi_name_valid( name ) ) {
                KOutMsg( "\t<name>%S</name>\n", name );
            }
        }
        KOutMsg( "\t<column>\n" );
        KOutMsg( "\t\t<name>%S</name>\n", self -> name );
        KOutMsg( "\t\t<size>%lu</size>\n", self -> compressed_size );
        KOutMsg( "\t\t<percent>%.4f</percent>\n", self -> percent_of_item );
        if ( ctx -> with_compression ) {
            KOutMsg( "\t\t<uncompressed>%lu</uncompressed>\n", self -> uncompressed_size );
            KOutMsg( "\t\t<compression>%.4f</compression>\n", self -> compression );
        }
        KOutMsg( "\t</column>\n" );
        if ( self -> last ) {
            KOutMsg( "</table>\n" );
        }
    }
}

static void vdi_print_cb_tab( VDI_ITEM *self, void * data ) {
    if ( vdi_item_is_col( self ) ) {
        const VDI_PRINT_CTX * ctx = data;
        const String * name = vdi_parent_parent_name( self );
        if ( vdi_name_valid( name ) ) {
            KOutMsg( "%S\t", name );
        }
        KOutMsg( "%S\t%lu\t%.4f", self -> name, self -> compressed_size, self -> percent_of_item );
        if ( ctx -> with_compression ) {
            KOutMsg( "\t%lu\t%.4f\n", self -> uncompressed_size, self -> compression );
        } else {
            KOutMsg( "\n" );
        }
    }
}

static void vdi_print_cb_csv( VDI_ITEM *self, void * data ) {
    if ( vdi_item_is_col( self ) ) {
        const VDI_PRINT_CTX * ctx = data;
        const String * name = vdi_parent_parent_name( self );
        if ( vdi_name_valid( name ) ) {
            KOutMsg( "\"%S\",", name );
        }
        KOutMsg( "\"%S\",%lu,%.4f", self -> name, self -> compressed_size, self -> percent_of_item );
        if ( ctx -> with_compression ) {
            KOutMsg( ",%lu,%.4f\n", self -> uncompressed_size, self -> compression );
        } else {
            KOutMsg( "\n" );
        }
    }
}

static void vdi_print_item( VDI_ITEM * self, const VDI_PRINT_CTX * ctx ) {
    void ( CC * f ) ( VDI_ITEM *item, void * data );
    switch( ctx -> format ) {
        case df_json : f = vdi_print_cb_json; break;
        case df_xml  : f = vdi_print_cb_xml; break;
        case df_tab  : f = vdi_print_cb_tab; break;
        case df_csv  : f = vdi_print_cb_csv; break;
        default      : f = vdi_print_cb_default; break;
    }
    vdi_for_each_item( self, VDI_FOR_EACH_ORDER_SELF_FIRST, f, ( void * )ctx );
}

/* -------------------------------------------------------------------------------------------------- */

static void vdi_calc_percent_cb( VDI_ITEM * self, void * data ) {
    uint64_t total = *( (uint64_t *)data );
    self -> percent_of_item = percent_of( self -> compressed_size, total );
}

static void vdi_calc_percent( VDI_ITEM * self, uint64_t * total ) {
    vdi_for_each_item( self, VDI_FOR_EACH_ORDER_SELF_LAST,
                       vdi_calc_percent_cb, total );
}

/* -------------------------------------------------------------------------------------------------- */

static int64_t vdi_cmp_size_cb( const void ** item1, const void ** item2, void *data ) {
    const VDI_ITEM * vitem1 = *item1;
    const VDI_ITEM * vitem2 = *item2;
    return vitem2 -> compressed_size - vitem1 -> compressed_size;
}

static void vdi_order_by_compressed_size_cb( VDI_ITEM * self, void * data ) {
    uint32_t count;
    VectorReorder( &( self -> items ), vdi_cmp_size_cb, NULL );
    /* mark the last child-item */
    count = VectorLength( &( self -> items ) );
    if ( count > 0 ) {
        VDI_ITEM * item = VectorGet( &( self -> items ), 0 );
        if ( NULL != item ) { item -> first = true; }
        item = VectorGet( &( self -> items ), count - 1 );
        if ( NULL != item ) { item -> last = true; }
    }    
}

static void vdi_order_by_compressed_size( VDI_ITEM * self ) {
    vdi_for_each_item( self, VDI_FOR_EACH_ORDER_SELF_LAST,
                       vdi_order_by_compressed_size_cb, NULL );
}

/* -------------------------------------------------------------------------------------------------- */

static uint64_t vdi_calc_item_compressed_size( VDI_ITEM * self );
static void vdi_calc_item_compressed_size_cb( void *item, void *data ) {
    uint64_t * sum = data;
    *sum += vdi_calc_item_compressed_size( item );
}
static uint64_t vdi_calc_item_compressed_size( VDI_ITEM * self ) {
    if ( 0 == self -> compressed_size ) {
        VectorForEach( &( self -> items ), false, vdi_calc_item_compressed_size_cb, 
                       (void *)&( self -> compressed_size ) );
    }
    return self -> compressed_size;
}

/* -------------------------------------------------------------------------------------------------- */

static void vdi_calc_item_uncompressed_size_tbl_cb( VDI_ITEM * self, void * data ) {
    if ( vdi_item_is_col( self ) ) {
        const VTable * tbl = data;
        self -> uncompressed_size = vdi_count_total_bytes_of_column( tbl, self -> name );
        self -> compression = percent_of( self -> compressed_size, self -> uncompressed_size );
    }
}
static void vdi_calc_item_uncompressed_size_tbl( VDI_ITEM * self, const VTable * tbl ) {
    vdi_for_each_item( self, VDI_FOR_EACH_ORDER_SELF_FIRST,
                       vdi_calc_item_uncompressed_size_tbl_cb, ( void * )tbl );
}

/* -------------------------------------------------------------------------------------------------- */

typedef struct VDI_UNCOMPR_SIZE_CTX {
    const VDatabase * db;
    const VTable * tbl;
} VDI_UNCOMPR_SIZE_CTX;

static void vdi_calc_item_uncompressed_size_db_cb( VDI_ITEM * self, void * data ) {
    VDI_ITEM * parent = ( VDI_ITEM * ) self -> parent;  /* remove const-ness */
    if ( NULL != parent && NULL != data ) {
        VDI_UNCOMPR_SIZE_CTX * ctx = data;
        if ( vdi_item_is_col( self ) && NULL != ctx -> tbl ) {
            self -> uncompressed_size = vdi_count_total_bytes_of_column( ctx -> tbl, self -> name );
        } else if ( vdi_item_is_tbl( self ) ) {
            rc_t rc;
            if ( NULL != ctx -> tbl ) {
                rc = VTableRelease( ctx -> tbl );
                ctx -> tbl = NULL;
            }
            rc = VDatabaseOpenTableRead( ctx -> db, &( ctx -> tbl ), "%s", self -> name -> addr );
            if ( 0 != rc ) {
                ctx -> tbl = NULL;
            }
        }
    }
}

static void vdi_collect_item_uncompressed_size_db_cb( VDI_ITEM * self, void * data ) {
    VDI_ITEM * parent = ( VDI_ITEM * ) self -> parent;  /* remove const-ness */
    bool add_2_parent = vdi_item_is_col_or_tbl( self );
    if ( !add_2_parent && NULL != parent ) {
        add_2_parent = vdi_item_is_col_or_tbl( self -> parent );
    }
    if ( add_2_parent ) {
        parent -> uncompressed_size += self -> uncompressed_size;
    }
    self -> compression = percent_of( self -> compressed_size, self -> uncompressed_size );
}

static void vdi_calc_item_uncompressed_size_db( VDI_ITEM * self, const VDatabase * db ) {
    VDI_UNCOMPR_SIZE_CTX ctx;
    ctx . db = db;
    ctx . tbl = NULL;
    vdi_for_each_item( self, VDI_FOR_EACH_ORDER_SELF_FIRST,
                       vdi_calc_item_uncompressed_size_db_cb, &ctx );
    if ( NULL != ctx . tbl ) {
        VTableRelease( ctx . tbl );
    }
    vdi_for_each_item( self, VDI_FOR_EACH_ORDER_SELF_LAST,
                       vdi_collect_item_uncompressed_size_db_cb, NULL );
}
                                                 
/* -------------------------------------------------------------------------------------------------- */

static VDI_ITEM * vdi_build_tree_from_dir( const KDirectory * dir ) {
    VDI_ITEM * root = vdi_make_item( dir, kptDir, ".", NULL );
    if ( NULL != root ) {
        uint64_t compressed_size = vdi_calc_item_compressed_size( root );
        vdi_order_by_compressed_size( root );
        vdi_calc_percent( root, &compressed_size );
    }
    vdh_kdirectory_release( 0, dir );
    return root;
}

static void vdi_inspect_print( VDI_ITEM * self,
                               bool with_compression, dump_format_t format ) {
    VDI_PRINT_CTX ctx;
    ctx . with_compression = with_compression;
    ctx . format = format;
    vdi_print_item( self, &ctx );
    vdi_destroy_item( ( void * )self, NULL );    
}
                                
static void vdi_inspect_db_dir( const VDatabase *db, const KDirectory * dir,
                                bool with_compression, dump_format_t format ) {
    VDI_ITEM * root = vdi_build_tree_from_dir( dir );
    if ( with_compression ) {
        vdi_calc_item_uncompressed_size_db( root, db );        
    }
    vdi_inspect_print( root, with_compression, format );
}

static void vdi_inspect_tbl_dir( const VTable *tbl, const KDirectory * dir,
                                 bool with_compression, dump_format_t format ) {
    VDI_ITEM * root = vdi_build_tree_from_dir( dir );
    if ( with_compression ) {
        vdi_calc_item_uncompressed_size_tbl( root, tbl );
    }
    vdi_inspect_print( root, with_compression, format );    
}

static rc_t vdi_inspect_db( const VDBManager * mgr, const char * object,
                            const VPath * path,
                            bool with_compression, dump_format_t format ) {
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
                vdi_inspect_db_dir( db, dir, with_compression, format );
            }
            rc = vdh_kdatabase_release( rc, k_db );            
        }
        rc = vdh_vdatabase_release( rc, db );
    }
    return rc;
}

static rc_t vdi_inspect_tbl( const VDBManager * mgr, const char * object, 
                             const VPath * path,
                             bool with_compression, dump_format_t format ) {
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
                vdi_inspect_tbl_dir( tbl, dir, with_compression, format );
            }
            rc = vdh_ktable_release( rc, k_tbl );
        }
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}

rc_t vdi_inspect( const KDirectory * dir, const VDBManager * mgr,
                  const char * object, bool with_compression,
                  dump_format_t format ) {
    VPath * path = NULL;
    rc_t rc = vdh_path_to_vpath( object, &path );
    if ( 0 != rc ) {
        ErrMsg( "vdh_path_to_vpath( '%s' ) -> %R", object, rc );            
    } else {
        /* let's check if we can use the object */
        uint32_t path_type = ( VDBManagerPathType ( mgr, "%s", object ) & ~ kptAlias );
        /* types defined in <kdb/manager.h> */
        switch ( path_type ) {
            case kptDatabase        : rc = vdi_inspect_db( mgr, object,
                                                path, with_compression, format ); break;
            case kptPrereleaseTbl   :
            case kptTable           : rc = vdi_inspect_tbl( mgr, object,
                                                path, with_compression, format ); break;
            default                 : rc = KOutMsg( "\tis not a vdb-object\n" ); break;
        }
        rc = vdh_vpath_release( rc, path );
    }
    return rc;
}
