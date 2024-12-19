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

#include "srapath.h" /* OPTION_PARAM */
#include "services.h" /* names_request */
#include "helper.h"
#include "line_iter.h"

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver-priv.h> /* VResolverGetProject */
#include <vfs/services.h> /* KServiceMake */
#include <vfs/services-priv.h> /* KServiceGetQuality */

#include <kfs/directory.h>
#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/log.h>
#include <klib/out.h>
#include <klib/printf.h> /* string_printf */
#include <klib/rc.h>
#include <klib/status.h> /* STSMSG */
#include <klib/text.h>
#include <klib/vector.h>

#include <sysalloc.h>


#include <limits.h> /* PATH_MAX */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

#define DEF_PROTO "https"

static const char * proto_usage[]
  = { "protocol (fasp; http; https; fasp,http; ...) default=" DEF_PROTO, NULL };
#define OPTION_PROTO  "protocol"
#define ALIAS_PROTO   "a"

static const char * cache_usage[] = { "resolve cache location along with remote"
                                      " when performing names function", NULL };
#define ALIAS_CACHE   "c"

static const char * cart_usage[] = { "PATH to jwt cart file", NULL };
#define OPTION_CART   "perm"
#define ALIAS_CART    NULL

static const char * prj_usage[]
 = { "use numeric [dbGaP] project-id in names-cgi-call", NULL };
#define ALIAS_PRJ    "d"

static const char * vers_usage[] = { "version-string for cgi-calls", NULL };
#define OPTION_VERS   "vers"
#define ALIAS_VERS    "e"

static const char * func_usage[] = { "function to perform "
    "(" FUNCTION_RESOLVE ", " FUNCTION_NAMES ", " FUNCTION_SEARCH ") "
    "default=" FUNCTION_RESOLVE
    " or " FUNCTION_NAMES " if " OPTION_PROTO " is specified", NULL };
#define OPTION_FUNC   "function"
#define ALIAS_FUNC    "f"

static const char * locn_usage[] = { "location of data", NULL };
#define OPTION_LOCN   "location"
#define ALIAS_LOCN    NULL

static const char * ngc_usage[] = { "PATH to ngc file", NULL };
#define OPTION_NGC   "ngc"
#define ALIAS_NGC     NULL

static const char * path_usage[]
 = { "print path of object: names function-only", NULL };
#define OPTION_PATH   "path"
#define ALIAS_PATH    "P"

static const char * param_usage[]
 = { "param to be added to cgi-call (tic=XXXXX): raw-only", NULL };
#define ALIAS_PARAM   "p"

static const char * raw_usage[] = { "print the raw reply (instead of parsing it)", NULL };
#define ALIAS_RAW     "r"

static const char * json_usage[] = { "print the reply in JSON", NULL };
#define ALIAS_JSON    "j"

static const char * timeout_usage[] = { "timeout-value for request", NULL };
#define OPTION_TIMEOUT "timeout"
#define ALIAS_TIMEOUT "t"

static const char * url_usage[] = { "url to be used for cgi-calls", NULL };
#define OPTION_URL    "url"
#define ALIAS_URL     "u"

