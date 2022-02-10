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

#include "SRA_Statistics.h"

#include "NGS_String.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/refcount.h>
#include <klib/container.h>

#include <kdb/meta.h>

#include <vdb/table.h>
#include <vdb/database.h>

#include <sysalloc.h>

#include <stddef.h>
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <errno.h>

#include <strtol.h>
#include <compiler.h>

/* sadly, MSVS does not define trunc */
static double xtrunc (double x)
{ 
    return x < 0 ? ceil ( x ) : floor ( x );
}

/*--------------------------------------------------------------------------
 * SRA_Read
 */

struct SRA_Statistics
{
    NGS_Statistics dad;   
    
    BSTree dictionary;
};

struct DictionaryEntry
{
    BSTNode dad;
    
    uint32_t type;
    union 
    {
        int64_t     i64;
        uint64_t    u64;
        double      real;
        NGS_String* str;
    } value;
    char path[1]; /* 0-terminated */
};
typedef struct DictionaryEntry DictionaryEntry;

static void DictionaryEntryWhack ( BSTNode *n, void *data )
{
    ctx_t ctx = ( ctx_t ) data;
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcDestroying );
    
    DictionaryEntry* entry = ( DictionaryEntry * ) n;
    
    if ( entry -> type == NGS_StatisticValueType_String )
    {
        NGS_StringRelease ( entry -> value . str, ctx );
    }
    
    free ( n );
}

static int64_t DictionaryEntryCompare ( const BSTNode *p_a, const BSTNode *p_b )
{
    DictionaryEntry* a = ( DictionaryEntry * ) p_a;
    DictionaryEntry* b = ( DictionaryEntry * ) p_b;
    
    size_t a_path_size = string_size ( a -> path );
    return string_cmp ( a -> path, a_path_size, b -> path, string_size ( b -> path ), ( uint32_t ) a_path_size );
}

static int64_t DictionaryEntryFind ( const void *p_a, const BSTNode *p_b )
{
    const char * a = ( const char * ) p_a;
    DictionaryEntry* b = ( DictionaryEntry * ) p_b;
    
    size_t a_path_size = string_size ( a );
    return string_cmp ( a, a_path_size, b -> path, string_size ( b -> path ), ( uint32_t ) a_path_size );
}

static void         SRA_StatisticsWhack         ( SRA_Statistics * self, ctx_t ctx );
static uint32_t     SRA_StatisticsGetValueType  ( const SRA_Statistics * self, ctx_t ctx, const char * path );
static NGS_String * SRA_StatisticsGetAsString   ( const SRA_Statistics * self, ctx_t ctx, const char * path );
static int64_t      SRA_StatisticsGetAsI64      ( const SRA_Statistics * self, ctx_t ctx, const char * path );
static uint64_t     SRA_StatisticsGetAsU64      ( const SRA_Statistics * self, ctx_t ctx, const char * path );
static double       SRA_StatisticsGetAsDouble   ( const SRA_Statistics * self, ctx_t ctx, const char * path );
static bool         SRA_StatisticsNextPath      ( const SRA_Statistics * self, ctx_t ctx, const char * path, const char ** next );
static void         SRA_StatisticsAddString     ( SRA_Statistics * self, ctx_t ctx, const char * path, const NGS_String * value );
static void         SRA_StatisticsAddI64        ( SRA_Statistics * self, ctx_t ctx, const char * path, int64_t value );
static void         SRA_StatisticsAddU64        ( SRA_Statistics * self, ctx_t ctx, const char * path, uint64_t value );
static void         SRA_StatisticsAddDouble     ( SRA_Statistics * self, ctx_t ctx, const char * path, double value );

static NGS_Statistics_vt SRA_Statistics_vt_inst =
{
    {
        /* NGS_RefCount */
        SRA_StatisticsWhack
    },

    /* NGS_Statistics */
    SRA_StatisticsGetValueType,
    SRA_StatisticsGetAsString,
    SRA_StatisticsGetAsI64,               
    SRA_StatisticsGetAsU64,               
    SRA_StatisticsGetAsDouble,
    SRA_StatisticsNextPath,
    SRA_StatisticsAddString,
    SRA_StatisticsAddI64,
    SRA_StatisticsAddU64,
    SRA_StatisticsAddDouble,
}; 

void SRA_StatisticsWhack ( SRA_Statistics * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcDestroying );
    
    assert ( self );
    
    BSTreeWhack ( & self -> dictionary, DictionaryEntryWhack, ( void * ) ctx );
}

