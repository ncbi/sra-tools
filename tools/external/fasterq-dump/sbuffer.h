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

#ifndef _h_sbuffer_
#define _h_sbuffer_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

typedef struct SBuffer
{
    String S;
    size_t buffer_size;
} SBuffer_t;

rc_t make_SBuffer( SBuffer_t * self, size_t len );
void release_SBuffer( SBuffer_t * self );
rc_t increase_SBuffer_by( SBuffer_t * self, size_t by );
rc_t increase_SBuffer_to( SBuffer_t * self, size_t new_size );
rc_t print_to_SBufferV( SBuffer_t * self, const char * fmt, va_list args );
rc_t print_to_SBuffer( SBuffer_t * self, const char * fmt, ... );
rc_t try_to_enlarge_SBuffer( SBuffer_t * self, rc_t rc_err );
rc_t make_and_print_to_SBuffer( SBuffer_t * self, size_t len, const char * fmt, ... );

rc_t split_filename_insert_idx( SBuffer_t * dst, size_t dst_size,
                                const char * filename, uint32_t idx );

rc_t copy_SBuffer( SBuffer_t * self, const SBuffer_t * src );
rc_t append_SBuffer( SBuffer_t * self, const SBuffer_t * src );
rc_t clear_SBuffer( SBuffer_t * self );

#ifdef __cplusplus
}
#endif

#endif
