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

#include "NGS_Statistics.h"

#include "NGS_ErrBlock.h"
#include <ngs/itf/Refcount.h>
#include <ngs/itf/StatisticsItf.h>

#include "NGS_String.h"

#include <klib/text.h>

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <sysalloc.h>

/*--------------------------------------------------------------------------
 * NGS_Statistics_v1
 */

#define Self( obj ) \
    ( ( NGS_Statistics* ) ( obj ) )
    
static
uint32_t NGS_Statistics_v1_get_type ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint32_t ret = NGS_StatisticsGetValueType ( Self ( self ), ctx, path ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static
NGS_String_v1 * NGS_Statistics_v1_as_string ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( NGS_String * ret = NGS_StatisticsGetAsString ( Self ( self ), ctx, path ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

static
int64_t NGS_Statistics_v1_as_I64 ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( int64_t ret = NGS_StatisticsGetAsI64( Self ( self ), ctx, path ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static
uint64_t NGS_Statistics_v1_as_U64 ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( uint64_t ret = NGS_StatisticsGetAsU64( Self ( self ), ctx, path ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static
double NGS_Statistics_v1_as_F64 ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    ON_FAIL ( double ret = NGS_StatisticsGetAsDouble( Self ( self ), ctx, path ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ret;
}

static
NGS_String_v1 * NGS_Statistics_v1_next_path ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcRefcount, rcAccessing );
    NGS_String * ret;
    const char* new_path;
    ON_FAIL ( bool more = NGS_StatisticsNextPath( Self ( self ), ctx, path, & new_path ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
        return NULL;
    }
    
    if ( more )
    {
        ON_FAIL ( ret = NGS_StringMakeCopy ( ctx, new_path, string_size ( new_path ) ) )
        {
            NGS_ErrBlockThrow ( err, ctx );
        }
    }
    else
    {
        ON_FAIL ( ret = NGS_StringMake ( ctx, "", 0 ) )
        {
            NGS_ErrBlockThrow ( err, ctx );
        }
    }

    CLEAR ();
    return ( NGS_String_v1 * ) ret;
}

#undef Self


NGS_Statistics_v1_vt ITF_Statistics_vt =
{
    {
        "NGS_Statistics",
        "NGS_Statistics_v1",
        0,
        & ITF_Refcount_vt . dad
    },

    NGS_Statistics_v1_get_type,
    NGS_Statistics_v1_as_string,
    NGS_Statistics_v1_as_I64,
    NGS_Statistics_v1_as_U64,
    NGS_Statistics_v1_as_F64,
    NGS_Statistics_v1_next_path
};

/*--------------------------------------------------------------------------
 * NGS_Statistics
 */

#define VT( self, msg ) \
    ( ( ( const NGS_Statistics_vt* ) ( self ) -> dad . vt ) -> msg )
    
void NGS_StatisticsInit ( ctx_t ctx, struct NGS_Statistics * self, NGS_Statistics_vt * vt, const char *clsname, const char *instname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcRow, rcConstructing );
    
    TRY ( NGS_RefcountInit ( ctx, & self -> dad, & ITF_Statistics_vt . dad, & vt -> dad, clsname, instname ) )
    {
        assert ( VT ( self, get_value_type ) != NULL );
        assert ( VT ( self, get_as_string ) != NULL );
        assert ( VT ( self, get_as_int64 ) != NULL );
        assert ( VT ( self, get_as_uint64 ) != NULL );
        assert ( VT ( self, get_as_double ) != NULL );
        assert ( VT ( self, next_path ) != NULL );
        assert ( VT ( self, add_string ) != NULL );
        assert ( VT ( self, add_int64  ) != NULL );
        assert ( VT ( self, add_uint64 ) != NULL );
        assert ( VT ( self, add_double ) != NULL );
    }
}
   

uint32_t NGS_StatisticsGetValueType ( const NGS_Statistics * self, ctx_t ctx, const char * path )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get value type" );
    }
    else
    {
        return VT ( self, get_value_type ) ( self, ctx, path );
    }

    return NGS_StatisticValueType_Undefined;
}
    
struct NGS_String* NGS_StatisticsGetAsString ( const NGS_Statistics * self, ctx_t ctx, const char * path )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get value type" );
    }
    else
    {
        return VT ( self, get_as_string ) ( self, ctx, path );
    }

    return NULL;
}

int64_t NGS_StatisticsGetAsI64 ( const NGS_Statistics * self, ctx_t ctx, const char * path )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get value as I64" );
    }
    else
    {
        return VT ( self, get_as_int64 ) ( self, ctx, path );
    }

    return 0;
}

uint64_t NGS_StatisticsGetAsU64 ( const NGS_Statistics * self, ctx_t ctx, const char * path )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get value as U64" );
    }
    else
    {
        return VT ( self, get_as_uint64) ( self, ctx, path );
    }

    return 0;
}

double NGS_StatisticsGetAsDouble ( const NGS_Statistics * self, ctx_t ctx, const char * path )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get value as Double" );
    }
    else
    {
        return VT ( self, get_as_double) ( self, ctx, path );
    }

    return 0.0;
}

bool NGS_StatisticsNextPath ( const NGS_Statistics * self, ctx_t ctx, const char * path, const char ** next )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to get next path" );
    }
    else
    {
        return VT ( self, next_path ) ( self, ctx, path, next );
    }

    return false;
}

void NGS_StatisticsAddString ( NGS_Statistics * self, ctx_t ctx, const char * path, const NGS_String * value )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to add '%s'", path );
    }
    else
    {
        VT ( self, add_string ) ( self, ctx, path, value );
    }
}

void NGS_StatisticsAddI64 ( NGS_Statistics * self, ctx_t ctx, const char * path, int64_t value )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to add '%s'", path );
    }
    else
    {
        VT ( self, add_int64 ) ( self, ctx, path, value );
    }
}

void NGS_StatisticsAddU64 ( NGS_Statistics * self, ctx_t ctx, const char * path, uint64_t value )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to add '%s'", path );
    }
    else
    {
        VT ( self, add_uint64 ) ( self, ctx, path, value );
    }
}

void NGS_StatisticsAddDouble ( NGS_Statistics * self, ctx_t ctx, const char * path, double value )
{
    if ( self == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
        INTERNAL_ERROR ( xcSelfNull, "failed to add '%s'", path );
    }
    else
    {
        VT ( self, add_double ) ( self, ctx, path, value );
    }
}


