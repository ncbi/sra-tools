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

#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/container.h>
#include <klib/sort.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/time.h>
#include <sysalloc.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/toc.h>
#include <kfs/sra.h>
#include <kfs/md5.h>

#include <kapp/main.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <endian.h>
#include <byteswap.h>

#include "kar+args.h"
#include "kar+.h"

/********************************************************************
 **  copying string
 ********************************************************************/
rc_t CC kar_stdp ( const char ** Dst, const char * Src )
{
    char * Ret;
    size_t Size;

    Ret = NULL;
    Size = 0;

    if ( Dst != NULL ) {
        * Dst = NULL;
    }

    if ( Dst == NULL || Src == NULL ) {
        return RC (rcExe, rcData, rcCopying, rcParam, rcNull);
    }

    Size = strlen ( Src );

    Ret = ( char * ) calloc ( Size + 1, sizeof ( char ) );
    if ( Ret == NULL ) {
        return RC (rcExe, rcNode, rcAllocating, rcMemory, rcExhausted);
    }

    memmove ( Ret, Src, Size );
    * Dst = Ret;

    return 0;
}   /* kar_stdp () */

/********************************************************************
 **  composes path from top to node, allocates, result should be freed
 **  JOJOBA : That is very poorly made method, need to reimplement
 ********************************************************************/
rc_t CC kar_entry_path ( const char ** Ret, KAREntry * Entry )
{
    char Bm [ 1024 ];
    char Bs [ 1024 ];

    if ( Ret != NULL ) {
        * Ret = NULL;
    }

    if ( Ret == NULL || Entry == NULL ) {
        return RC ( rcExe, rcPath, rcInitializing, rcParam, rcNull );
    }

    * Bm = 0;
    * Bs = 0;
    while ( Entry != NULL ) {
        if ( Entry -> parentDir != NULL ) {
            strcpy ( Bs, Entry -> name );
            strcat ( Bs, "/" );
            strcat ( Bs, Bm );
            strcpy ( Bm, Bs );
        }

        Entry = ( KAREntry * ) Entry -> parentDir;
    }

    size_t Sz = strlen ( Bm );

    if ( Sz == 0 ) {
        return RC ( rcExe, rcPath, rcValidating, rcParam, rcInvalid );
    }

    if ( Bm [ Sz - 1 ] == '/' ) {
        Bm [ Sz - 1 ] = 0;
    }

    return kar_stdp ( Ret, Bm );
}   /* kar_entry_path () */

/********************************************************************
 **  will cal 'Cb' function for node and all it's subnodes in following
 **  order : first will call it for children, and last for node itself.
 **  NOTE : it does not follow soft/hard links to directory
 ********************************************************************/
typedef struct car_entry_for_each_pack car_entry_for_each_pack;
struct car_entry_for_each_pack {
    void * data;
    void ( CC * callback ) ( const KAREntry * Entry, void * data );
};

/* Forward */
void CC kar_entry_for_each (
                const KAREntry * self,
                void ( CC * Cb ) ( const KAREntry * Entry, void * Data),
                void * Data
                );

static
void CC kar_entry_for_each_callback ( BSTNode * Node, void * Data )
{
    if ( Node != NULL ) {
        kar_entry_for_each (
                    ( const KAREntry * ) Node,
                    ( ( car_entry_for_each_pack * ) Data ) -> callback, 
                    ( ( car_entry_for_each_pack * ) Data ) -> data 
                    );
    }
}   /* kar_entry_for_each_callback () */

void CC kar_entry_for_each (
                const KAREntry * self,
                void ( CC * Cb ) ( const KAREntry * Entry, void * Data),
                void * Data
)
{
    if ( self == NULL ) {
        return;
    }

        /*  First children
         */
    if ( self -> eff_type == ktocentrytype_dir ) {
        car_entry_for_each_pack pack;
        pack . data = Data;
        pack . callback = Cb;

        BSTreeForEach ( 
                    & ( ( KARDir * ) self ) -> contents,
                    false,
                    kar_entry_for_each_callback,
                    & pack
                    );
    }

        /*  Itself last
         */
    Cb ( self, Data );
}   /* kar_entry_for_each () */

/********************************************************************
 **  There are 'kar_p2e_XXX' things are stored.
 **  
 **  
 ********************************************************************/
typedef struct kar_p2e_node kar_p2e_node;
struct kar_p2e_node {
    BSTNode dad;

