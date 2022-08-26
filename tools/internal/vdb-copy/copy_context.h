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

#ifndef _h_vdb_copy_context_
#define _h_vdb_copy_context_

#ifndef _h_vdb_copy_includes_
#include "vdb-copy-includes.h"
#endif

#ifndef _h_definitions_
#include "definitions.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* ----------------------------------------------------------------------------------- */
typedef struct copy_ctx
{
    char * ptr;
} copy_ctx;
typedef copy_ctx* p_copy_ctx;


rc_t cctx_init( p_copy_ctx cctx );
void cctx_destroy( p_copy_ctx cctx );

#ifdef __cplusplus
}
#endif

#endif
