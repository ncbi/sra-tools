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

#ifndef _h_sra_sort_except_
#define _h_sra_sort_except_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif


/*--------------------------------------------------------------------------
 * exception-related macros
 */


/* ANNOTATE
 *  make some annotation
 *  but not an error
 */
void Annotate ( const ctx_t *ctx, uint32_t lineno, KLogLevel level, const char *msg, ... );
#define ANNOTATE( msg, ... ) \
    Annotate ( ctx, __LINE__, klogInfo, msg, ## __VA_ARGS__ )


/* WARN
 *  make some annotation
 *  but not an error
 */
#define WARN( msg, ... ) \
    Annotate ( ctx, __LINE__, klogWarn, msg, ## __VA_ARGS__ )


/* INFO_ERROR
 *  make an annotation
 *  record an informational error as an rc_t
 */
void Error ( const ctx_t *ctx, uint32_t lineno, KLogLevel level, rc_t rc, const char *msg, ... );
#define INFO_ERROR( rc, msg, ... ) \
    Error ( ctx, __LINE__, klogInfo, rc, msg, ## __VA_ARGS__ )


/* ERROR
 *  make an annotation
 *  record an error as an rc_t
 */
#ifdef ERROR
#undef ERROR
#endif
#define ERROR( rc, msg, ... ) \
    Error ( ctx, __LINE__, klogErr, rc, msg, ## __VA_ARGS__ )


/* INTERNAL_ERROR
 *  make an annotation
 *  record an internal error as an rc_t
 */
#define INTERNAL_ERROR( rc, msg, ... ) \
    Error ( ctx, __LINE__, klogInt, rc, msg, ## __VA_ARGS__ )


/* SYSTEM_ERROR
 *  make an annotation
 *  record a system error as an rc_t
 */
#define SYSTEM_ERROR( rc, msg, ... ) \
    Error ( ctx, __LINE__, klogSys, rc, msg, ## __VA_ARGS__ )


/* ABORT
 *  make an annotation
 *  record an error as an rc_t
 *  exit process
 */
void Abort ( const ctx_t *ctx, uint32_t lineno, rc_t rc, const char *msg, ... );
#define ABORT( rc, msg, ... )                        \
    Abort ( ctx, __LINE__, rc, msg, ## __VA_ARGS__ )


/* FAILED
 *  a test of rc within ctx_t
 */
#ifdef FAILED
#undef FAILED
#endif
#define FAILED() \
    ( ctx -> rc != 0 )


/* TRY
 *  another C language "try" macro
 */
#define TRY( expr ) \
    expr; \
    if ( ! FAILED () )


/* CATCH
 *  attempts to catch rc on certain types
 */
#define CATCH( obj_code, state_code ) \
    else if ( GetRCObject ( ctx -> rc ) == obj_code && GetRCState ( ctx -> rc ) == state_code )
#define CATCH_STATE( state_code ) \
    else if ( GetRCState ( ctx -> rc ) == state_code )
#define CATCH_OBJ( obj_code ) \
    else if ( GetRCObject ( ctx -> rc ) == obj_code )
#define CATCH_ALL() \
    else

/* ON_FAIL
 */
#define ON_FAIL( expr ) \
    expr; \
    if ( FAILED () )


/* CLEAR
 *  clears annotation and error
 */
void ClearAnnotation ( const ctx_t *ctx );
#define CLEAR() \
    ClearAnnotation ( ctx )


/* CLEAR_ERR
 *  clears error, leaving any annotation
 */
void ClearError ( const ctx_t *ctx );
#define CLEAR_ERR() \
    ClearError ( ctx )


#endif
