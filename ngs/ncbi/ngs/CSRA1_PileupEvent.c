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

typedef struct CSRA1_PileupEvent CSRA1_PileupEvent;
#define NGS_PILEUPEVENT CSRA1_PileupEvent
#include "NGS_PileupEvent.h"

#include "CSRA1_PileupEvent.h"
#include "CSRA1_Pileup.h"
#include "NGS_Pileup.h"
#include "NGS_Reference.h"
#include "NGS_ReadCollection.h"
#include "NGS_Id.h"
#include "NGS_String.h"
#include "NGS_Cursor.h"

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <kfc/xc.h>
#include <align/align.h>

#include <klib/printf.h>

#include "NGS_String.h"
#include "NGS_Pileup.h"

#include <sysalloc.h>
#include <string.h> /* memset */

#define CSRA1_PileupEventGetPileup( self ) \
    ( ( CSRA1_Pileup * ) ( self ) )

static
void CSRA1_PileupEventStateTest ( const CSRA1_PileupEvent * self, ctx_t ctx, uint32_t lineno )
{
    assert ( self != NULL );

    if ( ! self -> seen_first )
    {
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_user, xcIteratorUninitialized,
                    "PileupEvent accessed before a call to PileupEventIteratorNext()" );
    }
    else if ( self -> entry == NULL )
    {
        ctx_event ( ctx, lineno, xc_sev_fail, xc_org_user, xcCursorExhausted, "No more rows available" );
    }
}

#define CHECK_STATE( self, ctx ) \
    CSRA1_PileupEventStateTest ( self, ctx, __LINE__ )


void CSRA1_PileupEventWhack ( CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcDestroying );
    NGS_PileupWhack ( & self -> dad, ctx );
}

static
const void * CSRA1_PileupEventGetEntry ( const CSRA1_PileupEvent * self, ctx_t ctx,
    CSRA1_Pileup_Entry * entry, uint32_t col_idx )
{
    if ( entry -> cell_data [ col_idx ] == NULL )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
        return CSRA1_PileupGetEntry ( CSRA1_PileupEventGetPileup ( self ), ctx, entry, col_idx );
    }

    return entry -> cell_data [ col_idx ];
}

static
const void * CSRA1_PileupEventGetNonEmptyEntry ( const CSRA1_PileupEvent * self, ctx_t ctx,
    CSRA1_Pileup_Entry * entry, uint32_t col_idx )
{
    if ( entry -> cell_len [ col_idx ] == 0 )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

        if ( entry -> cell_data [ col_idx ] == NULL )
            CSRA1_PileupGetEntry ( CSRA1_PileupEventGetPileup ( self ), ctx, entry, col_idx );
        
        if ( entry -> cell_len [ col_idx ] == 0 )
        {
            INTERNAL_ERROR ( xcColumnEmpty, "zero-length cell data (row_id = %ld, col_idx = %u)", entry->row_id, col_idx );
            return NULL;
        }
    }
    return entry -> cell_data [ col_idx ];
}

int CSRA1_PileupEventGetMappingQuality ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        const int32_t * MAPQ;
        TRY ( MAPQ = CSRA1_PileupEventGetNonEmptyEntry ( self, ctx, self -> entry, pileup_event_col_MAPQ ) )
        {
            return MAPQ [ 0 ];
        }
    }
    return 0;
}

struct NGS_String * CSRA1_PileupEventGetAlignmentId ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        NGS_ReadCollection * coll = self -> dad . dad . ref -> coll;
        TRY ( const NGS_String * run = NGS_ReadCollectionGetName ( coll, ctx ) )
        {
            enum NGS_Object obj_type = self -> entry -> secondary ?
                NGSObject_SecondaryAlignment : NGSObject_PrimaryAlignment;
            return NGS_IdMake ( ctx, run, obj_type, self -> entry -> row_id );
        }
    }
    return NULL;
}

int64_t CSRA1_PileupEventGetAlignmentPosition ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        return self -> entry -> state_curr . seq_idx;
    }

    return 0;
}

int64_t CSRA1_PileupEventGetFirstAlignmentPosition ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        return self -> entry -> zstart;
    }

    return 0;
}

int64_t CSRA1_PileupEventGetLastAlignmentPosition ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        return self -> entry -> xend - 1;
    }
    return 0;
}

