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
#include <klib/container.h>
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
 *  That is tranformation file ... ugly, but works
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct scmDepotTrn {
    struct BSTNode _node;

    const char * _name;
    const char * _ver_old;
    const char * _ver_new;
};

static
rc_t CC
_scmDepotTrnDispose ( struct scmDepotTrn * self )
{
    if ( self != NULL ) {
        if ( self -> _name != NULL ) {
            free ( ( char * ) self -> _name );
            self -> _name = NULL;
        }
        if ( self -> _ver_old != NULL ) {
            free ( ( char * ) self -> _ver_old );
            self -> _ver_old = NULL;
        }
        if ( self -> _ver_new != NULL ) {
            free ( ( char * ) self -> _ver_new );
            self -> _ver_new = NULL;
        }

        free ( self );
    }

    return 0;
}   /* _scmDepotTrnDispose () */

/*  NOTE : VERY DANGEROUS METHOD
 */
static
const char * CC
_getSome ( const char * Line, const char ** Got )
{
    /*  NOTE : VERY DANGEROUS METHOD
     */
    const char * lB;
    const char * lC;

    lB = lC = Line;
    * Got = NULL;

        /*  Note, we suppose, line will come with stripped character
         *  return, or other bad stuff
         */
    while ( * lC != 0 && * lC != '\t') lC ++;

    if ( lC - lB < 1 ) { return NULL; }

    if ( copySStringSayNothing ( Got, lB, lC ) != 0 ) {
        return NULL;
    }

    return ( * lC == 0 ) ? NULL : ( lC ++ );
}   /* _getSome () */

/* Splitting line to Name, OldVer, and NewVer
 * The format of string could be one of three :
 *   Name
 *   Name<tab>NewVer
 *   Name<tab>OldVer<tab>NewVer
 */
static
rc_t CC
_scmDepotTrnSplit3 (
                    const char * Line,
                    const char ** Name,
                    const char ** OldVer,
                    const char ** NewVer
)
{
    const char * lB;
    const char * tVar;

    lB = NULL;
    tVar = NULL;

        /* no any NULL checks */
    * Name = NULL;
    * OldVer = NULL;
    * NewVer = NULL;

    lB = _getSome ( Line, Name );
    if ( * Name == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcParam, rcInvalid );
    }

    if ( lB == NULL ) {
            /*  Just notification that we should use latest version for
             *  declaration
             */
    }
    else {
        lB = _getSome ( lB, & tVar );
        if ( tVar == NULL ) {
            return RC ( rcApp, rcSchema, rcConstructing, rcParam, rcInvalid );
        }

        if ( lB == NULL ) {
                /*  Then Var contains NewVersion
                 */
            * OldVer = NULL;
            * NewVer = tVar;
        }
        else {
                /*  Then tVar contains OldVersion and should read
                 *  New Version
                 */
            * OldVer = tVar;
            lB = _getSome ( lB, NewVer );
            if ( * NewVer == NULL || lB != NULL ) {
                return RC ( rcApp, rcSchema, rcConstructing, rcParam, rcInvalid );
            }
        }
    }

    return 0;
}   /* _scmDepotTrnSplit3 () */

static
rc_t CC
_scmDepotTrnMake (
                    struct scmDepotTrn ** Node,
                    const char * Line
)
{
    rc_t RCt;
    struct scmDepotTrn * Ret;

    RCt = 0;
    Node = NULL;

    if ( Node != NULL ) {
        * Node = NULL;
    }

    if ( Node == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcParam, rcNull );
    }

    if ( Line == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcParam, rcNull );
    }

    Ret = calloc ( 1, sizeof ( struct scmDepotTrn ) );
    if ( Ret == NULL ) {
        RCt = RC ( rcApp, rcSchema, rcConstructing, rcMemory, rcExhausted );
    }
    else {
        RCt = _scmDepotTrnSplit3 (
                                Line, 
                                & ( Ret -> _name ),
                                & ( Ret -> _ver_old ),
                                & ( Ret -> _ver_new )
                                );

        if ( RCt == 0 ) {
            * Node = Ret;
        }
    }

    if ( RCt != 0 ) {
        * Node = NULL;

        if ( Ret != NULL ) {
            _scmDepotTrnDispose ( Ret );
        }
    }

    return RCt;
}   /*  _scmDepotTrnMake () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *  Schema Depot - the place where schemas do live :LOOL:
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
struct scmDepot {
    struct BSTree _trans;

    struct VSchema * _scm;

    const char * _scm_path;
    const char * _trn_path;

    struct VDBManager * _mgr;
};

static
void CC
_scmDepotTranslationsWhackCallback ( BSTNode * Node, void * Data )
{
    if ( Node != NULL ) {
        _scmDepotTrnDispose ( ( struct scmDepotTrn * ) Node );
    }
}   /* _scmDepotTranslationsWhackCallback () */

