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

#include <klib/text.h>
#include <klib/rc.h>
#include <klib/container.h>
#include <klib/refcount.h>
#include <klib/log.h>
#include <klib/pbstree.h>
#include <klib/sort.h>

#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/mmap.h>
#include <kfs/sra.h>
#include <kfs/impl.h>

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include "delite_k.h"
#include "delite_d.h"
#include "delite_m.h"
#include "delite.h"

#include <sysalloc.h>

#include <byteswap.h>
#include <stdio.h>
#include <string.h>

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Kinda rated K content ...
 *  KAR file
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Kar will return directory
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Useful forwards
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Misk Archive entries
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

struct karChiveDir;

struct karChiveEntry
{
    struct BSTNode _da_da_dad;

    KRefcount _refcount;

    struct karChiveDir * _parent;

    const char * _name;
    KTime_t _mod_time;
    uint32_t _access_mode;
    uint8_t _type;
};  /* struct karChiveEntry */

static const char * _skarChiveEntry_classname = "karChiveEntry";

struct karChiveDir
{
    struct karChiveEntry _da_da_dad;
    
    struct BSTree _entries;
};  /* struct karChiveDir */

struct karChiveFile
{
    struct karChiveEntry _da_da_dad;

    /* JOJOBA - put memory file here */

    const struct karChiveDS * _data_source;

    uint64_t _byte_offset;
    uint64_t _byte_size;

    uint64_t _new_byte_offset;
    uint64_t _new_byte_size;
};  /* struct karChiveFile */

struct karChiveAlias
{
    struct karChiveEntry _da_da_dad;

    struct karChiveEntry * _resolved;

    const char * _link;

    bool _is_invalid;
};  /* struct karChiveAlias */

enum
{
    karChive_unknown = -1,
    karChive_notfound,
    karChive_dir,
    karChive_file,
    karChive_chunked,
    karChive_softlink,
    karChive_hardlink,
    karChive_emptyfile,
    karChive_zombiefile
};

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Something special
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/*  That structure will contain crawling info for read or dup nodes
 */
struct FrogBrigade {
    struct karChiveDir * _parent;

    bool _bytes_swapped;

    rc_t _rc;
};

/*  That structure will contain stuff common for all entries
 */
struct LesClaypool {
    const void * _data;
    size_t _data_len;
    size_t _position;

    char * _name;       /* don't forget to free that resource */
    uint64_t _mod_time;
    uint32_t _access_mode;
    uint8_t _type_code;
};

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Struct karChive
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChive {
    KRefcount _refcount;

    const struct KMMap * _map;
    const void * _map_addr;
    size_t _map_size;

        /* HDR */
    struct KSraHeader * _hdr;       /* KAR header */
    bool _hdr_bytes_swapped;        /* Good one, WAKANDA forever */

        /* TOC, etc */
    struct karChiveDir * _root;     /* table of context */

// JOJOBA - continue

};  /* struct karChive */

static const char * _skarChive_classname = "karChive";

static bool _karChiveBytesSwapped ( const struct karChive * self );
static uint32_t _karChiveVersion ( const struct karChive * self );
static uint64_t _karChiveDataOffset ( const struct karChive * self );
static uint64_t _karChiveHdrSize ( const struct karChive * self );
    /* Lyrics : TOC size = _data_offest - _hdr_size */
    /* CUZ: KAR structure is HDR | TOC | DATA_FILES */
static uint64_t _karChiveTocSize ( const struct karChive * self );
static uint64_t _karChiveTocOffset ( const struct karChive * self );

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Used many times. Don't know why I put it here
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
int64_t
_compareEntries ( const BSTNode * left, const BSTNode * right )
{
            /*  JOJOBA : very dangerous ...
             */
    return strcmp (
                ( ( struct karChiveEntry * ) left ) -> _name,
                ( ( struct karChiveEntry * ) right ) -> _name
                );
}   /* _compareEntries () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Some entries could have a babies. There we providing free service
 *  which allow to locate and adopt some
 *  I know, these suppose to be done through virtual table, but ...
 *  only two methods and so much hasselblat
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
int64_t
_karChiveEntryFindCallback (
                            const void * Item,
                            const struct BSTNode * Node
)
{
    return strcmp ( 
                    ( const char * ) Item,
                    ( ( const struct karChiveEntry * ) Node ) -> _name
                    );
}   /* _karChiveEntryFindCallback () */

static
rc_t
_karChiveEntryFind (
                    const struct karChiveEntry ** Entry,
                    const struct karChiveEntry * self,
                    const char * Name
)
{
    rc_t RCt;
    const struct karChiveEntry * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Entry != NULL ) {
        * Entry = NULL;
    }

    if ( self == NULL ) {
        RCt = RC ( rcApp, rcNode, rcListing, rcSelf, rcNull );
    }

    if ( Entry == NULL ) {
        RCt = RC ( rcApp, rcNode, rcListing, rcParam, rcNull );
    }

    if ( Name == NULL ) {
        RCt = RC ( rcApp, rcNode, rcListing, rcParam, rcNull );
    }

    if ( self -> _type == kptDir ) {
        struct karChiveDir * Dir = ( struct karChiveDir * ) self;

        Ret = ( const struct karChiveEntry * ) BSTreeFind (
                                            & ( Dir -> _entries ),
                                            ( const void * ) Name,
                                            _karChiveEntryFindCallback
                                            );
        if ( Ret == NULL ) {
            RCt = RC ( rcApp, rcNode, rcSearching, rcItem, rcNotFound );
        }
        else {
            * Entry = Ret;
        }
    }
    else {
        if ( self -> _type == ( kptDir | kptAlias ) ) {
            struct karChiveAlias * Alias =
                                    ( struct karChiveAlias * ) self;
            if ( ! Alias -> _is_invalid ) {
                RCt = _karChiveEntryFind (
                                        Entry,
                                        Alias -> _resolved,
                                        Name
                                        );
            }
        }
        else {
             RCt = RC ( rcApp, rcNode, rcSearching, rcItem, rcNotFound );
        }
    }

    return RCt;
}   /* _karChiveEntryFind () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Struct karChive Resolving node by path
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karCVec {
    char ** _t;     /* tokeh */
    uint32_t _q;    /* qty */
    uint32_t _c;    /* capacity */
};

static
rc_t
_karCVecClear ( struct karCVec * self )
{
    int llp = 0;

    if ( self != NULL ) {
        if ( self -> _t != NULL ) {
            for ( llp = 0; llp < self -> _q; llp ++ ) {
                if ( self -> _t [ llp ] != NULL ) {
                    free ( self -> _t [ llp ] );
                    self -> _t [ llp ] = NULL;
                }
            }
        }
        self -> _q = 0;
    }

    return 0;
}   /* _karCVecClear () */

static
rc_t
_karCVecDispose ( struct karCVec * self )
{
    if ( self != NULL ) {

        _karCVecClear ( self );

        if ( self -> _t != NULL ) {
            free ( self -> _t );
        }
        self -> _t = NULL;
        self -> _q = 0;
        self -> _c = 0;

        free ( self );
    }

    return 0;
}   /* _karCVecDispose () */

static
rc_t
_karCVecMake ( struct karCVec ** Vec )
{
    rc_t RCt;
    struct karCVec * Ret;

    RCt = 0;
    Ret = NULL;

    Ret = calloc ( 1, sizeof ( struct karCVec ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcVector, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        * Vec = Ret;
    }

    return RCt;
}   /* _karCVecMake () */

static
rc_t
_karCVecRealloc ( struct karCVec * self, uint32_t Qty )
{
    rc_t RCt;
    char ** NewT;
    uint32_t NewC;

    RCt = 0;
    NewT = NULL;
    NewC = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcVector, rcAllocating, rcParam, rcNull );
    }

#define kCVI 16
    NewC = ( ( Qty / kCVI ) + 1 ) * kCVI;

    if ( self -> _c < NewC ) {
        NewT = calloc ( NewC, sizeof ( char * ) );
        if ( NewT == NULL ) {
            RCt = RC ( rcApp, rcVector, rcAllocating, rcMemory, rcExhausted );
        }
        else {
            if ( self -> _q != 0 ) {
                    /* JOJOBA: that is stupid situation, there should
                     * be check that _t is not null, that is possilbe
                     * lol
                     */
                memmove (
                        NewT,
                        self -> _t,
                        sizeof ( char * ) * self -> _q
                        );

                free ( self -> _t );
            }

            self -> _t = NewT;
            self -> _c = NewC;
        }
    }
#undef kCVI

    return RCt;
}   /* _karCVecRealloc () */

    /*  JOJOBA: do not free Token ... It becames a property of vector
     */
static
rc_t
_karCVecAdd ( struct karCVec * self, const char * Token )
{
    rc_t RCt;

    RCt = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcVector, rcInserting, rcParam, rcNull );
    }

        /*  stripping unnecessary stuff
         */
    if ( strcmp ( Token, "." ) == 0 ) {
        free ( ( char * ) Token );
        return 0;
    }

        /*  stripping unnecessary stuff
         */
    if ( strcmp ( Token, "" ) == 0 ) {
        free ( ( char * ) Token );
        return 0;
    }

    RCt = _karCVecRealloc ( self, self -> _q + 1 );
    if ( RCt == 0 ) {
        self -> _t [ self -> _q ] = ( char * ) Token; /* discarding
                                                       * const, lol
                                                       */
        self -> _q ++;
    }

    return RCt;
}   /* _karCVecAdd () */

static
rc_t
_karCVecAddS (
                struct karCVec * self,
                const char * Start,
                const char * End
)
{
    rc_t RCt;
    const char * Str;

    RCt = 0;
    Str = NULL;

    RCt = _copySStringSayNothing ( & Str, Start, End );
    if ( RCt == 0 ) {
        RCt = _karCVecAdd ( self, Str );
    }

    return RCt;
}   /* _karCVecAddS () */

static
rc_t
_karCVecTokenize ( struct karCVec ** Tokeh, const char * Path )
{
    rc_t RCt;
    struct karCVec * Ret;
    const char * Bg;
    const char * En;
    const char * Pz;

    RCt = 0;
    Ret = NULL;
    Bg = NULL;
    En = NULL;
    Pz = NULL;

    if ( Tokeh != NULL ) {
        * Tokeh = NULL;
    }

    if ( Tokeh == NULL ) { 
        return RC ( rcApp, rcVector, rcTokenizing, rcParam, rcNull );
    }

    if ( Path == NULL ) {
        return RC ( rcApp, rcVector, rcTokenizing, rcParam, rcNull );
    }

        /*  We only tokenize relative paths
         */
    if ( * Path == '/' ) {
        return RC ( rcApp, rcVector, rcTokenizing, rcParam, rcInvalid );
    }

        /*  First we do create contanment for tokens
         */
    RCt = _karCVecMake ( & Ret );
    if ( RCt == 0 ) {

            /*  Tokenizing
             */
        Bg = Pz = Path;
        En = Bg + KAR_MAX_ACCEPTED_STRING;    /* sorry for that LOL */

        while ( * Pz != 0 ) {
            if ( En <= Pz ) {
                RCt = RC ( rcApp, rcString, rcTokenizing, rcParam, rcTooBig );
                break;
            }

            if ( * Pz == '/' ) {
                if ( 0 < Pz - Bg ) {
                    RCt = _karCVecAddS ( Ret, Bg, Pz );
                    if ( RCt != 0 ) {
                        break;
                    }
                }

                Pz ++;
                Bg = Pz;
            }

            Pz ++;
        }

        if ( RCt == 0 ) {
            if ( En <= Pz ) {
                RCt = RC ( rcApp, rcString, rcTokenizing, rcParam, rcTooBig );
            }
            else {
                if ( 0 < Pz - Bg ) {
                    RCt = _karCVecAddS ( Ret, Bg, Pz );
                }

                if ( RCt == 0 ) {
                    * Tokeh = Ret;
                }
            }
        }
    }

    if ( RCt != 0 ) {
        * Tokeh = NULL;

        if ( Ret != NULL ) {
            _karCVecDispose ( Ret );
        }
    }

    return RCt;
}   /* _karCVecTokenize () */

/*  Next two methods are stupid, but it is better to use them instead
 *  using values in structure. Cuz that crapagne could be moved or 
 *  it's members could be renamed ...
 */
