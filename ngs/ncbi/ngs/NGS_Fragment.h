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

#ifndef _h_ngs_fragment_
#define _h_ngs_fragment_

#ifndef _h_ngs_extern_
#include "extern.h"
#endif

typedef struct NGS_Fragment NGS_Fragment;
#ifndef NGS_FRAGMENT
#define NGS_FRAGMENT NGS_Fragment
#endif

#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_FRAGMENT
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_String;
struct NGS_Read;
struct NGS_Fragment_v1_vt;
extern struct NGS_Fragment_v1_vt ITF_Fragment_vt;


/*--------------------------------------------------------------------------
 * NGS_Fragment
 */


/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_FragmentToRefcount( self ) \
    ( & ( self ) -> dad )


#if 0

/* Release
 *  release reference
 */
#define NGS_FragmentRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_FragmentToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_FragmentDuplicate( self, ctx ) \
    ( ( NGS_Fragment* ) NGS_RefcountDuplicate ( NGS_FragmentToRefcount ( self ), ctx ) )

#endif


/* GetId
 *  returns an unique id within the context of the entire ReadCollection
 */
NGS_EXTERN struct NGS_String * CC NGS_FragmentGetId ( NGS_Fragment* self, ctx_t ctx );


/* GetSequence
 *  read the Fragment sequence
 *  offset is zero based
 *  size is limited to bases available
 */
struct NGS_String * NGS_FragmentGetSequence ( NGS_Fragment * self, ctx_t ctx, uint64_t offset, uint64_t length );


/* GetQualities
 *  read the Fragment qualities as phred-33
 *  offset is zero based
 *  size is limited to qualities available
 */
struct NGS_String * NGS_FragmentGetQualities ( NGS_Fragment * self, ctx_t ctx, uint64_t offset, uint64_t length );


/* IsPaired
 */
bool NGS_FragmentIsPaired ( NGS_Fragment * self, ctx_t ctx );


/* IsAligned
 */
bool NGS_FragmentIsAligned ( NGS_Fragment * self, ctx_t ctx );


/* Make [ OBSOLETE ]
 *  make a stand-alone Fragment from Read
 */
NGS_Fragment * NGS_ReadFragmentMake ( ctx_t ctx, struct NGS_Read const* read , uint32_t idx );



/*--------------------------------------------------------------------------
 * NGS_FragmentIterator
 */

/* Next
 *  advance to the next Fragment
 *  for a Read, this is the next biological Fragment of the Read
 *  for any other iterator, simply returns the next Fragment
 */
bool NGS_FragmentIteratorNext ( NGS_Fragment * self, ctx_t ctx );


/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_Fragment
{
    NGS_Refcount dad;
};

typedef struct NGS_Fragment_vt NGS_Fragment_vt;
struct NGS_Fragment_vt
{
    NGS_Refcount_vt dad;

    /* Fragment interface */
    struct NGS_String * ( * get_id ) ( NGS_FRAGMENT * self, ctx_t ctx );
    struct NGS_String * ( * get_sequence ) ( NGS_FRAGMENT * self, ctx_t ctx, uint64_t offset, uint64_t length );
    struct NGS_String * ( * get_qualities ) ( NGS_FRAGMENT * self, ctx_t ctx, uint64_t offset, uint64_t length );
    bool ( * is_paired ) ( NGS_FRAGMENT * self, ctx_t ctx );
    bool ( * is_aligned ) ( NGS_FRAGMENT * self, ctx_t ctx );

    /* FragmentIterator interface */
    bool ( * next ) ( NGS_FRAGMENT * self, ctx_t ctx );
};

/* Init
 */
void NGS_FragmentInit ( ctx_t ctx, NGS_Fragment * frag, const struct NGS_VTable * ivt, const NGS_Fragment_vt * vt, const char *clsname, const char *instname );

/* Whack
 *  needed to release the ReadCollection
 */
void NGS_FragmentWhack ( NGS_Fragment * self, ctx_t ctx );


#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_fragment_ */
