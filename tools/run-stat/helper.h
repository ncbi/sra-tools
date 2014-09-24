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

#ifndef _h_helper_
#define _h_helper_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/namelist.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/text.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <sra/sraschema.h>
#include "definitions.h"
#include "context.h"


typedef struct char_buffer
{
    char *ptr;
    const char *trans_ptr;
    size_t len;         /* how many bytes are used... */
    size_t size;        /* how much is allocated... */
} char_buffer;
typedef char_buffer* p_char_buffer;

rc_t char_buffer_init( p_char_buffer buffer, const size_t size );
void char_buffer_destroy( p_char_buffer buffer );
rc_t char_buffer_realloc( p_char_buffer buffer, const size_t by );
rc_t char_buffer_append_cstring( p_char_buffer buffer, const char * s );
rc_t char_buffer_printfv( p_char_buffer buffer, const size_t estimated_len,
                          const char * fmt, va_list args );
rc_t char_buffer_printf( p_char_buffer buffer, const size_t estimated_len,
                         const char * fmt, ... );
rc_t char_buffer_saveto( p_char_buffer buffer, const char * filename );


typedef struct int_buffer
{
    uint32_t *ptr;
    const uint32_t *trans_ptr;
    size_t len;         /* how many ints are used... */
    size_t size;        /* how much is allocated... */
} int_buffer;
typedef int_buffer* p_int_buffer;

rc_t int_buffer_init( p_int_buffer buffer, const size_t size );
void int_buffer_destroy( p_int_buffer buffer );
rc_t int_buffer_realloc( p_int_buffer buffer, const size_t by );

int helper_str_cmp( const char *a, const char *b );

/*
 * calls the given manager to create a new SRA-schema
 * takes the list of user-supplied schema's (which can be empty)
 * and let the created schema parse all of them
*/
rc_t helper_parse_schema( const VDBManager *my_manager,
                          VSchema **new_schema,
                          const KNamelist *schema_list );


rc_t helper_make_namelist_from_string( const KNamelist **list, 
                                       const char * src,
                                       const char split_char );

bool helper_take_this_table_from_db( const VDatabase * db,
                                     p_stat_ctx ctx,
                                     const char * table_to_find );

bool helper_take_1st_table_from_db( const VDatabase * db,
                                    p_stat_ctx ctx );

char * helper_concat( const char * s1, const char * s2 );

double percent( const uint64_t value, const uint64_t sum );

rc_t helper_split_by_parenthesis( const char * src, char ** module, char ** param );

rc_t helper_read_uint32( const VCursor * cur, const uint32_t cur_idx,
                         uint32_t *value );

rc_t helper_read_uint64( const VCursor * cur, const uint32_t cur_idx,
                         uint64_t *value );

rc_t helper_read_int32_values( const VCursor * cur, const uint32_t cur_idx,
                               p_int_buffer buf );

rc_t helper_read_char_values( const VCursor * cur, const uint32_t cur_idx,
                              p_char_buffer buf );


#ifdef __cplusplus
}
#endif

#endif
