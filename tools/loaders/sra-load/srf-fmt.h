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
#ifndef _sra_load_srf_fmt_
#define _sra_load_srf_fmt_

#include "loader-fmt.h"

typedef struct SRF_context_struct {
    const SRALoaderFile *file;
    const char* file_name;
    const uint8_t* file_buf;
    size_t file_buf_sz;
} SRF_context;

typedef rc_t (SRF_parse_header_func)(SRF_context* ctx, ZTR_Context *ztr_ctx, const uint8_t *data, size_t size);
typedef rc_t (SRF_parse_read_func)(SRF_context* ctx, ZTR_Context *ztr_ctx, const uint8_t *data, size_t size);

rc_t SRF_parse(SRF_context* ctx, SRF_parse_header_func* header, SRF_parse_read_func* read,
               rc_t (*create)(ZTR_Context **ctx), rc_t (*release)(ZTR_Context *self));

void SRF_set_read_filter(uint8_t* filter, int SRF_flags);

#endif /* _sra_load_srf_fmt_ */
