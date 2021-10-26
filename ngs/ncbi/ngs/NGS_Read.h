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

#ifndef _h_ngs_read_
#define _h_ngs_read_

typedef struct NGS_Read NGS_Read;

#ifndef NGS_READ
#define NGS_READ NGS_Read
#endif

#ifndef _h_ngs_fragment_
#define NGS_FRAGMENT NGS_READ
#include "NGS_Fragment.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct String;
struct VCursor;
struct NGS_String;
struct NGS_Read_v1_vt;
extern struct NGS_Read_v1_vt ITF_Read_vt;

/*--------------------------------------------------------------------------
 * NGS_Read
 */
 
/* MakeCommon
 *  makes a common read from VCursor
 *  MAKES A STANDALONE READ - A SEPARATE IMPLEMENTATION CLASS
 *  mostly, it cannot iterate, and should throw an error when trying
 */
NGS_Read * NGS_ReadMakeCommon ( ctx_t ctx, struct VCursor const * curs, int64_t readId, struct String const * spec );


/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_ReadToRefcount( self ) \
    ( NGS_FragmentToRefcount ( & ( self ) -> dad ) )


/* Release
 *  release reference
 */
#define NGS_ReadRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_ReadToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_ReadDuplicate( self, ctx ) \
    ( ( NGS_Read* ) NGS_RefcountDuplicate ( NGS_ReadToRefcount ( self ), ctx ) )


/* GetReadName
 */
struct NGS_String * NGS_ReadGetReadName ( NGS_Read * self, ctx_t ctx );

/* GetReadId
 */
struct NGS_String * NGS_ReadGetReadId ( NGS_Read * self, ctx_t ctx );

/* GetReadGroup
 */
struct NGS_String * NGS_ReadGetReadGroup ( NGS_Read * self, ctx_t ctx );

/* GetReadCategory
 */
enum NGS_ReadCategory
{	/* keep in synch with ngs :: Read :: ReadCategory */
	/* TODO: use an enum from <ngs/itf/ReadItf.h> */
    NGS_ReadCategory_fullyAligned = 1,
    NGS_ReadCategory_partiallyAligned = 2,
    NGS_ReadCategory_unaligned = 4,
};

enum NGS_ReadCategory NGS_ReadGetReadCategory ( const NGS_Read * self, ctx_t ctx );

/* GetReadSequence
 * GetReadSubSequence
 */
struct NGS_String * NGS_ReadGetReadSequence ( NGS_Read * self, ctx_t ctx, uint64_t offset, uint64_t length );


/* GetReadQualities
 * GetReadSubQualities
 */
struct NGS_String * NGS_ReadGetReadQualities ( NGS_Read * self, ctx_t ctx, uint64_t offset, uint64_t length );


/* NumFragments
 */
uint32_t NGS_ReadNumFragments ( NGS_Read * self, ctx_t ctx );


/* FragIsAligned
 */
bool NGS_ReadFragIsAligned ( NGS_Read * self, ctx_t ctx, uint32_t frag_idx );


/*--------------------------------------------------------------------------
 * NGS_ReadIterator
 */

/* Next
 */
bool NGS_ReadIteratorNext ( NGS_Read * self, ctx_t ctx );

/* GetCount
 */
uint64_t NGS_ReadIteratorGetCount ( const NGS_Read * self, ctx_t ctx );


/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_Read
{
    NGS_Fragment dad;
};

typedef struct NGS_Read_vt NGS_Read_vt;
struct NGS_Read_vt
{
    NGS_Fragment_vt dad;

    /* Read interface */
    struct NGS_String *     ( * get_id )            ( NGS_READ * self, ctx_t ctx );
    struct NGS_String *     ( * get_name )          ( NGS_READ * self, ctx_t ctx );
    struct NGS_String *     ( * get_read_group )    ( NGS_READ * self, ctx_t ctx );
    enum NGS_ReadCategory   ( * get_category )      ( const NGS_READ * self, ctx_t ctx );
    struct NGS_String *     ( * get_sequence )      ( NGS_READ * self, ctx_t ctx, uint64_t offset, uint64_t length );
    struct NGS_String *     ( * get_qualities )     ( NGS_READ * self, ctx_t ctx, uint64_t offset, uint64_t length );
    uint32_t                ( * get_num_fragments ) ( NGS_READ * self, ctx_t ctx );
    bool                    ( * frag_is_aligned )   ( NGS_READ * self, ctx_t ctx, uint32_t frag_idx );

    /* ReadIterator interface */
    bool ( * next ) ( NGS_READ * self, ctx_t ctx );
    uint64_t ( * get_count ) ( const NGS_READ * self, ctx_t ctx );
    
};

/* Init
 */
void NGS_ReadInit ( ctx_t ctx, NGS_Read * read,
    const NGS_Read_vt * vt, const char *clsname, const char *instname );
void NGS_ReadIteratorInit ( ctx_t ctx, NGS_Read * read,
    const NGS_Read_vt * vt, const char *clsname, const char *instname );

/* NullRead
 * will error out on any call
 */
struct NGS_Read * NGS_ReadMakeNull ( ctx_t ctx, const struct NGS_String * spec );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_read_ */
