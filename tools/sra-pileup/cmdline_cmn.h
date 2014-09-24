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

#ifndef _h_cmdline_cmn_
#define _h_cmdline_cmn_

#ifdef __cplusplus
extern "C" {
#endif

#include "ref_regions.h"

#include <kapp/args.h>

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/text.h>
#include <klib/container.h>

#include <kfs/directory.h>
/* #include <sra/srapath.h> */

#include <vdb/manager.h>
#include <vdb/schema.h>

#include <align/iterator.h>
#include <align/reference.h>

#include <strtol.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define OPTION_REF     "aligned-region"
#define ALIAS_REF      "r"

typedef uint8_t align_tab_select;
enum { primary_ats = 1, secondary_ats = 2, evidence_ats = 4 };

typedef struct common_options
{
    bool gzip_output;
    bool bzip_output;
    bool no_mt;
    align_tab_select tab_select;
    const char * output_file;
    const char * input_file;
    const char * schema_file;
    const char * timing_file;
} common_options;


void print_common_helplines( void );

rc_t get_common_options( Args * args, common_options *opts );
OptDef * CommonOptions_ptr( void );
size_t CommonOptions_count( void );

/* get ref-ranges from the command-line and iterate them... */
rc_t init_ref_regions( BSTree * regions, Args * args );

rc_t foreach_argument( Args * args, KDirectory *dir, bool div_by_spotgrp, bool * empty,
    rc_t ( CC * on_argument ) ( const char * path, const char * spot_group, void * data ), void * data );


typedef struct pileup_col_ids
{
    uint32_t idx_quality;
    uint32_t idx_ref_orientation;
    uint32_t idx_read_filter;
    uint32_t idx_template_len;
} pileup_col_ids;

    
typedef struct prepare_ctx
{
    ReferenceIterator *ref_iter;
    PlacementSetIterator *plset_iter;
    const VDatabase *db;
    const VTable *seq_tab;

    const VCursor *prim_cur;
    const VCursor *sec_cur;
    const VCursor *ev_cur;

    pileup_col_ids *prim_cur_ids;
    pileup_col_ids *sec_cur_ids;
    pileup_col_ids *ev_cur_ids;

    const ReferenceList *reflist;
    const ReferenceObj *refobj;
    const char * spot_group;
    bool omit_qualities;
    bool read_tlen;
    bool use_primary_alignments;
    bool use_secondary_alignments;
    bool use_evidence_alignments;
    void * data;
    const char *path;
    rc_t ( CC * on_section ) ( struct prepare_ctx * ctx, const struct reference_range * range );
} prepare_ctx;



rc_t prepare_ref_iter( prepare_ctx *ctx,
                       const VDBManager *vdb_mgr,
                       VSchema *vdb_schema,
                       const char * path,
                       BSTree * regions );

rc_t prepare_plset_iter( prepare_ctx *ctx,
                         const VDBManager *vdb_mgr,
                         VSchema *vdb_schema,
                         const char * path,
                         BSTree * ranges );


rc_t parse_inf_file( Args * args );


#ifdef __cplusplus
}
#endif

#endif /*  _h_cmdline_cmn_ */
