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

#include <kdb/manager.h>
#include <kdb/meta.h>
#include <kdb/namelist.h>
#include <kdb/column.h>
#include <kdb/table.h>
#include <kdb/database.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>

#include <kapp/main.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <endian.h>
#include <byteswap.h>

#include "kar+.h"

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 *
 * Bergamotsya ...
 *
 * That utility has simple syntax;
 *
 *      kar_meta [options] path
 *
 * where path is local path to directory with SRA data.
 * Options could be "set/get value", "update schema"
 * "set/get value" and "update schema" are using locator string to
 * address value to acces. The format of locator string is :
 *
 *   <node>[@<attribute>][=VALUE]|schema_name
 *
 * where path is pat to object in SRA directory, node is a path to node
 * in metadata tree, attribute is a name of attribute to change.
 * in the case if we are editing content there could be string VALUE
 * for example:
 *
 *    schema@name="NCBI:JOJOBA#1.11.111"
 * or
 *    NCBI:JOJOBA#1.11.111
 *
 * Next structure is simple implementation of that kind of locator
 *
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
typedef enum AcType { kInvalid = 0, kSetVal, kInfo, kErase, kUpdScm } AcType;

typedef struct KARLok KARLok;
struct KARLok {
    AcType type;             /* type of action to do */

    const char * node;       /* node */
    const char * attr;       /* attribute */
    const char * val;        /* value */
};

#define KAR_ATTR_SEP '@'
#define KAR_VAL_SEP  '='

static
void CC kar_lok_dump ( KARLok * self )
{
    const char * Nm = NULL;

    if ( self == NULL ) {
        LOGMSG ( klogDebug, "LOK: NULL" );
        return;
    }

    switch ( self -> type ) {
        case kInfo   : Nm = "LOK[GV]"; break;
        case kSetVal : Nm = "LOK[SV]"; break;
        case kErase  : Nm = "LOK[ER]"; break;
        case kUpdScm : Nm = "LOK[US]"; break;
        default:       Nm = "LOK[??]"; break;
    }

    pLogMsg ( klogDebug, "$(name) n [$(node)] a [$(attr)] v [$(value)]",
                "name=%s,node=%s,attr=%s,value=%s",
                Nm,
                ( self -> node != NULL ? self -> node : "NULL" ),
                ( self -> attr != NULL ? self -> attr : "NULL" ),
                ( self -> val != NULL ? self -> val : "NULL" )
                );
}   /* kar_lok_dump () */

static
rc_t CC kar_lok_dispose ( KARLok * self )
{
    if ( self != NULL ) {
        if ( self -> node != NULL ) {
            free ( ( char * ) self -> node );
            self -> node = NULL;
        }

        if ( self -> attr != NULL ) {
            free ( ( char * ) self -> attr );
            self -> attr = NULL;
        }

        if ( self -> val != NULL ) {
            free ( ( char * ) self -> val );
            self -> val = NULL;
        }

        free ( self );
    }

    return 0;
}   /* kar_lok_dispose () */

static
rc_t CC kar_lok_make ( KARLok ** Lok, const char * Str, AcType Type )
{
    rc_t rc;
    KARLok * Ret;
    const char * Next;

    rc = 0;
    Ret = NULL;
    Next = NULL;

    if ( Lok != NULL ) {
        * Lok = NULL;
    }

    if ( Lok == NULL || Str == NULL ) {
        return RC (rcExe, rcPath, rcAllocating, rcParam, rcNull);
    }

    Ret = calloc ( 1, sizeof ( struct KARLok ) );
    if ( Ret == NULL ) {
        return RC (rcExe, rcPath, rcAllocating, rcMemory, rcExhausted);
    }
    else {
        Ret -> type = Type;
        Next = strchr ( Str, KAR_ATTR_SEP );
        if ( Next != NULL ) {
            rc = kar_stndp ( & ( Ret -> node ), Str, Next - Str );
            if ( rc == 0 ) {
                Str = Next + 1;
                Next = strchr ( Str, KAR_VAL_SEP );
                if ( Next == NULL ) {
                    rc = kar_stdp ( & ( Ret -> attr ), Str );
                }
                else {
                    rc = kar_stndp ( & ( Ret -> attr ), Str, Next - Str );
                    if ( rc == 0 ) {
                        Next ++;
                    }
                }
            }
        }
        else {
            Next = strchr ( Str, KAR_VAL_SEP );
            if ( Next == NULL ) {
                rc = kar_stdp ( & ( Ret -> node ), Str );
            }
            else {
                rc = kar_stndp ( & ( Ret -> node ), Str, Next - Str );
                if ( rc == 0 ) {
                    Next ++;
                }
            }
        }

        if ( rc == 0 && Next != NULL ) {
            rc = kar_stdp ( & ( Ret -> val ), Next );
        }
    }

    if ( Ret -> node == NULL ) {
        rc = RC (rcExe, rcPath, rcParsing, rcParam, rcInvalid);
    }

    if ( Ret -> type == kSetVal && Ret -> val == NULL ) {
        rc = RC (rcExe, rcPath, rcParsing, rcParam, rcInvalid);
    }

    if ( rc != 0 ) {
        * Lok = NULL;

        if ( Ret != NULL ) {
            kar_lok_dispose ( Ret );
        }
    }
    else {
        * Lok = Ret;
    }

    return rc;
}   /* kar_lok_make () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Porameters
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
typedef struct Porams Porams;
struct Porams {
    const char * path;
    const char * spath;

    bool allow_remote;

    KARWek * loks;
};

static
rc_t CC kar_porams_whack ( Porams * self )
{
    if ( self != NULL ) {
        if ( self -> path != NULL ) {
            free ( ( char * ) self -> path );
            self -> path = NULL;
        }

        if ( self -> spath != NULL ) {
            free ( ( char * ) self -> spath );
            self -> spath = NULL;
        }

        if ( self -> loks != NULL ) {
            kar_wek_dispose ( self -> loks );
            self -> loks = NULL;
        }
    }

    return 0;
}   /* kar_porams_whack () */

