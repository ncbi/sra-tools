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

#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <klib/log.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* open cursor
 *  open ID;
 */
static
rc_t kqsh_open_cursor ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    rc_t rc;
    KSymbol *sym = t -> sym;

    if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    rc = _VCursorOpen ( sym -> u . obj );
    if ( rc != 0 )
    {
        PLOGERR ( klogInt,  (klogInt, rc,
                  "failed to open cursor '$(name)' ( $(addr) )"
                  , "name=%.*s,addr=0x%zX"
                  , ( int ) sym -> name . size, sym -> name . addr
                             , ( size_t ) sym -> u . obj ));
    }
    else if ( interactive )
    {
        kqsh_printf ( "opened cursor '%N' ( %p )\n"
                      , sym, sym -> u . obj );
    }

    return rc;
}

static
rc_t kqsh_open_row ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    rc_t rc = 0;
    KSymbol *curs;
    int64_t row_id;
    bool has_row_id = false;
    bool negate = false;

    switch ( next_token ( tbl, src, t ) -> id )
    {
    case eMinus:
        negate = true;
    case ePlus:
        next_token ( tbl, src, t );
        break;
    }

    switch ( t -> id )
    {
    case eHex:
        if ( negate )
            LOGMSG ( klogWarn, "negative hex integer" );
    case eDecimal:
    case eOctal:
        rc = KTokenToI64 ( t, & row_id );
        if ( rc != 0 )
            return expected ( t, klogErr, "integer row id" );
        if ( negate )
            row_id = - row_id;
        has_row_id = true;
        next_token ( tbl, src, t );
        break;
    default:
        if ( negate )
            return expected ( t, klogErr, "integer row id" );
    }

    if ( t -> id == kw_on )
        next_token ( tbl, src, t );

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

    if ( has_row_id )
    {
        switch ( curs -> type )
        {
        case obj_VCursor:
            rc = _VCursorSetRowId ( curs -> u . obj, row_id );
            break;
        }
        if ( rc != 0 )
        {
            PLOGERR ( klogInt,  (klogInt, rc,
                      "failed to open row $(id) on cursor '$(name)' ( $(addr) )"
                      , "id=%ld,name=%.*s,addr=0x%zX"
                      , row_id
                      , ( int ) curs -> name . size, curs -> name . addr
                      , ( size_t ) curs -> u . obj ));
            return rc;
        }
    }

    switch ( curs -> type )
    {
    case obj_VCursor:
        rc = _VCursorOpenRow ( curs -> u . obj );
        break;
    }

    if ( rc != 0 )
    {
        PLOGERR ( klogInt,  (klogInt, rc,
                  "failed to open row on cursor '$(name)' ( $(addr) )"
                  , "name=%.*s,addr=0x%zX"
                  , ( int ) curs -> name . size, curs -> name . addr
                  , ( size_t ) curs -> u . obj ));
    }
    else if ( interactive )
    {
        if ( has_row_id )
        {
            kqsh_printf ( "opened row %ld on cursor '%N' ( %p )\n"
                          , row_id, curs, curs -> u . obj );
        }
        else
        {
            kqsh_printf ( "opened row on cursor '%N' ( %p )\n"
                          , curs, curs -> u . obj );
        }
    }

    return rc;
}

/* open
 */
rc_t kqsh_open ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    switch ( t -> id )
    {
    case kw_kdb:
    case kw_vdb:
    case kw_sra:
        /* handle open manager as a load library */
        return kqsh_load ( tbl, src, t );

    case kw_row:
        return kqsh_open_row ( tbl, src, t );

    case obj_VCursor:
        return kqsh_open_cursor ( tbl, src, t );
    }

    return expected ( t, klogErr, "manager or cursor or row" );
}
