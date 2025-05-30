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
*  and reliability of the software and data", the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties", express or implied", including
*  warranties of performance", merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include "pl-context.h"
#include "pl-tools.h"
#include "pl-zmw.h"
#include "pl-basecalls_cmn.h"
#include "pl-sequence.h"
#include "pl-consensus.h"
#include "pl-passes.h"
#include "pl-metrics.h"

#include <klib/out.h>
#include <klib/namelist.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/log.h>

#include <kdb/meta.h>
#include <kdb/database.h>

#include <vdb/vdb-priv.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <sra/sraschema.h>

#include <kapp/main.h>
#include <kapp/args.h>

#include <hdf5/kdf5.h>

#include <kfs/arrayfile.h>

#include <loader/loader-meta.h>

#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

const char UsageDefaultName[] = "pacbio-load";

rc_t CC UsageSummary ( const char * progname )
{
    OUTMSG ( ("\n"
        "Usage:\n"
        "  %s <hdf5-file> -o<target>\n"
        "\n", progname) );
    return 0;
}

static const char* schema_usage[] = { "schema-name to be used", NULL };
static const char* output_usage[] = { "target to be created", NULL };
static const char* force_usage[] = { "forces an existing target to be overwritten", NULL };
static const char* tabs_usage[] = { "load only these tabs (SCPM), dflt=all",
                                     " S...Sequence",
                                     " C...Consensus",
                                     " P...Passes",
                                     " M...Metrics", NULL };
static const char* progress_usage[] = { "show load-progress", NULL };


rc_t CC Usage ( const Args * args )
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

    KOutMsg ("Options:\n");

    HelpOptionLine ( ALIAS_OUTPUT, OPTION_OUTPUT, "output", output_usage );

    HelpOptionLine ( ALIAS_SCHEMA, OPTION_SCHEMA, "schema", schema_usage );
    HelpOptionLine ( ALIAS_FORCE, OPTION_FORCE, "force", force_usage );
    HelpOptionLine ( ALIAS_TABS, OPTION_TABS, "tabs", tabs_usage );
    HelpOptionLine ( ALIAS_WITH_PROGRESS, OPTION_WITH_PROGRESS,
                     "load-progress", progress_usage );
    XMLLogger_Usage();
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );
    return rc;
}

static bool pacbio_is_schema_dflt( const char * schema )
{
    size_t asize = string_size ( schema );
    size_t bsize = string_size ( DFLT_SCHEMA );
    uint32_t max_chars = ( asize > bsize ) ? asize : bsize;
    return ( string_cmp ( schema, asize, DFLT_SCHEMA, bsize, max_chars ) == 0 );
}


static rc_t pacbio_extract_path( const KDirectory *dir, const char *schema_name,
                                 char * dst, size_t dst_len )
{
    rc_t rc = KDirectoryResolvePath ( dir, true, dst, dst_len, "%s", schema_name );
    if ( rc != 0 )
        PLOGERR( klogErr, ( klogErr, rc, "cannot resolve path to schema-file '$(name)'",
                            "name=%s", schema_name ));
    else
    {
        char *ptr = strrchr ( dst, '/' );
        if ( ptr == 0 )
        {
            rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
            PLOGERR( klogErr, ( klogErr, rc, "cannot extract the path of '$(name)'",
                                "name=%s", schema_name ));
        }
        else
            *ptr = 0;
    }
    return rc;
}


