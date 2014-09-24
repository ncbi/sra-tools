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
 
%{ 
    #include "sra-shell-parse.h"
    #include "sra-shell-tokens.h"    
    
    #define YYSTYPE SRAShellToken

    /* code to populate token with internal values */
    #define YY_USER_ACTION              \
        yylval -> tokenText     = yytext;   \
        yylval -> tokenLength   = yyleng;   \
        yylval -> line_no       = yylineno; \
        yylval -> column_no     = yyextra -> column; \
        yyextra -> column += yyleng; \
        yyextra -> length += yyleng; \
        yyextra -> lastToken = yylval; 

    #define ENDLINE \
        yyextra -> column=1;\
        return shENDLINE;

    #define YY_INPUT(buf, result, max_size) \
        result = yyextra->input(yyextra, buf, max_size)

    #define ECHO

    static void SRAShell_fatal_error(yyconst char* msg);
    #define YY_FATAL_ERROR(msg) SRAShell_fatal_error(msg)
    
%}

%option never-interactive nounistd yylineno reentrant bison-bridge stack noyywrap

%option prefix="SRAShell"
%option extra-type="SRAShellParseBlock*"

 /*%option debug*/

ws          [ \t]+
digits      [0-9]+
alphanum    [A-Za-z0-9\-_]+
eol         (\r\n|\r|\n)

%%

. { return yytext[0]; }

{eol} { ENDLINE; }

%%    

rc_t CC SRAShellScan_yylex_init(SRAShellParseBlock* sb, bool debug)
{
    if (yylex_init_extra(sb, &sb->scanner) != 0)
    {
        return RC ( rcApp, rcFile, rcParsing, rcMemory, rcExhausted );
    }       

    yyset_debug(debug ? 1 : 0, sb->scanner);

    return 0;
}

void CC SRAShellScan_yylex_destroy(SRAShellParseBlock* sb)
{
    if (sb->scanner)
    {
        yylex_destroy(sb->scanner);
    }       
    sb->scanner=0;
}

void SRAShell_fatal_error(yyconst char* msg)
{
    rc_t rc = RC ( rcApp, rcFile, rcParsing, rcError, rcUnexpected );
    LogErr(klogErr, rc, msg);
    exit(rc);  
}
