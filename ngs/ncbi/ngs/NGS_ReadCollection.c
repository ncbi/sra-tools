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

#include "NGS_ReadCollection.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/ReadCollectionItf.h>

#include "NGS_String.h"
#include "NGS_ReadGroup.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <kfg/kfg-priv.h>
#include <kfg/repository.h>

#include <vdb/vdb-priv.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <sra/sraschema.h>

#include <stddef.h>
#include <assert.h>

#include <sysalloc.h>

/*--------------------------------------------------------------------------
 * NGS_ReadCollection_v1
 */

#define Self( obj ) \
    ( ( NGS_ReadCollection* ) ( obj ) )

static NGS_String_v1 * NGS_ReadCollection_v1_get_name ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_ReadCollectionGetName ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static struct NGS_ReadGroup_v1 * NGS_ReadCollection_v1_get_read_groups ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_ReadGroup * ret = NGS_ReadCollectionGetReadGroups ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_ReadGroup_v1 * ) ret;
}

static bool NGS_ReadCollection_v1_has_read_group ( const NGS_ReadCollection_v1 * self, const char * spec )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    bool ret = NGS_ReadCollectionHasReadGroup ( Self ( self ), ctx, spec );
    CLEAR ();
    return ret;
}

static struct NGS_ReadGroup_v1 * NGS_ReadCollection_v1_get_read_group ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, const char * spec )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_ReadGroup * ret = NGS_ReadCollectionGetReadGroup ( Self ( self ), ctx, spec ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_ReadGroup_v1 * ) ret;
}

static struct NGS_Reference_v1 * NGS_ReadCollection_v1_get_references ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Reference * ret = NGS_ReadCollectionGetReferences ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Reference_v1 * ) ret;
}

static bool NGS_ReadCollection_v1_has_reference ( const NGS_ReadCollection_v1 * self, const char * spec )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    bool ret = NGS_ReadCollectionHasReference ( Self ( self ), ctx, spec );
    CLEAR ();
    return ret;
}

static struct NGS_Reference_v1 * NGS_ReadCollection_v1_get_reference ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, const char * spec )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Reference * ret = NGS_ReadCollectionGetReference ( Self ( self ), ctx, spec ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Reference_v1 * ) ret;
}

static struct NGS_Alignment_v1 * NGS_ReadCollection_v1_get_alignment ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, const char * alignmentId )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReadCollectionGetAlignment ( Self ( self ), ctx, alignmentId ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

static struct NGS_Alignment_v1 * NGS_ReadCollection_v1_get_alignments ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReadCollectionGetAlignments ( Self ( self ), ctx, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

static uint64_t NGS_ReadCollection_v1_get_align_count ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_ReadCollectionGetAlignmentCount ( Self ( self ), ctx, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static struct NGS_Alignment_v1 * NGS_ReadCollection_v1_get_align_range ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, uint64_t first, uint64_t count, bool wants_primary, bool wants_secondary )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Alignment * ret = NGS_ReadCollectionGetAlignmentRange ( Self ( self ), ctx, first, count, wants_primary, wants_secondary ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Alignment_v1 * ) ret;
}

static struct NGS_Read_v1 * NGS_ReadCollection_v1_get_read ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, const char * readId )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Read * ret = NGS_ReadCollectionGetRead ( Self ( self ), ctx, readId ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Read_v1 * ) ret;
}

static struct NGS_Read_v1 * NGS_ReadCollection_v1_get_reads ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_full, bool wants_partial, bool wants_unaligned )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Read * ret = NGS_ReadCollectionGetReads ( Self ( self ), ctx, wants_full, wants_partial, wants_unaligned ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Read_v1 * ) ret;
}

static uint64_t NGS_ReadCollection_v1_get_read_count ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, bool wants_full, bool wants_partial, bool wants_unaligned )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_ReadCollectionGetReadCount ( Self ( self ), ctx, wants_full, wants_partial, wants_unaligned ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

struct NGS_Read_v1 * NGS_ReadCollection_v1_read_range ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err, uint64_t first, uint64_t count, bool wants_full, bool wants_partial, bool wants_unaligned )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Read * ret = NGS_ReadCollectionGetReadRange ( Self ( self ), ctx, first, count, wants_full, wants_partial, wants_unaligned ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Read_v1 * ) ret;
}

#undef Self


