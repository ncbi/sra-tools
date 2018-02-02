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

#ifndef _h_result_
#define _h_result_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_logger_
#include "logger.h"
#endif

struct validate_result;

void destroy_validate_result( struct validate_result * self );

rc_t make_validate_result( struct validate_result ** obj );

rc_t update_seq_validate_result( struct validate_result * self, uint32_t errors );
rc_t update_prim_validate_result( struct validate_result * self, uint32_t errors );

void set_to_finish_validate_result( struct validate_result * self, uint32_t value );
void finish_validate_result( struct validate_result * self );
void wait_for_validate_result( struct validate_result * self, uint32_t wait_time );

rc_t print_validate_result( struct validate_result * self, struct logger * log );

#ifdef __cplusplus
}
#endif

#endif
