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


/* alter cursor
 *  'alter <cursor> add column [ ( <typedecl> ) ] NAME;'
 *
 *  all data after the 'column' keyword are gathered into
 *  ( typedecl, name ) pairs. we are going to allow comma
 *  separation, but otherwise just gather everything up to
 *  the semi-colon.
 */
static
rc_t kqsh_alter_cursor_add_column ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *cursor )
{
    rc_t rc;

    do
    {
        size_t i;
        uint32_t idx;
        char coldecl [ 256 ];

        for ( i = 0; i < sizeof coldecl - 1; )
        {
            /* end of column */
            if ( t -> id == eSemiColon || t -> id == eComma )
                break;

            /* accumulate */
            i += string_copy ( & coldecl [ i ],
                sizeof coldecl - i, t -> str . addr, t -> str . size );

            next_token ( tbl, src, t );
        }

        /* the name needs to fit in our buffer */
        if ( i == sizeof coldecl - 1 )
        {
            rc = RC ( rcExe, rcCursor, rcUpdating, rcName, rcExcessive );
            if ( interactive )
            {
                kqsh_printf ( "this is really hard to believe, but you managed to request\n"
                              "a column with a %u byte expression. please stop trying to abuse me.\n",
                              ( unsigned int ) i );
            }
            else
            {
                LOGERR ( klogErr, rc, "failed to add column to cursor" );
            }
            return rc;
        }

        /* perform the task */
        rc = _VCursorAddColumn ( cursor -> u . obj, & idx, coldecl );
        if ( rc != 0 )
        {
            PLOGERR ( klogErr,  (klogErr, rc, "cannot add column '$(expr)' to cursor '$(curs)'",
                      "expr=%s,curs=%.*s"
                      , coldecl
                                 , ( int ) cursor -> name . size, cursor -> name . addr));
        }

        else if ( interactive )
        {
            kqsh_printf ( "added column '%s' ( idx %u ) to cursor '%N' ( %p )\n"
                          , coldecl
                          , idx
                          , cursor
                          , cursor -> u . obj
                );
        }
    }
    while ( t -> id == eComma );

    if ( t -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    return 0;
}

static
rc_t kqsh_alter_cursor_add ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *cursor )
{
    switch ( t -> id )
    {
    case kw_column:
        return kqsh_alter_cursor_add_column ( tbl, src, next_token ( tbl, src, t ), cursor );
    }

    return expected ( t, klogErr, "column" );
}

static
rc_t kqsh_alter_cursor ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *cursor )
{
    switch ( t -> id )
    {
    case kw_add:
        return kqsh_alter_cursor_add ( tbl, src, next_token ( tbl, src, t ), cursor );
    }

    return expected ( t, klogErr, "add" );
}

/* alter schema add path
 *  'alter <schema> add include [ path ] STRING'
 *  'alter <schema> add path STRING'
 */
static
rc_t kqsh_alter_schema_add_path ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym )
{
    rc_t rc;
    String path;
    struct VSchema *schema = ( void* ) sym -> u . obj;

    switch ( t -> id )
    {
    case eString:
    case eEscapedString:
        path = t -> str;
        path . addr += 1;
        path . size -= 2;
        break;

    case eUntermString:
    case eUntermEscapedString:
        CONST_STRING ( & t -> str, "<unterminated string>" );
    default:
        return expected ( t, klogErr, "path string" );
    }

    /* consume semi-colon */
    if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    rc = _VSchemaAddIncludePath ( schema, "%.*s", ( int ) path . size, path . addr );
    if ( rc != 0 )
    {
        PLOGERR ( klogErr,  (klogErr, rc, "cannot add search path '$(path)' to schema '$(name)'",
                  "path=%.*s,name=%.*s"
                  , ( int ) path . size, path . addr
                             , ( int ) sym -> name . size, sym -> name . addr));
    }
    else if ( interactive )
    {
        kqsh_printf ( "added search path '%S' to schema '%N' ( %p )\n",
                      & path, sym, schema );
    }

    return rc;
}

/* alter schema add text
 *  'alter <schema> add text STRING'
 */
