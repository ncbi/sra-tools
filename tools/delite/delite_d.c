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

#include <kfs/file.h>
#include <kfs/mmap.h>
#include <kfs/ramfile.h>

#include "delite_d.h"

#include <sysalloc.h>
#include <stdio.h>
#include <string.h>

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Data source, needed for unified access to node data
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveDS {
    KRefcount _refcount;

    rc_t ( CC * _on_dispose ) (
                        const struct karChiveDS * DataSource
                        );
    rc_t ( CC * _on_read ) (
                        const struct karChiveDS * DataSource,
                        uint64_t Offset,
                        void * Buffer,
                        size_t BufferSize,
                        size_t * NumReaded
                        );
    rc_t ( CC * _on_size ) (
                        const struct karChiveDS * DataSource,
                        uint64_t * Size
                        );
};

static const char * _skarChiveDS_classname = "karChiveDS";

static
rc_t
_karChiveDSMake (
                struct karChiveDS ** DS,
                size_t StructSize,
                rc_t ( CC * OnDispose ) (
                                const struct karChiveDS * DataSource
                                ),
                rc_t ( CC * OnRead ) (
                                const struct karChiveDS * DataSource,
                                uint64_t Offset,
                                void * Buffer,
                                size_t BufferSize,
                                size_t * NumReaded
                                ),
                rc_t ( CC * OnSize ) (
                                const struct karChiveDS * DataSource,
                                uint64_t * Size
                                )
)
{
    rc_t RCt;
    struct karChiveDS * Ret;

    RCt = 0;
    Ret = NULL;

    if ( DS != NULL ) {
        * DS = NULL;
    }

    if ( DS == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( StructSize < sizeof ( struct karChiveDS ) ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    Ret = calloc ( 1, StructSize );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcData, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        KRefcountInit (
                        & ( Ret -> _refcount ),
                        1,
                        _skarChiveDS_classname,
                        "_karChiveDSMake",
                        "DSMake"
                        );

        Ret -> _on_dispose = OnDispose;
        Ret -> _on_read = OnRead;
        Ret -> _on_size = OnSize;

        * DS = Ret;
    }

    return RCt;
}   /* _karChiveDSMake () */

static
rc_t
_karChiveDSDispose ( struct karChiveDS * self )
{
    if ( self != NULL ) {
        KRefcountWhack (
                        & ( self -> _refcount ),
                        _skarChiveDS_classname
                        );

        if ( self -> _on_dispose != NULL ) {
            self -> _on_dispose ( self );
        }

        self -> _on_dispose = NULL;
        self -> _on_read = NULL;
        self -> _on_size = NULL;

        free ( self );
    }

    return 0;
}   /* _karChiveDSDispose () */

rc_t
karChiveDSAddRef ( const struct karChiveDS * self )
{
    rc_t RCt;
    struct karChiveDS * DS;

    RCt = 0;
    DS = ( struct karChiveDS * ) self;

    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcAccessing, rcSelf, rcNull );
    }

    switch ( KRefcountAdd ( & ( DS -> _refcount ), _skarChiveDS_classname ) ) {
        case krefOkay :
            RCt = 0;
            break;
        case krefZero :
        case krefLimit :
        case krefNegative :
            RCt = RC ( rcApp, rcArc, rcAccessing, rcParam, rcNull );
        default :
            RCt = RC ( rcApp, rcArc, rcAccessing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* karChiveDSAddRef () */

rc_t
karChiveDSRelease ( const struct karChiveDS * self )
{
    rc_t RCt;
    struct karChiveDS * DS;

    RCt = 0;
    DS = ( struct karChiveDS * ) self;

    if ( self == NULL ) {
        return RC ( rcApp, rcArc, rcReleasing, rcSelf, rcNull );
    }

    switch ( KRefcountDrop ( & ( DS -> _refcount ), _skarChiveDS_classname ) ) {
        case krefOkay :
        case krefZero :
            RCt = 0;
            break;
        case krefWhack :
            RCt = _karChiveDSDispose ( DS );
            break;
        case krefNegative :
            RCt = RC ( rcApp, rcArc, rcReleasing, rcParam, rcInvalid );
            break;
        default :
            RCt = RC ( rcApp, rcArc, rcReleasing, rcParam, rcUnknown );
            break;
    }

    return RCt;
}   /* _karChiveDSRelease () */

rc_t CC
karChiveDSRead (
                const struct karChiveDS * self,
                uint64_t Offset,
                void * Buf,
                size_t BufSize,
                size_t * NumRead
)
{
    if ( self == NULL ) {
        return RC ( rcApp, rcData, rcReading, rcSelf, rcNull );
    }

    if ( self -> _on_read == NULL ) {
        return RC ( rcApp, rcData, rcReading, rcInterface, rcUnsupported );
    }

    return self -> _on_read ( self, Offset, Buf, BufSize, NumRead ) ;
}   /* karChiveDSRead () */


rc_t CC
karChiveDSReadAll (
                        const struct karChiveDS * self,
                        void ** DataRead,
                        size_t * ReadSize
)
{
    rc_t RCt;
    size_t DSize, Pos, Num2R, NumR;
    char * Buf;

    RCt = 0;
    DSize = Pos = Num2R = NumR = 0;
    Buf = NULL;

    if ( DataRead != NULL ) {
        * DataRead = NULL;
    }

    if ( ReadSize != NULL ) {
        * ReadSize = 0;
    }

    if ( self == 0 ) {
        return RC ( rcApp, rcData, rcReading, rcSelf, rcNull );
    }

    if ( DataRead == NULL ) {
        return RC ( rcApp, rcData, rcReading, rcParam, rcNull );
    }

    if ( ReadSize == NULL ) {
        return RC ( rcApp, rcData, rcReading, rcParam, rcNull );
    }

    RCt = karChiveDSSize ( self, & DSize );
    if ( RCt == 0 ) {
        if ( DSize == 0 ) {
            RCt = RC ( rcApp, rcData, rcReading, rcSize, rcEmpty );
        }
        else {
            Buf = malloc ( DSize );
            if ( Buf == NULL ) {
                RCt = RC ( rcApp, rcData, rcAllocating, rcMemory, rcExhausted );
            }
            else {
                while ( Pos < DSize ) {
                    Num2R = DSize - Pos;

                    RCt = karChiveDSRead (
                                        self,
                                        Pos,
                                        Buf + Pos,
                                        Num2R,
                                        & NumR
                                        );
                    if ( RCt != 0 ) {
                        break;
                    }

                    if ( NumR == 0 ) {
                        /*  End of File
                         */
                        break;
                    }

                    Pos += NumR;
                }

                if ( RCt == 0 ) {
                    if ( Pos != DSize ) {
                        RCt = RC ( rcApp, rcData, rcReading, rcItem, rcCorrupt );
                    }
                    else {
                        * DataRead = Buf;
                        * ReadSize = DSize;
                    }
                }

            }
        }
    }

    if ( RCt != 0 ) {
        * DataRead = NULL;
        * ReadSize = 0;

        if ( Buf != NULL ) {
            free ( Buf );
        }
    }

    return RCt;
}   /* karChiveDSReadAll () */

rc_t CC
karChiveDSSize ( const struct karChiveDS * self, size_t * Size )
{
    if ( self == NULL ) {
        return RC ( rcApp, rcData, rcReading, rcParam, rcNull );
    }

    if ( self -> _on_size == NULL ) {
        return RC ( rcApp, rcData, rcReading, rcInterface, rcUnsupported );
    }

    return self -> _on_size ( self, Size ) ;
}   /* karChiveDSSize () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Different sourcenile variants
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Standard KFile source : base for MMapDS and MemDS 
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveFileDS {
    struct karChiveDS _ds;

    const struct KFile * _file;

    uint64_t _data_offset;
    uint64_t _data_size;
};

static
rc_t CC
_karChiveFileDS_on_dispose ( const struct karChiveDS * DS )
{
    struct karChiveFileDS * FDS = ( struct karChiveFileDS * ) DS;

    if ( FDS != NULL ) {
        if ( FDS -> _file != NULL ) {
            KFileRelease ( FDS -> _file );
            FDS -> _file = NULL;
        }

        FDS -> _data_offset = 0;
        FDS -> _data_size = 0;
    }

    return 0;
}   /* _karChiveFileDS_on_dispose () */

static
rc_t CC
_karChiveFileDS_on_read (
                        const struct karChiveDS * DS,
                        uint64_t Offset,
                        void * Buffer,
                        size_t BufferSize,
                        size_t * NumReaded
)
{
    const struct karChiveFileDS * FDS =
                                ( const struct karChiveFileDS * ) DS;

    if ( NumReaded != NULL ) {
        * NumReaded = 0;
    }

        /*  Sure, most of these checks are stupid and many necessary
         *  are missed
         */
    if ( DS == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( Buffer == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( BufferSize == 0 ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    if ( FDS -> _file == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    return KFileReadAll (
                        FDS -> _file,
                        FDS -> _data_offset + Offset,
                        Buffer,
                        BufferSize,
                        NumReaded
                        );
}   /* _karChiveFileDS_on_read () */

static
rc_t CC
_karChiveFileDS_on_size (
                        const struct karChiveDS * DS,
                        uint64_t * Size
)
{
    if ( Size != NULL ) {
        * Size = 0;
    }

        /*  Sure, most of these checks are stupid and many necessary
         *  are missed
         */
    if ( DS == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( Size == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( ( ( const struct karChiveFileDS * ) DS ) -> _file == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    * Size = ( ( const struct karChiveFileDS * ) DS ) -> _data_size;

    return 0;
}   /* _karChiveFileDS_on_size () */

static
rc_t CC
_karChiveFileDSMake (
                    struct karChiveDS ** DS,
                    size_t StructSize,
                    const struct KFile * File,
                    uint64_t DataOffset,
                    uint64_t DataSize
)
{
    rc_t RCt;
    struct karChiveFileDS * FDS;

    RCt = 0;
    FDS = NULL;

    if ( DS != NULL ) {
        * DS = NULL;
    }

    if ( DS == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( File == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( DataSize == 0 ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    if ( StructSize < sizeof ( struct karChiveFileDS ) ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

        /*  NOTE : we do not check boundaries, because it is caller
         *         privelegy. And, some files we will receive could be
         *         fake ones
         */

    RCt = _karChiveDSMake (
                        ( struct karChiveDS ** ) & FDS,
                        StructSize,
                        _karChiveFileDS_on_dispose,
                        _karChiveFileDS_on_read,
                        _karChiveFileDS_on_size
                        );
    if ( RCt == 0 ) {
        RCt = KFileAddRef ( File );
        if ( RCt == 0 ) {
            FDS -> _file = File;
            FDS -> _data_offset = DataOffset;
            FDS -> _data_size = DataSize;

            * DS = ( struct karChiveDS * ) FDS;
        }
    }

    return RCt;
}   /* _karChiveFileDSMake () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Base for both MMap and Memory data sources
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t
_karChiveRamFileDSMake (
                    struct karChiveDS ** DS,
                    size_t StructSize,
                    char * Buffer,
                    size_t BufferSize
)
{
    rc_t RCt;
    const struct KFile * RamFile;

    RCt = 0;
    RamFile = NULL;

    if ( DS != NULL ) {
        * DS = NULL;
    }

    if ( DS == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( Buffer == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( BufferSize == 0 ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    if ( StructSize < sizeof ( struct karChiveFileDS ) ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    RCt = KRamFileMakeRead ( & RamFile, Buffer, BufferSize );
    if ( RCt == 0 ) {
        RCt = _karChiveFileDSMake (
                                    DS,
                                    StructSize,
                                    RamFile,
                                    0,
                                    BufferSize
                                    );

        KFileRelease ( RamFile );
    }

    return RCt;
}   /* _karChiveRamFileDSMake () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Standard KMMap source : MMap is added ref, so it will never die
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveMMapDS {
    struct karChiveFileDS _ds;

    const struct KMMap * _map;
};


static
rc_t CC
_karChiveMMapDS_on_dispose ( const struct karChiveDS * DS )
{
    struct karChiveMMapDS * MDS = ( struct karChiveMMapDS * ) DS;
    if ( MDS != NULL ) {
        if ( MDS -> _map != NULL ) {
            KMMapRelease ( MDS -> _map );
            MDS -> _map = NULL;
        }
    }
    return _karChiveFileDS_on_dispose ( DS );
}   /* _karChiveMMapDS_on_dispose () */

rc_t CC
karChiveMMapDSMake (
                    const struct karChiveDS ** DS,
                    const struct KMMap * Map,
                    uint64_t Offset,
                    uint64_t Size
)
{
    rc_t RCt;
    struct karChiveMMapDS * Ret;
    const void * MapAddr;
    size_t MapSize;

    RCt = 0;
    Ret = NULL;
    MapAddr = NULL;
    MapSize = 0;

    if ( DS != NULL ) {
        * DS = NULL;
    }

    if ( DS == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( Map == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

/*  JOJOBA - Dont know if we need it here
    if ( Size == 0 ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }
*/

    RCt = KMMapAddRef ( Map );
    if ( RCt == 0 ) {
        RCt = KMMapSize ( Map, & MapSize );
        if ( RCt == 0 ) {
            RCt = KMMapAddrRead ( Map, & MapAddr );
            if ( RCt == 0 ) {
                if ( MapSize < Offset + Size ) {
                    RCt = RC ( rcApp, rcData, rcInflating, rcFormat, rcCorrupt );
                }
                else {
                    RCt = _karChiveRamFileDSMake (
                                    ( struct karChiveDS ** ) & Ret,
                                    sizeof ( struct karChiveMMapDS ),
                                    ( ( char * ) MapAddr ) + Offset,
                                    Size
                                    );
                    if ( RCt == 0 ) {
                        Ret -> _map = Map;
                        ( ( struct karChiveDS * ) Ret ) -> _on_dispose
                                        = _karChiveMMapDS_on_dispose;
                        * DS = ( struct karChiveDS * ) Ret;
                    }
                    else {
                        KMMapRelease ( Map );
                    }
                }
            }
        }
    }

    if ( RCt != 0 ) {
        * DS = NULL;

        if ( Ret != NULL ) {
            karChiveDSRelease ( ( const struct karChiveDS * ) Ret );
        }
    }

    return RCt;
}   /* karChiveMMapDSMake () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Standard Memory source : alloc buffer and copyes data into it
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

struct karChiveMemDS {
    struct karChiveFileDS _ds;

    const void * _buf;
    size_t _buf_size;
};


static
rc_t CC
_karChiveMemDS_on_dispose ( const struct karChiveDS * DS )
{
    struct karChiveMemDS * MDS = ( struct karChiveMemDS * ) DS;
    if ( MDS != NULL ) {
        if ( MDS -> _buf != NULL ) {
            free ( ( void * ) MDS -> _buf );
        }

        MDS -> _buf = NULL;
        MDS -> _buf_size = 0;
    }

    return _karChiveFileDS_on_dispose ( DS );
}   /* _karChiveMMapDS_on_dispose () */

/*  That one will create data source from memory buffer
 *  NOTE, that one creates copy of memory buffer, and keeps it
 */
rc_t CC
karChiveMemDSMake (
                const struct karChiveDS ** DS,
                const void * Buf,
                uint64_t Size
)
{
    rc_t RCt;
    struct karChiveMemDS * Ret;
    char * NewBuf;

    RCt = 0;
    Ret = NULL;
    NewBuf = NULL;

    if ( DS != NULL ) {
        * DS = NULL;
    }

    if ( DS == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( Buf == NULL ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcNull );
    }

    if ( Size == 0 ) {
        return RC ( rcApp, rcData, rcAllocating, rcParam, rcInvalid );
    }

    NewBuf = calloc ( Size, sizeof ( char ) );
    if ( NewBuf == NULL ) {
        RCt = RC ( rcApp, rcData, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        memmove ( NewBuf, Buf, sizeof ( char ) * Size );
        RCt = _karChiveRamFileDSMake (
                                    ( struct karChiveDS ** ) & Ret,
                                    sizeof ( struct karChiveMemDS ),
                                    NewBuf,
                                    Size
                                    );
        if ( RCt == 0 ) {
            Ret -> _buf = NewBuf;
            Ret -> _buf_size = Size;
            ( ( struct karChiveDS * ) Ret ) -> _on_dispose
                                        = _karChiveMemDS_on_dispose;
            * DS = ( struct karChiveDS * ) Ret;
        }
        else {
            free ( NewBuf );
            NewBuf = NULL;
        }
    }

    if ( RCt != 0 ) {
        * DS = NULL;

        if ( Ret != NULL ) {
            karChiveDSRelease ( ( const struct karChiveDS * ) Ret );
        }
    }

    return RCt;
}   /* karChiveMemDSMake () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Standard File source : Not sure if that will be used, but why not?
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
rc_t CC
karChiveFileDSMake (
                    const struct karChiveDS ** DS,
                    const struct KFile * File,
                    uint64_t Offset,
                    uint64_t Size
)
{
    return _karChiveFileDSMake (
                                ( struct karChiveDS ** ) DS,
                                sizeof ( struct karChiveFileDS ),
                                File,
                                Offset,
                                Size
                                );
}   /* karChiveFileDSMake () */
