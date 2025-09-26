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

struct CSRA1_Pileup;
#define NGS_PILEUP struct CSRA1_Pileup

#include "CSRA1_Pileup.h"
#include "CSRA1_PileupEvent.h"
#include "NGS_Reference.h"
#include "NGS_Cursor.h"
#include "NGS_String.h"

#include <vdb/blob.h>
#include <vdb/cursor.h>
#include <klib/refcount.h>
#include <klib/vector.h>
#include <klib/sort.h>
#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>

#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <insdc/insdc.h>

#include <klib/rc.h>

#if UNIX
#include <sys/resource.h>
#endif

/* to turn off blob caching entirely, set to 0 */
#define CACHED_BLOB_LIMIT ( 1 << 30 )

/* The READ_AHEAD_LIMIT sets the total number of alignment ids to gather
   at one time from the REFERENCE table indices. This is given as a constant,
   although in the code it is passed as a parameter. We may want to make it
   dependent upon detecting whether alignments are sorted. */
#define READ_AHEAD_LIMIT 10000

/* Heuristic quantity for gathering maximum projected alignment length */
#define MIN_ALIGN_OBSERVE 100

#if _DEBUGGING

#define IGNORE_OVERLAP_REF_POS 0
#define IGNORE_OVERLAP_REF_LEN 0
#define IGNORE_SYSTEM_RLIMIT   0

#include <stdio.h>
static bool printing;
void enable_pileup_printing ( void ) { printing = true; }
#define PRINT( fmt, ... ) if ( printing ) fprintf ( stderr, fmt, __VA_ARGS__ )

#else
#define PRINT( fmt, ... ) ( ( void ) 0 )
#endif


/*--------------------------------------------------------------------------
 * CSRA1_Pileup_Entry
 */

static
void CC CSRA1_Pileup_EntryWhack ( DLNode * node, void * param )
{
    uint32_t i;
    ctx_t ctx = ( ctx_t ) param;
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcDestroying );

    CSRA1_Pileup_Entry * self = ( CSRA1_Pileup_Entry * ) node;

    /* tear down stuff here */
    for ( i = 0; i < sizeof self -> blob / sizeof self -> blob [ 0 ]; ++ i )
    {
        const VBlob * blob = self -> blob [ i ];
        if ( blob != NULL )
            VBlobRelease ( blob );
    }

    free ( self );
}

static
CSRA1_Pileup_Entry * CSRA1_Pileup_EntryMake ( ctx_t ctx, int64_t row_id,
    int64_t ref_zstart, uint64_t ref_len, bool secondary )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    CSRA1_Pileup_Entry * obj = calloc ( 1, sizeof * obj );
    if ( obj == NULL )
        SYSTEM_ERROR ( xcNoMemory, "allocating CSRA1_Pileup_Entry" );
    else
    {
        obj -> row_id = row_id;
        obj -> zstart = ref_zstart;
        obj -> xend = ref_zstart + ref_len;
        obj -> secondary = secondary;

        obj -> status = pileup_entry_status_INITIAL;
    }

    return obj;
}

static
int CSRA1_Pileup_EntryCmp ( const CSRA1_Pileup_Entry * a, const CSRA1_Pileup_Entry * b )
{
    if ( a -> zstart < b -> zstart )
        return -1;
    if ( a -> zstart > b -> zstart )
        return 1;
    if ( a -> xend > b -> xend )
        return -1;
    if ( a -> xend < b -> xend )
        return 1;
    if ( a -> secondary != b -> secondary )
        return a -> secondary - b -> secondary;
    if ( a -> row_id < b -> row_id )
        return -1;
    return a -> row_id > b -> row_id;
}

static
void CSRA1_Pileup_EntrySort ( CSRA1_Pileup_Entry ** base, size_t count )
{
#define SWAP( a, b, off, size )                                                \
    do                                                                         \
    {                                                                          \
        CSRA1_Pileup_Entry * tmp = * ( CSRA1_Pileup_Entry ** ) ( a );          \
        * ( CSRA1_Pileup_Entry ** ) ( a ) = * ( CSRA1_Pileup_Entry ** ) ( b ); \
        * ( CSRA1_Pileup_Entry ** ) ( b ) = tmp;                               \
    }                                                                          \
    while ( 0 )

#define CMP( a, b )                                                                                 \
    CSRA1_Pileup_EntryCmp ( * ( CSRA1_Pileup_Entry ** ) ( a ), * ( CSRA1_Pileup_Entry ** ) ( b ) )

    KSORT ( base, count, sizeof * base, 0, sizeof * base );

#undef SWAP
#undef CMP
}

/*--------------------------------------------------------------------------
 * CSRA1_Pileup_AlignList
 */

static
void CSRA1_Pileup_AlignListWhack ( CSRA1_Pileup_AlignList * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcDestroying );
    DLListWhack ( & self -> pileup, CSRA1_Pileup_EntryWhack, ( void* ) ctx );
    DLListWhack ( & self -> waiting, CSRA1_Pileup_EntryWhack, ( void* ) ctx );
    self -> depth = self -> avail = 0;
}

static
void CSRA1_PileupAlignListSort ( CSRA1_Pileup_AlignList * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    if ( self -> avail > 1 )
    {
        CSRA1_Pileup_Entry ** a = malloc ( self -> avail * sizeof * a );
        if ( a == NULL )
            SYSTEM_ERROR ( xcNoMemory, "allocating CSRA1_Pileup_Entry" );
        else
        {
            CSRA1_Pileup_Entry * e = ( CSRA1_Pileup_Entry * )
                DLListHead ( & self -> waiting );

            uint32_t i, count = self -> avail;
            for ( i = 0; i < count; ++ i )
            {
                a [ i ] = e;
                e = ( CSRA1_Pileup_Entry * ) DLNodeNext ( & e -> node );
            }

            CSRA1_Pileup_EntrySort ( a, count );

            DLListInit ( & self -> waiting );
            for ( i = 0; i < count; ++ i )
            {
                e = a [ i ];
                DLListPushTail ( & self -> waiting, & e -> node );
            }

            free ( a );
        }
    }
}

static
void CSRA1_PileupAlignListMerge ( CSRA1_Pileup_AlignList * self, DLList * pa_waiting, uint32_t pa_avail )
{
    DLList sa_waiting = self -> waiting;
    CSRA1_Pileup_Entry * pe = ( CSRA1_Pileup_Entry * ) DLListHead ( pa_waiting );
    CSRA1_Pileup_Entry * se = ( CSRA1_Pileup_Entry * ) DLListHead ( & sa_waiting );

    DLListInit ( & self -> waiting );

    while ( pe != NULL && se != NULL )
    {
        if ( CSRA1_Pileup_EntryCmp ( pe, se ) < 0 )
        {
            DLListUnlink ( pa_waiting, & pe -> node );
            DLListPushTail ( & self -> waiting, & pe -> node );
            pe = ( CSRA1_Pileup_Entry * ) DLListHead ( pa_waiting );
        }
        else
        {
            DLListUnlink ( & sa_waiting, & se -> node );
            DLListPushTail ( & self -> waiting, & se -> node );
            se = ( CSRA1_Pileup_Entry * ) DLListHead ( & sa_waiting );
        }
    }

    DLListAppendList ( & self -> waiting, pa_waiting );
    DLListAppendList ( & self -> waiting, & sa_waiting );

    self -> avail += pa_avail;
}


/*--------------------------------------------------------------------------
 * CSRA1_Pileup_RefCursorData
 */

