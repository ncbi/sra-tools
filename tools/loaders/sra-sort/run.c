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

#include "sra-sort.h"
#include "db-pair.h"
#include "ctx.h"
#include "caps.h"
#include "mem.h"
#include "except.h"
#include "status.h"

#include <sra/sraschema.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <kfg/config.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>


FILE_ENTRY ( run );

static
void typename_to_config_path ( const char *in, char *out )
{
    bool end;
    uint32_t i;

    for ( end = false, i = 0; ! end; ++ i )
    {
        switch ( out [ i ] = in [ i ] )
        {
        case ':':
            out [ i ] = '/';
            break;
        case ' ':
        case '#':
            out [ i ] = 0;
        case 0:
            end = true;
            return;
        }
    }
}

static
bool map_typename_builtin ( const ctx_t *ctx, const char *sub_node, const char *in, char *out, char *schema_src, size_t size )
{
    FUNC_ENTRY ( ctx );

    size_t copied;

#define ALIGN_EVIDENCE_MAP "NCBI:align:db:alignment_evidence_sorted"
#define ALIGN_UNSORTED_MAP "NCBI:align:db:alignment_sorted"
#define ALIGN_SRC          "align/align.vschema"

    if ( strcmp ( out, "NCBI/align/db/alignment_evidence" ) == 0 )
        copied = string_copy ( out, size, ALIGN_EVIDENCE_MAP, sizeof ALIGN_EVIDENCE_MAP - 1 );
    else if ( strcmp ( out, "NCBI/align/db/alignment_unsorted" ) == 0 )
        copied = string_copy ( out, size, ALIGN_UNSORTED_MAP, sizeof ALIGN_UNSORTED_MAP - 1 );
    else
    {
        strcpy ( out, in );
        schema_src [ 0 ] = 0;
        return false;
    }

    if ( copied != size )
    {
        out [ copied ] = 0;
        copied = string_copy ( schema_src, size, ALIGN_SRC, sizeof ALIGN_SRC - 1 );
    }

    if ( copied == size )
    {
        rc_t rc = RC ( rcExe, rcType, rcCopying, rcBuffer, rcInsufficient );
        INTERNAL_ERROR ( rc, "failed to copy a built-in constant" );
        strcpy ( out, in );
        schema_src [ 0 ] = 0;
        return false;
    }

#undef ALIGN_EVIDENCE_MAP
#undef ALIGN_UNSORTED_MAP
#undef ALIGN_SRC

    schema_src [ copied ] = 0;
    return true;
}

static
bool map_typename ( const ctx_t *ctx, const char *sub_node, const char *in, char *out, char *schema_src, size_t size )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const KConfigNode *n;
    size_t num_read, remaining;

    typename_to_config_path ( in, out );

    rc = KConfigOpenNodeRead ( ctx -> caps -> cfg, & n, "sra-sort/%s/%s", sub_node, out );
    if ( rc != 0 )
        return map_typename_builtin ( ctx, sub_node, in, out, schema_src, size );

    rc = KConfigNodeRead ( n, 0, out, size - 1, & num_read, & remaining );
    if ( rc != 0 )
        ERROR ( rc, "KConfigNodeRead failed" );
    else if ( remaining != 0 )
    {
        rc = RC ( rcExe, rcNode, rcReading, rcBuffer, rcInsufficient );
        ERROR ( rc, "type map of '%s' is too long", in );
    }
    else
    {
        out [ num_read ] = 0;
        typename_to_config_path ( out, schema_src );

        KConfigNodeRelease ( n );
        rc = KConfigOpenNodeRead ( ctx -> caps -> cfg, & n, "sra-sort/schema-src/%s", schema_src );
        if ( rc != 0 )
            ERROR ( rc, "KConfigOpenNodeRead - failed to find entry 'sra-sort/schema-src/%s'", schema_src );
        else
        {
            rc = KConfigNodeRead ( n, 0, schema_src, size - 1, & num_read, & remaining );
            if ( rc != 0 )
                ERROR ( rc, "KConfigNodeRead failed" );
            else if ( remaining != 0 )
            {
                rc = RC ( rcExe, rcNode, rcReading, rcBuffer, rcInsufficient );
                ERROR ( rc, "type map of '%s' is too long", in );
            }
            else
            {
                schema_src [ num_read ] = 0;
            }
        }
    }

    KConfigNodeRelease ( n );
    return true;
}

