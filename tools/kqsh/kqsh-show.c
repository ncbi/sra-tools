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

#include "kqsh-priv.h"
#include "kqsh-tok.h"
#include "kqsh-kdb.h"
#include "kqsh-vdb.h"

#include <vdb/schema.h>
#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <klib/log.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/*--------------------------------------------------------------------------
 * KDBManager
 */

static
rc_t kqsh_show_kmgr_version ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym )
{
    rc_t rc;
    uint32_t vers;

    if ( t -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    rc = _KDBManagerVersion ( sym -> u . obj, & vers );
    if ( rc == 0 )
    {
        if ( ( vers & 0xFFFF ) != 0 )
            kqsh_printf ( "kdb manager '%N' v%u.%u.%u\n", sym, vers >> 24, ( vers >> 16 ) & 0xFF, vers & 0xFFFF );
        else
            kqsh_printf ( "kdb manager '%N' v%u.%u\n", sym, vers >> 24, ( vers >> 16 ) & 0xFF );
    }
    else
    {
        PLOGERR ( klogInt,  (klogInt, rc, "failed to obtain version of kdb manager '$(name)'",
                             "name=%.*s", ( int ) sym -> name . size, sym -> name . addr ));
    }

    return rc;
}

static
rc_t kqsh_show_kmgr ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym )
{
    switch ( t -> id )
    {
    case kw_version:
        return kqsh_show_kmgr_version ( tbl, src, next_token ( tbl, src, t ), sym );
    }

    return expected ( t, klogErr, "version" );
}

/*--------------------------------------------------------------------------
 * VDBManager
 */

static
rc_t kqsh_show_vmgr_version ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym )
{
    rc_t rc;
    uint32_t vers;

    if ( t -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    rc = _VDBManagerVersion ( sym -> u . obj, & vers );
    if ( rc == 0 )
    {
        if ( ( vers & 0xFFFF ) != 0 )
            kqsh_printf ( "vdb manager '%N' v%u.%u.%u\n", sym, vers >> 24, ( vers >> 16 ) & 0xFF, vers & 0xFFFF );
        else
            kqsh_printf ( "vdb manager '%N' v%u.%u\n", sym, vers >> 24, ( vers >> 16 ) & 0xFF );
    }
    else
    {
        PLOGERR ( klogInt,  (klogInt, rc, "failed to obtain version of vdb manager '$(name)'",
                             "name=%.*s", ( int ) sym -> name . size, sym -> name . addr ));
    }

    return rc;
}

static
rc_t kqsh_show_vmgr ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym )
{
    switch ( t -> id )
    {
    case kw_version:
        return kqsh_show_vmgr_version ( tbl, src, next_token ( tbl, src, t ), sym );
    }

    return expected ( t, klogErr, "version" );
}

/*--------------------------------------------------------------------------
 * VSchema
 */

static
rc_t CC kqsh_flush_schema ( void *f, const void *buffer, size_t bsize )
{
    size_t num_writ, total;

    for ( total = 0; total < bsize; total += num_writ )
    {
        num_writ = fwrite ( buffer, 1, bsize, f );
        if ( num_writ == 0 )
            return RC ( rcExe, rcFile, rcWriting, rcNoObj, rcUnknown );
    }

    return 0;
}

static
rc_t kqsh_dump_schema ( KSymbol *sym, uint32_t mode, const String *decl )
{
    rc_t rc;
    char buffer [ 256 ];

    if ( decl -> size != 0 )
    {
        string_copy ( buffer, sizeof buffer, decl -> addr, decl -> size );
        buffer [ sizeof buffer - 1 ] = 0;
    }

    if ( interactive )
        putchar ( '\n' );

    rc = _VSchemaDump ( sym -> u . obj, mode,
        decl -> size ? buffer : NULL, kqsh_flush_schema, stdout );

    if ( rc != 0 )
    {
        printf ( "\n\n\n***** ABORTED *****\n\n" );
        PLOGERR ( klogInt,  (klogInt, rc, "failed to dump schema '$(name)'",
                             "name=%.*s", ( int ) sym -> name . size, sym -> name . addr ));
    }

    if ( interactive )
        putchar ( '\n' );

    return rc;
}

enum VSchemaDumpClass
{
    sdcTypes = 1,
    sdcTypesets,
    sdcFormats,
    sdcConstants,
    sdcFunctions,
    sdcColumns,
    sdcTables,
    sdcDatabases
};

static
rc_t kqsh_show_schema ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym, uint32_t mode )
{
    String decl;
    CONST_STRING ( & decl, "" );

    switch ( t -> id )
    {
    case eSemiColon:
        break;
    case kw_types:
        mode |= sdcTypes << 8;
        break;
    case kw_typesets:
        mode |= sdcTypesets << 8;
        break;
    case kw_formats:
        mode |= sdcFormats << 8;
        break;
    case kw_constants:
        mode |= sdcConstants << 8;
        break;
    case kw_functions:
        mode |= sdcFunctions << 8;
        break;
    case kw_columns:
        mode |= sdcColumns << 8;
        break;
    case kw_tables:
        mode |= sdcTables << 8;
        break;
    case kw_databases:
        mode |= sdcDatabases << 8;
        break;
    case ePeriod:
        if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        {
            decl = t -> str;
            while ( next_token ( tbl, src, t ) -> id != eSemiColon )
                decl . size = t -> str . addr + t -> str . size - decl . addr;
            decl . len = string_len ( decl . addr, decl . size );
            break;
        }
    default:
        return expected ( t, klogErr, "formats or types or typesets or constants or functions or columns or tables or databases" );
    }

    return kqsh_dump_schema ( sym, mode, & decl );
}

/*--------------------------------------------------------------------------
 * kqsh
 */

/* show
 */
rc_t kqsh_show ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    uint32_t mode = sdmPrint;
    KSymbol *sym = t -> sym;

    switch ( t -> id )
    {
    case kw_manager:
        sym = next_token ( tbl, src, t ) -> sym;
        switch ( t -> id )
        {
        case obj_KDBManager:
            return kqsh_show_kmgr ( tbl, src, next_token ( tbl, src, t ), sym );
        case obj_VDBManager:
            return kqsh_show_vmgr ( tbl, src, next_token ( tbl, src, t ), sym );
        }
        break;
    case obj_KDBManager:
        return kqsh_show_kmgr ( tbl, src, next_token ( tbl, src, t ), sym );
    case obj_VDBManager:
        return kqsh_show_vmgr ( tbl, src, next_token ( tbl, src, t ), sym );

    case kw_compact:
        mode = sdmCompact;
        if ( next_token ( tbl, src, t ) -> id != kw_schema )
        {
            if ( t -> id != obj_VSchema )
                break;
            sym = t -> sym;
            return kqsh_show_schema ( tbl, src, next_token ( tbl, src, t ), sym, mode );
        }
        /* no break */
    case kw_schema:
        if ( next_token ( tbl, src, t ) -> id != obj_VSchema )
            break;
        sym = t -> sym;
    case obj_VSchema:
        return kqsh_show_schema ( tbl, src, next_token ( tbl, src, t ), sym, mode );
    }

    return expected ( t, klogErr, "object" );
}

