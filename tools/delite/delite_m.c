/*==============================================================================
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
#include <klib/refcount.h>
#include <klib/log.h>
#include <klib/container.h>
#include <klib/pbstree.h>

#include <kfs/file.h>
#include <kfs/mmap.h>
#include <kfs/ramfile.h>

#include "delite.h"
#include "delite_m.h"

#include <sysalloc.h>

#include <byteswap.h>
#include <stdio.h>
#include <string.h>

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Unusuals and stolen stuff
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
#define KMETADATAVERS       2
#define eByteOrderTag       0x05031988
#define eByteOrderReverse   0x88190305
#define NODE_SIZE_LIMIT     ( 25 * 1024 * 1024 )
#define NODE_CHILD_LIMIT    ( 100 * 1024 )

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Some weird appendables ... very weird
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karCBuf {
    void * _b;  /* buf */
    size_t _s;  /* size */
    size_t _c;  /* capacity */
};

#define KAR_CBUF_INCS       32768       /* 1024 * 32 */

static
rc_t CC
_karCBufWhack ( struct karCBuf * self )
{
    if ( self != NULL ) {
        if ( self -> _b != NULL ) {
            free ( self -> _b );
        }
        self -> _b = NULL;
        self -> _s = 0;
        self -> _c = 0;
    }

    return 0;
}   /* _karCBufWhack () */

static
rc_t CC
_karCBufDispose ( struct karCBuf * self )
{
    if ( self != NULL ) {
        _karCBufWhack ( self );

        free ( self );
    }
    return 0;
}   /* _karCBufDispose () */

static
rc_t
_karCBufInit ( struct karCBuf * self, size_t Reserve )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAllocating, rcSelf, rcNull );
    }

    if ( Reserve != 0 ) {
        self -> _b = calloc ( Reserve, sizeof ( char ) );
        if ( self -> _b == NULL ) {
            return RC ( rcApp, rcBuffer, rcAllocating, rcMemory, rcExhausted );
        }
    }

    self -> _c = Reserve;
    self -> _s = 0;

    return 0;
}   /* _karCBufInit () */

/* Reserve could be '0' in that case it will be set randomly later
 */
static
rc_t CC
_karCBufMake ( struct karCBuf ** Buf, size_t Reserve )
{
    rc_t RCt;
    struct karCBuf * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Buf != NULL ) {
        * Buf = NULL;
    }

    if ( Buf == NULL ) {
        return RC ( rcApp, rcBuffer, rcAllocating, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( struct karCBuf ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcBuffer, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        RCt = _karCBufInit ( Ret, Reserve );
        if ( RCt == 0 ) {
            * Buf = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Buf = NULL;

        if ( Ret != NULL ) {
            _karCBufDispose ( Ret );
        }
    } 

    return RCt;
}   /* _karCBufMake () */

static
rc_t
_karCBufRealloc ( struct karCBuf * self, size_t NewSize )
{
    rc_t RCt;
    size_t NewCap;
    void * NewBuf;

    RCt = 0;
    NewCap = 0;
    NewBuf = NULL;

    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAllocating, rcParam, rcNull );
    }

    if ( NewSize == 0 ) {
        return RC ( rcApp, rcBuffer, rcAllocating, rcParam, rcInvalid );
    }

    if ( self -> _c < NewSize  ) {
        NewCap = ( ( NewSize / KAR_CBUF_INCS ) + 1 ) * KAR_CBUF_INCS;

        NewBuf = calloc ( NewCap, sizeof ( char ) );
        if ( NewBuf == NULL ) {
            RCt = RC ( rcApp, rcBuffer, rcAllocating, rcMemory, rcExhausted );
        }
        else {
            if ( self -> _s != 0 ) {
                memmove (
                        NewBuf,
                        self -> _b,
                        sizeof ( char ) * self -> _s
                        );

                free ( self -> _b );
                self -> _b = NULL;
            }

            self -> _b = NewBuf;
            self -> _c = NewCap;
        }
    }

    return RCt;
}   /* _karCBufRealloc () */

static
rc_t
_karCBufSet (
            struct karCBuf * self,
            size_t Off,
            void * Buf,
            size_t BSize
)
{
    rc_t RCt;
    size_t NewSize;

    RCt = 0;
    NewSize = Off + BSize;

    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcSelf, rcNull );
    }

    if ( Buf == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcParam, rcNull );
    }

    if ( BSize == 0 ) {
            /*  I bet it is better to return some smarty RC code here
             */
        return 0;
    }

    if ( self -> _s <= NewSize ) {
        RCt = _karCBufRealloc ( self, NewSize );
    }

    if ( RCt == 0 ) {
        memmove ( ( ( char * ) self -> _b ) + Off, Buf, BSize );
        self -> _s = NewSize;
    }

    return RCt;
}   /* _karCBufSet () */

static
rc_t
_karCBufAppend ( struct karCBuf * self, void * Buf, size_t BSize )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcSelf, rcNull );
    }

    return _karCBufSet ( self, self -> _s, Buf, BSize );
}   /* _karCBufAppend () */

