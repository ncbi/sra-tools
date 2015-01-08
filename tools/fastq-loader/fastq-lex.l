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
    #include <sysalloc.h>   
    #include <ctype.h>
    
    #include <klib/rc.h>
    #include <klib/log.h>

    #include "fastq-parse.h"
    #include "fastq-tokens.h"    
    
    #define YYSTYPE FASTQToken

    /* code to populate token with internal values */
	static void ConsumeToken(FASTQToken* token, FASTQParseBlock* pb);
    #define YY_USER_ACTION ConsumeToken(yylval, yyextra);
	
    #define ENDLINE \
        yyextra -> column=1;\
        return fqENDLINE;

    #define YY_INPUT(buf, result, max_size) \
        result = yyextra->input(yyextra, buf, max_size)

    #define ECHO

    static void FASTQ_fatal_error(yyconst char* msg);
    #define YY_FATAL_ERROR(msg) FASTQ_fatal_error(msg)
    
%}

%option never-interactive nounistd yylineno reentrant bison-bridge stack noyywrap

%option prefix="FASTQ_"
%option extra-type="FASTQParseBlock*"

 /*%option debug*/

%x TAG_LINE
%x SEE_COORDS
%x PAST_COORDS
%x READ_NUMBER
%x NAMEVALUE
%x IN_SEQUENCE
%x QTAG_LINE
%x IN_QUALITY
%x INLINE_SEQUENCE
%x INLINE_QUALITY

ws          [ \t]+
digits      [0-9]+
alphanum    [A-Za-z0-9\-]+
eol         (\r\n|\r|\n)

/* there are more: =ACMGRSVTWYHKDBN. plus lowercase */
base [ACGTacgtNn.]   
cskey [ACGTacgt]
color   [0-3.]

ascqual   [\x0E-\x1F \x21-\x7F]+

%%

^@                              { BEGIN TAG_LINE; return yytext[0]; }
^>                              { BEGIN TAG_LINE; return yytext[0]; }

^\+                             { BEGIN QTAG_LINE; return yytext[0]; }

<INITIAL,TAG_LINE>
{
    :{digits}:{digits}:{digits}:{digits}                    { return fqCOORDS; }
    [SDE]RR{digits}\.{digits}   { return fqRUNDOTSPOT; } 
    {digits}                    { return fqNUMBER; }
    {alphanum}                  { return fqALPHANUM; }
    {ws}                        { return fqWS; }
    ^{base}+/{eol}              { return fqBASESEQ; }
    ^{cskey}{color}+/{eol}      { return fqCOLORSEQ; }
    .                           { return yytext[0]; }
}

<TAG_LINE>[ \t]*{eol}           { BEGIN IN_SEQUENCE; ENDLINE; } 

<IN_SEQUENCE>
{
    ^{base}+/{eol}              { BEGIN 0; return fqBASESEQ; }
    ^{cskey}{color}+/{eol}      { BEGIN 0; return fqCOLORSEQ; }
    ^{ascqual}/{eol}            { BEGIN 0; return fqASCQUAL; }
    ^.*$                        { BEGIN 0; return fqUNRECOGNIZED; }
}
  
<INLINE_SEQUENCE>
{
    {base}+                     { BEGIN 0; return fqBASESEQ; }
    {cskey}{color}+             { BEGIN 0; return fqCOLORSEQ; }
}

<QTAG_LINE>
{
    .+$                         { return fqTOKEN; /* do not bother to parse the inside of the line */ }
    {eol}                       { BEGIN IN_QUALITY; ENDLINE }
}

<IN_QUALITY,INLINE_QUALITY>
{
    {ws}                        { } 
}

<IN_QUALITY>
{
    ^{ascqual}/{eol}            { return fqASCQUAL; }
    {eol}                       {   /* if read was split across several lines, expect the same number of lines in quality */
                                    if (yyextra->expectedQualityLines <= 1) 
                                        BEGIN 0; 
                                    else 
                                        --yyextra->expectedQualityLines;
                                    ENDLINE; 
                                }
    .                           { return yytext[0]; }
}

<INLINE_QUALITY>
{
    {ascqual}/{eol}             { return fqASCQUAL; }
    {eol}                       { BEGIN 0; ENDLINE; }
    .                           { return yytext[0]; }
}

{eol}                           { ENDLINE; }

%%    

rc_t CC FASTQScan_yylex_init(FASTQParseBlock* sb, bool debug)
{
    if (yylex_init_extra(sb, &sb->scanner) != 0)
    {
        return RC ( rcApp, rcFile, rcParsing, rcMemory, rcExhausted );
    }       

    sb->length = 0;
    sb->lastToken = NULL;
    sb->record = NULL;
    sb->column = 1;
     
    sb->expectedQualityLines = 0;
    
    yyset_debug(debug ? 1 : 0, sb->scanner);

    yy_push_state(INITIAL, sb->scanner);
    
    return 0;
}

void CC FASTQScan_yylex_destroy(FASTQParseBlock* sb)
{
    if (sb->scanner)
    {
        yylex_destroy(sb->scanner);
    }       
    sb->scanner=0;
}

void CC FASTQScan_inline_sequence(FASTQParseBlock* pb) 
{
    yy_pop_state(pb->scanner);
    yy_push_state(INLINE_SEQUENCE, pb->scanner);
}

void CC FASTQScan_inline_quality(FASTQParseBlock* pb)
{
    yy_pop_state(pb->scanner);
    yy_push_state(INLINE_QUALITY, pb->scanner);
}

void CC FASTQ_unlex(FASTQParseBlock* pb, FASTQToken* token)
{
    size_t i;
    struct yyguts_t* yyg = (struct yyguts_t*)pb->scanner;
    for (i = 0; i < token->tokenLength; ++i)
    {
        yyunput(TokenTextPtr(pb, token)[token->tokenLength - 1 - i], yyg->yytext_r, pb->scanner);
    }
    yyextra -> column -= token->tokenLength; 
    yyextra -> length -= token->tokenLength; 
	KDataBufferResize( & pb->record->source, KDataBufferBytes( & pb->record->source ) - token->tokenLength );
}

void FASTQ_fatal_error(yyconst char* msg)
{
    rc_t rc = RC ( rcApp, rcFile, rcParsing, rcError, rcUnexpected );
    if (strcmp(msg, "input buffer overflow, can't enlarge buffer because scanner uses REJECT") == 0)
        pLogErr(klogErr, rc, "line is too long (maximum line length = $(B) bytes)", "B=%d", YY_BUF_SIZE);
    else
        LogErr(klogErr, rc, msg);
    exit(rc);  
}

void ConsumeToken(FASTQToken* token, FASTQParseBlock* pb)
{
    struct yyguts_t* yyg = (struct yyguts_t*)pb->scanner;
	
	token -> tokenStart = KDataBufferBytes( & pb->record->source );
	token -> tokenLength   = yyleng;   
	token -> line_no       = yylineno; 
	token -> column_no     = pb -> column; 
	
	KDataBufferResize( & pb->record->source, KDataBufferBytes( & pb->record->source ) + token->tokenLength );
	strncpy((char*)TokenTextPtr(pb, token), yytext, token->tokenLength);
    
	pb -> column += token->tokenLength; 
	pb -> length += token->tokenLength; 
	pb -> lastToken = token; 
}