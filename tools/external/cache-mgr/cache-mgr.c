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

#include <kapp/main.h>
#include <kapp/args.h>

#include <klib/container.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/debug.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/rc.h>
#include <klib/namelist.h>

#include <kfg/config.h>
#include <kfg/repository.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/cacheteefile.h>
#include <kfs/defs.h>

#include <os-native.h>
#include <sysalloc.h>
#include <strtol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static const char * report_usage[]  = { "report objects in cache", NULL };
static const char * rreport_usage[] = { "report status of repositories", NULL };
static const char * unlock_usage[]  = { "remove lock-files", NULL };
static const char * clear_usage[]   = { "clear cache", NULL };
static const char * enable_usage[]  = { "enable repository [user/site/rem]", NULL };
static const char * disable_usage[] = { "disable repository [user/site/rem]", NULL };
static const char * detail_usage[]  = { "show detailed report", NULL };
static const char * tstzero_usage[] = { "test for zero blocks (for report function)", NULL };
static const char * urname_usage[]  = { "restrict to this user-repository", NULL };
static const char * max_rem_usage[] = { "remove until reached that many bytes", NULL };
static const char * rem_dir_usage[] = { "remove directories, not only files", NULL };

#define OPTION_CREPORT  "report"
#define ALIAS_CREPORT   "r"

#define OPTION_UNLOCK   "unlock"
#define ALIAS_UNLOCK    "u"

#define OPTION_CLEAR    "clear"
#define ALIAS_CLEAR     "c"

#define OPTION_ENABLE   "enable"
#define ALIAS_ENABLE    "e"

#define OPTION_DISABLE  "disable"
#define ALIAS_DISABLE   "d"

#define OPTION_RREPORT  "rep-report"
#define ALIAS_RREPORT   "o"

#define OPTION_DETAIL   "details"
#define ALIAS_DETAIL    "t"

#define OPTION_URNAME   "user-repo-name"
#define ALIAS_URNAME    "p"

#define OPTION_MAXREM   "max-remove"
#define ALIAS_MAXREM    "m"

#define OPTION_REMDIR   "remove-dirs"
#define ALIAS_REMDIR    "i"

#define OPTION_TSTZERO  "test-zero"
#define ALIAS_TSTZERO   "z"

OptDef ToolOptions[] =
{
    { OPTION_CREPORT,   ALIAS_CREPORT,  NULL,   report_usage,   1,  false,  false },
    { OPTION_RREPORT,   ALIAS_RREPORT,  NULL,   rreport_usage,  1,  false,  false },
    { OPTION_DETAIL,    ALIAS_DETAIL,   NULL,   detail_usage,   1,  false,  false },
    { OPTION_TSTZERO,   ALIAS_TSTZERO,  NULL,   tstzero_usage,  1,  false,  false },
    { OPTION_UNLOCK,    ALIAS_UNLOCK,   NULL,   unlock_usage,   1,  false,  false },
    { OPTION_CLEAR,     ALIAS_CLEAR,    NULL,   clear_usage,    1,  false,  false },
    { OPTION_MAXREM,    ALIAS_MAXREM,   NULL,   max_rem_usage,  1,  true,   false },
    { OPTION_REMDIR,    ALIAS_REMDIR,   NULL,   rem_dir_usage,  1,  false,  false },
    { OPTION_ENABLE,    ALIAS_ENABLE,   NULL,   enable_usage,   1,  true,   false },
    { OPTION_DISABLE,   ALIAS_DISABLE,  NULL,   disable_usage,  1,  true,   false },
    { OPTION_URNAME,    ALIAS_URNAME,   NULL,   urname_usage,   1,  true,   false }
};

const char UsageDefaultName[] = "cache-mgr";


rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [path_to_cache] options\n"
                    "\n", progname);
}


rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
    {
        rc = ArgsProgram ( args, &fullpath, &progname );

        if ( rc != 0 )
            progname = fullpath = UsageDefaultName;

        rc = UsageSummary ( progname );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "UsageSummary() failed in $(func)", "func=%s", __func__ ) );

        }
        else
            rc = KOutMsg ( "Options:\n" );

        if ( rc == 0 )
        {
            uint32_t idx, count = sizeof ToolOptions / sizeof ToolOptions [ 0 ];
            for ( idx = 0; idx < count; ++idx )
            {
                OptDef * o = &ToolOptions[ idx ];
                HelpOptionLine ( o->aliases, o->name, NULL, o->help );
            }
        }

        if ( rc == 0 )
            rc = KOutMsg ( "\n" );

        if ( rc == 0 )
        {
            HelpOptionsStandard ();
            HelpVersion ( fullpath, KAppVersion() );
        }
    }
    return rc;
}

typedef enum tool_main_function
{
    tf_report,
    tf_rreport,
    tf_unlock,
    tf_clear,
    tf_enable,
    tf_disable,
    tf_unknown
} tool_main_function;