static
size_t
_karCVecQty ( struct karCVec * self )
{
    return self == NULL ? 0 : ( self -> _q );
}   /* _karCVecQty () */

static
const char *
_karCVecGet ( struct karCVec * self, size_t Idx )
{
    if ( self != NULL ) {
        if ( Idx < self -> _q ) {
            return self -> _t [ Idx ];
        }
    }

    return NULL;
}   /* _karCVecGet () */

static
rc_t
_karChiveResolvePath (
                    const struct karChiveEntry ** Found,
                    const struct karChiveDir * Root,
                    const char * Path
)
{
    rc_t RCt;
    struct karCVec * Tokeh;
    size_t llp;
    const char * Name;
    struct karChiveEntry * Curr;
    struct karChiveEntry * CurrMore;

    RCt = 0;
    Tokeh = NULL;
    llp = 0;
    Name = NULL;
    Curr = NULL;
    CurrMore = NULL;

    RCt = _karCVecTokenize ( & Tokeh, Path );
    if ( RCt == 0 ) {
        if ( _karCVecQty ( Tokeh ) == 0 ) {
                /*  Very simple case ... clinique :LOL:
                 */
            * Found = ( const struct karChiveEntry * ) Root;
        }
        else {
                /*  Here we do traverse back and forth
                 */
            Curr = ( struct karChiveEntry * ) Root;
            for ( llp = 0; llp < _karCVecQty ( Tokeh ); llp ++ ) {
                Name = _karCVecGet ( Tokeh, llp );

                if ( Curr == NULL ) {
                        /*  Name exist, and directory does not
                         */
                    RCt = RC ( rcApp, rcPath, rcResolving, rcItem, rcNotFound );
                    break;
                }

                if ( strcmp ( Name, ".." ) == 0 ) {
                        /*  one level up
                         */
                    CurrMore = ( struct karChiveEntry * ) Curr -> _parent;
                }
                else {
                    RCt = _karChiveEntryFind (
                            ( const struct karChiveEntry  ** ) & CurrMore,
                            Curr,
                            Name
                            );
                    if ( RCt != 0 ) {
                        break;
                    }

                    if ( ( CurrMore -> _type & kptAlias ) == kptAlias ) {

                        CurrMore = ( ( const struct karChiveAlias * ) CurrMore ) -> _resolved;
                    }
                }

                Curr = CurrMore;
            }

            if ( Curr != NULL ) {
                * Found = Curr;
            }
            else {
                RCt = RC ( rcApp, rcPath, rcResolving, rcItem, rcNotFound );
            }
        }

        _karCVecDispose ( Tokeh );
    }

    return RCt;
}   /* _karChiveResolvePath () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Struct karChive TOC - continued
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static rc_t _karChiveEntryRelease ( const struct karChiveEntry * self );

static
void
_karChiveEntryNodeWhack ( struct BSTNode * Node, void * Data )
{
    _karChiveEntryRelease ( ( const struct karChiveEntry * ) Node );
}   /* _karChiveEntryNodeWhack () */

rc_t 
_karChiveEntryDispose ( const struct karChiveEntry * self )
{
    struct karChiveEntry * Entry = ( struct karChiveEntry * ) self;
    if ( Entry != NULL ) {
        KRefcountWhack (
                        & ( Entry -> _refcount ),
                        _skarChiveEntry_classname
                        );
        switch ( Entry -> _type ) {
            case kptFile: 
                {
                    struct karChiveFile * File = ( struct karChiveFile * ) Entry;

                    if ( File -> _data_source != NULL ) {
                        karChiveDSRelease ( File -> _data_source );
                    }

                    File -> _data_source = NULL;
                    File -> _byte_offset = 0;
                    File -> _byte_size = 0;
                    File -> _new_byte_offset = 0;
                    File -> _new_byte_size = 0;
                }
                break;
            case kptDir:
                { 
                    struct karChiveDir * Dir = ( struct karChiveDir * ) Entry;
                    BSTreeWhack (
                                & ( Dir -> _entries ),
                                _karChiveEntryNodeWhack,
                                NULL
                                );
                }
                break;
            case kptAlias:
                { 
                    struct karChiveAlias * Alias = ( struct karChiveAlias * ) Entry;
                    if ( Alias -> _link != NULL ) {
                        free ( ( char * ) Alias -> _link );
                        Alias -> _link = NULL;
                    }
                    Alias -> _resolved = NULL;
                    Alias -> _is_invalid = false;
                }
                break;
            default:
                break;
        }

        Entry -> _parent = NULL;
        if ( Entry -> _name != NULL ) {
            free ( ( char * ) Entry -> _name );
        }
        Entry -> _name = NULL;
        Entry -> _mod_time = 0;
        Entry -> _access_mode = 0;
        Entry -> _type = 0;

        free ( Entry );
    }

    return 0;
}   /* _karChiveEntryDispose () */

static
rc_t
_karChiveEntryAddRef ( const struct karChiveEntry * self )
{
    rc_t RCt;
    struct karChiveEntry * Entry;

    RCt = 0;
    Entry = ( struct karChiveEntry * ) self;

    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcAccessing, rcSelf, rcNull );
    }

    switch (
        KRefcountAdd ( & ( Entry -> _refcount ), _skarChiveEntry_classname )
    ) {
        case krefOkay :
            RCt = 0;
            break;
        case krefZero :
        case krefLimit :
        case krefNegative :
            RCt = RC ( rcApp, rcArc, rcAccessing, rcParam, rcNull );
            break;
        default :
            RCt = RC ( rcApp, rcArc, rcAccessing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* _karChiveEntryAddRef () */

static
rc_t
_karChiveEntryRelease ( const struct karChiveEntry * self )
{
    rc_t RCt;
    struct karChiveEntry * Entry;

    RCt = 0;
    Entry = ( struct karChiveEntry * ) self;

    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcReleasing, rcSelf, rcNull );
    }

    switch (
        KRefcountDrop ( & ( Entry -> _refcount ), _skarChiveEntry_classname )
    ) {
        case krefOkay :
        case krefZero :
            RCt = 0;
            break;
        case krefWhack :
            RCt = _karChiveEntryDispose ( Entry );
            break;
        case krefNegative :
            RCt = RC ( rcApp, rcArc, rcReleasing, rcParam, rcInvalid );
            break;
        default :
            RCt = RC ( rcApp, rcArc, rcReleasing, rcParam, rcUnknown );
            break;
    }
    return RCt;
}   /* _karChiveEntryRelease () */

static
rc_t
_karChiveTOCDispose ( const struct karChive * Chive )
{
    struct karChive * TheChive = ( struct karChive * ) Chive;

    if ( TheChive -> _root != NULL ) {
        _karChiveEntryRelease (
                    ( const struct karChiveEntry * ) TheChive -> _root
                    );

        TheChive -> _root = NULL;
    }

    return 0;
}   /* _karChiveTOCDispose () */

static
const struct karChiveDir *
_karChiveEntryParent ( const struct karChiveEntry * self )
{
    return self == NULL ? NULL : ( self -> _parent );
}   /* _karChiveEntryParetn () */

/*  Resolving aliases - using Kurt Jr. code
 */
static
struct karChiveEntry *
_karChiveDealiasRecursively ( struct karChiveAlias * Alias )
{
    rc_t RCt;
    struct karChiveEntry * Found;
    struct karChiveDir * Root;
    const char * Path;

    RCt = 0;
    Found = NULL;
    Root = NULL;
    Path = NULL;

    if ( Alias == NULL ) {
        return NULL;
    }

        /*  We already were here
         */
    if ( Alias -> _is_invalid ) {
        return NULL;
    }

    Root = ( struct karChiveDir * ) _karChiveEntryParent ( & ( Alias -> _da_da_dad ) );

    if ( Root == NULL ) {
        return NULL;
    }

    Path = Alias -> _link;
    if ( Path == NULL ) {
        return NULL;
    }

        /*  Looking for first non-alias
         */
    RCt = _karChiveResolvePath (
                            ( const struct karChiveEntry ** ) & Found,
                            Root,
                            Path
                            );
    if ( RCt != 0 || Found == NULL ) {
        return NULL;
    }

        /*  I afraid that there could be cycled links,
         *  so small protection
         */
    Alias -> _is_invalid = true;

    if ( Found -> _type == kptAlias ) {
            /*  Recursively falling down
             */
        Found = _karChiveDealiasRecursively (
                                        ( struct karChiveAlias * ) Found
                                        );
    }

    if ( Found != NULL ) {
            /*  So after that fidgeting we suppose that Found is not an
             *  alias and it has some type :LOL:
             */
        Alias -> _da_da_dad . _type = Found -> _type | kptAlias;
        Alias -> _resolved = Found;

        Alias -> _is_invalid = false;
    }

    return Found;
}   /* _karChiveDealiasRecursively () */

static
void
_karChiveCheckResolveAlias ( struct BSTNode *Node, void * Data )
{
        /*  sorry, dropped const ... lol
         */
    struct karChiveEntry * Entry;

    Entry = ( struct karChiveEntry * ) Node;

        /*  That one looks like alias
         */
    if ( Entry -> _type == kptAlias ) {
        _karChiveDealiasRecursively ( ( struct karChiveAlias * ) Entry );
    }

    if ( Entry -> _type == kptDir ) {
        BSTreeForEach (
                & ( ( ( struct karChiveDir * ) Entry ) -> _entries ),
                false,
                _karChiveCheckResolveAlias,
                ( struct karChiveDir * ) Entry
               );
    }
}   /* _karChiveCheckResolveAlias () */

static
rc_t
_karChiveResolveAliases ( const struct karChiveDir * RootDir )
{
    struct karChiveDir * Dir = ( struct karChiveDir * ) RootDir;
    if ( Dir != NULL ) {
        BSTreeForEach (
                        & ( Dir -> _entries ),
                        false,
                        _karChiveCheckResolveAlias,
                        NULL
                    );
    }

    return 0;
}   /* _karChiveResolveAliases () */

static
void
_karChiveEntryForEach (
                    const struct karChiveEntry * self,
                    void ( CC * Callback ) (
                                    const struct karChiveEntry * Entry,
                                    void * Data
                                    ),
                    void * Data
                    );

struct _Ambush_ {
    void ( CC * Callback ) (
                            const struct karChiveEntry * Entry,
                            void * Data
                            );
    void * Data;
};

static
void
_karChiveEntryForEachCallback ( struct BSTNode * Node, void * Data )
{
    struct _Ambush_ * Ambush = ( struct _Ambush_ * ) Data;

    _karChiveEntryForEach (
                        ( const struct karChiveEntry * ) Node,
                        Ambush -> Callback,
                        Ambush -> Data
                        );
}   /* _karChiveEntryForEachCallback () */

void
_karChiveEntryForEach (
                    const struct karChiveEntry * self,
                    void ( CC * Callback ) (
                                    const struct karChiveEntry * Entry,
                                    void * Data
                                    ),
                    void * Data
)
{
    struct _Ambush_ Ambush;
    memset ( & Ambush, 0, sizeof ( struct _Ambush_ ) );

    if ( self == NULL ) {
        return;
    }

    if ( Callback == NULL ) {
        return;
    }

    Callback ( self, Data );

    if ( self -> _type == kptDir ) {
        Ambush . Callback = Callback;
        Ambush . Data = Data;

        BSTreeForEach (
                    & ( ( ( struct karChiveDir * ) self ) -> _entries ),
                    false,
                    _karChiveEntryForEachCallback,
                    & Ambush
                    );
    }
}   /* _karChiveEntryForEach () */

static
void CC
_karChiveSetSourcesCallback (
                            const struct karChiveEntry * Entry,
                            void * Data
)
{
    struct karChive * Chive;
    struct karChiveFile * File;

    Chive = Data;
    File = NULL;

    if ( Data == NULL ) {
            /*  there is nothing to do ... don't know what to do here
             */
        return;
    }

    if ( Entry -> _type == kptFile ) {
        File = ( struct karChiveFile * ) Entry;

            /*  Another situation when I do not know what to do
             *  Should Produce error
             */
        karChiveMMapDSMake (
                                & ( File -> _data_source ),
                                Chive -> _map,
                                _karChiveDataOffset ( Chive )
                                                + File -> _byte_offset,
                                File -> _byte_size
                                );
    }
}   /* _karChiveSetSourcesCallback () */

