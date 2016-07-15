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

#include "helper.h"
#include "cgi_request.h"
#include "line_iter.h"

#include <vfs/resolver.h>
#include <vfs/path.h>
#include <vfs/manager.h>
#include <kfs/directory.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h> /* STSMSG */
#include <klib/rc.h>
#include <klib/vector.h>
#include <sysalloc.h>


#include <limits.h> /* PATH_MAX */

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


static const char * func_usage[] = { "function to perform (resolve, names, search) default=resolve", NULL };
#define OPTION_FUNC   "funtion"
#define ALIAS_FUNC    "f"

static const char * timeout_usage[] = { "timeout-value for request", NULL };
#define OPTION_TIMEOUT "timeout"
#define ALIAS_TIMEOUT "t"

static const char * vers_usage[] = { "version-string for cgi-calls", NULL };
#define OPTION_VERS   "vers"
#define ALIAS_VERS    "e"

static const char * url_usage[] = { "url to be used for cgi-calls", NULL };
#define OPTION_URL    "url"
#define ALIAS_URL     "u"

static const char * param_usage[] = { "param to be added to cgi-call (tic=XXXXX)", NULL };
#define OPTION_PARAM  "param"
#define ALIAS_PARAM   "p"

static const char * raw_usage[] = { "print the raw reply (instead of parsing it)", NULL };
#define OPTION_RAW    "raw"
#define ALIAS_RAW     "r"

static const char * size_usage[] = { "print size of object", NULL };
#define OPTION_SIZE   "size"
#define ALIAS_SIZE    "s"

static const char * date_usage[] = { "print date of object", NULL };
#define OPTION_DATE   "date"
#define ALIAS_DATE    "d"

OptDef ToolOptions[] =
{
    { OPTION_FUNC,      ALIAS_FUNC,      NULL, func_usage,       1,  true,   false },
    { OPTION_TIMEOUT,   ALIAS_TIMEOUT,   NULL, timeout_usage,    1,  true,   false },
    { OPTION_VERS,      ALIAS_VERS,      NULL, vers_usage,       1,  true,   false },
    { OPTION_URL,       ALIAS_URL,       NULL, url_usage,        1,  true,   false },
    { OPTION_PARAM,     ALIAS_PARAM,     NULL, param_usage,      10, true,   false },
    { OPTION_SIZE,      ALIAS_SIZE,      NULL, size_usage,       1,  false,  false },
    { OPTION_DATE,      ALIAS_DATE,      NULL, date_usage,       1,  false,  false },
    { OPTION_RAW,       ALIAS_RAW,       NULL, raw_usage,        1,  false,  false }
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
        rc = RC( rcExe, rcArgv, rcAccessing, rcSelf, rcNull );
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

    for ( idx = 0; idx < count; ++idx ) /* start with 1, do not advertize row-range-option*/
        HelpOptionLine( ToolOptions[ idx ].aliases, ToolOptions[ idx ].name, NULL, ToolOptions[ idx ].help );

    OUTMSG(( "\n" ));
    HelpOptionsStandard();
    HelpVersion( fullpath, KAppVersion() );

    return rc;
}