uint32_t SRA_StatisticsGetValueType ( const SRA_Statistics * self, ctx_t ctx, const char * path )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
	
    assert ( self );

    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
	{
		DictionaryEntry * node = ( DictionaryEntry * ) 
			BSTreeFind ( & self -> dictionary, ( const void * ) path, DictionaryEntryFind );
        if ( node == NULL )
        {
            INTERNAL_ERROR ( xcUnexpected, "dictionary item '%s' is not found", path );
        }
        else
        {
			return node -> type;
		}
	}

    return NGS_StatisticValueType_Undefined;
}

NGS_String* SRA_StatisticsGetAsString ( const SRA_Statistics * self, ctx_t ctx, const char * path )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
    {
        DictionaryEntry * node = ( DictionaryEntry * ) 
            BSTreeFind ( & self -> dictionary, ( const void * ) path, DictionaryEntryFind );
        if ( node == NULL )
        {
            INTERNAL_ERROR ( xcUnexpected, "dictionary item '%s' is not found", path );
        }
        else
        {
            switch ( node -> type )
            {
            case NGS_StatisticValueType_UInt64: 
                {
                    char buf[1024];
                    size_t num_writ;
                    string_printf ( buf, sizeof(buf), &num_writ, "%lu", node -> value . u64 );
                    return NGS_StringMakeCopy ( ctx, buf, num_writ );
                }
                break;
                
            case NGS_StatisticValueType_Int64:  
                {
                    char buf[1024];
                    size_t num_writ;
                    string_printf ( buf, sizeof(buf), &num_writ, "%li", node -> value . i64 );
                    return NGS_StringMakeCopy ( ctx, buf, num_writ );
                }
                
            case NGS_StatisticValueType_Real:   
                {
                    char buf[1024];
                    size_t num_writ;
                    string_printf ( buf, sizeof(buf), &num_writ, "%f", node -> value . real );
                    return NGS_StringMakeCopy ( ctx, buf, num_writ );
                }
                
            case NGS_StatisticValueType_String: 
                return NGS_StringDuplicate ( node -> value . str, ctx );
                
            default :
                INTERNAL_ERROR ( xcUnexpected, "unexpected type %u for dictionary item '%s'", node -> type, path );
                break;
            }
        }
    }
    
    return NULL;
}

static int64_t NGS_StringToI64( const NGS_String * str, ctx_t ctx )
{
    /* have to guarantee NUL-termination for strtoi64/strtod */
    char buf[4096];
    if ( sizeof(buf) > NGS_StringSize ( str, ctx ) )
    {
        char* end;
        int64_t value;
        string_copy ( buf, sizeof(buf), 
                      NGS_StringData ( str, ctx ), NGS_StringSize ( str, ctx ) );
        
        errno = 0;
        value = strtoi64 ( buf, &end, 10 );
        if ( *end == 0 )
        {
            if ( errno == 0 )   
            {   
                return value;
            }
        }
        else
        {   /* attempt to parse as a double */
            double dbl;
            errno = 0;
            dbl = strtod ( buf, &end );
            if ( *end == 0 && errno == 0 && dbl >= LLONG_MIN && dbl <= LLONG_MAX )
            {
                return ( int64_t ) xtrunc ( dbl );
            }
        }
    }
    INTERNAL_ERROR ( xcUnexpected, "cannot convert dictionary value '%.*s' from string to int64", 
                                    NGS_StringSize ( str, ctx ), NGS_StringData ( str, ctx ) );
    return 0;
}

int64_t SRA_StatisticsGetAsI64 ( const SRA_Statistics * self, ctx_t ctx, const char * path )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );

    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
    {
        DictionaryEntry * node = ( DictionaryEntry * ) 
            BSTreeFind ( & self -> dictionary, ( const void * ) path, DictionaryEntryFind );
        if ( node == NULL )
        {
            INTERNAL_ERROR ( xcUnexpected, "dictionary item '%s' is not found", path );
        }
        else
        {
            switch ( node -> type )
            {
            case NGS_StatisticValueType_Int64:  
                return node -> value . i64;
                
            case NGS_StatisticValueType_UInt64: 
                if ( node -> value . u64 > LLONG_MAX )
                {
                    INTERNAL_ERROR ( xcUnexpected, "cannot convert dictionary item '%s' from uin64_t to int64_t", path );
                }
                else
                {
                    return ( int64_t ) node -> value . u64;
                }
                break;
                
            case NGS_StatisticValueType_Real:   
                if ( node -> value . real < LLONG_MIN || node -> value . real > LLONG_MAX )
                {
                    INTERNAL_ERROR ( xcUnexpected, "cannot convert dictionary item '%s' from double to int64_t", path );
                }
                else
                {
                    return ( int64_t ) xtrunc ( node -> value . real );
                }
                break;
                
            case NGS_StatisticValueType_String: 
                return NGS_StringToI64 ( node -> value . str, ctx );
                
            default :
                INTERNAL_ERROR ( xcUnexpected, "unexpected type %u for dictionary item '%s'", node -> type, path );
                break;
            }
        }
    }
    
    return 0;
}

