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

#include <klib/out.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/printf.h>

#include "allele_lookup.h"

#include <stdio.h>
#include <ctype.h>
#include <sqlite3.h>

const char UsageDefaultName[] = "alookup";

rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg( "\n"
                     "Usage:\n"
                     "  %s <path> [<path> ...] [options]\n"
                     "\n", progname );
}

#define OPTION_CACHE  "cache"
#define ALIAS_CACHE   "c"
static const char * cache_usage[]     = { "the lookup-cache to use", NULL };

#define OPTION_DB     "database"
#define ALIAS_DB      "d"
static const char * db_usage[]        = { "opt sqlite-db to write into", NULL };

OptDef ToolOptions[] =
{
/*    name              alias           fkt    usage-txt,       cnt, needs value, required */
    { OPTION_CACHE,     ALIAS_CACHE,    NULL, cache_usage,     1,   true,        false },
    { OPTION_DB,        ALIAS_DB,       NULL, db_usage,        1,   true,        false }
};

rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    uint32_t i;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg( "Options:\n" );

    HelpOptionsStandard ();
    for ( i = 0; i < sizeof ( ToolOptions ) / sizeof ( ToolOptions[ 0 ] ); ++i )
        HelpOptionLine ( ToolOptions[ i ].aliases, ToolOptions[ i ].name, NULL, ToolOptions[ i ].help );

    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t log_err( const char * t_fmt, ... )
{
    rc_t rc;
    char buffer[ 4096 ];
    size_t num_writ;
    
    va_list args;
    va_start( args, t_fmt );
    rc = string_vprintf( buffer, sizeof buffer, &num_writ, t_fmt, args );
    va_end( args );
    if ( rc == 0 )
        rc = LogMsg( klogErr, buffer );
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

typedef struct tool_ctx
{
    const char * cache_file;
    const char * db_file;
} tool_ctx;


static rc_t get_charptr( const Args * args, const char *option, const char ** value )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option, &count );
    if ( rc == 0 && count > 0 )
    {
        rc = ArgsOptionValue( args, option, 0, ( const void ** )value );
        if ( rc != 0 )
            *value = NULL;
    }
    else
        *value = NULL;
    return rc;
}


