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
    #include <stdlib.h>
    #include <string.h>

    #include "fastq-parse.h"

    #define YYSTYPE FASTQToken
    #define YYLEX_PARAM pb->scanner
    #define YYDEBUG 1

    #include "fastq-tokens.h"

    static void AddQuality(FASTQParseBlock* pb, const FASTQToken* token);
    static void SetReadNumber(FASTQParseBlock* pb, const FASTQToken* token);
    static void StartSpotName(FASTQParseBlock* pb, size_t offset);
    static void GrowSpotName(FASTQParseBlock* pb, const FASTQToken* token);
    static void StopSpotName(FASTQParseBlock* pb);
    static void SetSpotGroup(FASTQParseBlock* pb, const FASTQToken* token);
    static void SetRead(FASTQParseBlock* pb, const FASTQToken* token);

    #define UNLEX do { if (yychar != YYEMPTY && yychar != YYEOF) FASTQ_unlex(pb, & yylval); } while (0)
    
    #define IS_PACBIO(pb) ((pb)->defaultReadNumber == -1)
%}

%pure-parser
%parse-param {FASTQParseBlock* pb }
%lex-param {FASTQParseBlock* pb }
%error-verbose 
%name-prefix="FASTQ_"

%token fqRUNDOTSPOT
%token fqNUMBER
%token fqALPHANUM
%token fqWS
%token fqENDLINE
%token fqBASESEQ
%token fqCOLORSEQ
%token fqTOKEN
%token fqASCQUAL
%token fqCOORDS
%token fqUNRECOGNIZED
%token fqENDOFTEXT 0


%%

sequence /* have to return the lookahead symbol before returning since it belongs to the next record and cannot be dropped */
    : readLines qualityLines    { UNLEX; return 1; }
    
    | readLines                 { UNLEX; return 1; }
    
    | qualityLines              { UNLEX; return 1; } 
    
    | name                      { StartSpotName(pb, 0); }
        fqCOORDS                { GrowSpotName(pb, &$3); StopSpotName(pb); }
        ':'                     { FASTQScan_inline_sequence(pb); } 
        inlineRead              
        ':'                     { FASTQScan_inline_quality(pb); } 
        quality                 { UNLEX; return 1; }
           
    | fqALPHANUM error endline  { UNLEX; return 1; }
           
    | endfile                   { return 0; }
    ;

endfile
    : fqENDOFTEXT
    | endline endfile
    ;

endline
    : fqENDLINE
    ;

readLines
    : header  endline  read   
    | header  endline error endline
    | error   endline  read 
    ;

header 
    : '@' { StartSpotName(pb, 1); } tagLine
    | '>' { StartSpotName(pb, 1); } tagLine
    ;

read
    : baseRead  { pb->record->seq.is_colorspace = false; }
    | csRead    { pb->record->seq.is_colorspace = true; }
    ;

baseRead
    : fqBASESEQ { SetRead(pb, & $1); } 
        endline            
    | baseRead fqBASESEQ { SetRead(pb, & $2); } 
        endline  
    ;
 
csRead
    : fqCOLORSEQ { SetRead(pb, & $1); } 
        endline           
    | csRead fqCOLORSEQ { SetRead(pb, & $2); } 
        endline
    ;

inlineRead
    : fqBASESEQ                   { SetRead(pb, & $1); pb->record->seq.is_colorspace = false; }
    | fqCOLORSEQ                  { SetRead(pb, & $1); pb->record->seq.is_colorspace = true; }
    ;
    
 /*************** tag line rules *****************/
tagLine    
    : nameSpotGroup 
    | nameSpotGroup readNumberOrTail
    | runSpotRead 
    ;
    
nameSpotGroup
    : name { StopSpotName(pb); } 
        spotGroup 
    | name fqCOORDS         { GrowSpotName(pb, &$2); StopSpotName(pb); }                                               
        spotGroup
    | name fqCOORDS '_' 
                {   /* another crazy variation by Illumina, this time "_" is used as " /" */
                    GrowSpotName(pb, &$2); 
                    StopSpotName(pb);
                    GrowSpotName(pb, &$3);
                }    
                casava1_8
    | name fqCOORDS ':'     { GrowSpotName(pb, &$2); GrowSpotName(pb, &$3); StopSpotName(pb); }                          
        spotGroup  
    | name fqCOORDS ':'     { GrowSpotName(pb, &$2); GrowSpotName(pb, &$3);}                          
        name
    | name fqCOORDS '.'     { GrowSpotName(pb, &$2); GrowSpotName(pb, &$3);}                          
        name
    | name fqCOORDS ':' '.' { GrowSpotName(pb, &$2); GrowSpotName(pb, &$3); GrowSpotName(pb, &$3);}    
        name
    ;
    