OptDef ToolOptions[] =
{                                                    /* needs_value, required */
    { OPTION_FUNC   , ALIAS_FUNC   , NULL, func_usage   ,   1,  true,   false },
    { OPTION_LOCN   , ALIAS_LOCN   , NULL, locn_usage   ,   1,  true,   false },
    { OPTION_TIMEOUT, ALIAS_TIMEOUT, NULL, timeout_usage,   1,  true,   false },
    { OPTION_PROTO  , ALIAS_PROTO  , NULL, proto_usage  ,   1,  true,   false },
    { OPTION_VERS   , ALIAS_VERS   , NULL, vers_usage   ,   1,  true,   false },
    { OPTION_URL    , ALIAS_URL    , NULL, url_usage    ,   1,  true,   false },
    { OPTION_PARAM  , ALIAS_PARAM  , NULL, param_usage  ,  10,  true,   false },
    { OPTION_RAW    , ALIAS_RAW    , NULL, raw_usage    ,   1,  false,  false },
    { OPTION_JSON   , ALIAS_JSON   , NULL, json_usage   ,   1,  false,  false },
    { OPTION_PRJ    , ALIAS_PRJ    , NULL, prj_usage    ,  10,  true ,  false },
    { OPTION_CACHE  , ALIAS_CACHE  , NULL, cache_usage  ,   1,  false,  false },
    { OPTION_PATH   , ALIAS_PATH   , NULL, path_usage   ,   1,  false,  false },
    { OPTION_CART   , ALIAS_CART   , NULL, cart_usage   ,   1,  true ,  false },
    { OPTION_NGC    , ALIAS_NGC    , NULL, ngc_usage    ,   1,  true ,  false },
};

const char UsageDefaultName[] = "srapath";


rc_t CC UsageSummary( const char * progname )
{
    return OUTMSG(("\n"
        "Usage:\n"
        "  %s [options] <accession> ...\n\n"
        "Summary:\n"
        "  Tool to produce a list of full paths to files\n"
        "  (SRA and WGS runs, refseqs: reference sequences)\n"
        "  from list of NCBI accessions.\n"
        "\n", progname));
}


