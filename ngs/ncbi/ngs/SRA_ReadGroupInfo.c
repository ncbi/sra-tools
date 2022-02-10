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

#include "SRA_ReadGroupInfo.h"

#include "NGS_String.h"

#include <klib/namelist.h>
#include <klib/rc.h>

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <kdb/meta.h>
#include <kdb/namelist.h>

#include <vdb/table.h>

#include <sysalloc.h>
#include <string.h>

const SRA_ReadGroupInfo* SRA_ReadGroupInfoDuplicate ( const SRA_ReadGroupInfo* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );

    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "SRA_ReadGroupInfo" ) )
        {
        case krefOkay:
            break;
        case krefLimit:
            {
                FUNC_ENTRY ( ctx, rcSRA, rcRefcount, rcAttaching );
                INTERNAL_ERROR ( xcRefcountOutOfBounds, "SRA_ReadGroupInfo at %#p", self );
                atomic32_set ( & ( ( SRA_ReadGroupInfo * ) self ) -> refcount, 0 );
                break;
            }
        }
    }

    return self;
}

static
void
SRA_ReadGroupInfoWhack( const SRA_ReadGroupInfo* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    uint32_t i;
    for ( i = 0; i < self -> count; ++i )
    {
        NGS_StringRelease ( self -> groups [ i ] . name, ctx );
    }
    
    free ( ( void * ) self );
}

static
uint64_t ReadU64 ( const struct KMetadata * meta, ctx_t ctx, const char* fmt, const char* name )
{
    uint64_t ret = 0;
    
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, & node, fmt, name );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "KMetadataOpenNodeRead(%s) rc = %R", name, rc );
    }
    else
    {
        rc = KMDataNodeReadAsU64 ( node, & ret );
        if ( rc != 0 )
        {
            INTERNAL_ERROR ( xcUnexpected, "KMDataNodeReadAsU64(%s) rc = %R", name, rc );
        }
        KMDataNodeRelease ( node );
    }
    
    return ret;
}            

static
void ParseBamHeader ( struct SRA_ReadGroupStats * self, ctx_t ctx, const struct KMetadata * meta, const char* group_name )
{

}

static
void SRA_ReadGroupStatsInit( struct SRA_ReadGroupStats * self, ctx_t ctx, const struct KMetadata * meta, const char* group_name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    const KMDataNode * group_node;
    bool skip_group = true;
    rc_t rc;
    
    assert ( self );
    assert ( meta );
    assert ( group_name );

    rc = KMetadataOpenNodeRead ( meta, & group_node, "STATS/SPOT_GROUP/%s", group_name );
    if ( rc != 0 )
    {
        if ( strcmp ( group_name, "default" ) == 0 &&
             rc == RC ( rcDB, rcMetadata, rcReading, rcTransfer, rcIncomplete ) )
        {   /* no default read group - skip */ 
        }
        else
        {
            INTERNAL_ERROR ( xcUnexpected, "KMetadataOpenNodeRead(STATS/SPOT_GROUP/%s) rc = %R", group_name, rc );
        }
    }
    
    ON_FAIL ( self -> min_row = ReadU64 ( meta, ctx, "STATS/SPOT_GROUP/%s/SPOT_MIN", group_name ) )
    {
        if ( strcmp ( group_name, "default" ) == 0 )
        {   /* default read group has no data - skip */ 
            CLEAR ();
        }
        else
        {
            INTERNAL_ERROR ( xcUnexpected, "KMetadataOpenNodeRead(STATS/SPOT_GROUP/%s/SPOT_MIN) rc = %R", group_name, ctx -> rc );
        }
    }
    else
    {
        TRY ( self -> max_row = ReadU64 ( meta, ctx, "STATS/SPOT_GROUP/%s/SPOT_MAX", group_name ) )
        {
            TRY ( self -> row_count = ReadU64 ( meta, ctx, "STATS/SPOT_GROUP/%s/SPOT_COUNT", group_name ) )
            {
                TRY ( self -> base_count = ReadU64 ( meta, ctx, "STATS/SPOT_GROUP/%s/BASE_COUNT", group_name ) )
                {
                    TRY ( self -> bio_base_count = ReadU64 ( meta, ctx, "STATS/SPOT_GROUP/%s/BIO_BASE_COUNT", group_name ) )
                    {
                        skip_group = false;
                    }
                }
            }
        }
    }
    
    if ( skip_group )
    {
        self -> name = NGS_StringMake( ctx, "", 0 );
    }
    else
    {
        TRY ( ParseBamHeader ( self, ctx, meta, group_name ) )
        {
            /* if the node has attribute 'name', use it as the read group name, otherwise use the group node's key */
            char buf[1024];
            size_t size;
            rc = KMDataNodeReadAttr ( group_node, "name", buf, sizeof ( buf ), & size );
            if ( rc == 0 )
            {
                self -> name = NGS_StringMakeCopy ( ctx, buf, size );
            }
            else if ( GetRCState ( rc ) == rcNotFound )
            {
                self -> name = NGS_StringMakeCopy ( ctx, group_name, string_size ( group_name ) );
            }
            else
            {
                INTERNAL_ERROR ( xcUnexpected, "KMDataNodeReadAttr(STATS/SPOT_GROUP/%s, 'name') rc = %R", group_name, rc );
            }
        }
    }
    
    KMDataNodeRelease ( group_node );
}

