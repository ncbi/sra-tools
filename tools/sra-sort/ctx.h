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

#ifndef _h_sra_sort_ctx_
#define _h_sra_sort_ctx_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct Caps;


/*--------------------------------------------------------------------------
 * file entry
 *  modified from vdb-3 to not place requirements on Makefile
 */
#if ! defined __mod__ && ! defined __file__
#define FILE_ENTRY( __file_name__ )                                     \
    static const char __mod__ [] = "tools/sra-sort";                    \
    static const char __file__ [] = STRINGIZE_DEFINE ( __file_name__ )
#else
#define FILE_ENTRY( __file_name__ ) /* already defined */
#endif


/*--------------------------------------------------------------------------
 * ctx_info_t
 */
typedef struct ctx_info_t ctx_info_t;
struct ctx_info_t
{
    const char *mod;
    const char *file;
    const char *func;
};

#define DECLARE_CTX_INFO() \
    static ctx_info_t ctx_info = { __mod__, __file__, __func__ }


/*--------------------------------------------------------------------------
 * ctx_t
 *  modified from vdb-3
 */
struct ctx_t
{
    struct Caps const *caps;
    const ctx_t *caller;
    const ctx_info_t *info;
    volatile rc_t rc;
};


/* INIT
 *  initialize local context block
 */
static __inline__
const ctx_t ctx_init ( ctx_t *new_ctx, const ctx_t **ctxp, const ctx_info_t *info )
{
    const ctx_t *ctx = * ctxp;
    ctx_t local_ctx = { ctx -> caps, ctx, info };
    * ctxp = new_ctx;
    return local_ctx;
}


/* FUNC_ENTRY
 */
#define FUNC_ENTRY( ctx )                                               \
    DECLARE_CTX_INFO ();                                                \
    ctx_t local_ctx = ctx_init ( & local_ctx, & ( ctx ), & ctx_info )


/* POP_CTX
 *  intended to have VERY specific usage
 */
#define POP_CTX( ctx ) \
    ctx = ctx -> caller


#endif
