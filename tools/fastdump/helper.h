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

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_klib_num_gen_
#include <klib/num-gen.h>
#endif

#ifndef _h_vdb_cursor_
#include <vdb/cursor.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

/* 
    this is in interfaces/cc/XXX/YYY/atomic.h
    XXX ... the compiler ( cc, gcc, icc, vc++ )
    YYY ... the architecture ( fat86, i386, noarch, ppc32, x86_64 )
 */
#ifndef _h_atomic_
#include <atomic.h>
#endif

#ifndef _h_kproc_thread_
#include <kproc/thread.h>
#endif

#ifndef _h_kproc_lock_
#include <kproc/lock.h>
#endif

rc_t CC Quitting(); /* to avoid including kapp/main.h */

typedef struct tmp_id
{
    const char * temp_path;
    const char * hostname;
    uint32_t pid;
    bool temp_path_ends_in_slash;
} tmp_id;


typedef struct SBuffer
{
    String S;
    size_t buffer_size;
} SBuffer;

typedef struct part_head
{
    uint64_t row_id;
    uint32_t total, len, part, padd;
} part_head;

typedef enum format_t { ft_special, ft_fastq, ft_fastq_split } format_t;
typedef enum compress_t { ct_none, ct_gzip, ct_bzip2 } compress_t;

rc_t ErrMsg( const char * fmt, ... );

rc_t make_SBuffer( SBuffer * self, size_t len );
void release_SBuffer( SBuffer * self );
rc_t print_to_SBufferV( SBuffer * self, const char * fmt, va_list args );
rc_t print_to_SBuffer( SBuffer * self, const char * fmt, ... );
rc_t make_and_print_to_SBuffer( SBuffer * self, size_t len, const char * fmt, ... );

rc_t add_column( const VCursor * cursor, const char * name, uint32_t * id );

rc_t make_row_iter( struct num_gen * ranges, int64_t first, uint64_t count, 
                    const struct num_gen_iter ** iter );

rc_t split_string( String * in, String * p0, String * p1, uint32_t ch );

format_t get_format_t( const char * format );

compress_t get_compress_t( bool gzip, bool bzip2 );

struct Args;
const char * get_str_option( const struct Args *args, const char *name, const char * dflt );
bool get_bool_option( const struct Args *args, const char *name );
size_t get_size_t_option( const struct Args * args, const char *name, size_t dflt );
uint64_t get_uint64_t_option( const struct Args * args, const char *name, uint64_t dflt );

uint64_t make_key( int64_t seq_spot_id, uint32_t seq_read_id );

void pack_4na( const String * unpacked, SBuffer * packed );
void unpack_4na( const String * packed, SBuffer * unpacked );

uint64_t calc_percent( uint64_t max, uint64_t value, uint16_t digits );

bool file_exists( const KDirectory * dir, const char * fmt, ... );

void join_and_release_threads( Vector * threads );

rc_t delete_files( KDirectory * dir, const VNamelist * files );

int get_vdb_pathtype( KDirectory * dir, const char * accession );

typedef struct multi_progress
{
    atomic_t progress_done;
    atomic_t progress_rows;
    uint64_t row_count;
} multi_progress;

void init_progress_data( multi_progress * progress_data, uint64_t row_count );
rc_t start_multi_progress( KThread **t, multi_progress * progress_data );
void join_multi_progress( KThread *t, multi_progress * progress_data );

rc_t make_pre_and_post_fixed( char * dst, size_t dst_size,
                              const char * acc,
                              const tmp_id * tmp_id,
                              const char * extension );

rc_t make_prefixed( char * buffer, size_t bufsize, const char * prefix,
                    const char * path, const char * postfix );

rc_t make_postfixed( char * buffer, size_t bufsize, const char * path, const char * postfix );

/* ===================================================================================== */

typedef struct locked_file_list
{
    KLock * lock;
    VNamelist * files;
} locked_file_list;

rc_t locked_file_list_init( locked_file_list * self, uint32_t alloc_blocksize );
rc_t locked_file_list_release( locked_file_list * self, KDirectory * dir );
rc_t locked_file_list_append( const locked_file_list * self, const char * filename );
rc_t locked_file_list_delete_all( KDirectory * dir, locked_file_list * self );
rc_t locked_file_list_count( const locked_file_list * self, uint32_t * count );
rc_t locked_file_list_pop( locked_file_list * self, const String ** item );

/* ===================================================================================== */

typedef struct locked_vector
{
    KLock * lock;
    Vector vector;
    bool sealed;
} locked_vector;

rc_t locked_vector_init( locked_vector * self, uint32_t alloc_blocksize );
void locked_vector_release( locked_vector * self,
                            void ( CC * whack ) ( void *item, void *data ), void *data );
rc_t locked_vector_push( locked_vector * self, const void * item, bool seal );
rc_t locked_vector_pop( locked_vector * self, void ** item, bool * sealed );

/* ===================================================================================== */

typedef struct locked_value
{
    KLock * lock;
    uint64_t value;
} locked_value;

rc_t locked_value_init( locked_value * self, uint64_t init_value );
void locked_value_release( locked_value * self );
rc_t locked_value_get( locked_value * self, uint64_t * value );
rc_t locked_value_set( locked_value * self, uint64_t value );



#ifdef __cplusplus
}
#endif

#endif
