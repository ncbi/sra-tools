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

#include "SRA_ReadGroup.h"

#include "SRA_Read.h"
#include "SRA_ReadGroupInfo.h"
#include "SRA_Statistics.h"

#include "NGS_String.h"
#include "NGS_Cursor.h"
#include "NGS_Id.h"

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

#include <strtol.h> /* strtoi64 */

#include <stddef.h>
#include <assert.h>

#include <sysalloc.h>

/*--------------------------------------------------------------------------
 * SRA_Read
 */

struct SRA_ReadGroup
{
    NGS_ReadGroup dad;   
    
    const NGS_String * run_name; 
    const NGS_String * name; /* owns the char buffer */
    
    const NGS_Cursor * curs;
    const SRA_ReadGroupInfo* group_info;
    
    bool seen_first;
    bool iterating;
    
    uint32_t cur_group;
};

static void                     SRA_ReadGroupWhack ( SRA_ReadGroup * self, ctx_t ctx );
static struct NGS_String*       SRA_ReadGroupGetName ( const SRA_ReadGroup * self, ctx_t ctx );
static struct NGS_Read*         SRA_ReadGroupGetReads ( const SRA_ReadGroup * self, ctx_t ctx, bool wants_full, bool wants_partial, bool wants_unaligned );
static struct NGS_Read*         SRA_ReadGroupGetRead ( const SRA_ReadGroup * self, ctx_t ctx, const char* readId );
static struct NGS_Statistics*   SRA_ReadGroupGetStatistics ( const SRA_ReadGroup * self, ctx_t ctx );
static bool                     SRA_ReadGroupIteratorNext ( SRA_ReadGroup * self, ctx_t ctx );

static NGS_ReadGroup_vt NGS_ReadGroup_vt_inst =
{
    {
        /* NGS_RefCount */
        SRA_ReadGroupWhack
    },

    /* NGS_ReadGroup */
    SRA_ReadGroupGetName,
    SRA_ReadGroupGetReads,
    SRA_ReadGroupGetRead,
    SRA_ReadGroupGetStatistics,
    SRA_ReadGroupIteratorNext,
}; 

/* Init
 */
static
void SRA_ReadGroupInit ( ctx_t ctx, 
                         SRA_ReadGroup * self, 
                         const char *clsname, 
                         const char *instname, 
                         const NGS_String* run_name, 
                         const char* group_name, size_t group_name_size,
                         const struct SRA_ReadGroupInfo* group_info )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    if ( self == NULL )
        INTERNAL_ERROR ( xcParamNull, "bad object reference" );
    else
    {
        TRY ( NGS_ReadGroupInit ( ctx, & self -> dad, & NGS_ReadGroup_vt_inst, clsname, instname ) )
        {   
            TRY ( self -> run_name = NGS_StringDuplicate ( run_name, ctx ) )
            {
                TRY ( self -> name = NGS_StringMakeCopy ( ctx, group_name, group_name_size ) )
                {
                    self -> group_info = SRA_ReadGroupInfoDuplicate ( group_info, ctx );
                }
            }
        }
    }
}

/* Whack
 */
static
void SRA_ReadGroupWhack ( SRA_ReadGroup * self, ctx_t ctx )
{
    NGS_StringRelease ( self -> run_name, ctx );
    NGS_StringRelease ( self -> name, ctx );
    NGS_CursorRelease ( self -> curs, ctx );
    SRA_ReadGroupInfoRelease ( self -> group_info, ctx );
}

struct NGS_String* SRA_ReadGroupGetName ( const SRA_ReadGroup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    assert ( self != NULL );
    
    if ( ! self -> seen_first ) 
    {
        USER_ERROR ( xcIteratorUninitialized, "ReadGroup accessed before a call to ReadIteratorNext()" );
        return NULL;
    }
    else if ( self -> cur_group >= self -> group_info -> count )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    
    return NGS_StringDuplicate ( self -> name, ctx );
}

