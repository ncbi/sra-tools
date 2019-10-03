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
#include <ctype.h>

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

    const char * _name_old;
    const char * _name_new;
};

static
rc_t CC
_scmDepotTrnDispose ( struct scmDepotTrn * self )
{
    if ( self != NULL ) {
        if ( self -> _name_old != NULL ) {
            free ( ( char * ) self -> _name_old );
            self -> _name_old = NULL;
        }

        if ( self -> _name_new != NULL ) {
            free ( ( char * ) self -> _name_new );
            self -> _name_new = NULL;
        }

        free ( self );
    }

    return 0;
}   /* _scmDepotTrnDispose () */

static
bool CC
_scmDepotTrnEmptyLine ( const char * Line )
{
    if ( Line == NULL ) {
        return true;
    }

    while ( * Line != 0 ) {
        if ( ! isspace ( * Line ) ) {
            return * Line == '#';
        }

        Line ++;
    }

    return true;
}   /* _scmDepotTrnEmptyLine () */

static
const char * CC
_scmDepotTrnReadSome (
                        const char * Line,
                        char * Buf,
                        size_t BufSize  /* we don't check it ... */
)
{
    /*  NOTE : VERY DANGEROUS METHOD
     */
    const char * lB;
    const char * lC;

    lB = lC = Line;
    * Buf = 0;

        /*  Note, we suppose, line will come with stripped character
         *  return, or other bad stuff
         */
    while ( * lC != 0 && * lC != '\t' ) lC ++;

    if ( lC - lB < 1 ) { return NULL; }

    strncpy ( Buf, lB, lC - lB );
    Buf [ lC - lB ] = 0;

    while ( * lC != 0 && * lC == '\t' ) lC ++;

    return ( * lC == 0 ) ? NULL : lC;
}   /* _scmDepotTrnReadSome () */