int CSRA1_PileupEventGetEventType ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    int event_type = 0;

    TRY ( CHECK_STATE ( self, ctx ) )
    {
        const bool * REF_ORIENTATION;

        CSRA1_Pileup_Entry * entry = self -> entry;

        /*
          during "next" we took these steps:
          1. if within a deletion, decrement deletion repeat && exit if ! 0
          2. check HAS_REF_OFFSET. if not false:
             a. a positive REF_OFFSET[ref_offset_idx] indicates a deletion
             b. a negative REF_OFFSET[ref_offset_idx] indicates an insertion
          3. move current offset ahead until ref_pos >= that of pileup
          
          so here, we first detect a deletion event
          next, we detect a match or mismatch by checking HAS_MISMATCH.
          if there was a prior insertion, we or that onto the event.
          if this event starts a new alignment, or start onto event.
          if it ends an alignment, or that onto the event.
        */

        if ( entry -> state_curr . del_cnt != 0 )
            event_type = NGS_PileupEventType_deletion;
        else
        {
            const bool * HAS_MISMATCH = entry -> cell_data [ pileup_event_col_HAS_MISMATCH ];
            assert ( HAS_MISMATCH != NULL );
            assert ( entry -> state_curr . seq_idx < entry -> cell_len [ pileup_event_col_HAS_MISMATCH ] );
            event_type = HAS_MISMATCH [ entry -> state_curr . seq_idx ];
        }

        /* detect prior insertion */
        if ( entry -> state_curr . ins_cnt != 0 )
            event_type |= NGS_PileupEventType_insertion;

        /* detect initial event */
        if ( CSRA1_PileupEventGetPileup ( self ) -> ref_zpos == entry -> zstart )
            event_type |= NGS_PileupEventType_start;

        /* detect final event */
        if ( CSRA1_PileupEventGetPileup ( self ) -> ref_zpos + 1 == entry -> xend ||
            entry -> status == pileup_entry_status_DONE)
        {
            event_type |= NGS_PileupEventType_stop;
        }

        /* detect minus strand */
        TRY ( REF_ORIENTATION = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_REF_ORIENTATION ) )
        {
            assert ( REF_ORIENTATION != NULL );
            assert ( entry -> cell_len [ pileup_event_col_REF_ORIENTATION ] == 1 );
            if ( REF_ORIENTATION [ 0 ] )
                event_type |= NGS_PileupEventType_minus_strand;
        }

    }
    
    return event_type;
}

char CSRA1_PileupEventGetAlignmentBase ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        CSRA1_Pileup * pileup;
        CSRA1_Pileup_Entry * entry = self -> entry;
        const bool * HAS_MISMATCH = entry -> cell_data [ pileup_event_col_HAS_MISMATCH ];

        if ( entry -> state_curr . del_cnt != 0 )
            return '-';

        assert ( HAS_MISMATCH != NULL );
        assert ( entry -> state_curr . seq_idx < entry -> cell_len [ pileup_event_col_HAS_MISMATCH ] );

        if ( HAS_MISMATCH [ entry -> state_curr . seq_idx ] )
        {
            if ( entry -> state_curr . mismatch == 0 )
            {
                const INSDC_dna_text * MISMATCH;
                TRY ( MISMATCH = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_MISMATCH ) )
                {
                    if ( entry -> state_curr . mismatch_idx < entry -> cell_len [ pileup_event_col_MISMATCH ] )
                        entry -> state_curr . mismatch = MISMATCH [ entry -> state_curr . mismatch_idx ];
                }
            }

            return entry -> state_curr . mismatch;
        }

        pileup = CSRA1_PileupEventGetPileup ( self );
        if ( pileup -> ref_base == 0 )
        {
            if ( pileup -> ref_chunk_bases == NULL )
            {
                const void * base;
                uint32_t elem_bits, boff, row_len;
                ON_FAIL ( NGS_CursorCellDataDirect ( pileup -> ref . curs, ctx, pileup -> ref_chunk_id,
                    reference_READ, & elem_bits, & base, & boff, & row_len ) )
                {
                    return 0;
                }

                pileup -> ref_chunk_bases = base;
                assert ( row_len == pileup -> ref . max_seq_len ||
                         pileup -> ref_chunk_xend - pileup -> ref . max_seq_len + row_len >= pileup -> slice_xend );
            }

            assert ( pileup -> ref . max_seq_len != 0 );
            pileup -> ref_base = pileup -> ref_chunk_bases [ CSRA1_PileupEventGetPileup ( self ) -> ref_zpos % pileup -> ref . max_seq_len ]; 
        }

        return pileup -> ref_base;
    }
    return 0;
}