static
rc_t
_karCBufDetatch (
                struct karCBuf * self,
                const void ** Buf,
                size_t * BSize
)
{
    if ( Buf != NULL ) {
        * Buf = NULL;
    }

    if ( BSize != NULL ) {
        * BSize = 0;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcSelf, rcNull );
    }

    if ( Buf == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcParam, rcNull );
    }

    if ( BSize == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcParam, rcNull );
    }

    * Buf = self -> _b;
    * BSize = self -> _s;

    self -> _b = NULL;
    self -> _s = 0;
    self -> _c = 0;

    return 0;
}   /* _karCBufDetach () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Meta data NODE first
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveMDN {
    struct BSTNode _bst_node;

    KRefcount _refcount;

    struct karChiveMDN * _par;
    struct karChiveMD * _meta;

    void * _value;
    size_t _vsize;

    struct BSTree _attr;
    struct BSTree _child;

    char * _name;
};
static const char * _skarChiveMDN_classname = "karChiveMDN";

static
int64_t CC
_karChiveMDNSort ( const struct BSTNode * L, const struct BSTNode * R )
{
    return strcmp ( 
                ( ( const struct karChiveMDN * ) L ) -> _name, 
                ( ( const struct karChiveMDN * ) R ) -> _name
                );
}   /* _karChiveMDNSort () */

static void CC _karChiveMDNAttrWhack ( struct BSTNode * Node, void * Data );
static void CC _karChiveMDNWhack ( struct BSTNode * Node, void * Data );


static
rc_t CC
_karChiveMDNDispose ( struct karChiveMDN * self )
{
    if ( self != NULL ) {
        KRefcountWhack (
                        & ( self -> _refcount ),
                        _skarChiveMDN_classname
                        );

            /*  We do keep copy of that ... so removing
             */
        if ( self -> _value != NULL ) {
            free ( self -> _value );
        }
        self -> _value = NULL;
        self -> _vsize = 0;

            /*  JOJOBA : we should clean children and attributes
             *           later
             */
        BSTreeWhack ( & self -> _attr, _karChiveMDNAttrWhack, NULL );
        BSTreeWhack ( & self -> _child, _karChiveMDNWhack, NULL );

        if ( self -> _name != NULL ) {
            free ( self -> _name );
        }
        self -> _name = NULL;

        free ( self );
    }
    return 0;
}   /* _karChiveMDNDispose () */

static
rc_t CC
_karChiveMDNMakeS (
                struct karChiveMDN ** Node,
                struct karChiveMDN * Parent,
                struct karChiveMD * Meta,
                const char * Name,
                size_t NameLen
)
{
    rc_t RCt;
    struct karChiveMDN * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Node != NULL ) {
        * Node = NULL;
    }

    if ( Node == NULL ) {
        return RC ( rcApp, rcMetadata, rcAllocating, rcParam, rcNull );
    }

    if ( Meta == NULL ) {
        return RC ( rcApp, rcMetadata, rcAllocating, rcParam, rcNull );
    }

/********  Name could be NULL, lol
    if ( Name == NULL ) {
        return RC ( rcApp, rcMetadata, rcAllocating, rcParam, rcNull );
    }
 ********/

    Ret = calloc ( 1, sizeof ( struct karChiveMDN ) );
    if ( Ret == NULL ) {
        return RC ( rcApp, rcMetadata, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        if ( Name != NULL && NameLen != 0 ) {
            RCt = _copyLStringSayNothing (
                                ( const char ** ) & ( Ret -> _name ),
                                Name,
                                NameLen
                                );
        }
        if ( RCt == 0 ) {
            KRefcountInit (
                            & ( Ret -> _refcount ),
                            1,
                            _skarChiveMDN_classname,
                            "_karChiveMDNMake",
                            "MDNMake"
                            );

            BSTreeInit ( & ( Ret -> _attr ) );
            BSTreeInit ( & ( Ret -> _child ) );

            Ret -> _par = Parent;
            Ret -> _meta = Meta;

            * Node = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Node = NULL;

        if ( Ret != NULL ) {
            _karChiveMDNDispose ( Ret );
        }
    }

    return RCt;
}   /* _karChiveMDNMakeS () */

static
rc_t CC
_karChiveMDNMake (
                struct karChiveMDN ** Node,
                struct karChiveMDN * Parent,
                struct karChiveMD * Meta,
                const char * Name
)
{
    return _karChiveMDNMakeS (
                                Node,
                                Parent,
                                Meta,
                                Name,
                                ( Name == NULL ? 0 : strlen ( Name ) )
                                );
}   /* _karChiveMDNMake () */

rc_t CC
karChiveMDNAddRef ( struct karChiveMDN * self )
{
    rc_t RCt = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcMetadata, rcAccessing, rcSelf, rcNull );
    }

    switch (
        KRefcountAdd ( & ( self -> _refcount ), _skarChiveMDN_classname )
    ) {
        case krefOkay :
            break;
        case krefZero :
        case krefLimit :
        case krefNegative :
            RCt = RC ( rcApp, rcMetadata, rcAccessing, rcParam, rcNull );
            break;
        default :
            RCt = RC ( rcApp, rcMetadata, rcAccessing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* karChiveMDNAddRef () */

rc_t CC
karChiveMDNRelease ( struct karChiveMDN * self )
{
    rc_t RCt = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcMetadata, rcReleasing, rcSelf, rcNull );
    }

    switch (
        KRefcountDrop ( & ( self -> _refcount ), _skarChiveMDN_classname )
    ) {
        case krefOkay :
        case krefZero :
            RCt = 0;
            break;
        case krefWhack :
            RCt = _karChiveMDNDispose ( self );
            break;
        case krefNegative :
            RCt = RC ( rcApp, rcMetadata, rcReleasing, rcParam, rcInvalid );
            break;
        default :
            RCt = RC ( rcApp, rcMetadata, rcReleasing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* karChiveMDNRelease () */

void CC
_karChiveMDNWhack ( struct BSTNode * Node, void * Data )
{
    karChiveMDNRelease ( ( struct karChiveMDN * ) Node );
}   /* _karChiveMDNWhack () */

static
int64_t CC
_karChiveMDNFindCallback ( const void * Item, const BSTNode * Node )
{
    return strcmp (
                    ( const char * ) Item,
                    ( ( struct karChiveMDN * ) Node ) -> _name
                    );
}   /* _karChiveMDNFindCallback () */

rc_t CC
karChiveMDNFind (
                const struct karChiveMDN * self,
                const char * NodeName,
                const struct karChiveMDN ** Node
)
{
    rc_t RCt;
    struct karChiveMDN * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Node != NULL ) {
        * Node = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcMetadata, rcSearching, rcSelf, rcNull );
    }

    if ( NodeName == NULL ) {
        return RC ( rcApp, rcMetadata, rcSearching, rcName, rcNull );
    }

    if ( Node == NULL ) {
        return RC ( rcApp, rcMetadata, rcSearching, rcParam, rcNull );
    }

    Ret = ( struct karChiveMDN * ) BSTreeFind (
                                            & ( self -> _child ),
                                            ( const void * ) NodeName,
                                            _karChiveMDNFindCallback
                                            );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcMetadata, rcSearching, rcItem, rcNotFound );
    }

    * Node = Ret;

    return RCt;
}   /* karChiveMDNFind () */