static
rc_t CC
_karChiveSetSources ( struct karChive * self )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcPath, rcConverting, rcSelf, rcNull );
    }

    _karChiveEntryForEach (
                        ( const struct karChiveEntry * ) self -> _root,
                        _karChiveSetSourcesCallback,
                        self
                        );

    return 0;
}   /* _karChiveSetSources () */

/*  JOJOBA: dangerous method ... debug only
 */
static
rc_t
_karChiveEntryPath (
                    const struct karChiveEntry * self,
                    char * Buffer,
                    size_t BufferSize
)
{
    char B1[4096];
    char B2[4096];
    const struct karChiveEntry * Entry;

    * B1 = 0;
    * B2 = 0;
    Entry = self;

    if ( self == NULL ) {
        return RC ( rcApp, rcPath, rcConverting, rcSelf, rcNull );
    }

    if ( Buffer == NULL ) {
        return RC ( rcApp, rcPath, rcConverting, rcParam, rcNull );
    }

    if ( BufferSize < 1 ) {
        return RC ( rcApp, rcPath, rcConverting, rcParam, rcInvalid );
    }

    while ( Entry != NULL ) {
        if ( Entry -> _name == NULL ) {
            strcpy ( B1, "." );
        }
        else {
            strcpy ( B1, "/" );
            strcat ( B1, Entry -> _name );
        }

        strcat ( B1, B2 );
        strcpy ( B2, B1 );
        Entry = ( struct karChiveEntry * )
                                        _karChiveEntryParent ( Entry );
    }

    if ( BufferSize < strlen ( B2 ) ) {
        return RC ( rcApp, rcBuffer, rcConverting, rcSize, rcTooShort );
    }

    strcpy ( Buffer, B2 );

    return 0;
}   /* _karChiveEntryPath () */

/*  JOJOBA: dangerous method ... debug only
 */
static
rc_t
_karChiveEntryDetails (
                    const struct karChiveEntry * self,
                    char * Buffer,
                    size_t BufferSize
)
{
    char B1[4096];

    * B1 = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcPath, rcConverting, rcSelf, rcNull );
    }

    if ( Buffer == NULL ) {
        return RC ( rcApp, rcPath, rcConverting, rcParam, rcNull );
    }

    if ( BufferSize < 1 ) {
        return RC ( rcApp, rcPath, rcConverting, rcParam, rcInvalid );
    }

    if ( self -> _type == kptFile ) {
        struct karChiveFile * File = ( struct karChiveFile * ) self;
        sprintf ( B1, "[%lu] [%.04o] [F]",
                        File -> _byte_size, self -> _access_mode ); 

    }
    else {
        if ( self -> _type == kptDir ) {
            sprintf ( B1, "[%.04o] [D]", self -> _access_mode ); 

        }
        else {
            if ( ( self -> _type & kptAlias ) == kptAlias ) {
                struct karChiveAlias * Alias = ( struct karChiveAlias * ) self;
                sprintf ( B1, "[%s] [%.04o] [L]",
                                Alias -> _link, self -> _access_mode ); 
            }
            else {
                sprintf ( B1, "[%.04o] [?]", self -> _access_mode ); 
            }
        }
    }

    if ( BufferSize < strlen ( B1 ) ) {
        return RC ( rcApp, rcBuffer, rcConverting, rcSize, rcTooShort );
    }

    strcpy ( Buffer, B1 );

    return 0;
}   /* _karChiveEntryDetails () */

static
void
_karChiveEntryDumpCallback (
                            const struct karChiveEntry * self,
                            void * Data
)
{
    char B1[4096];
    char B2[4096];

    * B1 = 0;
    * B2 = 0;

    if ( * ( ( bool * ) Data ) ) {
        _karChiveEntryPath ( self, B1, sizeof ( B1 ) );
        _karChiveEntryDetails ( self, B2, sizeof ( B2 ) );

        printf ( "%24s %s\n", B2, B1 );
    }
    else {
        _karChiveEntryPath ( self, B1, sizeof ( B1 ) );

        printf ( "%s\n", B1 );
    }

}   /* _karChievEntryDumpCallback () */

static
void
_karChiveEntryDump ( struct karChiveEntry * self, bool MoreDetails )
{
    bool MD = MoreDetails;

    _karChiveEntryForEach ( self, _karChiveEntryDumpCallback, & MD ); 
}   /* _karChiveEntryDump () */

static rc_t _karChivePersist (
                            void * Param,
                            const void * Node,
                            size_t * NumWrit,
                            PTWriteFunc Write,
                            void * WriteParam
                            );

static
rc_t
_karChivePersistEntry (
                    const struct karChiveEntry * Entry,
                    uint8_t EntryType,
                    size_t * NumWrit,
                    PTWriteFunc Write,
                    void * WriteParam
)
{
    rc_t RCt;
    size_t TotalExpected;
    size_t TotalWritten;
    uint16_t NameLen;

    RCt = 0;
    TotalExpected = 0;
    TotalWritten = 0;
    NameLen = 0;

    NameLen = strlen ( Entry -> _name );

    TotalExpected   = sizeof ( NameLen )
                    + NameLen
                    + sizeof ( Entry -> _mod_time )
                    + sizeof ( Entry -> _access_mode )
                    + sizeof ( EntryType )
                    ;

    if ( Write == NULL ) {
        * NumWrit = TotalExpected;
        return 0;
    }

    RCt = Write ( WriteParam, & NameLen, sizeof ( NameLen ), NumWrit );
    if ( RCt == 0 ) {
        TotalWritten += * NumWrit;

        RCt = Write ( WriteParam, Entry -> _name, NameLen, NumWrit );
        if ( RCt == 0 ) {
            TotalWritten += * NumWrit;

            RCt = Write (
                        WriteParam,
                        & ( Entry -> _mod_time ),
                        sizeof ( Entry -> _mod_time ),
                        NumWrit
                        );
            if ( RCt == 0 ) {
                TotalWritten += * NumWrit;

                RCt = Write (
                            WriteParam,
                            & ( Entry -> _access_mode ),
                            sizeof ( Entry -> _access_mode ),
                            NumWrit
                            );
                if ( RCt == 0 ) {
                    TotalWritten += * NumWrit;

                    RCt = Write (
                                WriteParam,
                                & EntryType,
                                sizeof ( EntryType ),
                                NumWrit
                                );
                    if ( RCt == 0 ) {
                        TotalWritten += * NumWrit;

                        if ( RCt == 0 ) {
                            if ( TotalWritten != TotalExpected ) {
                                RCt = RC ( rcApp, rcFile, rcWriting, rcTransfer, rcIncorrect );
                            }
                            else {
                                * NumWrit = TotalWritten;
                            }
                        }
                    }
                }
            }
        }
    }

    return RCt;
}   /* _karChivePersistEntry () */

static
rc_t
_karChivePersistFile (
                    const struct karChiveFile * File,
                    size_t * NumWrit,
                    PTWriteFunc Write,
                    void * WriteParam
)
{
    rc_t RCt;
    size_t TotalExpected;
    size_t TotalWritten;

    RCt = 0;
    TotalExpected = 0;
    TotalWritten = 0;

    RCt = _karChivePersistEntry (
                                & ( File -> _da_da_dad ),
                                ( File -> _new_byte_size == 0
                                        ? karChive_emptyfile
                                        : karChive_file
                                ),
                                NumWrit,
                                Write,
                                WriteParam
                                );
    if ( RCt == 0 ) {
        TotalWritten = * NumWrit;

        if ( File -> _new_byte_size == 0 ) {
            TotalExpected = TotalWritten;
        }
        else {
            TotalExpected   = TotalWritten 
                            + sizeof ( File -> _new_byte_offset ) 
                            + sizeof ( File -> _new_byte_size ) 
                            ;

            if ( Write == NULL ) {
                * NumWrit = TotalExpected;
                return 0;
            }

            RCt = Write (
                        WriteParam,
                        & ( File -> _new_byte_offset ),
                        sizeof ( File -> _new_byte_offset ),
                        NumWrit
                        );
            if ( RCt == 0 ) {
                TotalWritten += * NumWrit;

                RCt = Write (
                            WriteParam,
                            & ( File -> _new_byte_size ),
                            sizeof ( File -> _new_byte_size ),
                            NumWrit
                            );
                if ( RCt == 0 ) {
                    TotalWritten += * NumWrit;

                    if ( TotalWritten != TotalExpected ) {
                        RCt = RC ( rcApp, rcFile, rcWriting, rcTransfer, rcIncorrect );
                    }
                    else {
                        * NumWrit = TotalWritten;
                    }
                }
            }
        }
    }

    return RCt;
}   /* _karChivePersistFile () */

static
rc_t
_karChivePersistDir (
                    const struct karChiveDir * Dir,
                    size_t * NumWrit,
                    PTWriteFunc Write,
                    void * WriteParam
)
{
    rc_t RCt;
    size_t EntryWritten;

    RCt = 0;
    EntryWritten = 0;

    RCt = _karChivePersistEntry (
                            & ( Dir -> _da_da_dad ),
                            karChive_dir,
                            NumWrit,
                            Write,
                            WriteParam
                            );
    if ( RCt == 0 ) {
        EntryWritten = * NumWrit;

        RCt = BSTreePersist (
                            & Dir -> _entries,
                            NumWrit,
                            Write,
                            WriteParam,
                            _karChivePersist,
                            NULL
                            );
        if ( RCt == 0 ) {
            * NumWrit += EntryWritten;
        }
    }

    return RCt;
}   /* _karChivePersistDir () */

static
rc_t
_karChivePersistAlias (
                    const struct karChiveAlias * Alias,
                    size_t * NumWrit,
                    PTWriteFunc Write,
                    void * WriteParam
)
{
    rc_t RCt;
    size_t TotalExpected;
    size_t TotalWritten;
    uint16_t LinkLen;

    RCt = 0;
    TotalExpected = 0;
    TotalWritten = 0;
    LinkLen = 0;

    RCt = _karChivePersistEntry (
                                & ( Alias -> _da_da_dad ),
                                karChive_softlink,
                                NumWrit,
                                Write,
                                WriteParam
                                );

    if ( RCt == 0 ) {
        TotalWritten = * NumWrit;

        LinkLen = strlen ( Alias -> _link );

        TotalExpected   = TotalWritten 
                        + sizeof ( LinkLen )
                        + LinkLen
                        ;

        if ( Write == NULL ) {
            * NumWrit = TotalExpected;

            return 0;
        }

        RCt = Write (
                    WriteParam,
                    & ( LinkLen ),
                    sizeof ( LinkLen ),
                    NumWrit
                    );
        if ( RCt == 0 ) {
            TotalWritten += * NumWrit;

            RCt = Write (
                        WriteParam,
                        Alias -> _link,
                        LinkLen,
                        NumWrit
                        );

            if ( RCt == 0 ) {
                TotalWritten += * NumWrit;

                if ( TotalExpected != TotalWritten ) {
                    RCt = RC ( rcApp, rcFile, rcWriting, rcTransfer, rcIncorrect );
                }
                else {
                    * NumWrit = TotalWritten;
                }
            }
        }
    }

    return RCt;
}   /* _karChivePersistAlias () */

rc_t
_karChivePersist (
                void * Param,
                const void * Node,
                size_t * NumWrit,
                PTWriteFunc Write,
                void * WriteParam
)
{
    rc_t RCt;
    const struct karChiveEntry * Entry;
    uint8_t Type;

    RCt = 0;
    Entry = ( const struct karChiveEntry * ) Node;
    Type = 0;


    Type = Entry -> _type;
    if ( ( Type & kptAlias ) == kptAlias ) {
        Type = kptAlias;
    }

    switch ( Type ) {
        case kptFile :
            RCt = _karChivePersistFile (
                            ( const struct karChiveFile * ) Entry,
                            NumWrit,
                            Write,
                            WriteParam
                            );
            break;
        case kptDir :
            RCt = _karChivePersistDir (
                            ( const struct karChiveDir * ) Entry,
                            NumWrit,
                            Write,
                            WriteParam
                            );
            break;
        case kptAlias :
            RCt = _karChivePersistAlias (
                            ( const struct karChiveAlias * ) Entry,
                            NumWrit,
                            Write,
                            WriteParam
                            );
            break;
        default :
            break;
    }


    return RCt;
}   /* _karChivePersist () */