typedef struct tool_options
{
    uint64_t max_remove;

    VNamelist * paths;
    const char * user_repo_name;

    uint32_t path_count;
    tool_main_function main_function;
    KRepCategory category;

    bool detailed;
    bool tstzero;
    bool remove_dirs;
} tool_options;


static rc_t init_tool_options( tool_options * options )
{
    rc_t rc = VNamelistMake ( &options->paths, 5 );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "VNamelistMake() failed in $(func)", "func=%s", __func__ ) );
    }
    else
    {
        options->path_count = 0;
        options->main_function = tf_unknown;
        options->user_repo_name = NULL;
        options->max_remove = 0;
    }
    return rc;
}


static rc_t clear_tool_options( tool_options * options )
{
    rc_t rc = VNamelistRelease ( options->paths );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "VNamelistRelease() failed in $(func)", "func=%s", __func__ ) );
    }

    if ( options->user_repo_name != NULL )
    {
        free( (void*) options->user_repo_name );
        options->user_repo_name = NULL;
    }
    return rc;
}


static bool get_bool_option( const Args * args, const char * name )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "ArgsOptionCount() failed in $(func)", "func=%s", __func__ ) );
        return false;
    }
    return ( count > 0 );
}


static bool string_cmp_1( const char * s1, const char * s2 )
{
    int cmp = string_cmp( s1, string_size( s1 ), s2, string_size( s2 ), -1 );
    return ( cmp == 0 );
}


static KRepCategory get_repo_select( const Args * args, const char * name )
{
    KRepCategory res = krepBadCategory;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "ArgsOptionCount() failed in $(func)", "func=%s", __func__ ) );
    }
    else if ( count > 0 )
    {
        const char * s = NULL;
        rc = ArgsOptionValue( args, name, 0, (const void **)&s );
        if ( rc == 0 && s != NULL )
        {
            if ( string_cmp_1 ( s, "user" ) )
                res = krepUserCategory;
            else if ( string_cmp_1 ( s, "site" ) )
                res = krepSiteCategory;
            else if ( string_cmp_1 ( s, "rem" ) )
                res = krepRemoteCategory;
        }
    }
    return res;
}


static rc_t get_user_repo_name( const Args * args, const char ** name )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, OPTION_URNAME, &count );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "ArgsOptionCount( $(option) ) failed in $(func)", "option=%s,func=%s", OPTION_URNAME, __func__ ) );
    }
    else if ( count > 0 )
    {
        const char * s = NULL;
        rc = ArgsOptionValue( args, OPTION_URNAME, 0, (const void **)&s );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "ArgsOptionValue( $(option), 0 ) failed in $(func)", "option=%s,func=%s", OPTION_URNAME, __func__ ) );
        }
        else if ( s != NULL )
            *name = string_dup_measure ( s, NULL );
    }
    return rc;
}


static rc_t get_max_remove( const Args * args, uint64_t * max_rem )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, OPTION_MAXREM, &count );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "ArgsOptionCount( $(option) ) failed in $(func)", "option=%s,func=%s", OPTION_MAXREM, __func__ ) );
    }
    else if ( count > 0 )
    {
        const char * s = NULL;
        rc = ArgsOptionValue( args, OPTION_MAXREM, 0, (const void **)&s );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "ArgsOptionValue( $(option), 0 ) failed in $(func)", "option=%s,func=%s", OPTION_MAXREM, __func__ ) );
        }
        else if ( s != NULL )
        {
            char *endp;
            *max_rem = strtou64( s, &endp, 10 );
        }
    }
    return rc;
}


static rc_t add_tool_options_path( tool_options * options, const char * path )
{
    rc_t rc = VNamelistAppend ( options->paths, path );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "VNamelistAppend( $(name) ) failed in $(func)", "name=%s, func=%s", path, __func__ ) );
    }
    if ( rc == 0 )
        options->path_count++;
    return rc;
}


static rc_t get_tool_options( Args * args, tool_options * options )
{
    uint32_t idx, count;

    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "ArgsParamCount() failed in $(func)", "func=%s", __func__ ) );
    }
    else if ( count > 0 )
    {
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char *value = NULL;
            rc = ArgsParamValue( args, idx, (const void **)&value );
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "ArgsParamValue( $(idx) ) failed in $(func)", "idx=%d,func=%s", idx, __func__ ) );
            }
            else if ( value != NULL )
                rc = add_tool_options_path( options, value );
        }
    }

    options->main_function = tf_unknown;
    options->detailed = get_bool_option( args, OPTION_DETAIL );
    options->tstzero = get_bool_option( args, OPTION_TSTZERO );
    options->remove_dirs = get_bool_option( args, OPTION_REMDIR );

    if ( get_bool_option( args, OPTION_CREPORT ) )
        options->main_function = tf_report;
    else if ( get_bool_option( args, OPTION_RREPORT ) )
        options->main_function = tf_rreport;
    else if ( get_bool_option( args, OPTION_UNLOCK ) )
        options->main_function = tf_unlock;
    else if ( get_bool_option( args, OPTION_CLEAR ) )
        options->main_function = tf_clear;
    else
    {
        options->category = get_repo_select( args, OPTION_ENABLE );
        if ( options->category != krepBadCategory )
            options->main_function = tf_enable;

        if ( options->category == krepBadCategory )
        {
            options->category = get_repo_select( args, OPTION_DISABLE );
            if ( options->category != krepBadCategory )
                options->main_function = tf_disable;
        }
    }

    if ( rc == 0 )
        rc = get_user_repo_name( args, &options->user_repo_name );
    if ( rc == 0 )
        rc = get_max_remove( args, &options->max_remove );
    return rc;
}