    const char * path;
    KAREntry * entry;
};

static
rc_t CC kar_p2e_node_dispose ( kar_p2e_node * node )
{
    if ( node != NULL ) {
        if ( node -> path != NULL ) {
            free ( ( char * ) node -> path );
            node -> path = NULL;
        }

        free ( node );
    }

    return 0;
}   /* kar_p2e_node_dispose () */

static
rc_t CC kar_p2e_node_create ( kar_p2e_node ** Node, KAREntry * Entry )
{
    rc_t rc;
    kar_p2e_node * Ret;

    rc = 0;
    Ret = NULL;

    if ( Node != NULL ) {
        * Node = NULL;
    }

    if ( Node == NULL || Entry == NULL ) {
        return RC ( rcExe, rcNode, rcAllocating, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( kar_p2e_node ) );
    if ( Ret == NULL ) {
        rc = RC ( rcExe, rcNode, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        rc = kar_entry_path ( & ( Ret -> path ), Entry );
        if ( rc == 0 ) {
            Ret -> entry = Entry;

            * Node = Ret;
        }
    }

    if ( rc != 0 ) {
        * Node = NULL;

        if ( Ret != NULL ) {
            kar_p2e_node_dispose ( Ret );
        }
    }

    return rc;
}   /* kar_p2e_node_create () */

rc_t CC kar_p2e_init ( BSTree * p2e, KAREntry * Entry )
{
    BSTreeInit ( p2e );

    return kar_p2e_scan ( p2e, Entry );
}   /* kar_p2e_init () */

static
void CC kar_p2e_whack_callback ( BSTNode * node, void * data )
{
    kar_p2e_node_dispose ( ( struct kar_p2e_node * ) node );
}   /* kar_p2e_whack_callback () */

rc_t CC kar_p2e_whack ( BSTree * p2e )
{
    BSTreeWhack ( p2e, kar_p2e_whack_callback, NULL );

    return 0;
}   /* kar_p2e_whack () */

static
int64_t CC kar_p2e_add_callback ( const BSTNode * Item, const BSTNode * Node )
{
        /*  JOJOBA ... dull
         */
    return strcmp (
                    ( ( kar_p2e_node * ) Item ) -> path,
                    ( ( kar_p2e_node * ) Node ) -> path
                    );
}   /* kar_p2e_add_callback () */

rc_t CC kar_p2e_add ( BSTree * p2e, KAREntry * Entry )
{
    rc_t rc;
    kar_p2e_node * Node;

    rc = 0;
    Node = NULL;

    rc = kar_p2e_node_create ( & Node, Entry );
    if ( rc == 0 ) {
        rc = BSTreeInsert (
                            p2e,
                            & ( Node -> dad ),
                            kar_p2e_add_callback
                            );
        if ( rc != 0 ) {
            kar_p2e_node_dispose ( Node );
        }
    }

    return rc;
}   /* kar_p2e_add () */


typedef struct kar_p2e_scan_struct kar_p2e_scan_struct;
struct kar_p2e_scan_struct {
    BSTree * p2e;
    rc_t rc;
};

static
void CC kar_p2e_scan_callback ( const KAREntry * Entry, void * Data )
{
        /*  That is root node
         */
    if ( Entry -> name == NULL && Entry -> parentDir == NULL ) {
        return;
    }

    kar_p2e_scan_struct * SC = ( struct kar_p2e_scan_struct * ) Data;
    if ( SC -> rc == 0 ) {
        SC -> rc = kar_p2e_add ( SC -> p2e, ( KAREntry * ) Entry );
    }
}   /* kar_p2e_scan_callback () */

rc_t CC kar_p2e_scan ( BSTree * p2e, KAREntry * Entry )
{
    struct kar_p2e_scan_struct SC;

    SC . p2e = p2e;
    SC . rc = 0;

    kar_entry_for_each ( Entry, kar_p2e_scan_callback, ( void * ) & SC );
    return SC . rc;
}   /* kar_p2e_scan () */

static
int64_t CC kar_p2e_find_callback ( const void * Item, const BSTNode * Node )
{
    return strcmp (
                    ( const char * ) Item,
                    ( ( kar_p2e_node * ) Node ) -> path
                    );
}   /* path2etnry_find_callback() */

KAREntry * CC kar_p2e_find ( BSTree * p2e, const char * Path )
{
    kar_p2e_node * Node = NULL;
    if ( Path != NULL ) {
        Node = ( kar_p2e_node * )
            BSTreeFind ( p2e, Path, kar_p2e_find_callback );
    }
    return Node == NULL ? NULL : ( Node -> entry );
}   /* kar_p2e_find () */

/********************************************************************
 **  Misc TOC methods
 **  
 **  
 ********************************************************************/
size_t CC kar_entry_full_path ( const KAREntry * entry, const char * root_dir, char * buffer, size_t bsize )
{
    size_t offset = 0;
    if ( entry -> parentDir != NULL )
    {
        offset = kar_entry_full_path ( & entry -> parentDir -> dad, root_dir, buffer, bsize );
        if ( offset < bsize )
            buffer [ offset ++ ] = '/';
    }
    else if ( root_dir != NULL && root_dir [ 0 ] != 0 )
    {
        offset = string_copy_measure ( buffer, bsize, root_dir );
        if ( buffer [ offset - 1 ] != '/' && offset < bsize )
            buffer [ offset ++ ] = '/';
    }

    return string_copy_measure ( & buffer [ offset ], bsize - offset, entry -> name ) + offset;
}   /* kar_entry_full_path () */

/********************************************************************
 **  KARWek lives here
 **  
 **  
 ********************************************************************/
struct KARWek {
    void ** wektor;

    size_t size;
    size_t capacity;
    size_t inc_size;
    
    void ( CC * destructor ) ( void * i );
};

rc_t CC kar_wek_clean ( KARWek * self )
{
    if ( self != NULL ) {
        void ** Wek = self -> wektor;
        if ( Wek != NULL && self -> size != 0 ) {
            if ( self -> destructor != NULL ) {
                for ( size_t llp = 0; llp < self -> size; llp ++ ) {
                    if ( Wek [ llp ] != NULL ) {
                        self -> destructor ( Wek [ llp ] );
                    }
                }
            }

            memset ( Wek, 0, sizeof ( void * ) * self -> size );
            self -> size = 0;
        }
    }
    return 0;
}   /* kar_wek_clean () */

static
rc_t CC kar_wek_whack ( KARWek * self )
{
    if ( self != NULL ) {
        kar_wek_clean ( self );

        if ( self -> wektor != NULL ) {
            free ( self -> wektor );

            self -> wektor = NULL;
        }

        self -> size = 0;
        self -> capacity = 0;
        self -> inc_size = 0;
        self -> destructor = NULL;
    }

    return 0;
}   /* kar_wek_whack () */

rc_t CC kar_wek_dispose ( KARWek * self )
{
    if ( self != NULL ) {
        kar_wek_whack ( self );

        free ( self );
    }

    return 0;
}   /* kar_wek_dispose () */

static
rc_t CC kar_wek_realloc ( KARWek * self, size_t size )
{
    size_t NewCap;
    size_t IncSize;

    NewCap = 0;
    IncSize = 16;

    if ( self == NULL ) {
        return RC ( rcExe, rcVector, rcAllocating, rcSelf, rcNull );
    }

    if ( size == 0 ) {
        return 0;
    }

    IncSize = self -> inc_size == 0 ? 16 : self -> inc_size;

    NewCap = ( ( size / IncSize ) + 1 ) * IncSize;
    if ( self -> capacity < NewCap ) {
        void ** NewWek = calloc ( NewCap, sizeof ( void * ) );
        if ( NewWek == NULL ) {
            return RC ( rcExe, rcVector, rcAllocating, rcMemory, rcExhausted );
        }

        if ( self -> wektor != NULL ) {
            if ( self -> size != 0 ) {
                memmove (
                        NewWek,
                        self -> wektor,
                        sizeof ( void * ) * self -> size
                        );
            }

            free ( self -> wektor );
            self -> wektor = NULL;
        }

        self -> wektor = NewWek;
        self -> capacity = NewCap;
    }

    return 0;
}   /* kar_wek_realloc () */


static
rc_t CC kar_wek_init (
                        KARWek * self,
                        size_t initial_capacity,
                        size_t increment_size,
                        void ( CC * destructor ) ( void * i )
)
{
    rc_t rc = 0;

    if ( self == NULL ) {
        return RC ( rcExe, rcVector, rcAllocating, rcSelf, rcNull );
    }


    self -> wektor = NULL;
    self -> size = 0;
    self -> inc_size = increment_size == 0 ? 16 : increment_size;
    self -> capacity = 0;

    if ( initial_capacity != 0 ) {
        rc = kar_wek_realloc ( self, initial_capacity );
    }

    return rc;
}   /* kar_wek_init () */

rc_t CC kar_wek_make (
                    KARWek ** Wek,
                    size_t initial_capacity,
                    size_t increment_size,
                    void ( CC * destructor ) ( void * i )
)
{
    rc_t rc;
    KARWek * Ret;

    rc = 0;
    Ret = NULL;

    if ( Wek != NULL ) {
        * Wek = NULL;
    }

    if ( Wek == NULL ) {
        return RC ( rcExe, rcVector, rcAllocating, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( KARWek ) );
    if ( Ret == NULL ) {
        rc = RC ( rcExe, rcVector, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        rc = kar_wek_init (
                            Ret,
                            initial_capacity,
                            increment_size,
                            destructor
                            );
        if ( rc == 0 ) {
            * Wek = Ret;
        }
    }

    return rc;
}   /* kar_wek_make () */

rc_t CC kar_wek_make_def ( KARWek ** Wek )
{
    return kar_wek_make ( Wek, 0, 0, NULL );
}   /* kar_wek_make_def () */

rc_t CC kar_wek_append ( KARWek * self, void * Item )
{
    rc_t rc = 0;

    if ( self == 0 ) {
        return RC ( rcExe, rcVector, rcAppending, rcSelf, rcNull );
    }

/* Not sure about that check
    if ( Item == 0 ) {
        return RC ( rcExe, rcVector, rcAppending, rcParam, rcNull );
    }
*/

    rc = kar_wek_realloc ( self, self -> size + 1 );
    if ( rc == 0 ) {
        self -> wektor [ self -> size ] = Item;
        self -> size ++;
    }

    return rc;
}   /* kar_wek_append () */

size_t CC kar_wek_size ( KARWek * self )
{
    return self == NULL ? 0 : self -> size;
}   /* kar_wek_size () */

void ** CC kar_wek_data ( KARWek * self )
{
    return self == NULL ? 0 : self -> wektor;
}   /* kar_wek_data () */

void * CC kar_wek_get ( KARWek * self, size_t Idx )
{
    if ( self != NULL ) {
        if ( 0 <= Idx && Idx < self -> size ) {
            return self -> wektor [ Idx ];
        }
    }

    return NULL;
}   /* kar_wek_get () */

/********************************************************************
 **  Misk methods
 **  
 **  
 ********************************************************************/
static
void CC list_files_callback ( const KAREntry * Entry, void * Data )
{
    if ( Entry != NULL && Data != NULL ) {
        if ( Entry -> eff_type == ktocentrytype_file
            || Entry -> eff_type == ktocentrytype_chunked ) {
            if ( ( ( KARFile * ) Entry ) -> byte_size != 0 ) {
                kar_wek_append ( ( KARWek * ) Data, ( void * ) Entry );
            }
        }
    }
}   /* list_files_callback () */

rc_t CC kar_entry_list_files ( const KAREntry * self, KARWek ** Files )
{
    rc_t rc;
    KARWek * Wek;

    rc = 0;
    Wek = NULL;

    if ( Files != NULL ) {
        * Files = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcExe, rcVector, rcInitializing, rcSelf, rcNull );
    }

    if ( Files == NULL ) {
        return RC ( rcExe, rcVector, rcInitializing, rcParam, rcNull );
    }

    rc = kar_wek_make_def ( & Wek );
    if ( rc == 0 ) {
        kar_entry_for_each ( self, list_files_callback, Wek );

        * Files = Wek;
    }

    return rc;
}   /* kar_entry_list_files () */

typedef struct D3D D3D;
struct D3D {
    bool ( CC * filter ) ( const KAREntry * Entry, void * Data );
    void * data;
    KARWek * wek;
};

static
void CC filter_files_callback ( const KAREntry * Entry, void * Data )
{
    if ( Entry != NULL && Data != NULL ) {
        D3D * d3d = ( D3D * ) Data;
        if ( d3d -> filter != NULL ) {
            if ( d3d -> filter ( Entry, d3d -> data ) ) {
                kar_wek_append ( d3d -> wek, ( void * ) Entry );

            }
        }
    }
}   /* filter_files_callback () */

rc_t CC kar_entry_filter_files (
                            const KAREntry * self,
                            KARWek ** Files,
                            bool ( CC * Filter ) (
                                                const KAREntry * Entry,
                                                void * Data
                                                ),
                            void * Data
)
{
    rc_t rc;
    KARWek * Wek;

    rc = 0;
    Wek = NULL;

    if ( Files != NULL ) {
        * Files = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcExe, rcVector, rcInitializing, rcSelf, rcNull );
    }

    if ( Files == NULL ) {
        return RC ( rcExe, rcVector, rcInitializing, rcParam, rcNull );
    }

    rc = kar_wek_make_def ( & Wek );
    if ( rc == 0 ) {
        D3D d3d;
        d3d . filter = Filter;
        d3d . data = Data;
        d3d . wek = Wek;

        kar_entry_for_each (
                            self,
                            filter_files_callback,
                            ( void * ) & d3d
                            );

        * Files = Wek;
    }

    return rc;
}   /* kar_entry_filter_files () */


bool CC filter_flag_filter ( const KAREntry * Entry, void * Data )
{
    return ( Entry != NULL && Data != NULL )
                        ? ( Entry -> the_flag == * ( ( bool * ) Data ) )
                        : false
                        ;
}   /* filter_flag_filter () */

rc_t CC kar_entry_filter_flag (
                                const KAREntry * self,
                                KARWek ** Files,
                                bool FlagValue
)
{
    return kar_entry_filter_files (
                                self,
                                Files,
                                filter_flag_filter,
                                ( void * ) & FlagValue
                                );
}   /* kar_entry_filter_flag () */

static
void CC set_flag_callback ( const KAREntry * Entry, void * Data )
{
    if ( Entry != NULL && Data != NULL ) {
        ( ( KAREntry * ) Entry ) -> the_flag = * ( ( bool * ) Data );
    }
}   /* set_flag_callback () */

rc_t CC kar_entry_set_flag ( const KAREntry * self, bool Value )
{
    rc_t rc;

    rc = 0;

    if ( self == NULL ) {
        return RC ( rcExe, rcVector, rcAccessing, rcSelf, rcNull );
    }

    kar_entry_for_each ( self, set_flag_callback, & Value );

    return rc;
}   /* kar_entry_set_flag () */

#ifdef JOJOBA
Temporary removing it from here
/********************************************************************
 **  General KAREntry methods are here
 **  
 **  
 ********************************************************************/
void kar_entry_whack ( BSTNode *node, void *data )
{
    KAREntry *entry = ( KAREntry * ) node;

    if ( entry -> type == kptDir ) {
        BSTreeWhack (
                    & ( ( KARDir * ) entry ) -> contents,
                    kar_entry_whack,
                    NULL
                    );
    }

    /* do the cleanup */
    switch ( entry -> type )
    {
    case kptAlias:
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        free ( ( void * ) ( ( KARAlias * ) entry ) -> link );
        break;
    }

    free ( entry );
}

rc_t kar_entry_create ( KAREntry ** rtn, size_t entry_size,
    const KDirectory * dir, const char * name, uint32_t type,
    uint8_t eff_type
)
{
    rc_t rc;

    size_t name_len = strlen ( name ) + 1;
    KAREntry * entry = calloc ( 1, entry_size + name_len );
    if ( entry == NULL )
    {
        rc = RC (rcExe, rcNode, rcAllocating, rcMemory, rcExhausted);
        pLogErr ( klogErr, rc, "Failed to allocated memory for entry '$(name)'",
                  "name=%s", name );
    }
    else
    {
        /* location for string copy */
        char * dst = & ( ( char * ) entry ) [ entry_size ];

        /* sanity check */
        assert ( entry_size >= sizeof * entry );

        /* populate the name by copying to the end of structure */
        memmove ( dst, name, name_len );
        entry -> name = dst;

        entry -> type = type;
        entry -> eff_type = eff_type;

        rc = KDirectoryAccess ( dir, & entry -> access_mode, "%s", entry -> name );
        if ( rc != 0 )
        {
            pLogErr ( klogErr, rc, "Failed to get access mode for entry '$(name)'",
                      "name=%s", entry -> name );
        }
        else
        {
            rc = KDirectoryDate ( dir, &entry -> mod_time, "%s", entry -> name );
            if ( rc != 0 )
            {
                pLogErr ( klogErr, rc, "Failed to get modification for entry '$(name)'",
                          "name=%s", entry -> name );
            }
            else
            {
                * rtn = entry;
                return 0;
            }
        }

        free ( entry );
    }

    * rtn = NULL;
    return rc;
}
#endif /* JOJOBA */