struct NGS_Read* SRA_ReadGroupGetReads ( const SRA_ReadGroup * self, ctx_t ctx, bool wants_full, bool wants_partial, bool wants_unaligned )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    if ( ! self -> seen_first ) 
    {
        USER_ERROR ( xcIteratorUninitialized, "ReadGroup accessed before a call to ReadIteratorNext()" );
        return NULL;
    }    
    else if ( self -> cur_group >= self -> group_info -> count )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    
    {   
        uint64_t start = self -> group_info -> groups [ self -> cur_group ] . min_row;
        uint64_t count = self -> group_info -> groups [ self -> cur_group ] . max_row - start;
        return SRA_ReadIteratorMakeReadGroup ( ctx,         
                                               self -> curs, 
                                               self -> run_name, 
                                               self -> name, 
                                               start, 
                                               count, 
                                               wants_full, 
                                               wants_partial, 
                                               wants_unaligned );
    }
}

struct NGS_Read* SRA_ReadGroupGetRead ( const SRA_ReadGroup * self, ctx_t ctx, const char* readIdStr )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    if ( ! self -> seen_first ) 
    {
        USER_ERROR ( xcIteratorUninitialized, "ReadGroup accessed before a call to ReadIteratorNext()" );
        return NULL;
    }    
    else if ( self -> cur_group >= self -> group_info -> count )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }
    
    {
        TRY ( struct NGS_Id id = NGS_IdParse ( readIdStr, string_size ( readIdStr ), ctx ) )
        {
            if ( string_cmp ( NGS_StringData ( self -> run_name, ctx ), 
                NGS_StringSize ( self -> run_name, ctx ),
                id . run . addr, 
                id . run . size, 
                id . run . len ) != 0 ) 
            {
                INTERNAL_ERROR ( xcArcIncorrect, 
                    " expected '%.*s', actual '%.*s'", 
                    NGS_StringSize ( self -> run_name, ctx ),
                    NGS_StringData ( self -> run_name, ctx ), 
                    id . run . size, 
                    id . run . addr );
            }    
            else
            {   
                /* make sure the requested read is from this read group */
                NGS_Read* ret;
                TRY ( ret = SRA_ReadMake ( ctx, self -> curs, id . rowId, self -> run_name ) )
                {   
                    TRY ( const NGS_String* readGroup = NGS_ReadGetReadGroup ( ret, ctx ) )
                    {
                        if ( string_cmp ( NGS_StringData ( self -> name, ctx ), 
                            NGS_StringSize ( self -> name, ctx ),
                            NGS_StringData ( readGroup, ctx ), 
                            NGS_StringSize ( readGroup, ctx ),
                            NGS_StringSize ( readGroup, ctx ) ) == 0 ) 
                        {
                            NGS_StringRelease ( readGroup, ctx );
                            return ret; 
                        }
                        INTERNAL_ERROR ( xcWrongReadGroup, 
                            "Requested read is from a difference read group (expected '%.*s', actual '%.s')", 
                            NGS_StringSize ( self -> name, ctx ),
                            NGS_StringData ( self -> name, ctx ), 
                            NGS_StringSize ( readGroup, ctx ),                            
                            NGS_StringData ( readGroup, ctx ) );
                        NGS_StringRelease ( readGroup, ctx );
                    }
                    NGS_ReadRelease ( ret, ctx );
                }
            }
        }
    }
    return NULL;
}

static struct NGS_Statistics* SRA_ReadGroupGetStatistics ( const SRA_ReadGroup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    
    if ( ! self -> seen_first ) 
    {
        USER_ERROR ( xcIteratorUninitialized, "ReadGroup accessed before a call to ReadIteratorNext()" );
        return NULL;
    }    
    else if ( self -> cur_group >= self -> group_info -> count )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return NULL;
    }

    {
        const struct SRA_ReadGroupStats * group_stats = & self -> group_info -> groups [ self -> cur_group ];
        TRY ( NGS_Statistics * ret = SRA_StatisticsMake ( ctx ) )
        {
            TRY ( NGS_StatisticsAddU64 ( ret, ctx, "BASE_COUNT", group_stats -> base_count ) )
            {
                TRY ( NGS_StatisticsAddU64 ( ret, ctx, "BIO_BASE_COUNT", group_stats -> bio_base_count ) )
                {
                    TRY ( NGS_StatisticsAddU64 ( ret, ctx, "SPOT_COUNT", group_stats -> row_count ) )
                    {
                        TRY ( NGS_StatisticsAddU64 ( ret, ctx, "SPOT_MAX", group_stats -> max_row) )
                        {
                            TRY ( NGS_StatisticsAddU64 ( ret, ctx, "SPOT_MIN", group_stats -> min_row) )
                            {
                                return ret;
                            }
                        }
                    }
                }
            }
            NGS_StatisticsRelease ( ret, ctx );
        }
    }
    
    return NULL;
}

