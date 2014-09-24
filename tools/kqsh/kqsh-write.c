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

#include <kfs/directory.h>
#include <klib/log.h>
#include <klib/vector.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* write_cursor_xxx
 */
static
rc_t kqsh_write_cursor_array ( KSymTable *tbl, KTokenSource *src, KToken *t,
    const KSymbol *curs, uint32_t idx, const VTypedesc *desc,
    rc_t ( * write ) ( KSymTable*, KTokenSource*, KToken*, const KSymbol*, uint32_t, const VTypedesc* ) )
{
    rc_t rc;

    /* allow empty array */
    if ( next_token ( tbl, src, t ) -> id == eRightSquare )
        return 0;

    /* write elements */
    while ( 1 )
    {
        /* write the element */
        rc = ( * write ) ( tbl, src, t, curs, idx, desc );
        if ( rc != 0 )
            break;

        /* punctuation */
        if ( next_token ( tbl, src, t ) -> id != eComma )
            break;

        /* next element */
        next_token ( tbl, src, t );
    }

    /* expect ']' */
    if ( rc == 0 && t -> id != eRightSquare )
        rc = expected ( t, klogErr, "]" );

    return rc;
}

static
rc_t kqsh_write_cursor_bool ( KSymTable *tbl, KTokenSource *src, KToken *t,
    const KSymbol *curs, uint32_t idx, const VTypedesc *desc )
{
    if ( t -> id == eLeftSquare )
        return kqsh_write_cursor_array ( tbl, src, t, curs, idx, desc, kqsh_write_cursor_bool );

    assert ( desc -> intrinsic_bits == 8 );
    return -1;
}

static
rc_t kqsh_write_cursor_uint ( KSymTable *tbl, KTokenSource *src, KToken *t,
    const KSymbol *curs, uint32_t idx, const VTypedesc *desc )
{
    if ( t -> id == eLeftSquare )
        return kqsh_write_cursor_array ( tbl, src, t, curs, idx, desc, kqsh_write_cursor_uint );
    return -1;
}

static
rc_t kqsh_write_cursor_int ( KSymTable *tbl, KTokenSource *src, KToken *t,
    const KSymbol *curs, uint32_t idx, const VTypedesc *desc )
{
    if ( t -> id == eLeftSquare )
        return kqsh_write_cursor_array ( tbl, src, t, curs, idx, desc, kqsh_write_cursor_int );
    return -1;
}

static
rc_t kqsh_write_cursor_float ( KSymTable *tbl, KTokenSource *src, KToken *t,
    const KSymbol *curs, uint32_t idx, const VTypedesc *desc )
{
    if ( t -> id == eLeftSquare )
        return kqsh_write_cursor_array ( tbl, src, t, curs, idx, desc, kqsh_write_cursor_float );
    return -1;
}

static
rc_t kqsh_write_cursor_ascii ( KSymTable *tbl, KTokenSource *src, KToken *t,
    const KSymbol *curs, uint32_t idx, const VTypedesc *desc )
{
    rc_t rc;

    switch ( t -> id )
    {
    case eUntermString:
    case eUntermEscapedString:
        return expected ( t, klogErr, "terminated string" );
    case eString:
        rc = _VCursorWrite ( ( void* ) curs -> u . obj, idx, 8, t -> str . addr + 1, 0, t -> str . len - 2 );
        if ( rc == 0 )
            return 0;
        break;
    case eEscapedString:
        rc = 0;
        break;
    default:
        return expected ( t, klogErr, "string" );
    }

    if ( rc == 0 )
    {
        char buffer [ 256 ], *p = buffer;
        size_t size = sizeof buffer;
        if ( t -> str . size >= sizeof buffer )
        {
            p = malloc ( size = t -> str . size + 1 );
            if ( p == NULL )
                rc = RC ( rcExe, rcCursor, rcWriting, rcMemory, rcExhausted );
        }

        if ( rc == 0 )
        {
            rc = KTokenToString ( t, p, size, & size );
            if ( rc == 0 )
                rc = _VCursorWrite ( ( void* ) curs -> u . obj, idx, 8, p, 0, size );
            if ( p != buffer )
                free ( p );
        }
    }

    if ( rc != 0 )
    {
        PLOGERR ( klogInt,  (klogInt, rc,
                  "failed to write string '$(str)' to cursor '$(curs)' ( $(addr) )"
                  , "str=%.*s,curs=%.*s,addr=0x%zX"
                  , ( int ) t -> str . size, t -> str . addr
                  , ( int ) curs -> name . size, curs -> name . addr
                  , ( size_t ) curs -> u . obj ));
   }

    return rc;
}