static
void CSRA1_Pileup_RefCursorDataWhack ( CSRA1_Pileup_RefCursorData * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( self != NULL );
    KVectorRelease ( self -> pa_ids );
    KVectorRelease ( self -> sa_ids );

    NGS_CursorRelease ( self -> curs, ctx );
}

static
void CSRA1_Pileup_RefCursorDataInit ( ctx_t ctx, CSRA1_Pileup_RefCursorData * obj,
    const NGS_Cursor * curs, int64_t reference_start_id )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcDestroying );

    assert ( obj != NULL );
    assert ( curs != NULL );

    TRY ( obj -> curs = NGS_CursorDuplicate ( curs, ctx ) )
    {
        obj -> max_seq_len = NGS_CursorGetUInt32 ( curs, ctx, reference_start_id, reference_MAX_SEQ_LEN );
    }
}


/*--------------------------------------------------------------------------
 * CSRA1_Pileup_AlignCursorData
 */

static
void CSRA1_Pileup_AlignCursorDataWhack ( CSRA1_Pileup_AlignCursorData * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcDestroying );

    uint32_t i;
    for ( i = 0; i < sizeof self -> blob / sizeof self -> blob [ 0 ]; ++ i )
        VBlobRelease ( self -> blob [ i ] );

    VCursorRelease ( self -> curs );
}

static
void CSRA1_Pileup_AlignCursorDataGetCell ( CSRA1_Pileup_AlignCursorData * self, ctx_t ctx,
    int64_t row_id, uint32_t col_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    rc_t rc;
    uint32_t elem_bits, boff;

    if ( self -> blob [ col_idx ] != NULL )
    {
        rc = VBlobCellData ( self -> blob [ col_idx ], row_id, & elem_bits,
            & self -> cell_data [ col_idx ], & boff, & self -> cell_len [ col_idx ] );
        if ( rc == 0 )
            return;

        VBlobRelease ( self -> blob [ col_idx ] );
        self -> blob [ col_idx ] = NULL;
    }

    self -> cell_data [ col_idx ] = NULL;

    rc = VCursorGetBlobDirect ( self -> curs, & self -> blob [ col_idx ], row_id, self -> col_idx [ col_idx ] );
    if ( rc != 0 )
        INTERNAL_ERROR ( xcStorageExhausted, "VCursorGetBlobDirect rc = %R", rc );
    else
    {
        rc = VBlobCellData ( self -> blob [ col_idx ], row_id, & elem_bits,
            & self -> cell_data [ col_idx ], & boff, & self -> cell_len [ col_idx ] );
        if ( rc != 0 )
            INTERNAL_ERROR ( xcStorageExhausted, "VBlobCellData rc = %R", rc );
    }
}

static
void CSRA1_Pileup_AlignCursorDataGetNonEmptyCell ( CSRA1_Pileup_AlignCursorData * self, ctx_t ctx,
    int64_t row_id, uint32_t col_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CSRA1_Pileup_AlignCursorDataGetCell ( self, ctx, row_id, col_idx ) )
    {
        if ( self -> cell_len [ col_idx ] == 0 )
            INTERNAL_ERROR ( xcColumnEmpty, "zero-length cell data (row_id = %ld, col_idx = %u)", row_id, col_idx );
    }
}

/*static
uint8_t CSRA1_Pileup_AlignCursorDataGetUInt8 ( CSRA1_Pileup_AlignCursorData * self, ctx_t ctx,
    int64_t row_id, uint32_t col_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CSRA1_Pileup_AlignCursorDataGetNonEmptyCell ( self, ctx, row_id, col_idx ) )
    {
        const uint8_t * p = self -> cell_data [ col_idx ];
        return p [ 0 ];
    }

    return 0;
}*/

static
int32_t CSRA1_Pileup_AlignCursorDataGetInt32 ( CSRA1_Pileup_AlignCursorData * self, ctx_t ctx,
    int64_t row_id, uint32_t col_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CSRA1_Pileup_AlignCursorDataGetNonEmptyCell ( self, ctx, row_id, col_idx ) )
    {
        const int32_t * p = self -> cell_data [ col_idx ];
        return p [ 0 ];
    }

    return 0;
}

static
uint32_t CSRA1_Pileup_AlignCursorDataGetUInt32 ( CSRA1_Pileup_AlignCursorData * self, ctx_t ctx,
    int64_t row_id, uint32_t col_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CSRA1_Pileup_AlignCursorDataGetNonEmptyCell ( self, ctx, row_id, col_idx ) )
    {
        const uint32_t * p = self -> cell_data [ col_idx ];
        return p [ 0 ];
    }

    return 0;
}

/*--------------------------------------------------------------------------
 * CSRA1_Pileup
 */

enum
{
    /* the initial state of a pileup iterator object */
    pileup_state_invalid,

    /* the iterator needs to advance to the next position */
    pileup_state_position,

    /* the iterator needs to advance to the next chunk */
    pileup_state_chunk,

    /* the iterator needs to read alignment ids
       and populate the alignment list */
    pileup_state_populate,

    /* the iterator has not yet had "next" called,
       and needs to be primed for further operation */
    pileup_state_initial,
    pileup_state_initial_position,
    pileup_state_initial_chunk,
    pileup_state_initial_populate,

    /* terminated states */
    pileup_state_finished,
    pileup_state_err,

    /* object has been destroyed */
    pileup_state_zombie
};


/* TBD - check these error types */
static
void CSRA1_PileupStateTest ( const CSRA1_Pileup * self, ctx_t ctx, uint32_t lineno )
{
    assert ( self != NULL );

    switch ( self -> state )
    {
    case pileup_state_invalid:
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_internal, xcIteratorUninitialized, "PileupIterator invalid state" );
        break;
    case pileup_state_position:
    case pileup_state_chunk:
    case pileup_state_populate:
        break;
    case pileup_state_initial:
    case pileup_state_initial_position:
    case pileup_state_initial_chunk:
    case pileup_state_initial_populate:
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_user, xcIteratorUninitialized,
                    "Pileup accessed before a call to PileupIteratorNext()" );
        break;
    case pileup_state_finished:
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_user, xcCursorExhausted, "No more rows available" );
        break;
    case pileup_state_err:
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_user, xcIteratorUninitialized, "PileupIterator invalid state" );
        break;
    case pileup_state_zombie:
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_internal, xcIteratorUninitialized, "PileupIterator accessed after destruction" );
        break;
    default:
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_internal, xcIteratorUninitialized, "PileupIterator unknown state" );
    }
}

#define CHECK_STATE( self, ctx ) \
    CSRA1_PileupStateTest ( self, ctx, __LINE__ )


static
void CSRA1_PileupWhack ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    self -> state = pileup_state_zombie;

    /* any alignments */
    CSRA1_Pileup_AlignListWhack ( & self -> align, ctx );

    /* alignment cursors, blobs */
    CSRA1_Pileup_AlignCursorDataWhack ( & self -> pa, ctx );
    CSRA1_Pileup_AlignCursorDataWhack ( & self -> sa, ctx );

    /* reference cursor, blobs */
    CSRA1_Pileup_RefCursorDataWhack ( & self -> ref, ctx );

    CSRA1_PileupEventWhack ( & self -> dad, ctx );
}

static
NGS_String * CSRA1_PileupGetReferenceSpec ( const CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CHECK_STATE ( self, ctx ) )
    {
        return NGS_ReferenceGetCanonicalName ( self -> dad . dad . dad . ref, ctx );
    }

    return NULL;
}

static
int64_t CSRA1_PileupGetReferencePosition ( const CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CHECK_STATE ( self, ctx ) )
    {
        return self -> ref_zpos;
    }

    return 0;
}