static
rc_t kqsh_alter_schema_add_text ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym )
{
    rc_t rc;
    String text;
    struct VSchema *schema = ( void* ) sym -> u . obj;

    switch ( t -> id )
    {
    case eString:
    case eEscapedString:
        text = t -> str;
        text . addr += 1;
        text . size -= 2;
        break;

    case eUntermString:
    case eUntermEscapedString:
        CONST_STRING ( & t -> str, "<unterminated string>" );
    default:
        return expected ( t, klogErr, "schema text string" );
    }

    /* consume semi-colon */
    if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    /* parse text */
    rc = _VSchemaParseText ( schema, "kqsh-console", text . addr, text . size );
    if ( rc != 0 )
    {
        PLOGERR ( klogErr,  (klogErr, rc, "cannot add text into schema '$(name)'",
                             "name=%.*s", ( int ) sym -> name . size, sym -> name . addr));
    }
    else if ( interactive )
    {
        kqsh_printf ( "added text to schema '%N' ( %p )\n", sym, schema );
    }

    return rc;
}

/* alter schema add
 */
static
rc_t kqsh_alter_schema_add ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *schema )
{
    switch ( t -> id )
    {
    case kw_include:
        if ( next_token ( tbl, src, t ) -> id == kw_path )
    case kw_path:
            next_token ( tbl, src, t );
        return kqsh_alter_schema_add_path ( tbl, src, t, schema );
    case kw_text:
        return kqsh_alter_schema_add_text ( tbl, src, next_token ( tbl, src, t ), schema );
    }

    return expected ( t, klogErr, "include, path or text" );
}


/* alter schema load
 *  'alter <schema> load STRING'
 */
static
rc_t kqsh_alter_schema_load ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *sym )
{
    rc_t rc;
    String path;
    struct VSchema *schema = ( void* ) sym -> u . obj;

    switch ( t -> id )
    {
    case eString:
    case eEscapedString:
        path = t -> str;
        path . addr += 1;
        path . size -= 2;
        break;

    case eUntermString:
    case eUntermEscapedString:
        CONST_STRING ( & t -> str, "<unterminated string>" );
    default:
        return expected ( t, klogErr, "path string" );
    }

    /* consume semi-colon */
    if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    rc = _VSchemaParseFile ( schema, "%.*s", ( int ) path . size, path . addr );
    if ( rc != 0 )
    {
        PLOGERR ( klogErr,  (klogErr, rc, "cannot load file '$(path)' into schema '$(name)'",
                  "path=%.*s,name=%.*s"
                  , ( int ) path . size, path . addr
                             , ( int ) sym -> name . size, sym -> name . addr));
    }
    else if ( interactive )
    {
        kqsh_printf ( "loaded file '%S' into schema '%N' ( %p )\n",
                      & path, sym, schema );
    }

    return rc;
}

/* alter schema
 */
static
rc_t kqsh_alter_schema ( KSymTable *tbl, KTokenSource *src, KToken *t, KSymbol *schema )
{
    switch ( t -> id )
    {
    case kw_add:
        return kqsh_alter_schema_add ( tbl, src, next_token ( tbl, src, t ), schema );
    case kw_load:
        return kqsh_alter_schema_load ( tbl, src, next_token ( tbl, src, t ), schema );
    }

    return expected ( t, klogErr, "add or load" );
}


/* alter
 */
rc_t kqsh_alter ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    KSymbol *sym = t -> sym;

    switch ( t -> id )
    {
    case kw_cursor:
        if ( next_token ( tbl, src, t ) -> id != obj_VCursor )
            break;
        sym = t -> sym;
    case obj_VCursor:
        return kqsh_alter_cursor ( tbl, src, next_token ( tbl, src, t ), sym );

    case kw_schema:
        if ( next_token ( tbl, src, t ) -> id != obj_VSchema )
            break;
        sym = t -> sym;
    case obj_VSchema:
        return kqsh_alter_schema ( tbl, src, next_token ( tbl, src, t ), sym );
    }

    return expected ( t, klogErr, "cursor or schema" );
}