static
rc_t CC kar_porams_init ( Porams * self )
{
    if ( self == NULL ) {
        return RC (rcExe, rcData, rcInitializing, rcSelf, rcNull);
    }

    memset ( self, 0, sizeof ( Porams ) );

    return kar_wek_make ( & ( self -> loks ), 0, 0, ( void (*) ( void * ) ) kar_lok_dispose );
}   /* kar_porams_init () */

static
rc_t CC kar_porams_add_locator (
                                Porams * self,
                                const char * Line,
                                AcType Type
)
{
    rc_t rc;
    KARLok * Lok;

    rc = 0;
    Lok = NULL;

    if ( self == NULL ) {
        return RC (rcApp, rcArgv, rcConstructing, rcSelf, rcNull);
    }

    if ( Line == NULL ) {
        return RC (rcApp, rcArgv, rcConstructing, rcParam, rcNull);
    }

    rc = kar_lok_make ( & Lok, Line, Type );
    if ( rc == 0 ) {
        rc = kar_wek_append ( self -> loks, Lok );
    }

    if ( rc != 0 ) {
        kar_lok_dispose ( Lok );
    }

    return rc;
}   /* kar_porams_add_locator () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Arguments
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
#define OPTION_INFO          "info"
#define OPTION_SET_VAL       "setvalue"
#define OPTION_ERASE         "erase"
#define OPTION_UPD_SCM       "updschema"
#define OPTION_ALLOW_REMOTE  "ara"
#define OPTION_SPATH         "spath"

static const char * info_usage[]         = { "Get info about node node/alias.", NULL };
static const char * set_val_usage[]      = { "Set value of node.", NULL };
static const char * erase_usage[]        = { "Erase node or attribute.", NULL };
static const char * upd_scm_usage[]      = { "Update schema for node.", NULL };
static const char * allow_remote_usage[] = { "Will allow remote data access. Editing may be disabled.", NULL };
static const char * spath_usage[]        = { "Schema path.", NULL };

/* OptDef fields : name  alias  help_gen  help
 *                 max_count  need_value  required
 */
OptDef Options [] =
{
    { OPTION_INFO,          NULL,   NULL, info_usage,         0, true,  false },
    { OPTION_SET_VAL,       NULL,   NULL, set_val_usage,      0, true,  false },
    { OPTION_ERASE,         NULL,   NULL, erase_usage,        0, true,  false },
    { OPTION_UPD_SCM,       NULL,   NULL, upd_scm_usage,      0, true,  false },
    { OPTION_ALLOW_REMOTE,  NULL,   NULL, allow_remote_usage, 1, false, false },
    { OPTION_SPATH,         NULL,   NULL, spath_usage,        1, true,  false }
};

const char UsageDefaultName[] = "kar_meta";


rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n  %s [OPTIONS] local_path\n", progname );
}

rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    OUTMSG (("Archive Command:\n"
	     "  All of these options require the next token on the command line to be\n"
	     "  the name of the archive\n\n"));

    KOutMsg ("Options:\n");

    for ( size_t llp = 0; llp < sizeof ( Options ) / sizeof ( OptDef ); llp ++ ) {
        OptDef * O = Options + llp;

        HelpOptionLine (
                        O -> aliases,
                        O -> name,
                        ( O -> needs_value ? "location" : NULL ),
                        O -> help
                        );
    }

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}

static
rc_t CC get_single_option ( const Args * args, const char * OptName, const char ** Ret, const char * Dflt, bool Required )
{
    rc_t rc;
    uint32_t count, i;
    const void * Val;

    rc = 0;
    count = i = 0;
    Val = NULL;

    rc = ArgsOptionCount ( args, OptName, & count );
    if ( rc != 0 ) {
        pLogErr ( klogFatal, rc, "Failed to verify '$(name)' option", "name=%s", OptName );
        return rc;
    }

    if ( count == 1 ) {
        rc = ArgsOptionValue (args, OptName, 0, & Val );
    }
    else {
        if ( Dflt != NULL) {
            Val = Dflt;
        }
        else {
            if ( Required ) {
                pLogErr ( klogFatal, rc, "Invalid amount of '$(name)' option", "name=%s", OptName );
                return rc;
            }
            else {
                return 0;
            }
        }
    }

    if ( rc == 0 ) {
        if ( Val == NULL ) {
            Val = Dflt;
        }

        if ( Val != NULL ) {
            rc = kar_stdp ( Ret, Val );
        }
    }

    return rc;
}   /* get_single_option () */

static
rc_t CC get_multiply_options (
                            const Args * args,
                            const char * OptName,
                            Porams * P,
                            AcType Type
)
{
    rc_t rc;
    uint32_t count, i;
    const void * Val;

    rc = 0;
    count = i = 0;
    Val = NULL;

    rc = ArgsOptionCount ( args, OptName, & count );
    if ( rc != 0 ) {
        pLogErr ( klogFatal, rc, "Failed to verify '$(name)' option", "name=%s", OptName );
        return rc;
    }

    if ( count == 0 ) {
        return 0;
    }

    for ( i = 0; i < count; i ++ ) {
        rc = ArgsOptionValue (args, OptName, i, & Val );
        if ( rc != 0 ) {
            pLogErr ( klogFatal, rc, "Failed to access '$(name)' option", "name=%s", OptName );
            return rc;
        }

        rc = kar_porams_add_locator ( P, ( const char * ) Val, Type );
        if ( rc != 0 ) {
            pLogErr ( klogFatal, rc, "Failed to append '$(name)' option", "name=%s", OptName );
            return rc;
        }
    }

    return 0;
}   /* get_multiply_options () */