static
char CSRA1_PileupGetReferenceBase ( const CSRA1_Pileup * cself, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    CSRA1_Pileup * self = ( CSRA1_Pileup * ) cself;

    TRY ( CHECK_STATE ( self, ctx ) )
    {
        if ( self -> ref_base == 0 )
        {
            if ( self -> ref_chunk_bases == NULL )
            {
                const void * base;
                uint32_t elem_bits, boff, row_len;
                ON_FAIL ( NGS_CursorCellDataDirect ( self -> ref . curs, ctx, self -> ref_chunk_id,
                    reference_READ, & elem_bits, & base, & boff, & row_len ) )
                {
                    return 0;
                }

                self -> ref_chunk_bases = base;
                assert ( row_len == self -> ref . max_seq_len ||
                         self -> ref_chunk_xend - self -> ref . max_seq_len + row_len >= self -> slice_xend );
            }

            assert ( self -> ref . max_seq_len != 0 );
            self -> ref_base = self -> ref_chunk_bases [ self -> ref_zpos % self -> ref . max_seq_len ];
        }

        return self -> ref_base;

    }

    return 0;
}

static
unsigned int CSRA1_PileupGetDepth ( const CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CHECK_STATE ( self, ctx ) )
    {
        return ( unsigned int ) self -> align . depth;
    }

    return 0;
}

static
bool CSRA1_PileupPosition ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    if ( self -> align . avail != 0 )
    {

        CSRA1_Pileup_Entry * head = ( CSRA1_Pileup_Entry * )
            DLListHead ( & self -> align . waiting );

        uint32_t avail = 0;
        CSRA1_Pileup_Entry * prev = NULL;
        CSRA1_Pileup_Entry * entry = head;

        /* walk the waiting list, adding everything to the end of pileup list */
        while ( entry != NULL )
        {
            if ( entry -> zstart > self -> ref_zpos )
                break;

PRINT ( ">>> adding alignment at refpos %lld, row-id %lld: %lld-%lld ( zero-based, half-closed )\n",
         ( long long int ) self -> ref_zpos, ( long long int ) entry -> row_id, ( long long int ) entry -> zstart, ( long long int ) entry -> xend );

            prev = entry;

            ++ avail;

            entry = ( CSRA1_Pileup_Entry * )
                DLNodeNext ( & entry -> node );
        }

        /* if "entry" is NULL, then everything waiting is ready */
        if ( entry == NULL )
        {
            assert ( self -> align . avail == avail );

            DLListAppendList ( & self -> align . pileup, & self -> align . waiting );
            self -> align . depth += avail;
            self -> align . avail = 0;
        }

        /* otherwise, just take the top guys off the end */
        else if ( prev != NULL )
        {
            if ( head == prev )
            {
                assert ( avail == 1 );
                DLListPopHead ( & self -> align . waiting );
                DLListPushTail ( & self -> align . pileup, & head -> node );
                self -> align . avail -= 1;
                self -> align . depth += 1;
            }
            else
            {
                DLList transfer;

                /* TBD - add this functionality to DLList */

                /* sever the ties */
                prev -> node . next = NULL;
                entry -> node . prev = NULL;

                /* put the new guys onto transfer list */
                transfer . head = & head -> node;
                transfer . tail = & prev -> node;

                /* update the waiting list */
                self -> align . waiting . head = & entry -> node;
                self -> align . avail -= avail;

                /* transfer elements */
                DLListAppendList ( & self -> align . pileup, & transfer );
                self -> align . depth += avail;
            }
        }
    }

    /* update cached REFERENCE values at current position */
    self -> ref_base = 0;

    return self -> ref_zpos< self -> slice_xend;
}

static
bool CSRA1_PileupAdvance ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    CSRA1_Pileup_Entry * entry;

    /* see if advance is possible */
    if ( ++ self -> ref_zpos >= self -> slice_xend )
    {
        /* should never go over */
        assert ( self -> ref_zpos == self -> slice_xend );
        self -> state = pileup_state_finished;
        return false;
    }

    /* test for end of chunk */
    if ( self -> ref_zpos == self -> ref_chunk_xend )
    {
        -- self -> ref_zpos;
        self -> ref_chunk_id += 1;
        self -> state = pileup_state_chunk;
        return false;
    }

    /* drop everything that ends at current position */
    entry = ( CSRA1_Pileup_Entry * )
        DLListHead ( & self -> align . pileup );

    while ( entry != NULL )
    {
        CSRA1_Pileup_Entry * next = ( CSRA1_Pileup_Entry * )
            DLNodeNext ( & entry -> node );

        /* test for temporarily cached data */
        if ( entry -> temporary )
        {
            uint32_t i;
#if _DEBUGGING
            uint32_t num_flushed = 0;
#endif

            for ( i = 0; i < sizeof entry -> cell_data / sizeof entry -> cell_data [ 0 ]; ++ i )
            {
                if ( entry -> cell_data [ i ] != NULL && entry -> blob [ i ] == NULL )
                {
                    entry -> cell_data [ i ] = NULL;
                    entry -> cell_len [ i ] = 0;
#if _DEBUGGING
                    ++ num_flushed;
#endif
                }
            }

PRINT ( ">>> flushed %u columns of temporary cell data\n", num_flushed );

            entry -> temporary = false;
        }

        if ( entry -> xend == self -> ref_zpos || entry -> status == pileup_entry_status_DONE )
        {
PRINT ( ">>> dropping alignment at refpos %lld, row-id %lld: %lld-%lld ( zero-based, half-closed )\n",
         ( long long int ) self -> ref_zpos, ( long long int ) entry -> row_id, ( long long int ) entry -> zstart, ( long long int ) entry -> xend );

            DLListUnlink ( & self -> align . pileup, & entry -> node );
            self -> align . depth -= 1;
            self -> cached_blob_total -= entry -> blob_total;
            CSRA1_Pileup_EntryWhack ( & entry -> node, ( void* ) ctx );
        }

        entry = next;
    }

    return CSRA1_PileupPosition ( self, ctx );
}

static
void CSRA1_PileupChunk ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    /* zero out any cached REFERENCE cells for current chunk */
    self -> ref_chunk_bases = NULL;
    self -> ref_base = 0;

    /* prepare next chunk */
    self -> ref_chunk_xend = ( self -> ref_chunk_id - self -> reference_start_id + 1 ) * self -> ref . max_seq_len;

    /* detect need to populate */
    if ( self -> ref_chunk_id >= self -> idx_chunk_id )
        self -> state = pileup_state_populate;
    else
        self -> state = pileup_state_position;
}

static
uint32_t CSRA1_PileupGatherCategoryIds ( CSRA1_Pileup * self, ctx_t ctx, const KVector * ids, uint32_t col_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    const void * base;
    uint32_t elem_bits, boff, row_len = 0;

    TRY ( NGS_CursorCellDataDirect ( self -> ref . curs, ctx,
          self -> idx_chunk_id, col_idx, & elem_bits, & base, & boff, & row_len ) )
    {
        uint32_t i;
        const int64_t * cell = base;

        /* test for alignment */
        assert ( ( ( size_t ) cell & ( sizeof * cell - 1 ) ) == 0 );

        /* copy the row ids */
        for ( i = 0; i < row_len; ++ i )
        {
            rc_t rc = KVectorSetBool ( ( KVector * ) ids, cell [ i ], true );
            if ( rc != 0 )
            {
                INTERNAL_ERROR ( xcStorageExhausted, "KVectorSetBool rc = %R", rc );
                break;
            }
        }
    }

    return row_len;
}

