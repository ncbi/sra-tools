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

#ifndef _h_lookup_reader_
#define _h_lookup_reader_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_helper_
#include "helper.h"
#endif


#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_index_
#include "index.h"
#endif

struct lookup_reader;

void release_lookup_reader( struct lookup_reader * self );

rc_t make_lookup_reader( const KDirectory *dir, const struct index_reader * index,
                         struct lookup_reader ** reader, size_t buf_size, const char * fmt, ... );

rc_t seek_lookup_reader( struct lookup_reader * self, uint64_t key, uint64_t * key_found, bool exactly );

rc_t lookup_reader_get( struct lookup_reader * self, uint64_t * key, SBuffer * packed_bases );
rc_t lookup_bases( struct lookup_reader * self, int64_t row_id, uint32_t read_id, SBuffer * B );

rc_t lookup_check( struct lookup_reader * self );
rc_t lookup_check_file( const KDirectory *dir, size_t buf_size, const char * filename );

rc_t lookup_count_file( const KDirectory *dir, size_t buf_size, const char * filename, uint32_t * count );

rc_t write_out_lookup( const KDirectory *dir, size_t buf_size, const char * lookup_file, const char * output_file );

#ifdef __cplusplus
}
#endif

#endif
