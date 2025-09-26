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

#ifndef _h_csra1_pileup_
#define _h_csra1_pileup_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#ifndef _h_klib_container_
#include <klib/container.h>
#endif

#ifndef _h_insdc_insdc_
#include <insdc/insdc.h>
#endif

#ifndef _h_ngs_pileup_
#include "NGS_Pileup.h"
#endif

#ifndef _h_csra1_reference_
#include "CSRA1_Reference.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct VBlob;
struct VTable;
struct VCursor;
struct KVector;
struct VDatabase;
struct NGS_Cursor;
struct NGS_Reference;


/*--------------------------------------------------------------------------
 * CSRA1_Pileup_Entry
 *  holds extracted data from an alignment record
 *  kept in a list of alignments that intersect the current pileup position
 */
enum
{
    pileup_event_col_MAPQ,
    pileup_event_col_REF_OFFSET,
    pileup_event_col_HAS_REF_OFFSET,
    pileup_event_col_MISMATCH,
    pileup_event_col_HAS_MISMATCH,
    pileup_event_col_REF_ORIENTATION,
    pileup_event_col_QUALITY,

    pileup_event_col_REF_OFFSET_TYPE,           /* OPTIONAL */

    pileup_event_col_count
};

typedef struct CSRA1_Pileup_Entry_State CSRA1_Pileup_Entry_State;
struct CSRA1_Pileup_Entry_State
{
    /* insertion count */
    uint32_t ins_cnt;

    /* deletion count */
    uint32_t del_cnt;

    /* offset into REF_OFFSET */
    uint32_t ref_off_idx;

    /* offset into MISMATCH */
    uint32_t mismatch_idx;

    /* offset into aligned sequence */
    uint32_t seq_idx;

    /* adjustment to "zstart" for current alignment */
    volatile int32_t zstart_adj; /* TODO: find out why volatile or remove it */

    /* set to a base or NUL if not set */
    char mismatch;
};

enum 
{
    pileup_entry_status_INITIAL,
    pileup_entry_status_VALID,
    pileup_entry_status_DONE
};

typedef struct CSRA1_Pileup_Entry CSRA1_Pileup_Entry;
struct CSRA1_Pileup_Entry
{
    /* list node */
    DLNode node;

    /* row id within the alignment table indicated by "secondary" */
    int64_t row_id;

    /* projected range upon reference */
    int64_t zstart;
    int64_t xend;  /* EXCLUSIVE */

    /* blob cache to ensure cell-data remain valid */
    struct VBlob const * blob [ pileup_event_col_count ];
    size_t blob_total;

    /* cell data of interest to pileup event */
    const void * cell_data [ pileup_event_col_count ];
    uint32_t cell_len [ pileup_event_col_count ];

    /* current state of the event */
    CSRA1_Pileup_Entry_State state_curr;

    /* to properly set the current state we have to look ahead */
    CSRA1_Pileup_Entry_State state_next;

    /* true if alignment comes from secondary table */
    bool secondary;

    /* true if blobs were not entirely cached */
    bool temporary;

    /* true if event has already been seen */
    bool seen;

    /* the status of the entry: one of pileup_entry_status_* */
    int status;
};


/*--------------------------------------------------------------------------
 * CSRA1_PileupEvent
 *  built-in base class iterator
 */
struct CSRA1_PileupEvent
{
    NGS_Pileup dad;

    /* current alignment being examined */
    CSRA1_Pileup_Entry * entry;

    /* set to true within "next" */
    bool seen_first;
};


/*--------------------------------------------------------------------------
 * CSRA1_Pileup_AlignList
 *  list of alignments that intersect the current pileup position
 */
typedef struct CSRA1_Pileup_AlignList CSRA1_Pileup_AlignList;
struct CSRA1_Pileup_AlignList
{
    DLList pileup;
    DLList waiting;
    uint32_t depth;
    uint32_t avail;
    uint32_t observed;
    uint32_t max_ref_len;
};


/*--------------------------------------------------------------------------
 * CSRA1_Pileup_RefCursorData
 *  cursor and cell data from REFERENCE table
 */
typedef struct CSRA1_Pileup_RefCursorData CSRA1_Pileup_RefCursorData;
struct CSRA1_Pileup_RefCursorData
{
    struct NGS_Cursor const * curs;
    struct KVector const * pa_ids;
    struct KVector const * sa_ids;
    uint32_t max_seq_len;
};


/*--------------------------------------------------------------------------
 * CSRA1_Pileup_AlignCursorData
 *  cursor, blobs, cell data and column indices from *_ALIGNMENT table
 */
enum
{
    /* pileup-align columns are in a different space from pileup-event columns */
    pileup_align_col_REF_POS = pileup_event_col_count,
    pileup_align_col_REF_LEN,
    pileup_align_col_READ_FILTER,

    /* total of combined columns managed by pileup */
    pileup_align_col_total
};

