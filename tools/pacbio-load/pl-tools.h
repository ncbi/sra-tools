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

#ifndef _h_pl_tools_
#define _h_pl_tools_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/out.h>
#include <klib/rc.h>
#include <klib/text.h>
#include <klib/log.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <kfs/file.h>
#include <kfs/arrayfile.h>
#include <hdf5/kdf5.h>
#include <kapp/log-xml.h>
#include <kapp/progressbar.h>

/* for zmw */
#define HOLE_NUMBER_BITSIZE 32
#define HOLE_NUMBER_COLS 1

#define HOLE_STATUS_BITSIZE 8
#define HOLE_STATUS_COLS 1

#define HOLE_XY_BITSIZE 16
#define HOLE_XY_COLS 2

#define NUMEVENT_BITSIZE 32
#define NUMEVENT_COLS 1

#define NUMPASSES_BITSIZE 32
#define NUMPASSES_COLS 1

/* for BaseCalls_cmn */
#define BASECALL_BITSIZE 8
#define BASECALL_COLS 1

#define QUALITY_VALUE_BITSIZE 8
#define QUALITY_VALUE_COLS 1

#define DELETION_QV_BITSIZE 8
#define DELETION_QV_COLS 1

#define DELETION_TAG_BITSIZE 8
#define DELETION_TAG_COLS 1

#define INSERTION_QV_BITSIZE 8
#define INSERTION_QV_COLS 1

#define SUBSTITUTION_QV_BITZISE 8
#define SUBSTITUTION_QV_COLS 1

#define SUBSTITUTION_TAG_BITSIZE 8
#define SUBSTITUTION_TAG_COLS 1

/* for regions */
#define REGIONS_BITSIZE 32
#define REGIONS_COLS 5

/* for sequence */
#define PRE_BASE_FRAMES_BITSIZE 16
#define PRE_BASE_FRAMES_COLS 1

#define PULSE_INDEX_BITSIZE_16 16
#define PULSE_INDEX_BITSIZE_32 32
#define PULSE_INDEX_COLS 1

#define WIDTH_IN_FRAMES_BITSIZE 16
#define WIDTH_IN_FRAMES_COLS 1

/* for metrics */
#define BASE_FRACTION_BITSIZE 32
#define BASE_FRACTION_COLS 4

#define BASE_IPD_BITSIZE 32
#define BASE_IPD_COLS 1

#define BASE_RATE_BITSIZE 32
#define BASE_RATE_COLS 1

#define BASE_WIDTH_BITSIZE 32
#define BASE_WIDTH_COLS 1

#define CM_BAS_QV_BITSIZE 32
#define CM_BAS_QV_COLS 4

#define CM_DEL_QV_BITSIZE 32
#define CM_DEL_QV_COLS 4

#define CM_INS_QV_BITSIZE 32
#define CM_INS_QV_COLS 4

#define CM_SUB_QV_BITSIZE 32
#define CM_SUB_QV_COLS 4

#define LOCAL_BASE_RATE_BITSIZE 32
#define LOCAL_BASE_RATE_COLS 1

#define DARK_BASE_RATE_BITSIZE 32
#define DARK_BASE_RATE_COLS 1

#define HQ_REGION_START_TIME_BITSIZE 32
#define HQ_REGION_START_TIME_COLS 1

#define HQ_REGION_END_TIME_BITSIZE 32
#define HQ_REGION_END_TIME_COLS 1

#define HQ_REGION_SNR_BITSIZE 32
#define HQ_REGION_SNR_COLS 4

#define PRODUCTIVITY_BITSIZE 8
#define PRODUCTIVITY_COLS 1

#define READ_SCORE_BITSIZE 32
#define READ_SCORE_COLS 1

#define RM_BAS_QV_BITSIZE 32
#define RM_BAS_QV_COLS 1

#define RM_DEL_QV_BITSIZE 32
#define RM_DEL_QV_COLS 1

#define RM_INS_QV_BITSIZE 32
#define RM_INS_QV_COLS 1

#define RM_SUB_QV_BITSIZE 32
#define RM_SUB_QV_COLS 1

/* for passes */
#define ADAPTER_HIT_AFTER_BITSIZE 8
#define ADAPTER_HIT_AFTER_COLS 1

