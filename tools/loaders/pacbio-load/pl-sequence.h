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
#ifndef _h_pl_sequence_
#define _h_pl_sequence_

#ifdef __cplusplus
extern "C" {
#endif

#include "pl-tools.h"
#include "pl-zmw.h"
#include "pl-basecalls_cmn.h"
#include "pl-regions.h"
#include <klib/rc.h>
#include <insdc/sra.h>

/* enumeration of the columns of the pulse-table */
enum
{
    /* base-space */
    seq_tab_READ = 0,
    seq_tab_QUALITY,
    seq_tab_NREADS,
    seq_tab_PLATFORM,

    seq_tab_LABEL,
    seq_tab_LABEL_START,
    seq_tab_LABEL_LEN,
    seq_tab_READ_TYPE,
    seq_tab_READ_START,
    seq_tab_READ_LEN,
    seq_tab_CLIP_QUALITY_LEFT,
    seq_tab_CLIP_QUALITY_RIGHT,
    seq_tab_READ_FILTER,

    /* pulse-space */
    seq_tab_PRE_BASE_FRAMES,
    seq_tab_WIDTH_IN_FRAMES,
    seq_tab_PULSE_INDEX_16,
    seq_tab_PULSE_INDEX_32,
    seq_tab_HOLE_NUMBER,
    seq_tab_HOLE_STATUS,
    seq_tab_HOLE_XY,
    seq_tab_INSERTION_QV,
    seq_tab_DELETION_QV,
    seq_tab_DELETION_TAG,
    seq_tab_SUBSTITUTION_QV,
    seq_tab_SUBSTITUTION_TAG,
    seq_tab_count
};


typedef struct BaseCalls
{
    BaseCalls_cmn cmn;
    regions rgn;
    af_data PreBaseFrames;
    af_data PulseIndex;
    af_data WidthInFrames;
} BaseCalls;


typedef struct seq_ctx
{
    VCursor * cursor;
    ld_context *lctx;
    BaseCalls BaseCallsTab;
    uint32_t col_idx[ seq_tab_count ];
    bool rgn_present;
    bool src_open;
} seq_ctx;


/* special case for SEQUENCE: in order to prepare the cursor correctly, we have to know the first HDF5-source-obj !*/
rc_t prepare_seq( VDatabase * database, seq_ctx * sctx, KDirectory * hdf5_src, ld_context *lctx );
rc_t load_seq_src( seq_ctx * sctx, KDirectory * hdf5_src );
rc_t finish_seq( seq_ctx * sctx );

void seq_report_totals( ld_context *lctx );

rc_t load_seq( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx );

#ifdef __cplusplus
}
#endif

#endif