char CSRA1_PileupEventGetAlignmentQuality ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        const INSDC_quality_phred * QUALITY;

        CSRA1_Pileup_Entry * entry = self -> entry;

        if ( entry -> state_curr . del_cnt != 0 )
            return '!';
        
        TRY ( QUALITY = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_QUALITY ) )
        {
            assert ( QUALITY != NULL );
            assert ( entry -> state_curr . seq_idx < entry -> cell_len [ pileup_event_col_QUALITY ] );
            return ( char ) ( QUALITY [ entry -> state_curr . seq_idx ] + 33 );
        }
    }
    return 0;
}

struct NGS_String * CSRA1_PileupEventGetInsertionBases ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        CSRA1_Pileup_Entry * entry = self -> entry;

        /* handle the case where there is no insertion */
        if ( entry -> state_curr . ins_cnt == 0 )
        {
            return NGS_StringMake ( ctx, "", 0 );
        }
        else
        {
            /* allocate a buffer for the NGS_String */
            char * buffer = calloc ( 1, entry -> state_curr . ins_cnt + 1 );
            if ( buffer == NULL )
                SYSTEM_ERROR ( xcNoMemory, "allocating %zu bytes", entry -> state_curr . ins_cnt + 1 );
            else
            {
                const INSDC_dna_text * MISMATCH;
                const bool * HAS_MISMATCH = entry -> cell_data [ pileup_event_col_HAS_MISMATCH ];
                assert ( HAS_MISMATCH != NULL );

                /* it is "possible" but not likely that we may not need the MISMATCH cell.
                   this would be the case if there was an insertion that exactly repeated
                   a region of the reference, such that there were no mismatches in it.
                   but even so, it should not be a problem to prefetch MISMATCH */
                TRY ( MISMATCH = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_MISMATCH ) )
                {
                    uint32_t ref_first = entry -> state_curr . seq_idx;
                    uint32_t mismatch_idx = entry -> state_curr . mismatch_idx;
                    uint32_t seq_idx, ins_start = entry -> state_curr . seq_idx - entry -> state_curr . ins_cnt;

                    assert ( MISMATCH != 0 );

                    /* seq_idx MUST be > ins_cnt, which is non-zero, ...
                       for seq_idx to be == ins_cnt implies that the sequence
                       starts with an insertion, otherwise considered a soft-clip,
                       and not an insertion at all.
                     */
                    assert ( entry -> state_curr . seq_idx > entry -> state_curr . ins_cnt );

                    /* fill in the buffer with each entry from mismatch */
                    for ( seq_idx = entry -> state_curr . seq_idx - 1; seq_idx >= ins_start; -- seq_idx )
                    {
                        /* pull base from MISMATCH */
                        if ( HAS_MISMATCH [ seq_idx ] )
                            buffer [ seq_idx - ins_start ] = MISMATCH [ -- mismatch_idx ];

                        /* will need to get base from reference */
                        else
                            ref_first = seq_idx;
                    }

                    /* if there are some to be filled from reference */
                    if ( entry -> state_curr . mismatch_idx - mismatch_idx != entry -> state_curr . ins_cnt )
                    {
                        CSRA1_Pileup * pileup = CSRA1_PileupEventGetPileup ( self );

                        /* a little more complex than we'd like here...
                           chances are quite good that the matched portion of the reference
                           is in our current chunk, but it's not guaranteed,
                           nor is it guaranteed to be in a single chunk. */

                        /* the number of characters in string that could come from reference */
                        uint32_t str_len = entry -> state_curr . seq_idx - ref_first;

                        /* offset the string buffer to the first base to be filled by reference */
                        char * rbuffer = & buffer [ entry -> state_curr . ins_cnt - str_len ];

                        /* the current chunk of reference bases or NULL */
                        const INSDC_dna_text * READ = pileup -> ref_chunk_bases;

                        /* generate range of reference positions */
                        int64_t ins_ref_zstart = CSRA1_PileupEventGetPileup ( self ) -> ref_zpos - ( int64_t ) str_len;
                        int64_t ins_ref_last = CSRA1_PileupEventGetPileup ( self ) -> ref_zpos - 1;

                        /* generate range of reference chunk ids */
                        int64_t ins_ref_start_id = ins_ref_zstart / pileup -> ref . max_seq_len + pileup -> reference_start_id;
                        int64_t ins_ref_last_id = ins_ref_last / pileup -> ref . max_seq_len + pileup -> reference_start_id;

                        /* the starting offset into the left-most reference chunk */
                        uint32_t ref_off = ( uint32_t ) ( ins_ref_zstart % pileup -> ref . max_seq_len );

                        /* check for error in the starting position: must be >= 0 */
                        if ( ins_ref_zstart < 0 )
                        {
                            INTERNAL_ERROR ( xcParamOutOfBounds, "insertion string accessing reference at position %ld", ins_ref_zstart );
                            free ( buffer );
                            return NULL;
                        }

                        /* try to take advantage of the chunk that's loaded right now */
                        if ( READ != NULL && pileup -> ref_chunk_id == ins_ref_last_id )
                        {
                            /* most common case - everything within this chunk */
                            if ( ins_ref_start_id == ins_ref_last_id )
                            {
                                /* "seq_off" is implied 0, i.e. start of insertion sequence.
                                   "ref_off" is calculated start of insert in reference coords modulo chunk size */
                                for ( seq_idx = 0; seq_idx < str_len; ++ seq_idx )
                                {
                                    if ( rbuffer [ seq_idx ] == 0 )
                                        rbuffer [ seq_idx ] = READ [ ref_off + seq_idx ];
                                }
                                goto buffer_complete;
                            }
                            /* less common case - share only part of this chunk */
                            else
                            {
                                /* "ref_off" is implied 0, i.e. start of reference chunk which is
                                   known to be the last but not first chunk, therefore the start is
                                   at 0 where the insertion crosses chunk boundaries.
                                   "seq_off" is the start of the last portion of the string to
                                   intersect this reference chunk. */
                                uint32_t seq_off = str_len - ( uint32_t ) ( CSRA1_PileupEventGetPileup ( self ) -> ref_zpos % pileup -> ref . max_seq_len );
                                for ( seq_idx = seq_off; seq_idx < str_len; ++ seq_idx )
                                {
                                    if ( rbuffer [ seq_idx ] == 0 )
                                        rbuffer [ seq_idx ] = READ [ seq_idx - seq_off ];
                                }

                                /* the last part of the string has been completed */
                                str_len = seq_off;
                                -- ins_ref_last_id;
                            }
                        }

                        /* forget current reference chunk */
                        pileup -> ref_chunk_bases = NULL;
                        pileup -> ref_base = 0;

                        /* set the reference offset of initial chunk */
                        ref_off = ( uint32_t ) ( ins_ref_zstart % pileup -> ref . max_seq_len );

                        /* walk from ins_ref_start_id to ins_ref_last_id */
                        for ( seq_idx = 0; ins_ref_start_id <= ins_ref_last_id; ++ ins_ref_start_id, ref_off = 0 )
                        {
                            const void * base;
                            uint32_t limit, seq_off, row_len;
                            ON_FAIL ( NGS_CursorCellDataDirect ( pileup -> ref . curs, ctx, ins_ref_start_id,
                                reference_READ, & limit, & base, & seq_off, & row_len ) )
                            {
                                READ = NULL;
                                break;
                            }

                            READ = base;

                            /* the total number of bases left in insertion string */
                            limit = str_len - seq_idx;

                            /* cannot exceed the bases available in this chunk */
                            if ( ref_off + limit > row_len )
                                limit = row_len - ref_off;

                            /* end index within string */
                            limit += seq_idx;

                            for ( seq_off = seq_idx; seq_idx < limit; ++ seq_idx )
                            {
                                if ( rbuffer [ seq_idx ] == 0 )
                                    rbuffer [ seq_idx ] = READ [ ref_off + seq_idx - seq_off ];
                            }

                            /* we stopped either due to:
                               1. end of string, or
                               2. end of chunk - in which case
                                  a. must not be an end chunk, i.e. has row_len == MAX_SEQ_LEN, and
                                  b. we must loop again
                            */
                            assert ( seq_idx == str_len || ( row_len == pileup -> ref . max_seq_len && ins_ref_start_id < ins_ref_last_id ) );
                        }

                        /* finally, if at this point we have cached the READ for reference
                           and within our current chunk, save it on the pileup */
                        if ( ins_ref_last_id == pileup -> ref_chunk_id )
                            pileup -> ref_chunk_bases = READ;
                    }

                    if ( ! FAILED () )
                    {
                        NGS_String * bases;

                    buffer_complete:

                        TRY ( bases = NGS_StringMakeOwned ( ctx, buffer, entry -> state_curr . ins_cnt ) )
                        {
                            return bases;
                        }
                    }
                }

                free ( buffer );
            }
        }
    }

    return NULL;
}

