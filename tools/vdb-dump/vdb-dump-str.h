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

#ifndef _h_vdb_dump_str_
#define _h_vdb_dump_str_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <klib/rc.h>

typedef struct dump_str
{
    char *buf;
    size_t buf_size;
	size_t str_len;
	size_t str_limit;
	size_t buf_inc;
	bool truncated;
} dump_str;
typedef dump_str* p_dump_str;

#define DUMP_STR_INC 512

/* initializes the dump-string with limit and buffer-increment */
rc_t vds_make( p_dump_str s, const size_t limit, const size_t inc );

/* free's the dump-string's internal buffer */
rc_t vds_free( p_dump_str s );

/* the internal buffer stay's allocated, but the internal string-len
   and the truncated-state is cleared */
rc_t vds_clear( p_dump_str s );

/* returns if the dump-string was trancated */
bool vds_truncated( p_dump_str s );

/* returns if assembled dump-string */
char *vds_ptr( p_dump_str s );

/* appends the formated string with parameters, truncates to the limit */
rc_t vds_append_fmt( p_dump_str s, const size_t aprox_len, const char *fmt, ... );

/* appends the string, truncates to the limit */
rc_t vds_append_str( p_dump_str s, const char *s1 );

/* appends the string, does not truncate */
rc_t vds_append_str_no_limit_check( p_dump_str s, const char *s1 );

/* right-inserts the string at the end of the ev. limited string */
rc_t vds_rinsert( p_dump_str s, const char *s1 );

/* indents the content of a dump-string */
rc_t vds_indent( p_dump_str s, const size_t limit, const size_t indent );

/* converts the dump-string into csv-style (quotes at begin and end if the
   string contains a comma or other quotes, escapes quotes with quotes ) */
rc_t vds_2_csv( p_dump_str s );

rc_t vds_enclose_string( p_dump_str s, const char c_left, const char c_right );
rc_t vds_escape( p_dump_str s, const char to_escape, const char escape_char );

#ifdef __cplusplus
}
#endif

#endif
