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

#ifndef _h_ngs_pileupevent_
#define _h_ngs_pileupevent_

typedef struct NGS_PileupEvent NGS_PileupEvent;
#ifndef NGS_PILEUPEVENT
#define NGS_PILEUPEVENT NGS_PileupEvent
#endif

#ifndef _h_ngs_refcount_
#define NGS_REFCOUNT NGS_PILEUPEVENT
#include "NGS_Refcount.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_String;
struct NGS_Alignment;
struct NGS_Reference;
struct NGS_PileupEvent_v1_vt;
extern struct NGS_PileupEvent_v1_vt ITF_PileupEvent_vt;

/*--------------------------------------------------------------------------
 * NGS_PileupEvent
 */
 
/* ToRefcount
 *  inline cast that preserves const
 */
#define NGS_PileupEventToRefcount( self ) \
    ( & ( self ) -> dad )

/* Release
 *  release reference
 */
#define NGS_PileupEventRelease( self, ctx ) \
    NGS_RefcountRelease ( NGS_PileupEventToRefcount ( self ), ctx )

/* Duplicate
 *  duplicate reference
 */
#define NGS_PileupEventDuplicate( self, ctx ) \
    ( ( NGS_PileupEvent* ) NGS_RefcountDuplicate ( NGS_PileupEventToRefcount ( self ), ctx ) ) 
 
int NGS_PileupEventGetMappingQuality( const NGS_PileupEvent * self, ctx_t ctx );

struct NGS_String * NGS_PileupEventGetAlignmentId( const NGS_PileupEvent * self, ctx_t ctx );

int64_t NGS_PileupEventGetAlignmentPosition( const NGS_PileupEvent * self, ctx_t ctx );

int64_t NGS_PileupEventGetFirstAlignmentPosition( const NGS_PileupEvent * self, ctx_t ctx );

int64_t NGS_PileupEventGetLastAlignmentPosition( const NGS_PileupEvent * self, ctx_t ctx );

enum NGS_PileupEventType
{
    /* basic event types */
    NGS_PileupEventType_match        = 0,
    NGS_PileupEventType_mismatch     = 1,
    NGS_PileupEventType_deletion     = 2,

    /* event modifiers */
    NGS_PileupEventType_insertion    = 0x08,
    NGS_PileupEventType_minus_strand = 0x20,
    NGS_PileupEventType_stop         = 0x40,
    NGS_PileupEventType_start        = 0x80
};        
int NGS_PileupEventGetEventType( const NGS_PileupEvent * self, ctx_t ctx );

char NGS_PileupEventGetAlignmentBase( const NGS_PileupEvent * self, ctx_t ctx );

char NGS_PileupEventGetAlignmentQuality( const NGS_PileupEvent * self, ctx_t ctx );

struct NGS_String * NGS_PileupEventGetInsertionBases( const NGS_PileupEvent * self, ctx_t ctx );

struct NGS_String * NGS_PileupEventGetInsertionQualities( const NGS_PileupEvent * self, ctx_t ctx );

unsigned int NGS_PileupEventGetRepeatCount( const NGS_PileupEvent * self, ctx_t ctx );

enum NGS_PileupIndelType
{
    NGS_PileupIndelType_normal         = 0,
    NGS_PileupIndelType_intron_plus    = 1,
    NGS_PileupIndelType_intron_minus   = 2,
    NGS_PileupIndelType_intron_unknown = 3,
    NGS_PileupIndelType_read_overlap   = 4,
    NGS_PileupIndelType_read_gap       = 5
};
int NGS_PileupEventGetIndelType( const NGS_PileupEvent * self, ctx_t ctx );

/*--------------------------------------------------------------------------
 * NGS_PileupEventIterator
 */

/* Next
 */
bool NGS_PileupEventIteratorNext ( NGS_PileupEvent * self, ctx_t ctx );

/* Reset
 */
void NGS_PileupEventIteratorReset ( NGS_PileupEvent * self, ctx_t ctx );


/*--------------------------------------------------------------------------
 * implementation details
 */
struct NGS_PileupEvent
{
    NGS_Refcount dad;
    struct NGS_Reference * ref;
};

typedef struct NGS_PileupEvent_vt NGS_PileupEvent_vt;
struct NGS_PileupEvent_vt
{
    NGS_Refcount_vt dad;
    
    int                     ( * get_mapping_quality )           ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    struct NGS_String *     ( * get_alignment_id )              ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    int64_t                 ( * get_alignment_position )        ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    int64_t                 ( * get_first_alignment_position )  ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    int64_t                 ( * get_last_alignment_position )   ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    int                     ( * get_event_type )                ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    char                    ( * get_alignment_base )            ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    char                    ( * get_alignment_quality )         ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    struct NGS_String *     ( * get_insertion_bases )           ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    struct NGS_String *     ( * get_insertion_qualities )       ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    unsigned int            ( * get_repeat_count )              ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    int                     ( * get_indel_type )                ( const NGS_PILEUPEVENT * self, ctx_t ctx );
    bool                    ( * next )                          ( NGS_PILEUPEVENT * self, ctx_t ctx );    
    void                    ( * reset )                         ( NGS_PILEUPEVENT * self, ctx_t ctx );    
};


/* Init
*/
void NGS_PileupEventInit ( ctx_t ctx, struct NGS_PileupEvent * obj, 
    struct NGS_VTable const * ivt, const NGS_PileupEvent_vt * vt, 
    const char *clsname, const char *instname, struct NGS_Reference * ref );

/* Whack
*/
void NGS_PileupEventWhack ( struct NGS_PileupEvent * self, ctx_t ctx );

#ifdef __cplusplus
}
#endif

#endif /* _h_ngs_pileupevent_ */
