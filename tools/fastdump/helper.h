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

#ifndef _h_vdb_curosr_
#include <vdb/cursor.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

typedef struct SBuffer
{
    String S;
    size_t buffer_size;
} SBuffer;


typedef enum format_t { ft_special, ft_fastq, ft_lookup, ft_test } format_t;

rc_t ErrMsg( const char * fmt, ... );

rc_t make_SBuffer( SBuffer * buffer, size_t len );
void release_SBuffer( SBuffer * buffer );
rc_t print_to_SBufferV( SBuffer * buffer, const char * fmt, va_list args );
rc_t print_to_SBuffer( SBuffer * buffer, const char * fmt, ... );

rc_t add_column( const VCursor * cursor, const char * name, uint32_t * id );

rc_t make_row_iter( struct num_gen * ranges, int64_t first, uint64_t count, 
                    const struct num_gen_iter ** iter );

rc_t split_string( String * in, String * p0, String * p1, uint32_t ch );

format_t get_format_t( const char * format );

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

#ifdef __cplusplus
}
#endif

#endif
