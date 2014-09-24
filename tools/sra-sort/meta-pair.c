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

typedef struct MetaPair StdMetaPair;
#define METAPAIR_IMPL StdMetaPair

#include "meta-pair.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"

#include <kdb/meta.h>
#include <kdb/namelist.h>
#include <klib/namelist.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>


FILE_ENTRY ( meta-pair );


/*--------------------------------------------------------------------------
 * StdMetaPair
 */

static
void StdMetaPairWhack ( StdMetaPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    MetaPairDestroy ( self, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static MetaPair_vt StdMetaPair_vt =
{
    StdMetaPairWhack
};


/*--------------------------------------------------------------------------
 * MetaPair
 *  interface to pairing of source and destination tables
 */


/* Make
 *  make a standard metadata pair from existing KMetadata objects
 */
MetaPair *MetaPairMake ( const ctx_t *ctx,
    const KMetadata *src, KMetadata *dst, const char *full_path )
{
    FUNC_ENTRY ( ctx );

    StdMetaPair *meta;

    TRY ( meta = MemAlloc ( ctx, sizeof * meta, false ) )
    {
        TRY ( MetaPairInit ( meta, ctx, & StdMetaPair_vt, src, dst, full_path ) )
        {
            return meta;
        }

        MemFree ( ctx, meta, sizeof * meta );
    }

    return NULL;
}


/* Release
 *  called by table at end of copy
 */
void MetaPairRelease ( MetaPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "MetaPair" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            ( * self -> vt -> whack ) ( ( void* ) self, ctx );
            break;
        case krefZero:
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcMetadata, rcDestroying, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to release MetaPair" );
        }
    }
}

/* Duplicate
 */
MetaPair *MetaPairDuplicate ( MetaPair *self, const ctx_t *ctx )
{
    rc_t rc;
    FUNC_ENTRY ( ctx );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "MetaPair" ) )
        {
        case krefOkay:
        case krefWhack:
        case krefZero:
            break;
        case krefLimit:
        case krefNegative:
            rc = RC ( rcExe, rcMetadata, rcAttaching, rcRefcount, rcInvalid );
            INTERNAL_ERROR ( rc, "failed to duplicate MetaPair" );
            return NULL;
        }
    }

    return ( MetaPair* ) self;
}


/* Init
 */
void MetaPairInit ( MetaPair *self, const ctx_t *ctx, const MetaPair_vt *vt,
    const KMetadata *src, KMetadata *dst, const char *full_spec )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    if ( self == NULL )
    {
        rc = RC ( rcExe, rcMetadata, rcConstructing, rcSelf, rcNull );
        INTERNAL_ERROR ( rc, "bad MetaPair" );
        return;
    }

    memset ( self, 0, sizeof * self );
    self -> vt = vt;

    rc = KMetadataAddRef ( self -> smeta = src );
    if ( rc != 0 )
        ERROR ( rc, "failed to duplicate metadata 'src.%s'", full_spec );
    else
    {
        rc = KMetadataAddRef ( self -> dmeta = dst );
        if ( rc != 0 )
            ERROR ( rc, "failed to duplicate metadata 'dst.%s'", full_spec );
        else
        {
            KRefcountInit ( & self -> refcount, 1, "MetaPair", "init", full_spec );
            return;
        }

        KMetadataRelease ( self -> smeta );
        self -> smeta = NULL;
    }
}

/* Destroy
 */
void MetaPairDestroy ( MetaPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    rc = KMetadataRelease ( self -> dmeta );
    if ( rc != 0 )
        WARN ( "KMetadataRelease failed" );
    KMetadataRelease ( self -> smeta );

    memset ( self, 0, sizeof * self );
}


static
void copy_meta_node_value ( const KMDataNode *src, const ctx_t *ctx, KMDataNode *dst )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    char buff [ 4096 ];
    size_t num_read, remaining;

    rc = KMDataNodeRead ( src, 0, buff, sizeof buff, & num_read, & remaining );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "KMDataNodeRead failed" );
    else if ( num_read != 0 )
    {
        rc = KMDataNodeWrite ( dst, buff, num_read );
        if ( rc != 0 )
            SYSTEM_ERROR ( rc, "KMDataNodeWrite failed" );
        else
        {
            size_t offset;
            for ( offset = num_read; remaining != 0; offset += num_read )
            {
                rc = KMDataNodeRead ( src, offset, buff, sizeof buff, & num_read, & remaining );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( rc, "KMDataNodeRead failed" );
                    break;
                }
                rc = KMDataNodeAppend ( dst, buff, num_read );
                if ( rc != 0 )
                {
                    SYSTEM_ERROR ( rc, "KMDataNodeAppend failed" );
                    break;
                }
            }
        }
    }
}

static
void copy_meta_node_attr ( const KMDataNode *src, const ctx_t *ctx, KMDataNode *dst, const char *name )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    size_t num_read;
    char buff [ 4096 ];

    rc = KMDataNodeReadAttr ( src, name, buff, sizeof buff, & num_read );
    if ( rc != 0 )
        ERROR ( rc, "KMDataNodeReadAttr '%s' failed", name );
    else
    {
        rc = KMDataNodeWriteAttr ( dst, name, buff );
        if ( rc != 0 )
            ERROR ( rc, "KMDataNodeWriteAttr '%s' failed", name );
    }
}


