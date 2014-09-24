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

#ifndef _h_sra_sort_defs_
#define _h_sra_sort_defs_

#ifndef _h_kfc_callconv_
#include <kfc/callconv.h>
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

/* prevent inclusion of <kfc/defs.h> */
#define _h_kfc_defs_ 1


/*--------------------------------------------------------------------------
 * NAME_VERS
 *  synthesize versioned type and message names
 */
#define NAME_VERS( name, maj_vers ) \
    MAKE_NAME_VERS1 ( name, maj_vers )
#define MAKE_NAME_VERS1( name, maj_vers ) \
    MAKE_NAME_VERS2 ( name, maj_vers )
#define MAKE_NAME_VERS2( name, maj_vers ) \
    name ## _v ## maj_vers

typedef uint32_t rc_t, ver_t;

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <assert.h>

#if _DEBUGGING &&! defined SRA_SORT_CHECK_NULL_SELF
#define SRA_SORT_CHECK_NULL_SELF 1
#endif

#define USE_OLD_KSORT 0


/*--------------------------------------------------------------------------
 * vdb-3 items missing from klib/defs.h
 */
typedef uint32_t caps_t;
typedef struct ctx_t ctx_t;
#define STRINGIZE( str ) # str
#define STRINGIZE_DEFINE( def ) STRINGIZE ( def )


/* POLY_DISPATCH
 *  dispatch a polymorphic message
 */
#if  SRA_SORT_CHECK_NULL_SELF

void null_self_error ( const ctx_t *ctx, uint32_t lineno, const char *msg );

#define POLY_DISPATCH_VOID( msg, self, cast_expr, ctx, ... ) \
    ( ( ( self ) == NULL ) ? null_self_error ( ctx, __LINE__, # msg ) : \
      ( * ( self ) -> vt -> msg ) ( ( cast_expr* ) ( self ), ctx, ## __VA_ARGS__ ) )

#define POLY_DISPATCH_INT( msg, self, cast_expr, ctx, ... ) \
    ( ( ( self ) == NULL ) ? ( null_self_error ( ctx, __LINE__, # msg ), 0 ) : \
      ( * ( self ) -> vt -> msg ) ( ( cast_expr* ) ( self ), ctx, ## __VA_ARGS__ ) )

#define POLY_DISPATCH_PTR( msg, self, cast_expr, ctx, ... ) \
    ( ( ( self ) == NULL ) ? ( null_self_error ( ctx, __LINE__, # msg ), NULL ) : \
      ( * ( self ) -> vt -> msg ) ( ( cast_expr* ) ( self ), ctx, ## __VA_ARGS__ ) )

#else

#define POLY_DISPATCH_VOID( msg, self, cast_expr, ctx, ... ) \
    ( * ( self ) -> vt -> msg ) ( ( cast_expr* ) ( self ), ctx, ## __VA_ARGS__ )
#define POLY_DISPATCH_INT( msg, self, cast_expr, ctx, ... ) \
    ( * ( self ) -> vt -> msg ) ( ( cast_expr* ) ( self ), ctx, ## __VA_ARGS__ )
#define POLY_DISPATCH_PTR( msg, self, cast_expr, ctx, ... ) \
    ( * ( self ) -> vt -> msg ) ( ( cast_expr* ) ( self ), ctx, ## __VA_ARGS__ )

#endif

#endif /* _h_sra_sort_defs_ */
