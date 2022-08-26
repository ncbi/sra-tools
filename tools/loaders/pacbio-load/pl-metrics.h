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
#ifndef _h_pl_metrics_
#define _h_pl_metrics_

#ifdef __cplusplus
extern "C" {
#endif

#include "pl-tools.h"
#include "pl-progress.h"
#include <kapp/main.h>
#include <klib/rc.h>

/* enumeration of the columns of the metrics-table */
enum
{
    /* metrix-space */
    metrics_tab_BASE_FRACTION,
    metrics_tab_BASE_IPD,
    metrics_tab_BASE_RATE,
    metrics_tab_BASE_WIDTH,
    metrics_tab_CHAN_BASE_QV,
    metrics_tab_CHAN_DEL_QV,
    metrics_tab_CHAN_INS_QV,
    metrics_tab_CHAN_SUB_QV,
    metrics_tab_LOCAL_BASE_RATE,
    metrics_tab_DARK_BASE_RATE,
    metrics_tab_HQ_RGN_START_TIME,
    metrics_tab_HQ_RGN_END_TIME,
    metrics_tab_HQ_RGN_SNR,
    metrics_tab_PRODUCTIVITY,
    metrics_tab_READ_SCORE,
    metrics_tab_READ_BASE_QV,
    metrics_tab_READ_DEL_QV,
    metrics_tab_READ_INS_QV,
    metrics_tab_READ_SUB_QV,
    metrics_tab_count
};


typedef struct met_ctx
{
    VCursor * cursor;
    ld_context *lctx;
    uint32_t col_idx[ metrics_tab_count ];
} met_ctx;


rc_t prepare_metrics( VDatabase * database, met_ctx * sctx, ld_context *lctx );
rc_t load_metrics_src( met_ctx * sctx, KDirectory * hdf5_src );
rc_t finish_metrics( met_ctx * sctx );

rc_t load_metrics( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx );

#ifdef __cplusplus
}
#endif

#endif
