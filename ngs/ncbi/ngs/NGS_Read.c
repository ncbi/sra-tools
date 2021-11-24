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


#include "NGS_Read.h"

#include "NGS_String.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/FragmentItf.h>
#include <ngs/itf/ReadItf.h>

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/refcount.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/vdb-priv.h>
#include <insdc/insdc.h>

#include <sysalloc.h>

#include <stddef.h>
#include <assert.h>

/*--------------------------------------------------------------------------
 * NGS_Read_v1
 */

#define Self( obj ) \
    ( ( NGS_Read* ) ( obj ) )
    
static NGS_String_v1 * ITF_Read_v1_get_id ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReadGetReadId ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static uint32_t ITF_Read_v1_get_num_frags ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint32_t ret = NGS_ReadNumFragments ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static bool ITF_Read_v1_frag_is_aligned ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err, uint32_t frag_idx )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_ReadFragIsAligned ( Self ( self ), ctx, frag_idx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static uint32_t ITF_Read_v1_get_category ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint32_t ret = NGS_ReadGetReadCategory ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static NGS_String_v1 * ITF_Read_v1_get_read_group ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReadGetReadGroup ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Read_v1_get_name ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReadGetReadName ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Read_v1_get_bases ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReadGetReadSequence ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static NGS_String_v1 * ITF_Read_v1_get_quals ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( struct NGS_String * ret = NGS_ReadGetReadQualities ( Self ( self ), ctx, offset, length ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static bool ITF_Read_v1_next ( NGS_Read_v1 * self, NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( bool ret = NGS_ReadIteratorNext ( Self ( self ), ctx ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

#undef Self


NGS_Read_v1_vt ITF_Read_vt =
{
    {
        "NGS_Read",
        "NGS_Read_v1",
        1,
        & ITF_Fragment_vt . dad
    },

    ITF_Read_v1_get_id,
    ITF_Read_v1_get_num_frags,
    ITF_Read_v1_get_category,
    ITF_Read_v1_get_read_group,
    ITF_Read_v1_get_name,
    ITF_Read_v1_get_bases,
    ITF_Read_v1_get_quals,
    ITF_Read_v1_next,

    /* 1.1 */
    ITF_Read_v1_frag_is_aligned
};

/*--------------------------------------------------------------------------
 * NGS_Read
 */
#define VT( self, msg ) \
    ( ( ( const NGS_Read_vt* ) ( self ) -> dad . dad . vt ) -> msg )

NGS_String * NGS_ReadGetReadName ( NGS_Read * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read name" );
    }
    else
    {
        return VT ( self, get_name ) ( self, ctx );
    }

    return NULL;
}

NGS_String * NGS_ReadGetReadId ( NGS_Read * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read id" );
    }
    else
    {
        return VT ( self, get_id ) ( self, ctx );
    }

    return 0;
}

NGS_String * NGS_ReadGetReadGroup ( NGS_Read * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read group" );
    }
    else
    {
        return VT ( self, get_read_group ) ( self, ctx );
    }

    return NULL;
}

enum NGS_ReadCategory NGS_ReadGetReadCategory ( const NGS_Read * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read category" );
    }
    else
    {
        return VT ( self, get_category ) ( self, ctx );
    }

    return NGS_ReadCategory_unaligned;
}

NGS_String * NGS_ReadGetReadSequence ( NGS_Read * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read sequence" );
    }
    else
    {
        return VT ( self, get_sequence ) ( self, ctx, offset, size );
    }

    return NULL;
}

NGS_String * NGS_ReadGetReadQualities( NGS_Read * self, ctx_t ctx, uint64_t offset, uint64_t size )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read qualities" );
    }
    else
    {
        return VT ( self, get_qualities ) ( self, ctx, offset, size );
    }

    return NULL;
}

uint32_t NGS_ReadNumFragments ( NGS_Read * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read fragment count" );
    }
    else
    {
        return VT ( self, get_num_fragments ) ( self, ctx );
    }

    return 0;
}

bool NGS_ReadFragIsAligned ( NGS_Read * self, ctx_t ctx, uint32_t frag_idx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to test fragment alignment" );
    }
    else
    {
        return VT ( self, frag_is_aligned ) ( self, ctx, frag_idx );
    }

    return 0;
}

/*--------------------------------------------------------------------------
 * NGS_ReadIterator
 */