static
rc_t parse_porams_int ( Porams *p, const Args *args )
{
    rc_t rc;
    const char *value;
    uint32_t count;

    rc = 0;
    value = NULL;
    count = 0;

    /* Parameters */
    rc = ArgsParamCount ( args, &count );
    if ( rc != 0 )
    {
        LogErr ( klogFatal, rc, "Failed to retrieve parameter count" );
        return rc;
    }

    if ( count != 1 ) {
        LogErr ( klogFatal, rc, "Path to SRA database (local_path) undefined" );
        return RC ( rcApp, rcArgv, rcParsing, rcParam, rcInsufficient );
    }

    /* Bool Options : allow remote access */
    rc = ArgsOptionCount ( args, OPTION_ALLOW_REMOTE, &count );
    if ( rc == 0 && count != 0 )
        p -> allow_remote = true;

    /* Get Value Options */
    rc = get_multiply_options ( args, OPTION_INFO, p, kInfo );
    if ( rc != 0 ) {
        return rc;
    }

    rc = get_multiply_options ( args, OPTION_SET_VAL, p, kSetVal );
    if ( rc != 0 ) {
        return rc;
    }

    rc = get_multiply_options ( args, OPTION_ERASE, p, kErase );
    if ( rc != 0 ) {
        return rc;
    }

    rc = get_multiply_options ( args, OPTION_UPD_SCM, p, kUpdScm );
    if ( rc != 0 ) {
        return rc;
    }

        /*  JOJOBA: good idea to get SCHEMA path from configuration, but we
         *          will leave it as is for now
         */
    rc = get_single_option ( args, OPTION_SPATH, & ( p -> spath ), NULL, false );
    if ( rc != 0 ) {
        return rc;
    }

    rc = ArgsParamValue ( args, 0, ( const void ** ) &value );
    if ( rc != 0 ) {
        LogErr ( klogFatal, rc, "Failed to retrieve parameter value" );
        return rc;
    }

    if ( p -> allow_remote ) {
        rc = kar_stdp ( & ( p -> path ), value );
        if ( rc != 0 ) {
            LogErr ( klogFatal, rc, "Failed to allocate parameter value" );
            return rc;
        }
    }
    else {
        char BF [ 1024 ];
        const char * pos = strchr ( value, '/' );
        int n = snprintf ( BF, sizeof(BF), ( pos == NULL ? "./%s" : "%s" ), value );
        assert(n < sizeof(BF));
        rc = kar_stdp ( & ( p -> path ), BF );
        if ( rc != 0 ) {
            LogErr ( klogFatal, rc, "Failed to allocate parameter value" );
            return rc;
        }
    }

    return rc;
}

rc_t validate_porams ( Porams *p );

rc_t parse_porams ( Porams *p, int argc, char * argv [] )
{
    rc_t rc;
    Args * args;

    rc = 0;
    args = NULL;

    rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
        Options, sizeof Options / sizeof ( Options [ 0 ] ) );
    if ( rc == 0 )
    {
        rc = parse_porams_int ( p, args );

        if ( rc == 0 )
            rc = validate_porams ( p );

        ArgsWhack ( args );
    }

    return rc;
}

rc_t validate_porams ( Porams *p )
{
    rc_t rc;
    bool UpdateSchema;
    size_t llp;

    rc = 0;
    UpdateSchema = false;
    llp = 0;

    if ( p == NULL ) {
        rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcNull );
        LogErr ( klogErr, rc, "NULL parameter passed to validate method" );
        return rc;
    }

    if ( p -> path == NULL ) {
        rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInvalid );
        LogErr ( klogErr, rc, "Missed path to SRA directory parameter" );
        return rc;
    }

    if ( p -> loks != NULL ) {
        for ( llp = 0; llp < kar_wek_size ( p -> loks ) ; llp ++ ) {
            KARLok * Lok = kar_wek_get ( p -> loks, llp );
            if ( Lok != NULL ) {
                if ( Lok -> type == kUpdScm ) {
                    UpdateSchema = true;
                    break;
                }
            }
        }
    }

    if ( UpdateSchema && p -> spath == NULL ) {
        rc = RC ( rcApp, rcArgv, rcParsing, rcParam, rcInvalid );
        LogErr ( klogErr, rc, "Missed path to SCHEMA directory argument" );
        return rc;
    }


    return rc;
}

/*
 * Startup
 */
static rc_t CC run ( Porams * porams );

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Porams porams;

    rc_t rc = kar_porams_init ( & porams );
    if ( rc == 0 ) {
        rc = parse_porams ( &porams, argc, argv );
        if ( rc == 0 ) {
            rc = run ( &porams );
        }

        kar_porams_whack ( & porams );
    }

    if ( rc == 0 )
        STSMSG (1, ("Success: Exiting kar\n"));

    return VDB_TERMINATE( rc );
}


/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Middle of nowhere ...
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
typedef struct UtSt UtSt;
struct UtSt {
    Porams * porams;

    KDBManager * manager;

    uint32_t type;

    size_t info;          /* get basic info about node */
    size_t setval;        /* set value node/attf */
    size_t erase;         /* erase node or attribute */
    size_t updscm;        /* update schema */
};

static
rc_t CC utst_whack ( UtSt * self )
{
    if ( self != NULL ) {
        self -> porams = NULL;

        self -> info = 0;
        self -> setval = 0;
        self -> erase = 0;
        self -> updscm = 0;

        if ( self -> manager != NULL ) {
            KDBManagerRelease ( self -> manager );
            self -> manager = NULL;
        }
    }

    return 0;
}   /* utst_whack () */