#define ADAPTER_HIT_BEFORE_BITSIZE 8
#define ADAPTER_HIT_BEFORE_COLS 1

#define PASS_DIRECTION_BITSIZE 8
#define PASS_DIRECTION_COLS 1

#define PASS_NUM_BASES_BITSIZE 32
#define PASS_NUM_BASES_COLS 1

#define PASS_START_BASE_BITSIZE 32
#define PASS_START_BASE_COLS 1

typedef struct ld_context
{
    const XMLLogger* xml_logger;
    const KLoadProgressbar *xml_progress;
    const char *dst_path;
    uint64_t total_seq_bases;
    uint64_t total_seq_spots;
    bool with_progress;
    bool total_printed;
    bool cache_content;
    bool check_src_obj;
} ld_context;


void lctx_init( ld_context * lctx );
void lctx_free( ld_context * lctx );


rc_t check_src_objects( const KDirectory *hdf5_dir,
                        const char ** groups, 
                        const char **tables,
                        bool show_not_found );

typedef struct af_data
{
    struct KFile const *f;      /* the fake "file" from a HDF5-dir */
    struct KArrayFile *af;      /* the arrayfile made from f */
    rc_t rc;
    uint8_t dimensionality;     /* how many dimensions the HDF5-dataset has */
    uint64_t * extents;         /* the extension in every dimension */
    uint64_t element_bits;      /* how big in bits is the element */
    void * content;             /* read the whole thing into memory */
} af_data;


void init_array_file( af_data * af );
void free_array_file( af_data * af );

rc_t open_array_file( const KDirectory *dir, 
                      const char *name,
                      af_data * af,
                      const uint64_t expected_element_bits,
                      const uint64_t expected_cols,
                      bool disp_wrong_bitsize,
                      bool cache_content,
                      bool supress_err_msg );

rc_t open_element( const KDirectory *hdf5_dir,
                   af_data *element, 
                   const char * path,
                   const char * name, 
                   const uint64_t expected_element_bits,
                   const uint64_t expected_cols,
                   bool disp_wrong_bitsize,
                   bool cache_content,
                   bool supress_err_msg );

rc_t array_file_read_dim1( af_data * af, const uint64_t pos,
                           void *dst, const uint64_t count,
                           uint64_t *n_read );

rc_t array_file_read_dim2( af_data * af, const uint64_t pos,
                           void *dst, const uint64_t count,
                           const uint64_t ext2, uint64_t *n_read );

rc_t add_columns( VCursor * cursor, uint32_t count, int32_t exclude_this,
                  uint32_t * idx_vector, const char ** names );

bool check_table_count( af_data *tab, const char * name,
                        const uint64_t expected );

rc_t transfer_bits( VCursor *cursor, const uint32_t col_idx,
    af_data *src, char * buffer, const uint64_t offset, const uint64_t count,
    const uint32_t n_bits, const char * explanation );

rc_t vdb_write_value( VCursor *cursor, const uint32_t col_idx,
                      void * src, const uint32_t n_bits,
                      const uint32_t n_elem, const char *explanation );

rc_t vdb_write_uint32( VCursor *cursor, const uint32_t col_idx,
                       uint32_t value, const char *explanation );

rc_t vdb_write_uint16( VCursor *cursor, const uint32_t col_idx,
                       uint16_t value, const char *explanation );

rc_t vdb_write_uint8( VCursor *cursor, const uint32_t col_idx,
                      uint8_t value, const char *explanation );

rc_t vdb_write_float32( VCursor *cursor, const uint32_t col_idx,
                        float value, const char *explanation );

typedef rc_t (*loader_func)( ld_context *lctx,
                             KDirectory * hdf5_src, VCursor * cursor,
                             const char * table_name );
                        
rc_t prepare_table( VDatabase * database, VCursor ** cursor,
                    const char * template_name,
                    const char * table_name );

rc_t load_table( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx,
                 const char * template_name, const char * table_name,
                 loader_func func );

rc_t progress_chunk( const KLoadProgressbar ** xml_progress, const uint64_t chunk );
rc_t progress_step( const KLoadProgressbar * xml_progress );

void print_log_info( const char * info );

rc_t pacbio_make_alias( VDatabase * vdb_db,
                        const char *existing_obj, const char *alias_to_create );

#ifdef __cplusplus
}
#endif

#endif