NGS_ReadCollection_v1_vt ITF_ReadCollection_vt =
{
    {
        "NGS_ReadCollection",
        "NGS_ReadCollection_v1",
        1,
        & ITF_Refcount_vt . dad
    },

    /* v1.0 */
    NGS_ReadCollection_v1_get_name,
    NGS_ReadCollection_v1_get_read_groups,
    NGS_ReadCollection_v1_get_read_group,
    NGS_ReadCollection_v1_get_references,
    NGS_ReadCollection_v1_get_reference,
    NGS_ReadCollection_v1_get_alignment,
    NGS_ReadCollection_v1_get_alignments,
    NGS_ReadCollection_v1_get_align_count,
    NGS_ReadCollection_v1_get_align_range,
    NGS_ReadCollection_v1_get_read,
    NGS_ReadCollection_v1_get_reads,
    NGS_ReadCollection_v1_get_read_count,
	NGS_ReadCollection_v1_read_range,

    /* v1.1 */
	NGS_ReadCollection_v1_has_read_group,
	NGS_ReadCollection_v1_has_reference
};


/*--------------------------------------------------------------------------
 * NGS_ReadCollection
 */

#define VT( self, msg ) \
    ( ( ( const NGS_ReadCollection_vt* ) ( self ) -> dad . vt ) -> msg )

/* GetName
 */
struct NGS_String * NGS_ReadCollectionGetName ( NGS_ReadCollection * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get name" );
    }
    else
    {
        return VT ( self, get_name ) ( self, ctx );
    }

    return NULL;
}


/* READ GROUPS
 */
struct NGS_ReadGroup * NGS_ReadCollectionGetReadGroups ( NGS_ReadCollection * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read group iterator" );
    }
    else
    {
        return VT ( self, get_read_groups ) ( self, ctx );
    }

    return NULL;
}

bool NGS_ReadCollectionHasReadGroup ( NGS_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    if ( self == NULL )
        INTERNAL_WARNING ( xcSelfNull, "failed to get read group '%.128s'", spec );
    else if ( spec == NULL )
        INTERNAL_WARNING ( xcParamNull, "read group spec" );
    else
    {
        POP_CTX ( ctx );
        return VT ( self, has_read_group ) ( self, ctx, spec [ 0 ] == 0 ? DEFAULT_READGROUP_NAME : spec );
    }

    return false;
}

struct NGS_ReadGroup * NGS_ReadCollectionGetReadGroup ( NGS_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    if ( self == NULL )
        INTERNAL_ERROR ( xcSelfNull, "failed to get read group '%.128s'", spec );
    else if ( spec == NULL )
        INTERNAL_ERROR ( xcParamNull, "read group spec" );
    else
    {
        POP_CTX ( ctx );
        return VT ( self, get_read_group ) ( self, ctx, spec [ 0 ] == 0 ? DEFAULT_READGROUP_NAME : spec );
    }

    return NULL;
}


/* REFERENCES
 */
struct NGS_Reference * NGS_ReadCollectionGetReferences ( NGS_ReadCollection * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference iterator" );
    }
    else
    {
        return VT ( self, get_references ) ( self, ctx );
    }

    return NULL;
}

bool NGS_ReadCollectionHasReference ( NGS_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    if ( self == NULL )
        INTERNAL_WARNING ( xcSelfNull, "failed to get reference '%.128s'", spec );
    else if ( spec == NULL )
        INTERNAL_WARNING ( xcParamNull, "NULL reference spec" );
    else if ( spec [ 0 ] == 0 )
        INTERNAL_WARNING ( xcStringEmpty, "empty reference spec" );
    else
    {
        POP_CTX ( ctx );
        return VT ( self, has_reference ) ( self, ctx, spec );
    }

    return false;
}

struct NGS_Reference * NGS_ReadCollectionGetReference ( NGS_ReadCollection * self, ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    if ( self == NULL )
        INTERNAL_ERROR ( xcSelfNull, "failed to get reference '%.128s'", spec );
    else if ( spec == NULL )
        INTERNAL_ERROR ( xcParamNull, "NULL reference spec" );
    else if ( spec [ 0 ] == 0 )
        INTERNAL_ERROR ( xcStringEmpty, "empty reference spec" );
    else
    {
        POP_CTX ( ctx );
        return VT ( self, get_reference ) ( self, ctx, spec );
    }

    return NULL;
}