struct NGS_String * CSRA1_PileupEventGetInsertionQualities ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        CSRA1_Pileup_Entry * entry = self -> entry;

        /* handle the case where there is no insertion */
        if ( entry -> state_curr . ins_cnt == 0 )
        {
            return NGS_StringMake ( ctx, "", 0 );
        }
        else
        {
            /* allocate a buffer for the NGS_String */
            char * buffer = calloc ( 1, entry -> state_curr . ins_cnt + 1 );
            if ( buffer == NULL )
                SYSTEM_ERROR ( xcNoMemory, "allocating %zu bytes", entry -> state_curr . ins_cnt + 1 );
            else
            {
                const INSDC_quality_phred * QUALITY;
                TRY ( QUALITY = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_QUALITY ) )
                {
                    NGS_String * bases;
                    uint32_t i, qstart = entry -> state_curr . seq_idx - entry -> state_curr . ins_cnt;

                    assert ( QUALITY != NULL );
                    assert ( entry -> state_curr . seq_idx <= entry -> cell_len [ pileup_event_col_QUALITY ] );
                    assert ( entry -> state_curr . seq_idx >= entry -> state_curr . ins_cnt );

                    for ( i = 0; i < entry -> state_curr . ins_cnt; ++ i )
                        buffer [ i ] = ( char ) ( QUALITY [ qstart + i ] + 33 );

                    TRY ( bases = NGS_StringMakeOwned ( ctx, buffer, entry -> state_curr . ins_cnt ) )
                    {
                        return bases;
                    }
                }

                free ( buffer );
            }
        }
    }

    return NULL;
}