typedef struct visit_ctx
{
    KDirectory * dir;
    KConfig * cfg;
    const KRepositoryMgr * repo_mgr;

    const tool_options * options;
    const char * path;
    void * data;
    uint32_t path_type;
    bool terminate;
} visit_ctx;


typedef rc_t ( *on_path_t )( visit_ctx * obj );


static rc_t foreach_path_obj( visit_ctx * ctx, on_path_t func )
{
    rc_t rc = 0;
    ctx->path_type = ( KDirectoryPathType ( ctx->dir, "%s", ctx->path ) & ~ kptAlias );
    if ( ctx->path_type == kptDir )
    {
        KNamelist * path_objects;
        rc = KDirectoryList ( ctx->dir, &path_objects, NULL, NULL, "%s", ctx->path );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KDirectoryList( $(path) ) failed in $(func)", "path=%s,func=%s", ctx->path, __func__ ) );
        }
        else
        {
            uint32_t idx, count;
            rc = KNamelistCount ( path_objects, &count );
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "KNamelistCount() failed in $(func)", "func=%s", __func__ ) );
            }
            for ( idx = 0; idx < count && rc == 0 && !ctx->terminate; ++idx )
            {
                const char * obj_name = NULL;
                rc = KNamelistGet ( path_objects, idx, &obj_name );
                if ( rc != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc,
                             "KNamelistGet( $(idx) ) failed in $(func)", "idx=%d,func=%s", idx, __func__ ) );
                }
                else if ( obj_name != NULL )
                {
                    char obj_path[ 4096 ];
                    rc = KDirectoryResolvePath ( ctx->dir, true, obj_path, sizeof obj_path, "%s/%s", ctx->path, obj_name );
                    if ( rc != 0 )
                    {
                        PLOGERR( klogErr, ( klogErr, rc,
                                 "KDirectoryResolvePath( $(path) ) failed in $(func)", "path=%s,func=%s", ctx->path, __func__ ) );
                    }
                    else
                    {
                        visit_ctx octx;
                        octx.dir = ctx->dir;
                        octx.options = ctx->options;
                        octx.path = obj_path;
                        octx.data = ctx->data;
                        octx.path_type = ( KDirectoryPathType ( ctx->dir, "%s", obj_path ) & ~ kptAlias );
                        if ( octx.path_type == kptDir )
                        {
                            rc = foreach_path_obj( &octx, func );   /* recursion !!! */
                        }
                        else if ( octx.path_type == kptFile )
                        {
                            rc = func( &octx );
                        }
                    }
                }
            }

            {
                rc_t rc2 = KNamelistRelease ( path_objects );
                if ( rc2 != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc2,
                             "KNamelistRelease() failed in $(func)", "func=%s", __func__ ) );
                }
            }
        }
    }
    if ( rc == 0 && ( ctx->path_type == kptDir || ctx->path_type == kptFile ) && !ctx->terminate )
    {
        /* at the very end of it, call the function for the path itself */
        rc = func( ctx );
    }
    return rc;
}


static rc_t foreach_path( visit_ctx * octx, on_path_t func )
{
    rc_t rc = 0;
    uint32_t idx;

    octx->terminate = false;
    for ( idx = 0; idx < octx->options->path_count && rc == 0 && !octx->terminate; ++idx )
    {
        rc = VNameListGet ( octx->options->paths, idx, &octx->path );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "VNameListGet( $(idx) ) failed in $(func)", "idx=%u,func=%s", idx, __func__ ) );
        }
        else if ( octx->path != NULL )
        {
            rc = foreach_path_obj( octx, func );
        }
    }
    return rc;
}


static bool string_ends_in( const char * str, const char * end )
{
    bool res = false;
    if ( str != NULL && end != NULL )
    {
        uint32_t l_str = string_len( str, string_size( str ) );
        uint32_t l_end = string_len( end, string_size( end ) );
        if ( l_str >= l_end && l_end > 0 )
        {
            const char * p = str + ( l_str - l_end );
            res = ( string_cmp ( p, l_end, end, l_end, l_end ) == 0 );
        }
    }
    return res;
}


/***************************************************************************************************************/