void SRA_ReadGroupInfoRelease ( const SRA_ReadGroupInfo* self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "NGS_Refcount" ) )
        {
        case krefOkay:
            break;
        case krefWhack:
            SRA_ReadGroupInfoWhack ( self, ctx );
            break;
        case krefNegative:
            {
                FUNC_ENTRY ( ctx, rcSRA, rcRefcount, rcReleasing );
                INTERNAL_ERROR ( xcSelfZombie, "SRA_ReadGroupInfo at %#p", self );
                atomic32_set ( & ( ( SRA_ReadGroupInfo * ) self ) -> refcount, 0 );
                break;
            }
        }
    }
}

const SRA_ReadGroupInfo* SRA_ReadGroupInfoMake ( ctx_t ctx, const struct VTable* table )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    rc_t rc = 0;
    const struct KMetadata * meta;
    
    assert ( table != NULL );
    
    rc = VTableOpenMetadataRead ( table, & meta );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "VTableOpenMetadataRead rc = %R", rc );
    }
    else
    {
        const KMDataNode * groups;
        rc = KMetadataOpenNodeRead ( meta, & groups, "STATS/SPOT_GROUP" );
        if ( rc != 0 )
        {
            INTERNAL_ERROR ( xcUnexpected, "KMetadataOpenNodeRead rc = %R", rc );
        }
        else
        {
            struct KNamelist * names;
            rc = KMDataNodeListChildren ( groups, & names );
            if ( rc != 0 )
            {
                INTERNAL_ERROR ( xcUnexpected, "KMDataNodeListChildren rc = %R", rc );
            }
            else
            {
                uint32_t count;
                rc = KNamelistCount ( names, & count );
                if ( rc != 0 )
                {
                    INTERNAL_ERROR ( xcUnexpected, "KNamelistCount rc = %R", rc );
                }
                else
                {
                    SRA_ReadGroupInfo* ref = calloc ( 1, sizeof ( * ref ) + ( count - 1 ) * sizeof ( ref -> groups [ 0 ] ) );
                    if ( ref == NULL )
                    {
                        SYSTEM_ERROR ( xcNoMemory, "allocating SRA_ReadGroupInfo" );
                    }
                    else
                    {
                        uint32_t i;
                        
                        KRefcountInit ( & ref -> refcount, 1, "SRA_ReadGroupInfo", "Make", "" );
                        ref -> count = count;
                        
                        for ( i = 0; i < count; ++i )
                        {
                            const char* group_name;
                            rc = KNamelistGet ( names, i, & group_name );
                            if ( rc != 0 )
                                INTERNAL_ERROR ( xcUnexpected, "KNamelistGet = %R", rc );
                            else
                                SRA_ReadGroupStatsInit ( & ref -> groups [ i ], ctx, meta, group_name );
                                                       
                            if ( FAILED () )
                                break;
                        }
               
                        if ( ! FAILED () )
                        {
                            KNamelistRelease ( names );
                            KMDataNodeRelease ( groups );
                            KMetadataRelease ( meta );
                            
                            return ref;
                        }
                        
                        SRA_ReadGroupInfoWhack ( ref, ctx );
                    }
                }
                KNamelistRelease ( names );
            }
            KMDataNodeRelease ( groups );
        }
        KMetadataRelease ( meta );
    }
    return NULL;
}

uint32_t SRA_ReadGroupInfoFind ( const SRA_ReadGroupInfo* self, ctx_t ctx, char const* name, size_t name_size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self != NULL );
    assert ( name != NULL );

    {
        uint32_t i;
        for ( i = 0; i < self -> count; ++i )
        {
            if ( string_cmp ( NGS_StringData ( self -> groups [ i ] . name, ctx ), 
                              NGS_StringSize ( self -> groups [ i ] . name, ctx ), 
                              name, 
                              name_size, 
                              ( uint32_t ) name_size ) == 0 )
            {
                return i;
            }
        }
        INTERNAL_ERROR ( xcStringNotFound, "Read Group '%.*s' is not found", name_size, name );
    }
    return 0;
}


