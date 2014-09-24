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

#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/mmap.h>
#include <kapp/main.h>
#include <klib/log.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* kqsh_execute
 *
 *  'execute' PATH ';'
 */
static
rc_t kqsh_execute ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    String path;

    if ( t -> id != eString )
        return expected ( t, klogErr, "file path" );

    path = t -> str;
    path . addr += 1;
    path . size -= 2;
    path . len -= 2;

    if ( next_token ( tbl, src, t ) -> id != eSemiColon )
        return expected ( t, klogErr, ";" );

    return kqsh_exec_file ( tbl, & path );
}


/* macro for generating switch statements */
#define KQSH_CMD_KEYWORD( cmd ) \
    case kw_ ## cmd: \
        rc = kqsh_ ## cmd ( tbl, src, next_token ( tbl, src, & t ) ); \
        break

/* case for unimplemented commands */
#define UNIMPL_CMD_KEYWORD( cmd ) \
    case kw_ ## cmd: ( void ) 0

/* kqsh
 *  main loop for shell
 */
static
rc_t kqsh_parse ( KSymTable *tbl, KTokenSource *src, kqsh_stack *frame )
{
    rc_t rc;
    KToken t;

    for ( rc = 0, frame -> cmd_num = 1; rc == 0; ++ frame -> cmd_num )
    {
        /* check for signals */
        rc = Quitting ();
        if ( rc != 0 )
            break;

        /* issue prompt */
        /* the prompt should allow db.db.tbl.obj stack or 'kqsh' when empty */
        if ( interactive )
            kqsh_printf ( "\nkqsh|%F> ", frame );

        /* process input */
        next_token ( tbl, src, & t );
        if ( interactive )
            kqsh_printf ( "\n" );

        switch ( t . id )
        {
        case eEndOfInput:
            return 0;
        case eSemiColon:
            break;

        case eHash:
            while ( 1 )
            {
                KTokenizerNext ( kLineTokenizer, src, & t );
                if ( t . id == eEndOfLine || t . id == eEndOfInput )
                    break;
            }
            break;

        /* implemented commands */
        KQSH_CMD_KEYWORD ( alter );
        KQSH_CMD_KEYWORD ( close );
        KQSH_CMD_KEYWORD ( create );
        KQSH_CMD_KEYWORD ( execute );
        KQSH_CMD_KEYWORD ( open );
        KQSH_CMD_KEYWORD ( show );
        KQSH_CMD_KEYWORD ( write );

        /* unimplemented commands */
        UNIMPL_CMD_KEYWORD ( alias );
        UNIMPL_CMD_KEYWORD ( delete );
        UNIMPL_CMD_KEYWORD ( drop );
        UNIMPL_CMD_KEYWORD ( insert );
        UNIMPL_CMD_KEYWORD ( rename );
        UNIMPL_CMD_KEYWORD ( update );
        UNIMPL_CMD_KEYWORD ( use );
            if ( interactive )
                kqsh_printf ( "the %S command is not yet implemented.\n", & t . str );
            rc = RC ( rcExe, rcNoTarg, rcExecuting, rcFunction, rcIncomplete );
            break;

        /* help and quit are special
           in that they don't require a semi-colon
           and thus can't use look-ahead which might block */
        case kw_help:
            rc = kqsh_help ( tbl, src, & t );
            break;
        case kw_exit:
        case kw_quit:
            return 0;

        default:
            rc = expected ( & t, klogErr, "command" );
        }

        /* tolerate errors in interactive mode */
        if ( rc != 0 && interactive )
        {
            KTokenSourceConsume ( src );
            rc = 0;
        }
    }
    
    return rc;
}


rc_t kqsh ( KSymTable *tbl, KTokenSource *src )
{
    rc_t rc;
    BSTree scope;

    kqsh_stack frame;
    static kqsh_stack const *stk = NULL;

    frame . prev = stk;
    stk = & frame;

    BSTreeInit ( & scope );

    rc = KSymTablePushScope ( tbl, & scope );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to initialize symbol table" );
    else
    {
        rc = kqsh_parse ( tbl, src, & frame );

        KSymTablePopScope ( tbl );
        BSTreeWhack ( & scope, kqsh_whackobj, NULL );
    }

    stk = frame . prev;

    return rc;
}
