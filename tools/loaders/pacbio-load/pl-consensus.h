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
#ifndef _h_pl_consensus_
#define _h_pl_consensus_

#ifdef __cplusplus
extern "C" {
#endif

#include "pl-tools.h"
#include "pl-zmw.h"
#include "pl-basecalls_cmn.h"
#include <klib/rc.h>
#include <insdc/sra.h>

/* enumeration of the columns of the consensus-table */
enum
{
    /* base-space */
    consensus_tab_READ = 0,
    consensus_tab_QUALITY,
    consensus_tab_NREADS,
    consensus_tab_READ_TYPE,
    consensus_tab_READ_START,
    consensus_tab_READ_LEN,
    consensus_tab_PLATFORM,
    consensus_tab_READ_FILTER,

    /* consensus-space */
    consensus_tab_HOLE_NUMBER,
    consensus_tab_HOLE_STATUS,
    consensus_tab_HOLE_XY,
    consensus_tab_NUM_PASSES,
    consensus_tab_INSERTION_QV,
    consensus_tab_DELETION_QV,
    consensus_tab_DELETION_TAG,
    consensus_tab_SUBSTITUTION_QV,
    consensus_tab_SUBSTITUTION_TAG,
    consensus_tab_count
};


typedef struct con_ctx
{
    VCursor * cursor;
    ld_context *lctx;
    uint32_t col_idx[ consensus_tab_count ];
} con_ctx;

rc_t prepare_consensus( VDatabase * database, con_ctx * sctx, ld_context *lctx );
rc_t load_consensus_src( con_ctx * sctx, KDirectory * hdf5_src );
rc_t finish_consensus( con_ctx * sctx );

rc_t load_consensus( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx );

#ifdef __cplusplus
}
#endif

#endif
