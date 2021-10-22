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

#ifndef _h_ngs_alignment_
#define _h_ngs_alignment_

typedef struct NGS_Alignment NGS_Alignment;
#ifndef NGS_ALIGNMENT
#define NGS_ALIGNMENT NGS_Alignment
#endif

#ifndef _h_ngs_fragment_
#define NGS_FRAGMENT NGS_ALIGNMENT
#include "NGS_Fragment.h"
#endif

#ifndef _h_insdc_sra_
#include <insdc/sra.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */

struct NGS_String;
struct NGS_Fragment;
struct NGS_Alignment_v1_vt;
extern struct NGS_Alignment_v1_vt ITF_Alignment_vt;

/*--------------------------------------------------------------------------
 * NGS_Alignment
 */

enum NGS_AlignmentFilterBits
{
    NGS_AlignmentFilterBits_pass_bad            = 0x01,
    NGS_AlignmentFilterBits_pass_dups           = 0x02,
    NGS_AlignmentFilterBits_min_map_qual        = 0x04,
    NGS_AlignmentFilterBits_max_map_qual        = 0x08,
    NGS_AlignmentFilterBits_no_wraparound       = 0x10,
    NGS_AlignmentFilterBits_start_within_window = 0x20,

    NGS_AlignmentFilterBits_map_qual = NGS_AlignmentFilterBits_min_map_qual | NGS_AlignmentFilterBits_max_map_qual
};

 /* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_AlignmentToRefcount( self ) \
    ( NGS_FragmentToRefcount ( & ( self ) -> dad ) )


/* Release
 *  release reference
 */
#define NGS_AlignmentRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_AlignmentToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_AlignmentDuplicate( self, ctx ) \
    ( ( NGS_Alignment* ) NGS_RefcountDuplicate ( NGS_AlignmentToRefcount ( self ), ctx ) )


struct NGS_String * NGS_AlignmentGetAlignmentId( NGS_Alignment* self, ctx_t ctx );

struct NGS_String * NGS_AlignmentGetReferenceSpec( NGS_Alignment* self, ctx_t ctx );

int NGS_AlignmentGetMappingQuality( NGS_Alignment* self, ctx_t ctx );

INSDC_read_filter NGS_AlignmentGetReadFilter ( NGS_Alignment * self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetReferenceBases( NGS_Alignment* self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetReadGroup( NGS_Alignment* self, ctx_t ctx );

struct NGS_String * NGS_AlignmentGetReadId( NGS_Alignment* self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetClippedFragmentBases( NGS_Alignment* self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetClippedFragmentQualities( NGS_Alignment* self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetAlignedFragmentBases( NGS_Alignment* self, ctx_t ctx );

bool NGS_AlignmentIsPrimary( NGS_Alignment* self, ctx_t ctx );

int64_t NGS_AlignmentGetAlignmentPosition( NGS_Alignment* self, ctx_t ctx );

uint64_t NGS_AlignmentGetReferencePositionProjectionRange( NGS_Alignment* self, ctx_t ctx, int64_t ref_pos );

uint64_t NGS_AlignmentGetAlignmentLength( NGS_Alignment* self, ctx_t ctx );

bool NGS_AlignmentGetIsReversedOrientation( NGS_Alignment* self, ctx_t ctx );

int NGS_AlignmentGetSoftClip( NGS_Alignment* self, ctx_t ctx, bool left );

uint64_t NGS_AlignmentGetTemplateLength( NGS_Alignment* self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetShortCigar( NGS_Alignment* self, ctx_t ctx, bool clipped );

struct NGS_String* NGS_AlignmentGetLongCigar( NGS_Alignment* self, ctx_t ctx, bool clipped );

char NGS_AlignmentGetRNAOrientation( NGS_Alignment* self, ctx_t ctx );

bool NGS_AlignmentHasMate ( NGS_Alignment* self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetMateAlignmentId( NGS_Alignment* self, ctx_t ctx );

NGS_Alignment* NGS_AlignmentGetMateAlignment( NGS_Alignment* self, ctx_t ctx );

struct NGS_String* NGS_AlignmentGetMateReferenceSpec( NGS_Alignment* self, ctx_t ctx );

bool NGS_AlignmentGetMateIsReversedOrientation( NGS_Alignment* self, ctx_t ctx );

bool NGS_AlignmentIsFirst ( NGS_Alignment* self, ctx_t ctx );

/*--------------------------------------------------------------------------
 * NGS_AlignmentIterator
 */

bool NGS_AlignmentIteratorNext ( NGS_Alignment* self, ctx_t ctx );

/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_Alignment
{
    NGS_Fragment dad;
};

typedef struct NGS_Alignment_vt NGS_Alignment_vt;
struct NGS_Alignment_vt
{
    NGS_Fragment_vt dad;

    /* Alignment */
    struct NGS_String*      ( * getId )                         ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getReferenceSpec )              ( NGS_ALIGNMENT* self, ctx_t ctx );
    int                     ( * getMappingQuality )             ( NGS_ALIGNMENT* self, ctx_t ctx );
    INSDC_read_filter       ( * getReadFilter )                 ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getReferenceBases )             ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getReadGroup )                  ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getReadId )                     ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getClippedFragmentBases )       ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getClippedFragmentQualities )   ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getAlignedFragmentBases )       ( NGS_ALIGNMENT* self, ctx_t ctx );
    bool                    ( * isPrimary )                     ( NGS_ALIGNMENT* self, ctx_t ctx );
    int64_t                 ( * getAlignmentPosition )          ( NGS_ALIGNMENT* self, ctx_t ctx );
    uint64_t                ( * getReferencePositionProjectionRange )( NGS_ALIGNMENT* self, ctx_t ctx, int64_t ref_pos );
    uint64_t                ( * getAlignmentLength )            ( NGS_ALIGNMENT* self, ctx_t ctx );
    bool                    ( * getIsReversedOrientation )      ( NGS_ALIGNMENT* self, ctx_t ctx );
    int                     ( * getSoftClip )                   ( NGS_ALIGNMENT* self, ctx_t ctx, bool left );
    uint64_t                ( * getTemplateLength )             ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getShortCigar )                 ( NGS_ALIGNMENT* self, ctx_t ctx, bool clipped );
    struct NGS_String*      ( * getLongCigar )                  ( NGS_ALIGNMENT* self, ctx_t ctx, bool clipped );
    char                    ( * getRNAOrientation )             ( NGS_ALIGNMENT* self, ctx_t ctx );
    bool                    ( * hasMate )                       ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getMateAlignmentId )            ( NGS_ALIGNMENT* self, ctx_t ctx );
    NGS_ALIGNMENT*          ( * getMateAlignment )              ( NGS_ALIGNMENT* self, ctx_t ctx );
    struct NGS_String*      ( * getMateReferenceSpec )          ( NGS_ALIGNMENT* self, ctx_t ctx );
    bool                    ( * getMateIsReversedOrientation )  ( NGS_ALIGNMENT* self, ctx_t ctx );
    bool                    ( * isFirst )                       ( NGS_ALIGNMENT* self, ctx_t ctx );

    /* Iterator */
    bool                    ( * next )                          ( NGS_ALIGNMENT* self, ctx_t ctx );
};

/* Init
 */
void NGS_AlignmentInit ( ctx_t ctx, NGS_ALIGNMENT * ref, const NGS_Alignment_vt *vt, const char *clsname, const char *instname );

/* NullAlignment
 * will error out on any call; can be used as an empty alignment iterator
 */
struct NGS_Alignment * NGS_AlignmentMakeNull ( ctx_t ctx, char const * run_name, size_t run_name_size );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_alignment_ */
