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

#ifndef _h_fastq_parse_
#define _h_fastq_parse_

#include <align/extern.h>
#include <klib/text.h>

#include <loader/common-reader-priv.h>

#ifdef __cplusplus
extern "C" {
#endif

/* values used in validating quality lines */
#define MIN_PHRED_33 33
#define MAX_PHRED_33 126
#define MIN_PHRED_64 64
#define MAX_PHRED_64 127

struct FastqSequence
{
    Sequence_vt sequence_vt;
    KRefcount   refcount;

    /* tagline components: */
    String spotname; /* tag line up to and including coordinates */
    String spotgroup; /* token following '#' */
    uint8_t readnumber; /* token following '/' 1 - IsFirst, 2 - IsSecond, 0 - dont know */

    /* not populated at this time: */
#if 0
    String rungroup; 
    String fmt_name; /* x and y replaced with $X and $Y */
    uint8_t coord_num;
    int32_t coords[16];
#endif

    /* read bases */
    String read; 

    bool is_colorspace;
    
    String  quality;
    uint8_t qualityOffset;
    
    bool lowQuality;
};

struct FastqRecord
{
    Record  dad;

    KDataBuffer source;
    struct FastqSequence    seq;
    Rejected*               rej; 
};

typedef struct FASTQToken
{ 
    size_t tokenStart;  /* offset into FASTQParseBlock.record->source */
    size_t tokenLength;
    size_t line_no;
    size_t column_no;
} FASTQToken;

/* obtain a pointer to the token's text */
#define TokenTextPtr(pb, token) ((const char*)((pb)->record->source.base) + (token)->tokenStart)


typedef struct FASTQParseBlock
{
    void* self;
    size_t (CC *input)(struct FASTQParseBlock* sb, char* buf, size_t max_size);
    uint8_t phredOffset;
    uint8_t maxPhred;
    
    void* scanner;
    size_t length; /* input characters consumed for the current record */
    FASTQToken* lastToken;
    struct FastqRecord* record;
    size_t column;

    /* temporaries for bison */
    /* all offsets are into record->source */
    size_t spotNameOffset; 
    size_t spotNameLength; 
    bool spotNameDone;
    size_t spotGroupOffset;
    size_t spotGroupLength;
    size_t readOffset;
    size_t readLength;
    size_t qualityOffset;
    size_t qualityLength;

    size_t expectedQualityLines;
    
    int8_t defaultReadNumber; /* -1: never assign read numbers */

    /*  Secondary (>1) read number observed previously (usually 2, sometimes 3). 
        Once one is seen, do not allow any other values in the same input file. 
        0 = has not seen one yet in this input. 
        Always record as 2 */
    uint8_t secondaryReadNumber; 
    
    bool ignoreSpotGroups;
    
    bool fatalError;
} FASTQParseBlock;

extern rc_t FASTQScan_yylex_init(FASTQParseBlock* context, bool debug);
extern void FASTQScan_yylex_destroy(FASTQParseBlock* context);

/* explicit FLEX state control for bison*/
extern void FASTQScan_inline_sequence(FASTQParseBlock* pb);
extern void FASTQScan_inline_quality(FASTQParseBlock* pb);

extern void FASTQ_set_lineno (int line_number, void* scanner);

extern int FASTQ_lex(FASTQToken* pb, void * scanner);
extern void FASTQ_unlex(FASTQParseBlock* pb, FASTQToken* token);
extern void FASTQ_qualityContext(FASTQParseBlock* pb);

extern int FASTQ_debug; /* set to 1 to print Bison trace */ 

extern int FASTQ_parse(FASTQParseBlock* pb); /* 0 = end of input, 1 = success, a new record is in context->record, 2 - syntax error */

/* call before parsing every record (FASTQ_parse does so internally; this is for testing the parser) */
extern void FASTQ_ParseBlockInit(FASTQParseBlock* pb); 

extern void FASTQ_error(FASTQParseBlock* pb, const char* msg);

#ifdef __cplusplus
}
#endif

#endif /* _h_fastq_parse_ */
