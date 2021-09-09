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

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_kfs_file_
#include <kfs/file.h>
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

#ifndef _h_kproc_thread_
#include <kproc/thread.h>
#endif

#ifndef _h_kproc_lock_
#include <kproc/lock.h>
#endif

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif

rc_t CC Quitting(); /* to avoid including kapp/main.h */

typedef struct join_stats
{
    uint64_t spots_read;
    uint64_t reads_read;
    uint64_t reads_written;
    uint64_t reads_zero_length;
    uint64_t reads_technical;
    uint64_t reads_too_short;
    uint64_t reads_invalid;
} join_stats_t;

typedef struct join_options
{
    bool rowid_as_name;
    bool skip_tech;
    bool print_read_nr;
    bool print_name;
    bool terminate_on_invalid;
    uint32_t min_read_len;
    const char * filter_bases;
} join_options_t;

typedef struct SBuffer
{
    String S;
    size_t buffer_size;
} SBuffer_t;

typedef enum format_t {
    ft_unknown, ft_special,
    ft_fastq_whole_spot, ft_fastq_split_spot, ft_fastq_split_file, ft_fastq_split_3,
    ft_fasta_whole_spot, ft_fasta_split_spot, ft_fasta_split_file, ft_fasta_split_3,
    ft_fasta_us_split_spot
    } format_t;
typedef enum compress_t { ct_none, ct_gzip, ct_bzip2 } compress_t;

typedef struct cmn_iter_params
{
    const KDirectory * dir;
    const VDBManager * vdb_mgr;
    const char * accession_short;
    const char * accession_path;
    int64_t first_row;
    uint64_t row_count;
    size_t cursor_cache;
} cmn_iter_params_t;

rc_t ErrMsg( const char * fmt, ... );

rc_t make_SBuffer( SBuffer_t * self, size_t len );
void release_SBuffer( SBuffer_t * self );
rc_t increase_SBuffer( SBuffer_t * self, size_t by );
rc_t increase_SBuffer_to( SBuffer_t * self, size_t new_size );
rc_t print_to_SBufferV( SBuffer_t * self, const char * fmt, va_list args );
rc_t print_to_SBuffer( SBuffer_t * self, const char * fmt, ... );
rc_t try_to_enlarge_SBuffer( SBuffer_t * self, rc_t rc_err );
rc_t make_and_print_to_SBuffer( SBuffer_t * self, size_t len, const char * fmt, ... );

rc_t split_string( String * in, String * p0, String * p1, uint32_t ch );
rc_t split_string_r( String * in, String * p0, String * p1, uint32_t ch );

rc_t split_filename_insert_idx( SBuffer_t * dst, size_t dst_size,
                                const char * filename, uint32_t idx );

format_t get_format_t( const char * format,
        bool split_spot, bool split_file, bool split_3, bool whole_spot,
        bool fasta, bool fasta_us );

compress_t get_compress_t( bool gzip, bool bzip2 );

struct Args;
const char * get_str_option( const struct Args *args, const char *name, const char * dflt );
bool get_bool_option( const struct Args *args, const char *name );
size_t get_size_t_option( const struct Args * args, const char *name, size_t dflt );
uint64_t get_uint64_t_option( const struct Args * args, const char *name, uint64_t dflt );
uint32_t get_uint32_t_option( const struct Args * args, const char *name, uint32_t dflt );

uint64_t make_key( int64_t seq_spot_id, uint32_t seq_read_id );

rc_t pack_4na( const String * unpacked, SBuffer_t * packed );
rc_t pack_read_2_4na( const String * read, SBuffer_t * packed );
rc_t unpack_4na( const String * packed, SBuffer_t * unpacked, bool reverse );

bool ends_in_slash( const char * s );
bool extract_path( const char * s, String * path );
const char * extract_acc( const char * s );
const char * extract_acc2( const char * s );

rc_t create_this_file( KDirectory * dir, const char * filename, bool force );
rc_t create_this_dir( KDirectory * dir, const String * dir_name, bool force );
rc_t create_this_dir_2( KDirectory * dir, const char * dir_name, bool force );