static
rc_t CC utst_init ( UtSt * self, Porams * p )
{
    rc_t rc;
    KDirectory * Dir;
    KARLok * Lok;

    rc = 0;
    Dir = NULL;
    Lok = NULL;

    if ( self == NULL ) {
        return RC (rcExe, rcApp, rcProcessing, rcSelf, rcNull);
    }

    if ( p == NULL ) {
        return RC (rcExe, rcApp, rcProcessing, rcParam, rcNull);
    }

    memset ( self, 0, sizeof ( UtSt ) );

    self -> porams = p;

    for ( size_t llp = 0; llp < kar_wek_size ( p -> loks ) ; llp ++ ) {
        Lok = ( KARLok * ) kar_wek_get ( p -> loks, llp );
        if ( Lok != NULL ) {
            if ( Lok -> type == kInfo )   { self -> info ++; }
            if ( Lok -> type == kSetVal ) { self -> setval ++; }
            if ( Lok -> type == kErase )  { self -> erase ++; }
            if ( Lok -> type == kUpdScm ) { self -> updscm ++; }
        }
    }

    rc = KDirectoryNativeDir ( & Dir );
    if ( rc == 0 ) {
        rc = KDBManagerMakeUpdate ( & ( self -> manager ), Dir );

        if ( rc == 0 ) {
            self -> type = KDBManagerPathType ( self -> manager, "%s", p -> path );
            switch ( self -> type ) {
                case kptDatabase:
                case kptTable:
                case kptColumn:
                    break;
                case kptNotFound:
                    rc = RC ( rcDB, rcMgr, rcAccessing, rcPath, rcNotFound );
                    pLogErr ( klogFatal, rc, "'$(path)' not found", "path=%s", p -> path );
                    break;
                case kptBadPath:
                    rc = RC ( rcDB, rcMgr, rcAccessing, rcPath, rcInvalid );
                    pLogErr ( klogFatal, rc, "'$(path)' -- bad path", "path=%s", p -> path );
                    break;
                default:
                    rc = RC ( rcDB, rcMgr, rcAccessing, rcPath, rcIncorrect );
                    pLogErr ( klogFatal, rc, "'$(path)' -- type unknown", "path=%s", p -> path );
                    break;
            }
        }

        KDirectoryRelease ( Dir );
    }

    return rc;
}   /* utst_init () */

static
void CC utst_dump ( UtSt * utst )
{
    const char * Nm = NULL;

    if ( utst == NULL ) {
        LOGMSG ( klogDebug, "UTST: NULL" );
        return;
    }

    switch ( utst -> type ) {
        case kptDatabase      : Nm = "DB";      break;
        case kptTable         : Nm = "TBL";     break;
        case kptIndex         : Nm = "IND";     break;
        case kptColumn        : Nm = "COL";     break;
        case kptMetadata      : Nm = "MD";      break;
        case kptPrereleaseTbl : Nm = "PRE TBL"; break;
        default               : Nm = "UNK";     break;
    }

    pLogMsg ( klogDebug, "UTST: tp [$(type)][$(name)] op [$(oper)] in [$(info)] sv [$(setv)] er [$(eras)] us [$(ups)] sp [$(spat)]",
                    "type=%u,name=%s,oper=%ld,info=%ld,setv=%ld,eras=%ld,ups=%ld,spat=%s",
                    utst -> type,
                    Nm,
                    kar_wek_size ( utst -> porams -> loks ),
                    utst -> info,
                    utst -> setval,
                    utst -> erase,
                    utst -> updscm,
                    ( utst -> porams == NULL ? NULL : ( utst -> porams -> spath ) )
                    );
}   /* utst_dump () */

static
rc_t CC print_basic_info ( UtSt * utst )
{
    rc_t rc;

    rc = 0;

    if ( utst -> info || utst -> setval || utst -> erase || utst -> updscm ) {
        return 0;
    }

    return rc;
}   /* print_basic_info () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Updating schema
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t CC update_schema_for ( KMetadata * Meta, KARLok * Lok, VSchema * Schema )
{
    rc_t rc;
    KMDataNode * Node;

    rc = 0;
    Node = NULL;

    rc = KMetadataOpenNodeUpdate ( Meta, & Node, Lok -> node );
    if ( rc == 0 ) {
        rc = VSchemaDumpToKMDataNode ( Schema, Node, Lok -> val );

        KMDataNodeRelease ( Node );
    }

    return rc;
}   /* update_schema_for () */

static
rc_t CC update_schema_for_db ( UtSt * utst, KARLok * Lok, VSchema * Schema )
{
    rc_t rc;
    KDatabase * Db;
    KMetadata * Meta;

    rc = 0;
    Db = NULL;
    Meta = NULL;

    rc = KDBManagerOpenDBUpdate ( utst -> manager, & Db, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KDatabaseOpenMetadataUpdate ( Db, & Meta );
        if ( rc == 0 ) {
            rc = update_schema_for ( Meta, Lok, Schema );

            KMetadataRelease ( Meta );
        }

        KDatabaseRelease ( Db );
    }

    return rc;
}   /* update_schema_for_db () */

static
rc_t CC update_schema_for_table ( UtSt * utst, KARLok * Lok, VSchema * Schema )
{
    rc_t rc;
    KTable * Tbl;
    KMetadata * Meta;

    rc = 0;
    Tbl = NULL;
    Meta = NULL;

    rc = KDBManagerOpenTableUpdate ( utst -> manager, & Tbl, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KTableOpenMetadataUpdate ( Tbl, & Meta );
        if ( rc == 0 ) {
            rc = update_schema_for ( Meta, Lok, Schema );

            KMetadataRelease ( Meta );
        }

        KTableRelease ( Tbl );
    }

    return rc;
}   /* update_schema_for_table () */

static
rc_t CC update_schema_for_column ( UtSt * utst, KARLok * Lok, VSchema * Schema )
{
    rc_t rc;
    KColumn * Col;
    KMetadata * Meta;

    rc = 0;
    Col = NULL;
    Meta = NULL;

    rc = KDBManagerOpenColumnUpdate ( utst -> manager, & Col, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KColumnOpenMetadataUpdate ( Col, & Meta );
        if ( rc == 0 ) {
            rc = update_schema_for ( Meta, Lok, Schema );

            KMetadataRelease ( Meta );
        }

        KColumnRelease ( Col );
    }

    return rc;
}   /* update_schema_for_column () */

