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

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/vdb-priv.h>
#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/meta.h>
#include <kfs/directory.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <os-native.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define OPTION_SCHEMA         "schema"
#define OPTION_DB_TYPE        "db_type"
#define OPTION_TAB_TYPE       "tab_type"
#define OPTION_TAB_NAME       "tab_name"

#define ALIAS_SCHEMA          "s"
#define ALIAS_DB_TYPE         "d"
#define ALIAS_TAB_TYPE        "t"
#define ALIAS_TAB_NAME        "n"

static const char * schema_usage[] = { "path to file, that contains new schema", NULL };
static const char * db_type_usage[] = { "string, specifies the type of the database", NULL };
static const char * tab_type_usage[] = { "string, specifies the type of the selected table", NULL };
static const char * tab_name_usage[] = { "string, specifies the name of the table to be processed", NULL };

OptDef SchemaUpOptions[] =
{
    { OPTION_SCHEMA, ALIAS_SCHEMA, NULL, schema_usage, 1, true, true },
    { OPTION_DB_TYPE, ALIAS_DB_TYPE, NULL, db_type_usage, 1, true, true },
    { OPTION_TAB_TYPE, ALIAS_TAB_TYPE, NULL, tab_type_usage, 1, true, true },
    { OPTION_TAB_NAME, ALIAS_TAB_NAME, NULL, tab_name_usage, 1, true, true }
};

const char UsageDefaultName[] = "schema-update";


rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <path> [options]\n"
                    "\n", progname);
}


rc_t CC Usage( const Args * args  )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram( args, &fullpath, &progname );
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary( progname );

    KOutMsg ("Options:\n");

    HelpOptionLine ( ALIAS_SCHEMA, OPTION_SCHEMA, NULL, schema_usage );
    HelpOptionLine ( ALIAS_DB_TYPE, OPTION_DB_TYPE, NULL, db_type_usage );
    HelpOptionLine ( ALIAS_TAB_TYPE, OPTION_TAB_TYPE, NULL, tab_type_usage );
    HelpOptionLine ( ALIAS_TAB_NAME, OPTION_TAB_NAME, NULL, tab_name_usage );

    HelpOptionsStandard();

    HelpVersion( fullpath, KAppVersion() );
    return rc;
}

static const char* get_str_option( const Args *my_args,
                                   const char *name )
{
    const char* res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( my_args, name, &count );
    if ( ( rc == 0 )&&( count > 0 ) )
    {
        rc = ArgsOptionValue( my_args, name, 0, (const void **)&res );
    }
    return res;
}


typedef struct ctx
{
    VDBManager * vdb_mgr;
    KDBManager * kdb_mgr;
    VSchema *schema;

    const char *db_type;
    const char *tab_type;
    const char *tab_name;

    char * db_schema_buff;
    size_t db_schema_len;
    char * tab_schema_buff;
    size_t tab_schema_len;
} ctx;
typedef ctx* p_ctx;


static rc_t write_schema( KMetadata *meta, const char * schema_dump, size_t schema_len )
{
    KMDataNode *schema_node;
    rc_t rc = KMetadataOpenNodeUpdate ( meta, &schema_node, "schema" );
    if ( rc == 0 )
    {
        rc = KMDataNodeWrite ( schema_node, schema_dump, schema_len );
        KMDataNodeRelease ( schema_node );
    }
    else
        OUTMSG(( "error KMetadataOpenNodeUpdate: %R\n", rc ));
    return rc;
}


static rc_t process_csra( p_ctx ctx, const char *csra )
{
    KDatabase *this_kdb;
    rc_t rc = KDBManagerOpenDBUpdate ( ctx->kdb_mgr, &this_kdb, "%s", csra );
    if ( rc == 0 )
    {
        /* do it for the whole database ... */
        KMetadata *meta;
        KTable *this_tab;

        rc = KDatabaseOpenMetadataUpdate ( this_kdb, &meta );
        if ( rc == 0 )
        {
            rc = write_schema( meta, ctx->db_schema_buff, ctx->db_schema_len );
            KMetadataRelease ( meta );
        }
        else
            OUTMSG(( "error KDatabaseOpenMetadataUpdate: %R\n", rc ));

        /* do it for the selected table ... */
        if ( rc == 0 )
        {
            rc = KDatabaseOpenTableUpdate ( this_kdb, &this_tab, "%s", ctx->tab_name );
            if ( rc == 0 )
            {
                rc = KTableOpenMetadataUpdate ( this_tab, &meta );
                if ( rc == 0 )
                {
                    rc = write_schema( meta, ctx->tab_schema_buff, ctx->tab_schema_len );
                    KMetadataRelease ( meta );
                }
                else
                    OUTMSG(( "error KTableOpenMetadataUpdate: %R\n", rc ));
            }
            else
                OUTMSG(( "error KDatabaseOpenTableUpdate: %R\n", rc ));
        }

        KDatabaseRelease ( this_kdb );
    }
        else OUTMSG(( "error KDBManagerOpenDBUpdate: %R\n", rc ));
    return rc;
}