static
const char *filter_node_name ( const char *name, size_t nsize, const char **exclude )
{
    uint32_t i;
    for ( i = 0; exclude [ i ] != NULL; ++ i )
    {
        const char *x = exclude [ i ];
        size_t xsize = strlen ( x );

        /* allow exclude to be a prefix */
        if ( xsize > 0 && x [ xsize - 1 ] == '*' )
        {
            if ( nsize < -- xsize )
                continue;
        }
        else if ( nsize != xsize )
        {
            continue;
        }

        if ( memcmp ( name, x, xsize ) == 0 )
            return NULL;
    }

    return name;
 }


static
void copy_meta_node ( const KMDataNode *src, const ctx_t *ctx, KMDataNode *dst,
    char path [ 4096 ], size_t psize, const char *owner_spec, const char **exclude )
{
    FUNC_ENTRY ( ctx );

    /* first, copy value */
    TRY ( copy_meta_node_value ( src, ctx, dst ) )
    {
        /* next, copy attributes */
        KNamelist *names;
        rc_t rc = KMDataNodeListAttr ( src, & names );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "KMDataNodeListAttr failed" );
        else
        {
            uint32_t count;
            rc = KNamelistCount ( names, & count );
            if ( rc != 0 )
                INTERNAL_ERROR ( rc, "KNamelistCount failed" );
            else
            {
                uint32_t i;
                const char *name;
                for ( i = 0; i < count; ++ i )
                {
                    rc = KNamelistGet ( names, i, & name );
                    if ( rc != 0 )
                    {
                        INTERNAL_ERROR ( rc, "KNamelistGet [ %u ] failed", i );
                        break;
                    }

                    ON_FAIL ( copy_meta_node_attr ( src, ctx, dst, name ) )
                        break;
                }

                if ( ! FAILED () )
                {
                    KNamelistRelease ( names );

                    /* last, copy children */
                    rc = KMDataNodeListChildren ( src, & names );
                    if ( rc != 0 )
                        INTERNAL_ERROR ( rc, "KMDataNodeListChildren failed" );
                    else
                    {
                        rc = KNamelistCount ( names, & count );
                        if ( rc != 0 )
                            INTERNAL_ERROR ( rc, "KNamelistCount failed" );
                        else if ( psize + 1 == 4096 )
                        {
                            rc = RC ( rcExe, rcNode, rcCopying, rcBuffer, rcInsufficient );
                            ERROR ( rc, "metadata path too long" );
                        }
                        else
                        {
                            for ( i = 0; i < count; ++ i )
                            {
                                KMDataNode *dchild;
                                const KMDataNode *schild;

                                size_t nsize;
                                static const char *always_exclude [] =
                                {
                                    ".seq", "col", "schema", 
                                    "HUFFMAN*", "MSC454*", "NREADS", "NUMBER_P*", "NUMBER_S*",
                                    NULL
                                };

                                rc = KNamelistGet ( names, i, & name );
                                if ( rc != 0 )
                                {
                                    INTERNAL_ERROR ( rc, "KNamelistGet [ %u ] failed", i );
                                    break;
                                }

                                nsize = string_size ( name );
                                if ( psize + nsize + 1 >= 4096 )
                                {
                                    WARN ( "skipping node '%.*s/%s' - path too long", ( uint32_t ) psize, path, name );
                                    continue;
                                }

                                if ( psize != 0 )
                                    path [ psize ++ ] = '/';
                                strcpy ( & path [ psize ], name );

                                if ( exclude != NULL )
                                {
                                    name = filter_node_name ( name, nsize, exclude );
                                    if ( name == NULL )
                                        continue;
                                }

                                name = filter_node_name ( name, nsize, always_exclude );
                                if ( name == NULL )
                                    continue;

                                rc = KMDataNodeOpenNodeRead ( src, & schild, "%s", name );
                                if ( rc != 0 )
                                    ERROR ( rc, "failed to open source metadata node '%s'", name );
                                else
                                {
                                    rc = KMDataNodeOpenNodeUpdate ( dst, & dchild, "%s", name );
                                    if ( rc != 0 )
                                        ERROR ( rc, "failed to open destination metadata node '%s'", name );
                                    else
                                    {
                                        copy_meta_node ( schild, ctx, dchild, path, psize + nsize, owner_spec, exclude );
                                        KMDataNodeRelease ( dchild );
                                    }

                                    KMDataNodeRelease ( schild );
                                }

                                if ( FAILED () )
                                    break;
                            }
                        }
                    }
                }
            }

            KNamelistRelease ( names );
        }
    }
}


/* Copy
 *  copy from source to destination
 */
void MetaPairCopy ( MetaPair *self, const ctx_t *ctx, const char *owner_spec, const char **exclude )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    char path [ 4096 ];
    const KMDataNode *src;

    STATUS ( 3, "copying '%s' metadata", owner_spec );

    rc = KMetadataOpenNodeRead ( self -> smeta, & src, "/" );
    if ( rc != 0 )
        INTERNAL_ERROR ( rc, "KMetadataOpenNodeRead failed to obtain '%s' root node", owner_spec );
    else
    {
        KMDataNode *dst;
        rc = KMetadataOpenNodeUpdate ( self -> dmeta, & dst, "/" );
        if ( rc != 0 )
            INTERNAL_ERROR ( rc, "KMetadataOpenNodeUpdate failed to obtain '%s' root node", owner_spec );
        else
        {
            path [ 0 ] = 0;
            copy_meta_node ( src, ctx, dst, path, 0, owner_spec, exclude );

            KMDataNodeRelease ( dst );
        }

        KMDataNodeRelease ( src );
    }

    STATUS ( 3, "finished copying metadata" );
}