unsigned int CSRA1_PileupEventGetRepeatCount ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        bool event_type;
        uint32_t repeat, limit;
        const bool * HAS_MISMATCH, * HAS_REF_OFFSET;
        const CSRA1_Pileup_Entry * entry = self -> entry;

        /* handle the easy part first */
        if ( entry -> state_curr . del_cnt != 0 )
            return entry -> state_curr . del_cnt;

        /* now, count the number of repeated matches or mismatches,
           WITHOUT any intervening insertions or deletions */
        HAS_MISMATCH = entry -> cell_data [ pileup_event_col_HAS_MISMATCH ];
        HAS_REF_OFFSET = entry -> cell_data [ pileup_event_col_HAS_REF_OFFSET ];
        limit = entry -> xend - ( entry -> zstart + entry -> state_curr . zstart_adj );

        /* grab the type of event we have now */
        event_type = HAS_MISMATCH [ entry -> state_curr . seq_idx ];
        
        for ( repeat = 1; repeat < limit; ++ repeat )
        {
            if ( HAS_REF_OFFSET [ entry -> state_curr . seq_idx + repeat ] )
                break;
            if ( HAS_MISMATCH [ entry -> state_curr . seq_idx + repeat ] != event_type )
                break;
        }

        return repeat;
    }
    return 0;
}