static
rc_t CC update_schema ( UtSt * utst, KARLok * Lok, VSchema * Schema )
{
    rc_t rc = 0;

    if ( utst == NULL || Lok == NULL ) {
        return RC (rcExe, rcApp, rcAccessing, rcParam, rcNull);
    }

    switch ( utst -> type ) {
        case kptDatabase:
            rc = update_schema_for_db ( utst, Lok, Schema );
            break;

        case kptTable:
            rc = update_schema_for_table ( utst, Lok, Schema );
            break;

        case kptColumn:
            rc = update_schema_for_column ( utst, Lok, Schema );
            break;

        default:
            rc = RC (rcExe, rcApp, rcAccessing, rcParam, rcInvalid);
            break;
    }

    return rc;
}   /* update_schema () */

/*  JOJOBA: Very ugly - reimplement
 */
static
rc_t CC make_parse_schema_callback (
                                    const struct KDirectory_v1 * Dir,
                                    uint32_t Type,
                                    const char * Name,
                                    void * Data
)
{
    rc_t rc;
    static const char * Pat = ".vschema";
    size_t l1, l2;
    VSchema * Schema;
    char Buf [ 4096 ];

    rc = 0;
    l1 = l2 = 0;
    Schema = ( VSchema * ) Data;

    if ( Type == kptFile ) {
        if ( strcmp ( Name, "pevents.vschema" ) == 0 ) {
            pLogMsg ( klogInfo, "Skipping schema '$(name)'", "name=%s", Name );
            return 0;
        }

            /*  First we should be sure if that file is schema file
             *  or, in in other words it's name ends with ".vschema"
             */
        l1 = strlen ( Name );
        l2 = strlen ( Pat );
        if ( l2 < l1 ) {
            if ( strcmp ( Name + l1 - l2, Pat ) == 0 ) {
                rc = KDirectoryResolvePath (
                                            Dir,
                                            true,
                                            Buf,
                                            sizeof ( Buf ),
                                            "%s",
                                            Name
                                            );
                if ( rc == 0 ) {
                    rc = VSchemaParseFile ( Schema, "%s", Buf );
                }
            }
        }
    }

    return rc;
}   /* make_parse_schema_callback () */

static
rc_t CC make_parse_schema ( UtSt * utst, VSchema ** Schema )
{
    rc_t rc;
    VSchema * Ret;
    KDirectory * Dir;
    VDBManager * Mgr;

    rc = 0;
    Ret = NULL;
    Dir = NULL;
    Mgr = NULL;

    if ( Schema != NULL ) {
        * Schema = NULL;
    }

    if ( utst == NULL || Schema == NULL ) {
        return RC (rcExe, rcApp, rcAccessing, rcParam, rcNull);
    }

    rc = KDirectoryNativeDir ( & Dir );
    if ( rc == 0 ) {

        rc = VDBManagerMakeUpdate ( & Mgr, Dir );
        if ( rc == 0 ) {

            rc = VDBManagerMakeSchema ( Mgr, & Ret );
            if ( rc == 0 ) {
                rc = VSchemaAddIncludePath ( Ret, utst -> porams -> spath );
                if ( rc == 0 ) {
                    rc = KDirectoryVisit (
                                            Dir,
                                            true,
                                            make_parse_schema_callback,
                                            Ret,
                                            utst -> porams -> spath
                                            );
                    if ( rc == 0 ) {
                        * Schema = Ret;
                    }
                }
            }

            VDBManagerRelease ( Mgr );
        }

        KDirectoryRelease ( Dir );
    }

    return rc;
}   /* make_parse_schema () */

static
rc_t CC update_schemas ( UtSt * utst )
{
    rc_t rc;
    size_t llp;
    VSchema * Schema;

    rc = 0;
    llp = 0;

    if ( ! utst -> updscm ) {
        return 0;
    }

    rc = make_parse_schema ( utst, & Schema );
    if ( rc == 0 ) {
        for ( llp = 0; llp < kar_wek_size ( utst -> porams -> loks ); llp ++ ) {
            KARLok * Lok = kar_wek_get ( utst -> porams -> loks, llp );
            if ( Lok != NULL ) {
                if ( Lok -> type == kUpdScm ) {
                    kar_lok_dump ( Lok );

                    update_schema ( utst, Lok, Schema );
                }
            }
        }

        VSchemaRelease ( Schema );
    }

    return rc;
}   /* update_schemas () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Erasing all that things
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t CC names_for_path ( const char * Path, const char ** Parent, const char ** Name )
{
    rc_t rc;
    const char * Pos;

    * Parent = NULL;
    * Name = NULL;

    rc = 0;
    Pos = strrchr ( Path, '/' );
    if ( Pos == NULL ) {
        if ( strcmp ( Path, "." ) == 0 ) {
            rc = RC (rcExe, rcApp, rcAccessing, rcParam, rcInvalid);
        }
        else {
            rc = kar_stdp ( Parent, "." );
            if ( rc == 0 ) {
                rc = kar_stdp ( Name, Path );
            }
        }
    }
    else {
        rc = kar_stndp ( Parent, Path, Pos - Path );
        if ( rc == 0 ) {
            rc = kar_stdp ( Name, Pos + 1 );
        }
    }

    if ( rc != 0 ) {
        if ( * Parent != NULL ) {
            free ( ( char * ) * Parent );
            * Parent = NULL;
        }
        if ( * Name != NULL ) {
            free ( ( char * ) * Name );
            * Name = NULL;
        }
    }

    return rc;
}   /* names_for_path () */