static void _karChiveTOCReadEntry ( struct PBSTNode *node, void *data );

/*  How TOC entry is stored 
 *  
 *  uint16_t          : name_len
 *  char [ name_len ] : name
 *  uint64_t          : mod_time
 *  uint32_t          : access_mode
 *  uint8_t           : type_code
 *
 *  and after it are going data related to special types
 *
 */

static
rc_t
_karChiveTOCReadMem (
                    struct LesClaypool * Claypool,
                    void * Buf,
                    size_t NumToRead
)
{
    if ( Claypool -> _data_len < Claypool -> _position + NumToRead ) {
        return RC ( rcApp, rcArc, rcReading, rcOffset, rcInvalid );
    }

    memmove ( 
            Buf,
            ( ( char * ) Claypool -> _data ) + Claypool -> _position,
            NumToRead
            );

    Claypool -> _position += NumToRead;

    return 0;
}   /* _karChiveTOCReadMem () */

static
rc_t
_karChiveTOCReadUint8 ( struct LesClaypool * Claypool, uint8_t * Value )
{
    rc_t RCt;
    uint8_t Buf;

    RCt = 0;
    Buf = 0;

    * Value = 0;

    RCt = _karChiveTOCReadMem ( Claypool, & Buf, sizeof ( Buf ) );
    if ( RCt == 0 ) {
        * Value = Buf;
    }

    return RCt;
}   /* _karChiveTOCReadUint8 () */

static
rc_t
_karChiveTOCReadUint16 (
                        struct LesClaypool * Claypool,
                        bool BytesSwapped,
                        uint16_t * Value
)
{
    rc_t RCt;
    uint16_t Buf;

    RCt = 0;
    Buf = 0;

    * Value = 0;

    RCt = _karChiveTOCReadMem ( Claypool, & Buf, sizeof ( Buf ) );
    if ( RCt == 0 ) {
        if ( BytesSwapped ) {
            Buf = bswap_16 ( Buf );
        }

        * Value = Buf;
    }

    return RCt;
}   /* _karChiveTOCReadUint16 () */

static
rc_t
_karChiveTOCReadUint32 (
                        struct LesClaypool * Claypool,
                        bool BytesSwapped,
                        uint32_t * Value
)
{
    rc_t RCt;
    uint32_t Buf;

    RCt = 0;
    Buf = 0;

    * Value = 0;

    RCt = _karChiveTOCReadMem ( Claypool, & Buf, sizeof ( Buf ) );
    if ( RCt == 0 ) {
        if ( BytesSwapped ) {
            Buf = bswap_32 ( Buf );
        }

        * Value = Buf;
    }

    return RCt;
}   /* _karChiveTOCReadUint32 () */

static
rc_t
_karChiveTOCReadUint64 (
                        struct LesClaypool * Claypool,
                        bool BytesSwapped,
                        uint64_t * Value
)
{
    rc_t RCt;
    uint64_t Buf;

    RCt = 0;
    Buf = 0;

    * Value = 0;

    RCt = _karChiveTOCReadMem ( Claypool, & Buf, sizeof ( Buf ) );
    if ( RCt == 0 ) {
        if ( BytesSwapped ) {
            Buf = bswap_64 ( Buf );
        }

        * Value = Buf;
    }

    return RCt;
}   /* _karChiveTOCReadUint64 () */

static
rc_t
_karChiveTOCReadString (
                        struct LesClaypool * Claypool,
                        bool BytesSwapped,
                        char ** Ret
)
{
    rc_t RCt;
    uint16_t StrLen;
    char * Str;

    RCt = 0;
    StrLen = 0;
    Str = NULL;

    * Ret = NULL;

    RCt = _karChiveTOCReadUint16 ( Claypool, BytesSwapped, & StrLen );
    if ( RCt == 0 ) {
        Str = calloc ( 1, sizeof ( char ) * ( StrLen + 1 ) ); 
        if ( Str == NULL ) {
            return RC ( rcApp, rcArc, rcAllocating, rcItem, rcExhausted );
        }
        else {
            RCt = _karChiveTOCReadMem ( Claypool, Str, StrLen );
            if ( RCt == 0 ) {
                * Ret = Str;
            }
        }
    }

    if ( RCt != 0 ) {
        * Ret = NULL;

        if ( Str != NULL ) {
            free ( Str );
            Str = NULL;
        }
    }

    return RCt;
}   /* _karChiveTOCReadString () */

static
rc_t
_karChiveTOCReadCommonFields (
                            struct LesClaypool * Claypool,
                            bool BytesSwapped
)
{
    rc_t RCt = 0;

        /*  Reading name
         */
    RCt = _karChiveTOCReadString (
                                Claypool,
                                BytesSwapped,
                                & ( Claypool -> _name )
                                );
    if ( RCt == 0 ) {
            /*  Reading mod time
             */
        RCt = _karChiveTOCReadUint64 (
                                        Claypool,
                                        BytesSwapped,
                                        & ( Claypool -> _mod_time )
                                        );
        if ( RCt == 0 ) {
                /*  Reading access mode
                 */
            RCt = _karChiveTOCReadUint32 (
                                        Claypool,
                                        BytesSwapped,
                                        & ( Claypool -> _access_mode )
                                        );
            if ( RCt == 0 ) {
                    /*  Reading entry type
                     */
                RCt = _karChiveTOCReadUint8 (
                                        Claypool,
                                        & ( Claypool -> _type_code )
                                        );
            }
        }
    }

    if ( RCt != 0 ) {
        if ( Claypool -> _name != NULL ) {
            free ( Claypool -> _name );
            Claypool -> _name = NULL;
        }
    }

    return RCt;
}   /* _karChiveTOCReadCommonFields () */

static
rc_t
_karChiveAllocAndInitEntryWithValues (
                                    struct karChiveEntry ** Entry,
                                    size_t EntrySize,
                                    struct karChiveDir * Parent,
                                    const char * Name,
                                    KTime_t ModTime,
                                    uint32_t AccessMode,
                                    uint8_t Type
)
{
    rc_t RCt;
    struct karChiveEntry * Ret;

    RCt = 0;
    Ret = NULL;

    Ret = calloc ( 1, EntrySize );
    if ( Ret == NULL ) {
        if ( Name != NULL ) {
            free ( ( char * ) Name );
        }

        RCt = RC ( rcApp, rcArc, rcAllocating, rcItem, rcExhausted );
    }
    else {
        KRefcountInit ( 
                        & ( Ret -> _refcount ),
                        1,
                        _skarChiveEntry_classname,
                        "_karChiveEntryAllocAndInit",
                        "AllocAndInit"
                        );

        Ret -> _parent = Parent;
        Ret -> _name = Name;
        Ret -> _mod_time = ModTime;
        Ret -> _access_mode = AccessMode;
        Ret -> _type = Type;

        * Entry = Ret;
    }

    return RCt;
}   /* _karChiveAllocAndInitEntryWithValues () */

static
rc_t
_karChiveAllocAndInitEntry (
                            struct karChiveEntry ** Entry,
                            struct karChiveDir * Parent,
                            size_t EntrySize,
                            struct LesClaypool * Claypool,
                            uint8_t Type
)
{
    rc_t RCt;

    RCt = 0;

    RCt = _karChiveAllocAndInitEntryWithValues (
                                            Entry,
                                            EntrySize,
                                            Parent,
                                            Claypool -> _name,
                                            Claypool -> _mod_time,
                                            Claypool -> _access_mode,
                                            Type
                                            );

    Claypool -> _name = NULL;

    return RCt;
}   /* _karChiveAllocAndInitEntry () */

rc_t
_karChiveTOCReadFile ( 
                    struct LesClaypool * Claypool,
                    struct FrogBrigade * Brigade,
                    bool EmptyFile
)
{
    rc_t RCt;
    struct karChiveFile * Entry;
    uint64_t ByteOff, ByteSz;

    RCt = 0;
    Entry = NULL;
    ByteOff = ByteSz = 0;

    if ( EmptyFile ) {
        ByteOff = 0;
        ByteSz = 0;
    }
    else {
        RCt = _karChiveTOCReadUint64 (
                                    Claypool,
                                    Brigade -> _bytes_swapped,
                                    & ByteOff
                                    );
        if ( RCt == 0 ) {
            RCt = _karChiveTOCReadUint64 (
                                        Claypool,
                                        Brigade -> _bytes_swapped,
                                        & ByteSz
                                        );
        }
    }

    if ( RCt == 0 ) {
        RCt = _karChiveAllocAndInitEntry (
                                ( struct karChiveEntry ** ) & Entry,
                                Brigade -> _parent,
                                sizeof ( struct karChiveFile ),
                                Claypool,
                                kptFile
                                );
        if ( RCt == 0 ) { 
            Entry -> _byte_offset = ByteOff;
            Entry -> _byte_size = ByteSz;

            Entry -> _new_byte_offset = 0;
            Entry -> _new_byte_size = 0;

                /* JOJOBA ... max offset / max size */

            RCt = BSTreeInsert (
                                & ( Brigade -> _parent -> _entries ),
                                & ( Entry -> _da_da_dad . _da_da_dad ),
                                _compareEntries
                                );
            if ( RCt != 0 ) {
                pLogErr ( klogErr, RCt, "Failed to insert 'file' to BSTree", "" );
            }
        }
    }

    return RCt;
}   /* _karChiveTOCReadFile () */

static
rc_t
_karChiveTOCReadDir (
                    struct LesClaypool * Claypool,
                    struct FrogBrigade * Brigade
)
{
    rc_t RCt;
    struct karChiveDir * Entry;
    struct PBSTree * PbsTree;
    struct FrogBrigade FRBR;

    RCt = 0;
    Entry = NULL;
    PbsTree = NULL;
    memset ( & FRBR, 0, sizeof ( struct FrogBrigade ) );

    RCt = _karChiveAllocAndInitEntry (
                                ( struct karChiveEntry ** ) & Entry,
                                Brigade -> _parent,
                                sizeof ( struct karChiveDir ),
                                Claypool,
                                kptDir
                                );
    if ( RCt == 0 ) {
        BSTreeInit ( & ( Entry -> _entries ) );

        RCt = PBSTreeMake (
                        & PbsTree,
                        ( const void * ) ( ( ( const char * ) Claypool -> _data ) + Claypool -> _position ),
                        Claypool -> _data_len - Claypool -> _position,
                        Brigade -> _bytes_swapped
                        );
        if ( RCt == 0 ) {
            FRBR . _parent = Entry;
            FRBR . _bytes_swapped = Brigade -> _bytes_swapped;
            PBSTreeForEach (
                            PbsTree,
                            false,
                            _karChiveTOCReadEntry,
                            ( void * ) & ( FRBR )
                            );

            RCt = FRBR . _rc;

            PBSTreeWhack ( PbsTree );
        }

        if ( RCt == 0 ) {
            RCt = BSTreeInsert (
                                & ( Brigade -> _parent -> _entries ),
                                & ( Entry -> _da_da_dad . _da_da_dad ),
                                _compareEntries
                                );
            if ( RCt != 0 ) {
                pLogErr ( klogErr, RCt, "Failed to insert 'dir' to BSTree", "" );
            }
        }
        else {
            pLogErr ( klogErr, RCt, "Failed to scan 'dir' [$(name)] to BSTree", "name=%s", Claypool -> _name );
        }
    }

    return RCt;
}   /* _karChiveTOCReadDir () */

rc_t
_karChiveTOCReadSoftlink ( 
                    struct LesClaypool * Claypool,
                    struct FrogBrigade * Brigade
)
{
    rc_t RCt;
    struct karChiveAlias * Entry;
    char * EntryAlias;

    RCt = 0;
    Entry = NULL;
    EntryAlias = NULL;

    if ( RCt == 0 ) {
        RCt = _karChiveAllocAndInitEntry (
                                ( struct karChiveEntry ** ) & Entry,
                                Brigade -> _parent,
                                sizeof ( struct karChiveAlias ),
                                Claypool,
                                kptAlias
                                );
        if ( RCt == 0 ) { 

            RCt = _karChiveTOCReadString (
                                        Claypool,
                                        Brigade -> _bytes_swapped,
                                        & EntryAlias
                                        );
            if ( RCt == 0 ) {
                Entry -> _link = EntryAlias;

                Entry -> _is_invalid = false;

                RCt = BSTreeInsert (
                                    & ( Brigade -> _parent -> _entries ),
                                    & ( Entry -> _da_da_dad . _da_da_dad ),
                                    _compareEntries
                                    );
                if ( RCt != 0 ) {
                    pLogErr ( klogErr, RCt, "Failed to insert 'alias' to BSTree", "" );
                }
            }
        }
    }

    return RCt;
}   /* _karChiveTOCReadSoftlink () */