static
rc_t CC
_scmDepotTrnReadLine (
                        const char * Line,
                        const char ** OldVer,
                        const char ** NewVer
)
{
    rc_t RCt;
    char OV [ 128 ];
    char NV [ 128 ];
    const char * pB;

    RCt = 0;
    pB = NULL;

    * OldVer = NULL;
    * NewVer = NULL;

    /*  We don't do parameter checks ... not our business
     */

    pB = _scmDepotTrnReadSome ( Line, OV, sizeof ( OV ) );
    if ( pB == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcItem, rcInvalid );
    }

    strcat ( OV, "#" );
    strcpy ( NV, OV );

    pB = _scmDepotTrnReadSome (
                                pB,
                                OV + strlen ( OV ),
                                sizeof ( OV ) - strlen ( OV )
                                );
    if ( pB == NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcItem, rcInvalid );
    }

    pB = _scmDepotTrnReadSome (
                                pB,
                                NV + strlen ( NV ),
                                sizeof ( NV ) - strlen ( NV )
                                );
    if ( pB != NULL ) {
        return RC ( rcApp, rcSchema, rcConstructing, rcItem, rcInvalid );
    }

    RCt = copyStringSayNothingRelax ( OldVer, OV );
    if ( RCt == 0 ) {
        RCt = copyStringSayNothingRelax ( NewVer, NV );
    }

    if ( RCt != 0 ) {
        if ( * OldVer != NULL ) {
            free ( ( char * ) * OldVer );
            * OldVer = NULL;
        }
        if ( * NewVer != NULL ) {
            free ( ( char * ) * NewVer );
            * NewVer = NULL;
        }
    }

    return RCt;
}   /* _scmDepotTrnReadLine () */


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
    Ret = NULL;

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
        RCt = _scmDepotTrnReadLine (
                                    Line,
                                    & ( Ret -> _name_old ),
                                    & ( Ret -> _name_new )
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
    const struct karLookUp * _excludes;

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

        if ( self -> _excludes != NULL ) {
            karLookUpDispose ( self -> _excludes );
            self -> _excludes = NULL;
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
                                            true,
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
/*
printf ( "RC[%d] SCHEMA[%s]\n", One -> rc, Buf );
*/
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
    struct scmDepotTrn * lT = ( struct scmDepotTrn * ) L;
    struct scmDepotTrn * rT = ( struct scmDepotTrn * ) R;

    const char * lC = lT == NULL
                ? ""
                : ( lT -> _name_old == NULL ? "" : lT -> _name_old )
                ;
    const char * rC = rT == NULL
                ? ""
                : ( rT -> _name_old == NULL ? "" : rT -> _name_old )
                ;
    return strcmp ( lC, rC );
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
                    if ( ! _scmDepotTrnEmptyLine ( Line ) ) {
                        RCt = _scmDepotTrnMake ( & Trn, Line );
                        if ( RCt == 0 ) {
                            RCt = BSTreeInsert (
                                    & ( self -> _trans ),
                                    & ( Trn -> _node ),
                                    _scmDepotLoadTransformsProcessCallback
                                    );
                        }
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

static
rc_t CC
_scmDepotLoadExcludes (
                        struct scmDepot * self,
                        const char * ExcludesFile
)
{
    rc_t RCt = 0;

    if ( self != NULL && ExcludesFile != NULL ) {
        RCt = karLookUpMake ( & ( self -> _excludes ) );
        if ( RCt == 0 ) {
            RCt = karLookUpLoad ( self -> _excludes, ExcludesFile );
            if ( RCt != 0 ) {
                karLookUpDispose ( self -> _excludes );
                self -> _excludes = NULL;
            }
        }
    }

    return RCt;
}   /* _scmDepotLoadExcludes () */

rc_t CC
scmDepotMake ( struct scmDepot ** Depot, struct DeLiteParams * Params )
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
            RCt = _scmDepotLoadSchemas ( Ret, Params -> _schema );
            if ( RCt == 0 ) {
                    /*  Here we are loading transformations
                     */
                RCt = _scmDepotLoadTransforms (
                                            Ret,
                                            Params -> _transf
                                            );
                if ( RCt == 0 ) {
                    RCt = _scmDepotLoadExcludes (
                                                Ret,
                                                Params -> _exclf
                                                );
                    if ( RCt == 0 ) {
                        * Depot = Ret;
                    }
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
                ( ( struct scmDepotTrn * ) Node ) -> _name_old
                );
}   /* _scmDepotTrnComapre () */

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

/*  That is general method which transform schema for node
 *  In general, if translation for current schema wasn't found,
 *  it is OK. However, in the 'delite' case, schema replacement 
 *  is mandatory.
 */
rc_t CC
scmDepotTransform (
                    struct scmDepot * self,
                    struct KMetadata * Meta,
                    bool ForDelite
)
{
    rc_t RCt;
    struct KMDataNode * DataNode;
    struct scmDepotTrn * Trans;
    char Buf [ 256 ];
    size_t BSize;

    RCt = 0;
    DataNode = NULL;
    Trans = NULL;
    * Buf = 0;
    BSize = 0;

    if ( self == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcNull );
    }

    if ( Meta == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcParam, rcNull );
    }

    if ( self -> _mgr == NULL ) {
        return RC ( rcApp, rcSchema, rcUpdating, rcSelf, rcInvalid );
    }

    RCt = KMetadataOpenNodeUpdate (
                                    Meta,
                                    & DataNode,
                                    SCHEMA_ATTR_NAME
                                    );
    if ( RCt == 0 ) {
        RCt = KMDataNodeReadAttr (
                                DataNode,
                                NAME_ATTR_NAME,
                                Buf,
                                sizeof ( Buf ),
                                & BSize
                                );
        if ( RCt == 0 ) {
            Buf [ BSize ] = 0;
                /*  Here we check if schema was excluded from processing
                 */
            if ( self -> _excludes != NULL ) {
                if ( karLookUpHas ( self -> _excludes, Buf ) ) {
                    RCt = RC ( rcApp, rcSchema, rcUpdating, rcName, rcUnexpected );
                    pLogErr ( klogErr, RCt, "Schema name was excluded from DELITE [$(name)]", "name=%s", Buf );
                }
            } 
            if ( RCt == 0 ) {
                Trans = ( struct scmDepotTrn * ) BSTreeFind (
                                                & ( self -> _trans ),
                                                Buf,
                                                _scmDepotTrnCompare
                                                );
                if ( Trans == NULL ) {
                    if ( ForDelite ) {
                        RCt = RC ( rcApp, rcSchema, rcUpdating, rcItem, rcNotFound );
                        pLogErr ( klogErr, RCt, "Can not find schema for DELITE [$(name)]", "name=%s", Buf );
                    }
                }
                else {
                    KOutMsg ( "    [%s] [%s] -> [%s]\n", SCHEMA_ATTR_NAME, Trans -> _name_old, Trans -> _name_new );
                    pLogMsg ( klogInfo, "    [$(qual)] [$(old)] -> [$(scm)]", "qual=%s,old=%s,scm=%s", SCHEMA_ATTR_NAME, Trans -> _name_old, Trans -> _name_new );

                    RCt = VSchemaDumpToKMDataNode (
                                                    self -> _scm,
                                                    DataNode,
                                                    Trans -> _name_new
                                                    );
                }
            }
        }

        KMDataNodeRelease ( DataNode );
    } 

    return RCt;
}   /* scmDepotTransform () */