typedef struct report_data
{
    uint32_t partial_count;
    uint32_t full_count;
    uint32_t lock_count;
    uint64_t file_size;
    uint64_t used_file_size;
} report_data;


static rc_t on_report_cache_file( visit_ctx * obj )
{
    rc_t rc = 0;
    uint64_t file_size = 0;
    uint64_t used_size = 0;
    uint64_t checked_blocks = 0;
    uint64_t empty_blocks = 0;
    float completeness = 0.0;

    bool locked = false;
    report_data * data = obj->data;

    data->partial_count++;
    rc = KDirectoryFileSize ( obj->dir, &file_size, "%s", obj->path );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "KDirectoryFileSize( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
    }
    else
    {
        struct KFile const *f;

        data->file_size += file_size;
        rc = KDirectoryOpenFileRead ( obj->dir, &f, "%s", obj->path );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KDirectoryOpenFileRead( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
        }
        else
        {
            rc = GetCacheCompleteness( f, &completeness, &used_size ); /* libs/kfs/cacheteefile.c */
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "GetCacheCompleteness( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
            }
            else
            {
                data->used_file_size += used_size;
            }

            if ( rc == 0 && obj->options->tstzero )
            {
                rc = Has_Cache_Zero_Blocks( f, &checked_blocks, &empty_blocks ); /* libs/kfs/cacheteefile.c */
                if ( rc != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc,
                             "Has_Cache_Zero_Blocks( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
                }
            }

            KFileRelease( f );
        }
    }
    if ( rc == 0 )
    {
        uint32_t pt = ( KDirectoryPathType ( obj->dir, "%s.lock", obj->path ) & ~ kptAlias );
        locked = ( pt == kptFile );
    }

    if ( rc == 0 && obj->options->detailed )
    {
        if ( locked )
            rc = KOutMsg( "%s complete by %.02f %% [%,lu of %,lu]bytes locked\n",
                          obj->path, completeness, used_size, file_size );
        else
            rc = KOutMsg( "%s complete by %.02f %% [%,lu of %,lu]bytes\n",
                          obj->path, completeness, used_size, file_size );
    }

    if ( rc == 0 && obj->options->tstzero )
    {
        rc = KOutMsg( "%s has %lu blocks set in bitmap where %lu are empty\n",
                          obj->path, checked_blocks, empty_blocks );
    }

    return rc;
}


static rc_t on_report_full_file( visit_ctx * obj )
{
    rc_t rc = 0;
    uint64_t file_size = 0;
    report_data * data = obj->data;

    data->full_count++;
    rc = KDirectoryFileSize ( obj->dir, &file_size, "%s", obj->path );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "KDirectoryFileSize( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
    }
    else
    {
        data->file_size += file_size;
        data->used_file_size += file_size;
        if ( obj->options->detailed )
            rc = KOutMsg( "%s complete file of %,lu bytes\n", obj->path, file_size );
    }
    return rc;
}


static rc_t on_report_path( visit_ctx * obj )
{
    rc_t rc = 0;
    if ( obj->path_type == kptFile )
    {
        report_data * data = obj->data;
        if ( string_ends_in( obj->path, ".cache" ) )
        {
            rc = on_report_cache_file( obj );
        }
        else if ( string_ends_in( obj->path, ".lock" ) )
        {
            data->lock_count ++;
        }
        else if ( string_ends_in( obj->path, ".sra" ) )
        {
            rc = on_report_full_file( obj );
        }
    }
    return rc;
}


static rc_t perform_report( visit_ctx * octx )
{
    rc_t rc = 0;
    report_data data;

    memset( &data, 0, sizeof data );
    octx->data = &data;

    if ( octx->options->detailed )
        rc = KOutMsg( "\n-----------------------------------\n" );
    if ( rc == 0 )
        rc = foreach_path( octx, on_report_path );

    if ( rc == 0 )
        rc = KOutMsg( "-----------------------------------\n" );
    if ( rc == 0 )
        rc = KOutMsg( "%,u cached file(s)\n", data.partial_count );
    if ( rc == 0 )
        rc = KOutMsg( "%,u complete file(s)\n", data.full_count );
    if ( rc == 0 )
        rc = KOutMsg( "%,lu bytes in cached files\n", data.file_size );
    if ( rc == 0 )
        rc = KOutMsg( "%,lu bytes used in cached files\n", data.used_file_size );
    if ( rc == 0 )
        rc = KOutMsg( "%,u lock files\n", data.lock_count );

    return rc;
}


/***************************************************************************************************************/
typedef rc_t ( CC * get_repo_cb )( const KRepositoryMgr *self, KRepositoryVector *repos );

const char MAIN_CAT_USER[] = "user";
const char MAIN_CAT_SITE[] = "site";
const char MAIN_CAT_REMOTE[] = "remote";

const char SUB_CAT_UNKNOWN[] = "unknown";
const char SUB_CAT_MAIN[] = "main";
const char SUB_CAT_AUX[]  = "aux";
const char SUB_CAT_PROT[] = "protected";


