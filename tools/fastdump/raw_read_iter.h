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

#ifndef _h_raw_read_iter_
#define _h_raw_read_iter_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_cmn_iter_
#include "cmn_iter.h"
#endif

struct raw_read_iter;

typedef struct raw_read_rec
{
    uint64_t seq_spot_id;
    uint32_t seq_read_id;
    String raw_read;
} raw_read_rec;

void destroy_raw_read_iter( struct raw_read_iter * iter );

rc_t make_raw_read_iter( cmn_params * params, struct raw_read_iter ** iter );

bool get_from_raw_read_iter( struct raw_read_iter * iter, raw_read_rec * rec, rc_t * rc );

uint64_t get_row_count_of_raw_read( struct raw_read_iter * iter );

rc_t write_out_prim( const KDirectory *dir, size_t buf_size, size_t cursor_cache, const char * accession, const char * output_file );

#ifdef __cplusplus
}
#endif

#endif
