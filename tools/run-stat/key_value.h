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

#ifndef _h_key_value_
#define _h_key_value_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/rc.h>
#include "helper.h"

typedef struct report
{
    VNamelist * columns;
    uint32_t col_count;
    Vector data;
    uint32_t *max_width;
} report;
typedef report* p_report;


rc_t report_init( p_report * self, uint32_t prealloc );

rc_t report_destroy( p_report self );

rc_t report_clear( p_report self );

rc_t report_set_columns( p_report self, size_t count, ... );

rc_t report_new_data( p_report self );

rc_t report_add_data( p_report self, const size_t estimated_len, const char * fmt, ... );

rc_t report_print( p_report self, p_char_buffer dst, uint32_t mode );

#endif