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
#ifndef _h_join_results_
#define _h_join_results_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_kfs_file_
#include <kfs/file.h>
#endif

#ifndef _h_temp_registry_
#include "temp_registry.h"
#endif

struct join_results;

void destroy_join_results( struct join_results * self );

rc_t make_join_results( struct KDirectory * dir,
                        struct join_results ** results,
                        struct temp_registry * registry,
                        const char * output_base,
                        const char * accession_short,
                        size_t file_buffer_size,
                        size_t print_buffer_size,
                        bool print_frag_nr,
                        bool print_name,
                        const char * filter_bases );

bool join_results_match( struct join_results * self, const String * bases );
bool join_results_match2( struct join_results * self, const String * bases1, const String * bases2 );

rc_t join_results_print( struct join_results * self, uint32_t read_id, const char * fmt, ... );

rc_t join_results_print_fastq_v1( struct join_results * self,
                                  int64_t row_id,
                                  uint32_t dst_id,
                                  uint32_t read_id,
                                  const String * name,
                                  const String * read,
                                  const String * quality );

rc_t join_results_print_fastq_v2( struct join_results * self,
                                  int64_t row_id,
                                  uint32_t dst_id,
                                  uint32_t read_id,
                                  const String * name,
                                  const String * read1,
                                  const String * read2,
                                  const String * quality );

#ifdef __cplusplus
}
#endif

#endif