int CSRA1_PileupEventGetIndelType ( const CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );
    TRY ( CHECK_STATE ( self, ctx ) )
    {
        CSRA1_Pileup_Entry * entry = self -> entry;

        if ( entry -> state_curr . del_cnt != 0 || entry -> state_curr . ins_cnt != 0 )
        {
            CSRA1_Pileup * pileup = CSRA1_PileupEventGetPileup ( self );
            CSRA1_Pileup_AlignCursorData * cd = entry -> secondary ? & pileup -> sa : & pileup -> pa;
            if ( ! cd -> missing_REF_OFFSET_TYPE )
            {
                const NCBI_align_ro_type * REF_OFFSET_TYPE;
                TRY ( REF_OFFSET_TYPE = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_REF_OFFSET_TYPE ) )
                {
                    assert ( REF_OFFSET_TYPE != NULL );
                    assert ( entry -> state_curr . ref_off_idx > 0 );
                    assert ( entry -> state_curr . ref_off_idx <= entry -> cell_len [ pileup_event_col_REF_OFFSET_TYPE ] );
                    switch ( REF_OFFSET_TYPE [ entry -> state_curr . ref_off_idx - 1 ] )
                    {
                    case NCBI_align_ro_normal:
                    case NCBI_align_ro_soft_clip:
                        break;
                    case NCBI_align_ro_intron_plus:
                        return NGS_PileupIndelType_intron_plus;
                    case NCBI_align_ro_intron_minus:
                        return NGS_PileupIndelType_intron_minus;
                    case NCBI_align_ro_intron_unknown:
                        return NGS_PileupIndelType_intron_unknown;
                    case NCBI_align_ro_complete_genomics:
                        if ( entry -> state_curr . ins_cnt != 0 )
                            return NGS_PileupIndelType_read_overlap;
                        assert ( entry -> state_curr . del_cnt != 0 );
                        return NGS_PileupIndelType_read_gap;
                    }
                }
                CATCH_ALL ()
                {
                    CLEAR ();
                    cd -> missing_REF_OFFSET_TYPE = true;
                }
            }
        }

        return NGS_PileupIndelType_normal;
    }
    return 0;
}

