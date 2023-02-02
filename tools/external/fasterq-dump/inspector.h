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

#ifndef _h_inspector_
#define _h_inspector_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif

#ifndef _h_vfs_path_
#include <vfs/path.h>
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

typedef struct inspector_input_t
{
    KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * accession_short;
    const char * accession_path;
    const char * requested_seq_tbl_name;
} inspector_input_t;

typedef enum acc_type_t { 
    acc_csra,               /* proper cSRA ( at least a SEQ, REF, and ALIGN table ) */
    acc_pacbio_bam,         /* PLATFORM is PacBio, but schema is cSRA, loaded with bam-load */
    acc_pacbio_native,      /* PLATFORM is PacBio, schema is PacBio, at least a SEQ-table, loaded with pacbio-load */
    acc_sra_flat,           /* older SRA-format, unaligned, just a table */
    acc_sra_db,             /* newer SRA-format, unaligned, a database with only 1 table: SEQ */
    acc_none                /* none of the above */
 } acc_type_t;

typedef struct inspector_seq_data_t
{
    const char * tbl_name;
    bool has_name_column;
    bool has_spot_group_column;
    int64_t  first_row;
    uint64_t row_count;
    uint64_t spot_count;
    uint64_t total_base_count;
    uint64_t bio_base_count;
    uint32_t avg_name_len;
    uint32_t avg_spot_group_len;
    uint32_t avg_bio_reads;
    uint32_t avg_tech_reads;
    
} inspector_seq_data_t;

typedef struct inspector_align_data_t
{
    int64_t  first_row;
    uint64_t row_count;
    uint64_t spot_count;    
    uint64_t total_base_count;
    uint64_t bio_base_count;

} inspector_align_data_t;

typedef struct inspector_output_t
{
    acc_type_t acc_type;
    bool is_remote;
    size_t acc_size;

    inspector_seq_data_t seq;
    inspector_align_data_t align;
} inspector_output_t;

rc_t inspect( const inspector_input_t * input, inspector_output_t * output );

rc_t inspection_report( const inspector_input_t * input, const inspector_output_t * output );

rc_t inspector_path_to_vpath( const char * path, VPath ** vpath );

const char * inspector_extract_acc_from_path( const char * s );

/* ------------------------------------------------------------------------------------------- */

typedef struct inspector_estimate_input_t
{
    const inspector_output_t * insp;    /* above */
    const char * seq_defline;
    const char * qual_defline;
    const char * acc;
    format_t fmt;                       /* helper.h */
    uint32_t avg_name_len;
    uint32_t avg_bio_reads;
    uint32_t avg_tech_reads;
    bool skip_tech;
} inspector_estimate_input_t;

size_t inspector_estimate_output_size( const inspector_estimate_input_t * input );

/* ------------------------------------------------------------------------------------------- */

void unread_rc_info( bool show );

#ifdef __cplusplus
}
#endif

#endif
