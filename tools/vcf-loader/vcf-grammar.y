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

    #define YYSTYPE VCFToken
    #define YYLEX_PARAM pb->scanner
    #define YYDEBUG 1

    #include "vcf-grammar.h"
%}

%pure-parser
%parse-param {VCFParseBlock* pb }
%lex-param {VCFParseBlock* pb }
%error-verbose 
%name-prefix="VCF_"

%token vcfMETAKEY_FORMAT
%token vcfMETAKEY
%token vcfMETAVALUE
%token vcfHEADERITEM
%token vcfDATAITEM
%token vcfENDLINE
%token vcfENDOFTEXT 0

%%

vcfFile: 
        fileFormatLine
        metaLinesOpt           
        headerLine
        dataLines
        vcfENDOFTEXT
            { return 1; }  
    |
        error { return 0; } 
    |
        { yyerror(pb, "no input found"); return 0; }
    ;

fileFormatLine:
        vcfMETAKEY_FORMAT '=' vcfMETAVALUE vcfENDLINE   { pb->metaLine(pb, & $1, &$3); }
    ;
    
metaLinesOpt:
    metaLines
    |
    ;

metaLines:
        metaLine
    |   metaLines metaLine
    ;
    
metaLine:
        vcfMETAKEY '=' vcfMETAVALUE vcfENDLINE         { pb->metaLine(pb, & $1, &$3); }                          
    |   vcfMETAKEY '=' '<' { pb->openMetaLine(pb, &$1); } keyValuePairs '>' vcfENDLINE { pb->closeMetaLine(pb); }         
    ;

keyValuePairs:
        keyValue
    |   keyValuePairs ',' keyValue
    ;    
    
keyValue:
    vcfMETAKEY '=' vcfMETAVALUE { pb->keyValue(pb, & $1, &$3); }                          
    ;
    
headerLine:    
    headerItems vcfENDLINE 
    ;

headerItems:    
        vcfHEADERITEM               { pb->headerItem(pb, & $1); }  
    |   headerItems vcfHEADERITEM   { pb->headerItem(pb, & $2); }  
    ;    

dataLines:
        dataLine
    |   dataLines dataLine  
    ;
    
dataLine:
    dataItems vcfENDLINE        { pb->closeDataLine(pb); }    
    ;
    
dataItems:    
        vcfDATAITEM             { pb->openDataLine(pb); pb->dataItem(pb, & $1); }  
    |   dataItems vcfDATAITEM   { pb->dataItem(pb, & $2); }  
    ;
    
%%

void VCF_error(struct VCFParseBlock* pb, const char* msg)
{
    if (pb && pb->error)
        pb->error(pb, msg);
}