name
    : fqALPHANUM        { GrowSpotName(pb, &$1); }
    | fqNUMBER          { GrowSpotName(pb, &$1); }
    | name '_'          { GrowSpotName(pb, &$2); }
    | name '-'          { GrowSpotName(pb, &$2); }
    | name '.'          { GrowSpotName(pb, &$2); }
    | name ':'          { GrowSpotName(pb, &$2); }
    | name fqALPHANUM   { GrowSpotName(pb, &$2); }
    | name fqNUMBER     { GrowSpotName(pb, &$2); }
    ;

spotGroup
    : '#'           { GrowSpotName(pb, &$1); }
        fqNUMBER    { SetSpotGroup(pb, &$3);  GrowSpotName(pb, &$3); }
    | '#'           { GrowSpotName(pb, &$1); }    
        fqALPHANUM  { SetSpotGroup(pb, &$3);  GrowSpotName(pb, &$3); }    
    |
    ;
    
readNumberOrTail
    : readNumber
    | fqWS  { GrowSpotName(pb, &$1); }    
        casava1_8
    | fqWS  { GrowSpotName(pb, &$1); }  
        tail
    | readNumberOrTail fqWS { GrowSpotName(pb, &$2); } tail
    ;

readNumber
    : '/'       
        {   /* in PACBIO fastq, the first '/' and the following digits are treated as a continuation of the spot name, not a read number */
            if (IS_PACBIO(pb)) pb->spotNameDone = false; 
            GrowSpotName(pb, &$1); 
        } 
      fqNUMBER  
        { 
            if (!IS_PACBIO(pb)) SetReadNumber(pb, &$3); 
            GrowSpotName(pb, &$3); 
            StopSpotName(pb); 
        }
 
    | readNumber '/' 
        { 
            if (IS_PACBIO(pb)) pb->spotNameDone = false; 
            GrowSpotName(pb, &$2); 
        } 
        name 
        { 
            if (IS_PACBIO(pb)) StopSpotName(pb); 
        }
    ;
    
casava1_8
    : fqNUMBER          { SetReadNumber(pb, &$1); GrowSpotName(pb, &$1); StopSpotName(pb); }
     ':'                { GrowSpotName(pb, &$3); }
     fqALPHANUM         { GrowSpotName(pb, &$5); if ($5.tokenLength == 1 && TokenTextPtr(pb, &$5)[0] == 'Y') pb->record->seq.lowQuality = true; }
     ':'                { GrowSpotName(pb, &$7); }
     fqNUMBER           { GrowSpotName(pb, &$9); }
     ':'                { GrowSpotName(pb, &$11); } 
     indexSequence
    ;
     
indexSequence
    : fqALPHANUM        { SetSpotGroup(pb, &$1); GrowSpotName(pb, &$1); } 
    | fqNUMBER          { SetSpotGroup(pb, &$1); GrowSpotName(pb, &$1); } 
    |
    ;
    
tail
    : fqALPHANUM        { GrowSpotName(pb, &$1); }
    | tail fqNUMBER          { GrowSpotName(pb, &$2); }
    | tail fqALPHANUM        { GrowSpotName(pb, &$2); }
    | tail '_'               { GrowSpotName(pb, &$2); }
    | tail '/'               { GrowSpotName(pb, &$2); }
    | tail '='               { GrowSpotName(pb, &$2); }
    ;
   
runSpotRead
    : fqRUNDOTSPOT '.' fqNUMBER     { SetReadNumber(pb, &$3); GrowSpotName(pb, &$3); StopSpotName(pb); }
    | fqRUNDOTSPOT '/' fqNUMBER     { SetReadNumber(pb, &$3); GrowSpotName(pb, &$3); StopSpotName(pb); }
    | fqRUNDOTSPOT                  { StopSpotName(pb); }
    | runSpotRead fqWS tail
    | runSpotRead fqWS fqNUMBER 
    | runSpotRead fqWS fqNUMBER tail
    ;
    
 /*************** quality rules *****************/