static
VSchema *map_schema_types ( TypeParams *type, const ctx_t *ctx, const VSchema *src_schema )
{
    FUNC_ENTRY ( ctx );

    bool mapped;
    char schema_src [ 256 ];
    assert ( sizeof schema_src == sizeof type -> dst_type );

    TRY ( mapped = map_typename ( ctx, "out-map", type -> src_type, type -> dst_type, schema_src, sizeof schema_src ) )
    {
        rc_t rc;
        VSchema *dst_schema;

        if ( ! mapped )
        {
            type -> view_type [ 0 ] = 0;
            rc = VSchemaAddRef ( src_schema );
            if ( rc != 0 )
                ERROR ( rc, "VSchemaAddRef failed" );
            return ( VSchema* ) src_schema;
        }

        rc = VDBManagerMakeSchema ( ctx -> caps -> vdb, & dst_schema );
        if ( rc != 0 )
            ERROR ( rc, "VDBManagerMakeSchema failed" );
        else
        {
            rc = VSchemaParseFile ( dst_schema, "%s", schema_src );
            if ( rc != 0 )
                ERROR ( rc, "VSchemaParseFile failed adding file '%s' for destination", src_schema );
            else
            {
                TRY ( mapped = map_typename ( ctx, "view-map", type -> src_type, type -> view_type, schema_src, sizeof schema_src ) )
                {
                    if ( ! mapped )
                    {
                        type -> view_type [ 0 ] = 0;
                        return dst_schema;
                    }

                    rc = VSchemaParseFile ( dst_schema, "%s", schema_src );
                    if ( rc == 0 )
                        return dst_schema;

                    ERROR ( rc, "VSchemaParseFile failed adding file '%s' for view", src_schema );
                }
            }

            VSchemaRelease ( dst_schema );
        }
    }

    return NULL;
}

/* open_db
 *  called from run
 *  determines the type of schema
 *  opens output object
 *  dispatches to the appropriate handler
 */
static
void open_db ( const ctx_t *ctx, const VDatabase **srcp )
{
    FUNC_ENTRY ( ctx );

    const VDatabase *src = *srcp;

    const VSchema *src_schema;
    rc_t rc = VDatabaseOpenSchema ( src, & src_schema );
    if ( rc != 0 )
        ERROR ( rc, "VDatabaseOpenSchema failed" );
    else
    {
        TypeParams type;
        rc = VDatabaseTypespec ( src, type . src_type, sizeof type . src_type );
        if ( rc != 0 )
            ERROR ( rc, "failed to obtain database typespec" );
        else
        {
            VSchema *dst_schema;

            /* map input type to output type */
            TRY ( dst_schema = map_schema_types ( & type, ctx, src_schema ) )
            {
                const Tool *tp = ctx -> caps -> tool;

                /* if view was remapped, reopen the database */
                if ( type . view_type [ 0 ] != 0 )
                {
                    VSchemaRelease ( src_schema );
                    rc = VSchemaAddRef ( dst_schema );
                    if ( rc != 0 )
                    {
                        ERROR ( rc, "VSchemaAddRef failed" );
                        src_schema = NULL;
                    }
                    else
                    {
                        src_schema = dst_schema;
                        VDatabaseRelease ( src );
                        rc = VDBManagerOpenDBRead ( ctx -> caps -> vdb, srcp, src_schema, "%s", tp -> src_path );
                        if ( rc != 0 )
                            ERROR ( rc, "VDBManagerOpenDBRead failed reopening db '%s'", tp -> src_path );
                        src = *srcp;
                    }
                }

                if ( ! FAILED () )
                {
                    VDatabase *dst;
                    rc = VDBManagerCreateDB ( ctx -> caps -> vdb, & dst, dst_schema,
                                              type . dst_type, tp -> db . cmode, "%s", tp -> dst_path );
                    if ( rc != 0 )
                        ERROR ( rc, "VDBManagerCreateDB failed to create '%s' with type '%s'", tp -> dst_path, type . dst_type );
                    else
                    {
                        rc = VDatabaseColumnCreateParams ( dst, tp -> col . cmode, tp -> col . checksum, tp -> col . pgsize );
                        if ( rc != 0 )
                            ERROR ( rc, "VDatabaseColumnCreateParams: failed to set column create params on db '%s'", tp -> dst_path );
                        else
                        {
                            DbPair *pb;

                            /* TBD - this has to be fixed to use proper stuff */
                            const char *name = strrchr ( tp -> src_path, '/' );
                            if ( name ++ == NULL )
                                name = tp -> src_path;

                            TRY ( pb = DbPairMake ( ctx, src, dst, name ) )
                            {
                                DbPairRun ( pb, ctx );
                                DbPairRelease ( pb, ctx );
                            }
                        }

                        rc = VDatabaseRelease ( dst );
                        if ( rc != 0 )
                            ERROR ( rc, "VDatabaseRelease failed on '%s'", tp -> dst_path );
                    }
                }

                VSchemaRelease ( dst_schema );
            }
        }

        VSchemaRelease ( src_schema );
    }
}


