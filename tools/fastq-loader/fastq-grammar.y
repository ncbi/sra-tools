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

    static void SetReadNumber ( FASTQParseBlock * pb, const FASTQToken * token );

    static void SetSpotGroup ( FASTQParseBlock * pb, const FASTQToken * token );

    static void SetRead     ( FASTQParseBlock * pb, const FASTQToken * token);
    static void ExpandRead  ( FASTQParseBlock * pb, const FASTQToken * token);

    static void SetQuality      ( FASTQParseBlock * pb, const FASTQToken * token);
    static void ExpandQuality   ( FASTQParseBlock * pb, const FASTQToken * token);

    static void StartSpotName(FASTQParseBlock* pb, size_t offset);
    static void ExpandSpotName(FASTQParseBlock* pb, const FASTQToken* token);
    static void StopSpotName(FASTQParseBlock* pb);
    static void RestartSpotName(FASTQParseBlock* pb);
    static void SaveSpotName(FASTQParseBlock* pb);
    static void RevertSpotName(FASTQParseBlock* pb);

    #define UNLEX do { if (yychar != YYEMPTY && yychar != YYEOF) FASTQ_unlex(pb, & yylval); } while (0)

    #define IS_PACBIO(pb) ((pb)->defaultReadNumber == -1)
%}

%pure-parser
%parse-param {FASTQParseBlock* pb }
%lex-param {FASTQParseBlock* pb }
%error-verbose
%name-prefix "FASTQ_"

%token fqRUNDOTSPOT
%token fqSPOTGROUP
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

/*    | qualityLines              { UNLEX; return 1; } */

    | name
        fqCOORDS                { ExpandSpotName(pb, &$2); StopSpotName(pb); }
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
    | error endline
    ;

header
    : '@' { StartSpotName(pb, 1); } tagLine
    | '@' fqWS { StartSpotName(pb, 1 + $2.tokenLength); } tagLine
    | '>' { StartSpotName(pb, 1); } tagLine
    ;

read
    : baseRead  { pb->record->seq.is_colorspace = false; }
    | csRead    { pb->record->seq.is_colorspace = true; }
    ;

baseRead
    : fqBASESEQ { SetRead(pb, & $1); }
    | baseRead endline fqBASESEQ { ExpandRead(pb, & $3); }
    | baseRead endline
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
    | nameSpotGroup readNumber

    | nameSpotGroup readNumber fqWS fqNUMBER ':' fqALPHANUM ':' fqNUMBER indexSequence

    | nameSpotGroup readNumber fqWS fqALPHANUM { FASTQScan_skip_to_eol(pb); }
    | nameSpotGroup readNumber fqWS { FASTQScan_skip_to_eol(pb); }

    | nameSpotGroup fqWS  { ExpandSpotName(pb, &$1); StopSpotName(pb); } casava1_8 { FASTQScan_skip_to_eol(pb); }
    | nameSpotGroup fqWS  { ExpandSpotName(pb, &$1); StopSpotName(pb); } fqALPHANUM { FASTQScan_skip_to_eol(pb); } /* no recognizable read number */
    | runSpotRead fqWS  { FASTQScan_skip_to_eol(pb); }
    | runSpotRead       { FASTQScan_skip_to_eol(pb); }
    | name readNumber
    | name readNumber fqWS  { FASTQScan_skip_to_eol(pb); }
    | name
    ;

nameSpotGroup
    : nameWithCoords
    | nameWithCoords fqSPOTGROUP                { SetSpotGroup(pb, &$2); }
    | name { StopSpotName(pb); } fqSPOTGROUP    { SetSpotGroup(pb, &$3); }
    | nameWS nameWithCoords                                                     /* nameWS ignored */
    | nameWS nameWithCoords fqSPOTGROUP         { SetSpotGroup(pb, &$3); }      /* nameWS ignored */
    | nameWS fqALPHANUM '='  { RevertSpotName(pb); FASTQScan_skip_to_eol(pb); }
    ;

nameWS
    : name fqWS
    {   /* 'name' without coordinates attached will be ignored if followed by a name with coordinates (see the previous production).
           however, if not followed, this will be the spot name, so we need to save the 'name's coordinates in case
           we need to revert to them later (see call to RevertSpotName() above) */
        SaveSpotName(pb);
        ExpandSpotName(pb, &$2); /* need to account for white space but it is not part of the spot name */
        RestartSpotName(pb); /* clean up for the potential nameWithCoords to start here */
    }

