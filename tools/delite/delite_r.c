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
#include <klib/time.h>
#include <klib/printf.h>

#include <kapp/main.h>

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/mmap.h>

#include <kfg/config.h>

#include <sysalloc.h>
#include <stdio.h>
#include <string.h>

#include "delite.h"

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Kinda rated R content ...
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Params
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
LIB_EXPORT
rc_t CC
DeLiteParamsInit ( struct DeLiteParams * Params )
{
    memset ( Params, 0, sizeof ( struct DeLiteParams ) );

    Params -> _force_write = true;

    return 0;
}   /* DeLiteParamsInit () */

LIB_EXPORT
rc_t CC
DeLiteParamsWhack ( struct DeLiteParams * Params )
{
    if ( Params != NULL ) {
        if ( Params -> _config != NULL ) {
            KConfigRelease ( Params -> _config );
            Params -> _config = NULL;
        }

        if ( Params -> _program != NULL ) {
            free ( ( char * ) Params -> _program );
            Params -> _program = NULL;
        }
        if ( Params -> _accession != NULL ) {
            free ( ( char * ) Params -> _accession );
            Params -> _accession = NULL;
        }
        if ( Params -> _accession_path != NULL ) {
            free ( ( char * ) Params -> _accession_path );
            Params -> _accession_path = NULL;
        }
        if ( Params -> _output != NULL ) {
            free ( ( char * ) Params -> _output );
            Params -> _output = NULL;
        }
        if ( Params -> _schema != NULL ) {
            free ( ( char * ) Params -> _schema );
            Params -> _schema = NULL;
        }
        if ( Params -> _transf != NULL ) {
            free ( ( char * ) Params -> _transf );
            Params -> _transf = NULL;
        }
        Params -> _output_stdout = false;
        Params -> _force_write = true;

        Params -> _check = false;
        Params -> _noedit = false;
        Params -> _update = false;
        Params -> _delite = false;
    }
    return 0;
}   /* DeLiteParamsWhack () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Just copying string - don't forget to free it
 *  Does not allow to copy string of any length ... surprize?
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
LIB_EXPORT
rc_t CC
copyStringSayNothingRelax ( const char ** Dst, const char * Src )
{
    size_t Len;
    rc_t RCt;
    char * Ret;

    Len = 0;
    RCt = 0;
    Ret = NULL;

        /* first we will check that Src is short enough
         */
    if ( Src == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    Len = 0;

    while ( Len < KAR_MAX_ACCEPTED_STRING ) {
        if ( * ( Src + Len ) == 0 ) {
            break;
        }
        Len ++;
    }

    if ( KAR_MAX_ACCEPTED_STRING <= Len ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcTooBig );
    }

    if ( RCt == 0 ) {
        if ( Dst != NULL ) {
            * Dst = NULL;

            Ret = calloc ( 1, sizeof ( char ) * Len + 1 );
            if ( Ret != NULL ) {
                strcpy ( Ret, Src );

                * Dst = Ret;
            }
            else {
                RCt = RC ( rcApp, rcString, rcCopying, rcParam, rcExhausted );
            }
        }
        else {
            RCt = RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
        }
    }

    if ( RCt != 0 ) {
        * Dst = NULL;

        if ( Ret != NULL ) {
            free ( Ret );
        }
    }

    return RCt;
}   /* copyStringSayNothingRelax () */

LIB_EXPORT
rc_t CC
copyLStringSayNothing (
                        const char ** Dst,
                        const char * Str,
                        size_t Len
)
{
    rc_t RCt;
    char * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Dst == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    * Dst = NULL;

    if ( Str == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    if ( KAR_MAX_ACCEPTED_STRING <= Len ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcTooBig );
    }

    Ret = calloc ( Len + 1, sizeof ( char ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcString, rcCopying, rcMemory, rcExhausted );
    }
    else {
        if ( Len != 0 ) {
            memmove ( Ret, Str, Len * sizeof ( char ) );
        }
        Ret [ Len ] = 0;

        * Dst = Ret;
    }

    return RCt;
}   /* copyLStringSayNothing () */

