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

#ifndef _h_lookup_writer_
#define _h_lookup_writer_

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

#ifndef _h_index_
#include "index.h"
#endif

struct lookup_writer_t;

void release_lookup_writer( struct lookup_writer_t * writer );

rc_t make_lookup_writer( KDirectory *dir, struct index_writer_t * idx, struct lookup_writer_t ** writer,
                         size_t buf_size, const char * fmt, ... );

/* used in merge_sorter.c and lookup_writer.c */
rc_t write_packed_to_lookup_writer( struct lookup_writer_t * writer,
            uint64_t key, const String * bases_as_packed_4na );

#ifdef __cplusplus
}
#endif

#endif
