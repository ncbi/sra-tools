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

#include "NGS_ReadGroup.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/ReadGroupItf.h>

#include "NGS_String.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <klib/text.h>

#include <vdb/vdb-priv.h>

#include <stddef.h>
#include <assert.h>
#include <string.h>

#include <sysalloc.h>

/*--------------------------------------------------------------------------
 * NGS_ReadGroup_v1
 */

#define Self( obj ) \
    ( ( NGS_ReadGroup* ) ( obj ) )
    
static NGS_String_v1 * ITF_ReadGroup_v1_get_name ( const NGS_ReadGroup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_ReadGroupGetName ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static struct NGS_Statistics_v1 * ITF_ReadGroup_v1_get_stats ( const NGS_ReadGroup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_Statistics * ret = NGS_ReadGroupGetStatistics ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_Statistics_v1 * ) ret;
}

static bool ITF_ReadGroup_v1_next ( NGS_ReadGroup_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_ReadGroupIteratorNext ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

#undef Self


NGS_ReadGroup_v1_vt ITF_ReadGroup_vt =
{
    {
        "NGS_ReadGroup",
        "NGS_ReadGroup_v1",
        0,
        & ITF_Refcount_vt . dad
    },

    ITF_ReadGroup_v1_get_name,
    ITF_ReadGroup_v1_get_stats,
    ITF_ReadGroup_v1_next
};


/*--------------------------------------------------------------------------
 * NGS_ReadGroup
 */

#define VT( self, msg ) \
    ( ( ( const NGS_ReadGroup_vt* ) ( self ) -> dad . vt ) -> msg )

/* Init
*/    
void NGS_ReadGroupInit ( ctx_t ctx, NGS_ReadGroup * self, NGS_ReadGroup_vt * vt, const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );
    
    TRY ( NGS_RefcountInit ( ctx, & self -> dad, & ITF_ReadGroup_vt . dad, & vt -> dad, clsname, instname ) )
    {
        assert ( vt -> get_name != NULL );
        assert ( vt -> get_reads != NULL );
        assert ( vt -> get_read != NULL );
        assert ( vt -> get_statistics != NULL );
        assert ( vt -> get_next != NULL );
    }
}
    
/* GetName
 */
struct NGS_String * NGS_ReadGroupGetName ( const NGS_ReadGroup * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get name" );
    }
    else
    {
        NGS_String* ret = VT ( self, get_name ) ( self, ctx );
        if ( ret != NULL && string_cmp ( DEFAULT_READGROUP_NAME, 
                                         strlen ( DEFAULT_READGROUP_NAME ), 
                                         NGS_StringData ( ret, ctx ),
                                         NGS_StringSize ( ret, ctx ),
                                         ( uint32_t ) NGS_StringSize ( ret, ctx ) ) == 0 )
        {
            NGS_String* tmp = ret;
            ret = NGS_StringSubstrOffsetSize ( ret, ctx, 0, 0 );
            NGS_StringRelease ( tmp, ctx );
        }
        return ret;
    }

    return NULL;
}

#if READ_GROUP_SUPPORTS_READS
struct NGS_Read * NGS_ReadGroupGetReads ( const NGS_ReadGroup * self, ctx_t ctx,
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

struct NGS_Read * NGS_ReadGroupGetRead ( const NGS_ReadGroup * self, ctx_t ctx, const char * readId )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read %ld", readId );
    }
    else
    {
        return VT ( self, get_read ) ( self, ctx, readId );
    }

    return NULL;
}
#endif

struct NGS_Statistics* NGS_ReadGroupGetStatistics ( const NGS_ReadGroup * self, ctx_t ctx )
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


/*--------------------------------------------------------------------------
 * NGS_ReadGroupIterator
 */

bool NGS_ReadGroupIteratorNext ( NGS_ReadGroup* self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get next read group " );
    }
    else
    {
        return VT ( self, get_next) ( self, ctx );
    }

    return false;
}