struct NGS_ReadGroup * SRA_ReadGroupMake ( ctx_t ctx, 
                                           const struct NGS_Cursor * curs, 
                                           const struct SRA_ReadGroupInfo* group_info, 
                                           const struct NGS_String * run_name,
                                           const char * group_name, size_t group_name_size )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );
    SRA_ReadGroup * ref;

    assert ( curs != NULL );
    assert ( run_name != NULL );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating NGS_ReadGroup on '%.*s'", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( SRA_ReadGroupInit ( ctx, ref, "NGS_ReadGroup", instname, run_name, group_name, group_name_size, group_info ) )
        {
            TRY ( ref -> curs = NGS_CursorDuplicate ( curs, ctx ) )
            {
                TRY ( ref -> cur_group = SRA_ReadGroupInfoFind ( ref -> group_info, ctx, group_name, group_name_size ) )
                {
                    ref -> seen_first = true;
                    return & ref -> dad;
                }
            }
            SRA_ReadGroupWhack ( ref, ctx );
        }

        free ( ref );
    }

    return NULL;
}                                            


/*--------------------------------------------------------------------------
 * NGS_ReadGroupIterator
 */
 
/* Make
 */
NGS_ReadGroup * SRA_ReadGroupIteratorMake ( ctx_t ctx, 
                                                  const NGS_Cursor * curs, 
                                                  const struct SRA_ReadGroupInfo* group_info, 
                                                  const struct NGS_String * run_name )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    SRA_ReadGroup * ref;

    assert ( curs != NULL );
    assert ( run_name != NULL );

    ref = calloc ( 1, sizeof * ref );
    if ( ref == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating NGS_ReadGroupIterator on '%.*s'", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
    else
    {
#if _DEBUGGING
        char instname [ 256 ];
        string_printf ( instname, sizeof instname, NULL, "%.*s", NGS_StringSize ( run_name, ctx ), NGS_StringData ( run_name, ctx ) );
        instname [ sizeof instname - 1 ] = 0;
#else
        const char *instname = "";
#endif
        TRY ( SRA_ReadGroupInit ( ctx, ref, "NGS_ReadGroupIterator", instname, run_name, "", 0, group_info ) )
        {
            TRY ( ref -> curs = NGS_CursorDuplicate ( curs, ctx ) )
            {
                ref -> iterating = true;
                return & ref -> dad;
            }
            SRA_ReadGroupWhack ( ref, ctx );
        }

        free ( ref );
    }

    return NULL;
}

/* Next
 */
bool SRA_ReadGroupIteratorNext ( SRA_ReadGroup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    assert ( self != NULL );
    
    if ( ! self -> iterating )
    {
        USER_ERROR ( xcCursorExhausted, "No more rows available" );
        return false;
    }

    if ( self -> seen_first )
    {   /* move to next group */
        ++ self -> cur_group;
    }
    else
    {
        self -> seen_first = true;
    }

    while ( self -> cur_group < self -> group_info -> count )
    {
        if ( self -> group_info -> groups [ self -> cur_group ] . min_row == 0 )
        {
           ++ self -> cur_group;
        }
        else
        {
            NGS_StringRelease ( self -> name, ctx );
            self -> name = NULL;
            TRY ( self -> name = NGS_StringDuplicate ( self -> group_info -> groups [ self -> cur_group ] . name, ctx ) )
            {
                return true;
            }
            /* error - make the iterator unusable */
            self -> cur_group = self -> group_info -> count;
            return false;
        }
    }
    
    return false;
}