static uint64_t NGS_StringToU64( const NGS_String * str, ctx_t ctx )
{
    /* have to guarantee NUL-termination for strtou64/strtod */
    char buf[4096];
    if ( sizeof(buf) > NGS_StringSize ( str, ctx ) )
    {
        char* end;
        uint64_t value;
        string_copy ( buf, sizeof(buf), 
                      NGS_StringData ( str, ctx ), NGS_StringSize ( str, ctx ) );
                      
        errno = 0;
        value = strtou64 ( buf, &end, 10 );
        if ( *end == 0 ) 
        {
            if ( errno == 0 )   
            {   
                return value;
            }
        }
        else
        {   /* attempt to parse as a double */
            double dbl;
            errno = 0;
            dbl = strtod ( buf, &end );
            if ( *end == 0 && errno == 0 && dbl >= 0 && dbl <= ULLONG_MAX )
            {
                return ( uint64_t ) xtrunc ( dbl );
            }
        }
    }
    INTERNAL_ERROR ( xcUnexpected, "cannot convert dictionary value '%.*s' from string to uint64", 
                                    NGS_StringSize ( str, ctx ), NGS_StringData ( str, ctx ) );
    return 0;
}

uint64_t SRA_StatisticsGetAsU64 ( const SRA_Statistics * self, ctx_t ctx, const char * path )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
    {
        DictionaryEntry * node = ( DictionaryEntry * ) 
            BSTreeFind ( & self -> dictionary, ( const void * ) path, DictionaryEntryFind );
        if ( node == NULL )
        {
            INTERNAL_ERROR ( xcUnexpected, "dictionary item '%s' is not found", path );
        }
        else
        {
            switch ( node -> type )
            {
            case NGS_StatisticValueType_Int64:  
                if ( node -> value . i64 < 0 )
                {
                    INTERNAL_ERROR ( xcUnexpected, "cannot convert dictionary item '%s' from in64_t to uint64_t", path );
                }
                else
                {
                    return ( uint64_t ) node -> value . i64;
                }
                break;
                
            case NGS_StatisticValueType_UInt64: 
                return node -> value . i64; 
            
            case NGS_StatisticValueType_Real:   
                if ( node -> value . real < 0 || node -> value . real > ULLONG_MAX )
                {
                    INTERNAL_ERROR ( xcUnexpected, "cannot convert dictionary item '%s' from double to uint64_t", path );
                }
                else
                {
                    return ( uint64_t ) xtrunc ( node -> value . real );
                }
                break;
            
            case NGS_StatisticValueType_String: 
                return NGS_StringToU64 ( node -> value . str, ctx );
                
            default :
                INTERNAL_ERROR ( xcUnexpected, "unexpected type %u for dictionary item '%s'", node -> type, path );
                break;
            }
        }
    }
    
    return 0;
}

static double NGS_StringToReal ( const NGS_String * str, ctx_t ctx )
{
    /* have to guarantee NUL-termination for strtod */
    char buf[4096];
    if ( sizeof(buf) > NGS_StringSize ( str, ctx ) )
    {
        char* end;
        double real;
        string_copy ( buf, sizeof(buf), 
                      NGS_StringData ( str, ctx ), NGS_StringSize ( str, ctx ) );

        errno = 0;
        real = strtod ( buf, &end );
        if ( *end == 0 && errno == 0 )   
        {
            return real;
        }
    }
    INTERNAL_ERROR ( xcUnexpected, "cannot convert dictionary value '%.*s' from string to numeric", 
                                    NGS_StringSize ( str, ctx ), NGS_StringData ( str, ctx ) );
    return 0.0;
}