rc_t CC
scmDepotDispose ( struct scmDepot * self )
{
    if ( self != NULL ) {
        if ( self -> _scm_path != NULL ) {
            free ( ( char * ) self -> _scm_path );
            self -> _scm_path = NULL;
        }

        if ( self -> _trn_path != NULL ) {
            free ( ( char * ) self -> _trn_path );
            self -> _trn_path = NULL;
        }

        if ( self -> _mgr != NULL ) {
            VDBManagerRelease ( self -> _mgr );
        }

        if ( self -> _scm != NULL ) {
            VSchemaRelease ( self -> _scm );
            self -> _mgr = NULL;
        }

        BSTreeWhack (
                    & ( self -> _trans ),
                    _scmDepotTranslationsWhackCallback,
                    NULL
                    );

        free ( self );
    }
    return 0;
}   /* scmDepotDispose () */

static
rc_t CC
_karDepotSchemaPath ( const char ** Resolved, const char * SchemaPath )
{
    rc_t RCt;
    struct KConfig * Kfg;
    const struct KConfigNode * Node;
    char Buf [ 1024 ];
    size_t NumRead;
    size_t Remaining;

    RCt = 0;
    Kfg = NULL;
    Node = NULL;
    * Buf = 0;
    NumRead = 0;
    Remaining = 0;

    if ( SchemaPath == NULL ) {
        RCt = KConfigMake ( & Kfg, NULL );
        if ( RCt == 0 ) {
            RCt = KConfigOpenNodeRead (
                                        Kfg,
                                        & Node,
                                        "%s%s",
                                        KFG_DELITE_SECTION,
                                        KFG_DELITE_SCHEMA
                                        );
            if ( RCt == 0 ) {
                RCt = KConfigNodeRead (
                                        Node,
                                        0,
                                        Buf,
                                        sizeof ( Buf ),
                                        & NumRead,
                                        & Remaining
                                        );
                if ( RCt == 0 ) {
                    if ( Remaining != 0 ) {
                        return RC ( rcApp, rcSchema, rcConstructing, rcPath, rcExcessive );
                    }
                    else {
                        RCt = copyLStringSayNothing (
                                                    Resolved,
                                                    Buf,
                                                    NumRead
                                                    );
                    }
                }
                KConfigNodeRelease ( Node );
            }
            KConfigRelease ( Kfg );
        }
    }
    else {
        RCt = copyStringSayNothingRelax ( Resolved, SchemaPath );
    }

    return RCt;
}   /* _karDepotSchemaPath () */

struct StructOne {
    struct scmDepot * depot;
    rc_t rc;
};

static
rc_t CC
_scmDepotLoadSchemasCallback (
                                const struct KDirectory_v1 * Dir,
                                uint32_t Type,
                                const char * Name,
                                void * Data
)
{
    rc_t RCt;
    static const char * Pat = ".vschema";
    size_t l1, l2;
    struct StructOne * One;
    char Buf [ 4096 ];

    RCt = 0;
    l1 = l2 = 0;
    One = ( struct StructOne * ) Data;
    * Buf = 0;

    if ( Type == kptFile ) {
            /*  First we should be sure if that file is schema file
             *  or, in in other words it's name ends with ".vschema"
             */
        l1 = strlen ( Name );
        l2 = strlen ( Pat );
        if ( l2 < l1 ) {
            if ( strcmp ( Name + l1 - l2, Pat ) == 0 ) {
                RCt = KDirectoryResolvePath (
                                            Dir,
                                            true, /* can not do false
                                                   * why? JOJOBA!!!
                                                   */
                                            Buf,
                                            sizeof ( Buf ),
                                            "%s",
                                            Name
                                            );
                if ( RCt == 0 ) {
                    One -> rc = VSchemaParseFile ( 
                                                One -> depot -> _scm,
                                                "%s",
                                                Buf
                                                );
                }
            }
        }
    }

    return RCt;
}   /* _scmDepotLoadSchemasCallback () */

static
rc_t CC
_scmDepotLoadSchemas ( struct scmDepot * self, const char * SchemaPath )
{
    rc_t RCt;
    struct KDirectory * Dir;
    struct StructOne One;

    RCt = 0;
    Dir = NULL;
    One . depot = self;
    One . rc = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcSelf, rcNull );
    }


    RCt = _karDepotSchemaPath ( & ( self -> _scm_path ), SchemaPath );
    if ( RCt == 0 ) {
        RCt = VDBManagerMakeSchema ( self -> _mgr, & ( self -> _scm ) );
        if ( RCt == 0 ) {
VSchemaAddIncludePath ( self -> _scm, SchemaPath );

            RCt = KDirectoryNativeDir ( & Dir );
            if ( RCt == 0 ) {

                RCt = KDirectoryVisit (
                                Dir,
                                true,
                                _scmDepotLoadSchemasCallback,
                                & One,
                                self -> _scm_path
                                );

                KDirectoryRelease ( Dir );
            }

            if ( One . rc != 0 ) {
                RCt = One . rc;
            }

        }
    }

    return RCt;
}   /* _scmDepotLoadSchemas () */

