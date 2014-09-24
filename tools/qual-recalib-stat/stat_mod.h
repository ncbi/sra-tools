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
#include <vdb/cursor.h>
#include "ref_exclude.h"
#include "columns.h"

#define N_QUAL_VALUES 40
#define N_DIMER_VALUES 17
#define N_GC_VALUES 8
#define N_HP_VALUES 16

#define POS_VECTOR_INC 50

#define RIDX_READ            0
#define RIDX_QUALITY         1
#define RIDX_HAS_MISMATCH    2
#define RIDX_SPOT_GROUP      3
#define RIDX_SEQ_SPOT_GROUP  4
#define RIDX_REF_ORIENTATION 5
#define RIDX_READ_LEN        6
#define RIDX_SEQ_READ_ID     7
#define RIDX_HAS_REF_OFFSET  8
#define RIDX_REF_OFFSET      9
#define RIDX_REF_POS        10
#define RIDX_REF_SEQ_ID     11
#define RIDX_REF_LEN        12
#define N_RIDX              13

#define WIDX_SPOT_GROUP      0
#define WIDX_KMER            1
#define WIDX_ORIG_QUAL       2
#define WIDX_TOTAL_COUNT     3
#define WIDX_MISMATCH_COUNT  4
#define WIDX_CYCLE           5
#define WIDX_HPRUN           6
#define WIDX_GC_CONTENT      7
#define N_WIDX               8

#define CASE_MATCH      0
#define CASE_IGNORE     1
#define CASE_MISMATCH   2


typedef struct stat_row
{
    char * spotgroup;
    char * dimer;
    uint8_t quality;
    uint32_t base_pos;
    uint32_t count;
    uint32_t mismatch_count;
    uint32_t hp_run;
    uint32_t gc_content;
} stat_row;


typedef struct statistic
{
    BSTree spotgroups;
    col rd_col[ N_RIDX ];

    ref_exclude exclude;
    uint32_t gc_window;

    uint8_t *exclude_vector;
    uint32_t exclude_vector_len;

    uint8_t *case_vector;
    uint32_t case_vector_len;

    void * last_used_spotgroup;
} statistic;


typedef struct statistic_writer
{
    VCursor *cursor;
    col wr_col[ N_WIDX ];
} statistic_writer;


/*************** the READER ***************/
rc_t make_statistic( statistic *data, uint32_t gc_window,
                     KDirectory *dir, const char * exclude_db );

rc_t open_statistic_cursor( statistic * data, const VCursor *my_cursor );

rc_t query_statistic_rowrange( statistic * data, const VCursor *my_cursor, 
                               int64_t *first, uint64_t * count );

rc_t read_and_extract_statistic_from_row( statistic * data,
                const VCursor *my_cursor, const int64_t row_id );

uint64_t foreach_statistic( statistic * data,
    bool ( CC * f ) ( stat_row * row, void * f_data ), void *f_data );

void whack_statistic( statistic *data );


/*************** the WRITER ***************/
rc_t make_statistic_writer( statistic_writer *writer, VCursor * cursor );

rc_t write_statistic( statistic_writer *writer, statistic *data,
                      uint64_t * written );

rc_t whack_statistic_writer( statistic_writer *writer );

#ifdef __cplusplus
}
#endif

#endif
