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

#ifndef _h_ngs_read_group_
#define _h_ngs_read_group_

typedef struct NGS_ReadGroup NGS_ReadGroup;
#ifndef NGS_READGROUP
#define NGS_READGROUP NGS_ReadGroup
#endif

#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_READGROUP
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
struct NGS_Read;
struct NGS_Statistics;
struct NGS_ReadGroup_v1_vt;
extern struct NGS_ReadGroup_v1_vt ITF_ReadGroup_vt;

/*--------------------------------------------------------------------------
 * NGS_ReadGroup
 */
 
#define DEFAULT_READGROUP_NAME "default"

/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_ReadGroupToRefcount( self ) \
    ( & ( self ) -> dad )

/* Release
 *  release reference
 */
#define NGS_ReadGroupRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_ReadGroupToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_ReadGroupDuplicate( self, ctx ) \
    ( ( NGS_ReadGroup* ) NGS_RefcountDuplicate ( NGS_ReadGroupToRefcount ( self ), ctx ) )
 
struct NGS_String* NGS_ReadGroupGetName ( const NGS_ReadGroup * self, ctx_t ctx );

#if READ_GROUP_SUPPORTS_READS
struct NGS_Read* NGS_ReadGroupGetRead ( const NGS_ReadGroup * self, ctx_t ctx, const char * readId );

struct NGS_Read* NGS_ReadGroupGetReads ( const NGS_ReadGroup * self, ctx_t ctx, bool wants_full, bool wants_partial, bool wants_unaligned );
#endif

struct NGS_Statistics* NGS_ReadGroupGetStatistics ( const NGS_ReadGroup * self, ctx_t ctx );

/*--------------------------------------------------------------------------
 * NGS_ReadGroupIterator
 */

/* Next
 */
bool NGS_ReadGroupIteratorNext ( NGS_ReadGroup * self, ctx_t ctx );


/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_ReadGroup
{
    NGS_Refcount dad;
};

typedef struct NGS_ReadGroup_vt NGS_ReadGroup_vt;
struct NGS_ReadGroup_vt
{
    NGS_Refcount_vt dad;

    struct NGS_String*      ( * get_name )          ( const NGS_READGROUP * self, ctx_t ctx );
    struct NGS_Read*        ( * get_reads )         ( const NGS_READGROUP * self, ctx_t ctx, bool wants_full, bool wants_partial, bool wants_unaligned );
    struct NGS_Read*        ( * get_read )          ( const NGS_READGROUP * self, ctx_t ctx, const char * readId );
    struct NGS_Statistics*  ( * get_statistics )    ( const NGS_READGROUP * self, ctx_t ctx );
    bool                    ( * get_next )          ( NGS_READGROUP * self, ctx_t ctx );
};

/* Init
*/
void NGS_ReadGroupInit ( ctx_t ctx, struct NGS_ReadGroup * self, NGS_ReadGroup_vt * vt, const char *clsname, const char *instname );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_read_group_ */
