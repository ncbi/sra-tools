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
	#include "vcf-parse.h"
	#include "vcf-grammar.h"    

    #define YYSTYPE VCFToken

    /* code to populate token with internal values */
    #define YY_USER_ACTION              \
        yylval -> tokenText     = yytext; \
        yylval -> tokenStart    = yyextra->offset; \
        yylval -> tokenLength	= yyleng;	\
        yylval -> line_no		= yylineno;	\
        yylval -> column_no		= yyextra -> column; \
        yyextra -> lastToken = yylval; \
        yyextra -> offset += yyleng; \
        yyextra -> column += yyleng; 

    #define ENDLINE \
        yyextra -> column=1; \
        return vcfENDLINE;

    #define YY_INPUT(buf, result, max_size) \
        result = yyextra->input(yyextra, buf, max_size)

    #define ECHO

    #define YY_NO_INPUT
%}

%option never-interactive nounistd yylineno reentrant bison-bridge noyywrap

%option prefix="VCF_"
%option extra-type="VCFParseBlock*"

%option debug

alphanum    [A-Za-z0-9\-_]+

%x HEADER
%x DATA
%x META
%x META_SIMPLE
%x META_COMPOSITE

%%

^##                     { BEGIN META; }
^#                      { BEGIN HEADER; }

^[^#][^\t\r\n]*         { BEGIN DATA; return vcfDATAITEM; }

<DATA>[^\t\r\n]+        { return vcfDATAITEM; }

<META>fileformat/=      { BEGIN META_SIMPLE; return vcfMETAKEY_FORMAT; }
<META>{alphanum}/=      { BEGIN META_SIMPLE; return vcfMETAKEY; }
<META>{alphanum}/=<     { BEGIN META_COMPOSITE; return vcfMETAKEY; }

<META_SIMPLE>
{
    [^=\r\n]+       { return vcfMETAVALUE; }
}
<META_COMPOSITE>
{
    {alphanum}/=      { return vcfMETAKEY; }
    [^=<,>\r\n]+      { return vcfMETAVALUE; }
}

<*>
{
    \t                          
    \n          { BEGIN 0; ENDLINE; }
    \r\n        { BEGIN 0; ENDLINE; }
    \r/[^\n]    { BEGIN 0; ENDLINE; }
    .           { return yytext[0]; }
}

<HEADER>[^\t\r\n]+      { return vcfHEADERITEM; }

%%    

bool VCFScan_yylex_init(VCFParseBlock* sb, bool debug)
{
	if (yylex_init_extra(sb, &sb->scanner) != 0)
	{
        return false;
		/*return RC ( rcKFG, rcMgr, rcParsing, rcMemory, rcExhausted );*/
	}		

    sb->offset = 0;
    
    sb->lastToken = NULL;
    sb->column = 1;
    
	yyset_debug(debug ? 1 : 0, sb->scanner);

	return true;
}

void VCFScan_yylex_destroy(VCFParseBlock* sb)
{
	if (sb->scanner)
	{
		yylex_destroy(sb->scanner);
	}		
	sb->scanner=0;
}