static
rc_t CC do_erase_for ( KMetadata * Meta, KARLok * Lok )
{
    rc_t rc;
    KMDataNode * Node;

    rc = 0;
    Node = NULL;

        /* since API erases nodes and attributes by name ... */
    if ( Lok -> attr == NULL ) {
        const char * Parent = NULL, * Name = NULL;
        rc = names_for_path ( Lok -> node, & Parent, & Name );
        if ( rc == 0 ) {
            rc = KMetadataOpenNodeUpdate ( Meta, & Node, Parent );
            if ( rc == 0 ) {
                rc = KMDataNodeDropChild ( Node, "%s", Name );

                KMDataNodeRelease ( Node );
            }

            free ( ( char * ) Parent );
            free ( ( char * ) Name );
        }
    }
    else {
        rc = KMetadataOpenNodeUpdate ( Meta, & Node, Lok -> node );
        if ( rc == 0 ) {
            rc = KMDataNodeDropAttr ( Node, Lok -> attr );

            KMDataNodeRelease ( Node );
        }
    }

    return rc;
}   /* do_erase_for () */

static
rc_t CC do_erase_for_db ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    KDatabase * Db;
    KMetadata * Meta;

    rc = 0;
    Db = NULL;
    Meta = NULL;

    rc = KDBManagerOpenDBUpdate ( utst -> manager, & Db, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KDatabaseOpenMetadataUpdate ( Db, & Meta );
        if ( rc == 0 ) {
            rc = do_erase_for ( Meta, Lok );

            if ( rc == 0 ) {
                rc = KMetadataRelease ( Meta );
            }
        }

        KDatabaseRelease ( Db );
    }

    return rc;
}   /* do_erase_for_db () */

static
rc_t CC do_erase_for_table ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    KTable * Tbl;
    KMetadata * Meta;

    rc = 0;
    Tbl = NULL;
    Meta = NULL;

    rc = KDBManagerOpenTableUpdate ( utst -> manager, & Tbl, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KTableOpenMetadataUpdate ( Tbl, & Meta );
        if ( rc == 0 ) {
            rc = do_erase_for ( Meta, Lok );

            if ( rc == 0 ) {
                rc = KMetadataRelease ( Meta );
            }
        }

        KTableRelease ( Tbl );
    }

    return rc;
}   /* do_erase_for_table () */

static
rc_t CC do_erase_for_column ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    KColumn * Col;
    KMetadata * Meta;

    rc = 0;
    Col = NULL;
    Meta = NULL;

    rc = KDBManagerOpenColumnUpdate ( utst -> manager, & Col, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KColumnOpenMetadataUpdate ( Col, & Meta );
        if ( rc == 0 ) {
            rc = do_erase_for ( Meta, Lok );

            if ( rc == 0 ) {
                rc = KMetadataRelease ( Meta );
            }
        }

        KColumnRelease ( Col );
    }

    return rc;
}   /* do_erase_for_column () */

static
rc_t CC do_erase (UtSt * utst, KARLok * Lok )
{
    rc_t rc = 0;

    if ( utst == NULL || Lok == NULL ) {
        return RC (rcExe, rcApp, rcAccessing, rcParam, rcNull);
    }

    kar_lok_dump ( Lok );

    switch ( utst -> type ) {
        case kptDatabase:
            rc = do_erase_for_db ( utst, Lok );
            break;

        case kptTable:
            rc = do_erase_for_table ( utst, Lok );
            break;

        case kptColumn:
            rc = do_erase_for_column ( utst, Lok );
            break;

        default:
            rc = RC (rcExe, rcApp, rcAccessing, rcParam, rcInvalid);
            break;
    }

    return rc;
}   /* do_erase () */

static
rc_t CC do_erase_all ( UtSt * utst )
{
    rc_t rc;
    size_t llp;

    rc = 0;
    llp = 0;

    if ( ! utst -> erase ) {
        return 0;
    }

    for ( llp = 0; llp < kar_wek_size ( utst -> porams -> loks ); llp ++ ) {
        KARLok * Lok = kar_wek_get ( utst -> porams -> loks, llp );
        if ( Lok != NULL ) {
            if ( Lok -> type == kErase ) {
                do_erase ( utst, Lok );
            }
        }
    }

    return rc;
}   /* do_erase_all () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Set Value
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t reencode_hex ( const char ** Ret, const char * expr )
{
    char * buff = NULL;

    * Ret = NULL;

    /* took that code from kdbmeta.c without changes. I find that it works well */
    /* according to documentation, "expr" is allowed to be text
       or text with escaped hex sequences. examine for escaped hex */
    size_t len = string_size ( expr );
    buff = malloc ( len + 1 );
    if ( buff == NULL )
        return RC ( rcExe, rcMetadata, rcUpdating, rcMemory, rcExhausted );
    else
    {
        size_t i, j;
        for ( i = j = 0; i < len; ++ i, ++ j )
        {
            if ( ( buff [ j ] = expr [ i ] ) == '\\' )
            {
                /* we know "expr" is NUL-terminated, so this is safe */
                if ( tolower ( expr [ i + 1 ] ) == 'x' &&
                     isxdigit ( expr [ i + 2 ] ) &&
                     isxdigit ( expr [ i + 3 ] ) )
                {
                    int msn = toupper ( expr [ i + 2 ] ) - '0';
                    int lsn = toupper ( expr [ i + 3 ] ) - '0';
                    if ( msn >= 10 )
                        msn += '0' - 'A' + 10;
                    if ( lsn >= 10 )
                        lsn += '0' - 'A' + 10;
                    buff [ j ] = ( char ) ( ( msn << 4 ) | lsn );
                    i += 3;
                }
            }
        }

        buff [ j ] = 0;

        * Ret = buff;
    }

    return 0;
}   /* reencode_hex () */

static
rc_t CC update_attr_value ( KMDataNode * Node, const char * AttrName, const char * Val )
{
    rc_t rc;
    const char * Expr;

    rc = 0;
    Expr = NULL;

    rc = reencode_hex ( & Expr, Val );
    if ( rc == 0 ) {
        rc = KMDataNodeWriteAttr ( Node, AttrName, Expr );

        free ( ( char * ) Expr );
    }

    return rc;
}   /* update_attr_value () */