static
uint32_t CSRA1_PileupGatherChunkIds ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    uint32_t pa_ids = 0;
    uint32_t sa_ids = 0;

    if ( self -> pa . curs != NULL )
    {
        ON_FAIL ( pa_ids = CSRA1_PileupGatherCategoryIds ( self, ctx, self -> ref . pa_ids, reference_PRIMARY_ALIGNMENT_IDS ) )
            return 0;
    }

    if ( self -> sa . curs != NULL )
    {
        ON_FAIL ( sa_ids = CSRA1_PileupGatherCategoryIds ( self, ctx, self -> ref . sa_ids, reference_SECONDARY_ALIGNMENT_IDS ) )
            return 0;
    }

    return pa_ids + sa_ids;
}

static
void CSRA1_PileupGatherIds ( CSRA1_Pileup * self, ctx_t ctx, uint32_t id_limit )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    rc_t rc;
    KVector * v;
    uint32_t total_ids;

    /* create empty KVectors for each category in use */
    if ( self -> pa . curs != NULL )
    {
        KVectorRelease ( self -> ref . pa_ids );
        rc = KVectorMake ( & v );
        self -> ref . pa_ids = v;
        if ( rc != 0 )
            INTERNAL_ERROR ( xcStorageExhausted, "KVectorMake rc = %R", rc );
    }
    if ( ! FAILED () && self -> sa . curs != NULL )
    {
        KVectorRelease ( self -> ref . sa_ids );
        rc = KVectorMake ( & v );
        self -> ref . sa_ids = v;
        if ( rc != 0 )
            INTERNAL_ERROR ( xcStorageExhausted, "KVectorMake rc = %R", rc );
    }

    /* set row ids appropriately */
    self -> idx_chunk_id = self -> ref_chunk_id;

    /* loop until we reach a limit */
    total_ids = 0;
    do
    {
        uint32_t align_ids;

        ON_FAIL ( align_ids = CSRA1_PileupGatherChunkIds ( self, ctx ) )
            break;

        total_ids += align_ids;
        ++ self -> idx_chunk_id;

    }
    while ( total_ids < id_limit && self -> idx_chunk_id <= self -> slice_end_id );
}

/*  Get RD_FILTER and check if it's present
    e.g for SRR1164787 SECONDARY_ALIGNMENT row_id == 3
    has invalid data (SEQ_ID == 0, no RD_FILTER) -
    we need to ignore such records

    return: true - there is a valid RD_FILTER value in the db
            false - there is no RD_FILTER in the db
*/
static bool CSRA1_Pileup_GetReadFilter ( CSRA1_Pileup * self, ctx_t ctx,
    int64_t row_id, CSRA1_Pileup_AlignCursorData * cd, INSDC_read_filter* ret_val )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    TRY ( CSRA1_Pileup_AlignCursorDataGetCell ( cd, ctx, row_id, pileup_align_col_READ_FILTER ) )
    {
        if ( cd -> cell_len [ pileup_align_col_READ_FILTER ] == 0 )
            return false;
    }

    * ret_val = *( (INSDC_read_filter*) cd -> cell_data [ pileup_align_col_READ_FILTER ]);
    return true;
}

static
bool CSRA1_PileupFilterAlignment ( CSRA1_Pileup * self, ctx_t ctx,
    int64_t row_id, CSRA1_Pileup_AlignCursorData * cd )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    int32_t map_qual;
    INSDC_read_filter read_filter;

    if ( CSRA1_Pileup_GetReadFilter ( self, ctx, row_id, cd, & read_filter ) == true )
    /*TRY ( read_filter = CSRA1_Pileup_AlignCursorDataGetUInt8 ( cd, ctx, row_id, pileup_align_col_READ_FILTER ) )*/
    {
        switch ( read_filter )
        {
        case READ_FILTER_PASS:

            /* unless looking at mapping quality, accept */
            if ( ( self -> filters & NGS_PileupFilterBits_map_qual ) == 0 )
                return true;

            /* look at mapping quality for alignment */
            TRY ( map_qual = CSRA1_Pileup_AlignCursorDataGetInt32 ( cd, ctx, row_id, pileup_event_col_MAPQ ) )
            {
                /* generally specify a minimum mapping quality */
                if ( ( self -> filters & NGS_PileupFilterBits_min_map_qual ) != 0 )
                    return map_qual >= self -> map_qual;

                /* but allow a maximum as well */
                return map_qual <= self -> map_qual;
            }
            break;

        case READ_FILTER_REJECT:
            return ( self -> filters & NGS_PileupFilterBits_pass_bad ) != 0;

        case READ_FILTER_CRITERIA:
            return ( self -> filters & NGS_PileupFilterBits_pass_dups ) != 0;

        case READ_FILTER_REDACTED:
            /* do not include */
            break;
        }
    }
    return false;
}

static
CSRA1_Pileup_Entry * CSRA1_PileupReadAlignment ( CSRA1_Pileup * self, ctx_t ctx,
    int64_t row_id, CSRA1_Pileup_AlignCursorData * cd )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    bool pass;
    uint64_t ref_len;
    int64_t ref_zstart;
    bool secondary = ( cd == & self -> sa );

    TRY ( pass = CSRA1_PileupFilterAlignment ( self, ctx, row_id, cd ) )
    {
        if ( pass )
        {
            TRY ( ref_zstart = CSRA1_Pileup_AlignCursorDataGetUInt32 ( cd, ctx, row_id, pileup_align_col_REF_POS ) )
            {
                /* adjust for circular reference wrap-around.
                   applied when gathering initial alignments whose
                   effective starting coordinates will go negative */
                ref_zstart += self -> effective_ref_zstart;

                /* could perform early filtering here, but we want to
                   detect the longest REF_LEN, so don't exclude */
                TRY ( ref_len = CSRA1_Pileup_AlignCursorDataGetUInt32 ( cd, ctx, row_id, pileup_align_col_REF_LEN ) )
                {
                    int64_t ref_xend = ref_zstart + ref_len;

                    self -> align . observed += 1;
                    if ( ( uint32_t ) ref_len > self -> align . max_ref_len )
                        self -> align . max_ref_len = ( uint32_t ) ref_len;

                    if ( ref_zstart < self -> slice_xend && ref_xend > self -> slice_zstart )
                    {
                        CSRA1_Pileup_Entry * entry;

                        TRY ( entry = CSRA1_Pileup_EntryMake ( ctx, row_id, ref_zstart, ref_len, secondary ) )
                        {
                            DLListPushTail ( & self -> align . waiting, & entry -> node );
                            self -> align . avail += 1;
                            return entry;
                        }
                    }
                }
            }
        }
    }

    return NULL;
}

static
bool CSRA1_PileupPopulateCategory ( CSRA1_Pileup * self, ctx_t ctx,
    const KVector * ids, CSRA1_Pileup_AlignCursorData * cd )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    uint64_t cur, next;
    bool ordered = true;
    CSRA1_Pileup_Entry * last = NULL;

    /* get first id */
    bool ignore;
    const char * vfunc = "First";
    rc_t rc = KVectorGetFirstBool ( ids, & next, & ignore );

    for ( cur = next; rc == 0; cur = next )
    {
        CSRA1_Pileup_Entry * entry;

        /* "next" is our row-id */
        ON_FAIL ( entry = CSRA1_PileupReadAlignment ( self, ctx, ( int64_t ) cur, cd ) )
            break;

        /* detect out of order alignments */
        if ( ordered && entry != NULL )
        {
            if ( last != NULL && CSRA1_Pileup_EntryCmp ( entry, last ) < 0 )
                ordered = false;

            last = entry;
        }

        /* get next id */
        vfunc = "Next";
        rc = KVectorGetNextBool ( ids, & next, cur, & ignore );
    }

    if ( rc != 0 && GetRCState ( rc ) != rcNotFound )
        INTERNAL_ERROR ( xcStorageExhausted, "KVectorGet%sBool rc = %R", vfunc, rc );

    return ordered;
}