LIB_EXPORT
rc_t CC
copySStringSayNothing (
                        const char ** Dst,
                        const char * Begin,
                        const char * End
)
{
    if ( Dst == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    * Dst = NULL;

    if ( Begin == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    if ( End == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    if ( End < Begin ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcInvalid );
    }

    return copyLStringSayNothing ( Dst, Begin, End - Begin );
}   /* copySStringSayNothing () */

/******************************************************************************/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  VPath to char *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
LIB_EXPORT
rc_t CC
DeLiteVPathToChar ( char ** CharPath, const struct VPath * Path )
{
    rc_t RCt;
    const struct String * Str;
    char * RetVal;
    char BB [ 4096 ];

    RCt = 0;
    Str = NULL;
    * BB = 0;

    if ( CharPath != NULL ) {
        * CharPath = NULL;
    }

    if ( Path == NULL ) {
        return RC ( rcApp, rcPath, rcConverting, rcParam, rcNull );
    }

    if ( CharPath == NULL ) {
        return RC ( rcApp, rcPath, rcConverting, rcParam, rcNull );
    }

    RCt = VPathMakeString ( Path, & Str );
    if ( RCt == 0 ) {
        string_copy ( BB, sizeof ( BB ), Str -> addr, Str -> size );
        BB [ Str -> size ] = 0;

        RCt = copyStringSayNothingRelax ( ( const char ** ) & RetVal, BB );
        if ( RCt == 0 ) {
            * CharPath = RetVal;
        }

        StringWhack ( Str );
    }

    return RCt;
}   /* DeLiteVPathToChar () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Weird resolver stuff
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/*)))
  \\\   Simple plan: first GetRepository, second CreateResolver,
   \\\  third Resolve ...
   (((*/
LIB_EXPORT
rc_t CC
DeLiteResolvePath (
                    char ** ResolvedLocalPath,
                    const char * PathOrAccession
)
{
    rc_t RCt;
    struct VFSManager * VfsMan;
    struct VResolver * Resolver;
    const struct VPath * QueryPath;
    const struct VPath * LocalPath;

    RCt = 0;
    VfsMan = NULL;
    Resolver = NULL;
    QueryPath = NULL;
    LocalPath = NULL;

    if ( ResolvedLocalPath != NULL ) {
        * ResolvedLocalPath = NULL;
    }

    if ( PathOrAccession == NULL ) {
        return RC ( rcApp, rcPath, rcResolving, rcParam, rcNull );
    }

    if ( ResolvedLocalPath == NULL ) {
        return RC ( rcApp, rcPath, rcResolving, rcParam, rcNull );
    }

    RCt = VFSManagerMake ( & VfsMan );
    if ( RCt == 0 ) {
        RCt = VFSManagerGetResolver ( VfsMan, & Resolver );
        if ( RCt == 0 ) {
            RCt = VFSManagerMakePath (
                                VfsMan,
                                ( struct VPath ** ) & QueryPath,
                                "%s",
                                PathOrAccession
                                );
            if ( RCt == 0 ) {
                RCt = VResolverQuery (
                                    Resolver,       /* resolver */
                                    0,              /* protocols */
                                    QueryPath,      /* query */
                                    & LocalPath,    /* local */
                                    NULL,           /* remote */
                                    NULL            /* cache */
                                    );
                if ( RCt == 0 ) {
                    if ( LocalPath != NULL ) {
                        RCt = DeLiteVPathToChar ( 
                                                ResolvedLocalPath,
                                                LocalPath
                                                );
                        VPathRelease ( LocalPath );
                    }
                    else {
                        RCt = RC ( rcApp, rcPath, rcResolving, rcPath, rcNotFound );
                    }
                }
                VPathRelease ( QueryPath );
            }
            VResolverRelease ( Resolver );
        }

        VFSManagerRelease ( VfsMan );
    }

    return RCt;
}   /* DeLiteResolvePath () */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Hardcored by Kurt's request
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
LIB_EXPORT
bool CC
IsQualityName ( const char * Name )
{
    if ( Name != NULL ) {
        if ( strcmp ( Name, "QUALITY" ) == 0 ) {
            return true;
        }
        if ( strcmp ( Name, "QUALITY2" ) == 0 ) {
            return true;
        }
        if ( strcmp ( Name, "CMP_QUALITY" ) == 0 ) {
            return true;
        }
    }
    return false;
}   /* IsQualityName () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Time for fume
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
LIB_EXPORT
rc_t CC
NowAsString ( char ** Str )
{
    rc_t RCt;
    char Buf [ 128 ];

    RCt = 0;
    * Buf = 0;

    if ( Str != NULL ) {
        * Str = NULL;
    }

    if ( Str == NULL ) {
        return RC ( rcApp, rcApp, rcAccessing, rcParam, rcNull );
    }

    RCt = NowToString ( Buf, sizeof ( Buf ) );
    if ( RCt == 0 ) {
        RCt = copyStringSayNothingRelax ( ( const char ** ) Str, Buf );
    }

    return RCt;
}   /* NowAsString () */

LIB_EXPORT
rc_t CC
NowToString ( char * Buf, size_t BufSize )
{
    struct KTime Time;

    memset ( & Time, 0, sizeof ( struct KTime ) );

    if ( Buf == NULL ) {
        return RC ( rcApp, rcApp, rcAccessing, rcParam, rcNull );
    }

    if ( BufSize < 32 ) {
        return RC ( rcApp, rcApp, rcAccessing, rcParam, rcInvalid );
    }

    KTimeLocal ( & Time, KTimeStamp () );

    return string_printf ( Buf, BufSize, NULL, "%lT", & Time );
}   /* NowToString () */

LIB_EXPORT
rc_t CC
VersAsString ( char ** Str )
{
    rc_t RCt;
    char Buf [ 128 ];

    RCt = 0;
    * Buf = 0;

    if ( Str != NULL ) {
        * Str = NULL;
    }

    if ( Str == NULL ) {
        return RC ( rcApp, rcApp, rcAccessing, rcParam, rcNull );
    }

    RCt = VersToString ( Buf, sizeof ( Buf ) );
    if ( RCt == 0 ) {
        RCt = copyStringSayNothingRelax ( ( const char ** ) Str, Buf );
    }

    return RCt;
}   /* VersAsString () */

LIB_EXPORT
rc_t CC
VersToString ( char * Buf, size_t BufSize )
{
    if ( Buf == NULL ) {
        return RC ( rcApp, rcApp, rcAccessing, rcParam, rcNull );
    }

    if ( BufSize < 32 ) {
        return RC ( rcApp, rcApp, rcAccessing, rcParam, rcInvalid );
    }

    return string_printf ( Buf, BufSize, NULL, "%.3V", KAppVersion () );
}   /* VersToString () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Bufer for suffer
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

#define KAR_CBUF_INCS       32768       /* 1024 * 32 */

rc_t CC
karCBufWhack ( struct karCBuf * self )
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
}   /* karCBufWhack () */

rc_t CC
karCBufDispose ( struct karCBuf * self )
{
    if ( self != NULL ) {
        karCBufWhack ( self );

        free ( self );
    }
    return 0;
}   /* karCBufDispose () */

rc_t
karCBufInit ( struct karCBuf * self, size_t Reserve )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAllocating, rcSelf, rcNull );
    }

    memset ( self, 0, sizeof ( struct karCBuf ) );

    if ( Reserve != 0 ) {
        self -> _b = calloc ( Reserve, sizeof ( char ) );
        if ( self -> _b == NULL ) {
            return RC ( rcApp, rcBuffer, rcAllocating, rcMemory, rcExhausted );
        }
    }

    self -> _c = Reserve;
    self -> _s = 0;

    return 0;
}   /* karCBufInit () */