rc_t CC
karChiveMDNAsSting (
                    const struct karChiveMDN * self,
                    const char ** AsString
)
{
    char * Ret = NULL;

    if ( AsString != NULL ) {
        * AsString = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcMetadata, rcAccessing, rcSelf, rcNull );
    }

    if ( AsString == NULL ) {
        return RC ( rcApp, rcMetadata, rcAccessing, rcParam, rcNull );
    }

    if ( self -> _vsize == 0 ) {
        return 0;
    }

    Ret = malloc ( self -> _vsize + 1 );
    if ( Ret == NULL ) {
        return RC ( rcApp, rcMetadata, rcAccessing, rcMemory, rcExhausted );
    }

    memmove ( Ret, self -> _value, self -> _vsize );
    Ret [ self -> _vsize ] = 0;

    * AsString = Ret;

    return 0;
}   /* karChiveMDNAsSting () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  OTTREBUTES now
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveMDNAttr {
    struct BSTNode _bst_node;

    void * _value;
    size_t _vsize;
    char * _name;
};

static
rc_t
_karChiveMDNAttrDispose ( struct karChiveMDNAttr * self )
{
    if ( self != NULL ) {
        if ( self -> _name != NULL ) {
            free ( self -> _name );
        }
        self -> _name = NULL;

        if ( self -> _value != NULL ) {
            free ( self -> _value );
        }
        self -> _value = NULL;

        free ( self );
    }

    return 0;
}   /* _karChiveMDNAttrDispose () */

void CC
_karChiveMDNAttrWhack ( struct BSTNode * Node, void * Data )
{
    _karChiveMDNAttrDispose ( ( struct karChiveMDNAttr * ) Node );
}   /* _karChiveMDNAttrWhack () */

static
rc_t
_karChiveMDNAttrMake (
                        struct karChiveMDNAttr ** Attr,
                        const void * Data,
                        size_t DataSize
)
{
    rc_t RCt;
    char * Name;
    size_t Size;
    struct karChiveMDNAttr * Ret;

    RCt = 0;
    Name = ( char * ) Data;
    Size = strlen ( Name );
    Ret = NULL;

    if ( Size >= DataSize ) {
        return RC ( rcApp, rcMetadata, rcConstructing, rcData, rcCorrupt );
    }

    Ret = calloc ( 1, sizeof ( struct karChiveMDNAttr ) ); 
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcMetadata, rcConstructing, rcMemory, rcExhausted );
    }
    else {
        RCt = _copyLStringSayNothing (
                                ( const char ** ) & ( Ret -> _name ),
                                Name,
                                Size
                                );

        if ( RCt == 0 ) {
            Ret -> _vsize = DataSize - Size - 1;
            Ret -> _value = calloc ( Ret -> _vsize, sizeof ( char ) );
            if ( Ret -> _value == NULL ) {
                RCt = RC ( rcApp, rcMetadata, rcConstructing, rcMemory, rcExhausted );
            }
            else {
                memmove ( Ret -> _value, Name + Size + 1, Ret -> _vsize  );

                * Attr = Ret;
            }
        }
    }

    if ( RCt != 0 ) {
        * Attr = NULL;

        if ( Ret != NULL ) {
            _karChiveMDNAttrDispose ( Ret );
        }
    }

    return RCt;
}   /* _karChiveMDNAttrMake () */