static
void CSRA1_PileupPopulate ( CSRA1_Pileup * self, ctx_t ctx, uint32_t id_limit )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    /* read some ids */
    TRY ( CSRA1_PileupGatherIds ( self, ctx, id_limit ) )
    {
        bool ordered = true;

        /* gather primary alignments */
        if ( self -> pa . curs != NULL )
        {
            TRY ( ordered = CSRA1_PileupPopulateCategory ( self, ctx, self -> ref . pa_ids, & self -> pa ) )
            {
                if ( ! ordered )
                    CSRA1_PileupAlignListSort ( & self -> align, ctx );
            }
        }

        /* gather secondary alignments */
        if ( ! FAILED () && self -> sa . curs != NULL )
        {
            /* capture the waiting guys from primary alignment */
            DLList pa_waiting = self -> align . waiting;
            uint32_t pa_avail = self -> align . avail;

            /* erase the list for secondary */
            DLListInit ( & self -> align . waiting );
            self -> align . avail = 0;

            /* populate with secondary alignments */
            TRY ( ordered = CSRA1_PileupPopulateCategory ( self, ctx, self -> ref . sa_ids, & self -> sa ) )
            {
                if ( ! ordered )
                    CSRA1_PileupAlignListSort ( & self -> align, ctx );

                if ( pa_avail != 0 )
                    CSRA1_PileupAlignListMerge ( & self -> align, & pa_waiting, pa_avail );
            }
            CATCH_ALL ()
            {
                /* put anything that was already gathered onto list for destruction */
                DLListPrependList ( & self -> align . waiting, & pa_waiting );
                self -> align . avail += pa_avail;
            }
        }
    }
}

static
bool CSRA1_PileupGetOverlapPossible ( const CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    const void * base;
    uint32_t elem_bits, boff, row_len;

#if IGNORE_OVERLAP_REF_LEN
    return true;
#endif

    /* read OVERLAP_REF_LEN */
    TRY ( NGS_CursorCellDataDirect ( self -> ref . curs, ctx, self -> slice_start_id,
              reference_OVERLAP_REF_LEN, & elem_bits, & base, & boff, & row_len ) )
    {
        const uint32_t * OVERLAP_REF_LEN = base;
        uint32_t slice_start = ( uint32_t ) ( self -> slice_zstart % self -> ref . max_seq_len );

        assert ( ( ( size_t ) OVERLAP_REF_LEN & ( sizeof * OVERLAP_REF_LEN - 1 ) ) == 0 );

        if ( self -> pa . curs != NULL )
        {
            if ( OVERLAP_REF_LEN [ 0 ] > slice_start )
                return true;
        }
        if ( self -> sa . curs != NULL )
        {
            if ( OVERLAP_REF_LEN [ 1 ] > slice_start )
                return true;
        }

        /* according to the recorded data, no overlap is possible for linear references */
        return self -> circular;
    }
    CATCH_ALL ()
    {
        CLEAR ();
    }

    return true;
}

static
bool CSRA1_PileupGetOverlapChunkId ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    const void * base;
    uint32_t elem_bits, boff, row_len;

    /* TBD - there are cases when this may be valid even when circular */
    if ( self -> circular )
        return false;

#if IGNORE_OVERLAP_REF_POS
    return false;
#endif

    /* read OVERLAP_REF_POS */
    TRY ( NGS_CursorCellDataDirect ( self -> ref . curs, ctx, self -> slice_start_id,
              reference_OVERLAP_REF_POS, & elem_bits, & base, & boff, & row_len ) )
    {
        bool have_overlap_ref_pos = false;
        const int32_t * OVERLAP_REF_POS = base;
        int64_t new_chunk_id, ref_chunk_id = self -> ref_chunk_id;

        assert ( ( ( size_t ) OVERLAP_REF_POS & ( sizeof * OVERLAP_REF_POS - 1 ) ) == 0 );

        if ( self -> pa . curs != NULL && OVERLAP_REF_POS [ 0 ] > 0 )
        {
            new_chunk_id = ( OVERLAP_REF_POS [ 0 ] - 1 ) / self -> ref . max_seq_len + self -> reference_start_id;
            if ( new_chunk_id < ref_chunk_id )
                ref_chunk_id = new_chunk_id;
            have_overlap_ref_pos = true;
        }

        if ( self -> sa . curs != NULL && OVERLAP_REF_POS [ 1 ] > 0 )
        {
            new_chunk_id = ( OVERLAP_REF_POS [ 1 ] - 1 ) / self -> ref . max_seq_len + self -> reference_start_id;
            if ( new_chunk_id < ref_chunk_id )
                ref_chunk_id = new_chunk_id;
            have_overlap_ref_pos = true;
        }

        self -> ref_chunk_id = ref_chunk_id;
        return have_overlap_ref_pos;
    }
    CATCH_ALL ()
    {
        CLEAR ();
    }

    return false;
}

static
void CSRA1_PileupOverlap ( CSRA1_Pileup * self, ctx_t ctx, int64_t stop_xid )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    while ( ! FAILED () )
    {
        /* detect circular wrap around */
        if ( self -> ref_chunk_id > self -> reference_last_id )
        {
            assert ( self -> circular );
            assert ( self -> effective_ref_zstart != 0 );
            self -> ref_chunk_id = self -> reference_start_id;
            self -> effective_ref_zstart = 0;
        }

        /* exit condition #1: at end of overlap */
        if ( self -> ref_chunk_id == stop_xid )
            break;

        /* run population from ref_chunk_id to stop_xid ( exclusive ) */
        CSRA1_PileupPopulate ( self, ctx, 0 );
        self -> ref_chunk_id = self -> idx_chunk_id;
    }
}

static
void CSRA1_PileupRevOverlap ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    while ( ! FAILED () )
    {
        DLList waiting;
        uint32_t avail;

        /* exit condition #1: at start of non-circular reference */
        if ( ! self -> circular && self -> ref_chunk_id == self -> reference_start_id )
            break;

        /* exit condition #2: have seen sufficient alignments to
           obtain a maximum projected length, and have backed up
           far enough to include this length */
        if ( self -> align . observed >= MIN_ALIGN_OBSERVE )
        {
            int64_t stop_xid = self -> ref_chunk_id;
            int64_t overlap_zstart = self -> slice_zstart - self -> align . max_ref_len;

            if ( overlap_zstart >= 0 )
                self -> ref_chunk_id = overlap_zstart / self -> ref . max_seq_len + self -> reference_start_id;
            else if ( ! self -> circular )
                self -> ref_chunk_id = self -> reference_start_id;
            else
            {
                uint64_t ref_len = NGS_ReferenceGetLength ( self -> dad . dad . dad . ref, ctx );
                overlap_zstart += ref_len;
                self -> ref_chunk_id = overlap_zstart / self -> ref . max_seq_len + self -> reference_start_id;
                self -> effective_ref_zstart -= ref_len;
            }

            CSRA1_PileupOverlap ( self, ctx, stop_xid );
            break;
        }

        /* save previous results */
        waiting = self -> align . waiting;
        avail = self -> align . avail;

        /* reset accumulator */
        DLListInit ( & self -> align . waiting );
        self -> align . avail = 0;

        /* need to back up one chunk and continue */
        if ( self -> ref_chunk_id != self -> reference_start_id )
            self -> ref_chunk_id -= 1;
        else
        {
            assert ( self -> circular );

            /* this should never occur */
            if ( self -> effective_ref_zstart != 0 )
                break;

            /* linearize circularity */
            self -> effective_ref_zstart -= NGS_ReferenceGetLength ( self -> dad . dad . dad . ref, ctx );

            /* wrap around */
            self -> ref_chunk_id = self -> reference_last_id;
        }

        CSRA1_PileupPopulate ( self, ctx, 0 );

        /* regardless of error state, merge old results back in */
        DLListAppendList ( & self -> align . waiting, & waiting );
        self -> align . avail += avail;
    }
}