double SRA_StatisticsGetAsDouble ( const SRA_Statistics * self, ctx_t ctx, const char * path )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
    {
        DictionaryEntry * node = ( DictionaryEntry * ) 
            BSTreeFind ( & self -> dictionary, ( const void * ) path, DictionaryEntryFind );
        if ( node == NULL )
        {
            INTERNAL_ERROR ( xcUnexpected, "dictionary item '%s' is not found", path );
        }
        else
        {
            switch ( node -> type )
            {
            case NGS_StatisticValueType_Int64:  
                return ( double ) node -> value . i64;
                
            case NGS_StatisticValueType_UInt64: 
                return ( double ) node -> value . u64;
                
            case NGS_StatisticValueType_Real:   
                return node -> value . real;
                
            case NGS_StatisticValueType_String: 
                return NGS_StringToReal ( node -> value . str, ctx );
                break;
                
            default :
                INTERNAL_ERROR ( xcUnexpected, "unexpected type %u for dictionary item '%s'", node -> type, path );
                break;
            }
        }
    }
    
    return 0;
}

bool SRA_StatisticsNextPath ( const SRA_Statistics * self, ctx_t ctx, const char * path, const char** next )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    const DictionaryEntry * node = NULL;
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else if ( path[0] == 0 )
    {
        node = ( const DictionaryEntry * ) BSTreeFirst ( & self -> dictionary );
    }
    else
    {
        node = ( const DictionaryEntry * ) BSTreeFind ( & self -> dictionary, ( const void * ) path, DictionaryEntryFind );
        if ( node == NULL )
        {
            INTERNAL_ERROR ( xcUnexpected, "dictionary item '%s' is not found", path );
        }
        else
        {
            node = ( const DictionaryEntry * ) BSTNodeNext ( & node -> dad );
        }
    }
    
    if ( node == NULL )
    {
        *next = NULL;
        return false;
    }
    *next = node -> path;
    return true;
}

static
DictionaryEntry * MakeNode ( SRA_Statistics * self, ctx_t ctx, const char * path )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    size_t path_size = string_size ( path );
    DictionaryEntry * node = malloc ( sizeof ( * node ) + path_size );
    if ( node == NULL )
    {
        SYSTEM_ERROR ( xcNoMemory, "allocating dictionary item" );
    }
    else
    {
        rc_t rc;
        string_copy ( node -> path, path_size + 1, path, path_size );
        
        /*TODO: decide whether to allow overwriting (not allowed now) */
        rc = BSTreeInsertUnique ( & self -> dictionary, & node -> dad, NULL, DictionaryEntryCompare );
        if ( rc == 0 )
        {
            return node;
        }
        
        INTERNAL_ERROR ( xcUnexpected, "inserting dictionary item '%s' rc = %R", node -> path, rc );
        free ( node );
    }
    return NULL;
}

void SRA_StatisticsAddString ( SRA_Statistics * self, ctx_t ctx, const char * path, const NGS_String * value )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
    {
        TRY ( DictionaryEntry * node = MakeNode ( self, ctx, path ) )
        {
            node -> type = NGS_StatisticValueType_String;
            node -> value . str = NGS_StringDuplicate ( value, ctx );
        }
    }
}

void SRA_StatisticsAddI64 ( SRA_Statistics * self, ctx_t ctx, const char * path, int64_t value )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
    {
        TRY ( DictionaryEntry * node = MakeNode ( self, ctx, path ) )
        {
            node -> type = NGS_StatisticValueType_Int64;
            node -> value . i64 = value;
        }
    }
}

void SRA_StatisticsAddU64 ( SRA_Statistics * self, ctx_t ctx, const char * path, uint64_t value )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else
    {
        TRY ( DictionaryEntry * node = MakeNode ( self, ctx, path ) )
        {
            node -> type = NGS_StatisticValueType_UInt64;
            node -> value . u64 = value;
        }
    }
}

void SRA_StatisticsAddDouble ( SRA_Statistics * self, ctx_t ctx, const char * path, double value )
{
    FUNC_ENTRY ( ctx, rcSRA, rcDatabase, rcAccessing );
    
    assert ( self );
    
    if ( path == NULL )
        INTERNAL_ERROR ( xcParamNull, "path is NULL" );
    else if ( isnan ( value ) )
    {
        INTERNAL_ERROR ( xcUnexpected, "NAN is not supported" );
    }
    else
    {
        TRY ( DictionaryEntry * node = MakeNode ( self, ctx, path ) )
        {
            node -> type = NGS_StatisticValueType_Real;
            node -> value . real = value;
        }
    }
}

NGS_Statistics * SRA_StatisticsMake ( ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    SRA_Statistics * ref;

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating SRA_Statistics" );
    else
    {
        TRY ( NGS_StatisticsInit ( ctx, & ref -> dad, & SRA_Statistics_vt_inst, "SRA_Statistics", "" ) )
        {   
            BSTreeInit ( & ref -> dictionary );
            return & ref -> dad;
        }
        
        free ( ref );
    }

    return NULL;
}

