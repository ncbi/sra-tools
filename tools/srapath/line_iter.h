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

#ifndef _h_line_iter_
#define _h_line_iter_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_kns_stream_
#include <kns/stream.h>
#endif

struct line_iter;

void release_line_iter( struct line_iter * iter );
bool advance_line_iter( struct line_iter * iter );
String * get_line_iter( struct line_iter * iter );
bool is_line_iter_done( const struct line_iter * iter );

rc_t make_line_iter( struct line_iter ** iter, const struct KStream * stream,
        size_t buffer_size, uint32_t timeout_ms );

typedef rc_t ( * stream_line_handler_t )( const String * line, void * data );

rc_t stream_line_read( const struct KStream * stream, stream_line_handler_t on_line,
        size_t buffer_size, uint32_t timeout_ms, void * data );

typedef rc_t ( * stream_handler_t )( const struct KStream * stream, void * data );

rc_t print_stream( const struct KStream * stream, size_t buffer_size,
        uint32_t timeout_ms, void * data );


#ifdef __cplusplus
}
#endif

#endif