static rc_t report_repo( visit_ctx * octx, KRepCategory category )
{
    rc_t rc, rc1;
    KRepositoryVector repos;
    const char * hint;

    VectorInit ( &repos, 0, 5 );

    switch ( category )
    {
        case krepUserCategory   : hint = MAIN_CAT_USER;
                                   rc = KRepositoryMgrUserRepositories( octx->repo_mgr, &repos );
                                   break;

        case krepSiteCategory   : hint = MAIN_CAT_SITE;
                                   rc = KRepositoryMgrSiteRepositories( octx->repo_mgr, &repos );
                                   break;

        case krepRemoteCategory : hint = MAIN_CAT_REMOTE;
                                   rc = KRepositoryMgrRemoteRepositories( octx->repo_mgr, &repos );
                                   break;
    }

    if ( rc != 0 )
    {
        if ( rc == SILENT_RC( rcKFG, rcNode, rcOpening, rcPath, rcNotFound ) )
        {
            KOutMsg("\n%s:\n", hint);
            KOutMsg("\tnot found in configuration\n");
            rc = 0;
        }
        else
        {
            PLOGERR( klogErr, ( klogErr, rc,
                 "KRepositoryMgr<$(hint)>repositories() failed in $(func)",
                 "hint=%s,func=%s", hint, __func__ ) );
        }
    }
    else
    {
        uint32_t idx;
        bool disabled = KRepositoryMgrCategoryDisabled ( octx->repo_mgr, category );

        rc = KOutMsg( "\n%s:\n", hint );
        if ( rc == 0 && disabled )
            rc = KOutMsg( "\tglobally disabled\n" );

        for ( idx = 0; idx < VectorLength( &repos ) && rc == 0; ++idx )
        {
            const KRepository *repo = VectorGet( &repos, idx );
            if ( repo != NULL )
            {
                char repo_name[ 1024 ];
                rc = KRepositoryDisplayName ( repo, repo_name, sizeof repo_name, NULL );
                if ( rc != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc,
                             "KRepositoryName() for $(hint) failed in $(func)", "hint=%s,func=%s", hint, __func__ ) );
                }
                else
                {
                    KRepSubCategory sub_cat = KRepositorySubCategory ( repo );
                    bool disabled = KRepositoryDisabled ( repo );
                    bool cache_enabled = KRepositoryCacheEnabled ( repo );
                    const char * sub_cat_ptr = SUB_CAT_UNKNOWN;
                    switch( sub_cat )
                    {
                        case krepMainSubCategory        : sub_cat_ptr = SUB_CAT_MAIN; break;
                        case krepAuxSubCategory         : sub_cat_ptr = SUB_CAT_AUX; break;
                        case krepProtectedSubCategory   : sub_cat_ptr = SUB_CAT_PROT; break;
                        default                         : sub_cat_ptr = SUB_CAT_UNKNOWN; break;
                    }

                    rc = KOutMsg( "\t%s.%s: %s, cache-%s",
                            sub_cat_ptr, repo_name,
                            ( disabled ? "disabled" : "enabled" ),
                            ( cache_enabled ? "enabled" : "disabled" ) );
                    if ( rc == 0 )
                    {
                        if ( octx->options->detailed )
                        {
                            /* it is OK if we cannot find the root of a repository... */
                            char where[ 4096 ];
                            rc1 = KRepositoryRoot ( repo, where, sizeof where, NULL );
                            if ( rc1 == 0 )
                                rc = KOutMsg( ", at %s", where );
                            else
                            {
                                rc1 = KRepositoryResolver ( repo, where, sizeof where, NULL );
                                if ( rc1 == 0 )
                                    rc = KOutMsg( ", at %s", where );
                            }
                        }
                                            }
                    if ( rc == 0 )
                        rc = KOutMsg( "\n" );
                }
            }
        }
    }
    {
        rc1 = KRepositoryVectorWhack ( &repos );
        if ( rc1 != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc1,
                     "KRepositoryVectorWhack() for $(hint) failed in $(func)", "hint=%s,func=%s", hint, __func__ ) );
        }
    }
    return rc;
}


static rc_t perform_rreport( visit_ctx * octx )
{
    rc_t rc = report_repo( octx, krepUserCategory );

    if ( rc == 0 )
        rc = report_repo( octx, krepSiteCategory );

    if ( rc == 0 )
        rc = report_repo( octx, krepRemoteCategory );

    if ( rc == 0 )
        rc = KOutMsg( "\n" );

    return rc;
}


/***************************************************************************************************************/


typedef struct unlock_data
{
    uint32_t lock_count;
} unlock_data;


