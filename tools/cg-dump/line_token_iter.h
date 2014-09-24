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

#ifndef _h_line_token_iter_
#define _h_line_token_iter_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <klib/text.h>


typedef struct buf_line_iter
{
    const char * nxt;
    size_t len;
    uint32_t line_nr;
} buf_line_iter;

rc_t buf_line_iter_init( struct buf_line_iter * self, const char * buffer, size_t len );
rc_t buf_line_iter_get( struct buf_line_iter * self, String * line, bool * valid, uint32_t * line_nr );


typedef struct token_iter
{
    String line;
    char delim;
    uint32_t token_nr;
    uint32_t idx;
} token_iter;

rc_t token_iter_init( struct token_iter * self, const String * line, char delim );
rc_t token_iter_get( struct token_iter * self, String * token, bool * valid, uint32_t * token_nr );



#ifdef __cplusplus
}
#endif

#endif