/* open_tbl
 *  called from run
 *  determines the type of schema
 *  opens output object
 *  dispatches to the appropriate handler
 */
static
void open_tbl ( const ctx_t *ctx, const VTable **tblp )
{
    FUNC_ENTRY ( ctx );

    rc_t rc = RC ( rcExe, rcFunction, rcExecuting, rcFunction, rcUnsupported );
    ERROR ( rc, "unimplemented function" );
}

/* Sep. 2022 : to make sure that the column-oriented mode of sra-sort does not
 * invalidate the STATS-metadata-node in all tables of the output database it was
 * requested to unconditionaly copy the STATS-metadata-node of all tables of the
 * src-database into the dst-database
 *
 * This function only succeeds if the dst-database has been closed!
 *
 */
rc_t copy_stats_metadata( const char * src_path, const char * dst_path ) {
    VDBManager * mgr;
    rc_t rc = VDBManagerMakeUpdate( &mgr, NULL );
    if ( 0 != rc ) {
        LogErr( klogErr, rc, "cannot create VDB-update-manager" );
    } else {
        const VDatabase * src;
        rc = VDBManagerOpenDBRead( mgr, &src, NULL, "%s", src_path );
        if ( 0 != rc ) {
            pLogErr( klogErr, rc, "cannot open source-database: $(db)", "db=%s", src_path );
        } else {
            VDatabase * dst;
            rc = VDBManagerOpenDBUpdate( mgr, & dst, NULL, "%s", dst_path );
            if ( 0 != rc ) {
                LogErr( klogErr, rc, "cannot open dst-database" );
            } else {
                bool equal;
                rc = VDatabaseMetaCompare( src, dst, "STATS", NULL, &equal );
                if ( 0 != rc ) {
                    LogErr( klogErr, rc, "cannot compare metadata on databases" );
                } else {
                    if ( !equal ) {
                        LogErr( klogInfo, rc, "STATS metadata on databases differ" );
                        rc = VDatabaseMetaCopy( dst, src, "STATS", NULL, false );
                        if ( 0 != rc ) {
                            LogErr( klogErr, rc, "cannot copy metadata for databases" );
                        } else {
                            rc = VDatabaseMetaCompare( src, dst, "STATS", NULL, &equal );
                            if ( 0 != rc ) {
                                LogErr( klogErr, rc, "cannot compare metadata on databases after copy" );
                            } else if ( !equal ) {
                                LogErr( klogErr, rc, "metadata for databases still not equal, even after copy" );
                            } else {
                                LogErr( klogInfo, rc, "STATS metadata successfully copied" );
                            }
                        }
                    }
                }
                VDatabaseRelease ( dst );
            }
            VDatabaseRelease( src );
        }
        VDBManagerRelease( mgr );
    }
    return rc;
}

/* run
 *  called from main
 *  determines the type of object being copied/sorted
 *  opens input object
 *  dispatches to the appropriate handler
 */
void run ( const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    VDBManager *mgr = ctx -> caps -> vdb;
    const Tool *tp = ctx -> caps -> tool;

    const VDatabase *db;
    rc_t rc = VDBManagerOpenDBRead ( mgr, & db, NULL, "%s", tp -> src_path );
    if ( rc == 0 )
    {
        open_db ( ctx, & db );  /* <--- the meat!!! */
        VDatabaseRelease ( db );
    }
    else
    {
        const VTable *tbl;
        rc_t rc2 = VDBManagerOpenTableRead ( mgr, & tbl, NULL, "%s", tp -> src_path );
        if ( rc2 == 0 )
        {
            rc = 0;
            open_tbl ( ctx, & tbl );
            VTableRelease ( tbl );
        }
        else
        {
            VSchema *sra_dflt;
            rc2 = VDBManagerMakeSRASchema ( mgr, & sra_dflt );
            if ( rc2 == 0 )
            {
                rc2 = VDBManagerOpenTableRead ( mgr, & tbl, sra_dflt, "%s", tp -> src_path );
                if ( rc2 == 0 )
                {
                    rc = 0;
                    open_tbl ( ctx, & tbl );
                    VTableRelease ( tbl );
                }

                VSchemaRelease ( sra_dflt );
            }
        }
    }

    if ( rc != 0 )
        ERROR ( rc, "failed to open object '%s'", tp -> src_path );
}