static
int64_t CC
_scmDepotLoadTransformsProcessCallback (
                                    const struct BSTNode * L,
                                    const struct BSTNode * R
)
{
    /*  JOJOBA !!!! implement different comparision
     */
    return strcmp (
                    ( ( struct scmDepotTrn * ) L ) -> _name,
                    ( ( struct scmDepotTrn * ) R ) -> _name
                    );
}   /* _scmDepotLoadTransformsProcessCallback () */

static
rc_t CC
_scmDepotLoadTransforms (
                        struct scmDepot * self,
                        const char * TransFile
)
{
    rc_t RCt;
    const struct karLnRd * LnRd;
    const char * Line;
    struct scmDepotTrn * Trn;

    RCt = 0;
    LnRd = NULL;
    LnRd = NULL;
    Line = NULL;
    Trn = NULL;

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcSelf, rcNull );
    }

    if ( TransFile == NULL ) {
            /*  Does not error, just transform everything
             */
        return 0;
    }

    RCt = copyStringSayNothingRelax ( & self -> _trn_path, TransFile );
    if ( RCt == 0 ) {
            /*  Open File and read line by line
             */
        RCt = karLnRdOpen ( & LnRd, TransFile );
        if ( RCt == 0 ) {
            while ( karLnRdNext ( LnRd ) ) {
                RCt = karLnRdGet ( LnRd, & Line );
                if ( RCt == 0 ) {
                    RCt = _scmDepotTrnMake ( & Trn, Line );
                    if ( RCt == 0 ) {
                        RCt = BSTreeInsert (
                                & ( self -> _trans ),
                                & ( Trn -> _node ),
                                _scmDepotLoadTransformsProcessCallback
                                );
                    }

                    free ( ( char * ) Line );
                }

                if ( RCt != 0 ) {
                    break;
                }
            }
            karLnRdDispose ( LnRd );
        }
    }

    return RCt;
}   /* _scmDepotLoadTransforms () */

rc_t CC
scmDepotMake (
            struct scmDepot ** Depot, 
            const char * SchemaPath,
            const char * TransformFile
)
{
    rc_t RCt;
    struct scmDepot * Ret;

    RCt = 0;
    Ret = NULL;

    if ( Depot != NULL ) {
        * Depot = NULL;
    }

    if ( Depot == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcParam, rcNull );
    }

        /*  There are two cases :
         *   1) Schema Path == NULL in that case we loading from config
         *   2) Transform File == NULL - doing nothing
         */
    Ret = calloc ( 1, sizeof ( struct scmDepot ) );
    if ( Ret == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcMemory, rcExhausted );
    }
    else {
        BSTreeInit ( & ( Ret -> _trans ) );

        RCt = VDBManagerMakeUpdate ( & ( Ret -> _mgr ), NULL );
            /*  Reading FilePath
             */
        if ( RCt == 0 ) {
            RCt = _scmDepotLoadSchemas ( Ret, SchemaPath );
            if ( RCt == 0 ) {
                    /*  Here we are loading transformations
                     */
                RCt = _scmDepotLoadTransforms ( Ret, TransformFile );
                if ( RCt == 0 ) {
                    * Depot = Ret;
                }
            }
        }
    }

    if ( RCt != 0 ) {
        * Depot = NULL;

        if ( Ret != NULL ) {
            scmDepotDispose ( Ret );
        }
    }

    return RCt;
}   /* scmDepotMake () */

static
int64_t CC
_scmDepotTrnCompare ( const void * Item, const BSTNode * Node )
{
    return strcmp (
                ( const char * ) Item,
                ( ( struct scmDepotTrn * ) Node ) -> _name
                );
}   /* _scmDepotTrnComapre () */

static
rc_t CC
_scmDepotDumpCallback ( void * Dst, const void * Data, size_t DSize )
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
}   /* _scmDepotDumpCallback () */

void
_tempExtractName ( const char * Name, const char * Dump, char * Buf, size_t BS )
{
    char * Buu = strstr ( Dump, Name );
    if ( Buu != 0 ) {
        char * Poo = Buu;
        while ( * Poo != '=' ) {
            Poo ++;
        }

        strncpy ( Buf, Buu, Poo - Buu );
        Buf [ Poo - Buu ] = 0;
    }
}   /* _tempExtractName () */

