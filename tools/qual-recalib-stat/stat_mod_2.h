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

#ifndef _h_stat_mod_
#define _h_stat_mod_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <klib/container.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/vector.h>
#include <vdb/cursor.h>
#include "columns.h"

#define N_QUAL_VALUES 41
#define N_DIMER_VALUES 25
#define N_GC_VALUES 8
#define N_HP_VALUES 8
#define N_READS 2
#define N_MAX_QUAL_VALUES 41
#define COUNTER_BLOCK_SIZE 100

#define CASE_MATCH      0
#define CASE_IGNORE     1
#define CASE_MISMATCH   2


typedef struct row_input
{
    char * spotgroup;
    uint32_t spotgroup_len;

    char * seq_spotgroup;
    uint32_t seq_spotgroup_len;

    char * read;
    uint32_t read_len;

    uint8_t * quality;
    uint32_t quality_len;

    bool * has_mismatch;
    uint32_t has_mismatch_len;

    bool * has_roffs;
    uint32_t has_roffs_len;

    int32_t * roffs;
    uint32_t roffs_len;

    uint8_t * exclude;
    uint32_t exclude_len;

    bool reversed;
    uint32_t seq_read_id;
    uint32_t spot_id;
    uint32_t base_pos_offset;
    uint32_t ref_len;
} row_input;


typedef struct stat_row
{
    char * spotgroup;
    char * dimer;
    uint8_t quality;
    uint32_t base_pos;
    uint32_t count;
    uint32_t mismatch_count;
    uint8_t hp_run;
    uint8_t gc_content;
    uint8_t max_qual_value;
    uint8_t n_read;
} stat_row;


typedef struct statistic
{
    BSTree spotgroups;      /* the tree contains 'spotgrp'-node, it collects the statistic */

    bool ignore_mismatches;

    uint32_t gc_window;

    uint8_t *case_vector;
    uint32_t case_vector_len;

    void * last_used_spotgroup;
    uint64_t entries;
    uint32_t max_cycle;
} statistic;


/*************** the STATISTIC GATHERER ***************/
rc_t make_statistic( statistic *data,
                     uint32_t gc_window,
                     bool ignore_mismatches );


rc_t extract_statistic_from_row( statistic * data, 
                                 row_input * row_data,
                                 const int64_t row_id );

uint64_t foreach_statistic( statistic * data,
    bool ( CC * f ) ( stat_row * row, void * f_data ), void *f_data );

void whack_statistic( statistic *data );

#ifdef __cplusplus
}
#endif

#endif
