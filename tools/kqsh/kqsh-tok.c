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

#include "kqsh-tok.h"
#include "kqsh-priv.h"

#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <kfs/dyload.h>
#include <klib/log.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* next_token
 */
KToken *next_token ( struct KSymTable const *tbl, KTokenSource *src, KToken *t )
{
    if ( KTokenizerNext ( kDefaultTokenizer, src, t ) -> id == eIdent )
    {
        KSymbol *sym = KSymTableFind ( tbl, & t -> str );
        t -> sym = sym;
        if ( sym != NULL )
            t -> id = sym -> type;
    }

    return t;
}

KToken *next_shallow_token ( struct KSymTable const *tbl, KTokenSource *src, KToken *t )
{
    if ( KTokenizerNext ( kDefaultTokenizer, src, t ) -> id == eIdent || t -> id == eName )
    {
        KSymbol *sym = KSymTableFindShallow ( tbl, & t -> str );
        t -> sym = sym;
        if ( sym != NULL )
            t -> id = sym -> type;
    }

    return t;
}

/* expected
 */
rc_t expected ( const KToken *self, KLogLevel lvl, const char *expected )
{
    String eof;
    const String *str = & self -> str;

    if ( self -> id == eEndOfInput )
    {
        CONST_STRING ( & eof, "EOF" );
        str = & eof;
    }

    if ( interactive )
        kqsh_printf ( "expected '%s' but found '%S'\n", expected, str );
    else
    {
        PLOGMSG ( lvl, ( lvl, "$(file):$(lineno): "
                         "expected '$(expected)' but found '$(found)'",
                         "file=%.*s,lineno=%u,expected=%s,found=%.*s"
                         , ( int ) self -> txt -> path . size, self -> txt -> path . addr
                         , self -> lineno
                         , expected
                         , ( int ) str -> size, str -> addr ));
    }

    return RC ( rcExe, rcText, rcParsing, rcToken, rcUnexpected );
}