typedef struct CSRA1_Pileup_AlignCursorData CSRA1_Pileup_AlignCursorData;
struct CSRA1_Pileup_AlignCursorData
{
    struct VCursor const * curs;
    struct VBlob const * blob [ pileup_align_col_total ];
    const void * cell_data [ pileup_align_col_total ];
    uint32_t cell_len [ pileup_align_col_total ];
    uint32_t col_idx [ pileup_align_col_total ];
    bool missing_REF_OFFSET_TYPE;
};


/*--------------------------------------------------------------------------
 * CSRA1_Pileup
 *  represents a pileup iterator
 */
typedef struct CSRA1_Pileup CSRA1_Pileup;
struct CSRA1_Pileup
{
    struct CSRA1_PileupEvent dad;   

    /* rows for this chromosome: [ reference_start_id, reference_last_id ] */
    int64_t reference_start_id;
    int64_t reference_last_id;

    /* effective reference start */
    int64_t effective_ref_zstart; /* ZERO-BASED */

    /* rows for this slice: [ slice_start_id, slice_end_id ] */
    int64_t slice_start_id;
    int64_t slice_end_id;

    /* reference coordinate window: [ slice_start, slice_xend ) */
    int64_t slice_zstart;    /* ZERO-BASED */
    int64_t slice_xend;      /* EXCLUSIVE  */

    /* current iterator position on reference and its chunk id */
    int64_t ref_zpos;
    int64_t ref_chunk_id;

    /* end of current chunk */
    int64_t ref_chunk_xend;

    /* current chunk for reading alignment ids */
    int64_t idx_chunk_id;

    /* cached vblob limit and sum */
    size_t cached_blob_limit;
    size_t cached_blob_total;

    /* pointer to bases of current reference chunk */
    const INSDC_dna_text * ref_chunk_bases;

    /* list of alignments under this position */
    CSRA1_Pileup_AlignList align;

    /* reference cursor/data */
    CSRA1_Pileup_RefCursorData ref;

    /* alignment cursor/data */
    CSRA1_Pileup_AlignCursorData pa, sa;

    /* alignment filters */
    uint32_t filters;
    int32_t map_qual;

    /* reference base - lazily populated */
    char ref_base;

    uint8_t state;
    bool circular;      /* true iff Reference is circular */
};

/* Make
 *  make an iterator across entire reference
 */
struct NGS_Pileup * CSRA1_PileupIteratorMake ( ctx_t ctx, struct NGS_Reference * ref,
    struct VDatabase const * db, struct NGS_Cursor const * curs_ref,
    int64_t first_row_id, int64_t last_row_id, bool wants_primary, bool wants_secondary,
    uint32_t filters, int32_t map_qual );

/* MakeSlice
 *  make an iterator across a portion of reference
 */
struct NGS_Pileup * CSRA1_PileupIteratorMakeSlice ( ctx_t ctx, struct NGS_Reference * ref,
    struct VDatabase const * db, struct NGS_Cursor const * curs_ref,
    int64_t first_row_id, int64_t last_row_id, uint64_t slice_start, 
    uint64_t slice_size, bool wants_primary, bool wants_secondary,
    uint32_t filters, int32_t map_qual );

/* GetEntry
 */
const void * CSRA1_PileupGetEntry ( CSRA1_Pileup * self, ctx_t ctx,
    CSRA1_Pileup_Entry * entry, uint32_t col_idx );

/* PileupEntry method declarations */
void CSRA1_PileupEventWhack ( struct CSRA1_PileupEvent * self, ctx_t ctx );
int CSRA1_PileupEventGetMappingQuality ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
struct NGS_String * CSRA1_PileupEventGetAlignmentId ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
struct NGS_Alignment * CSRA1_PileupEventGetAlignment ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
int64_t CSRA1_PileupEventGetAlignmentPosition ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
int64_t CSRA1_PileupEventGetFirstAlignmentPosition ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
int64_t CSRA1_PileupEventGetLastAlignmentPosition ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
int CSRA1_PileupEventGetEventType ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
char CSRA1_PileupEventGetAlignmentBase ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
char CSRA1_PileupEventGetAlignmentQuality ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
struct NGS_String * CSRA1_PileupEventGetInsertionBases ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
struct NGS_String * CSRA1_PileupEventGetInsertionQualities ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
unsigned int CSRA1_PileupEventGetRepeatCount ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
int CSRA1_PileupEventGetIndelType ( struct CSRA1_PileupEvent const * self, ctx_t ctx );
bool CSRA1_PileupEventIteratorNext ( struct CSRA1_PileupEvent * self, ctx_t ctx );
void CSRA1_PileupEventIteratorReset ( struct CSRA1_PileupEvent * self, ctx_t ctx );
void CSRA1_PileupEventInit ( ctx_t ctx, struct CSRA1_PileupEvent * obj, const NGS_Pileup_vt * vt,
    const char * clsname, const char * instname, struct NGS_Reference * ref );


#ifdef __cplusplus
}
#endif

#endif /* _h_csra1_pileup_ */
