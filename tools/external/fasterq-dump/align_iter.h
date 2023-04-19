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

#ifndef _h_align_iter_
#define _h_align_iter_

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


typedef struct alit_rec_t
{
    int64_t row_id;
    uint64_t spot_id;
    uint32_t read_id;
    String read;
} alit_rec_t;

struct alit_t;

rc_t alit_create( const cmn_iter_params_t * params,
                  struct alit_t ** iter,
                  bool uses_read_id );

void alit_release( struct alit_t * self );

bool alit_get_rec( struct alit_t * self, alit_rec_t * rec, rc_t * rc );

uint64_t alit_get_row_count( struct alit_t * self );

rc_t alit_extract_row_count( KDirectory * dir,
                             const VDBManager * vdb_mgr,
                             const char * accession_short,
                             const char * accession_path,
                             size_t cur_cache,
                             uint64_t * res );
    
#ifdef __cplusplus
}
#endif

#endif