void
_karChiveTOCReadEntry ( struct PBSTNode * Node, void * Data )
{
    struct LesClaypool Claypool;
    struct FrogBrigade * Brigade;

    memset ( & Claypool, 0, sizeof ( struct LesClaypool ) );
    Brigade = ( struct FrogBrigade * ) Data;

        /*  We are not doing any check for NULL here :LOL:
         */

        /* Something bad happened */
    if ( Brigade -> _rc != 0 ) {
        return;
    }

    Claypool . _data = Node -> data . addr;
    Claypool . _data_len = Node -> data . size;

        /* Reading common fields */
    Brigade -> _rc = _karChiveTOCReadCommonFields (
                                            & Claypool,
                                            Brigade -> _bytes_swapped
                                            );
    if ( Brigade -> _rc == 0 ) {
            /* Here we do entries */
        switch ( Claypool . _type_code ) {
            case karChive_file:
                Brigade -> _rc = _karChiveTOCReadFile (
                                                    & Claypool,
                                                    Brigade,
                                                    false
                                                    );
                break;

            case karChive_emptyfile:
                Brigade -> _rc = _karChiveTOCReadFile (
                                                    & Claypool,
                                                    Brigade,
                                                    true
                                                    );
                break;

            case karChive_dir:
                Brigade -> _rc = _karChiveTOCReadDir (
                                                    & Claypool,
                                                    Brigade
                                                    );
                break;

            case karChive_softlink:
                Brigade -> _rc = _karChiveTOCReadSoftlink (
                                                    & Claypool,
                                                    Brigade
                                                    );
                break;

            default:
                Brigade -> _rc = RC ( rcApp, rcArc, rcLoading, rcItem, rcInvalid );
                break;
        }
    }

}   /* _karChiveTOCReadEntry () */

static
rc_t
_karChiveTOCMake ( struct karChive * Chive )
{
    rc_t RCt;
    struct karChiveDir * Root;
    uint64_t TocOffset, TocSize;
    struct PBSTree * PbsTree;
    struct FrogBrigade Brigade;

    RCt = 0;
    Root = NULL;
    TocOffset = TocSize = 0;
    PbsTree = NULL;
    memset ( & Brigade, 0, sizeof ( Brigade ) );

    if ( Chive == NULL ) {
        return RC ( rcApp, rcToc, rcConstructing, rcParam, rcNull );
    }

    if ( Chive -> _root != NULL ) {
        return RC ( rcApp, rcToc, rcConstructing, rcParam, rcInvalid );
    }

    TocOffset = _karChiveTocOffset ( Chive );
    TocSize = _karChiveTocSize ( Chive );

    if ( TocOffset == 0 || TocSize == 0 ) {
        return RC ( rcApp, rcToc, rcConstructing, rcParam, rcInconsistent );
    }

    RCt = _karChiveAllocAndInitEntryWithValues (
                                    ( struct karChiveEntry ** ) & Root,
                                    sizeof ( struct karChiveDir ),
                                    NULL,
                                    NULL,
                                    0,
                                    0,
                                    kptDir
                                    );
    if ( RCt == 0 ) {
        BSTreeInit ( & ( Root -> _entries ) );

        RCt = PBSTreeMake (
                        & PbsTree,
                        ( const void * ) ( ( ( const char * ) Chive -> _map_addr ) + TocOffset ),
                        TocSize,
                        _karChiveBytesSwapped ( Chive )
                        );
        if ( RCt == 0 ) {
            Brigade . _parent = Root;
            PBSTreeForEach (
                            PbsTree,
                            false,
                            _karChiveTOCReadEntry,
                            ( void * ) & ( Brigade )
                            );

            if ( Brigade . _rc == 0 ) {
                    /* Resolving aliases. */
                Brigade . _rc = _karChiveResolveAliases ( Root );

                if ( Brigade . _rc == 0 ) { 
                    Chive -> _root = Root;

                    RCt = _karChiveSetSources ( Chive );
                }
            }

            PBSTreeWhack ( PbsTree );
        }
    }

    return RCt;
}   /* _karChiveTOCMake () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Struct karChive
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

const void *
_karChiveMapAddr ( const struct karChive * self )
{
    return self == NULL ? 0 : ( self -> _map_addr );
}   /* _karChiveMapAddr () */

bool
_karChiveBytesSwapped ( const struct karChive * self )
{
    return self == NULL ? false : ( self -> _hdr_bytes_swapped );
}   /* _karChiveBytesSwapped () */

uint32_t
_karChiveVersion ( const struct karChive * self )
{
    return ( self != NULL && self -> _hdr != NULL )
                    ?   ( _karChiveBytesSwapped ( self )
                            ? bswap_32 ( self -> _hdr -> version )
                            : self -> _hdr -> version
                        )
                    :   0
                    ;
}   /* _karChiveVersion () */

uint64_t
_karChiveDataOffset ( const struct karChive * self )
{
    return ( self != NULL && self -> _hdr != NULL )
                    ?   ( _karChiveBytesSwapped ( self )
                            ? bswap_64 ( self -> _hdr -> u . v1 . file_offset )
                            : self -> _hdr -> u . v1 . file_offset
                        )
                    :   0
                    ;
}   /* _karChiveDataOffset () */

uint64_t
_karChiveHdrSize ( const struct karChive * self )
{
    return sizeof ( struct KSraHeader );
}   /* _karChiveHdrSize () */

uint64_t
_karChiveTocSize ( const struct karChive * self )
{
    return _karChiveDataOffset ( self ) - _karChiveHdrSize ( self );
}   /* _karChiveTocSize () */

uint64_t
_karChiveTocOffset ( const struct karChive * self )
{
    return _karChiveHdrSize ( self );
}   /* _karChiveTocOffset () */

static
rc_t
_karChiveDispose ( const struct karChive * self )
{
    struct karChive * Chive = ( struct karChive * ) self;

    if ( Chive != NULL ) {
        KRefcountWhack (
                        & ( Chive -> _refcount ),
                        _skarChive_classname
                        );

            /* TOC */
        if ( Chive -> _root != NULL ) {
            _karChiveTOCDispose ( Chive );
        }

            /* HDR */
        Chive -> _hdr = NULL;
        Chive -> _hdr_bytes_swapped = false;

            /* MUNMAP */
        Chive -> _map_addr = NULL;
        Chive -> _map_size = 0;
        if ( Chive -> _map != NULL ) {
            KMMapRelease ( Chive -> _map );
            Chive -> _map = NULL;
        }

// JOJOBA - continue

        free ( Chive );
    }

    return 0;
}   /* _karChiveDispose () */

static
rc_t
_karChiveReadMap (
                const struct karChive * self,
                const struct KFile * File
)
{
    rc_t RCt;
    struct karChive * Chive;

    RCt = 0;
    Chive = ( struct karChive * ) self;

    if ( Chive == NULL ) {
        return RC ( rcApp, rcArc, rcLoading, rcParam, rcNull );
    }

    RCt = KMMapMakeRead ( & ( Chive -> _map ), File );
    if ( RCt == 0 ) {
        RCt = KMMapSize ( Chive -> _map, & ( Chive -> _map_size ) );
        if ( RCt == 0 ) {
            if ( Chive -> _map_size == 0 ) {
                RCt = RC ( rcApp, rcArc, rcLoading, rcFile, rcEmpty );
            }
            else {
                RCt = KMMapAddrRead (
                                    Chive -> _map,
                                    & ( Chive -> _map_addr )
                                    );
                if ( RCt == 0 ) {
                    if ( Chive -> _map_addr == NULL ) {
                        RCt = RC ( rcApp, rcArc, rcLoading, rcFile, rcInvalid );
                    }
                }
            }
        }
    }

    return RCt;
}   /* _karChiveReadMap () */

static
rc_t
_karChiveReadVerifyHeader ( const struct karChive * self )
{
    rc_t RCt;
    struct karChive * Chive;
    uint64_t WonderSize;

    RCt = 0;
    Chive = ( struct karChive * ) self;
    WonderSize = 0;

    if ( Chive == NULL ) {
        return RC ( rcApp, rcArc, rcLoading, rcParam, rcNull );
    }

    if ( Chive -> _map == NULL ) {
        return RC ( rcApp, rcArc, rcLoading, rcParam, rcInvalid );
    }

        /*  Initializem that here, cuz it is important to remember
         *  all _karChive...() uses Chive
         */
    Chive -> _hdr = ( struct KSraHeader * ) Chive -> _map_addr;

    WonderSize = _karChiveHdrSize ( Chive ) - sizeof ( Chive -> _hdr -> u );
    if ( Chive -> _map_size < WonderSize ) {
        RCt = RC ( rcApp, rcArc, rcLoading, rcFile, rcCorrupt );
        pLogErr ( klogErr, RCt, "Corrupted archive file - invalid header", "" );
        return RCt;
    }

        /*  verify "ncbi" and "sra"
         */
    if (    memcmp (
                    Chive -> _hdr -> ncbi,
                    "NCBI",
                    sizeof ( Chive -> _hdr -> ncbi )
                    ) != 0 ||
            memcmp (
                    Chive -> _hdr -> sra,
                    ".sra",
                    sizeof ( Chive -> _hdr -> sra )
                    ) != 0
    ) {
        RCt = RC ( rcApp, rcArc, rcLoading, rcFormat, rcInvalid );
        pLogErr ( klogErr, RCt, "Invalid file format", "" );
        return RCt;
    }

        /*  test "byte_order".
         *  this is allowed to be either eSraByteOrderTag or
         *  eSraByteOrderReverse.  anything else, is garbage
         *    (C) Kurt Jr.
         */
    switch ( Chive -> _hdr -> byte_order ) {
        case eSraByteOrderTag:
            Chive -> _hdr_bytes_swapped = false;
            break;
        case eSraByteOrderReverse:
            Chive -> _hdr_bytes_swapped = true;
            break;
        default:
            RCt = RC ( rcApp, rcArc, rcValidating, rcByteOrder, rcInvalid );
            pLogErr ( klogErr, RCt, "Failed to access archive file - invalid byte order", "" );
            return RCt;
    }

        /*  Test version */
    if ( _karChiveVersion ( Chive ) == 0 ) {
        RCt = RC ( rcApp, rcArc, rcValidating, rcInterface, rcInvalid );
        pLogErr ( klogErr, RCt, "Invalid version", "" );
        return RCt;
    }

    if ( _karChiveVersion ( Chive ) > 1 ) {
        RCt = RC ( rcApp, rcArc, rcValidating, rcInterface, rcUnsupported );
        pLogErr ( klogErr, RCt, "Version not supported", "" );
        return RCt;
    }

        /*  Test actual size against specific header version */
    WonderSize += sizeof ( Chive -> _hdr -> u . v1 );
    if ( Chive -> _map_size < WonderSize ) {
        RCt = RC ( rcApp, rcArc, rcValidating, rcOffset, rcIncorrect );
        pLogErr ( klogErr, RCt, "Failed to read header", "" );
        return RCt;
    }

    return RCt;
}   /* _karChiveReadVerifyHeader () */

static
rc_t
_karChiveMake ( 
            const struct karChive ** Chive,
            const struct KFile * File
)
{
    /*  Do not check arguments here, cuz they should be checked already
     */
    rc_t RCt;
    struct karChive * RetChive;

    RCt = 0;
    RetChive = NULL;

    RetChive = calloc ( 1, sizeof ( struct karChive ) );
    if ( RetChive == NULL ) {
        RCt = RC ( rcApp, rcArc, rcAllocating, rcParam, rcExhausted );
        pLogErr ( klogErr, RCt, "Can not allocate memory for Archive", "" );
        return RCt;
    }
    else {
        KRefcountInit ( 
                        & ( RetChive -> _refcount ),
                        1,
                        _skarChive_classname,
                        "_karChiveMake",
                        "Make"
                        );
            /*  First we are mapping file into memory
             */
        RCt = _karChiveReadMap ( RetChive, File );
        if ( RCt == 0 ) {
                /*  Second we are reading header and checking sizes
                 */
            RCt = _karChiveReadVerifyHeader ( RetChive );
            if ( RCt == 0 ) {
                    /*  Third we are loading TOC
                     */
                RCt = _karChiveTOCMake ( RetChive );
                if ( RCt == 0 ) {
                        /*  Here we are ready to go
                         */
                    * Chive = RetChive;
                }
            }
        }
    }

    if ( RCt != 0 ) {
        * Chive = NULL;

        if ( RetChive != NULL ) {
            _karChiveDispose ( RetChive );
        }
    }

    return RCt;
}   /* _karChiveMake () */