static rc_t resolve_one_argument( VFSManager * mgr, VResolver * resolver, const char * pc )
{
    const VPath * upath = NULL;
    rc_t rc = VFSManagerMakePath( mgr, ( VPath** )&upath, "%s", pc );
    if ( rc != 0 )
        PLOGMSG( klogErr, ( klogErr, "failed to create VPath-object from '$(name)'", "name=%s", pc ) );
    else
    {
        const VPath * local;
        const VPath * remote;

        rc = VResolverQuery( resolver, eProtocolHttp, upath, &local, &remote, NULL );
        if ( rc == 0 )
        {
            const String * s;

            if ( local != NULL )
                rc = VPathMakeString( local, &s );
            else 
                rc = VPathMakeString( remote, &s );
            if ( rc == 0 )
            {
                OUTMSG(("%S\n", s));
                free( ( void* )s );
            }
            VPathRelease( local );
            VPathRelease( remote );
        }
        else
        {
            KDirectory * cwd;
            rc_t orc = VFSManagerGetCWD( mgr, &cwd );
            if ( orc == 0 )
            {
                KPathType kpt = KDirectoryPathType( cwd, "%s", pc );
                switch ( kpt &= ~kptAlias )
                {
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
            if ( orc == 0 && rc == 0 )
            {
                if ( rc != 0 )
                {
                    PLOGMSG( klogErr, ( klogErr, "'$(name)': not found", "name=%s", pc ) );
                }
                else
                {
                    char resolved[ PATH_MAX ] = "";
                    rc = KDirectoryResolvePath( cwd, true, resolved, sizeof resolved, "%s", pc );
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
    }
    VPathRelease( upath );
    return rc;
}


static rc_t resolve_arguments( Args * args )
{
    uint32_t acount;
    rc_t rc = ArgsParamCount( args, &acount );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to count parameters" );
    else if ( acount < 1 )
    {
        /* That useless: rc = */ MiniUsage( args );
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
                uint32_t idx;
                for ( idx = 0; rc == 0 && idx < acount; ++ idx )
                {
                    const char * pc;
                    rc = ArgsParamValue( args, idx, ( const void ** )&pc );
                    if ( rc != 0 )
                        LOGERR( klogInt, rc, "failed to retrieve parameter value" );
                    else
                        rc = resolve_one_argument( mgr, resolver, pc );
                }
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
    bool raw, print_size, print_date;
} out_fmt;


static rc_t prepare_request( const Args * args, request_params * r, out_fmt * fmt,
                             bool for_names )
{
    rc_t rc = args_to_ptrs( args, &r->terms ); /* helper.c */
    if ( rc == 0 )
        rc = options_to_ptrs( args, OPTION_PARAM, &r->params ); /* helper.c */
    if ( rc == 0 )
    {
        if ( for_names )
        {
            r->names_url  = get_str_option( args, OPTION_URL, NULL ); /* helper.c */
            r->names_ver  = get_str_option( args, OPTION_VERS, NULL ); /* helper.c */
            r->search_url = NULL;
            r->search_ver = NULL;
            
        }
        else
        {
            r->names_url  = NULL;
            r->names_ver  = NULL;
            r->search_url = get_str_option( args, OPTION_URL, NULL ); /* helper.c */
            r->search_ver = get_str_option( args, OPTION_VERS, NULL ); /* helper.c */
        }
        r->buffer_size = 4096;
        r->timeout_ms = get_uint32_t_option( args, OPTION_TIMEOUT, 5000 );
    }
    fmt->raw = get_bool_option( args, OPTION_RAW );
    fmt->print_size = get_bool_option( args, OPTION_SIZE );
    fmt->print_date = get_bool_option( args, OPTION_DATE );
    return rc;
}


/* ---------------------------------------------------------------------------- */

static rc_t print_names_reply( const reply_obj * obj, void * data )
{
    rc_t rc;
    out_fmt * fmt = data;
    
    if ( obj->code == 200 )
    {
        if ( fmt->print_size )
            rc = KOutMsg( "%22S %,12lu --> %S\n", obj->id, obj->size, obj->path );
        else
            rc = KOutMsg( "%22S --> %S\n", obj->id, obj->path );
    }
    else
        rc = KOutMsg( "%22S --> %S (%d)\n", obj->id, obj->message, obj->code );

    return rc;
}

static rc_t names_cgi( const Args * args )
{
    request_params r; /* cgi_request.h */
    out_fmt fmt;
    rc_t rc = prepare_request( args, &r, &fmt, true );
    if ( rc == 0 )
    {
        uint32_t rslt_code;

        if ( fmt.raw )
            rc = raw_names_request( &r, on_reply_line, &rslt_code, NULL ); /* cgi_request.c */
        else
        {
            struct reply_obj_list * list; /* cgi_request.h */
            rc = names_request_to_list( &r, &rslt_code, &list ); /* cgi_request.c */
            if ( rc == 0 )
            {
                rc = foreach_reply_obj( list, print_names_reply, &fmt ); /* cgi_request.c */
                release_reply_obj_list( list ); /* cgi_request.c */
            }
        }
        
        free( ( void * ) r.terms );
        free( ( void * ) r.params );
    }
    return rc;
}

/* ---------------------------------------------------------------------------- */

static rc_t print_search_reply( const reply_obj * obj, void * data )
{
    return KOutMsg( "(%S) %S --> %S\n", obj->obj_type, obj->id, obj->path );
}

static rc_t search_cgi( const Args * args )
{
    request_params r; /* cgi_request.h */
    out_fmt fmt;
    rc_t rc = prepare_request( args, &r, &fmt, false );
    if ( rc == 0 )
    {
        uint32_t rslt_code;

        if ( fmt.raw )
            rc = raw_search_request( &r, on_reply_line, &rslt_code, NULL ); /* cgi_request.c */
        else
        {
            struct reply_obj_list * list; /* cgi_request.h */
            rc = search_request_to_list( &r, &rslt_code, &list, true ); /* cgi_request.c */
            if ( rc == 0 )
            {
                rc = foreach_reply_obj( list, print_search_reply, &fmt ); /* cgi_request.c */
                release_reply_obj_list( list ); /* cgi_request.c */
            }
        }

        free( ( void * ) r.terms );
        free( ( void * ) r.params );
    }
    return rc;
}

/* ---------------------------------------------------------------------------- */

rc_t CC KMain( int argc, char *argv [] )
{
    Args * args;
    uint32_t num_options = sizeof ToolOptions / sizeof ToolOptions [ 0 ];
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1, ToolOptions, num_options );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "failed to parse arguments" );
    else
    {
        const char * f = get_str_option( args, OPTION_FUNC, NULL );
        func_t ft = get_func_t( f );
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