/* Reserve could be '0' in that case it will be set randomly later
 */
rc_t CC
karCBufMake ( struct karCBuf ** Buf, size_t Reserve )
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
        RCt = karCBufInit ( Ret, Reserve );
        if ( RCt == 0 ) {
            * Buf = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Buf = NULL;

        if ( Ret != NULL ) {
            karCBufDispose ( Ret );
        }
    } 

    return RCt;
}   /* karCBufMake () */

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

rc_t
karCBufSet (
            struct karCBuf * self,
            size_t Off,
            void * Data,
            size_t DSize
)
{
    rc_t RCt;
    size_t NewSize;

    RCt = 0;
    NewSize = Off + DSize;

    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcSelf, rcNull );
    }

    if ( Data == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcParam, rcNull );
    }

    if ( DSize == 0 ) {
            /*  I bet it is better to return some smarty RC code here
             */
        return 0;
    }

    if ( self -> _s <= NewSize ) {
        RCt = _karCBufRealloc ( self, NewSize );
    }

    if ( RCt == 0 ) {
        memmove ( ( ( char * ) self -> _b ) + Off, Data, DSize );
        self -> _s = NewSize;
    }

    return RCt;
}   /* karCBufSet () */

rc_t
karCBufAppend ( struct karCBuf * self, void * Data, size_t DSize )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcSelf, rcNull );
    }

    return karCBufSet ( self, self -> _s, Data, DSize );
}   /* karCBufAppend () */

