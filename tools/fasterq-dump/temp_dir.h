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

#ifndef _h_temp_dir_
#define _h_temp_dir_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

struct temp_dir;

void destroy_temp_dir( struct temp_dir * self );

rc_t make_temp_dir( struct temp_dir ** obj, const char * requested, KDirectory * dir );

const char * get_temp_dir( struct temp_dir * self );

rc_t generate_lookup_filename( const struct temp_dir * self, char * dst, size_t dst_size );

rc_t generate_bg_sub_filename( const struct temp_dir * self, char * dst, size_t dst_size, uint32_t product_id );

rc_t generate_bg_merge_filename( const struct temp_dir * self, char * dst, size_t dst_size, uint32_t product_id );

rc_t make_joined_filename( const struct temp_dir * self, char * dst, size_t dst_size,
                           const char * accession, uint32_t id );

rc_t remove_temp_dir( const struct temp_dir * self, KDirectory * dir );

#ifdef __cplusplus
}
#endif

#endif