static
rc_t kqsh_write_cursor_unicode ( KSymTable *tbl, KTokenSource *src, KToken *t,
    const KSymbol *curs, uint32_t idx, const VTypedesc *desc )
{
    if ( desc -> intrinsic_bits == 8 )
        return kqsh_write_cursor_ascii ( tbl, src, t, curs, idx, desc );

    return -1;
}

/* write_cursor
 */
static
rc_t kqsh_write_cursor ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    rc_t rc = 0;
    uint32_t idx;
    VTypedecl td;
    VTypedesc desc;

    /* have cursor object */
    KSymbol *curs = t -> sym;

    /* error if read-only */
    if ( read_only )
    {
        rc = RC ( rcExe, rcCursor, rcWriting, rcCursor, rcReadonly );
        PLOGERR ( klogInt,  (klogInt, rc,
                  "failed to write to cursor '$(curs)' ( $(addr) )"
                  , "curs=%.*s,addr=0x%zX"
                  , ( int ) curs -> name . size, curs -> name . addr
                  , ( size_t ) curs -> u . obj ));
        return rc;
    }

    /* separator */
    if ( next_token ( tbl, src, t ) -> id != ePeriod )
        return expected ( t, klogErr, ". <colname> or IDX" );

    /* get column spec */
    switch ( next_token ( tbl, src, t ) -> id )
    {
        /* take integer index */
    case eDecimal:
    case eHex:
    case eOctal:
        rc = KTokenToU32 ( t, & idx );
        if ( rc != 0 )
            return expected ( t, klogErr, "integer column index" );
        break;

        /* take column name */
    default:
        if ( t -> sym == NULL )
            return expected ( t, klogErr, "column name" );
    case eIdent:
        switch ( curs -> type )
        {
        case obj_VCursor:
            rc = _VCursorGetColumnIdx ( curs -> u . obj, & idx, "%.*s",
                ( int ) t -> str . size, t -> str . addr );
            break;
        }
        if ( rc != 0 )
        {
            PLOGERR ( klogInt,  (klogInt, rc,
                      "failed to write to column '$(col)' on cursor '$(curs)' ( $(addr) )"
                      , "col=%.*s,curs=%.*s,addr=0x%zX"
                      , ( int ) t -> str . size, t -> str . addr
                      , ( int ) curs -> name . size, curs -> name . addr
                      , ( size_t ) curs -> u . obj ));
            return rc;
        }
        break;
    }

    /* access column object */
    switch ( curs -> type )
    {
    case obj_VCursor:
        rc = _VCursorDatatype ( curs -> u . obj, idx, & td, & desc );
        break;
    }

    if ( rc != 0 )
    {
        PLOGERR ( klogInt,  (klogInt, rc,
                  "failed to access datatype of column $(idx) on cursor '$(curs)' ( $(addr) )"
                  , "idx=%u,curs=%.*s,addr=0x%zX"
                  , idx
                  , ( int ) curs -> name . size, curs -> name . addr
                  , ( size_t ) curs -> u . obj ));
        return rc;
    }

    /* move on to first token of row */
    next_token ( tbl, src, t );

    /* switch on datatype */
    switch ( desc . domain )
    {
    case vtdBool:
        rc = kqsh_write_cursor_bool ( tbl, src, t, curs, idx, & desc );
        break;
    case vtdUint:
        rc = kqsh_write_cursor_uint ( tbl, src, t, curs, idx, & desc );
        break;
    case vtdInt:
        rc = kqsh_write_cursor_int ( tbl, src, t, curs, idx, & desc );
        break;
    case vtdFloat:
        rc = kqsh_write_cursor_float ( tbl, src, t, curs, idx, & desc );
        break;
    case vtdAscii:
        rc = kqsh_write_cursor_ascii ( tbl, src, t, curs, idx, & desc );
        break;
    case vtdUnicode:
        rc = kqsh_write_cursor_unicode ( tbl, src, t, curs, idx, & desc );
        break;
    }

    /* expect semicolon */
    if ( rc == 0 )
    {
        if ( next_token ( tbl, src, t ) -> id != eSemiColon )
            return expected ( t, klogErr, ";" );
    }

    return rc;
}


/* write
 */
rc_t kqsh_write ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    switch ( t -> id )
    {
    case obj_VCursor:
        return kqsh_write_cursor ( tbl, src, t );
    }

    return expected ( t, klogErr, "cursor" );
}