static rc_t pacbio_load_schema( KDirectory * wd, VDBManager * vdb_mgr, VSchema ** schema, const char * schema_name )
{
    rc_t rc;

    if ( pacbio_is_schema_dflt( schema_name ) )
    {
        rc = VDBManagerMakeSRASchema ( vdb_mgr, schema );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot create sra-schema" );

        if ( rc == 0 )
        {
            rc = VSchemaParseFile ( *schema, "%s", schema_name );
            if ( rc != 0 )
                PLOGERR( klogErr, ( klogErr, rc, "cannot parse schema file '$(schema)'",
                                    "schema=%s", schema_name ) );
        }
    }
    else
    {
        rc = VDBManagerMakeSchema ( vdb_mgr, schema );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "cannot create sra-schema" );
        else
        {
            char path[ 4096 ];
            rc = pacbio_extract_path( wd, schema_name, path, sizeof path );
            if ( rc == 0 )
            {
                rc = VSchemaAddIncludePath ( *schema, "%s", path );
                if ( rc != 0 )
                    PLOGERR( klogErr, ( klogErr, rc, "cannot add schema-include-path '$(path)'",
                                        "path=%s", path ) );
                else
                {
                    rc = VSchemaParseFile ( *schema, "%s", schema_name );
                    if ( rc != 0 )
                        PLOGERR( klogErr, ( klogErr, rc, "cannot parse schema file '$(schema)'",
                                            "schema=%s", schema_name ) );
                }
            }
        }
    }
    return rc;
}


static rc_t pacbio_meta_entry( VDatabase * db, const char * toolname )
{
    KMetadata* meta = NULL;
    rc_t rc = VDatabaseOpenMetadataUpdate( db, &meta );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "Cannot open database-metadata" );
    }
    else
    {
        KMDataNode *node = NULL;

        rc = KMetadataOpenNodeUpdate( meta, &node, "/" );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "Cannot open database-metadata-root" );
        }
        else
        {
            rc = KLoaderMeta_Write( node, toolname, __DATE__, "PacBio HDF5", KAppVersion() );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "Cannot write pacbio metadata node" );
            }
            KMDataNodeRelease( node );
        }
        KMetadataRelease( meta );
    }
    return rc;
}


/* the context that has the context of all 4 sub-tables (SEQUENCE,CONSENSU,PASSES,METRICS) */
typedef struct seq_con_pas_met
{
    seq_ctx sequence;       /* from pl-sequence.h */
    con_ctx consensus;      /* from pl-consensus.h */
    pas_ctx passes;         /* from pl-passes.h */
    met_ctx metrics;        /* from pl-metrics.h */
} seq_con_pas_met;


/* we have to pass in the first hdf5-source, because prepare of sequences needs it */
static rc_t pacbio_prepare( VDatabase * database, seq_con_pas_met * dst, KDirectory * first_src, ld_context *lctx )
{
    rc_t rc;

    dst->sequence.cursor = NULL;
    dst->consensus.cursor = NULL;
    dst->passes.cursor = NULL;
    dst->metrics.cursor = NULL;

    rc = prepare_seq( database, &dst->sequence, first_src, lctx ); /* pl-sequence.c */
    if ( rc == 0 )
        rc = prepare_consensus( database, &dst->consensus, lctx ); /* pl-consensus.c */
    if ( rc == 0 )
        rc = prepare_passes( database, &dst->passes, lctx ); /* pl-passes.c */
    if ( rc == 0 )
        rc = prepare_metrics( database, &dst->metrics, lctx ); /* pl-metrics.c */
    return rc;
}


static rc_t pacbio_load_src( context *ctx, seq_con_pas_met * dst, KDirectory * src, bool * consensus_present )
{
    rc_t rc1, rc = 0;

    if ( ctx_ld_sequence( ctx ) )
        rc = load_seq_src( &dst->sequence, src ); /* pl-sequence.c */

    if ( rc == 0 && ctx_ld_consensus( ctx ) )
    {
        rc1 = load_consensus_src( &dst->consensus, src ); /* pl-consensus.c */
        if ( rc1 == 0 )
            *consensus_present = true;
        else
            LOGMSG( klogWarn, "the consensus-group is missing" );
    }

    if ( rc == 0 && ctx_ld_passes( ctx ) && *consensus_present )
    {
        rc1 = load_passes_src( &dst->passes, src ); /* pl-passes.c */
        if ( rc1 != 0 )
            LOGMSG( klogWarn, "the passes-table is missing" );
    }

    if ( rc == 0 && ctx_ld_metrics( ctx ) && *consensus_present )
    {
        rc1 = load_metrics_src( &dst->metrics, src ); /* pl-metrics.c */
        if ( rc1 != 0 )
            LOGMSG( klogWarn, "the metrics-table is missing" );
    }
    return rc;
}