static
rc_t CC set_value_for ( KMetadata * Meta, KARLok * Lok )
{
    rc_t rc;
    KMDataNode * Node;

    rc = 0;
    Node = NULL;

    rc = KMetadataOpenNodeUpdate ( Meta, & Node, Lok -> node );
    if ( rc == 0 ) {
        if ( Lok -> attr == NULL ) {
            rc = KMDataNodeWrite ( Node, Lok -> val, strlen ( Lok -> val ) );
        }
        else {
            rc = update_attr_value ( Node, Lok -> attr, Lok -> val );
        }

        rc = KMDataNodeRelease ( Node );
    }

    return rc;
}   /* set_value_for () */

static
rc_t CC set_value_for_db ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    KDatabase * Db;
    KMetadata * Meta;

    rc = 0;
    Db = NULL;
    Meta = NULL;

    rc = KDBManagerOpenDBUpdate ( utst -> manager, & Db, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KDatabaseOpenMetadataUpdate ( Db, & Meta );
        if ( rc == 0 ) {
            rc = set_value_for ( Meta, Lok );

            rc = KMetadataRelease ( Meta );
        }

        rc = KDatabaseRelease ( Db );
    }

    return rc;
}   /* set_value_for_db () */

static
rc_t CC set_value_for_table ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    KTable * Tbl;
    KMetadata * Meta;

    rc = 0;
    Tbl = NULL;
    Meta = NULL;

    rc = KDBManagerOpenTableUpdate ( utst -> manager, & Tbl, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KTableOpenMetadataUpdate ( Tbl, & Meta );
        if ( rc == 0 ) {
            rc = set_value_for ( Meta, Lok );

            rc = KMetadataRelease ( Meta );
        }

        rc = KTableRelease ( Tbl );
    }

    return rc;
}   /* set_value_for_table () */

static
rc_t CC set_value_for_column ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    KColumn * Col;
    KMetadata * Meta;

    rc = 0;
    Col = NULL;
    Meta = NULL;

    rc = KDBManagerOpenColumnUpdate ( utst -> manager, & Col, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KColumnOpenMetadataUpdate ( Col, & Meta );
        if ( rc == 0 ) {
            rc = set_value_for ( Meta, Lok );

            rc = KMetadataRelease ( Meta );
        }

        rc = KColumnRelease ( Col );
    }

    return rc;
}   /* set_value_for_column () */

static
rc_t CC set_value (UtSt * utst, KARLok * Lok )
{
    rc_t rc = 0;

    if ( utst == NULL || Lok == NULL ) {
        return RC (rcExe, rcApp, rcAccessing, rcParam, rcNull);
    }

    kar_lok_dump ( Lok );

    switch ( utst -> type ) {
        case kptDatabase:
            rc = set_value_for_db ( utst, Lok );
            break;

        case kptTable:
            rc = set_value_for_table ( utst, Lok );
            break;

        case kptColumn:
            rc = set_value_for_column ( utst, Lok );
            break;

        default:
            rc = RC (rcExe, rcApp, rcAccessing, rcParam, rcInvalid);
            break;
    }

    return rc;
}   /* set_value () */

static
rc_t CC set_values ( UtSt * utst )
{
    rc_t rc;
    size_t llp;

    rc = 0;
    llp = 0;

    if ( ! utst -> setval ) {
        return 0;
    }

    for ( llp = 0; llp < kar_wek_size ( utst -> porams -> loks ); llp ++ ) {
        KARLok * Lok = kar_wek_get ( utst -> porams -> loks, llp );
        if ( Lok != NULL ) {
            if ( Lok -> type == kSetVal ) {
                rc = set_value ( utst, Lok );
                if ( rc != 0 ) {
                    break;
                }
            }
        }
    }

    return rc;
}   /* set_values () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Info
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t CC dump_namelist ( KNamelist * List, const char * Preffix )
{
    rc_t rc;
    const char * Name;
    uint32_t Qty, llp;

    rc = 0;
    Name = NULL;
    Qty = llp = 0;

    rc = KNamelistCount ( List, & Qty );
    if ( rc == 0 ) {
        for ( llp = 0; llp < Qty; llp ++ ) {
            rc = KNamelistGet ( List, llp, & Name );
            if ( rc != 0 ) {
                break;
            }

            if ( Preffix == NULL ) {
                printf ( "%s\n", Name );
            }
            else {
                printf ( "%s\t%s\n", Preffix, Name );
            }
        }
    }

    return rc;
}   /* dump_namelist () */

static
rc_t CC dump_attributes ( const KMDataNode * Node )
{
    rc_t rc;
    KNamelist * Attr;

    rc = 0;
    Attr = NULL;

    rc = KMDataNodeListAttr ( Node, & Attr );
    if ( rc == 0 ) {
        rc = dump_namelist ( Attr, "attr:" );

        KNamelistRelease ( Attr );
    }

    return rc;
}   /* dump_attributes () */

static
rc_t CC dump_children ( const KMDataNode * Node )
{
    rc_t rc;
    KNamelist * Cdn;

    rc = 0;
    Cdn = NULL;

    rc = KMDataNodeListChildren ( Node, & Cdn );
    if ( rc == 0 ) {
        rc = dump_namelist ( Cdn, "node:" );

        KNamelistRelease ( Cdn );
    }

    return rc;
}   /* dump_children () */

static
rc_t CC dump_node_value ( const KMDataNode * Node )
{
    rc_t rc;
    size_t NmRd, Off, Rem;
    char Bf [ 1024 ];

    rc = 0;
    NmRd = Off = 0;
    Rem = 1;

    while ( Rem != 0 ) {
        rc = KMDataNodeRead (
                            Node,
                            Off,
                            Bf,
                            sizeof ( Bf ) - 1,
                            & NmRd,
                            & Rem
                            );
        if ( rc != 0 ) {
            break;
        }

        if ( Off == 0 ) {
            printf ( "value: " );
        }

        if ( NmRd == 0 ) {
            break;
        }

        if ( NmRd != 0 ) {
            Bf [ NmRd ] = 0;

            printf ( "%s", Bf );
        }

        Off += NmRd;
    }

    if ( rc == 0 && Off != 0 ) {
        printf ( "\n" );
    }

    return rc;
}   /* dump_node_value () */

