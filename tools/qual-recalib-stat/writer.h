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

#ifndef _h_stat_writer_
#define _h_stat_writer_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <klib/namelist.h>
#include <kfs/directory.h>
#include <vdb/cursor.h>
#include <vdb/schema.h>
#include <vdb/manager.h>
#include <vdb/table.h>
#include <vdb/database.h>
#include "columns.h"
#include "stat_mod_2.h"
#include "progressbar.h"

#define WIDX_SPOT_GROUP      0
#define WIDX_KMER            1
#define WIDX_ORIG_QUAL       2
#define WIDX_TOTAL_COUNT     3
#define WIDX_MISMATCH_COUNT  4
#define WIDX_CYCLE           5
#define WIDX_HPRUN           6
#define WIDX_GC_CONTENT      7
#define WIDX_MAX_QUAL        8
#define WIDX_NREAD           9
#define N_WIDX               10

typedef struct statistic_writer
{
    VCursor *cursor;
    col wr_col[ N_WIDX ];
} statistic_writer;


rc_t write_output_file( KDirectory *dir, statistic * data,
                        const char * path, uint64_t * written );

rc_t write_statistic_into_tab( KDirectory *dir, statistic * data,
        const KNamelist *schema_list, const char *output_file_path,
        uint64_t * written, bool show_progress );

rc_t write_statistic_into_db( KDirectory *dir, statistic * data,
         const KNamelist *schema_list, const char *src_path,
        uint64_t * written, bool show_progress );

#ifdef __cplusplus
}
#endif

#endif