static rc_t pacbio_finish( seq_con_pas_met * dst )
{
    rc_t rc = finish_seq( &dst->sequence ); /* pl-sequence.c */
    if ( rc == 0 )
        rc = finish_consensus( &dst->consensus ); /* pl-consensus.c */
    if ( rc == 0 )
        rc = finish_passes( &dst->passes ); /* pl-passes.c */
    if ( rc == 0 )
        rc = finish_metrics( &dst->metrics ); /* pl-metrics.c */
    return rc;
}


static rc_t pacbio_get_hdf5_src( KDirectory * wd, const VNamelist * path_list, uint32_t idx, KDirectory ** hdf5_src )
{
    const char * src_path;
    rc_t rc = VNameListGet ( path_list, idx, &src_path );
    if ( rc == 0 && src_path != NULL )
    {
        rc = MakeHDF5RootDir ( wd, hdf5_src, false, src_path );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc, "cannot open hdf5-source-file '$(srcfile)'",
                                "srcfile=%s", src_path ) );
        }
    }
    return rc;
}


static bool pacbio_has_MultiParts( KDirectory * hdf5_src )
{
    uint32_t pt = KDirectoryPathType ( hdf5_src, "MultiPart/Parts" );
    return ( pt == kptDataset );
}


static rc_t pacbio_get_MultiParts( KDirectory * hdf5_src, VNamelist * parts )
{
    struct KFile const *f;      /* the fake "file" from a HDF5-dir */
    rc_t rc = KDirectoryOpenFileRead ( hdf5_src, &f, "MultiPart/Parts" );
    if ( rc == 0 )
    {
        struct KArrayFile *af;      /* the arrayfile made from f */
        rc = MakeHDF5ArrayFile ( f, &af );
        if ( rc == 0 )
        {
            uint8_t dimensionality;
            rc = KArrayFileDimensionality ( af, &dimensionality );
            if ( rc == 0 && dimensionality == 1 )
            {
                uint64_t extents[ 1 ];
                rc = KArrayFileDimExtents ( af, dimensionality, extents );
                if ( rc == 0 )
                {
                    uint64_t pos[ 1 ];
                    for ( pos[ 0 ] = 0; pos[ 0 ] < extents[ 0 ] && rc == 0; pos[ 0 ] += 1 )
                    {
                        char buffer[ 1024 ];
                        uint64_t num_read;
                        rc = KArrayFileRead_v ( af, 1, pos, buffer, sizeof buffer, &num_read );
                        if ( rc == 0 )
                            rc = VNamelistAppend ( parts, buffer );
                    }
                }
            }
            KArrayFileRelease( af );
        }
        KFileRelease( f );
    }
    return rc;
}


static rc_t pacbio_load_multipart( context * ctx, KDirectory * wd, VDatabase * database,
                                   KDirectory ** hdf5_src, bool * consensus_present,
                                   ld_context * lctx, uint32_t count )
{
    seq_con_pas_met dst;
    uint32_t idx = 0;
    /* the loop is complicated, because pacbio_prepare needs the first hdf5-src opened ! */
    rc_t rc = pacbio_prepare( database, &dst, *hdf5_src, lctx );
    while ( idx < count && rc == 0 )
    {
        rc = pacbio_load_src( ctx, &dst, *hdf5_src, consensus_present );
        idx++;
        if ( rc == 0 && idx < count )
        {
            KDirectoryRelease ( *hdf5_src );
            rc = pacbio_get_hdf5_src( wd, ctx->src_paths, idx, hdf5_src );
        }
    }
    pacbio_finish( &dst );
    KDirectoryRelease ( *hdf5_src );
    return rc;
}