static rc_t on_unlock_path( visit_ctx * obj )
{
    rc_t rc = 0;
    if ( obj->path_type == kptFile && string_ends_in( obj->path, ".lock" ) )
    {
        unlock_data * data = obj->data;
        rc = KDirectoryRemove ( obj->dir, false, "%s", obj->path );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KDirectoryRemove( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
        }
        else
        {
            data->lock_count++;
            rc = KOutMsg( "%s deleted\n", obj->path );
        }
    }
    return rc;
}


static rc_t perform_unlock( visit_ctx * octx )
{
    rc_t rc = 0;
    unlock_data data;

    memset( &data, 0, sizeof data );
    octx->data = &data;
    if ( rc == 0 )
        rc = foreach_path( octx, on_unlock_path );

    if ( rc == 0 )
        rc = KOutMsg( "-----------------------------------\n" );
    if ( rc == 0 )
        rc = KOutMsg( "%,u lock-files removed\n", data.lock_count );

    return rc;
}


/***************************************************************************************************************/


typedef struct clear_data
{
    uint32_t removed_files;
    uint32_t removed_directories;
    uint64_t removed_size;
} clear_data;


static rc_t on_clear_path( visit_ctx * obj )
{
    rc_t rc = 0;
    clear_data * data = obj->data;

    /* the caller is written to first hit the files/subdirs, then the directory that contains them */
    if ( obj->path_type == kptDir )
    {
        if ( obj->options->remove_dirs )
        {
            rc = KDirectoryRemove ( obj->dir, false, "%s", obj->path );
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "KDirectoryRemove( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
            }
            else
                data->removed_directories++;
        }
    }
    else if ( obj->path_type == kptFile )
    {
        uint64_t file_size;
        rc = KDirectoryFileSize ( obj->dir, &file_size, "%s", obj->path );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KDirectoryFileSize( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
        }
        else
        {
            rc = KDirectoryRemove ( obj->dir, false, "%s", obj->path );
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "KDirectoryRemove( $(path) ) failed in $(func)", "path=%s,func=%s", obj->path, __func__ ) );
            }
            else
            {
                if ( obj->options->detailed )
                    rc = KOutMsg( "FILE: '%s' removed (%,lu bytes)\n", obj->path, file_size );
                data->removed_files++;
                data->removed_size += file_size;
                if ( obj->options->max_remove > 0 && data->removed_size >= obj->options->max_remove )
                {
                    obj->terminate = true;
                    KOutMsg( "the maximum of %,lu bytes to be removed is reached now\n", obj->options->max_remove );
                }
            }
        }
    }
    return rc;
}


static rc_t perform_clear( visit_ctx * octx )
{
    rc_t rc = 0;
    clear_data data;

    memset( &data, 0, sizeof data );
    octx->data = &data;

    if ( octx->options->max_remove > 0 )
        rc = KOutMsg( "removing max %,u bytes\n", octx->options->max_remove );
    if ( rc == 0 )
        rc = foreach_path( octx, on_clear_path );

    if ( rc == 0 )
        rc = KOutMsg( "-----------------------------------\n" );
    if ( rc == 0 )
        rc = KOutMsg( "%,u files removed\n", data.removed_files );
    if ( rc == 0 )
        rc = KOutMsg( "%,u directories removed\n", data.removed_directories );
    if ( rc == 0 )
        rc = KOutMsg( "%,lu bytes removed\n", data.removed_size );

    return rc;
}


/***************************************************************************************************************/

/*
static rc_t enable_disable_repo( bool disabled, get_repo_cb getter, const char * hint )
{
    KConfig * cfg;
    rc_t rc = KConfigMake ( &cfg, NULL );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "KConfigMake() failed in $(func)", "func=%s", __func__ ) );
    }
    else
    {
        const KRepositoryMgr *repo_mgr;
        rc = KConfigMakeRepositoryMgrRead ( cfg, &repo_mgr );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KConfigMakeRepositoryMgrRead() failed in $(func)", "func=%s", __func__ ) );
        }
        else
        {
            uint32_t idx;
            KRepositoryVector repos;
            VectorInit ( &repos, 0, 5 );
            rc = getter ( repo_mgr, &repos );
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "KRepositoryMgr<$(hint)>repositories() failed in $(func)", "hint=%s,func=%s", hint, __func__ ) );
            }
            for ( idx = 0; idx < VectorLength( &repos ) && rc == 0; ++idx )
            {
                const KRepository *repo = VectorGet( &repos, idx );
                if ( repo != NULL )
                {
                    rc = KRepositorySetDisabled ( repo, disabled );
                    if ( rc != 0 )
                    {
                        PLOGERR( klogErr, ( klogErr, rc,
                                 "KRepositorySetDisabled() failed in $(func)", "func=%s", __func__ ) );
                    }
                }
            }
            if ( rc == 0 )
            {
                if ( disabled )
                    KOutMsg( "%s repositories disabled\n", hint );
                else
                    KOutMsg( "%s repositories enabled\n", hint );
            }
            {
                rc_t rc2 = KRepositoryVectorWhack ( &repos );
                if ( rc2 != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc2,
                             "KRepositoryVectorWhack() failed in $(func)", "func=%s", __func__ ) );
                }
                rc2 = KRepositoryMgrRelease ( repo_mgr );
                if ( rc2 != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc2,
                             "KRepositoryMgrRelease() failed in $(func)", "func=%s", __func__ ) );
                }

            }
        }

        {
            rc_t rc2 = KConfigRelease ( cfg );
            if ( rc2 != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc2,
                         "KConfigRelease() failed in $(func)", "func=%s", __func__ ) );
            }
        }
    }
    return rc;
}


static rc_t enable_disable_user_repo( bool disabled )
{
    return enable_disable_repo( disabled, KRepositoryMgrUserRepositories, "user" );
}


static rc_t enable_disable_site_repo( bool disabled )
{
    return enable_disable_repo( disabled, KRepositoryMgrSiteRepositories, "site" );
}


static rc_t enable_disable_remote_repo( bool disabled )
{
    return enable_disable_repo( disabled, KRepositoryMgrRemoteRepositories, "remote" );
}
*/

