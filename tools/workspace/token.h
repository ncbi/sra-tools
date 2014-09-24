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

#ifndef _h_token_
#define _h_token_

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Token Token;
struct Token
{
    /* token value ( if any ) */
    union
    {
        /* yytext */
        const char *c;

        /* unsigned integer */
        uint64_t u;

        /* version number */
        ver_t v;

        /* datetime */
        KTime_t t;

    } val;

    /* text length */
    uint32_t len;

    /* location of token in source */
    uint32_t lineno;

    /* token type - defined by bison/yacc */
    uint16_t type;

    /* value variant */
    uint16_t var;
};


/* token value variant codes */
enum
{
    val_none,   /* no value                                              */
    val_u64,    /* val.u is valid                                        */
    val_vers,   /* val.v is valid                                        */
    val_time,   /* val.t is valid                                        */
    val_txt,    /* val.c is valid and NUL terminated                     */
    val_quot,   /* val.c has a fully quoted NUL terminated string        */
    val_esc     /* val.c has a fully quoted string with escape sequences */
};

/* conversion functions
 *  returns false if there were problems, e.g. integer overflow
 *  returns a typed value in "val" on success
 *  "text" and "len" are taken from a volatile text buffer
 */
bool toolkit_dtoi ( uint64_t *val, const char *text, int len );
bool toolkit_atov ( ver_t *val, const char *text, int len );
bool toolkit_atotm ( KTime_t *val, const char *text, int len );
Token toolkit_itov ( const Token *i );
Token toolkit_rtov ( const Token *r );

#ifdef __cplusplus
}
#endif

#endif /* _h_token_ */
