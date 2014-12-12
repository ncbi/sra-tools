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

#ifndef _h_vcf_parse_
#define _h_vcf_parse_

#include <stdbool.h>
#include <stdlib.h>

typedef struct VCFToken
{ 
    const char* tokenText; /* 0-terminated, short-lived */
    size_t tokenStart;  /* 0-based offset from the start of input */
    size_t tokenLength;
    size_t line_no;
    size_t column_no;
} VCFToken;

struct VcfRecord;
struct VcfReader;

typedef struct VCFParseBlock
{
    void* self; /* VcfReader object or a test double */
    void* scanner; /* flex control block */
    
    size_t (*input)(struct VCFParseBlock* pb, char* buf, size_t max_size); /* read the next part of the input */
    void (*error)(struct VCFParseBlock* pb, const char* message);
    
    void (*metaLine)(struct VCFParseBlock* pb, VCFToken* key, VCFToken* value); /* simple meta line: ##key=value */
    
    /* two-level meta line: ##key=<key=value,...> */
    void (*openMetaLine)(struct VCFParseBlock* pb, VCFToken* key);              
    void (*keyValue)(struct VCFParseBlock* pb, VCFToken* key, VCFToken* value);
    void (*headerItem)(struct VCFParseBlock* pb, VCFToken* value);
    void (*closeMetaLine)(struct VCFParseBlock* pb);
    
    /* data line */
    void (*openDataLine)(struct VCFParseBlock* pb);
    void (*dataItem)(struct VCFParseBlock* pb, VCFToken* value);
    void (*closeDataLine)(struct VCFParseBlock* pb);

    size_t offset;  /* 0-based offset from the start of input */
    size_t column;  /* 1-based column, for error reporting */
    
   /* for error reporting, valid inside error() only: */
    VCFToken* lastToken;
  
} VCFParseBlock;

extern bool VCFScan_yylex_init(VCFParseBlock* context, bool debug); /* false - out of memory */
extern void VCFScan_yylex_destroy(VCFParseBlock* context);

extern void VCF_set_lineno (int line_number, void* scanner);

extern int VCF_lex(VCFToken* pb, void * scanner);
extern void VCF_unlex(VCFParseBlock* pb, VCFToken* token);

extern int VCF_debug; /* set to 1 to print Bison trace */ 

/* read 1 data line (possibly preceded by any number of meta/header lines) */
extern int VCF_parse(VCFParseBlock* pb); /* 0 = end of input, 1 = success, a new record is in context->record, 2 - syntax error */

extern void VCF_error(VCFParseBlock* pb, const char* msg);

#endif /* _h_vcf_parse_ */