/***************************************************************************************************************/


static rc_t perform_set_disable( visit_ctx * octx, bool disabled )
{
    rc_t rc;
    if ( octx->options->category == krepBadCategory )
        rc = KOutMsg( "enable unknown category\n" );
    else
    {
        rc = KRepositoryMgrCategorySetDisabled ( octx->repo_mgr, octx->options->category, disabled );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KRepositoryMgrCategorySetDisabled() failed in $(func)", "func=%s", __func__ ) );
        }
        else
        {
            rc = KConfigCommit ( octx->cfg );
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "KConfigCommit() failed in $(func)", "func=%s", __func__ ) );
            }
        }
    }
    return rc;
}


/***************************************************************************************************************/


static rc_t perform( tool_options * options, Args * args )
{
    visit_ctx octx;
    rc_t rc2, rc = KDirectoryNativeDir( &octx.dir );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "KDirectoryNativeDir() failed in $(func)", "func=%s", __func__ ) );
    }
    else
    {
        rc = KConfigMake ( &octx.cfg, NULL );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KConfigMake() failed in $(func)", "func=%s", __func__ ) );
        }
        else
        {
            rc = KConfigMakeRepositoryMgrRead ( octx.cfg, &octx.repo_mgr );
            if ( rc != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc,
                         "KConfigMakeRepositoryMgrRead() failed in $(func)", "func=%s", __func__ ) );
            }
            else
            {
                octx.options = options;
                octx.data = NULL;
                switch( options->main_function )
                {
                    case tf_report  : rc = perform_report( &octx ); break;
                    case tf_rreport : rc = perform_rreport( &octx ); break;
                    case tf_unlock  : rc = perform_unlock( &octx ); break;
                    case tf_clear   : rc = perform_clear( &octx ); break;
                    case tf_enable  : rc = perform_set_disable( &octx, false ); break;
                    case tf_disable : rc = perform_set_disable( &octx, true ); break;
                    case tf_unknown : rc = Usage( args ); break;
                }

                rc2 = KRepositoryMgrRelease ( octx.repo_mgr );
                if ( rc2 != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc2,
                             "KRepositoryMgrRelease() failed in $(func)", "func=%s", __func__ ) );
                }
            }

            rc2 = KConfigRelease ( octx.cfg );
            if ( rc2 != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc2,
                         "KConfigRelease() failed in $(func)", "func=%s", __func__ ) );
            }
        }

        rc2 = KDirectoryRelease( octx.dir );
        if ( rc2 != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc2,
                     "KDirectoryRelease() failed in $(func)", "func=%s", __func__ ) );
        }
    }
    return rc;
}


/***************************************************************************
    Main:
***************************************************************************/

static rc_t CC on_history_path( const String * part, void *data )
{
    tool_options * options = data;
    rc_t rc = add_tool_options_path( options, part->addr );
    if ( options -> detailed )
        KOutMsg( "source: %S\n", part );
    return rc;
}