LIB_EXPORT
rc_t
karChiveMake (
            const struct karChive ** Chive,
            const struct KFile * File
)
{
    if ( Chive != NULL ) {
        * Chive = NULL;
    }
    else {
        return RC ( rcApp, rcArc, rcLoading, rcParam, rcNull );
    }

    if ( File == NULL ) {
        return RC ( rcApp, rcArc, rcLoading, rcParam, rcNull );
    }

    return _karChiveMake ( Chive, File );
}   /* karChiveMake () */

LIB_EXPORT
rc_t
karChiveOpen ( const struct karChive ** Chive, const char * Path )
{
    rc_t RCt;
    struct KDirectory * NatDir;
    const struct KFile * File;

    RCt = 0;
    NatDir = NULL;
    File = NULL;

    RCt = KDirectoryNativeDir ( & NatDir );
    if ( RCt == 0 ) {
        RCt = KDirectoryOpenFileRead ( NatDir, & File, "%s", Path );
        if ( RCt == 0 ) {
            RCt = _karChiveMake ( Chive, File );

            KFileRelease ( File );
        }
        else {
            pLogErr ( klogErr, RCt, "Can not open file for read [$(name)]", "name=%s", Path );
        }

        KDirectoryRelease ( NatDir );
    }

    return RCt;
}   /* karChiveOpen () */

LIB_EXPORT
rc_t
karChiveAddRef ( const struct karChive * self )
{
    rc_t RCt;
    struct karChive * Chive;

    RCt = 0;
    Chive = ( struct karChive * ) self;

    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcAccessing, rcSelf, rcNull );
    }

    switch (
        KRefcountAdd ( & ( Chive -> _refcount ), _skarChive_classname )
    ) {
        case krefOkay :
            RCt = 0;
            break;
        case krefZero :
        case krefLimit :
        case krefNegative :
            RCt = RC ( rcApp, rcArc, rcAccessing, rcParam, rcNull );
            break;
        default :
            RCt = RC ( rcApp, rcArc, rcAccessing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* karChiveAddRef () */

LIB_EXPORT
rc_t
karChiveRelease ( const struct karChive * self )
{
    rc_t RCt;
    struct karChive * Chive;

    RCt = 0;
    Chive = ( struct karChive * ) self;

    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcReleasing, rcSelf, rcNull );
    }

    switch (
        KRefcountDrop ( & ( Chive -> _refcount ), _skarChive_classname )
    ) {
        case krefOkay :
        case krefZero :
            RCt = 0;
            break;
        case krefWhack :
            RCt = _karChiveDispose ( Chive );
            break;
        case krefNegative :
            RCt = RC ( rcApp, rcArc, rcReleasing, rcParam, rcInvalid );
            break;
        default :
            RCt = RC ( rcApp, rcArc, rcReleasing, rcParam, rcUnknown );
            break;
    }
    return RCt;
}   /* karChiveRelease () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  There we do edit karChive
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static rc_t CC karChiveEditMeta ( const struct karChiveFile * Meta );

LIB_EXPORT rc_t karChiveEdit ( const struct karChive * self )
{
    rc_t RCt;
    const struct karChiveEntry * Found;
    const struct karChiveDir * Parent;

    RCt = 0;
    Found = NULL;
    Parent = NULL;

    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcProcessing, rcSelf, rcNull );
    }

        /* First we do remove 'col/QUALITY' */
    RCt = _karChiveResolvePath (
                                & Found,
                                self -> _root,
                                "col/QUALITY"
                                );
    if ( RCt == 0 ) {
        Parent = _karChiveEntryParent ( Found );
        if ( Parent == NULL ) {
                /*  The node is root itself and could not be deleted 
                 */
            RCt = RC ( rcApp, rcNode, rcRemoving, rcItem, rcUnsupported );
        }
        else {
            if ( BSTreeUnlink ( ( struct BSTree * ) & ( Parent -> _entries ), ( struct BSTNode * ) & ( Found -> _da_da_dad ) ) ) {
                _karChiveEntryRelease ( Found );
            }
            else {
                RCt = RC ( rcApp, rcNode, rcRemoving, rcItem, rcFailed );
            }
        }
    }
    else {
        printf ( "Can not find that\n" );
        RCt = 0;
    }

        /*  Modifying md/cur
         */
    RCt = _karChiveResolvePath (
                                & Found,
                                self -> _root,
                                "md/cur"
                                );
    if ( RCt == 0 ) {
        struct karChiveFile * File = ( struct karChiveFile * ) Found;
        const char * Sig = "I was here";

        RCt = karChiveEditMeta ( File );
        if ( RCt == 0 ) {

            if ( File -> _data_source != NULL ) {
                karChiveDSRelease ( File -> _data_source );
                File -> _data_source = NULL;
            }

            RCt = karChiveMemDSMake ( 
                                & File -> _data_source,
                                Sig,
                                strlen ( Sig )
                                );
        }
    }
    else {
        printf ( "Can not find that\n" );
        RCt = 0;
    }

/* That's it for now */

    return RCt;
}   /* karChiveEdit () */

rc_t CC
karChiveEditMeta ( const struct karChiveFile * Meta )
{
    rc_t RCt;
    size_t Size;
    char BBB [ 1024 * 64 ]; /* <<< JOJOBA!!!!! */
    size_t NumR;
    struct karChiveMD * MD;
    const struct karChiveMDN * MDN;
    const void * Packed;
    size_t PSize;

    RCt = 0;
    Size = 0;
    * BBB = 0;
    NumR = 0;
    MD = NULL;
    MDN = NULL;
    Packed = NULL;
    PSize = 0;

    if ( Meta == NULL ) {
        return RC ( rcApp, rcArc, rcProcessing, rcParam, rcNull );
    }

    if ( Meta -> _data_source == NULL ) {
        return RC ( rcApp, rcArc, rcProcessing, rcParam, rcInvalid );
    }

    RCt = karChiveDSSize ( Meta -> _data_source, & Size );
    if ( RCt == 0 ) {
        if ( sizeof ( BBB ) < Size ) {
            RCt = RC ( rcApp, rcArc, rcProcessing, rcParam, rcCorrupt );
        }
        else {
            RCt = karChiveDSRead (
                                Meta -> _data_source,
                                0,
                                BBB,
                                Size,
                                & NumR
                                );
            if ( RCt == 0 ) {
                if ( NumR != Size ) { 
                    RCt = RC ( rcApp, rcArc, rcReading, rcSize, rcInvalid );
                }
                else {
                    RCt = karChiveMDUnpack ( & MD, BBB, Size );
                    if ( RCt == 0 ) {
                        RCt = karChiveMDFind ( MD, "schema", & MDN );
                        if ( RCt == 0 ) {
                            char * PPP = NULL;

                            RCt = karChiveMDNAsSting ( MDN, & PPP );
                            if ( RCt == 0 ) {
                                free ( PPP );
                            }
                        }
                        else {
printf ( "[NOT] [%p]\n", 0 );
                        }

/*  Here we should set new node and save it
 */
                        RCt = karChiveMDPack ( MD, & Packed, & PSize );
                        if ( RCt == 0 ) {
                            free ( ( void * ) Packed );
                        }

                        karChiveMDRelease ( MD );
                    }
                }
            }
        }
    }

    return RCt;
}

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  There we do write KAR file
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveWriter {
    const struct karChive * _chive;

    struct karChiveFile ** _files;
    uint32_t _files_qty;
    uint32_t _files_cap;


    size_t _toc_size;

    uint64_t _start_pos;
    uint64_t _curr_pos;

    struct KFile * _output;

    char * _buffer;
    size_t * _buffer_size;

    rc_t _rc;
};

static rc_t _karChiveWriterClearFiles ( struct karChiveWriter * self );

static
rc_t
_karChiveWriterWhack ( struct karChiveWriter * self )
{
    if ( self != NULL ) {
        if ( self -> _chive != NULL ) {
            karChiveRelease ( self -> _chive );

            self -> _chive = NULL;
        }

        if ( self -> _files != NULL ) {
            _karChiveWriterClearFiles ( self );

            free ( self -> _files );
        }
        self -> _files = NULL;
        self -> _files_qty = 0;
        self -> _files_cap = 0;

        self -> _toc_size = 0;
        self -> _start_pos = 0;
        self -> _curr_pos = 0;

        if ( self -> _output != NULL ) {
            KFileRelease ( self -> _output );

            self -> _output = NULL;
        }

        if ( self -> _buffer != NULL ) {
            free ( self -> _buffer );
        }
        self -> _buffer = NULL;
        self -> _buffer_size = 0;
    }

    return 0;
}   /* _karChiveWriterWhack () */

static
rc_t
_karChiveWriterInit (
                    struct karChiveWriter * self,
                    const struct karChive * Chive,
                    struct KFile * Output

)
{
    rc_t RCt;

    RCt = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcNoTarg, rcInitializing, rcSelf, rcNull );
    }

    if ( Chive == NULL ) {
        return RC ( rcApp, rcNoTarg, rcInitializing, rcParam, rcNull );
    }

    if ( Output == NULL ) {
        return RC ( rcApp, rcNoTarg, rcInitializing, rcParam, rcNull );
    }

    memset ( self, 0, sizeof ( struct karChiveWriter ) );

    RCt = karChiveAddRef ( Chive );
    if ( RCt == 0 ) {
        RCt = KFileAddRef ( Output );
        if ( RCt == 0 ) {
            self -> _chive = Chive;
            self -> _output = Output;
        }
        else {
            karChiveRelease ( Chive );
        }
    }


    return 0;
}   /* _karChiveWriterInit () */

static
rc_t
_karChiveWriterFunction (
                        void * Param,
                        const void * Buffer,
                        size_t Bytes,
                        size_t * NumWrit
)
{
    rc_t RCt;
    struct karChiveWriter * Writer;

    RCt = 0;
    Writer = ( struct karChiveWriter * ) Param;

    RCt = KFileWriteAll (
                        Writer -> _output,
                        Writer -> _curr_pos,
                        Buffer,
                        Bytes,
                        NumWrit
                        );
    Writer -> _curr_pos += * NumWrit;
    if ( RCt != 0 ) {
        Writer -> _rc = RCt;
    }

    return RCt;
}   /* _karChiveWriterFunction () */

rc_t
_karChiveWriterClearFiles ( struct karChiveWriter * self )
{
    size_t llp = 0;

    if ( self -> _files_qty != 0 ) {
        if ( self -> _files != NULL ) {
            for ( llp = 0; llp < self -> _files_qty; llp ++ ) {
                if ( self -> _files [ llp ] != NULL ) {
                    _karChiveEntryRelease (
                            & ( self -> _files [ llp ] -> _da_da_dad )
                            );
                    self -> _files [ llp ] = NULL;
                }
            }

            memset (
                self -> _files,
                0,
                sizeof ( struct karChiveFile * ) * self -> _files_qty
                );
        }
    }

    self -> _files_qty = 0;

    return 0;
}   /* _karChiveWriterClearFiles () */

