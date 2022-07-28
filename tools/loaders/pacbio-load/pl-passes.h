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
#ifndef _h_pl_passes_
#define _h_pl_passes_

#ifdef __cplusplus
extern "C" {
#endif

#include "pl-tools.h"
#include "pl-progress.h"
#include <klib/rc.h>
#include <kapp/main.h>

/* enumeration of the columns of the passes-table */
enum
{
    /* consensus-space */
    passes_tab_ADAPTER_HIT_BEFORE,
    passes_tab_ADAPTER_HIT_AFTER,
    passes_tab_PASS_DIRECTION,
    passes_tab_PASS_NUM_BASES,
    passes_tab_PASS_START_BASE,
    passes_tab_count
};


typedef struct pas_ctx
{
    VCursor * cursor;
    ld_context *lctx;
    uint32_t col_idx[ passes_tab_count ];
} pas_ctx;


rc_t prepare_passes( VDatabase * database, pas_ctx * sctx, ld_context *lctx );
rc_t load_passes_src( pas_ctx * sctx, KDirectory * hdf5_src );
rc_t finish_passes( pas_ctx * sctx );


rc_t load_passes( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx );

#ifdef __cplusplus
}
#endif

#endif