static
void CSRA1_PileupEventEntryFocus ( CSRA1_PileupEvent * self, CSRA1_Pileup_Entry * entry )
{
    const bool * HAS_MISMATCH = entry -> cell_data [ pileup_event_col_HAS_MISMATCH ];
    const bool * HAS_REF_OFFSET = entry -> cell_data [ pileup_event_col_HAS_REF_OFFSET ];
    const int32_t * REF_OFFSET = entry -> cell_data [ pileup_event_col_REF_OFFSET ];

    /* we need the entry to be fast-forwarded */
    int32_t ref_zpos_adj, plus_end_pos;
    uint32_t next_ins_cnt;

advance_to_the_next_position: /* TODO: try to reorganise the function not to have this goto */

    ref_zpos_adj = CSRA1_PileupEventGetPileup ( self ) -> ref_zpos - entry -> zstart;
    plus_end_pos = entry->status == pileup_entry_status_INITIAL ? 0 : 1;
    assert ( ref_zpos_adj >= 0 );

    /* always lose any insertion, forget cached values */
    /*entry -> state_next . ins_cnt = 0;*/
    entry -> state_next . mismatch = 0;
    next_ins_cnt = 0; /* this variable is needed not to erase
                         state_curr.ins_count on the first iteration
                         of the next while-loop */ /* TODO: advise with Kurt about the case with INITIAL - should it be reset? */

    /* must advance in all but initial case */
    assert ( ref_zpos_adj + plus_end_pos > entry -> state_next . zstart_adj || entry -> state_next . zstart_adj == 0 );

    /* walk forward */
    while ( ref_zpos_adj + plus_end_pos > entry -> state_next . zstart_adj )
    {
        entry -> state_curr = entry -> state_next;

        /* within a deletion */
        if ( entry -> state_next . del_cnt != 0 )
            -- entry -> state_next . del_cnt;

        else
        {
            uint32_t prior_seq_idx = entry -> state_next . seq_idx ++;

            /* adjust mismatch_idx */
            assert ( HAS_MISMATCH != NULL );
            assert ( prior_seq_idx < entry -> cell_len [ pileup_event_col_HAS_MISMATCH ] );
            entry -> state_next . mismatch_idx += HAS_MISMATCH [ prior_seq_idx ];

            /* if the current sequence address is beyond end, bail */
            if ( entry -> state_next . seq_idx >= entry -> cell_len [ pileup_event_col_HAS_REF_OFFSET ] )
            {
                entry -> status = pileup_entry_status_DONE;
                entry -> state_next . ins_cnt = next_ins_cnt;
                return;
            }

            /* retry point for merging events */
        merge_adjacent_indel_events:

            /* adjust alignment */
            if ( HAS_REF_OFFSET [ entry -> state_next . seq_idx ] )
            {
                assert ( REF_OFFSET != NULL );
                if ( REF_OFFSET [ entry -> state_next . ref_off_idx ] < 0 )
                {
                    /* insertion */
                    uint32_t i, ins_cnt = - REF_OFFSET [ entry -> state_next . ref_off_idx ];

                    /* clip to SEQUENCE length */
                    if ( ( uint32_t ) ( entry -> state_next . seq_idx + ins_cnt ) > entry -> cell_len [ pileup_event_col_HAS_REF_OFFSET ] )
                        ins_cnt = ( int32_t ) entry -> cell_len [ pileup_event_col_HAS_REF_OFFSET ] - entry -> state_next . seq_idx;

                    /* combine adjacent inserts */
                    /*entry -> state_next . ins_cnt += ins_cnt;*/
                    next_ins_cnt += ins_cnt;

                    /* scan over insertion to adjust mismatch index */
                    for ( i = 0; i < ins_cnt; ++ i )
                        entry -> state_next . mismatch_idx += HAS_MISMATCH [ entry -> state_next . seq_idx + i ];

                    entry -> state_next . seq_idx += ins_cnt;
                    if ( entry -> state_next . seq_idx >= entry -> cell_len [ pileup_event_col_HAS_REF_OFFSET ] )
                    {
                        entry -> status = pileup_entry_status_DONE;
                        entry -> state_next . ins_cnt = next_ins_cnt;
                        return;
                    }

                    /* NB - there may be entries in HAS_REF_OFFSET that are set
                       within the insertion. These are used to split the insertion
                       for preserving boundaries indicated by Complete Genomics BAM
                       cigar encoding. for Pileup, we treat the entire subsequence as a
                       single insertion.

                       The "true" values in HAS_REF_OFFSET within an insertion do NOT
                       represent a corresponding entry in REF_OFFSET, so they are ignored here.
                    */

                    /* detect the case of an insertion followed by a deletion */
                    if ( entry -> state_next . seq_idx < entry -> cell_len [ pileup_event_col_HAS_REF_OFFSET ] )
                    {
                        ++ entry -> state_next . ref_off_idx;
                        goto merge_adjacent_indel_events;
                    }
                }

                else
                {
                    /* deletion */
                    entry -> state_next . del_cnt += REF_OFFSET [ entry -> state_next . ref_off_idx ];

                    /* clip to PROJECTION length */
                    if ( ( int64_t ) entry -> state_next . del_cnt > entry -> xend - ( entry -> zstart + entry -> state_next . zstart_adj ) )
                        entry -> state_next . del_cnt = ( int32_t ) ( entry -> xend - ( entry -> zstart + entry -> state_next . zstart_adj ) );
                }

                ++ entry -> state_next . ref_off_idx;
            }
        }

        ++ entry -> state_next . zstart_adj;
        entry -> state_next . ins_cnt = next_ins_cnt;

    }

    if ( entry->status == pileup_entry_status_INITIAL )
    {
        entry->status = pileup_entry_status_VALID;
        goto advance_to_the_next_position;
    }
}

static
void CSRA1_PileupEventEntryInit ( CSRA1_PileupEvent * self, ctx_t ctx, CSRA1_Pileup_Entry * entry )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    const bool * HAS_MISMATCH;

    TRY ( HAS_MISMATCH = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_HAS_MISMATCH ) )
    {
        const int32_t * REF_OFFSET;

        TRY ( REF_OFFSET = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_REF_OFFSET ) )
        {
            const bool * HAS_REF_OFFSET;

            TRY ( HAS_REF_OFFSET = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_HAS_REF_OFFSET ) )
            {
                /* if there are no offsets, then there are no indels, which means
                   that there are only match and mismatch events */
                if ( entry -> cell_len [ pileup_event_col_REF_OFFSET ] == 0 )
                    return;

                /* check for left soft-clip */
                while ( HAS_REF_OFFSET [ entry -> state_next . seq_idx ] && REF_OFFSET [ entry -> state_next . ref_off_idx ] < 0 )
                {
                    uint32_t i, end = entry -> state_next . seq_idx - REF_OFFSET [ entry -> state_next . ref_off_idx ++ ];

                    /* safety check */
                    if ( end > entry -> cell_len [ pileup_event_col_HAS_REF_OFFSET ] )
                        end = entry -> cell_len [ pileup_event_col_HAS_REF_OFFSET ];

                    /* skip over soft-clip */
                    for ( i = entry -> state_next . seq_idx; i < end; ++ i )
                        entry -> state_next . mismatch_idx += HAS_MISMATCH [ i ];

                    entry -> state_next . seq_idx = end;
                }

                /* capture initial deletion - should never occur */
                if ( HAS_REF_OFFSET [ entry -> state_next . seq_idx ] && REF_OFFSET [ entry -> state_next . ref_off_idx ] > 0 )
                    entry -> state_next . del_cnt = REF_OFFSET [ entry -> state_next . ref_off_idx ];

                /* TODO: maybe pileup_entry_status_VALID must be set here */

                return;
            }
        }
    }

    self -> entry = NULL;
}