static
rc_t
_karChiveWriterAddFile (
                        struct karChiveWriter * self,
                        struct karChiveFile * File
)
{
    rc_t RCt;
    uint32_t NewCap;
    struct karChiveFile ** NewFiles;

    RCt = 0;
    NewCap = 0;
    NewFiles = NULL;

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcWriting, rcSelf, rcNull );
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcWriting, rcParam, rcNull );
    }

    RCt = _karChiveEntryAddRef ( & File -> _da_da_dad );
    if ( RCt == 0 ) {

        NewCap = ( ( ( self -> _files_qty + 1 ) / 64 ) + 1 ) * 64;
        if ( self -> _files_cap <= NewCap ) {
            NewFiles = calloc ( NewCap, sizeof ( struct karChiveFile * ) );
            if ( NewFiles == NULL ) {
                RCt = RC ( rcApp, rcNode, rcWriting, rcMemory, rcExhausted );
            }
            else {
                if ( self -> _files_qty != 0 ) {
                    memmove (
                            NewFiles,
                            self -> _files,
                            sizeof ( struct karChiveFile * ) * self -> _files_qty
                            );

                    free ( self -> _files );
                }

                self -> _files_cap = NewCap;
                self -> _files = NewFiles;
            }
        }
    }

    self -> _files [ self -> _files_qty ] = File;
    self -> _files_qty ++;

    return RCt;
}   /* _karChiveWriterAddFile () */

static
int64_t
_karChiveWriterSortFilesCallback (
                                const void * Left,
                                const void * Right,
                                void * Data
)
{
    struct karChiveFile * Lf, * Rf;
    size_t Ls, Rs;

    Ls = Rs = 0;

        /*  JOJOBA: we do not check if left or right for NULL.
         *      Not our problem
         */
    Lf = * ( ( struct karChiveFile ** ) Left );
    Rf = * ( ( struct karChiveFile ** ) Right );

    karChiveDSSize ( Lf -> _data_source, & Ls );
    Lf -> _new_byte_size = Ls;

    karChiveDSSize ( Rf -> _data_source, & Rs );
    Rf -> _new_byte_size = Rs;


    return Lf -> _new_byte_size - Rf -> _new_byte_size;
}   /* _karChiveWriterSortFilesCallback () */

static
rc_t
_karChiveWriterSortFiles ( struct karChiveWriter * self )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcWriting, rcSelf, rcNull );
    }

    ksort (
            self -> _files,
            self -> _files_qty,
            sizeof ( struct karChiveFile * ),
            _karChiveWriterSortFilesCallback,
            NULL
            );

    return 0;
}   /* _karChiveWriterSortFiles () */

static
void
_karChiveWriterPrepareChiveTOCCallback (
                                    const struct karChiveEntry * Entry,
                                    void * Data
)
{
    struct karChiveWriter * Writer = ( struct karChiveWriter * ) Data;

    if ( Writer -> _rc != 0 ) {
        return;
    }

    if ( Entry -> _type == kptFile ) {
        Writer -> _rc = _karChiveWriterAddFile (
                                        Writer,
                                        ( struct karChiveFile * ) Entry
                                        );
    }
}   /* _karChiveWriterPrepareChiveTOCCallback () */

static
uint64_t
align_offset ( uint64_t offset, uint64_t alignment )
{
    uint64_t mask = alignment - 1;
    return ( offset + mask ) & ( ~ mask );
}

static
rc_t
_karChiveWriterPrepareChiveTOC ( struct karChiveWriter * self )
{
    rc_t RCt;
    uint32_t llp;
    uint64_t Offset;
    uint64_t Size;

    RCt = 0;
    llp = 0;
    Offset = 0;
    Size = 0;

    if ( self == NULL ) {
        self -> _rc = RC ( rcApp, rcNode, rcWriting, rcSelf, rcNull );
        return self -> _rc;
    }

    RCt = _karChiveWriterClearFiles ( self );
    if ( RCt == 0 ) {
        _karChiveEntryForEach (
                        (const struct karChiveEntry * ) self -> _chive -> _root,
                        _karChiveWriterPrepareChiveTOCCallback,
                        self
                        );
        RCt = self -> _rc;
        if ( RCt == 0 ) {
            RCt = _karChiveWriterSortFiles ( self );
            if ( RCt == 0 ) {
                    /*  Going and assigning new offsets and file sizes
                     */
                for ( llp = 0; llp < self -> _files_qty; llp ++ ) {
                    Size = self -> _files [ llp ] -> _new_byte_size;

                    self -> _files [ llp ] -> _new_byte_offset = Offset;

                    Offset = align_offset ( Offset + Size, 4 );
                }

                    /*  Evaluating TOC size
                     */
                RCt = BSTreePersist (
                                & ( self -> _chive -> _root -> _entries ),
                                & ( self -> _toc_size ),
                                NULL,
                                NULL,
                                _karChivePersist,
                                NULL
                                );
            }
        }
    }

    if ( RCt == 0 && self -> _rc != 0 ) {
        RCt = self -> _rc;
    }

    return RCt;
}   /* _karChiveWriterPrepareChiveTOC () */

static
rc_t
_karChiveWriterWriteChiveHeaderAndTOC ( struct karChiveWriter * self )
{
    rc_t RCt;
    struct KSraHeader Header;
    size_t HeaderSize;
    size_t NumWrit;

    RCt = 0;
    memset ( & Header, 0, sizeof ( struct KSraHeader ) );
    HeaderSize = 0;
    NumWrit = 0;

    memmove ( Header . ncbi, "NCBI", sizeof ( Header . ncbi ) );
    memmove ( Header . sra, ".sra", sizeof ( Header . sra ) );

    Header . byte_order = eSraByteOrderTag;
    Header . version = 1;

    HeaderSize  = sizeof ( Header )
                - sizeof ( Header . u )
                + sizeof ( Header . u . v1 )
                ;

    Header . u . v1 . file_offset =
                    align_offset ( HeaderSize + self -> _toc_size, 4 );

    self -> _start_pos = Header . u . v1 . file_offset;

    RCt = KFileWriteAll (
                        self -> _output,
                        self -> _curr_pos,
                        & Header,
                        HeaderSize,
                        &NumWrit
                        );
    if ( RCt == 0 ) {
        if ( NumWrit != HeaderSize ) {
            RCt = RC ( rcApp, rcFile, rcWriting, rcTransfer, rcIncomplete );
        }
        else {
            self -> _curr_pos += NumWrit;
            RCt = BSTreePersist (
                            & ( self -> _chive -> _root -> _entries ),
                            & ( self -> _toc_size ),
                            _karChiveWriterFunction,
                            self,
                            _karChivePersist,
                            NULL
                            );
        }
    }

    return RCt;
}   /* _karChiveWriterWriteChiveHeaderAndTOC () */

static
rc_t
_karChiveWriterAlignBytes ( struct karChiveWriter * self )
{
    rc_t RCt;
    size_t AlignSize;
    char AlignBuffer [ 4 ] = "0000";    /* LOL */

    RCt = 0;
    AlignSize = 0;

    AlignSize = align_offset ( self -> _curr_pos, 4 )
                                                    - self -> _curr_pos;

    if ( AlignSize != 0 ) {
        RCt =  KFileWriteAll (
                            self -> _output,
                            self -> _curr_pos,
                            AlignBuffer,
                            AlignSize,
                            NULL
                            );
        if ( RCt == 0 ) {
            self -> _curr_pos += AlignSize;
        }
    }

    return RCt;
}   /* _karChiveWriterAlignBytes () */

static
rc_t
_karChiveWrtierCopyFile (
                        struct karChiveWriter * self,
                        const struct karChiveFile * File
)
{
    rc_t RCt;
    size_t Num2R, NumR, Pos, DSize, NumW;
    size_t BufSz;

    RCt = 0;
    Num2R = NumR = Pos = DSize = NumW = 0;
    BufSz = 1048576 * 32;

    if ( self -> _curr_pos - self -> _start_pos != File -> _new_byte_offset ) {
        return RC ( rcApp, rcFile, rcWriting, rcOffset, rcInvalid );
    }

    RCt = karChiveDSSize ( File -> _data_source, & DSize );
    if ( RCt == 0 ) {
        if ( self -> _buffer == NULL ) {
            self -> _buffer = malloc ( sizeof ( char ) * BufSz );

            if ( self -> _buffer == NULL ) {
                RCt = RC ( rcApp, rcFile, rcWriting, rcMemory, rcExhausted );
            }
        }

        if ( RCt == 0 ) {
            while ( Pos < DSize ) {
                Num2R = DSize - Pos;
                if ( BufSz < Num2R ) {
                    Num2R = BufSz;
                }

                RCt = karChiveDSRead (
                                    File -> _data_source,
                                    Pos,
                                    self -> _buffer,
                                    Num2R,
                                    & NumR
                                    );
                if ( RCt == 0 ) {
                    if ( NumR == 0 ) {
                            /*  Actually it is end of file
                             */
                        break;
                    }
                    else {
                        RCt = KFileWriteAll (
                                            self -> _output,
                                            self -> _curr_pos,
                                            self -> _buffer,
                                            NumR,
                                            & NumW
                                            );
                        if ( RCt == 0 ) {
                            if ( NumR != NumW ) {
                                RCt = RC ( rcApp, rcFile, rcWriting, rcTransfer, rcIncomplete );
                            }
                            else {
                                Pos += NumR;
                            }
                        }
                    }
                }

                if ( RCt != 0 ) {
                        /*  What did I expect here ?
                         */
                    break;
                }
            }

            if ( RCt == 0 ) {
                self -> _curr_pos += Pos;
            }
        }
    }

    return RCt;
}   /* _karChiveWrtierCopyFile () */

static
rc_t
_karChiveWriterWriteFile ( struct karChiveWriter * self, size_t FileNo )
{
    rc_t RCt;
    const struct karChiveFile * File;

    RCt = 0;
    File = NULL;


    if ( self == NULL ) {
        return RC ( rcApp, rcFile, rcWriting, rcSelf, rcNull );
    }

    if ( self -> _files_qty <= FileNo ) {
        return RC ( rcApp, rcFile, rcWriting, rcParam, rcInvalid );
    }

        /* Temporarily, there will be small missunderstanding according
         * to that
         */
    File = self -> _files [ FileNo ];
    if ( File == NULL ) {
        return RC ( rcApp, rcFile, rcWriting, rcParam, rcInvalid );
    }

        /*  Best file size to deal with :D
         */
    if ( File -> _new_byte_size == 0 ) {
        return 0;
    }

        /*  First we do aligh file
         */
    RCt = _karChiveWriterAlignBytes ( self );
    if ( RCt == 0 ) {
        RCt = _karChiveWrtierCopyFile ( self, File );
    }

    return RCt;
}   /* _karChiveWriterWriteFile () */

LIB_EXPORT
rc_t
karChiveWriteFile (
                    const struct karChive * Chive,
                    struct KFile * File
)
{
    rc_t RCt;
    struct karChiveWriter Writer;
    size_t llp;

    RCt = 0;
    llp = 0;

    if ( Chive == NULL ) {
        return RC ( rcApp, rcNode, rcWriting, rcSelf, rcNull );
    }

    if ( File == NULL ) {
        return RC ( rcApp, rcNode, rcWriting, rcParam, rcNull );
    }

    RCt = _karChiveWriterInit ( & Writer, Chive, File );
    if ( RCt == 0 ) {

        if ( RCt == 0 ) {
            RCt = _karChiveWriterPrepareChiveTOC ( & Writer );
            if ( RCt == 0 ) {
                RCt = _karChiveWriterWriteChiveHeaderAndTOC ( & Writer );
                if ( RCt == 0 ) {
                    for ( llp = 0; llp < Writer . _files_qty; llp ++ ) {
                        RCt = _karChiveWriterWriteFile (
                                                        & Writer,
                                                        llp
                                                        );
                        if ( RCt != 0 ) {
                            break;
                        }
                    }
                }
            }
        }
        _karChiveWriterWhack ( & Writer );
    }

    return RCt;
}   /* karChiveWriteFile () */

LIB_EXPORT
rc_t
karChiveWrite (
                const struct karChive * Chive,
                const char * Path,
                bool Force
)
{
    rc_t RCt;
    struct KDirectory * NatDir;
    struct KFile * File;
    KCreateMode Mode;

    RCt = 0;
    NatDir = NULL;
    File = NULL;
    Mode = 0;

    if ( Chive == NULL ) {
        return RC ( rcApp, rcNode, rcWriting, rcSelf, rcNull );
    }

    if ( Path == NULL ) {
        return RC ( rcApp, rcNode, rcWriting, rcParam, rcNull );
    }

    RCt = KDirectoryNativeDir ( & NatDir );
    if ( RCt == 0 ) {
        Mode = ( Force ? kcmInit : kcmCreate ) | kcmParents;

        RCt = KDirectoryCreateFile ( NatDir, & File, false, 0666, Mode, "%s", Path );
        if ( RCt == 0 ) {
            RCt = karChiveWriteFile ( Chive, File );

            KFileRelease ( File );
        }
        else {
            pLogErr ( klogErr, RCt, "Can not open file for write [$(name)]", "name=%s", Path );
        }

        KDirectoryRelease ( NatDir );
    }

    return RCt;
}   /* karChiveWrite () */

