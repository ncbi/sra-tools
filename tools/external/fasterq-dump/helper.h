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

#ifndef _h_sbuffer_
#include "sbuffer.h"
#endif
    
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
    bool print_spotgroup;
    uint32_t min_read_len;
    const char * filter_bases;
} join_options_t;

/* -------------------------------------------------------------------------------- */

typedef enum format_t {
    ft_unknown,
    ft_fastq_whole_spot, ft_fastq_split_spot, ft_fastq_split_file, ft_fastq_split_3,
    ft_fasta_whole_spot, ft_fasta_split_spot, ft_fasta_split_file, ft_fasta_split_3,
    ft_fasta_us_split_spot
    } format_t;

bool is_format_fasta( format_t fmt );

format_t get_format_t( const char * format,
        bool split_spot, bool split_file, bool split_3, bool whole_spot,
        bool fasta, bool fasta_us );

const char * out_ext( bool fasta );

const char * fmt_2_string( format_t fmt );

/* -------------------------------------------------------------------------------- */

typedef enum check_mode_t {
    cmt_unknown, cmt_on, cmt_off, cmt_only
    } check_mode_t;

check_mode_t get_check_mode_t( const char * mode );

bool is_perform_check( check_mode_t mode );

const char * check_mode_2_string( check_mode_t cm );

/* -------------------------------------------------------------------------------- */

const char * yes_or_no( bool b );

/* -------------------------------------------------------------------------------- */

rc_t CC Quitting(); /* to avoid including kapp/main.h */
uint32_t get_env_u32( const char * name, uint32_t dflt );
uint64_t make_key( int64_t seq_spot_id, uint32_t seq_read_id );
void correct_join_options( join_options_t * dst, const join_options_t * src, bool name_column_present );

rc_t get_quitting( void );
void set_quitting( void );

/* -------------------------------------------------------------------------------- */

const String * make_string_copy( const char * src );

rc_t split_string( String * in, String * p0, String * p1, uint32_t ch );
rc_t split_string_r( String * in, String * p0, String * p1, uint32_t ch );
bool ends_in_slash( const char * s );
bool ends_in_sra( const char * s );
bool extract_path( const char * s, String * path );

/* -------------------------------------------------------------------------------- */

void clear_join_stats( join_stats_t * stats );
void add_join_stats( join_stats_t * stats, const join_stats_t * to_add );

/* -------------------------------------------------------------------------------- */

struct Buf2NA_t;

rc_t make_Buf2NA( struct Buf2NA_t ** self, size_t size, const char * pattern );
void release_Buf2NA( struct Buf2NA_t * self );
bool match_Buf2NA( struct Buf2NA_t * self, const String * ascii );

/* -------------------------------------------------------------------------------- */

/* common define for bigger stack-size */
#define THREAD_BIG_STACK_SIZE ((size_t)(16u * 1024u * 1024u))
#define THREAD_DFLT_STACK_SIZE ((size_t)(0))

/* common-function to create a thread with a given thread-size */
rc_t helper_make_thread( KThread ** self,
                         rc_t ( CC * run_thread ) ( const KThread * self, void * data ),
                         void * data,
                         size_t stacksize );

rc_t join_and_release_threads( Vector * threads );
uint64_t calculate_rows_per_thread( uint32_t * num_threads, uint64_t row_count );

/* -------------------------------------------------------------------------------- */

rc_t print_stats( const join_stats_t * stats );

/* -------------------------------------------------------------------------------- */

struct filter_2na_t;

struct filter_2na_t * make_2na_filter( const char * filter_bases );
void release_2na_filter( struct filter_2na_t * self );

/* return true if no filter set, or filter matches the bases */
bool filter_2na_1( struct filter_2na_t * self, const String * bases );
bool filter_2na_2( struct filter_2na_t * self, const String * bases1, const String * bases2 );

/* -------------------------------------------------------------------------------- */

/* returns 0 if the id cannot be found ( for instance on none-posix systems ) */
uint32_t device_id_of_path( const char * path );
bool paths_on_same_filesystem( const char * path1, const char * path2 );

#ifdef __cplusplus
}
#endif

#endif