static
rc_t CC
_scmDepotResolveAndDump (
                    struct scmDepot * self,
                    struct KMDataNode * DataNode,
                    struct karCBuf * CBuf,
                    char * Name,
                    size_t NameSize
)
{
    rc_t RCt;
    size_t BSize;
    char * Ver;
    struct scmDepotTrn * Trn;
    char Buf [ 256 ];
    bool NeedTransform;

    RCt = 0;
    BSize = 0;
    Ver = NULL;
    Trn = NULL;
    NeedTransform = false;

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcNull );
    }

    if ( DataNode == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( CBuf == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( Name == NULL || NameSize == 0 ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcInvalid );
    }

    * Name = 0;

    RCt = KMDataNodeReadAttr (
                            DataNode,
                            NAME_ATTR_NAME,
                            Buf,
                            sizeof ( Buf ),
                            & BSize
                         );
printf ( "[JI] [%d] [%d] [%s]\n", __LINE__, RCt, Buf );
    if ( RCt == 0 ) {
        Buf [ BSize ] = 0;

        Ver = strchr ( Buf, '#' );
        if ( Ver != NULL ) {
            * Ver = 0;
            Ver ++;
        }

            /* The logic: if Transformation File was provided
             * we try to resolve new version from file, if version
             * exists, we will use it.
             * If Transformation file was not provided, we will try
             * to upgrade version to newest one
             */
        if ( self -> _trn_path != NULL ) {
            Trn = ( struct scmDepotTrn * ) BSTreeFind (
                                                & ( self -> _trans ),
                                                Buf,
                                                _scmDepotTrnCompare
                                                );
            if ( Trn == NULL ) {
                    /* Nothing to transform */
                * Buf = 0;
                NeedTransform = false;
            }
            else {
                if ( Trn -> _ver_old == NULL ) {
                    NeedTransform = true;
                }
                else {
                    if ( Ver != NULL ) {
                        if ( strcmp ( Ver, Trn -> _ver_old ) == 0 ) {
                            NeedTransform = true;
                        }
                        else {
                            * Buf = 0;
                            NeedTransform = false;
                        }
                    }
                }
            }
            if ( NeedTransform ) {
                if ( Trn -> _ver_new != NULL ) {
                    strcat ( Buf, "#" );
                    strcat ( Buf, Trn -> _ver_new );
                }
            }
        }
        else {
            NeedTransform = true;
        }
    }

    if ( NeedTransform ) {
        if ( * Buf == 0 ) {
            return RC ( rcApp, rcSchema, rcUpdating, rcName, rcInvalid );
        }
        else {
            RCt = VSchemaDump (
                            self -> _scm,
                            sdmCompact,
                            Buf,
                            _scmDepotDumpCallback,
                            ( void * ) CBuf
                            );
printf ( "[JI] [%d] [%d] [%s]\n", __LINE__, RCt, Buf );
printf ( "[JI] [%d] [%d] [%s]\n", __LINE__, CBuf -> _s, Buf );
{
if ( CBuf -> _s != 0 ) {
char B [ 128 ];
strncpy ( B, CBuf -> _b, 64 );
printf ( "[JI] [%d] [%s]\n", __LINE__, B );
}
else {
printf ( "[JI] [%d] [%s]\n", __LINE__, "JOPA" );
}
}
            if ( RCt == 0 ) {
                    /* JOJOBA here we are harvesting for a name */
                _tempExtractName ( Buf, CBuf -> _b, Name, NameSize );
printf ( "[JI] [%d] [%d] [%s]\n", __LINE__, RCt, Name );
            }
        }
    }

    return RCt;
}   /* _scmDepotResolveAndDump () */

rc_t CC
_scmDepotModifyNode (
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
}   /* _scmDepotModifyNode () */

rc_t CC
scmDepotTransform ( struct scmDepot * self, struct KMetadata * Meta )
{
    rc_t RCt;
    struct KMDataNode * DataNode;
    struct karCBuf Buf;
    char Name [ 256 ];

    RCt = 0;
    DataNode = NULL;

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
            RCt = _scmDepotResolveAndDump (
                                        self,
                                        DataNode,
                                        & Buf,
                                        Name,
                                        sizeof ( Name )
                                        );
            if ( RCt == 0 ) {
                if ( RCt == 0 ) {
                    RCt = _scmDepotModifyNode ( DataNode, Name, & Buf );
                }
            }

            KMDataNodeRelease ( DataNode );
        }
        karCBufWhack ( & Buf );
    } 
    return RCt;
}   /* scmDepotTransform () */