LIB_EXPORT
rc_t
karChiveDump ( const struct karChive * self, bool MoarDetails )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcListing, rcSelf, rcNull );
    }

    if ( self -> _root == NULL ) {
        return RC ( rcApp, rcArc, rcListing, rcSelf, rcInvalid );
    }

    _karChiveEntryDump (
                        & ( self -> _root -> _da_da_dad ),
                        MoarDetails
                        );
    return 0;
}   /* karChiveDump () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * 
 *  SANCTUARY: place for dead things
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

#ifdef NEED_DUPLICATE
static rc_t _karChiveEntryCopy (
                                const struct karChiveEntry * self,
                                const struct karChiveDir * Parent,
                                struct karChiveEntry ** Copy
                                );

static
rc_t
_karChiveEntryAllocCopyFields (
                                const struct karChiveEntry * self,
                                const struct karChiveDir * Parent,
                                size_t Size,
                                const struct karChiveEntry ** Copy
)
{
    rc_t RCt;
    struct karChiveEntry * Ret;

    RCt = 0;
    Ret = NULL;

    /* No usual checks */

    if ( Size < sizeof ( struct karChiveEntry ) ) {
        return RC ( rcApp, rcNode, rcAllocating, rcParam, rcInvalid );
    }

    Ret = calloc ( 1, sizeof ( char ) * Size );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcNode, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        RCt = _copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                ( const char ** ) & ( Ret -> _name ),
                                self -> _name
                                );
        if ( RCt == 0 ) {
            Ret -> _parent = ( struct karChiveDir * ) Parent;
            Ret -> _mod_time = self -> _mod_time;
            Ret -> _access_mode = self -> _access_mode;
            if ( ( self -> _type & kptAlias ) == kptAlias ) {
                Ret -> _type = kptAlias;
            }
            else {
                Ret -> _type = self -> _type;
            }

            * Copy = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Copy = NULL;

        if ( Ret != NULL ) {
            _karChiveEntryRelease ( Ret );
        }
    }

    return RCt;
}   /* _karChiveEntryAllocCopyFields () */

static
rc_t
_karChiveAliasCopy (
                    const struct karChiveAlias * self,
                    const struct karChiveDir * Parent,
                    struct karChiveEntry ** Copy
)
{
    rc_t RCt;
    struct karChiveAlias * Alias;
    struct karChiveAlias * Ret;

    RCt = 0;
    Alias = NULL;
    Ret = NULL;

    if ( Copy != NULL ) {
        * Copy = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcNull );
    }

    if ( Copy == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcParam, rcNull );
    }

    if ( ( self -> _da_da_dad . _type & kptAlias ) != kptAlias ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcInvalid );
    }

    Alias = ( struct karChiveAlias * ) self;

    RCt = _karChiveEntryAllocCopyFields (
                                ( const struct karChiveEntry * ) self,
                                Parent,
                                sizeof ( struct karChiveAlias ),
                                ( const struct karChiveEntry ** ) & Ret
                                );
    if ( RCt == 0 ) {
        RCt = _copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                ( const char ** ) & ( Ret -> _link ),
                                self -> _link
                                );
        if ( RCt == 0 ) {
            Ret -> _resolved = NULL;    /* don't need to do that */
            Ret -> _is_invalid = Alias -> _is_invalid;

            * Copy = ( struct karChiveEntry * ) Ret;
        }
    }

    if ( RCt != 0 ) {
        Copy = NULL;

        if ( Ret != NULL ) {
            _karChiveEntryRelease ( ( const struct karChiveEntry * ) Ret );
        }
    }

    return RCt;
}   /* _karChiveAliasCopy () */

static
rc_t
_karChiveFileCopy (
                    const struct karChiveFile * self,
                    const struct karChiveDir * Parent,
                    struct karChiveEntry ** Copy
)
{
    rc_t RCt;
    struct karChiveFile * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Copy != NULL ) {
        * Copy = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcNull );
    }

    if ( Copy == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcParam, rcNull );
    }

    if ( ( self -> _da_da_dad . _type ) != kptFile ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcInvalid );
    }

    RCt = _karChiveEntryAllocCopyFields (
                                ( const struct karChiveEntry * ) self,
                                Parent,
                                sizeof ( struct karChiveFile ),
                                ( const struct karChiveEntry ** ) & Ret
                                );
    if ( RCt == 0 ) {
        Ret -> _byte_offset = self -> _byte_offset;
        Ret -> _byte_size = self -> _byte_size;

        Ret -> _new_byte_offset = 0;
        Ret -> _new_byte_size = 0;

        * Copy = ( struct karChiveEntry * ) Ret;
    }

    if ( RCt != 0 ) {
        Copy = NULL;

        if ( Ret != NULL ) {
            _karChiveEntryRelease ( ( const struct karChiveEntry * ) Ret );
        }
    }

    return RCt;
}   /* _karChiveFileCopy () */

static
void
_karChiveDirCopyCallback ( struct BSTNode * Node, void * Data )
{
    const struct karChiveEntry * Src;
    struct karChiveEntry * Dst;
    struct FrogBrigade * Brigade;

    Src = ( const struct karChiveEntry * ) Node;
    Dst = NULL;
    Brigade = ( struct FrogBrigade * ) Data;

    if ( Brigade == NULL ) {
        return;
    }

    if ( Brigade -> _rc != 0 ) {
        return;
    }

    if ( Src == NULL ) {
        Brigade -> _rc = RC ( rcApp, rcNode, rcCopying, rcParam, rcNull );
        return;
    }

    Brigade -> _rc = _karChiveEntryCopy (
                                        Src,
                                        Brigade -> _parent,
                                        & Dst
                                        );
    if ( Brigade -> _rc == 0 ) {
        Brigade -> _rc = BSTreeInsert (
                                & ( Brigade -> _parent -> _entries ),
                                & ( Dst -> _da_da_dad ),
                                _compareEntries
                                );
    }
}   /* _karChiveDirCopyCallback () */

static
rc_t
_karChiveDirCopy (
                    const struct karChiveDir * self,
                    const struct karChiveDir * Parent,
                    struct karChiveEntry ** Copy
)
{
    rc_t RCt;
    struct karChiveDir * Dir;
    struct karChiveDir * Ret;
    struct FrogBrigade Brigade;

    RCt = 0;
    Dir = NULL;
    Ret = NULL;
    memset ( & Brigade, 0, sizeof ( struct FrogBrigade ) );

    if ( Copy != NULL ) {
        * Copy = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcNull );
    }

    if ( Copy == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcParam, rcNull );
    }

    if ( ( self -> _da_da_dad . _type ) != kptDir ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcInvalid );
    }

    Dir = ( struct karChiveDir * ) self;

    RCt = _karChiveEntryAllocCopyFields (
                                ( const struct karChiveEntry * ) Dir,
                                Parent,
                                sizeof ( struct karChiveDir ),
                                ( const struct karChiveEntry ** ) & Ret
                                );
    if ( RCt == 0 ) {
        BSTreeInit ( & ( Dir -> _entries ) );

        Brigade . _parent = Dir;

        BSTreeForEach (
                        & ( Dir -> _entries ),
                        false,
                        _karChiveDirCopyCallback,
                        & Brigade
                        );

        * Copy = ( struct karChiveEntry * ) Ret;
    }

    if ( RCt != 0 ) {
        Copy = NULL;

        if ( Ret != NULL ) {
            _karChiveEntryRelease ( ( const struct karChiveEntry * ) Ret );
        }
    }

    return RCt;
}   /* _karDireFileCopy () */

static
rc_t
_karChiveEntryCopy (
                    const struct karChiveEntry * self,
                    const struct karChiveDir * Parent,
                    struct karChiveEntry ** Copy
)
{
    rc_t RCt;
    struct karChiveEntry * Ret;
    uint8_t Type;

    RCt = 0;
    Ret = NULL;
    Type = 0;

    if ( Copy != NULL ) {
        * Copy = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcNull );
    }

    if ( Copy == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcParam, rcNull );
    }

    Type = self -> _type;
    if ( ( Type & kptAlias ) == kptAlias ) {
        Type = kptAlias;
    }

    switch ( Type ) {
        case kptAlias :
            RCt = _karChiveAliasCopy (
                                ( const struct karChiveAlias * ) self,
                                Parent,
                                & Ret
                                );
            break;
        case kptFile :
            RCt = _karChiveFileCopy (
                                ( const struct karChiveFile * ) self,
                                Parent,
                                & Ret
                                );
            break;
        case kptDir :
            RCt = _karChiveDirCopy (
                                ( const struct karChiveDir * ) self,
                                Parent,
                                & Ret
                                );
            break;
        default :
            RCt =  RC ( rcApp, rcNode, rcCopying, rcParam, rcInvalid );
            break;
    }

    if ( RCt == 0 ) {
        * Copy = Ret;
    }
    else {
        * Copy = NULL;

        if ( Ret != NULL ) {
            _karChiveEntryRelease ( Ret );
        }
    }

    return RCt;
}   /* _karChiveEntryCopy () */

static
rc_t
_karChiveEntryDuplicate (
                        const struct karChiveEntry * self,
                        struct karChiveEntry ** Duplicate
)
{
    rc_t RCt;
    struct karChiveEntry * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Duplicate != NULL ) {
        * Duplicate = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcSelf, rcNull );
    }

    if ( Duplicate == NULL ) {
        return RC ( rcApp, rcNode, rcCopying, rcParam, rcNull );
    }

    RCt = _karChiveEntryCopy ( self, NULL, & Ret );
    if ( RCt == 0 ) {

        if ( Ret -> _type == kptDir ) {
            RCt = _karChiveResolveAliases (
                                    ( const struct karChiveDir * ) Ret
                                    );
        }
        if ( RCt == 0 ) {
            * Duplicate = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Duplicate = NULL;

        if ( Ret != NULL ) {
            _karChiveEntryRelease ( Ret );
        }
    }

    return RCt;
}   /* _karChiveEntryDuplicate () */
#endif /* NEED_DUPLICATE */

#ifdef NEED_ENTY_LIST
static
void
_karChiveDirListCallback ( struct BSTNode * Node, void * Data )
{
    VNamelistAppend (
                    ( struct VNamelist * ) Data,
                    ( ( const struct karChiveEntry * ) Node ) -> _name
                    );
}   /* _karChiveDirListCallback () */

static
rc_t
_karChiveEntryList ( 
                    const struct karChiveEntry * self,
                    struct KNamelist ** List
)
{
    rc_t RCt;
    struct VNamelist * NL;

    RCt = 0;
    NL = NULL;

    if ( List != NULL ) {
        * List = NULL;
    }

    if ( self == NULL ) {
        RCt = RC ( rcApp, rcNode, rcListing, rcSelf, rcNull );
    }

    if ( List == NULL ) {
        RCt = RC ( rcApp, rcNode, rcListing, rcParam, rcNull );
    }

    if ( self -> _type == kptDir ) {
         struct karChiveDir * Dir = ( struct karChiveDir * ) self;

        RCt = VNamelistMake ( & NL, 16 );
        if ( RCt == 0 ) {
            BSTreeForEach (
                        & ( Dir -> _entries ),
                        false,
                        _karChiveDirListCallback,
                        ( void * ) NL
                        );

            RCt = VNamelistToNamelist ( NL, List );

            VNamelistRelease ( NL );
        }
    }
    else {
        if ( self -> _type == ( kptDir | kptAlias ) ) {
            struct karChiveAlias * Alias =
                                        ( struct karChiveAlias * ) self;
            if ( ! Alias -> _is_invalid ) {
                RCt = _karChiveEntryList ( Alias -> _resolved, List );
            }
        }
        else {
            RCt = RC ( rcApp, rcNode, rcListing, rcSelf, rcEmpty );
        }
    }

    return RCt;
}   /* _karChiveEntryList () */
#endif /* NEED_ENTY_LIST */
