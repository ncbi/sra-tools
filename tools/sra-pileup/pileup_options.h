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

#ifndef _h_pileup_options_
#define _h_pileup_options_

#ifdef __cplusplus
extern "C" {
#endif

#include "ref_regions.h"
#include "cmdline_cmn.h"

typedef struct pileup_options
{
    common_options cmn;     /* from cmdline_cmn.h */
    bool process_dups;
    bool omit_qualities;
    bool read_tlen;
    bool no_skip;
    bool show_id;
    bool div_by_spotgrp;
    bool use_seq_name;
    uint32_t minmapq;
    uint32_t min_mismatch;
    uint32_t merge_dist;
    uint32_t source_table;
    uint32_t function;  /* sra_pileup_samtools, sra_pileup_counters, sra_pileup_stat, 
                           sra_pileup_report_ref, sra_pileup_report_ref_ext, sra_pileup_debug, etc */
    struct skiplist * skiplist;     /* from ref_regions.h */
} pileup_options;


#ifdef __cplusplus
}
#endif

#endif /*  pileup_options_ */
