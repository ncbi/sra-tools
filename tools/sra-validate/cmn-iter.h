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

#ifndef _h_cmn_iter_
#define _h_cmn_iter_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_insdc_sra_
#include <insdc/sra.h>  /* INSDC_SRA_platform_id */
#endif

typedef struct cmn_params
{
    const KDirectory * dir;
    const char * accession;
    int64_t first_row;
    uint64_t row_count;
    size_t cursor_cache;
} cmn_params;

struct cmn_iter;

void destroy_cmn_iter( struct cmn_iter * self );

rc_t make_cmn_iter( const cmn_params * cp, const char * tblname, struct cmn_iter ** iter );

rc_t cmn_iter_add_column( struct cmn_iter * self, const char * name, uint32_t * id );
rc_t cmn_iter_range( struct cmn_iter * selfr, uint32_t col_id );

bool cmn_iter_next( struct cmn_iter * self, rc_t * rc );
int64_t cmn_iter_get_row_id( const struct cmn_iter * self );
rc_t cmn_iter_set_row_id( struct cmn_iter * self, int64_t row_id );

uint64_t cmn_iter_row_count( struct cmn_iter * self );

rc_t cmn_read_uint64( struct cmn_iter * self, uint32_t col_id, uint64_t *value );
rc_t cmn_read_uint64_array( struct cmn_iter * self, uint32_t col_id, uint64_t **values, uint32_t * values_read );
                            
rc_t cmn_read_uint32( struct cmn_iter * selfr, uint32_t col_id, uint32_t *value );
rc_t cmn_read_uint32_array( struct cmn_iter * self, uint32_t col_id, uint32_t **values, uint32_t * values_read );

rc_t cmn_read_uint8( struct cmn_iter * self, uint32_t col_id, uint8_t *value );
rc_t cmn_read_uint8_array( struct cmn_iter * self, uint32_t col_id, uint8_t **values, uint32_t * values_read );
                            
rc_t cmn_read_String( struct cmn_iter * self, uint32_t col_id, String *value );

typedef enum acc_type_t { acc_csra, acc_sra_flat, acc_sra_db, acc_pacbio, acc_none } acc_type_t;
typedef struct acc_info_t
{
    const char * accession;
    acc_type_t acc_type;
    INSDC_SRA_platform_id platform; /* insdc/sra.h */
    uint64_t seq_rows;
} acc_info_t;

rc_t cmn_get_acc_info( KDirectory * dir, const char * accession, acc_info_t * acc_info );

const char * platform_2_text( INSDC_SRA_platform_id id );

rc_t cmn_check_tbl_column( KDirectory * dir, const char * accession,
                           const char * col_name, bool * present );

rc_t cmn_check_db_column( KDirectory * dir, const char * accession, const char * tbl_name,
                          const char * col_name,  bool * present );

VNamelist * cmn_get_table_names( KDirectory * dir, const char * accession );

#ifdef __cplusplus
}
#endif

#endif