bool file_exists( const KDirectory * dir, const char * fmt, ... );
bool dir_exists( const KDirectory * dir, const char * fmt, ... );

rc_t join_and_release_threads( Vector * threads );

rc_t delete_files( KDirectory * dir, const VNamelist * files );
uint64_t total_size_of_files_in_list( KDirectory * dir, const VNamelist * files );

/*
int get_vdb_pathtype( KDirectory * dir, const VDBManager * vdb_mgr, const char * accession );
*/

void clear_join_stats( join_stats_t * stats );
void add_join_stats( join_stats_t * stats, const join_stats_t * to_add );

rc_t make_buffered_for_read( KDirectory * dir, const struct KFile ** f,
                             const char * filename, size_t buf_size );

/* ===================================================================================== */

typedef struct locked_file_list
{
    KLock * lock;
    VNamelist * files;
} locked_file_list_t;

rc_t locked_file_list_init( locked_file_list_t * self, uint32_t alloc_blocksize );
rc_t locked_file_list_release( locked_file_list_t * self, KDirectory * dir );
rc_t locked_file_list_append( const locked_file_list_t * self, const char * filename );
rc_t locked_file_list_delete_files( KDirectory * dir, locked_file_list_t * self );
rc_t locked_file_list_delete_dirs( KDirectory * dir, locked_file_list_t * self );
rc_t locked_file_list_count( const locked_file_list_t * self, uint32_t * count );
rc_t locked_file_list_pop( locked_file_list_t * self, const String ** item );

/* ===================================================================================== */

typedef struct locked_vector
{
    KLock * lock;
    Vector vector;
    bool sealed;
} locked_vector_t;

rc_t locked_vector_init( locked_vector_t * self, uint32_t alloc_blocksize );
void locked_vector_release( locked_vector_t * self,
                            void ( CC * whack ) ( void *item, void *data ), void *data );
rc_t locked_vector_push( locked_vector_t * self, const void * item, bool seal );
rc_t locked_vector_pop( locked_vector_t * self, void ** item, bool * sealed );


/* ===================================================================================== */

typedef struct locked_value
{
    KLock * lock;
    uint64_t value;
} locked_value_t;

rc_t locked_value_init( locked_value_t * self, uint64_t init_value );
void locked_value_release( locked_value_t * self );
rc_t locked_value_get( locked_value_t * self, uint64_t * value );
rc_t locked_value_set( locked_value_t * self, uint64_t value );

/* ===================================================================================== */

struct Buf2NA_t;

rc_t make_Buf2NA( struct Buf2NA_t ** self, size_t size, const char * pattern );
void release_Buf2NA( struct Buf2NA_t * self );
bool match_Buf2NA( struct Buf2NA_t * self, const String * ascii );

/* ===================================================================================== */

/* common define for bigger stack-size */
#define THREAD_BIG_STACK_SIZE ((size_t)(16u * 1024u * 1024u))
#define THREAD_DFLT_STACK_SIZE ((size_t)(0))

/* common-function to create a thread with a given thread-size */
rc_t helper_make_thread( KThread ** self,
                         rc_t ( CC * run_thread ) ( const KThread * self, void * data ),
                         void * data,
                         size_t stacksize );

/* ===================================================================================== */

rc_t get_quitting( void );
void set_quitting( void );

/* ===================================================================================== */

uint64_t calculate_rows_per_thread( uint32_t * num_threads, uint64_t row_count );
void correct_join_options( join_options_t * dst, const join_options_t * src, bool name_column_present );

/* ===================================================================================== */

rc_t release_file( struct KFile * f, const char * err_msg );
rc_t wrap_file_in_buffer( struct KFile ** f, size_t buffer_size, const char * err_msg );

/* ===================================================================================== */

/* 
    This object describes at which position in the str-args/int-args a variable can be found.
    Its purpose is the be created as a lookup: name->idx,
    to be used by the the var_fmt_.... functions
   */
struct var_desc_list_t;

struct var_desc_list_t * create_var_desc_list( void );
void release_var_desc_list( struct var_desc_list_t * self );
void var_desc_list_add_str( struct var_desc_list_t * self, const char * name, uint32_t idx );
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
