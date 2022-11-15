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

   /*
      lex file is three sections:
      definitions: name definition
      %%
      rules: pattern action
      %%
      user code
   */

  /*
    @HD\tVN:1.4\tSO:coordinate
    @SQ\tSN:1\t...
    @SQ\tSN:16\t...
    @RG\tID:PM89\tPL:Illumina\tLB...
    HWI-...
  */

%top{
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "sam.tab.h"

//#define YYSTYPE char
#define LEXDEBUG 0

    // do { if (LEXDEBUG) fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, __VA_ARGS__); } while (0)
#define lex_print(fmt, ...) \
    do { if (LEXDEBUG) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

  }

/* definitions */
digits [[:digit:]]+
integer [-+]?[0-9]+
float  [-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
eol    (\r\n|\r|\n)
controlchar [\x00-\x08\x0b-\x0c\x0e-\x1f]

tag [A-Za-z][A-Za-z0-9]
 /* validtag VN|SO|GO|SN|LN|AS|M5|SP|UR|ID|CN|DS|DT|FO|KS|LB|PG|PI|PL|PM|PU|SM|ID|PN|CL|PP|DS|VN */


attv {tag}:A:[!-~]
ittv {tag}:i:{integer}
fttv {tag}:f:{float}
zttv {tag}:Z:[[:print:]]*
httv {tag}:H:([0-9A-F][0-9A-F])*
bttv {tag}:B:[cCsSiIf](,{float})+
ttv {attv}|{ittv}|{fttv}|{zttv}|{httv}|{bttv}

/* No @ in first, to distinguish from headers */
qname [!-?A-~][!-~]{0,253}

/* stop after first EOF */
%option noyywrap
%option prefix="SAM"
%option warn
%option nodefault
%option nodebug
/* Not a TTY */
%option never-interactive
%option yylineno
%option nounistd
/* Allow multitheading */
/* %option reentrant */
/* yylex takes yylval */
/* %option bison-bridge */
%x INALIGNMENT
%x AFTERCOLON

%%
 /* rules */
 /* TODO: lots of strdups, when do we free()? */
^@HD   { lex_print( "lex: Header\n"); return HEADER; }
^@SQ   { lex_print( "lex: SQ\n"); return SEQUENCE; }
^@RG   { lex_print( "lex: RG\n"); return READGROUP; }
^@PG   { lex_print( "lex: PG\n"); return PROGRAM; }
^@CO\t.*{eol} { lex_print( "lex: Comment\n"); return COMMENT; }
<INITIAL,INALIGNMENT>^{qname} { BEGIN INALIGNMENT; SAMlval.strval=strdup(yytext); lex_print("\nlex: alignment qname, INALIGNMENT\n"); return QNAME; }
<INITIAL,INALIGNMENT,AFTERCOLON>{controlchar} { lex_print("lex:CONTROLCHAR1\n"); return CONTROLCHAR; }
<INITIAL,INALIGNMENT>\t { lex_print("lex: tab\n"); return TAB;}
<AFTERCOLON>\t { BEGIN INITIAL; lex_print("lex: tab INITIAL\n"); return TAB;}

<INITIAL,INALIGNMENT,AFTERCOLON>{eol} { BEGIN INITIAL; lex_print("lex: eol INITIAL\n"); return EOL;}
 /* <INALIGNMENT>{ttv} { lex_print("lex: ttv\n"); return TTV; } */
<INALIGNMENT>[ -~]+ { SAMlval.strval=strdup(yytext); lex_print("lex: alignvalue\n"); return ALIGNVALUE; }
{tag}  { SAMlval.strval=strdup(yytext); lex_print("lex: Valid Tag:%s\n", yytext); return TAG; }
<INITIAL,INALIGNMENT>: { BEGIN AFTERCOLON; lex_print("lex: colon AFTERCOLON\n"); return COLON;}
<AFTERCOLON>[ -~]+ { SAMlval.strval=strdup(yytext); lex_print("lex: Value:%s\n", yytext); return VALUE; }

 /* \t[!-~]+ { lex_print("lex: alignment field\n"); return ALIGNMENT; }
 \t{ttv} { lex_print("lex: optional alignment\n"); return ALIGNMENT; }
 */
 /*
    \t{flag} { lex_print("lex: alignment flag\n"); return ALIGNMENT; }
    \t{mapq} { lex_print("lex: alignment mapq\n"); return ALIGNMENT; }
    \t{cigar} { lex_print("lex: alignment cigar\n"); return ALIGNMENT; }
    \t{rnext} { lex_print("lex: alignment rnext\n"); return ALIGNMENT; }
    \t{pnext} { lex_print("lex: alignment pnext\n"); return ALIGNMENT; }
    \t{tlen} { lex_print("lex: alignment tlen\n"); return ALIGNMENT; }
    \t{seq} { lex_print("lex: alignment seq\n"); return ALIGNMENT; }
    \t{qual} { lex_print("lex: alignment qual\n"); return ALIGNMENT; }
    \t{ttv} { lex_print("lex: alignment ttv\n"); return ALIGNMENT; }
    \t{rname} { lex_print("lex: alignment rname\n"); return ALIGNMENT; }
    \t{pos} { lex_print("lex: alignment pos\n"); return ALIGNMENT; }
 */



    /*
    ^@VN\t{version}\n printf("lex: Version\n");
    ^{hdrtag}(\t[[:alpha:]][[:alnum:]]:[[:print:]]+)+\n  printf("lex: Allowed Header %d %s", yyleng, yytext);
    ^@[[:upper:]]{2}(\t[[:alpha:]][[:alnum:]]:[[:print:]]+)+\n  printf("lex: Header %d %s", yyleng, yytext);
    ^@[[:upper:]]{2}(\t[[:alpha:]][[:alnum:]]+:[[:print:]]+)+\n  printf("lex: Header long %d %s", yyleng, yytext);
    ^{qname}\t{flag}\t{rname}\t{pos}\t{mapq}\t{cigar}\t{rnext}\t{pnext}\t{tlen}\t{seq}\t{qual}\n printf("lex: Alignment\n");
    ^{qname}\t{flag}\t{rname}\t{pos}\t{mapq}\t{cigar}\t{rnext}\t{pnext}\t{tlen}\t{seq}\t{qual}({ttv})+\n {
        char * s1=strdup(yytext);
        printf("lex: Alignment with optional: %s\n", s1);
        char * s2=s1;
        char * tok;
        while ((tok=strsep(&s2,"\t")))
        {
            printf("  Token:%s\n", tok);
        }


        free(s1);
    }


    ^@[[:lower:]]{2}\t.*\n    printf("Optional, lowercase tag\n");
    ^@HD.*\n    printf("Empty HD: %s %d", yytext, yyleng);
    ^[^@].*\n printf("not at:                     %s", yytext);
 */
.|\n       lex_print("DEFAULT '%c' ", *yytext);

%%
 /* user code */
/*
int main(int argc, char * argv[])
{
    SAMlex();
//    SAMparse();
}
*/