rc_t
karCBufGet (
                struct karCBuf * self,
                const void ** Data,
                size_t * DSize
)
{
    if ( Data != NULL ) {
        * Data = NULL;
    }

    if ( DSize != NULL ) {
        * DSize = 0;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcSelf, rcNull );
    }

    if ( Data == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcParam, rcNull );
    }

    if ( DSize == NULL ) {
        return RC ( rcApp, rcBuffer, rcAccessing, rcParam, rcNull );
    }

    * Data = self -> _b;
    * DSize = self -> _s;

    return 0;
}   /* karCBufGet () */

rc_t
karCBufDetatch (
                struct karCBuf * self,
                const void ** Data,
                size_t * DSize
)
{
    rc_t RCt;

    RCt = karCBufGet ( self, Data, DSize );
    if ( RCt == 0 ) {
        self -> _b = NULL;
        self -> _s = 0;
        self -> _c = 0;
    }

    return RCt;
}   /* karCBufDetach () */


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  LineReader for line
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karLnRd {
    const struct KMMap * _map;

    const void * _map_addr;
    size_t _map_size;

    bool _is_good;

    size_t _start;
    size_t _stop;
    size_t _line_no;
};

rc_t CC
karLnRdDispose ( const struct karLnRd * self )
{
    struct karLnRd * Rd = ( struct karLnRd * ) self;

    if ( Rd != NULL ) {
        if ( Rd -> _map != NULL ) {
            KMMapRelease ( Rd -> _map );
            Rd -> _map = NULL;
        }

        if ( Rd -> _map_addr != NULL ) {
            Rd -> _map_addr = NULL;
        }
        Rd -> _map_size = 0;

        Rd -> _is_good = 0;
        Rd -> _start = 0;
        Rd -> _stop = 0;
        Rd -> _line_no = 0;

        free ( Rd );
    }

    return 0;
}   /* karLnRdDispose () */