static rc_t add_unique_to_namelist( const VNamelist * src, VNamelist * dst, int32_t idx )
{
    const char * s;
    rc_t rc = VNameListGet( src, idx, &s );
    if ( rc == 0 && s != NULL && s[ 0 ] != 0 )
    {
        uint32_t found;
        rc_t rc2 = VNamelistIndexOf( dst, s, &found );
        if ( GetRCState( rc2 ) == rcNotFound )
            rc = VNamelistAppend( dst, s );
    }
    return rc;
}

static rc_t pacbio_load( context *ctx, KDirectory * wd, ld_context *lctx, const char * toolname )
{
    VDBManager * vdb_mgr = NULL;
    VSchema * schema = NULL;
    VDatabase * database = NULL;

    rc_t rc = VDBManagerMakeUpdate ( &vdb_mgr, wd );
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "cannot create vdb-update-manager" );
    }

    if ( rc == 0 )
        rc = pacbio_load_schema( wd, vdb_mgr, &schema, ctx->schema_name );


    /* creates the output vdb database */
    if ( rc == 0 )
    {
        KCreateMode cmode = kcmMD5 | kcmParents;
        if ( ctx->force )
            cmode |= kcmInit;
        else
            cmode |= kcmCreate;
        rc = VDBManagerCreateDB( vdb_mgr, &database, schema,
                                 PACBIO_SCHEMA_DB, cmode, "%s", ctx->dst_path );
        if ( rc != 0 )
            PLOGERR( klogErr, ( klogErr, rc, "cannot create output-database '$(dst)'",
                                "dst=%s", ctx->dst_path ) );
    }


    /* creates the 4 output vdb tables... SEQUENCE, CONSENSUS, PASSES and METRICS */
    if ( rc == 0 )
    {
        bool consensus_present = false;
        VNamelist * to_process;
        rc = VNamelistMake ( &to_process, 5 );
        if ( rc == 0 )
        {
            KDirectory * hdf5_src;
            uint32_t count, idx;

            rc = VNameListCount ( ctx->src_paths, &count );
            for ( idx = 0; rc == 0 && idx < count; ++idx )
            {
                rc = pacbio_get_hdf5_src( wd, ctx->src_paths, 0, &hdf5_src );
                if ( rc == 0 )
                {
                    if ( pacbio_has_MultiParts( hdf5_src ) )
                    {
                        VNamelist * parts;
                        rc = VNamelistMake ( &parts, 5 );
                        if ( rc == 0 )
                        {
                            rc = pacbio_get_MultiParts( hdf5_src, parts );
                            if ( rc == 0 )
                            {
                                uint32_t p_count, p_idx;
                                rc = VNameListCount ( parts, &p_count );
                                for ( p_idx = 0; rc == 0 && p_idx < p_count; ++p_idx )
                                    rc = add_unique_to_namelist( parts, to_process, p_idx );
                            }
                            VNamelistRelease ( parts );
                        }
                    }
                    else
                        rc = add_unique_to_namelist( ctx->src_paths, to_process, idx );
                    KDirectoryRelease( hdf5_src );
                }
            }
            VNamelistRelease ( ctx->src_paths );
            ctx->src_paths = to_process;

            if ( rc == 0 )
            {
                rc = VNameListCount ( ctx->src_paths, &count );
                if ( rc == 0 && count > 0 )
                {
                    ctx_show( ctx );
                    rc = pacbio_get_hdf5_src( wd, ctx->src_paths, 0, &hdf5_src );
                    if ( rc == 0 )
                        rc = pacbio_load_multipart( ctx, wd, database, &hdf5_src, &consensus_present, lctx, count );
                }
            }

        }

        if ( !consensus_present )
            VDatabaseDropTable ( database, "CONSENSUS" );
    }

    if ( rc == 0 )
        seq_report_totals( lctx );

    if ( rc == 0 )
        rc = pacbio_meta_entry( database, toolname );

    if ( database != NULL )
        VDatabaseRelease ( database );

    if ( schema != NULL )
        VSchemaRelease ( schema );

    if ( vdb_mgr != NULL )
        VDBManagerRelease ( vdb_mgr );
    return rc;
}