/* ALIGNMENTS
 */
struct NGS_Alignment * NGS_ReadCollectionGetAlignments ( NGS_ReadCollection * self, ctx_t ctx,
    bool wants_primary, bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment iterator" );
    }
    else
    {
        return VT ( self, get_alignments ) ( self, ctx, wants_primary, wants_secondary );
    }

    return NULL;
}

struct NGS_Alignment * NGS_ReadCollectionGetAlignment ( NGS_ReadCollection * self, ctx_t ctx, const char * alignmentId )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    if ( self == NULL )
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment '%.128s'", alignmentId );
    else if ( alignmentId == NULL )
        INTERNAL_ERROR ( xcParamNull, "alignment id" );
    else if ( alignmentId [ 0 ] == 0 )
        INTERNAL_ERROR ( xcStringEmpty, "alignment id" );
    else
    {
        return VT ( self, get_alignment ) ( self, ctx, alignmentId );
    }

    return NULL;
}

uint64_t NGS_ReadCollectionGetAlignmentCount ( NGS_ReadCollection * self, ctx_t ctx, bool wants_primary, bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get alignment count" );
    }
    else
    {
        return VT ( self, get_alignment_count ) ( self, ctx, wants_primary, wants_secondary );
    }

    return 0;
}

struct NGS_Alignment * NGS_ReadCollectionGetAlignmentRange ( NGS_ReadCollection * self, ctx_t ctx, uint64_t first, uint64_t count,
    bool wants_primary, bool wants_secondary )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read range first = %lu count = %lu", first, count );
    }
    else
    {
        return VT ( self, get_alignment_range ) ( self, ctx, first, count, wants_primary, wants_secondary );
    }

    return NULL;
}

/* READS
 */
struct NGS_Read * NGS_ReadCollectionGetReads ( NGS_ReadCollection * self, ctx_t ctx,
    bool wants_full, bool wants_partial, bool wants_unaligned )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read iterator" );
    }
    else
    {
        return VT ( self, get_reads ) ( self, ctx, wants_full, wants_partial, wants_unaligned );
    }

    return NULL;
}

struct NGS_Read * NGS_ReadCollectionGetRead ( NGS_ReadCollection * self, ctx_t ctx, const char * readId )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    if ( self == NULL )
        INTERNAL_ERROR ( xcSelfNull, "failed to get read '%.128s'", readId );
    else if ( readId == NULL )
        INTERNAL_ERROR ( xcParamNull, "read id" );
    else if ( readId [ 0 ] == 0 )
        INTERNAL_ERROR ( xcStringEmpty, "read id" );
    else
    {
        return VT ( self, get_read ) ( self, ctx, readId );
    }

    return NULL;
}

uint64_t NGS_ReadCollectionGetReadCount ( NGS_ReadCollection * self, ctx_t ctx,
    bool wants_full, bool wants_partial, bool wants_unaligned )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read count" );
    }
    else
    {
        return VT ( self, get_read_count ) ( self, ctx, wants_full, wants_partial, wants_unaligned );
    }

    return 0;
}

struct NGS_Read * NGS_ReadCollectionGetReadRange ( NGS_ReadCollection * self, ctx_t ctx, uint64_t first, uint64_t count,
        bool wants_full, bool wants_partial, bool wants_unaligned  )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read range first = %lu count = %lu", first, count );
    }
    else
    {
        return VT ( self, get_read_range ) ( self, ctx, first, count, wants_full, wants_partial, wants_unaligned );
    }

    return NULL;
}

struct NGS_Statistics* NGS_ReadCollectionGetStatistics ( NGS_ReadCollection * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get statistics" );
    }
    else
    {
        return VT ( self, get_statistics ) ( self, ctx );
    }

    return NULL;
}

struct NGS_FragmentBlobIterator* NGS_ReadCollectionGetFragmentBlobs ( NGS_ReadCollection * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get fragment blobs" );
    }
    else
    {
        return VT ( self, get_frag_blobs ) ( self, ctx );
    }

    return NULL;
}


/* Make
 *  use provided specification to create an object
 *  any error returns NULL as a result and sets error in ctx
 */