static rc_t fill_out_tool_ctx( const Args * args, tool_ctx * ctx )
{
    rc_t rc = get_charptr( args, OPTION_CACHE, &ctx->cache_file );
    if ( rc == 0 )
    {
        if ( ctx->cache_file == NULL )
        {
            rc = RC ( rcApp, rcArgv, rcConstructing, rcParam, rcInvalid );
            log_err( "cache-file is missing!" );
        }
        else
            rc = get_charptr( args, OPTION_DB, &ctx->db_file );
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */


typedef void ( * on_part )( const String * part, uint32_t id, void * user_data );

static uint32_t split_String( const String * src, char c, on_part f, void * user_data )
{
    uint32_t char_idx = 0;
    uint32_t id = 0;
    String part = { .addr = src->addr, .size = 0, .len = 0 };
    while ( char_idx < src->len )
    {
        if ( src->addr[ char_idx ] == c )
        {
            part.size = part.len;
            f( &part, id++, user_data );
            part.addr = &src->addr[ char_idx + 1 ];
            part.size = part.len = 0;
        }
        else
            part.len++;
        char_idx++;
    }
    if ( part.len > 0 )
    {
        part.size = part.len;
        f( &part, id, user_data );
    }
    return id;
}

typedef struct key_parts
{
    String refname;
    uint64_t refpos;
    uint32_t deletes;
    String inserts;
} key_parts;

static void on_key_part( const String * part, uint32_t id, void * user_data )
{
    key_parts * parts = user_data;
    switch ( id )
    {
        case 0 : StringInit( &parts->refname, part->addr, part->size, part->len ); break;
        case 1 : parts->refpos = StringToU64( part, NULL ); break;
        case 2 : parts->deletes = ( uint32_t )StringToU64( part, NULL ); break;
        case 3 : StringInit( &parts->inserts, part->addr, part->size, part->len ); break;
    }
}

static rc_t print_entry( const String * key, uint64_t * values )
{
    key_parts parts;
    split_String( key, ':', on_key_part, &parts );
    return KOutMsg( "%S\t%lu\t%u\t%S\t%lu\t%lX\n",
        &parts.refname, parts.refpos, parts.deletes, &parts.inserts, values[ 0 ], values[ 1 ] );
}


/* ----------------------------------------------------------------------------------------------- */

static int db_cb( void *NotUsed, int argc, char **argv, char **azColName )
{
    int i;
    for( i = 0; i < argc; i++ )
        log_err( "%s = %s", azColName[ i ], argv[ i ] ? argv[ i ] : "NULL" );
    return 0;
}

static bool execute_stm( sqlite3 * db, const char * stm )
{
    char * errmsg = NULL;
    bool res = ( sqlite3_exec( db, stm, db_cb, 0, &errmsg ) == SQLITE_OK );
    if ( !res )
        log_err( "SQL error: %s", errmsg );
    return res;
}

static bool make_db( sqlite3 ** db, const char * path )
{
    bool res = ( sqlite3_open( path, db ) == SQLITE_OK );
    if ( !res )
        log_err( "canot open '%s' because '%s'", path, sqlite3_errmsg( *db ) );
    else
    {
        res = execute_stm( *db, "CREATE TABLE IF NOT EXISTS AL( NAME, POS, DEL, INS, RS, FLAGS );" );
        if ( !res )
        {
            sqlite3_close( *db );
            *db = NULL;
        }
    }
    return res;
}

static void release_db( sqlite3 * db )
{
    sqlite3_close( db );
}


static rc_t store_entry( sqlite3 * db, const String * key, uint64_t * values )
{
    rc_t rc;
    size_t num_writ;
    char stm[ 1024 ];
    key_parts parts;
    
    split_String( key, ':', on_key_part, &parts );
    rc = string_printf( stm, sizeof stm, &num_writ,
        "INSERT INTO AL VALUES( '%S', %lu, %u, '%S', %lu, '%lX' )",
        &parts.refname, parts.refpos, parts.deletes, &parts.inserts, values[ 0 ], values[ 1 ] );
    if ( rc == 0 )
        execute_stm( db, stm );
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

static rc_t print_cache( tool_ctx * ctx )
{
    struct Allele_Lookup * al;
    rc_t rc = allele_lookup_make( &al, ctx->cache_file );
    if ( rc == 0 )
    {
        struct Lookup_Cursor * curs;    
        rc = lookup_cursor_make( al, &curs );
        if ( rc == 0 )
        {
            bool running = true;
            if ( ctx->db_file != NULL )
            {
                sqlite3 * db = NULL;
                if ( make_db( &db, ctx->db_file ) )
                {
                    while ( running )
                    {
                        String key;
                        uint64_t values[ 2 ];

                        running = lookup_cursor_next( curs, &key, values );
                        if ( running )
                        {
                            rc = store_entry( db, &key, values );
                            if ( Quitting() != 0 ) running = false;
                        }
                    }
                    release_db( db );
                }
            }
            else
            {
                while ( running )
                {
                    String key;
                    uint64_t values[ 2 ];

                    running = lookup_cursor_next( curs, &key, values );
                    if ( running )
                    {
                        rc = print_entry( &key, values );
                        if ( Quitting() != 0 ) running = false;
                    }
                }
            }
            lookup_cursor_release( curs );
        }
        rc = allele_lookup_release( al );
    }
    return rc;
}


/* ----------------------------------------------------------------------------------------------- */


static rc_t lookup_allele( Args * args, uint32_t count, const char * cache_file )
{
    struct Allele_Lookup * al;
    rc_t rc = allele_lookup_make( &al, cache_file );
    if ( rc == 0 )
    {
        uint32_t idx;
        const char * allele;
        String key;
        uint64_t values[ 2 ];
       
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            rc = ArgsParamValue( args, idx, ( const void ** )&allele );
            if ( rc != 0 )
                log_err( "ArgsParamValue( %d ) failed %R", idx, rc );
            else
            {
                StringInitCString( &key, allele );
                if ( allele_lookup_perform( al, &key, values ) )
                    rc = print_entry( &key, values );
                else
                    rc = KOutMsg( "%S not found\n", &key );
            }
        }
        allele_lookup_release( al );
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------- */

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                ToolOptions, sizeof ( ToolOptions ) / sizeof ( OptDef ) );
    if ( rc != 0 )
        log_err( "ArgsMakeAndHandle() failed %R", rc );
    else
    {
        tool_ctx ctx;
        rc = fill_out_tool_ctx( args, &ctx );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = ArgsParamCount( args, &count );
            if ( rc != 0 )
                log_err( "ArgsParamCount() failed %R", rc );
            else
            {
                if ( count < 1 )
                    rc = print_cache( &ctx );
                else
                    rc = lookup_allele( args, count, ctx.cache_file );
            }
        }
        ArgsWhack ( args );
    }
    return rc;
}