static
int64_t CC
KMAttrNodeSort ( const struct BSTNode * L, const struct BSTNode * R )
{
    return strcmp ( 
                ( ( const struct karChiveMDNAttr * ) L ) -> _name, 
                ( ( const struct karChiveMDNAttr * ) R ) -> _name
                );
}   /* KMAttrNodeSort () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  META DATA last
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
    /*  kinda header for all that trouble
     */
struct karChiveMDH {
    uint32_t _endian;
    uint32_t _version;
};

struct karChiveMD {
    KRefcount _refcount;

    struct karChiveMDN * _root;

    uint32_t _vers;
    uint32_t _rev;

    struct karChiveMDH _hdr;

    bool _bytes_swapped;
};
static const char * _skarChiveMD_classname = "karChiveMD";

    /*  Kinda stealing that from original ... does not know
     *  if we do need that.
     */
struct karChiveMDInflate {
    struct karChiveMD * _meta;
    struct karChiveMDN * _par;

    struct BSTree * _tree;
    size_t _node_size_limit;
    uint32_t _node_child_limit;

    rc_t _rc;
    bool _bytes_swapped;
};

static
rc_t CC
_karChiveMDDispose ( struct karChiveMD * self )
{
    if ( self != NULL ) {
        KRefcountWhack (
                        & ( self -> _refcount ),
                        _skarChiveMD_classname
                        );

        if ( self -> _root != NULL ) {
            karChiveMDNRelease ( self -> _root );
        }
        self -> _root = NULL;

        self -> _vers = 0;
        self -> _rev = 0;
        self -> _bytes_swapped = 0;

        memset ( & ( self -> _hdr ), 0, sizeof ( struct karChiveMDH ) );

        free ( self );
    }

    return 0;
}   /* _karChiveMDDispose () */

