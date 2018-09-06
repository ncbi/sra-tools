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
#include <klib/out.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfg/config.h>
#include <kdb/meta.h>
#include <vdb/manager.h>
#include <vdb/schema.h>

#include "delite.h"
#include "delite_s.h"

#include <sysalloc.h>

#include <byteswap.h>
#include <stdio.h>
#include <string.h>

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Unusuals and stolen stuff
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
#define NAME_ATTR_NAME   "name"
#define SCHEMA_ATTR_NAME "schema"

#define KFG_DELITE_SECTION  "/DELITE/"
#define KFG_DELITE_NAME     "name"
#define KFG_DELITE_SCHEMA   "schema"

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Resolving part of the deal
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t CC
_karChiveResolverDispose ( struct karChiveResolver * self )
{
    if ( self != NULL ) {
        if ( self -> _dispose != NULL ) {
            self -> _dispose ( self );
        }
        else {
            free ( self );
        }
    }

    return 0;
}   /* _karChiveResolverDispose () */

static
rc_t CC
_karChiveResolverResolve (
                            struct karChiveResolver * self,
                            const char * Name,
                            char ** SchemaPath,
                            char ** SchemaName
)
{
    if ( * SchemaPath != NULL ) {
        * SchemaPath = NULL;
    }

    if ( * SchemaName != NULL ) {
        * SchemaName = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcSelf, rcNull );
    }

    if ( Name == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( SchemaName == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( SchemaPath == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( self -> _resolve == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcSelf, rcInvalid );
    }

    return self -> _resolve ( self, Name, SchemaPath, SchemaName );
}   /* _karChiveResolverResolve () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Standard resolver through Konfig
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t CC
_karCRStandardDispose ( struct karChiveResolver * self )
{
    struct KConfig * Konfig = NULL;

    if ( self != NULL ) {
        Konfig = ( struct KConfig * ) self -> _data;
        if ( Konfig != NULL ) {
            KConfigRelease ( Konfig );
        }
        self -> _data = NULL;

        free ( self );
    }

    return 0;
}   /* _karCRStandardDispose () */

static
rc_t CC
_karCRStandardNormalizeKey ( char ** Key, const char * Name )
{
    char B [ 64 ];
    char * Beg, * End;

    * B = 0;
    Beg = End = NULL;

    if ( Key != NULL ) {
        * Key = NULL;
    }

    if ( Key == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( Name == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

        /*  ho-ho-ho ... dangerous
         */
    strcpy ( B, Name );
    Beg = B;
    End = B + strlen ( B );

    while ( Beg < End ) {
        if ( * Beg == ':' ) {
            * Beg = '_';
        }

        if ( * Beg == '#' ) {
            * Beg = 0;
            break;
        }

        Beg ++;
    }

    return copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                                ( const char ** ) Key,
                                                B
                                                );
}   /* _karCRStandardNormalizeKey () */

static 
rc_t CC
_karCRStandardResolveValue (
                        struct karChiveResolver * Resolver,
                        const char * NormalizedKey,
                        const char * ValueName,
                        char ** Value
)
{
    rc_t RCt;
    const struct KConfigNode * Node;
    char Buf [ 1024 ];
    size_t NumRead, Remain, ToRead;
    char * RetVal;

    RCt = 0;
    Node = NULL;
    * Buf = 0;
    NumRead = Remain = ToRead = 0;
    RetVal = NULL;

        /*  We do suppose that there are all arguments are good, so 
         *  no regular checks
         */
    sprintf (
            Buf,
            "%s%s/%s",
            KFG_DELITE_SECTION,
            NormalizedKey,
            ValueName
            );

    RCt = KConfigOpenNodeRead (
                            ( struct KConfig * ) ( Resolver -> _data ),
                            & Node,
                            "%s",
                            Buf
                            );
    if ( RCt == 0 ) {
            /*  Size of buffer to Read
             */
        RCt = KConfigNodeRead ( Node, 0, NULL, 0, & NumRead, & Remain );
        if ( RCt == 0 ) {
            RetVal = calloc ( Remain + 1, sizeof ( char ) );
            if ( RetVal == NULL ) {
                RCt = RC ( rcApp, rcSchema, rcResolving, rcMemory, rcExhausted );
            }
            else {
                ToRead = Remain;
                RCt = KConfigNodeRead (
                                        Node,
                                        0,
                                        RetVal,
                                        ToRead,
                                        & NumRead,
                                        & Remain
                                        );
                if ( RCt == 0 ) {
                    RetVal [ NumRead ] = 0; /* no need to do that */
                    * Value = RetVal;
                }
            }
        }

        KConfigNodeRelease ( Node );
    }

    return RCt;
}   /* _karCRStandardResolveValue () */

/*  Resolving process:
 *
 *  1) Stripping revision number from 'Name', for example
 *     NCBI:SRA:_454_:tbl:v2#1.0.8 -> NCBI:SRA:_454_:tbl:v2
 *
 *  2) Substituting all ':' with '_' in 'Name', for example
 *     NCBI:SRA:_454_:tbl:v2 -> NCBI_SRA__454__tbl_v2
 *
 *  3) Creating config node path PATH = '/DELITE/Name', for example
 *       /DELITE/NCBI_SRA__454__tbl_v2
 *
 *  4) New schema name will be stored in PATH/name, and new schema
 *     path will be stored in PATH/schema
 *
 */
static
rc_t CC
_karCRStandardResolve (
                        struct karChiveResolver * Resolver,
                        const char * Name,
                        char ** SchemaPath,
                        char ** SchemaName
)
{
    rc_t RCt;
    char * Key;
    char Buf [ 1024 ];

    RCt = 0;
    Key = NULL;
    * Buf = 0;

    if ( SchemaPath != NULL ) {
        * SchemaPath = NULL;
    }

    if ( SchemaName != NULL ) {
        * SchemaName = NULL;
    }

    if ( Resolver == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( Name == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( SchemaName == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( SchemaPath == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    RCt = _karCRStandardNormalizeKey ( & Key, Name );
    if ( RCt == 0 ) {
        RCt = _karCRStandardResolveValue (
                                        Resolver,
                                        Key,
                                        KFG_DELITE_NAME,
                                        SchemaName
                                        );
        if ( RCt == 0 ) {
            RCt = _karCRStandardResolveValue (
                                            Resolver,
                                            Key,
                                            KFG_DELITE_SCHEMA,
                                            SchemaPath
                                            );
            if ( RCt == 0 ) {
                KOutMsg ( "MAP [%s] -> [%s][%s]\n",
                        Name,
                        * SchemaName,
                        * SchemaPath
                        );

                pLogMsg (
                        klogInfo,
                        "MAP [$(name)] -> [$(sname)][$(spath)]",
                        "name=%s,sname=%s,spath=%s",
                        Name,
                        * SchemaName,
                        * SchemaPath
                        );
            }
            else {
                pLogErr (
                        klogErr,
                        RCt,
                        "Can not map schema and path for [$(name)]",
                        "name=%s",
                        Name
                        );
            }
        }
        else {
            pLogErr (
                    klogErr,
                    RCt,
                    "Can not map schema and path for [$(name)]",
                    "name=%s",
                    Name
                    );
        }

        free ( Key );
    }

    if ( RCt != 0 ) {
        if ( * SchemaPath != NULL ) {
            free ( * SchemaPath );
            * SchemaPath = NULL;
        }
        if ( * SchemaName != NULL ) {
            free ( * SchemaName );
            * SchemaPath = NULL;
        }
    }

    return RCt;
}   /* _karCRStandardResolve () */

static
rc_t
_karCRStandardMakeKonfig ( struct KConfig ** Ret, const char * Path )
{
    rc_t RCt;
    struct KConfig * Konfig;
    const struct KFile * File;
    struct KDirectory * NatDir;

    RCt = 0;
    Konfig = NULL;
    File = NULL;
    NatDir = NULL;

    if ( Ret != NULL ) {
        * Ret = NULL;
    }

    if ( Ret == NULL ) {
        return RC ( rcApp, rcSchema, rcAllocating, rcParam, rcNull );
    }

    RCt = KConfigMake ( & Konfig, NULL );
    if ( RCt == 0 ) {
            /*  Here we are loading  config
             */
        if ( Path != NULL ) {
            RCt = KDirectoryNativeDir ( & NatDir );
            if ( RCt == 0 ) {
                RCt = KDirectoryOpenFileRead ( NatDir, & File, Path );
                if ( RCt == 0 ) {
                    RCt = KConfigLoadFile ( Konfig, NULL, File );

                    KFileRelease ( File );
                }

                KDirectoryRelease ( NatDir );
            }
        }
    }

    return RCt;
}   /* _karCRStandardMakeKonfig () */

static
rc_t CC
_karCRStandardMake (
                    struct karChiveResolver ** Ret,
                    const char * KonfigPath
)
{
    rc_t RCt;
    struct karChiveResolver * Rsl;
    struct KConfig * Konfig;

    RCt = 0;
    Rsl = NULL;
    Konfig = NULL;

    if ( Ret != NULL ) {
        * Ret = NULL;
    }

    if ( Ret == NULL ) {
        return RC ( rcApp, rcSchema, rcAllocating, rcParam, rcNull );
    }

    Rsl = calloc ( 1, sizeof ( struct karChiveResolver ) );
    if ( Rsl == NULL ) {
        RCt = RC ( rcApp, rcSchema, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        RCt = _karCRStandardMakeKonfig ( & Konfig, KonfigPath );
        RCt = KConfigMake ( & Konfig, NULL );
        if ( RCt == 0 ) {
            Rsl -> _data = ( void * ) Konfig;
            Rsl -> _dispose = _karCRStandardDispose;
            Rsl -> _resolve = _karCRStandardResolve;

            * Ret = Rsl;
        }
    }

    return RCt;
}   /* _karCRStandardMake () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Schema level structure
 *  No refcounts ... don't know why yet :LOL:
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct karChiveScm {
    const char * _konfig_path;
    struct karChiveResolver * _resolver;

    struct VDBManager * _mgr;
};

rc_t CC
karChiveScmDispose ( struct karChiveScm * self )
{
    if ( self != NULL ) {
        if ( self -> _mgr != NULL ) {
            VDBManagerRelease ( self -> _mgr );
        }
        self -> _mgr = NULL;

        if ( self -> _resolver != NULL ) {
            _karChiveResolverDispose ( self -> _resolver );
        }
        self -> _resolver = NULL;

        if ( self -> _konfig_path != NULL ) {
            free ( ( char * ) self -> _konfig_path );
        }
        self -> _konfig_path = NULL;

        free ( self );
    }

    return 0;
}   /* karChiveScmDispose () */

rc_t CC
karChiveScmMake ( struct karChiveScm ** Scm )
{
    rc_t RCt;
    struct karChiveScm * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Scm != NULL ) {
        * Scm = 0;
    }

    if ( Scm == NULL ) {
        return RC ( rcApp, rcSchema, rcAllocating, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( struct karChiveScm  ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcSchema, rcAllocating, rcMemory, rcExhausted );
    }
    else {
        RCt = VDBManagerMakeUpdate ( & ( Ret -> _mgr ), NULL );
        if ( RCt == 0 ) {
            Ret -> _konfig_path = NULL;
            Ret -> _resolver = NULL;

            * Scm = Ret;
        }
    }

    return RCt;
}   /* karChiveScmMake () */

static
rc_t CC
_karChiveScmGetResolver (
                        struct karChiveScm * self,
                        struct karChiveResolver ** Resolver
)
{
    rc_t RCt;

    RCt = 0;

    if ( Resolver != NULL ) {
        * Resolver = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcSelf, rcNull );
    }

    if ( Resolver == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( self -> _resolver == NULL ) {
        RCt = _karCRStandardMake (
                                & ( self -> _resolver ),
                                self -> _konfig_path
                                );
    }

    if ( RCt == 0 ) {
        * Resolver = self -> _resolver;
    }

    return RCt;
}   /* _karChiveScmGetResolver () */

rc_t CC
karChiveScmSetStandardResolver (
                                struct karChiveScm * self,
                                const char * KfgPath
)
{
    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcSelf, rcNull );
    }

    if ( KfgPath == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( self -> _resolver != NULL || self -> _konfig_path != NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcItem, rcExists );
    }

    if ( KfgPath != NULL ) {
        return copyStringSayNothingHopeKurtWillNeverSeeThatCode (
                                                & ( self -> _konfig_path ),
                                                KfgPath
                                                );
    }
    return 0;
}   /* karChiveScmSetStandardResolver () */

rc_t CC
ksrChiveScmSetResolver (
                        struct karChiveScm * self,
                        struct karChiveResolver * Rsl
)
{
    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcSelf, rcNull );
    }

    if ( Rsl == NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcParam, rcNull );
    }

    if ( self -> _resolver != NULL || self -> _konfig_path != NULL ) {
        return RC ( rcApp, rcSchema, rcResolving, rcItem, rcExists );
    }

    self -> _resolver = Rsl;

    return 0;
}   /* ksrChiveScmSetResolver () */

rc_t CC
DummpSchemaCallback ( void * Dst, const void * Data, size_t DSize )
{
    if ( Dst == 0 ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( Data == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( DSize == 0 ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    return karCBufAppend (
                            ( struct karCBuf * ) Dst,
                            ( void * ) Data,
                            DSize
                            );
}   /* DummpSchemaCallback () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  First we do find a node with name "schema" and rock-n-rooooooo
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t CC
_karChiveScmTransformGetName (
                            struct karChiveScm * self,
                            char ** SchemaPath,
                            char ** SchemaName,
                            struct KMDataNode * DataNode

)
{
    rc_t RCt;
    char Buf [ 1024 ];
    size_t BSize;
    struct karChiveResolver * Rsl;

    RCt = 0;
    * Buf = 0;
    BSize = 0;
    Rsl = NULL;

    if ( SchemaName != NULL ) {
        * SchemaName = NULL;
    }

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcNull );
    }

    if ( SchemaName == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( DataNode == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    RCt = _karChiveScmGetResolver ( self, & Rsl );
    if ( RCt == 0 ) {
        RCt = KMDataNodeReadAttr (
                                DataNode,
                                NAME_ATTR_NAME,
                                Buf,
                                sizeof ( Buf ),
                                & BSize
                             );
        if ( RCt == 0 ) {
            RCt = _karChiveResolverResolve (
                                            Rsl,
                                            Buf,
                                            SchemaPath,
                                            SchemaName
                                            );
        }
    }

    if ( RCt != 0 ) {
        if ( * SchemaName != NULL ) {
            free ( ( char * ) * SchemaName );
            * SchemaName = NULL;
        }
        if ( * SchemaPath != NULL ) {
            free ( ( char * ) * SchemaPath );
            * SchemaPath = NULL;
        }
    }

    return RCt;
}   /* _karChiveScmTransformGetName () */

static
rc_t CC
_karChiveScmTransformDumpSchema (
                                struct karChiveScm * self,
                                const char * SchemaPath,
                                struct karCBuf * RetData
)
{
    rc_t RCt;
    struct VSchema * Schema;

    RCt = 0;
    Schema = NULL;

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcNull );
    }

    if ( SchemaPath == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( RetData == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( self -> _mgr == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcInvalid );
    }

    RCt = VDBManagerMakeSchema ( self -> _mgr, & Schema );
    if ( RCt == 0 ) {

        RCt = VSchemaParseFile ( Schema, "%s", SchemaPath );
        if ( RCt == 0 ) {

            RCt = VSchemaDump (
                            Schema,
                            sdmCompact,
                            NULL,
                            DummpSchemaCallback,
                            ( void * ) RetData
                            );
        }

        VSchemaRelease ( Schema );
    }

    return RCt;
}   /* _karChiveScmTransformDumpSchema () */

rc_t CC
_karChiveScmTransformModifyNode (
                                struct KMDataNode * DataNode,
                                const char * SchemaName,
                                struct karCBuf * Buf
)
{
    rc_t RCt;
    const void * Data;
    size_t DSize;

    RCt = 0;
    Data = NULL;
    DSize = 0;

    if ( DataNode == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( SchemaName == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( Buf == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    RCt = karCBufGet ( Buf, & Data, & DSize );
    if ( RCt == 0 ) {
        if ( DSize == 0 || Data == NULL ) {
            return RC ( rcApp, rcSchema, rcUpdating, rcSize, rcInvalid );
        }
        else {
            RCt = KMDataNodeWrite ( DataNode, Data, DSize );
            if ( RCt == 0 ) {
                RCt = KMDataNodeWriteAttr (
                                            DataNode,
                                            NAME_ATTR_NAME,
                                            SchemaName
                                            );
            }
        }
    }
    return RCt;
}   /* _karChiveScmTransformModiryNode () */

rc_t CC
karChiveScmTransform (
                        struct karChiveScm * self,
                        struct KMetadata * Meta
)
{
    rc_t RCt;
    struct KMDataNode * DataNode;
    struct karCBuf Buf;
    char * SchemaName;
    char * SchemaPath;

    RCt = 0;
    DataNode = NULL;
    SchemaName = NULL;
    SchemaPath = NULL;

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcNull );
    }

    if ( Meta == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( self -> _mgr == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcInvalid );
    }

    RCt = karCBufInit ( & Buf, 0 );
    if ( RCt == 0 ) {
        RCt = KMetadataOpenNodeUpdate (
                                        Meta,
                                        & DataNode,
                                        SCHEMA_ATTR_NAME
                                        );
        if ( RCt == 0 ) {
            RCt = _karChiveScmTransformGetName (
                                                self,
                                                & SchemaPath,
                                                & SchemaName,
                                                DataNode
                                                );
            if ( RCt == 0 ) {
                RCt = _karChiveScmTransformDumpSchema (
                                                    self,
                                                    SchemaPath,
                                                    & Buf
                                                    );
                if ( RCt == 0 ) {
                    RCt = _karChiveScmTransformModifyNode (
                                                            DataNode,
                                                            SchemaName,
                                                            & Buf
                                                            );
                }

                free ( SchemaName );
                free ( SchemaPath );
            }
        }
        karCBufWhack ( & Buf );
    } 
    return RCt;
}   /* karChiveScmTransform () */
