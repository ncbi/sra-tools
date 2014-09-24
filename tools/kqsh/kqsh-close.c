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
#include "kqsh-sra.h"

#include <klib/container.h>
#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <klib/log.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* close row
 */
static
rc_t kqsh_close_row ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    rc_t rc = 0;
    KSymbol *curs;

    /* accept optional "on" keyword */
    if ( next_token ( tbl, src, t ) -> id == kw_on )
        next_token ( tbl, src, t );

    /* expect cursor object */
    switch ( t -> id )
    {
    case obj_VCursor:
        curs = t -> sym;
        break;
    default:
        return expected ( t, klogErr, "cursor" );
    }

    if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    switch ( curs -> type )
    {
    case obj_VCursor:
        rc = _VCursorCloseRow ( curs -> u . obj );
        break;
    }

    if ( rc != 0 )
    {
        PLOGERR ( klogInt,  (klogInt, rc,
                  "failed to close row on cursor '$(name)' ( $(addr) )"
                  , "name=%.*s,addr=0x%zX"
                  , ( int ) curs -> name . size, curs -> name . addr
                  , ( size_t ) curs -> u . obj ));
    }
    else if ( interactive )
    {
        kqsh_printf ( "closed row on cursor '%N' ( %p )\n"
                      , curs, curs -> u . obj );
    }

    return rc;
}


/* whackobj
 *  whacks created/opened objects
 */
void CC kqsh_whackobj ( BSTNode *n, void *ignore )
{
    KSymbol *sym = ( KSymbol* ) n;

    switch ( sym -> type )
    {
    case obj_KDBManager:
        _KDBManagerRelease ( sym -> u . obj );
        break;
    case obj_VDBManager:
        _VDBManagerRelease ( sym -> u . obj );
        break;
    case obj_SRAManager:
        _SRAMgrRelease ( sym -> u . obj );
        break;
    case obj_VSchema:
        _VSchemaRelease ( sym -> u . obj );
        break;

    case obj_KTable:
        _KTableRelease ( sym -> u . obj );
        break;
    case obj_VTable:
        _VTableRelease ( sym -> u . obj );
        break;

    case obj_VCursor:
        _VCursorRelease ( sym -> u . obj );
        break;
    }

    KSymbolWhack ( & sym -> n, ignore );
}

/* close
 */
rc_t kqsh_close ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    KSymbol *sym;

    /* handle close row */
    if ( t -> id == kw_row )
        return kqsh_close_row ( tbl, src, t );

    /* expect normal object */
    sym = t -> sym;

    if ( sym == NULL || t -> id < rsrv_first )
        return expected ( t, klogErr, "object id" );

    if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    if ( sym -> type >= obj_first )
    {
        if ( interactive )
        {
            const char *type;
            switch ( sym -> type )
            {
            case obj_KDBManager:
                type = "kdb manager"; break;
            case obj_VDBManager:
                type = "vdb manager"; break;
            case obj_VSchema:
                type = "schema"; break;
            case obj_KTable:
            case obj_VTable:
                type = "table"; break;
            case obj_VCursor:
                type = "cursor"; break;
            default:
                type = "object";
            }

            kqsh_printf ( "closing %s '%N' ( %p )\n", type, sym, sym -> u . obj );
        }

        KSymTableRemoveSymbol ( tbl, sym );
        kqsh_whackobj ( & sym -> n, NULL );
    }
    else if ( interactive )
    {
        kqsh_printf ( "reserved object '%N' is not open\n", sym );
    }

    return 0;
}