rc_t CC Usage( const Args *args )
{
    rc_t rc;
    uint32_t idx, count = ( sizeof ToolOptions ) / ( sizeof ToolOptions[ 0 ] );
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;

    if ( args == NULL )
        rc = RC ( rcExe, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram( args, &fullpath, &progname );

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary( progname );

    OUTMSG((
        "  Output paths are ordered according to accession list.\n"
        "\n"
        "  The accession search path will be determined according to the\n"
        "  configuration. It will attempt to find files in local and site\n"
        "  repositories, and will also check remote repositories for run\n"
        "  location.\n"));
    OUTMSG((
        "  This tool produces a path that is 'likely' to be a run, in that\n"
        "  an entry exists in the file system at the location predicted.\n"
        "  It is possible that this path will fail to produce success upon\n"
        "  opening a run if the path does not point to a valid object.\n\n"));

    OUTMSG(( "Options:\n" ));

    for ( idx = 0; idx < count; ++idx ) {
        /* start with 1, do not advertize row-range-option*/
        const char * param = NULL;
        if (ToolOptions[idx].aliases == NULL) {
            if (strcmp(ToolOptions[idx].name, OPTION_NGC) == 0 ||
                strcmp(ToolOptions[idx].name, OPTION_CART) == 0)
            {
                param = "PATH";
            }
        }

        HelpOptionLine( ToolOptions[ idx ].aliases, ToolOptions[ idx ].name,
            param, ToolOptions[ idx ].help );
        }

    OUTMSG(( "\n" ));
    HelpOptionsStandard();
    HelpVersion( fullpath, KAppVersion() );

    return rc;
}

static rc_t KSrvRun_Print(
    const KSrvRun * self, const char * arg, VQuality preferred)
{
    const VPath * local = NULL;
    const VPath * remote = NULL;
    const VPath * path = NULL;
    const String * tmp = NULL;

    rc_t rc = KSrvRunQuery(self, &local, &remote, NULL, NULL);
    if (rc == 0) {
        if (local != NULL)
            path = local;
        else if (remote != NULL)
            path = remote;
    }

    if (path != NULL) {
        VQuality q = VPathGetQuality(path);
        if (q < eQualLast) {
            if (q == eQualNo || q == eQualFull) {
                char msg[256] = "";
                string_printf(msg, sizeof msg, NULL,
                    "'%s' is an SRA %s file%s:",
                    arg,
                    q == eQualNo ? "Lite" : "Normalized Format",
                    preferred == q ? "" :
                     ", if this is different from your "
                     "preference, it may be due to current file availability");
                STSMSG(1, (msg));
            }
        }
        rc = VPathMakeString(path, &tmp);
        if (rc == 0) {
            OUTMSG(("%S\n", tmp));
            free((void *)tmp);
        }
    }

    RELEASE(VPath, local);
    RELEASE(VPath, remote);
    return rc;
}

static rc_t KSrvRespFile_Print(const KSrvRespFile * self) {
    const VPath * path = NULL;
    const String * tmp = NULL;

    rc_t rc = KSrvRespFileGetLocal(self, &path);
    if (rc != 0) {
        KSrvRespFileIterator * fi = NULL;
        rc = KSrvRespFileMakeIterator(self, &fi);
        if (rc == 0)
            rc = KSrvRespFileIteratorNextPath(fi, &path);
        RELEASE(KSrvRespFileIterator, fi);
    }

    if (path != NULL) {
        rc = VPathMakeString(path, &tmp);
        if (rc == 0) {
            OUTMSG(("%S\n", tmp));
            free((void *)tmp);
        }
    }

    RELEASE(VPath, path);
    return rc;
}

static rc_t resolve_one_argument( VFSManager * mgr, VResolver * resolver,
    const char * pc, const char * location,
    const char * cart, const char * ngc )
{
    bool found = true;
    rc_t rc = 0;

    if ( true ) {
        found = false;

        KService * service = NULL;
        rc = KServiceMake ( & service );
        if ( rc == 0 ) {
            if ( pc != NULL )
                rc = KServiceAddId ( service, pc );
            else if (cart != NULL) {
                rc = KServiceSetJwtKartFile(service, cart);
                if (rc != 0)
                    PLOGERR(klogErr, (klogErr, rc,
                        "cannot use '$(perm)' as jwt cart file",
                        "perm=%s", cart));
            }
            else
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
        }
        if (rc == 0 && location != NULL)
            rc = KServiceSetLocation(service, location);
        if (rc == 0) {
            uint32_t project = 0;
            rc = VResolverGetProject(resolver, &project);
            if (rc == 0 && project != 0)
                rc = KServiceAddProject(service, project);
            if (rc == 0 && ngc != NULL) {
                rc = KServiceSetNgcFile(service, ngc);
                if (rc != 0)
                    PLOGERR(klogErr, (klogErr, rc,
                        "cannot use '$(ngc)' as ngc file",
                        "ngc=%s", ngc));
            }
        }

        if ( rc == 0 ) {
            VQuality q = eQualLast;
            VRemoteProtocols protocol = eProtocolHttps;
            const KSrvResponse * response = NULL;
            rc = KServiceNamesQuery ( service, protocol, & response );

            {
                const char * quality = NULL;
                rc_t r2 = KServiceGetQuality(service, &quality);
                if (r2 != 0) {
                    if (rc == 0)
                        rc = r2;
                }
                else if (quality != NULL) {
                    const char * msg = NULL;
                    switch (quality[0]) {
                    case 'Z':
                        q = eQualNo;
                        msg = "Current preference is set to retrieve SRA "
                            "Lite files with simplified base quality scores.";
                        break;
                    case 'R':
                        q = eQualFull;
                        msg = "Current preference is set to retrieve SRA "
                            "Normalized Format files with full base quality scores.";
                        break;
                    }
                    if (msg != NULL)
                        STSMSG(1, (msg));
                }
            }

            if ( rc == 0 ) {
                KSrvRunIterator * ri = NULL;
                const KSrvRun * run = NULL;
                uint32_t i = 0;
                uint32_t l = KSrvResponseLength  ( response );
                rc = KSrvResponseMakeRunIterator ( response, & ri );
                if ( rc == 0 ) {
                    rc = KSrvRunIteratorNextRun ( ri, & run );
                    if ( rc == 0 && run != NULL ) {
                        rc = KSrvRun_Print ( run, pc, q );
                        found = true;
                    }
                    for ( i = 0; !found && i < l && rc == 0; ++ i ) {
                        const KSrvRespObj * obj = NULL;
                        KSrvRespObjIterator * it = NULL;
                        KSrvRespFile * file = NULL;
                        ESrvFileFormat type = eSFFInvalid;
                        rc = KSrvResponseGetObjByIdx ( response, i, & obj );
                        if ( rc == 0 )
                            rc = KSrvRespObjMakeIterator ( obj, & it );
                        while ( rc == 0 ) {
                            rc_t r2 = 0;
                            rc = KSrvRespObjIteratorNextFile ( it, & file );
                            if ( rc != 0 || file == NULL )
                                break;
                            r2 = KSrvRespFileGetFormat(file, &type);
                            if (r2 != 0 || type != eSFFVdbcache) {
                                rc = KSrvRespFile_Print(file);
                                found = true;
                            }
                            RELEASE ( KSrvRespFile, file );
                        }
                        RELEASE ( KSrvRespObjIterator, it );
                        RELEASE ( KSrvRespObj, obj );
                    }
                }

                RELEASE ( KSrvRun, run );
                RELEASE ( KSrvRunIterator, ri );
                KSrvResponseRelease ( response );
            }
        }
        KServiceRelease ( service );
    }
    else {
        const VPath * upath = NULL;
        rc = VFSManagerMakePath( mgr, ( VPath** )&upath, "%s", pc );
        if ( rc != 0 )
            PLOGMSG( klogErr, ( klogErr,
                "failed to create VPath-object from '$(name)'",
                "name=%s", pc ) );
        else {
            const VPath * local;
            const VPath * remote;

            rc = VResolverQuery( resolver, 0, upath, &local, &remote, NULL );
            if ( rc == 0 ) {
                const String * s;

                if ( local != NULL )
                    rc = VPathMakeString( local, &s );
                else
                    rc = VPathMakeString( remote, &s );
                if ( rc == 0 ) {
                    OUTMSG(("%S\n", s));
                    free( ( void* )s );
                }
                VPathRelease( local );
                VPathRelease( remote );
            }
            else
                found = false;

            VPathRelease( upath );
        }
    }

    if ( ! found ) {
        KDirectory * cwd = NULL;
        rc_t orc = VFSManagerGetCWD( mgr, &cwd );
        if ( orc == 0 ) {
            KPathType kpt = KDirectoryPathType( cwd, "%s", pc );
            switch ( kpt &= ~kptAlias ) {
                    case kptNotFound : STSMSG( 1, ( "'%s': not found while "
                                                   "searching the file system",
                                                     pc ) );
                                        break;

                    case kptBadPath  : STSMSG( 1, ( "'%s': bad path while "
                                                    "searching the file system",
                                                     pc ) );
                                        break;

                    default :          STSMSG( 1, ( "'%s': "
                                                     "found in the file system",
                                                     pc ) );
                                        rc = 0;
                                        break;
            }
        }
        if ( orc == 0 && rc == 0 ) {
                if ( rc != 0 )
                {
                    PLOGMSG( klogErr, ( klogErr, "'$(name)': not found",
                            "name=%s", pc ) );
                }
                else
                {
                    char resolved[ PATH_MAX ] = "";
                    rc = KDirectoryResolvePath( cwd, true, resolved,
                            sizeof resolved, "%s", pc );
                    if ( rc == 0 )
                    {
                        STSMSG( 1, ( "'%s': found in "
                                     "the current directory at '%s'",
                                     pc, resolved ) );
                        OUTMSG(( "%s\n", resolved ));
                    }
                    else
                    {
                        STSMSG( 1, ( "'%s': cannot resolve "
                                     "in the current directory",
                                     pc ) );
                        OUTMSG(( "./%s\n", pc ));
                    }
                }
        }
        KDirectoryRelease( cwd );
    }

    return rc;
}


static rc_t resolve_arguments( Args * args )
{
    uint32_t acount;
    rc_t rc = 0;

    uint32_t idx = 0;
    const char * cart = NULL;
    rc = ArgsOptionCount(args, OPTION_CART, &idx);
    if (rc == 0 && idx > 0)
        rc = ArgsOptionValue(args, OPTION_CART, 0, (const void**)&cart);

    if (rc == 0)
        rc = ArgsParamCount(args, &acount);
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to count parameters" );
    else if ( acount < 1 && cart == NULL )
    {
        MiniUsage( args );
        rc = RC( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
    }
    else
    {
        VFSManager * mgr;
        rc = VFSManagerMake( &mgr );
        if ( rc != 0 )
            LOGERR ( klogErr, rc, "failed to create VFSManager object" );
        else
        {
            VResolver * resolver;
            rc = VFSManagerGetResolver( mgr, &resolver );
            if ( rc != 0 )
                LOGERR ( klogErr, rc, "failed to get VResolver object" );
            else
            {
                rc_t r2 = 0;

                const char * location = get_str_option(args, OPTION_LOCN, NULL);
                const char * ngc = get_str_option(args, OPTION_NGC, NULL);

                rc = ArgsOptionCount ( args, OPTION_PROTO, & idx );
                if ( rc == 0 && idx == 0 )
                    rc = ArgsOptionCount ( args, OPTION_VERS, & idx );
                if ( rc == 0 && idx == 0 )
                    rc = ArgsOptionCount ( args, OPTION_URL, & idx );
                if ( rc == 0 && idx == 0 )
                    rc = ArgsOptionCount ( args, OPTION_PARAM, & idx );
                if ( rc == 0 && idx == 0 )
                    rc = ArgsOptionCount ( args, OPTION_RAW, & idx );
                if ( rc == 0 && idx == 0 )
                    rc = ArgsOptionCount ( args, OPTION_PRJ, & idx );
                if ( rc == 0 && idx == 0 )
                    rc = ArgsOptionCount ( args, OPTION_CACHE, & idx );
                if ( rc == 0 && idx > 0 )
                    LOGMSG ( klogWarn, "all the options are ignored "
                        "when running '" FUNCTION_RESOLVE "' function" );

                for ( idx = 0; rc == 0 && idx < acount; ++ idx )
                {
                    const char * pc;
                    rc = ArgsParamValue( args, idx, ( const void ** )&pc );
                    if ( rc != 0 )
                        LOGERR( klogInt, rc, "failed to retrieve parameter value" );
                    else {
                        rc_t rx = resolve_one_argument(
                            mgr, resolver, pc, location, NULL, ngc );
                        if ( rx != 0 && r2 == 0)
                            r2 = rx;
                    }
                }
                if (cart != NULL) {
                    rc_t rx = resolve_one_argument(
                        mgr, resolver, NULL, location, cart, ngc);
                    if (rx != 0 && r2 == 0)
                        r2 = rx;
                }

                if (r2 != 0 && rc == 0)
                    rc = r2;
                VResolverRelease( resolver );
            }
            VFSManagerRelease( mgr );
        }
    }
    return rc;
}


/* ---------------------------------------------------------------------------- */

static rc_t on_reply_line( const String * line, void * data )
{
    return KOutMsg( "%S\n", line );
}

typedef struct out_fmt
{
    bool raw, cache, path, json;
} out_fmt;


static rc_t prepare_request( const Args * args, request_params * r, out_fmt * fmt,
                             bool for_names )
{
    rc_t rc = 0;

    assert ( r );
    memset ( r, 0, sizeof * r );

    args_to_ptrs( args, &r->terms );
    if ( rc == 0 )
        rc = options_to_ptrs( args, OPTION_PARAM, &r->params );
    if ( rc == 0 )
        rc = options_to_nums ( args, OPTION_PRJ, & r -> projects );

    fmt->cache = get_bool_option( args, OPTION_CACHE );
    fmt->path  = get_bool_option( args, OPTION_PATH  );

    if ( rc == 0 )
    {
        if ( for_names )
        {
            r->names_url  = get_str_option( args, OPTION_URL, NULL );
            r->names_ver  = get_str_option( args, OPTION_VERS, NULL );
            r->proto      = get_str_option( args, OPTION_PROTO, DEF_PROTO );
            r->location   = get_str_option( args, OPTION_LOCN, NULL );
            r->search_url = NULL;
            r->search_ver = NULL;

        }
        else
        {
            uint32_t count = 0;

            r->names_url  = NULL;
            r->names_ver  = NULL;
            r->search_url = get_str_option( args, OPTION_URL, NULL );
            r->search_ver = get_str_option( args, OPTION_VERS, NULL );

            if ( fmt -> cache )
                LOGMSG ( klogWarn, "'--" OPTION_CACHE
                   "' is ignored when running '" FUNCTION_SEARCH "' function" );

            if ( r -> projects != NULL && * r -> projects != 0 )
                LOGMSG ( klogWarn, "'--" OPTION_PRJ
                   "' is ignored when running '" FUNCTION_SEARCH "' function" );

            rc = ArgsOptionCount ( args, OPTION_PROTO, & count );
            if ( rc == 0 && count > 0 )
                LOGMSG ( klogWarn, "'--" OPTION_PROTO
                   "' is ignored when running '" FUNCTION_SEARCH "' function" );
        }

        r->buffer_size = 4096;
        r->timeout_ms = get_uint32_t_option( args, OPTION_TIMEOUT, 5000 );
    }

    fmt->raw = get_bool_option( args, OPTION_RAW );
    fmt->json = get_bool_option( args, OPTION_JSON );
    r->cart = get_str_option(args, OPTION_CART, NULL);
    r->ngc = get_str_option(args, OPTION_NGC, NULL);

    return rc;
}

static void destroy_request ( request_params * self ) {
    assert ( self );

    free ( ( void * ) self -> params   );
    free (            self -> projects );
    free ( ( void * ) self -> terms    );

    memset ( self, 0, sizeof * self );
}

static rc_t names_cgi( const Args * args )
{
    request_params r;

    out_fmt fmt;
    rc_t rc = prepare_request( args, &r, &fmt, true );
    if ( rc == 0 )
    {
        uint32_t rslt_code;

        if ( fmt.raw ) {
            if ( fmt . cache )
                LOGMSG ( klogWarn, "'--" OPTION_CACHE
                   "' is ignored with '--" OPTION_RAW "'" );
            rc = raw_names_request( &r, on_reply_line, &rslt_code, NULL );
        }
        else if (fmt.json) {
            rc = names_request_json ( & r, fmt . cache, fmt . path );
        }
        else
            rc = names_request ( & r, fmt . cache, fmt . path );

        destroy_request ( & r );
    }

    return rc;
}

static rc_t search_cgi( const Args * args )
{
    request_params r;

    out_fmt fmt;
    rc_t rc = prepare_request( args, &r, &fmt, false );
    if ( rc == 0 )
    {
        uint32_t rslt_code;

        if ( fmt.raw )
            rc = raw_search_request( &r, on_reply_line, &rslt_code, NULL );
        else
            rc = search_request ( & r );

        destroy_request ( & r );
    }

    return rc;
}

/* ---------------------------------------------------------------------------- */

rc_t CC KMain( int argc, char *argv [] )
{
    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    Args * args;
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "failed to parse arguments" );
    else
    {
        const char * f = get_str_option( args, OPTION_FUNC, NULL );
        func_t ft = get_func_t( f );
        const char * p = get_str_option( args, OPTION_PROTO, NULL );
        if ( f == NULL && p != NULL )
            ft = ft_names;
        switch ( ft )
        {
            case ft_resolve : rc = resolve_arguments( args ); break;
            case ft_names   : rc = names_cgi( args ); break;
            case ft_search  : rc = search_cgi( args ); break;
        }
        ArgsWhack( args );
    }
    return rc;
}