static
void CSRA1_PileupFirst ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    /* APPROACH

       The general task here is to get all of the alignments in the
       first chunk whose projection contains THE CURRENT POSITION,
       in addition to all alignments within preceding chunks that also
       contain the current position. So the process is almost exactly
       like processing every normal chunk, with the differences that

         a) we look for intersection rather than entry
         b) we look outside ( to the left ) of the slice

       Circular references with chunks 1..N may be linearized as

         -N..2N

       When looking to the left of the first chunk, we are limited by

         a) the start of non-circular references
         b) the maximum length of any alignment's projection, i.e.
            only back up to current position - length of longest projection

       An optimization exists that utilizes pre-calculated overlap
       chunk row-ids. This optimization does not apply for circular
       references.

       In reality, we are interested in knowing the exact value of the
       longest projection onto the reference of any alignment. If this is
       not known a-priori, then a sampling of some number of alignments gives
       a reasonable estimate. This sample count needs to be added to the
       limitation "b" above, such that we can only know the maximum projection
       once the sample size conditions are met.

     */

    /* initialize current chunk */
    self -> ref_chunk_id = self -> slice_start_id;
    self -> state = pileup_state_initial_populate;

    /* test if there is even any possibility of overlap */
    if ( ! CSRA1_PileupGetOverlapPossible ( self, ctx ) )
        return;

    /* process the current chunk */
    TRY ( CSRA1_PileupPopulate ( self, ctx, READ_AHEAD_LIMIT ) )
    {
        /* we read ahead this far */
        int64_t idx_chunk_id = self -> idx_chunk_id;

        /* must save current avail list */
        DLList waiting = self -> align . waiting;
        uint32_t avail = self -> align . avail;

        /* reinitialize */
        DLListInit ( & self -> align . waiting );
        self -> align . avail = 0;

        /* look for OVERLAP_REF_POS optimization */
        if ( CSRA1_PileupGetOverlapChunkId ( self, ctx ) )
            CSRA1_PileupOverlap ( self, ctx, self -> slice_start_id );

        /* scan backward for overlaps */
        else
            CSRA1_PileupRevOverlap ( self, ctx );

        /* append previously saved waiting */
        DLListAppendList ( & self -> align . waiting, & waiting );
        self -> align . avail += avail;

        /* restore to prior value */
        self -> idx_chunk_id = idx_chunk_id;
    }

    /* restore to original value */
    self -> ref_chunk_id = self -> slice_start_id;
    self -> effective_ref_zstart = 0;
    self -> state = pileup_state_initial_chunk;
}

static
bool CSRA1_PileupIteratorGetNext ( CSRA1_Pileup * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    bool pos_valid = false;

    assert ( self != NULL );

    while ( 1 )
    {
        switch ( self -> state )
        {

        case pileup_state_position:

            ON_FAIL ( pos_valid = CSRA1_PileupAdvance ( self, ctx ) )
            {
                self -> state = pileup_state_err;
                break;
            }

            if ( ! pos_valid )
            {
                assert ( self -> state != pileup_state_position );
                continue;
            }

            break;

        case pileup_state_chunk:

            ON_FAIL ( CSRA1_PileupChunk ( self, ctx ) )
            {
                self -> state = pileup_state_err;
                break;
            }

            assert ( self -> state != pileup_state_chunk );
            continue;

        case pileup_state_populate:

            ON_FAIL ( CSRA1_PileupPopulate ( self, ctx, READ_AHEAD_LIMIT ) )
            {
                self -> state = pileup_state_err;
                break;
            }

            self -> state = pileup_state_position;
            continue;

        case pileup_state_initial:

            ON_FAIL ( CSRA1_PileupFirst ( self, ctx ) )
            {
                self -> state = pileup_state_err;
                break;
            }
            continue;

        case pileup_state_initial_position:

            ON_FAIL ( pos_valid = CSRA1_PileupPosition ( self, ctx ) )
            {
                self -> state = pileup_state_err;
                break;
            }

            if ( ! pos_valid )
            {
                assert ( self -> state != pileup_state_position );
                continue;
            }

            self -> state = pileup_state_position;
            break;

        case pileup_state_initial_chunk:

            ON_FAIL ( CSRA1_PileupChunk ( self, ctx ) )
            {
                self -> state = pileup_state_err;
                break;
            }

            self -> state = pileup_state_initial_position;
            continue;

        case pileup_state_initial_populate:

            ON_FAIL ( CSRA1_PileupPopulate ( self, ctx, READ_AHEAD_LIMIT ) )
            {
                self -> state = pileup_state_err;
                break;
            }

            self -> state = pileup_state_initial_chunk;
            continue;

        case pileup_state_finished:

            /* nothing to do */
            break;

        default:

            /* generate exception */
            CSRA1_PileupStateTest ( self, ctx, __LINE__ );
        }

        break;
    }

    /* partially reset the event iterator */
    if ( ! FAILED () )
    {
        self -> dad . entry = ( CSRA1_Pileup_Entry * ) DLListHead ( & self -> align . pileup );
        self -> dad . seen_first = false;
    }

    return pos_valid;
}

static NGS_Pileup_vt CSRA1_Pileup_vt =
{
    {
        {
            /* NGS_Refcount */
            CSRA1_PileupWhack
        },

        /* NGS_PileupEvent */
        CSRA1_PileupEventGetMappingQuality,
        CSRA1_PileupEventGetAlignmentId,
        CSRA1_PileupEventGetAlignmentPosition,
        CSRA1_PileupEventGetFirstAlignmentPosition,
        CSRA1_PileupEventGetLastAlignmentPosition,
        CSRA1_PileupEventGetEventType,
        CSRA1_PileupEventGetAlignmentBase,
        CSRA1_PileupEventGetAlignmentQuality,
        CSRA1_PileupEventGetInsertionBases,
        CSRA1_PileupEventGetInsertionQualities,
        CSRA1_PileupEventGetRepeatCount,
        CSRA1_PileupEventGetIndelType,
        CSRA1_PileupEventIteratorNext,
        CSRA1_PileupEventIteratorReset
    },

    CSRA1_PileupGetReferenceSpec,
    CSRA1_PileupGetReferencePosition,
    CSRA1_PileupGetReferenceBase,
    CSRA1_PileupGetDepth,
    CSRA1_PileupIteratorGetNext
};