static rc_t pacbio_check_sourcefile( const KDirectory * dir, char ** path )
{
    rc_t rc = 0;
    uint32_t src_path_type = KDirectoryPathType ( dir, "%s", *path );
    if ( ( src_path_type & kptFile ) == 0 )
    {
        rc = RC ( rcExe, rcFile, rcValidating, rcItem, rcNotFound );
        LOGERR( klogErr, rc, "source-file not found" );
    }
    else
    {
        if ( ( src_path_type & kptAlias ) != 0 )
        {
            char resolved[ 4096 ];
            rc = KDirectoryResolveAlias ( dir, true, resolved,
                                          sizeof resolved, "%s", *path );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "cannot resolve srcfile-link" );
            }
            else
            {
                free( *path );
                *path = string_dup_measure ( resolved, NULL );
            }
        }
    }
    return rc;
}


static rc_t pacbio_check_sourcefile_list( const KDirectory * dir, VNamelist ** list )
{
    VNamelist * temp;
    rc_t rc = VNamelistMake ( &temp, 5 );
    if ( rc == 0 )
    {
        uint32_t idx, count;
        rc = VNameListCount ( *list, &count );
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            const char * name = NULL;
            rc = VNameListGet ( *list, idx, &name );
            if ( rc == 0 && name != NULL )
            {
                char * path = string_dup_measure ( name, NULL );
                rc = pacbio_check_sourcefile( dir, &path );
                if ( rc == 0 )
                    rc = VNamelistAppend ( temp, path );
            }
        }
        if ( rc == 0 )
        {
            rc = VNamelistRelease ( *list );
            *list = temp;
        }
    }
    return rc;
}


OptDef MyOptions[] =
{
    { OPTION_SCHEMA, ALIAS_SCHEMA, NULL, schema_usage, 5, true, false },
    { OPTION_FORCE, ALIAS_FORCE, NULL, force_usage, 1, false, false },
    { OPTION_WITH_PROGRESS, ALIAS_WITH_PROGRESS, NULL, progress_usage, 1, false, false },
    { OPTION_TABS, ALIAS_TABS, NULL, tabs_usage, 1, true, false },
    { OPTION_OUTPUT, ALIAS_OUTPUT, NULL, output_usage, 1, true, true }
};

MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
        return VDB_INIT_FAILED;

    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 2,
                                  MyOptions, sizeof MyOptions / sizeof ( OptDef ),
                                  XMLLogger_Args, XMLLogger_ArgsQty  );

    KLogHandlerSetStdErr();
    if ( rc != 0 )
    {
        LOGERR( klogErr, rc, "error creating internal structure" );
    }
    else
    {
        KDirectory * wd;
        rc = KDirectoryNativeDir ( &wd );
        if ( rc != 0 )
        {
            LOGERR( klogErr, rc, "error creating internal structure" );
        }
        else
        {
            ld_context lctx;
            lctx_init( &lctx );
            rc = XMLLogger_Make( &lctx.xml_logger, wd, args );
            if ( rc != 0 )
            {
                LOGERR( klogErr, rc, "error creating internal structure" );
            }
            else
            {
                context ctx;
                rc = ctx_init( args, &ctx );
                if ( rc == 0 )
                {
                    rc = pacbio_check_sourcefile_list( wd, &ctx.src_paths );
                    if ( rc == 0 )
                    {
                        lctx.with_progress = ctx.with_progress;
                        lctx.dst_path = ctx.dst_path;
                        lctx.cache_content = false;
                        lctx.check_src_obj = false;

                        rc = KLoadProgressbar_Make( &lctx.xml_progress, 0 );
                        if ( rc != 0 )
                        {
                            LOGERR( klogErr, rc, "cannot create LoadProgressBar" );
                        }
                        else
                            rc = pacbio_load( &ctx, wd, &lctx, argv[ 0 ] );

                    }
                    ctx_free( &ctx );
                }
            }
            KDirectoryRelease ( wd );
            lctx_free( &lctx );
        }
        ArgsWhack ( args );
    }
    return VdbTerminate( rc );
}