static
void CSRA1_PileupEventRefreshEntry ( CSRA1_PileupEvent * self, ctx_t ctx, CSRA1_Pileup_Entry * entry )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    const bool * HAS_MISMATCH;
    TRY ( HAS_MISMATCH = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_HAS_MISMATCH ) )
    {
        const int32_t * REF_OFFSET;
        TRY ( REF_OFFSET = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_REF_OFFSET ) )
        {
            const bool * HAS_REF_OFFSET;
            TRY ( HAS_REF_OFFSET = CSRA1_PileupEventGetEntry ( self, ctx, entry, pileup_event_col_HAS_REF_OFFSET ) )
            {
                assert ( HAS_MISMATCH != NULL );
                assert ( HAS_REF_OFFSET != NULL );
                assert ( REF_OFFSET != NULL );

                (void)HAS_MISMATCH;
                (void)HAS_REF_OFFSET;
                (void)REF_OFFSET;
            }
        }
    }
}


bool CSRA1_PileupEventIteratorNext ( CSRA1_PileupEvent * self, ctx_t ctx )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcAccessing );

    CSRA1_Pileup_Entry * entry;
#if _DEBUGGING
    CSRA1_Pileup * pileup = CSRA1_PileupEventGetPileup ( self );
    assert ( pileup != NULL );
#endif

    /* go to next entry */
    if ( ! self -> seen_first )
        self -> seen_first = true;
    else if ( self -> entry != NULL )
        self -> entry = ( CSRA1_Pileup_Entry * ) DLNodeNext ( & self -> entry -> node );

    /* detect end of pileup */
    entry = self -> entry;
    if ( self -> entry == NULL )
        return false;

    /* detect new entry */
    if ( ! entry -> seen )
    {
        ON_FAIL ( CSRA1_PileupEventEntryInit ( self, ctx, entry ) )
            return false;
        entry -> seen = true;
        assert ( self -> entry != NULL );
    }
    else if ( entry -> cell_data [ pileup_event_col_REF_OFFSET ] == NULL )
    {
        ON_FAIL ( CSRA1_PileupEventRefreshEntry ( self, ctx, entry ) )
            return false;
    }

    /* this is an entry we've seen before */
    CSRA1_PileupEventEntryFocus ( self, entry );

    return true;
}

void CSRA1_PileupEventIteratorReset ( CSRA1_PileupEvent * self, ctx_t ctx )
{
    CSRA1_Pileup_Entry * entry;

    CSRA1_Pileup * pileup = CSRA1_PileupEventGetPileup ( self );
    self -> entry = ( CSRA1_Pileup_Entry * ) DLListHead ( & pileup -> align . pileup );
    self -> seen_first = false;

    for ( entry = self -> entry; entry != NULL; entry = ( CSRA1_Pileup_Entry * ) DLNodeNext ( & entry -> node ) )
    {
        memset ( & entry-> state_curr, 0, sizeof (entry-> state_curr) );
        memset ( & entry-> state_next, 0, sizeof (entry-> state_next) );

        /*entry -> status = pileup_entry_status_INITIAL;*/ /* TODO: remove comment */
    }
}

void CSRA1_PileupEventInit ( ctx_t ctx, CSRA1_PileupEvent * obj, const NGS_Pileup_vt * vt,
    const char * clsname, const char * instname, NGS_Reference * ref )
{
    FUNC_ENTRY ( ctx, rcSRA, rcCursor, rcConstructing );
    
    assert ( obj != NULL );
    
    NGS_PileupInit ( ctx, & obj -> dad, vt, clsname, instname, ref );
}