nameWithCoords
    : name fqCOORDS { ExpandSpotName(pb, &$2); StopSpotName(pb); }
    | name fqCOORDS '_'
                {   /* another variation by Illumina, this time "_" is used as " /" */
                    ExpandSpotName(pb, &$2);
                    StopSpotName(pb);
                    ExpandSpotName(pb, &$3);
                }
                casava1_8
    | name fqCOORDS ':'     { ExpandSpotName(pb, &$2); ExpandSpotName(pb, &$3);} name
    | name fqCOORDS '.'     { ExpandSpotName(pb, &$2); ExpandSpotName(pb, &$3);} name
    | name fqCOORDS ':' '.' { ExpandSpotName(pb, &$2); ExpandSpotName(pb, &$3); ExpandSpotName(pb, &$4);} name
    | name fqCOORDS ':'     { ExpandSpotName(pb, &$2); ExpandSpotName(pb, &$3); StopSpotName(pb); }
    ;

name
    : fqALPHANUM        { ExpandSpotName(pb, &$1); }
    | fqNUMBER          { ExpandSpotName(pb, &$1); }
    | name '_'          { ExpandSpotName(pb, &$2); }
    | name '-'          { ExpandSpotName(pb, &$2); }
    | name '.'          { ExpandSpotName(pb, &$2); }
    | name ':'          { ExpandSpotName(pb, &$2); }
    | name fqALPHANUM   { ExpandSpotName(pb, &$2); }
    | name fqNUMBER     { ExpandSpotName(pb, &$2); }
    ;

readNumber
    : '/'
        {   /* in PACBIO fastq, the first '/' and the following digits are treated as a continuation of the spot name, not a read number */
            if (IS_PACBIO(pb)) pb->spotNameDone = false;
            ExpandSpotName(pb, &$1);
        }
      fqNUMBER
        {
            if (!IS_PACBIO(pb)) SetReadNumber(pb, &$3);
            ExpandSpotName(pb, &$3);
            StopSpotName(pb);
        }

    | readNumber '/'
        {
            if (IS_PACBIO(pb)) pb->spotNameDone = false;
            ExpandSpotName(pb, &$2);
        }
        name
        {
            if (IS_PACBIO(pb)) StopSpotName(pb);
        }
    ;

casava1_8
    : fqNUMBER          { SetReadNumber(pb, &$1); ExpandSpotName(pb, &$1); StopSpotName(pb); }
    | fqNUMBER          { SetReadNumber(pb, &$1); ExpandSpotName(pb, &$1); StopSpotName(pb); }
     ':'                { ExpandSpotName(pb, &$3); }
     fqALPHANUM         { ExpandSpotName(pb, &$5); if ($5.tokenLength == 1 && TokenTextPtr(pb, &$5)[0] == 'Y') pb->record->seq.lowQuality = true; }
     ':'                { ExpandSpotName(pb, &$7); }
     fqNUMBER           { ExpandSpotName(pb, &$9); }
     indexSequence
    ;

indexSequence
    :  ':' { ExpandSpotName(pb, &$1); FASTQScan_inline_sequence(pb); } index
    |
    ;

index
    : fqBASESEQ { SetSpotGroup(pb, &$1); ExpandSpotName(pb, &$1); }
    | fqNUMBER  { SetSpotGroup(pb, &$1); ExpandSpotName(pb, &$1); }
    |
    ;

runSpotRead
    : fqRUNDOTSPOT '.' fqNUMBER     { ExpandSpotName(pb, &$1); StopSpotName(pb); SetReadNumber(pb, &$3); }
    | fqRUNDOTSPOT '/' fqNUMBER     { ExpandSpotName(pb, &$1); StopSpotName(pb); SetReadNumber(pb, &$3); }
    | fqRUNDOTSPOT                  { ExpandSpotName(pb, &$1); StopSpotName(pb); }
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
    : fqASCQUAL                 { SetQuality(pb, & $1); }
    | quality endline fqASCQUAL { ExpandQuality(pb, & $3); }
    | quality endline
    ;

%%

/* values used in validating quality lines */
#define MIN_PHRED_33    33
#define MAX_PHRED_33    126
#define MIN_PHRED_64    64
#define MAX_PHRED_64    127
#define MIN_LOGODDS     59
#define MAX_LOGODDS     126