static
rc_t CC dump_attr_value ( const KMDataNode * Node, const char * AttrName )
{
    rc_t rc;
    char BF [ 4096 ];
    size_t rsz;

    rc = 0;
    rsz = 0;


    rc = KMDataNodeReadAttr ( Node, AttrName, BF, sizeof ( BF ), & rsz );
    if ( rc == 0 ) {
        if ( rsz != 0 ) {
            BF [ rsz ] = 0;
            printf ( "value: %s\n", BF );
        }
    }

    return rc;
}   /* dump_attr_value () */

static
rc_t CC get_info_for ( const KMetadata * Meta, KARLok * Lok )
{
    rc_t rc;
    const KMDataNode * Node;

    rc = 0;
    Node = NULL;

    rc = KMetadataOpenNodeRead ( Meta, & Node, Lok -> node );
    if ( rc == 0 ) {
        if ( Lok -> attr == NULL ) {
            /*  Dealing with node
             */
            rc = dump_children ( Node );
            if ( rc == 0 ) {
                rc = dump_attributes ( Node );
            }

            rc = dump_node_value ( Node );
        }
        else {
            rc = dump_attr_value ( Node, Lok -> attr );
        }

        rc = KMDataNodeRelease ( Node );
    }

    return rc;
}   /* get_info_for () */

static
rc_t CC get_info_for_db ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    const KDatabase * Db;
    const KMetadata * Meta;

    rc = 0;
    Db = NULL;
    Meta = NULL;

    rc = KDBManagerOpenDBRead ( utst -> manager, & Db, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KDatabaseOpenMetadataRead ( Db, & Meta );
        if ( rc == 0 ) {
            rc = get_info_for ( Meta, Lok );

            KMetadataRelease ( Meta );
        }

        KDatabaseRelease ( Db );
    }

    return rc;
}   /* get_info_for_db () */

static
rc_t CC get_info_for_table ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    const KTable * Tbl;
    const KMetadata * Meta;

    rc = 0;
    Tbl = NULL;
    Meta = NULL;

    rc = KDBManagerOpenTableRead ( utst -> manager, & Tbl, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KTableOpenMetadataRead ( Tbl, & Meta );
        if ( rc == 0 ) {
            rc = get_info_for ( Meta, Lok );

            KMetadataRelease ( Meta );
        }

        KTableRelease ( Tbl );
    }

    return rc;
}   /* get_info_for_table () */

static
rc_t CC get_info_for_column ( UtSt * utst, KARLok * Lok )
{
    rc_t rc;
    const KColumn * Col;
    const KMetadata * Meta;

    rc = 0;
    Col = NULL;
    Meta = NULL;

    rc = KDBManagerOpenColumnRead ( utst -> manager, & Col, "%s", utst -> porams -> path );
    if ( rc == 0 ) {
        rc = KColumnOpenMetadataRead ( Col, & Meta );
        if ( rc == 0 ) {
            rc = get_info_for ( Meta, Lok );

            KMetadataRelease ( Meta );
        }

        KColumnRelease ( Col );
    }

    return rc;
}   /* get_info_for_column () */

static
rc_t CC get_info (UtSt * utst, KARLok * Lok )
{
    rc_t rc = 0;

    if ( utst == NULL || Lok == NULL ) {
        return RC (rcExe, rcApp, rcAccessing, rcParam, rcNull);
    }

    kar_lok_dump ( Lok );

    switch ( utst -> type ) {
        case kptDatabase:
            rc = get_info_for_db ( utst, Lok );
            break;

        case kptTable:
            rc = get_info_for_table ( utst, Lok );
            break;

        case kptColumn:
            rc = get_info_for_column ( utst, Lok );
            break;

        default:
            rc = RC (rcExe, rcApp, rcAccessing, rcParam, rcInvalid);
            break;
    }

    return rc;
}   /* get_info () */

static
rc_t CC get_infos ( UtSt * utst )
{
    rc_t rc;
    size_t llp;

    rc = 0;
    llp = 0;

    if ( ! utst -> info ) {
        return 0;
    }

    for ( llp = 0; llp < kar_wek_size ( utst -> porams -> loks ) ; llp ++ ) {
        KARLok * Lok = kar_wek_get ( utst -> porams -> loks, llp );
        if ( Lok != NULL ) {
            if ( Lok -> type == kInfo ) {
                rc = get_info ( utst, Lok );
                if ( rc != 0 ) {
                    break;
                }
            }
        }
    }

    return rc;
}   /* get_infos () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*
 * Here we go :D
 * Simple plan:
 *   if there aren't directives like set/get/upd it will print basic
 *   info about SRA object
 *   if there is directives, they will be performed in the order:
 *          updateschema; setvalue; erase; info;
 *_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
static
rc_t CC run ( Porams * p )
{
    rc_t rc;
    UtSt utst;

    rc = 0;

    if ( p == NULL ) {
        return RC (rcExe, rcApp, rcProcessing, rcParam, rcNull);
    }

    rc = utst_init ( & utst, p );
    if ( rc == 0 ) {
        utst_dump ( & utst );

        if ( ! utst . info && ! utst . setval && ! utst . erase && ! utst . updscm ) {
            rc = print_basic_info ( & utst );
        }
        if ( rc == 0 ) {
            rc = update_schemas ( & utst );
            if ( rc == 0 ) {
                rc = set_values ( & utst );
                if ( rc == 0 ) {
                    rc = get_infos ( & utst );
                    if ( rc == 0 ) {
                        rc = do_erase_all ( & utst );
                    }
                }
            }
        }
        utst_whack ( & utst );
    }

    return rc;
}   /* run () */
