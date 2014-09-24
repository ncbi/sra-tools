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

#include "caps.h"
#include "ctx.h"
#include "except.h"
#include "status.h"

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/status.h>
#include <klib/printf.h>

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

FILE_ENTRY ( except );


/*--------------------------------------------------------------------------
 * exception-related activities
 */

static
rc_t log_msg ( const ctx_t *ctx, uint32_t lineno, KLogLevel lvl, const char *msg )
{
#if _DEBUGGING
    const ctx_info_t *ctx_info = ctx -> info;
    return pLogMsg ( lvl, "$(mod)/$(file).c:$(line): $(func): $(msg)",
                     "mod=%s,file=%s,line=%u,func=%s,msg=%s",
                     ctx_info -> mod,
                     ctx_info -> file,
                     lineno,
                     ctx_info -> func,
                     msg );
#else
    return LogMsg ( lvl, msg );
#endif
}

static
rc_t log_err ( const ctx_t *ctx, uint32_t lineno, KLogLevel lvl, rc_t rc, const char *msg )
{
#if _DEBUGGING
    const ctx_info_t *ctx_info = ctx -> info;
    return pLogErr ( lvl, rc, "$(mod)/$(file).c:$(line): $(func): $(msg)",
                     "mod=%s,file=%s,line=%u,func=%s,msg=%s",
                     ctx_info -> mod,
                     ctx_info -> file,
                     lineno,
                     ctx_info -> func,
                     msg );
#else
    return LogErr ( lvl, rc, msg );
#endif
}

static
rc_t status_msg ( const ctx_t *ctx, uint32_t lineno, uint32_t level, const char *msg )
{
#if _DEBUGGING
    const ctx_info_t *ctx_info = ctx -> info;
    return KStsMsg ( "%s/%s.c:%u: %s: [ %u ] %s",
                     ctx_info -> mod,
                     ctx_info -> file,
                     lineno,
                     ctx_info -> func,
                     level,
                     msg );
#else
    return KStsMsg ( "%s", msg );
#endif
}


/* Annotate
 *  make some annotation
 *  but not an error
 */
void Annotate ( const ctx_t *ctx, uint32_t lineno, KLogLevel level, const char *msg, ... )
{
    rc_t rc2;
    size_t num_writ;
    char buff [ 8 * 1024 ];

    va_list args;
    va_start ( args, msg );

    rc2 = string_vprintf ( buff, sizeof buff, & num_writ, msg, args );
    if ( rc2 != 0 )
        log_err ( ctx, lineno, klogInt, rc2, "Annotate failure" );
    else
        log_msg ( ctx, lineno, level, buff );

    va_end ( args );
}


/* Error
 *  make an annotation
 *  record an error as an rc_t
 */
void Error ( const ctx_t *ctx, uint32_t lineno, KLogLevel level, rc_t rc, const char *msg, ... )
{
    rc_t rc2;
    ctx_t *mctx;
    size_t num_writ;
    char buff [ 8 * 1024 ];

    va_list args;
    va_start ( args, msg );

    rc2 = string_vprintf ( buff, sizeof buff, & num_writ, msg, args );
    if ( rc2 != 0 )
        log_err ( ctx, lineno, klogInt, rc2, "Error annotation failure" );
    else
        log_err ( ctx, lineno, level, rc, buff );

    va_end ( args );

    for ( mctx = ( ctx_t* ) ctx; mctx != NULL && mctx -> rc == 0; mctx = ( ctx_t* ) mctx -> caller )
        mctx -> rc = rc;
}


/* Abort
 *  make an annotation
 *  record an error as an rc_t
 *  exit process
 */
void Abort ( const ctx_t *ctx, uint32_t lineno, rc_t rc, const char *msg, ... )
{
    rc_t rc2;
    ctx_t *mctx;
    size_t num_writ;
    char buff [ 8 * 1024 ];

    va_list args;
    va_start ( args, msg );

    rc2 = string_vprintf ( buff, sizeof buff, & num_writ, msg, args );
    if ( rc2 != 0 )
        log_err ( ctx, lineno, klogFatal, rc2, "Abort annotation failure" );
    else
        log_err ( ctx, lineno, klogFatal, rc, buff );

    va_end ( args );

    for ( mctx = ( ctx_t* ) ctx; mctx != NULL && mctx -> rc == 0; mctx = ( ctx_t* ) mctx -> caller )
        mctx -> rc = rc;

    exit ( -1 );
}


/* Clear
 *  clears annotation and error
 */
void ClearAnnotation ( const ctx_t *ctx )
{
    ClearError ( ctx );
}


/* ClearError
 *  clears error, leaving any annotation
 */
void ClearError ( const ctx_t *ctx )
{
    ctx_t *mctx;
    for ( mctx = ( ctx_t* ) ctx; mctx != NULL && mctx -> rc != 0; mctx = ( ctx_t* ) mctx -> caller )
        mctx -> rc = 0;
}


/* Status
 *  report status
 */
void Status ( const ctx_t *ctx, uint32_t lineno, uint32_t level, const char *msg, ... )
{
    if ( KStsLevelGet () >= level )
    {
        rc_t rc2;
        size_t num_writ;
        char buff [ 8 * 1024 ];

        va_list args;
        va_start ( args, msg );

        rc2 = string_vprintf ( buff, sizeof buff, & num_writ, msg, args );
        if ( rc2 != 0 )
            log_err ( ctx, lineno, klogInt, rc2, "Status failure" );
        else
            status_msg ( ctx, lineno, level, buff );

        va_end ( args );
    }
}

/* POLY_DISPATCH
 *  dispatch a polymorphic message
 */
void null_self_error ( const ctx_t *ctx, uint32_t lineno, const char *msg )
{
    rc_t rc = RC ( rcExe, rcFunction, rcResolving, rcSelf, rcNull );
    Error ( ctx, lineno, klogInt, rc, "cannot dispatch message '%s'", msg );
}