rc_t CC
karLnRdOpen ( const struct karLnRd ** LineReader, const char * Path )
{
    rc_t RCt;
    struct KDirectory * Nat;
    const struct KFile * File;
    const struct KMMap * Map;
    const void * Addr;
    size_t Size;

    RCt = 0;
    Nat = NULL;
    File = NULL;
    Map = NULL;
    Addr = NULL;
    Size = 0;

    if ( LineReader != NULL ) {
        * LineReader = NULL;
    }

    if ( LineReader == NULL || Path == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    RCt = KDirectoryNativeDir ( & Nat );
    if ( RCt == 0 ) {
        RCt = KDirectoryOpenFileRead ( Nat, & File, "%s", Path );
        if ( RCt == 0 ) {
            RCt = KMMapMakeRead ( & Map, File );
            if ( RCt == 0 ) {
                RCt = KMMapAddrRead ( Map, & Addr );
                if ( RCt == 0 ) {
                    RCt = KMMapSize ( Map, & Size );
                    if ( RCt == 0 ) {
                        RCt = karLnRdMake ( LineReader, Addr, Size );
                        if ( RCt == 0 ) {
                            ( ( struct karLnRd * ) * LineReader )
                                                        -> _map = Map;
                        }
                    }
                }
            }

            KFileRelease ( File );
        }

        KDirectoryRelease ( Nat );
    }

    return RCt;
}   /* karLnRdOpen () */

rc_t CC
karLnRdMake (
            const struct karLnRd ** LineReader,
            const void * Buf,
            size_t BufSz
)
{
    rc_t RCt;
    struct karLnRd * Ret;

    RCt = 0;
    Ret = NULL;

    if ( LineReader != NULL ) {
        * LineReader = NULL;
    }

    if ( LineReader == NULL || Buf == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( BufSz == 0 ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    Ret = calloc ( 1, sizeof ( struct karLnRd ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcData, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        Ret -> _map_addr = Buf;
        Ret -> _map_size = BufSz;
        Ret -> _start = 0;
        Ret -> _stop = 0;
        Ret -> _line_no = 0;
        Ret -> _is_good = true;

        * LineReader = Ret;
    }

    return RCt;
}   /* karLnRdMake () */

bool CC
karLnRdIsGood ( const struct karLnRd * self )
{
    return self != NULL ? self -> _is_good : false;
}   /* karLnRdIsGood () */

bool CC
karLnRdNext ( const struct karLnRd * self )
{
    struct karLnRd * LnRd;
    const char * iBg;
    const char * iCr;
    const char * iEn;
    bool Ret;

    LnRd = ( struct karLnRd * ) self;
    iBg = iEn = iCr = NULL;
    Ret = false;

    if ( self == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcNull );
    }

    if ( ! self -> _is_good ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcInvalid );
    }

    if ( LnRd != NULL ) {
        iBg = ( const char * ) LnRd -> _map_addr;
        iEn = iBg + LnRd -> _map_size;
        if ( LnRd -> _stop != 0 ) {
            iBg += LnRd -> _stop + 1;
        }
        iCr = iBg;
        if ( iEn <= iBg ) {
            LnRd -> _start = LnRd -> _map_size;
            LnRd -> _stop = LnRd -> _map_size;
            Ret = false;
        }
        else {
            while ( iCr < iEn ) {
                if ( * iCr == '\n' ) {
                    break;
                }

                iCr ++;
            }

            LnRd -> _start = iBg - ( ( const char * ) LnRd -> _map_addr );
            LnRd -> _stop = iCr - ( ( const char * ) LnRd -> _map_addr );
            LnRd -> _line_no ++;

            Ret = true;
        }
    }

    return Ret;
}   /* karLnRdNext () */

/*  That method allocate new string each time, so
 *  feel free to delete it
 */
rc_t CC
karLnRdGet ( const struct karLnRd * self, const char ** Line )
{
    rc_t RCt = 0;

    if ( Line != NULL ) {
        * Line = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcNull );
    }

    if ( ! self -> _is_good ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcInvalid );
    }

    if ( Line == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    if ( self -> _start < self -> _map_size ) {
        RCt = copyLStringSayNothing (
                                Line,
                                ( ( const char * ) self -> _map_addr ) + self -> _start,
                                self -> _stop - self -> _start
                                );
    }

    return RCt;
}   /* karLnRdGet () */

rc_t CC
karLnRdGetNo ( const struct karLnRd * self, size_t * LineNo )
{
    if ( LineNo != NULL ) {
        * LineNo = 0;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcNull );
    }

    if ( ! self -> _is_good ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcInvalid );
    }

    if ( LineNo == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcParam, rcNull );
    }

    * LineNo = self -> _line_no;

    return 0;
}   /* karLnRdGetNo () */

rc_t CC
rarLnRdRewind ( const struct karLnRd * self )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcNull );
    }

    if ( ! self -> _is_good ) {
        return RC ( rcApp, rcString, rcCopying, rcSelf, rcInvalid );
    }

    ( ( struct karLnRd * ) self ) -> _start = 0;
    ( ( struct karLnRd * ) self ) -> _stop = 0;
    ( ( struct karLnRd * ) self ) -> _line_no = 0;

    return 0;
}   /* rarLnRdRewind () */

