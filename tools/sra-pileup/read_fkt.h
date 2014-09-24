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
#ifndef _h_read_fkt_
#define _h_read_fkt_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/vector.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/log.h>

#include <vdb/cursor.h>
#include <insdc/sra.h>

#define COL_NOT_AVAILABLE 0xFFFFFFFF
#define INVALID_COLUMN 0xFFFFFFFF

rc_t read_bool( int64_t row_id, const VCursor * cursor, uint32_t idx, bool *res, bool dflt, const char * hint );
rc_t read_bool_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const bool **res, uint32_t *res_len, const char * hint );

rc_t read_uint8( int64_t row_id, const VCursor * cursor, uint32_t idx, uint8_t *res, uint8_t dflt, const char * hint );
rc_t read_uint8_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const uint8_t **res, uint32_t *len, const char * hint );

rc_t read_uint32( int64_t row_id, const VCursor * cursor, uint32_t idx, uint32_t *res, uint32_t dflt, const char * hint );
rc_t read_uint32_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const uint32_t **res, uint32_t *len, const char * hint );

rc_t read_int32( int64_t row_id, const VCursor * cursor, uint32_t idx, int32_t *res, int32_t dflt, const char * hint );
rc_t read_int32_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const int32_t **res, uint32_t *len, const char * hint );

rc_t read_int64( int64_t row_id, const VCursor * cursor, uint32_t idx, int64_t *res, int64_t dflt, const char * hint );
rc_t read_int64_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const int64_t **res, uint32_t *len, const char * hint );

rc_t read_char_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const char **res, uint32_t *len, const char * hint );

rc_t read_INSDC_coord_zero( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_zero *res, INSDC_coord_zero dflt, const char * hint );
rc_t read_INSDC_coord_zero_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_coord_zero **res, uint32_t *len, const char * hint );

rc_t read_INSDC_coord_one( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_one *res, INSDC_coord_one dflt, const char * hint );
rc_t read_INSDC_coord_one_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_one **res, uint32_t *len, const char * hint );

rc_t read_INSDC_coord_len( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_len *res, INSDC_coord_len dflt, const char * hint );
rc_t read_INSDC_coord_len_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_coord_len **res, uint32_t *len, const char * hint );

rc_t read_INSDC_read_type_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_read_type **res, uint32_t *len, const char * hint );
rc_t read_INSDC_read_filter_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_read_filter **res, uint32_t *len, const char * hint );
rc_t read_INSDC_dna_text_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_dna_text **res, uint32_t *len, const char * hint );

bool namelist_contains( const KNamelist * names, const char * a_name );
rc_t add_column( const VCursor * cursor, const char *colname, uint32_t * idx );
void add_opt_column( const VCursor * cursor, const KNamelist *names, const char *colname, uint32_t * idx );

#endif
