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

#ifndef _h_var_fmt_
#define _h_var_fmt_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>     /* uint32_t etc. */
#include <stddef.h>     /* size_t */

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

/* 
    This object describes at which position in the str-args/int-args a variable can be found.
    Its purpose is the be created as a lookup: name->idx,
    to be used by the the var_fmt_.... functions
   */
struct var_desc_list_t;

struct var_desc_list_t * create_var_desc_list( void );
void release_var_desc_list( struct var_desc_list_t * self );
void var_desc_list_add_str( struct var_desc_list_t * self, const char * name, uint32_t idx, uint32_t idx2 );
void var_desc_list_add_int( struct var_desc_list_t * self, const char * name, uint32_t idx );

void var_desc_list_test( void );

/* 
    This object describes a format,
    to be used by the the var_fmt_.... functions
   */

struct var_fmt_t;

struct var_fmt_t * create_empty_var_fmt( size_t buffer_size );
struct var_fmt_t * create_var_fmt( const String * fmt, const struct var_desc_list_t * vars );
struct var_fmt_t * create_var_fmt_str( const char * fmt, const struct var_desc_list_t * vars );
void var_fmt_append( struct var_fmt_t * self,  const String * fmt, const struct var_desc_list_t * vars );
void var_fmt_append_str( struct var_fmt_t * self,  const char * fmt, const struct var_desc_list_t * vars );
struct var_fmt_t * var_fmt_clone( const struct var_fmt_t * src );

void var_fmt_debug( const struct var_fmt_t * self );

void release_var_fmt( struct var_fmt_t * self );

size_t var_fmt_buffer_size( const struct var_fmt_t * self,
                    const String ** str_args, size_t str_args_len );

/* print to buffer */
SBuffer_t * var_fmt_to_buffer( struct var_fmt_t * self,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len );

/* print to stdout */
rc_t var_fmt_to_stdout( struct var_fmt_t * self,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len );

/* print to file */
rc_t var_fmt_to_file( struct var_fmt_t * self,
                    KFile * f,
                    uint64_t * pos,
                    const String ** str_args, size_t str_args_len,
                    const uint64_t * int_args, size_t int_args_len );

void var_fmt_test( void );


#ifdef __cplusplus
}
#endif

#endif
