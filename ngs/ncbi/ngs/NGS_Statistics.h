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

#ifndef _h_ngs_statistics_
#define _h_ngs_statistics_

typedef struct NGS_Statistics NGS_Statistics;
#ifndef NGS_STATISTICS
#define NGS_STATISTICS NGS_Statistics
#endif

#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_STATISTICS
#include "NGS_Refcount.h"
#endif

#define READ_GROUP_SUPPORTS_READS 0

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */

struct NGS_String;
struct NGS_ReadGroup_v1_vt;
extern struct NGS_ReadGroup_v1_vt ITF_ReadGroup_vt;

/*--------------------------------------------------------------------------
 * NGS_Statistics
 */
 
/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_StatisticsToRefcount( self ) \
    ( & ( self ) -> dad )

/* Release
 *  release reference
 */
#define NGS_StatisticsRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_StatisticsToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_StatisticsDuplicate( self, ctx ) \
    ( ( NGS_Statistics* ) NGS_RefcountDuplicate ( NGS_StatisticsToRefcount ( self ), ctx ) )
 
    
uint32_t NGS_StatisticsGetValueType ( const NGS_Statistics * self, ctx_t ctx, const char * path );
    
struct NGS_String* NGS_StatisticsGetAsString ( const NGS_Statistics * self, ctx_t ctx, const char * path );

int64_t NGS_StatisticsGetAsI64 ( const NGS_Statistics * self, ctx_t ctx, const char * path );

uint64_t NGS_StatisticsGetAsU64 ( const NGS_Statistics * self, ctx_t ctx, const char * path );

double NGS_StatisticsGetAsDouble ( const NGS_Statistics * self, ctx_t ctx, const char * path );

bool NGS_StatisticsNextPath ( const NGS_Statistics * self, ctx_t ctx, const char * path, const char ** next );

void NGS_StatisticsAddString ( NGS_Statistics * self, ctx_t ctx, const char * path, const struct NGS_String * value );

void NGS_StatisticsAddI64 ( NGS_Statistics * self, ctx_t ctx, const char * path, int64_t value );

void NGS_StatisticsAddU64 ( NGS_Statistics * self, ctx_t ctx, const char * path, uint64_t value );
 
void NGS_StatisticsAddDouble ( NGS_Statistics * self, ctx_t ctx, const char * path, double value );

/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_Statistics
{
    NGS_Refcount dad;
};

enum NGS_StatisticValueType
{
    NGS_StatisticValueType_Undefined,
    NGS_StatisticValueType_String,
    /* other int types ? */
    NGS_StatisticValueType_Int64,
    NGS_StatisticValueType_UInt64,
    NGS_StatisticValueType_Real
};

typedef struct NGS_Statistics_vt NGS_Statistics_vt;
struct NGS_Statistics_vt
{
    NGS_Refcount_vt dad;
    
    uint32_t ( * get_value_type ) ( const NGS_STATISTICS * self, ctx_t ctx, const char * path ); 
    
    struct NGS_String * ( * get_as_string ) ( const NGS_STATISTICS * self, ctx_t ctx, const char * path ); 
    int64_t             ( * get_as_int64 )  ( const NGS_STATISTICS * self, ctx_t ctx, const char * path ); 
    uint64_t            ( * get_as_uint64 ) ( const NGS_STATISTICS * self, ctx_t ctx, const char * path ); 
    double              ( * get_as_double ) ( const NGS_STATISTICS * self, ctx_t ctx, const char * path ); 
    
    bool ( * next_path  ) ( const NGS_STATISTICS * self, ctx_t ctx, const char * path, const char ** new_path );     
    
    void ( * add_string )   ( NGS_STATISTICS * self, ctx_t ctx, const char * path, const struct NGS_String * value );
    void ( * add_int64 )    ( NGS_STATISTICS * self, ctx_t ctx, const char * path, int64_t value );
    void ( * add_uint64 )   ( NGS_STATISTICS * self, ctx_t ctx, const char * path, uint64_t value );
    void ( * add_double )   ( NGS_STATISTICS * self, ctx_t ctx, const char * path, double value );
};

/* Init
*/
void NGS_StatisticsInit ( ctx_t ctx, struct NGS_Statistics * self, NGS_Statistics_vt * vt, const char *clsname, const char *instname );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_statistics_ */