NGS_ReadCollection * NGS_ReadCollectionMake ( ctx_t ctx, const char * spec )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcConstructing );

    if ( spec == NULL )
        USER_ERROR ( xcParamNull, "NULL read-collection specification string" );
    else if ( spec [ 0 ] == 0 )
        USER_ERROR ( xcStringEmpty, "empty read-collection specification string" );
    else
    {
        rc_t rc;
        const VDatabase *db;
        VSchema *sra_schema;

        /* the first order of work is to determine what type of object is in "spec" */
        const VDBManager * mgr = ctx -> rsrc -> vdb;
        assert ( mgr != NULL );

        /* try as VDB database */
        rc = VDBManagerOpenDBRead ( mgr, & db, NULL, "%s", spec );
        if ( rc == 0 )
        {
            /* test for cSRA */
            if ( VDatabaseIsCSRA ( db ) )
                return NGS_ReadCollectionMakeCSRA ( ctx, db, spec );

            /* non-aligned */
            return NGS_ReadCollectionMakeVDatabase ( ctx, db, spec );
        }

        /* try as VDB table */
        rc = VDBManagerMakeSRASchema ( mgr, & sra_schema );
        if ( rc != 0 )
            INTERNAL_ERROR ( xcUnexpected, "failed to make default SRA schema: rc = %R", rc );
        else
        {
            const VTable *tbl;
            rc = VDBManagerOpenTableRead ( mgr, & tbl, sra_schema, "%s", spec );
            VSchemaRelease ( sra_schema );

            if ( rc == 0 )
            {   /* VDB-2641: examine the schema name to make sure this is an SRA table */
                char ts_buff[1024];
                rc = VTableTypespec ( tbl, ts_buff, sizeof ( ts_buff ) );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( xcUnexpected, "VTableTypespec failed: rc = %R", rc );
                }
                else
                {
                    const char SRA_PREFIX[] = "NCBI:SRA:";
                    size_t pref_size = sizeof ( SRA_PREFIX ) - 1;
                    if ( string_match ( SRA_PREFIX, pref_size, ts_buff, string_size ( ts_buff ), pref_size, NULL ) == pref_size )
                    {
                        return NGS_ReadCollectionMakeVTable ( ctx, tbl, spec );
                    }
                    USER_ERROR ( xcTableOpenFailed, "Cannot open accession '%s' as an SRA table.", spec );
                }
            }
            else
            {
                KConfig* kfg = NULL;
                const KRepositoryMgr* repoMgr = NULL;
                if ( KConfigMakeLocal ( & kfg, NULL ) != 0 ||
                     KConfigMakeRepositoryMgrRead ( kfg, & repoMgr ) != 0 ||
                     KRepositoryMgrHasRemoteAccess ( repoMgr ) )
                {
                    USER_ERROR ( xcTableOpenFailed, "Cannot open accession '%s', rc = %R", spec, rc );
                }
                else
                {
                    USER_ERROR ( xcTableOpenFailed, "Cannot open accession '%s', rc = %R. Note: remote access is disabled in the configuration.", spec, rc );
                }
                KRepositoryMgrRelease ( repoMgr );
                KConfigRelease ( kfg );
            }
            VTableRelease ( tbl );
        }
    }

    return NULL;
}

/* Init
 */
void NGS_ReadCollectionInit ( ctx_t ctx, NGS_ReadCollection * ref,
    const NGS_ReadCollection_vt *vt, const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcConstructing );
    TRY ( NGS_RefcountInit ( ctx, & ref -> dad, & ITF_ReadCollection_vt . dad, & vt -> dad, clsname, instname ) )
    {
        assert ( vt -> get_name != NULL );
        assert ( vt -> get_read_groups != NULL );
        assert ( vt -> has_read_group != NULL );
        assert ( vt -> get_read_group != NULL );
        assert ( vt -> get_references != NULL );
        assert ( vt -> has_reference != NULL );
        assert ( vt -> get_reference != NULL );
        assert ( vt -> get_alignments != NULL );
        assert ( vt -> get_alignment != NULL );
        assert ( vt -> get_alignment_count != NULL );
        assert ( vt -> get_alignment_range != NULL );
        assert ( vt -> get_reads != NULL );
        assert ( vt -> get_read != NULL );
        assert ( vt -> get_read_range != NULL );
        assert ( vt -> get_read_count != NULL );
        assert ( vt -> get_statistics != NULL );
        assert ( vt -> get_frag_blobs != NULL );
    }
}