static
void CSRA1_PileupPopulateAlignCurs ( ctx_t ctx, const VCursor * curs, uint32_t * col_idx, const char * tblname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    /* use preprocessor symbol that will disable assert() macro */
#if defined NDEBUG
#define COL_STRUCT                                                      \
    struct { const char * spec; bool opt; }
#define COL_SPEC1( cast, name )                                         \
    { cast stringize ( name ), false }
#define COL_SPEC2( cast, name )                                         \
    { cast stringize ( name ), true }
#define COL_SPEC3( cast, name )                                         \
    { cast stringize ( name ), false }
#else
    /* assert() macro will evaluate expression */
#define COL_STRUCT                                                      \
    struct { const char * spec; size_t idx; bool opt; }
#define COL_SPEC1( cast, name )                                         \
    { cast stringize ( name ), pileup_event_col_ ## name, false }
#define COL_SPEC2( cast, name )                                         \
    { cast stringize ( name ), pileup_event_col_ ## name, true }
#define COL_SPEC3( cast, name )                                         \
    { cast stringize ( name ), pileup_align_col_ ## name, false }
#endif

    static COL_STRUCT cols [] =
    {
        /* pileup-event */
        COL_SPEC1 ( "", MAPQ ),
        COL_SPEC1 ( "", REF_OFFSET ),
        COL_SPEC1 ( "(bool)", HAS_REF_OFFSET ),
        COL_SPEC1 ( "", MISMATCH ),
        COL_SPEC1 ( "(bool)", HAS_MISMATCH ),
        COL_SPEC1 ( "", REF_ORIENTATION ),
        COL_SPEC1 ( "", QUALITY ),

        /* optional pileup-event */
        COL_SPEC2 ( "", REF_OFFSET_TYPE ),

        /* pileup only */
        COL_SPEC3 ( "(INSDC:coord:zero)", REF_POS ),
        COL_SPEC3 ( "(INSDC:coord:len)", REF_LEN ),
        COL_SPEC3 ( "(INSDC:SRA:read_filter)", READ_FILTER )
    };

    rc_t rc;
    size_t i;

    for ( i = 0; i < sizeof cols / sizeof cols [ 0 ]; ++ i )
    {
        assert ( i == cols [ i ] . idx );

        if ( ! cols [ i ] . opt )
        {
            rc = VCursorAddColumn ( curs, & col_idx [ i ], "%s", cols [ i ] . spec );
            if ( rc != 0 )
            {
                INTERNAL_ERROR ( xcColumnNotFound, "VCursorAddColumn '%s' rc = %R", cols [ i ] . spec, rc );
                return;
            }
            assert ( col_idx [ i ] != 0 );
        }
    }

    rc = VCursorPermitPostOpenAdd ( curs );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcCursorOpenFailed,
                         "ERROR: VCursorPermitPostOpenAdd(%s) failed with error: 0x%08x (%u) [%R]",
                         tblname, rc, rc, rc );
        return;
    }

    rc = VCursorOpen ( curs );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcCursorOpenFailed,
                         "ERROR: VCursorOpen(%s) failed with error: 0x%08x (%u) [%R]",
                         tblname, rc, rc, rc );
        return;
    }

    for ( i = 0; i < sizeof cols / sizeof cols [ 0 ]; ++ i )
    {
        assert ( i == cols [ i ] . idx );

        if ( cols [ i ] . opt )
            VCursorAddColumn ( curs, & col_idx [ i ], "%s", cols [ i ] . spec );
    }
}

static
void CSRA1_PileupPopulatePACurs ( CSRA1_Pileup * obj, ctx_t ctx, const char * tblname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    const void * base;
    uint32_t elem_bits, boff, row_len;

    /* need to ensure PRIMARY_ALIGNMENT_IDS are in ref_curs */
    TRY ( NGS_CursorCellDataDirect ( obj -> ref . curs, ctx, obj -> reference_start_id,
              reference_PRIMARY_ALIGNMENT_IDS, & elem_bits, & base, & boff, & row_len ) )
    {
        /* populate cursor with known stuff */
        CSRA1_PileupPopulateAlignCurs ( ctx, obj -> pa . curs, obj -> pa . col_idx, tblname );
    }
}

static
void CSRA1_PileupPopulateSACurs ( CSRA1_Pileup * obj, ctx_t ctx, const char * tblname )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    const void * base;
    uint32_t elem_bits, boff, row_len;

    /* need to ensure SECONDARY_ALIGNMENT_IDS are in ref_curs */
    TRY ( NGS_CursorCellDataDirect ( obj -> ref . curs, ctx, obj -> reference_start_id,
              reference_SECONDARY_ALIGNMENT_IDS, & elem_bits, & base, & boff, & row_len ) )
    {
        /* populate cursor with known stuff */
        CSRA1_PileupPopulateAlignCurs ( ctx, obj -> sa . curs, obj -> sa . col_idx, tblname );
    }
}

static
void CSRA1_PileupInitAlignment ( CSRA1_Pileup * obj, ctx_t ctx,
    const VDatabase * db, const char * tblname, const VCursor ** curs,
    void ( * init_curs ) ( CSRA1_Pileup * obj, ctx_t ctx, const char * tblname ) )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    const VTable * tbl;
    rc_t rc = VDatabaseOpenTableRead ( db, & tbl, "%s", tblname );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcTableOpenFailed,
                         "ERROR: VDatabaseOpenTableRead(%s) failed with error: 0x%08x (%u) [%R]",
                         tblname, rc, rc, rc );
    }
    else
    {
        rc = VTableCreateCursorRead ( tbl, curs );
        if ( rc != 0 )
        {
            INTERNAL_ERROR ( xcCursorCreateFailed,
                             "ERROR: VTableCreateCursorRead(%s) failed with error: 0x%08x (%u) [%R]",
                             tblname, rc, rc, rc);
        }
        else
        {
            ( * init_curs ) ( obj, ctx, tblname );
        }

        VTableRelease ( tbl );
    }
}


static
void CSRA1_PileupInit ( ctx_t ctx, CSRA1_Pileup * obj, const char * instname,
    NGS_Reference * ref, const VDatabase * db, const NGS_Cursor * ref_curs,
    int64_t first_row_id, int64_t last_row_id, bool wants_primary, bool wants_secondary,
    uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    assert ( obj != NULL );
    assert ( ref != NULL );

    /* initialize superclass */
    TRY ( CSRA1_PileupEventInit ( ctx, & obj -> dad, & CSRA1_Pileup_vt, "CSRA1_Pileup", instname, ref ) )
    {
        /* capture reference cursor */
        TRY ( CSRA1_Pileup_RefCursorDataInit ( ctx, & obj -> ref, ref_curs, first_row_id ) )
        {
            TRY ( obj -> slice_xend = ( int64_t ) NGS_ReferenceGetLength ( ref, ctx ) )
            {
                /* determine whether the reference is circular */
                TRY ( obj -> circular = NGS_ReferenceGetIsCircular ( ref, ctx ) )
                {
                    /* capture row range - these are the rows within REFERENCE table
                       that represent the actual chromosome being analyzed */
                    obj -> reference_start_id = obj -> slice_start_id = first_row_id;
                    obj -> reference_last_id = obj -> slice_end_id = last_row_id;

                    /* record filter criteria */
                    obj -> filters = filters;
                    obj -> map_qual = map_qual;

                    /* set cache limits */
                    obj -> cached_blob_limit = CACHED_BLOB_LIMIT;
#if ! IGNORE_SYSTEM_RLIMIT
#if UNIX
                    {
                        struct rlimit rlim;
                        int status = getrlimit ( RLIMIT_AS, & rlim );
                        if ( status == 0 )
                            obj -> cached_blob_limit = rlim . rlim_cur >> 1;
                    }
#endif
#endif
                    /* initialize against one or more alignment tables */
                    if ( wants_primary )
                        CSRA1_PileupInitAlignment ( obj, ctx, db, "PRIMARY_ALIGNMENT", & obj -> pa . curs, CSRA1_PileupPopulatePACurs );
                    if ( wants_secondary && ! FAILED () )
                    {
                        ON_FAIL ( CSRA1_PileupInitAlignment ( obj, ctx, db, "SECONDARY_ALIGNMENT", & obj -> sa . curs, CSRA1_PileupPopulateSACurs ) )
                        {
                            /* TBD - need the ability to convert hard error to warning
                               stating that we have primary, but no secondary */
                            if ( wants_primary )
                                CLEAR ();
                        }
                    }
                }
            }
        }
    }
}