bool NGS_ReadIteratorNext ( NGS_Read * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to advance read iterator" );
    }
    else
    {
        return VT ( self, next ) ( self, ctx );
    }

    return false;
}

uint64_t NGS_ReadIteratorGetCount ( const NGS_Read * self, ctx_t ctx )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get read iterator count" );
    }
    else
    {
        return VT ( self, get_count ) ( self, ctx );
    }

    return 0;
}

void NGS_ReadInit ( ctx_t ctx, NGS_Read * read, const NGS_Read_vt * vt, const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );

    TRY ( NGS_FragmentInit ( ctx, & read -> dad, & ITF_Read_vt . dad, & vt -> dad, clsname, instname ) )
    {
        assert ( vt -> get_id != NULL );
        assert ( vt -> get_name != NULL );
        assert ( vt -> get_read_group != NULL );
        assert ( vt -> get_category != NULL );
        assert ( vt -> get_sequence != NULL );
        assert ( vt -> get_qualities != NULL ); 
        assert ( vt -> get_num_fragments != NULL );
   }
}

void NGS_ReadIteratorInit ( ctx_t ctx, NGS_Read * read, const NGS_Read_vt * vt, const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );

    TRY ( NGS_ReadInit ( ctx, read, vt, clsname, instname ) )
    {
        assert ( vt -> next != NULL ); 
        assert ( vt -> get_count != NULL );
   }
}

/*--------------------------------------------------------------------------
 * NGS_NullRead
 */

static 
void NullRead_ReadWhack ( NGS_Read * self, ctx_t ctx )
{
}

static 
struct NGS_String * NullRead_FragmentToString ( NGS_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static 
struct NGS_String * NullRead_FragmentOffsetLenToString ( NGS_Read * self, ctx_t ctx, uint64_t offset, uint64_t length )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static 
bool NullRead_FragmentToBool ( NGS_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static 
struct NGS_String * NullRead_ReadToString ( NGS_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static 
enum NGS_ReadCategory NullRead_ConstReadToCategory ( const NGS_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static struct NGS_String * NullRead_ReadOffsetLenToString ( NGS_Read * self, ctx_t ctx, uint64_t offset, uint64_t length )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static uint32_t NullRead_ReadToU32 ( NGS_Read * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return 0;
}

static bool NullRead_FragIsAligned ( NGS_Read * self, ctx_t ctx, uint32_t frag_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing);
    INTERNAL_ERROR ( xcSelfNull, "NULL Alignment accessed" );
    return false;
}

static bool NullRead_ReadToBool_NoError ( NGS_Read * self, ctx_t ctx )
{
    return false;
}

static uint64_t NullRead_ConstReadToU64_NoError ( const NGS_Read * self, ctx_t ctx )
{
    return 0;
}

static NGS_Read_vt NullRead_vt_inst =
{
    {
        {
            /* NGS_Refcount */
            NullRead_ReadWhack
        },

        /* NGS_Fragment */
        NullRead_FragmentToString,
        NullRead_FragmentOffsetLenToString,
        NullRead_FragmentOffsetLenToString,
        NullRead_FragmentToBool,
        NullRead_FragmentToBool
    },
    
    /* NGS_Read */
    NullRead_ReadToString,                     /* get-id          */
    NullRead_ReadToString,                     /* get-name        */
    NullRead_ReadToString,                     /* get-read-group  */
    NullRead_ConstReadToCategory,              /* get-category    */
    NullRead_ReadOffsetLenToString,            /* get-sequence    */
    NullRead_ReadOffsetLenToString,            /* get-qualities   */
    NullRead_ReadToU32,                        /* get-num-frags   */
    NullRead_FragIsAligned,                    /* frag-is-aligned */
    
    /* NGS_ReadIterator */
    NullRead_ReadToBool_NoError,               /* next            */
    NullRead_ConstReadToU64_NoError,           /* get-count       */
}; 

struct NGS_Read * NGS_ReadMakeNull ( ctx_t ctx, const NGS_String * run_name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    NGS_Read * ref;

    assert ( run_name != NULL );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating NullRead on '%.*s'", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s(NULL)", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
		TRY ( NGS_ReadInit ( ctx, ref, & NullRead_vt_inst, "NullRead", instname ) )
        {
            return ref;
        }
        free ( ref );
    }

    return NULL;
}