static rc_t get_cache_path_from_repo_mgr( tool_options * options )
{
    KConfig * cfg;
    rc_t rc = KConfigMake ( &cfg, NULL );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "KConfigMake() failed in $(func)", "func=%s", __func__ ) );
    }
    else
    {
        const KRepositoryMgr *repo_mgr;
        rc = KConfigMakeRepositoryMgrRead ( cfg, &repo_mgr );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "KConfigMakeRepositoryMgrRead() failed in $(func)", "func=%s", __func__ ) );
        }
        else
        {
            KRepositoryVector repos;
            VectorInit ( &repos, 0, 5 );
            rc = KRepositoryMgrUserRepositories ( repo_mgr, &repos );
            if ( rc != 0 )
            {
                rc = 0; /* we do not have a user-repository, but that is OK, the caller will display the lack of it ... */
            }
            else
            {
                uint32_t idx;
                for ( idx = 0; idx < VectorLength( &repos ) && rc == 0; ++idx )
                {
                    bool add = true;
                    KRepository * repo = VectorGet ( &repos, idx );
                    if ( repo != NULL )
                    {
                        if ( options->user_repo_name != NULL )
                        {
                            char name[ 1024 ];
                            rc = KRepositoryName( repo, name, sizeof name, NULL );
                            if ( rc != 0 )
                            {
                                PLOGERR( klogErr, ( klogErr, rc,
                                         "KRepositoryName( for repo # $(idx) ) failed in $(func)", "idx=%u,func=%s", idx, __func__ ) );
                            }
                            else
                                add = string_cmp_1( options->user_repo_name, name );
                        }
                        if ( add )
                        {
                            rc_t rc1;
                            char path[ 4096 ];
                            size_t path_size;
                            rc = KRepositoryRoot ( repo, path, sizeof path, &path_size );
                            if ( rc != 0 )
                            {
                                PLOGERR( klogErr, ( klogErr, rc,
                                         "KRepositoryRoot( for repo # $(idx) ) failed in $(func)", "idx=%u,func=%s", idx, __func__ ) );
                            }
                            else
                            {
                                path[ path_size ] = 0;
                                rc = add_tool_options_path( options, path );
                                if ( options->detailed )
                                    KOutMsg( "source: %s\n", path );
                            }
                            rc1 = KRepositoryRootHistory ( repo, path, sizeof path, &path_size );
                            if ( rc1 == 0 ) /* it is not an error if we have no repository-root */
                            {
                                path[ path_size ] = 0;
                                foreach_Str_part( path, ':', on_history_path, options );
                            }
                        }
                    }
                }
                {
                    rc_t rc2 = KRepositoryVectorWhack ( &repos );
                    if ( rc2 != 0 )
                    {
                        PLOGERR( klogErr, ( klogErr, rc2,
                                 "KRepositoryVectorWhack() failed in $(func)", "func=%s", __func__ ) );
                    }
                }
            }
            {
                rc_t rc2 = KRepositoryMgrRelease ( repo_mgr );
                if ( rc2 != 0 )
                {
                    PLOGERR( klogErr, ( klogErr, rc2,
                             "KRepositoryMgrRelease() failed in $(func)", "func=%s", __func__ ) );
                }
            }
        }
        {
            rc_t rc2 = KConfigRelease ( cfg );
            if ( rc2 != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc2,
                         "KConfigRelease() failed in $(func)", "func=%s", __func__ ) );
            }
        }
    }
    return rc;
}


static rc_t CC write_to_FILE ( void *f, const char *buffer, size_t bytes, size_t *num_writ )
{
    * num_writ = fwrite ( buffer, 1, bytes, f );    /* hardcoded value for stdout == 1 */
    if ( * num_writ != bytes )
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}


static rc_t explain_no_cache_found ( void )
{
    rc_t rc = KOutMsg( "this tool was not able to find the location of the cache\n" );
    if ( rc == 0 )
        rc = KOutMsg( "solution A : specify the cache-path at the commandline like 'cache-mgr ~/my_cache -r'\n" );
    if ( rc == 0 )
        rc = KOutMsg( "solution B : fix your broken configuration-setup ( use vdb-config )\n" );
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = KOutHandlerSet ( write_to_FILE, stdout );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc,
                 "KOutHandlerSet() failed in $(func)", "func=%s", __func__ ) );
    }
    else
    {
        Args * args;
        rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                                 ToolOptions, sizeof ToolOptions / sizeof ToolOptions [ 0 ] );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc,
                     "ArgsMakeAndHandle() failed in $(func)", "func=%s", __func__ ) );
        }
        else
        {
            tool_options options;
            rc = init_tool_options( &options );
            if ( rc == 0 )
            {
                rc = get_tool_options( args, &options );
                if ( rc == 0 )
                {
                    bool cache_paths_needed = false;
                    switch( options.main_function )
                    {
                        case tf_report  : ;
                        case tf_unlock  : ;
                        case tf_clear   : cache_paths_needed = true; break;
                        case tf_rreport : ;
                        case tf_enable  : ;
                        case tf_disable : ;
                        case tf_unknown : ; break;
                    }

                    if ( options.path_count == 0 && cache_paths_needed )
                        rc = get_cache_path_from_repo_mgr( &options );

                    if ( rc == 0 )
                    {
                        if ( options.path_count == 0 && cache_paths_needed )
                        {
                            rc = explain_no_cache_found ();
                        }
                        else
                            rc = perform( &options, args );
                    }
                }
                clear_tool_options( &options );
            }
        }
        {
            rc_t rc2 = ArgsWhack ( args );
            if ( rc2 != 0 )
            {
                PLOGERR( klogErr, ( klogErr, rc2,
                         "ArgsWhack() failed in $(func)", "func=%s", __func__ ) );
            }
        }
    }

    return VDB_TERMINATE( rc );
}