static
rc_t CC
_karChiveMDMake ( struct karChiveMD ** Meta )
{
    rc_t RCt;
    struct karChiveMD * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Meta != NULL ) {
        * Meta = NULL;
    }

    if ( Meta == NULL ) {
        return RC ( rcApp, rcMetadata, rcAllocating, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( struct karChiveMD ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcMetadata, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        KRefcountInit (
                        & ( Ret -> _refcount ),
                        1,
                        _skarChiveMD_classname,
                        "_karChiveMDMake",
                        "MDMake"
                        );

        RCt = _karChiveMDNMake (
                        & ( Ret -> _root ),
                        NULL,
                        Ret,
                        NULL
                        );
        if ( RCt == 0 ) {
            * Meta = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Meta = NULL;

        if ( Ret != NULL ) {
            _karChiveMDDispose ( Ret );
        }
    }

    return RCt;
}   /* _karChiveMDMake () */

static
int64_t CC
_karChiveMDNSortCallback (
                        const struct BSTNode * Left,
                        const struct BSTNode * Right
)
{
    return strcmp (
                    ( ( const struct karChiveMDN * ) Left ) -> _name,
                    ( ( const struct karChiveMDN * ) Right ) -> _name
                    );
}   /* _karChiveMDNSortCallback () */

static
bool CC
_karChiveMDNInflate_v1 ( PBSTNode * Node, void * Data )
{
        /*  v1 metadata are flat, with the name
         *  stored as a NUL terminated string
         *  followed by value payload
         */
    const char * Name;
    size_t Size;
    struct karChiveMDInflate * MDI;
    struct karChiveMDN * MDN;

    Name = Node -> data . addr;
    Size = strlen ( Name );
    MDI = ( struct karChiveMDInflate * ) Data;
    MDN = NULL;

    if ( Node -> data . size < Size ) {
        MDI -> _rc = RC ( rcApp, rcMetadata, rcConstructing, rcData, rcCorrupt );
        return true;
    }

    MDI -> _rc = _karChiveMDNMake (
                                & MDN,
                                MDI -> _par,
                                MDI -> _meta,
                                Name
                                );

    if ( MDI -> _rc != 0 ) {
        return true;
    }

    MDN -> _vsize = Node -> data . size - Size - 1;

    if ( MDN -> _vsize == 0 ) {
        MDN -> _value = NULL;
    }
    else {
        MDN -> _value = calloc ( MDN -> _vsize, sizeof ( char ) );
        if ( MDN -> _value == NULL ) {
            MDI -> _rc = RC ( rcApp, rcMetadata, rcConstructing, rcMemory, rcExhausted );
        }
        else {
            memmove ( MDN -> _value, Name + Size + 1, MDN -> _vsize );
        }
    }

    if ( MDI -> _rc == 0 ) {
        BSTreeInsert (
                        MDI -> _tree,
                        & ( MDN -> _bst_node ),
                        _karChiveMDNSortCallback
                        );
        return false;
    }

    if ( MDN != NULL ) {
        _karChiveMDNDispose ( MDN );
    }

    return true;
}   /* _karChiveMDNInflate_v1 () */

static
bool CC
_karChiveMDNInflateAttrNode ( struct PBSTNode * Node, void * Data )
{
    struct karChiveMDNAttr * AttrN;
    struct karChiveMDInflate * MDI;

    AttrN = NULL;
    MDI = ( struct karChiveMDInflate * ) Data;

    MDI -> _rc = _karChiveMDNAttrMake (
                                        & AttrN,
                                        Node -> data . addr,
                                        Node -> data . size
                                        );
    if ( MDI -> _rc == 0 ) {
        BSTreeInsert ( MDI -> _tree, & ( AttrN -> _bst_node ), KMAttrNodeSort );
    }

    return MDI -> _rc != 0;
}   /* _karChiveMDNInflateAttrNode () */

static
rc_t CC
_karChiveMDNInflateAttr ( struct karChiveMDN * Node, bool ByteSwap )
{
    rc_t RCt;
    PBSTree * BST;
    struct karChiveMDInflate MDI;
    size_t BSTS;

    BST = NULL;
    RCt = 0;
    memset ( & MDI, 0, sizeof ( struct karChiveMDInflate ) );
    BSTS = 0;

    RCt = PBSTreeMake (
                        & BST,
                        Node -> _value,
                        Node -> _vsize,
                        ByteSwap
                        );
    if ( RCt != 0 ) {
        RCt = RC ( rcApp, rcMetadata, rcConstructing, rcData, rcCorrupt );
    }
    else {
        BSTS = PBSTreeSize ( BST );

        MDI . _meta = Node -> _meta;
        MDI . _par = Node;
        MDI . _tree = & ( Node -> _attr );
        MDI . _node_size_limit = NODE_SIZE_LIMIT;
        MDI . _node_child_limit = NODE_CHILD_LIMIT;
        MDI . _rc = 0;
        MDI . _bytes_swapped = ByteSwap;
        PBSTreeDoUntil ( BST, 0, _karChiveMDNInflateAttrNode, & MDI );
        RCt = MDI . _rc;

        PBSTreeWhack ( BST );

        Node -> _value = ( ( char* ) ( Node -> _value ) ) + BSTS;
        Node -> _vsize -= BSTS;
    }

    return RCt;
}   /* _karChiveMDNInflateAttr () */

/*  Propuzam
 */
static bool CC _karChiveMDNInflate_v2 ( PBSTNode * Node, void * Data );

static
rc_t CC
_karChiveMDNInflateChild (
                        struct karChiveMDN * Node,
                        size_t NodeSizeLimit,
                        size_t NodeChildLimit,
                        bool ByteSwap
)
{
    rc_t RCt;
    struct PBSTree * PBST;
    uint32_t BstCount;
    size_t BstSize;
    struct karChiveMDInflate MDI;

    RCt = 0;
    PBST = NULL;
    BstCount = 0;
    BstSize = 0;
    memset ( & MDI, 0, sizeof ( struct karChiveMDInflate ) );

    RCt = PBSTreeMake (
                        & PBST,
                        Node -> _value,
                        Node -> _vsize,
                        ByteSwap
                        );
    if ( RCt != 0 ) {
        RCt = RC ( rcApp, rcMetadata, rcConstructing, rcData, rcCorrupt );
    }
    else {
        BstCount = PBSTreeCount ( PBST );
        BstSize = PBSTreeSize ( PBST );

        if ( NodeChildLimit < BstCount ) {
            PLOGMSG ( klogWarn, ( klogWarn,
                                    "refusing to inflate metadata node '$( node)' within file '$(path)': "
                                    "number of children ($(num_children) exceeds limit ($(limit))."
                                    , "node=%s,path=%s,num_children=%u,limit=%u"
                                    , Node -> _name
                                    , "Node -> _meta -> path LOL"
                                    , BstCount
                                    , NodeChildLimit )
                                    );
        }
        else {
            if ( NodeSizeLimit < BstSize ) {
                PLOGMSG ( klogWarn, ( klogWarn,
                                        "refusing to inflate metadata node '$(node)' within file '$(path)': "
                                        "node size ($(node_size)) exceeds limit ($(limit))."
                                        , "node=%s,path=%s,node_size=%zu,limit=%zu"
                                        , Node -> _name
                                        , "Node -> _meta -> path LOL"
                                        , BstSize
                                        , NodeSizeLimit )
                                        );
            }
            else {
                MDI . _meta = Node -> _meta;
                MDI . _par = Node;
                MDI . _tree = & ( Node -> _child );
                MDI . _node_size_limit = NodeSizeLimit;
                MDI . _node_child_limit = NodeChildLimit;
                MDI . _rc = 0;
                MDI . _bytes_swapped = ByteSwap;

                PBSTreeDoUntil ( PBST, 0, _karChiveMDNInflate_v2, & MDI );

                RCt = MDI . _rc;
            }
        }

        PBSTreeWhack ( PBST );

        Node -> _value = ( char * ) ( Node -> _value ) + BstSize;
        Node -> _vsize -= BstSize;
    }

    return RCt;
}   /* _karChiveMDNInflateChild () */

bool CC
_karChiveMDNInflate_v2 ( PBSTNode * Node, void * Data )
{
        /*  v2 names are preceded by a decremented length byte
         *  that has its upper two bits dedicated to
         *  signaling existence of attributes & children
         */
    uint8_t Bits;
    const char * Name;
    size_t Size;
    struct karChiveMDInflate * MDI;
    struct karChiveMDN * MDN;
    void * Val;

    Bits = 0;
    Name = NULL;
    Size = 0;
    MDI = ( struct karChiveMDInflate * ) Data;
    MDN = NULL;
    Val = NULL;

    Name = Node -> data . addr;
    Bits = * ( ( uint8_t * ) Name );
    Name ++;
    Size = ( Bits >> 2 ) + 1;

    if ( Node -> data . size < Size ) {
        MDI -> _rc = RC ( rcApp, rcMetadata, rcConstructing, rcData, rcCorrupt );
        return true;
    }

    MDI -> _rc = _karChiveMDNMakeS (
                                    & MDN,
                                    MDI -> _par,
                                    MDI -> _meta,
                                    Name,
                                    Size
                                    );

    if ( MDI -> _rc != 0 ) {
            /*  Supposing that at any error MDN == NULL
             */
        return true;
    }

/*  JOJOBA
printf ( "[INF] [%s]\n", ( MDN -> _name == NULL ? "NULL" : MDN -> _name ) );
*/

        /*  Chomporary 
         */
    MDN -> _value = ( void * ) ( Name + Size );
    MDN -> _vsize = Node -> data . size - Size - 1;

    if ( ( Bits & 1 ) == 1 ) {
        MDI -> _rc = _karChiveMDNInflateAttr (
                                                MDN,
                                                MDI -> _bytes_swapped
                                                );
    }

    if ( MDI -> _rc == 0 ) {
        if ( ( Bits & 2 ) == 2 ) {
            MDI -> _rc = _karChiveMDNInflateChild (
                                            MDN,
                                            MDI -> _node_size_limit,
                                            MDI -> _node_child_limit,
                                            MDI -> _bytes_swapped
                                            );

        }
    }
    if ( MDI -> _rc == 0 ) {
        if ( MDN -> _vsize == 0 ) {
            MDN -> _value = NULL;
        }
        else {
            Val = calloc ( MDN -> _vsize, sizeof ( char ) );
            if ( Val == NULL ) {
                MDI -> _rc = RC ( rcApp, rcMetadata, rcConstructing, rcMemory, rcExhausted );
            }
            else {
                memmove ( Val, MDN -> _value, MDN -> _vsize );
                MDN -> _value = Val;
            }
        }

        if ( MDI -> _rc == 0 ) {
            BSTreeInsert ( MDI -> _tree, & ( MDN -> _bst_node ), _karChiveMDNSort );

            return false;
        }
    }

    if ( MDN != NULL ) {
        _karChiveMDNDispose ( MDN );
    }

    return true;
}   /* _karChiveMDNInflate_v2 () */

rc_t CC
karChiveMDUnpack (
                struct karChiveMD ** Meta,
                const void * Data,
                size_t DataSize
)
{
    rc_t RCt;
    struct karChiveMD * Ret;
    const struct karChiveMDH * Hdr;
    const void * PBSTSrc;
    Hdr = NULL;
    PBSTSrc = NULL;

    RCt = 0;
    Ret = NULL;

    if ( Meta != NULL ) {
        * Meta = NULL;
    }

    if ( Meta == NULL ) {
        return RC ( rcApp, rcMetadata, rcUnpacking, rcParam, rcNull );
    }

    if ( Data == NULL ) {
        return RC ( rcApp, rcMetadata, rcUnpacking, rcParam, rcNull );
    }

    RCt = _karChiveMDMake ( & Ret );
    if ( RCt == 0 ) {
        Hdr = ( const struct karChiveMDH * ) Data; 
        PBSTSrc = ( const void * ) ( Hdr + 1 );

            /*  Validating header if it is possible :LOL:
             *  JOJOBA !!! IMPORTANT
             *      I did not use protected function from kdb library
             *      and used some hardcoding ... Sorry for that :|
             */
        switch ( Hdr -> _endian ) {
            case eByteOrderTag :
                Ret -> _hdr . _endian = Hdr -> _endian;
                Ret -> _hdr . _version = Hdr -> _version;
                Ret -> _bytes_swapped = false;
                break;
            case eByteOrderReverse :
                Ret -> _hdr . _endian = bswap_32 ( Hdr -> _endian );
                Ret -> _hdr . _version = bswap_32 ( Hdr -> _version );
                Ret -> _bytes_swapped = true;
                break;
            default :
                RCt = RC ( rcApp, rcHeader, rcValidating, rcData, rcCorrupt );
                break;
        }

        if ( RCt == 0 ) {
                /* JOJOBA : DAT BAAAD : hardcoded maximum and
                 *                      minimum versions
                 */
            if (    Ret -> _hdr . _version < 1
                ||  Ret -> _hdr . _version > 2
            ) {
                return RC ( rcApp, rcHeader, rcValidating, rcHeader, rcBadVersion );
            }
        }

            /*  Reading PBSTree
             */
        if ( RCt == 0 ) {
            struct PBSTree * Bst;
            RCt = PBSTreeMake (
                            & Bst,
                            PBSTSrc,
                            DataSize - sizeof ( struct karChiveMDH ),
                            Ret -> _bytes_swapped
                            );
            if ( RCt == 0 ) {
                struct karChiveMDInflate MDI;

                MDI . _meta = Ret;
                MDI . _par = Ret -> _root;
                MDI . _tree = & ( Ret -> _root -> _child );
                MDI . _node_size_limit = NODE_SIZE_LIMIT;
                MDI . _node_child_limit = NODE_CHILD_LIMIT;
                MDI . _rc = 0;
                MDI . _bytes_swapped = Ret -> _bytes_swapped;

                if ( Ret -> _hdr . _version == 1 ) {
                    PBSTreeDoUntil (
                                    Bst,
                                    0,
                                    _karChiveMDNInflate_v1,
                                    & MDI
                                    );
                }
                else {
                    PBSTreeDoUntil (
                                    Bst,
                                    0,
                                    _karChiveMDNInflate_v2,
                                    & MDI
                                    );
                }

                RCt = MDI . _rc;
                Ret -> _vers = Ret -> _hdr . _version;

                PBSTreeWhack ( Bst );

                if ( RCt == 0 ) {
                    * Meta = Ret;
                }
            }
        }

    }

    if ( RCt != 0 ) {
        * Meta = NULL;

        if ( Ret != NULL ) {
            _karChiveMDDispose ( Ret );
        }
    }

    return RCt;
}   /* karChiveMDUnpack () */

static
rc_t CC
_karChivePackWriteFunc (
                void * Param,
                const void * Buf,
                size_t BSize,
                size_t * NumWr
)
{
    rc_t RCt;
    struct karCBuf * cBuf;

    RCt = 0;
    cBuf = ( struct karCBuf * ) Param;

    if ( NumWr != NULL ) {
        * NumWr = 0;
    }

    if ( NumWr == NULL ) {
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

    if ( cBuf == NULL ) {
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

        /*  Nothing to write
         */
    if ( BSize == 0 ) {
        return 0;
    }

    RCt = _karCBufAppend ( cBuf, ( void * ) Buf, BSize );
    if ( RCt == 0 ) {
        * NumWr = BSize;
    }

    return RCt;
}   /* _karChivePackWriteFunc () */

static
rc_t CC
_karChivePackAttrAuxFunc (
                            void * Param,
                            const void * Node,
                            size_t * NumWr,
                            PTWriteFunc WriteFunc,
                            void * WriteParam
)
{
    rc_t RCt;
    struct karChiveMDNAttr * TheNode;
    size_t Total;

    RCt = 0;
    TheNode = ( struct karChiveMDNAttr * ) Node;
    Total = 0;

    if ( NumWr != NULL ) {
        * NumWr = 0;
    }

    if ( TheNode == NULL ) {
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

    if ( NumWr == NULL ) {
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

    if ( WriteFunc != NULL ) {
        RCt = ( * WriteFunc ) (
                                WriteParam,
                                TheNode -> _name,
                                strlen ( TheNode -> _name ) + 1,
                                & Total
                                );
        if ( RCt == 0 ) {
            RCt = ( * WriteFunc ) (
                                WriteParam,
                                TheNode -> _value,
                                TheNode -> _vsize,
                                NumWr
                                );
            * NumWr += Total;
        }
    }
    else {
        * NumWr = strlen ( TheNode -> _name ) + TheNode -> _vsize + 1;
    }

    return RCt;
}   /* _karChivePackAttrAuxFunc () */

static
rc_t CC
_karChivePackAuxFunc (
                    void * Param,
                    const void * Node,
                    size_t * NumWr,
                    PTWriteFunc WriteFunc,
                    void * WriteParam
)
{
    rc_t RCt;
    struct karChiveMDN * TheNode;
    size_t NSize;
    size_t AuxSize;
    uint8_t Bits;

    RCt = 0;
    TheNode = ( struct karChiveMDN * ) Node;
    NSize = 0;
    AuxSize = 0;
    Bits = 0;

    if ( NumWr != NULL ) {
        * NumWr = 0;
    }

    if ( TheNode == NULL ) {
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

    if ( NumWr == NULL ) {
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

    NSize = strlen ( TheNode -> _name );

        /*  first write node name
         */
    if ( WriteFunc != NULL ) {
        Bits = ( uint8_t ) ( NSize - 1 ) << 2;
        if ( TheNode -> _attr . root != NULL )  Bits |= 1;
        if ( TheNode -> _child . root != NULL ) Bits |= 2;

        RCt = ( * WriteFunc ) ( WriteParam, & Bits, 1, NumWr );
        if ( RCt == 0 ) {
            RCt = ( * WriteFunc ) (
                                    WriteParam,
                                    TheNode -> _name,
                                    NSize,
                                    NumWr
                                    );
        }

        if ( RCt != 0 ) {
            return RCt;
        }
    }

        /*  if there are any attributes
        */
    if ( TheNode -> _attr . root != NULL ) {
        RCt = BSTreePersist (
                            & ( TheNode -> _attr ),
                            NumWr,
                            WriteFunc,
                            WriteParam,
                            _karChivePackAttrAuxFunc,
                            NULL
                            );
        if ( RCt != 0 ) {
            return RCt;
        }

        AuxSize += * NumWr;
    }

        /* if there are any children */
    if ( TheNode -> _child . root != NULL ) {
        RCt = BSTreePersist (
                            & ( TheNode -> _child ),
                            NumWr,
                            WriteFunc,
                            WriteParam,
                            _karChivePackAuxFunc,
                            NULL
                            );
        if ( RCt != 0 ) {
            return RCt;
        }

        AuxSize += * NumWr;
    }

        /*  finally write value
         */
    if ( WriteFunc == NULL ) {
        * NumWr = NSize + 1 + AuxSize + TheNode -> _vsize;

        return 0;
    }

    RCt = ( * WriteFunc ) (
                            WriteParam,
                            TheNode -> _value,
                            TheNode -> _vsize,
                            NumWr
                            );
    * NumWr += NSize + 1 + AuxSize;

    return RCt;
}   /* _karChivePackAuxFunc () */


rc_t CC
karChiveMDPack (
                const struct karChiveMD * self,
                const void ** Data,
                size_t * DataSize
)
{
    rc_t RCt;
    struct karChiveMDH Hdr;
    struct karCBuf Buf;

    RCt = 0;
    memset ( & Hdr, 0, sizeof ( struct karChiveMDH ) );

    if ( Data != NULL ) {
        * Data = NULL;
    }

    if ( DataSize != NULL ) {
        * DataSize = 0;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcMetadata, rcWriting, rcSelf, rcNull );
    }

    if ( Data == NULL ) { 
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

    if ( DataSize == NULL ) { 
        return RC ( rcApp, rcMetadata, rcWriting, rcParam, rcNull );
    }

        /*  Writing Header first
         */
    RCt = _karCBufInit ( & Buf, 0 );
    if ( RCt == 0 ) {
        Hdr . _endian = eByteOrderTag;
        Hdr . _version = KMETADATAVERS;

        RCt = _karCBufAppend ( & Buf, & Hdr, sizeof ( Hdr ) );
    }

        /*  Time to persist
         */
    if ( RCt == 0 ) {
        RCt = BSTreePersist (
                            & ( self -> _root -> _child ),  /* Tree */
                            NULL,                           /* NumWr */
                            _karChivePackWriteFunc,         /* WrFnc */
                            & Buf,                          /* WrPrm */
                            _karChivePackAuxFunc,           /* AuxFn */
                            NULL                            /* AuxPr */
                            );
        if ( RCt == 0 ) {
            RCt = _karCBufDetatch ( & Buf, Data, DataSize );
        }
    }

    _karCBufWhack ( & Buf );

    return RCt;
}   /* karChiveMDPack () */

rc_t CC
karChiveMDAddRef ( struct karChiveMD * self )
{
    rc_t RCt = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcMetadata, rcAccessing, rcSelf, rcNull );
    }

    switch (
        KRefcountAdd ( & ( self -> _refcount ), _skarChiveMD_classname )
    ) {
        case krefOkay :
            break;
        case krefZero :
        case krefLimit :
        case krefNegative :
            RCt = RC ( rcApp, rcMetadata, rcAccessing, rcParam, rcNull );
            break;
        default :
            RCt = RC ( rcApp, rcMetadata, rcAccessing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* karChiveMDAddRef () */

rc_t CC
karChiveMDRelease ( struct karChiveMD * self )
{
    rc_t RCt = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcMetadata, rcReleasing, rcSelf, rcNull );
    }

    switch (
        KRefcountDrop ( & ( self -> _refcount ), _skarChiveMD_classname )
    ) {
        case krefOkay :
        case krefZero :
            RCt = 0;
            break;
        case krefWhack :
            RCt = _karChiveMDDispose ( self );
            break;
        case krefNegative :
            RCt = RC ( rcApp, rcMetadata, rcReleasing, rcParam, rcInvalid );
            break;
        default :
            RCt = RC ( rcApp, rcMetadata, rcReleasing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* karChiveMDRelease () */

rc_t CC
karChiveMDRoot (
                const struct karChiveMD * self,
                const struct karChiveMDN ** Root
)
{
    if ( Root != NULL ) {
        * Root = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcAccessing, rcSelf, rcNull );
    }

    if ( Root == NULL ) {
        return RC ( rcApp, rcNode, rcAccessing, rcParam, rcNull );
    }

    * Root = self -> _root;

    return 0;
}   /* karChiveMDRoot () */

rc_t CC
karChiveMDFind (
                const struct karChiveMD * self,
                const char * NodeName,
                const struct karChiveMDN ** Node
)
{
    rc_t RCt;
    const struct karChiveMDN * Root;

    RCt = 0;
    Root = NULL;

    RCt = karChiveMDRoot ( self, & Root );
    if ( RCt == 0 ) {
        RCt = karChiveMDNFind ( Root, NodeName, Node );
    }

    return RCt;
}   /* karChiveMDPack () */