static
uint64_t KMetadata_ReadU64 ( const struct KMetadata * meta, ctx_t ctx, const char* name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    uint64_t ret = 0;
    
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, & node, "%s", name );
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
NGS_String * KMetadata_ReadString ( const struct KMetadata * meta, ctx_t ctx, const char* name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    const KMDataNode * node;
    rc_t rc = KMetadataOpenNodeRead ( meta, & node, "%s", name );
    if ( rc == 0 )
    {
        char dummy;
        size_t num_read;
        size_t remaining;
        char * buf;
        KMDataNodeRead ( node, 0, & dummy, 0, & num_read, & remaining );
        
        buf = malloc ( remaining );
        if ( buf == NULL )
        {
            INTERNAL_ERROR ( xcUnexpected, "malloc (%u) failed", remaining );
        }
        else
        {
            rc = KMDataNodeRead ( node, 0, buf, remaining, & num_read, NULL );
            if ( rc != 0 )
            {
                INTERNAL_ERROR ( xcUnexpected, "KMDataNodeRead(%s) rc = %R", name, rc );
            }
            else
            {
                NGS_String * ret = NGS_StringMakeOwned ( ctx, buf, remaining );
                KMDataNodeRelease ( node );
                return ret;
            }
            free ( buf );
        }
        KMDataNodeRelease ( node );
    }
    
    return NULL;
}            

void AddWithPrefix( NGS_Statistics * self, ctx_t ctx, const char* prefix, const char* path, uint64_t value )
{
    char full_path[1024];
    string_printf( full_path, sizeof (full_path ), NULL, "%s/%s", prefix, path );
    NGS_StatisticsAddU64 ( self, ctx, full_path, value );
}

void SRA_StatisticsLoadTableStats ( NGS_Statistics * self, ctx_t ctx, const struct VTable* tbl, const char* prefix )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    const struct KMetadata * meta;
    
    rc_t rc = VTableOpenMetadataRead ( tbl, & meta );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "VTableOpenMetadataRead rc = %R", rc );
    }
    else
    {
        ON_FAIL ( AddWithPrefix ( self, ctx, prefix, "BASE_COUNT",
                                  KMetadata_ReadU64 ( meta, ctx, "STATS/TABLE/BASE_COUNT" ) ) ) CLEAR ();
        ON_FAIL ( AddWithPrefix ( self, ctx, prefix, "BIO_BASE_COUNT",   
                                  KMetadata_ReadU64 ( meta, ctx, "STATS/TABLE/BIO_BASE_COUNT" ) ) ) CLEAR ();
        ON_FAIL ( AddWithPrefix ( self, ctx, prefix, "CMP_BASE_COUNT",   
                                  KMetadata_ReadU64 ( meta, ctx, "STATS/TABLE/CMP_BASE_COUNT" ) ) ) CLEAR ();
        ON_FAIL ( AddWithPrefix ( self, ctx, prefix, "SPOT_COUNT",       
                                  KMetadata_ReadU64 ( meta, ctx, "STATS/TABLE/SPOT_COUNT" ) ) ) CLEAR ();
        ON_FAIL ( AddWithPrefix ( self, ctx, prefix, "SPOT_MAX",         
                                  KMetadata_ReadU64 ( meta, ctx, "STATS/TABLE/SPOT_MAX" ) ) ) CLEAR ();
        ON_FAIL ( AddWithPrefix ( self, ctx, prefix, "SPOT_MIN",         
                                  KMetadata_ReadU64 ( meta, ctx, "STATS/TABLE/SPOT_MIN" ) ) ) CLEAR ();
        
        KMetadataRelease ( meta );
    }
}

void SRA_StatisticsLoadBamHeader ( NGS_Statistics * self, ctx_t ctx, const struct VDatabase * db )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    const struct KMetadata * meta;
    
    rc_t rc = VDatabaseOpenMetadataRead ( db, & meta );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "VDatabaseOpenMetadataRead rc = %R", rc );
    }
    else
    {
        TRY ( NGS_String * str = KMetadata_ReadString ( meta, ctx, "BAM_HEADER" ) )
        {
            if ( str != NULL )
            {
                NGS_StatisticsAddString ( self, ctx, "BAM_HEADER", str );
                NGS_StringRelease ( str, ctx );
            }
        }
        KMetadataRelease ( meta );
    }
}