static rc_t process_files( p_ctx ctx, Args * args )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc == 0 )
    {
        uint32_t idx;
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char *csra = NULL;
            rc = ArgsParamValue( args, idx, (const void **)&csra );
            if ( rc == 0 )
            {
                OUTMSG(( "\nprocessing: %s\n", csra ));
                rc = process_csra( ctx, csra );
                if ( rc == 0 )
                    OUTMSG(( "processed: %s\n", csra ));
            }
        }
    }
/*
    if ( ctx->db_schema_buff != NULL )
        OUTMSG(( "%s\n", ctx->db_schema_buff ));
    if ( ctx->tab_schema_buff != NULL )
        OUTMSG(( "%s\n", ctx->tab_schema_buff ));
*/
    return rc;
}


typedef struct dump_ctx
{
    char *buffer;
    size_t len;
    size_t size;
} dump_ctx;
typedef dump_ctx* p_dump_ctx;


static rc_t CC schema_dump_flush( void *dst, const void *buffer, size_t bsize )
{
    rc_t rc = -1;
    p_dump_ctx ctx = dst;
    if ( ctx->buffer == NULL )
    {
        ctx->size = bsize + 1;
        ctx->buffer = malloc( ctx->size );
        if ( ctx->buffer != NULL )
        {
            memmove ( ctx->buffer, buffer, bsize );
            ctx->len = bsize;
            rc = 0;
        }
    }
    else
    {
        ctx->size += bsize;
        ctx->buffer = realloc( ctx->buffer, ctx->size );
        if ( ctx->buffer != NULL )
        {
            memmove ( &(ctx->buffer[ctx->len]), buffer, bsize );
            ctx->len += bsize;
            rc = 0;
        }
    }
    return rc;
}


static rc_t predump_schema( VSchema *schema, const char * type,
                            char **buffer, size_t *len )
{
    rc_t rc;
    dump_ctx ctx;
    ctx.buffer = NULL;
    ctx.len = 0;
    ctx.size = 0;

    *buffer = NULL;
    *len = 0;
    rc = VSchemaDump( schema, sdmCompact, type, schema_dump_flush, &ctx );
    if ( rc == 0 )
    {
        if ( ctx.buffer != NULL )
        {
            ctx.buffer[ ctx.len ] = 0;
            *buffer = ctx.buffer;
            *len = ctx.len;
        }
    }
    else
    {
        if ( ctx.buffer != NULL )
            free( ctx.buffer );
    }
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args;

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
            SchemaUpOptions, sizeof SchemaUpOptions / sizeof SchemaUpOptions [ 0 ] );
    if ( rc == 0 )
    {
        ctx ctx;
        const char * schema_name = get_str_option( args, OPTION_SCHEMA );
        ctx.db_type = get_str_option( args, OPTION_DB_TYPE );
        ctx.tab_type = get_str_option( args, OPTION_TAB_TYPE );
        ctx.tab_name = get_str_option( args, OPTION_TAB_NAME );

        if ( schema_name != NULL && ctx.db_type != NULL &&
             ctx.tab_type != NULL && ctx.tab_name )
        {
            KDirectory * directory;
            OUTMSG(( "schema   : %s\n", schema_name ));
            OUTMSG(( "db-type  : %s\n", ctx.db_type ));
            OUTMSG(( "tab-type : %s\n", ctx.tab_type ));
            OUTMSG(( "tab-name : %s\n", ctx.tab_name ));

            rc = KDirectoryNativeDir( &directory );
            if ( rc == 0 )
            {
                rc = VDBManagerMakeUpdate ( &ctx.vdb_mgr, directory );
                if ( rc == 0 )
                {
                    rc = VDBManagerOpenKDBManagerUpdate ( ctx.vdb_mgr, &ctx.kdb_mgr );
                    if ( rc ==  0 )
                    {
                        rc = VDBManagerMakeSchema( ctx.vdb_mgr, &ctx.schema );
                        if ( rc == 0 )
                        {
                            rc = VSchemaParseFile( ctx.schema, "%s", schema_name );
                            if ( rc == 0 )
                            {
                                rc = predump_schema( ctx.schema, ctx.db_type,
                                        &ctx.db_schema_buff, &ctx.db_schema_len );
                                if ( rc == 0 )
                                {
                                    rc = predump_schema( ctx.schema, ctx.tab_type,
                                        &ctx.tab_schema_buff, &ctx.tab_schema_len );
                                    if ( rc == 0 )
                                    {
                                        rc = process_files( &ctx, args );
                                        free( ctx.tab_schema_buff );
                                    }
                                    free( ctx.db_schema_buff );
                                }
                            }
                            else
                                OUTMSG(( "error VSchemaParseFile %R\n", rc ));
                            VSchemaRelease( ctx.schema );
                        }
                        else
                            OUTMSG(( "error VDBManagerMakeSchema %R\n", rc ));
                     }
                    else
                        OUTMSG(( "error VDBManagerOpenKDBManagerUpdate %R\n", rc ));
                    VDBManagerRelease( ctx.vdb_mgr );
                }
                else
                    OUTMSG(( "error VDBManagerMakeUpdate %R\n", rc ));
                KDirectoryRelease( directory );
            }
            else
                OUTMSG(( "error KDirectoryNativeDir %R\n", rc ));
        }
        else
            OUTMSG(( "error obtaining arguments\n" ));
        ArgsWhack( args );
    }
    return VDB_TERMINATE( rc );
}

