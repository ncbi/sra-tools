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

#ifndef _h_multi_writer_
#define _h_multi_writer_

#ifdef __cplusplus
extern "C" {
#endif
    
#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

struct multi_writer_block_t;

bool mw_append_block( struct multi_writer_block_t * self, const char * data, size_t len );

bool mw_expand_block( struct multi_writer_block_t * self, size_t size );

struct multi_writer_t;

struct multi_writer_t * mw_create( KDirectory * dir,
                                    const char * filename,
                                    size_t buf_size,
                                    uint32_t q_wait_time,
                                    uint32_t q_num_blocks,
                                    size_t q_block_size  );

void mw_release( struct multi_writer_t * self );

struct multi_writer_block_t * mw_get_empty_block( struct multi_writer_t * self );

bool mw_submit_block( struct multi_writer_t * self, struct multi_writer_block_t * block );

#ifdef __cplusplus
}
#endif

#endif