NGS_Pileup * CSRA1_PileupIteratorMake ( ctx_t ctx,
    NGS_Reference * ref, const VDatabase * db, const NGS_Cursor * curs_ref,
    int64_t first_row_id, int64_t last_row_id, bool wants_primary, bool wants_secondary,
    uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );

    CSRA1_Pileup * obj;

    assert ( db != NULL );
    assert ( curs_ref != NULL );
    assert ( wants_primary || wants_secondary );

    obj = calloc ( 1, sizeof * obj );
    if ( obj == NULL )
    {
        TRY ( NGS_String * ref_spec = NGS_ReferenceGetCommonName ( ref, ctx ) )
        {
            SYSTEM_ERROR ( xcNoMemory,
                           "allocating CSRA1_Pileup on '%.*s'",
                           NGS_StringSize ( ref_spec, ctx ),
                           NGS_StringData ( ref_spec, ctx ) );
            NGS_StringRelease ( ref_spec, ctx );
        }
        CATCH_ALL ()
        {
            CLEAR ();
            SYSTEM_ERROR ( xcNoMemory, "allocating CSRA1_Pileup" );
        }
    }
    else
    {
#if TRACK_REFERENCES
        char instname [ 256 ];
        TRY ( NGS_String * ref_spec = NGS_ReferenceGetCommonName ( ref, ctx ) )
        {
            string_printf ( instname,
                            sizeof instname,
                            NULL,
                            "%.*s",
                            NGS_StringSize ( ref_spec, ctx ),
                            NGS_StringData ( ref_spec, ctx )
                );
            NGS_StringRelease ( ref_spec, ctx );
            instname [ sizeof instname - 1 ] = 0;
        }
        CATCH_ALL ()
        {
            CLEAR ();
            string_copy_measure ( instname, sizeof instname, "unknown" );
        }
#else
        const char * instname = "unknown";
#endif
        TRY ( CSRA1_PileupInit ( ctx, obj, instname, ref, db, curs_ref, first_row_id, last_row_id, wants_primary, wants_secondary, filters, map_qual ) )
        {
            obj -> state = pileup_state_initial;
            return & obj -> dad . dad;
        }

        CSRA1_PileupWhack ( obj, ctx );
        free ( obj );
    }

    return NULL;
}

NGS_Pileup * CSRA1_PileupIteratorMakeSlice ( ctx_t ctx,
    NGS_Reference * ref, const VDatabase * db, const NGS_Cursor * curs_ref,
    int64_t first_row_id, int64_t last_row_id, uint64_t slice_zstart,
    uint64_t slice_size, bool wants_primary, bool wants_secondary,
    uint32_t filters, int32_t map_qual )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );
    uint64_t ref_len;

    assert ( ref != NULL );

    TRY ( ref_len = NGS_ReferenceGetLength ( ref, ctx ) )
    {
        if ( slice_zstart >= ref_len )
        {
            TRY ( NGS_String * ref_spec = NGS_ReferenceGetCommonName ( ref, ctx ) )
            {
                USER_ERROR ( xcParamOutOfBounds,
                             "slice start %lu, reference length %lu, "
                             "allocating CSRA1_Pileup on '%.*s'",
                             slice_zstart,
                             ref_len,
                             NGS_StringSize ( ref_spec, ctx ),
                             NGS_StringData ( ref_spec, ctx ) );
                NGS_StringRelease ( ref_spec, ctx );
            }
            CATCH_ALL ()
            {
                CLEAR ();
                USER_ERROR ( xcParamOutOfBounds, "slice start %lu, reference length %lu, "
                             "allocating CSRA1_Pileup", slice_zstart, ref_len );
            }
        }
        else
        {
            TRY ( NGS_Pileup * obj = CSRA1_PileupIteratorMake ( ctx, ref, db, curs_ref,
                first_row_id, last_row_id, wants_primary, wants_secondary, filters, map_qual ) )
            {
                CSRA1_Pileup * self = ( CSRA1_Pileup * ) obj;

                /* limit slice length */
                if ( self -> circular )
                {
                    /* limit to ref_len */
                    if ( slice_size > ref_len )
                        slice_size = ref_len;
                }
                else
                {
                    /* limit to end of reference */
                    if ( slice_zstart + slice_size > ref_len )
                        slice_size = ref_len - slice_zstart;
                }

                /* add slice boundaries */
                self -> ref_zpos        = slice_zstart;
                self -> slice_zstart    = slice_zstart;
                self -> slice_xend      = slice_zstart + slice_size;

                /* update slice row-ids */
                assert ( self -> ref . max_seq_len != 0 );
                self -> slice_start_id  = slice_zstart / self -> ref . max_seq_len + self -> reference_start_id;
                self -> slice_end_id    = ( self -> slice_xend - 1 ) / self -> ref . max_seq_len + self -> reference_start_id;

                return obj;
            }
        }
    }

    return NULL;
}

/* GetEntry
 */
const void * CSRA1_PileupGetEntry ( CSRA1_Pileup * self, ctx_t ctx,
    CSRA1_Pileup_Entry * entry, uint32_t col_idx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    rc_t rc;
    CSRA1_Pileup_AlignCursorData * cd = entry -> secondary ? & self -> sa : & self -> pa;

    assert ( entry -> blob [ col_idx ] == NULL );

    ON_FAIL ( CSRA1_Pileup_AlignCursorDataGetCell ( cd, ctx, entry -> row_id, col_idx ) )
        return NULL;

    /* if the entry is not already marked as having temporary data */
    if ( ! entry -> temporary )
    {
        /* get the size of the newly loaded blob */
        size_t blob_size;
        const VBlob * blob = cd -> blob [ col_idx ];
        rc = VBlobSize ( blob, & blob_size );

        /* a failure would generally represent an internal error */
        if ( rc != 0 )
        {
PRINT ( ">>> failed to determine blob size: rc = %u\n", rc );
            entry -> temporary = true;
        }

        /* test if caching this blob would exceed limit, and mark temporary if so */
        else if ( self -> cached_blob_total + blob_size > self -> cached_blob_limit )
        {
PRINT ( ">>> marking blob caching as temporary due to limits: %lu in cache, %lu in blob, limit %lu.\n"
        , ( unsigned long ) self -> cached_blob_total
        , ( unsigned long ) blob_size
        , ( unsigned long ) self -> cached_blob_limit
    );

            entry -> temporary = true;
        }
        else
        {
            /* cache the blob on entry */
            rc = VBlobAddRef ( blob );
            if ( rc != 0 )
            {
#if 1
                /* having the ability to NOT cache, eat error */
                entry -> temporary = true;
#else
                INTERNAL_ERROR ( xcRefcountOutOfBounds, "VBlob at %#p", cd -> blob [ col_idx ] );
                return NULL;
#endif
            }
            else
            {
                /* record the blob reference on entry */
                entry -> blob [ col_idx ] = cd -> blob [ col_idx ];

                /* accounting */
                entry -> blob_total += blob_size;
                self -> cached_blob_total += blob_size;
            }
        }
    }

    /* in all cases, record the cell data */
    entry -> cell_len [ col_idx ] = cd -> cell_len [ col_idx ];
    return entry -> cell_data [ col_idx ] = cd -> cell_data [ col_idx ];
}