/* make sure all qualities fall into the required range */
static bool CheckQualities ( FASTQParseBlock* pb, const FASTQToken* token )
{
    uint8_t floor;
    uint8_t ceiling;
    const char* format;
    switch ( pb->qualityFormat )
    {
    case FASTQphred33:
        floor   = MIN_PHRED_33;
        ceiling = MAX_PHRED_33;
        format = "Phred33";
        pb -> qualityAsciiOffset = 33;
        break;
    case FASTQphred64:
        floor   = MIN_PHRED_64;
        ceiling = MAX_PHRED_64;
        format = "Phred64";
        pb -> qualityAsciiOffset = 64;
        break;
    case FASTQlogodds:
        floor   = MIN_LOGODDS;
        ceiling = MAX_LOGODDS;
        format = "Logodds";
        pb -> qualityAsciiOffset = 64;
        break;
    default:
        /* TODO:
            if qualityAsciiOffset is 0,
                guess based on the raw values on the first quality line:
                    if all values are above MAX_PHRED_33, qualityAsciiOffset = 64
                    if all values are in MIN_PHRED_33..MAX_PHRED_33, qualityAsciiOffset = 33
                    if any value is below MIN_PHRED_33, abort
                if the guess is 33 and proven wrong (a raw quality value >MAX_PHRED_33 is encountered and no values below MIN_PHRED_64 ever seen)
                    reopen the file,
                    qualityAsciiOffset = 64
                    try to parse again
                    if a value below MIN_PHRED_64 seen, abort
        */
        {
            char buf[200];
            sprintf ( buf, "Invalid quality format: %d.", pb->qualityFormat );
            pb->fatalError = true;
            yyerror(pb, buf);
            return false;
        }
    }

    {
        unsigned int i;
        for (i=0; i < token->tokenLength; ++i)
        {
            char ch = TokenTextPtr(pb, token)[i];
            if (ch < floor || ch > ceiling)
            {
                char buf[200];
                sprintf ( buf, "Invalid quality value ('%c'=%d, position %d): for %s, valid range is from %d to %d.",
                                                        ch,
                                                        ch,
                                                        i,
                                                        format,
                                                        floor,
                                                        ceiling);
                pb->fatalError = true;
                yyerror(pb, buf);
                return false;
            }
        }
    }
    return true;
}

void SetQuality ( FASTQParseBlock* pb, const FASTQToken* token)
{
    if ( CheckQualities ( pb, token ) )
    {
        pb->qualityOffset = token->tokenStart;
        pb->qualityLength= token->tokenLength;
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
    pb->spotNameLength = 0;
}

void SaveSpotName(FASTQParseBlock* pb)
{
    pb->spotNameOffset_saved = pb->spotNameOffset;
    pb->spotNameLength_saved = pb->spotNameLength;
}

void RestartSpotName(FASTQParseBlock* pb)
{
    pb->spotNameOffset += pb->spotNameLength;
    pb->spotNameLength = 0;
}

void RevertSpotName(FASTQParseBlock* pb)
{
    pb->spotNameOffset = pb->spotNameOffset_saved;
    pb->spotNameLength = pb->spotNameLength_saved;
}

void ExpandSpotName(FASTQParseBlock* pb, const FASTQToken* token)
{
    if (!pb->spotNameDone)
    {
        pb->spotNameLength += token->tokenLength;
    }
}

void StopSpotName(FASTQParseBlock* pb)
{   /* there may be more tokens coming, they will not be a part of the spot name */
    pb->spotNameDone = true;
}

void SetSpotGroup(FASTQParseBlock* pb, const FASTQToken* token)
{
    if ( ! pb->ignoreSpotGroups )
    {
        unsigned int nameStart = 0;
        /* skip possible '#' at the start of spot group name */
        if ( TokenTextPtr ( pb, token )[0] == '#' )
        {
            nameStart = 1;
        }

        if ( token->tokenLength != 1+nameStart || TokenTextPtr(pb, token)[nameStart] != '0' ) /* ignore spot group 0 */
        {
            pb->spotGroupOffset = token->tokenStart  + nameStart;
            pb->spotGroupLength = token->tokenLength - nameStart;
        }
    }
}

void SetRead(FASTQParseBlock* pb, const FASTQToken* token)
{
    pb -> readOffset = token->tokenStart;
    pb -> readLength = token->tokenLength;
    pb -> expectedQualityLines = 1;
}

/* Handling of multi-line reads and qualities (VDB-3413) */

void ExpandQuality ( FASTQParseBlock * pb, const FASTQToken * newQualityLine )
{
    /* In the already-processed part of the flex buffer, move newQualityLine's text to be immediately after
       the current qualit string's text. Used to discard end-of-lines in multiline qualities */

    if ( CheckQualities ( pb, newQualityLine ) )
    {
        char * copyTo = ( char * ) ( pb -> record -> source . base ) + pb -> qualityOffset + pb -> qualityLength;
        memmove (  copyTo, TokenTextPtr ( pb, newQualityLine ), newQualityLine -> tokenLength );
        pb -> qualityLength += newQualityLine -> tokenLength;
    }
}

void ExpandRead ( FASTQParseBlock * pb, const FASTQToken * newReadLine )
{
    /* In the already-processed part of the flex buffer, move newReadLine's text to be immediately after
       the current read's text. Used to discard end-of-lines in multiline reads */

    char * copyTo = ( char * ) ( pb -> record -> source . base ) + pb -> readOffset + pb -> readLength;
    memmove (  copyTo, TokenTextPtr ( pb, newReadLine ), newReadLine -> tokenLength );
    pb -> readLength += newReadLine -> tokenLength;
    ++ pb -> expectedQualityLines;
}
