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

#ifndef _h_progress_thread_
#define _h_progress_thread_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

struct bg_progress;

rc_t bg_progress_make( struct bg_progress ** bgp, uint64_t max_value, uint32_t sleep_time, uint32_t digits );
void bg_progress_update( struct bg_progress * self, uint64_t by );
void bg_progress_inc( struct bg_progress * self );
void bg_progress_get( struct bg_progress * self, uint64_t * value );
void bg_progress_set_max( struct bg_progress * self, uint64_t value );
void bg_progress_release( struct bg_progress * self );

struct bg_update;

rc_t bg_update_make( struct bg_update ** bga, uint32_t sleep_time );
void bg_update_start( struct bg_update * self, const char * caption );
void bg_update_update( struct bg_update * self, uint64_t by );
void bg_update_release( struct bg_update * self );

#ifdef __cplusplus
}
#endif

#endif