qualityLines
    : qualityHeader endline quality 
    | qualityHeader endline error endline
    ;

qualityHeader
    : '+'                 
    | qualityHeader fqTOKEN
    ;

quality
    : qualityLine endline           
    | quality qualityLine endline
    
qualityLine
    : fqASCQUAL                {  AddQuality(pb, & $1); }
    ;

%%

void AddQuality(FASTQParseBlock* pb, const FASTQToken* token)
{
    if (pb->phredOffset != 0)
    {
        uint8_t floor   = pb->phredOffset == 33 ? MIN_PHRED_33 : MIN_PHRED_64;
        uint8_t ceiling = pb->maxPhred == 0 ? (pb->phredOffset == 33 ? MAX_PHRED_33 : MAX_PHRED_64) : pb->maxPhred;
        unsigned int i;
        for (i=0; i < token->tokenLength; ++i)
        {
            char buf[200];
			char ch = TokenTextPtr(pb, token)[i];
            if (ch < floor || ch > ceiling)
            {
                sprintf(buf, "Invalid quality value ('%c'=%d, position %d): for %s, valid range is from %d to %d.", 
                                                         ch,
														 ch,
														 i,
                                                         pb->phredOffset == 33 ? "Phred33": "Phred64", 
                                                         floor, 
                                                         ceiling);
                pb->fatalError = true;
                yyerror(pb, buf);
                return;
            }
        }
    }
    if (pb->qualityLength == 0)
    {
        pb->qualityOffset = token->tokenStart;
        pb->qualityLength= token->tokenLength;
    }
    else
    {
        pb->qualityLength += token->tokenLength;
    }
}

void SetReadNumber(FASTQParseBlock* pb, const FASTQToken* token)
{   /* token is known to be numeric */
    if (pb->defaultReadNumber != -1)
    {   /* we will only handle 1-digit read numbers for now*/
        if (token->tokenLength == 1)
        {
            switch (TokenTextPtr(pb, token)[0])
            {
            case '1': 
                {
                    pb->record->seq.readnumber = 1; 
                    break;
                }
            case '0': 
                {
                    pb->record->seq.readnumber = pb->defaultReadNumber;
                    break;
                }
            default: 
                {   /* all secondary read numbers should be the same across an input file */
                    uint8_t readNum = TokenTextPtr(pb, token)[0] - '0';
                    if (pb->secondaryReadNumber == 0) /* this is the first secondary read observed */
                    {
                        pb->secondaryReadNumber = readNum;
                    }
                    else if (pb->secondaryReadNumber != readNum)
                    {
                        char buf[200];
                        sprintf(buf, 
                                "Inconsistent secondary read number: previously used %d, now seen %d", 
                                pb->secondaryReadNumber, readNum);
                        pb->fatalError = true;
                        yyerror(pb, buf);
                        return;
                    }
                    /* all secondary read numbers are internally represented as 2 */
                    pb->record->seq.readnumber = 2; 
                    
                    break;
                }
            }
        }
        else
            pb->record->seq.readnumber = pb->defaultReadNumber;
    }
}

void StartSpotName(FASTQParseBlock* pb, size_t offset)
{
    pb->spotNameOffset = offset;
}
void GrowSpotName(FASTQParseBlock* pb, const FASTQToken* token)
{
    if (!pb->spotNameDone)
        pb->spotNameLength += token->tokenLength;
}

void StopSpotName(FASTQParseBlock* pb)
{   /* there may be more tokens coming, they will not be a part of the spot name */
    pb->spotNameDone = true;
}

void SetSpotGroup(FASTQParseBlock* pb, const FASTQToken* token)
{
    if ( ! pb->ignoreSpotGroups )
    {
        if (token->tokenLength != 1 || TokenTextPtr(pb, token)[0] != '0') /* ignore spot group 0 */
        {
            pb->spotGroupOffset = token->tokenStart;    
            pb->spotGroupLength = token->tokenLength;
        }
    }
}

void SetRead(FASTQParseBlock* pb, const FASTQToken* token)
{ 
    pb->readOffset = token->tokenStart;
    pb->readLength = token->tokenLength;
}
