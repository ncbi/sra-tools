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

#include "vcf-reader.h"
#include "vcf-parse.h"

#include <klib/rc.h>

#include <klib/text.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/printf.h>

#include <kfs/mmap.h>

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define LINE_VECTOR_BLOCK_SIZE 1024
#define GENOTYPE_LIST_BLOCK_SIZE 64
#define MESSAGE_LIST_BLOCK_SIZE 64
#define PARSE_ERROR RC ( rcAlign, rcFile, rcParsing, rcFormat, rcIncorrect )
#define MANDATORY_DATA_FIELDS_NUMBER 8

/*=============== VcfDataLine ================*/
static
rc_t VcfDataLineMake(VcfDataLine** pself)
{
    if ( pself == NULL )
        return RC ( rcAlign, rcFile, rcReading, rcSelf, rcNull );
    else
    {
        VcfDataLine* self;
        rc_t rc = 0;
        
        *pself = NULL;
        
        self = malloc(sizeof(VcfDataLine));
        if (self == NULL)
            return RC ( rcAlign, rcFile, rcReading, rcMemory, rcExhausted );
        
        memset(self, 0, sizeof(VcfDataLine));
        
        rc = VNamelistMake( &self->genotypeFields, GENOTYPE_LIST_BLOCK_SIZE);
        if (rc == 0)
            *pself = self;
        else
            free(self);
        return rc;
    }
}

static
rc_t VcfDataLineWhack(VcfDataLine* self)
{
    rc_t rc = 0;
    
    if (self != NULL)
        rc = VNamelistRelease(self->genotypeFields);
        
    free(self);
    
    return rc;
}

/*=============== VcfReader ================*/

struct VcfReader
{
    char* input;
    size_t inputSize;
    size_t curPos;
    VCFParseBlock pb;
    
    Vector lines;  /* the element type is VcfDataLine* */
    
    VNamelist* messages;
};

/* bison helpers */
static size_t Input(VCFParseBlock* pb, char* buf, size_t maxSize);
static void Error(VCFParseBlock* pb, const char* message);
static void AddMetaLine(VCFParseBlock* pb, VCFToken* key, VCFToken* value);
static void OpenMetaLine(VCFParseBlock* pb, VCFToken* key);
static void KeyValue(VCFParseBlock* pb, VCFToken* key, VCFToken* value);
static void HeaderItem(VCFParseBlock* pb, VCFToken* value);
static void CloseMetaLine(VCFParseBlock* pb);
static void OpenDataLine(VCFParseBlock* pb);
static void DataItem(VCFParseBlock* pb, VCFToken* value);
static void CloseDataLine(VCFParseBlock* pb);

static 
rc_t VcfReaderInit(VcfReader* self)
{
    self->input = NULL;
    
    self->pb.self           = self;
    self->pb.input          = Input;
    self->pb.error          = Error;
    
    self->pb.metaLine       = AddMetaLine;
    
    self->pb.openMetaLine   = OpenMetaLine;
    self->pb.keyValue       = KeyValue;
    self->pb.headerItem     = HeaderItem;
    self->pb.closeMetaLine  = CloseMetaLine;
    
    self->pb.openDataLine   = OpenDataLine;
    self->pb.dataItem       = DataItem;
    self->pb.closeDataLine  = CloseDataLine;
    
    VectorInit( &self->lines, 0, LINE_VECTOR_BLOCK_SIZE );
    
    return VNamelistMake( &self->messages, MESSAGE_LIST_BLOCK_SIZE);
}

static void CC WhackLineVectorElement( void *item, void *data )
{
    VcfDataLine* elem = (VcfDataLine*)item;
    VcfDataLineWhack(elem);
}

rc_t VcfReaderWhack( struct VcfReader* self)
{
    rc_t rc = 0;

    if ( self == NULL )
        return RC ( rcAlign, rcFile, rcDestroying, rcSelf, rcNull );

    VectorWhack( &self->lines, WhackLineVectorElement, NULL );

    rc = VNamelistRelease( self->messages );
    
    free(self->input);
    free(self);

    return rc;
}

rc_t VcfReaderMake( const struct VcfReader **pself)
{
    if ( pself == NULL )
        return RC ( rcAlign, rcFile, rcConstructing, rcSelf, rcNull );
        
    *pself = malloc(sizeof(VcfReader));
    if ( *pself == NULL )
        return RC ( rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted );

    return VcfReaderInit((VcfReader*)*pself);
}

/*=============== callbacks for the bison parser ================*/
static size_t Input(VCFParseBlock* pb, char* buf, size_t maxSize)
{
    VcfReader* self = (VcfReader*)(pb->self);
    size_t ret = string_copy(buf, maxSize, self->input + self->curPos, self->inputSize - self->curPos);
    
    self->curPos += ret;
    
    return ret;
}
static void Error(VCFParseBlock* pb, const char* message)
{
    char buf[1024];
    VcfReader* self = (VcfReader*)(pb->self);
    string_printf(buf, sizeof(buf), NULL, 
                  "line %d column %d: %s", 
                  pb->lastToken->line_no, pb->lastToken->column_no, message);
    VNamelistAppend(self->messages, buf);
}

static void AddMetaLine(VCFParseBlock* pb, VCFToken* key, VCFToken* value)
{
}
static void OpenMetaLine(VCFParseBlock* pb, VCFToken* key)
{
}
static void CloseMetaLine(VCFParseBlock* pb)
{
}
static void KeyValue(VCFParseBlock* pb, VCFToken* key, VCFToken* value)
{
}
static void HeaderItem(VCFParseBlock* pb, VCFToken* value)
{
}

static void OpenDataLine(VCFParseBlock* pb)
{
    VcfReader* self;
    VcfDataLine* line;
    rc_t rc = 0;

    assert(pb);

    self = (VcfReader*)(pb->self);
    assert(self);
    
    /* create new line object */
    VcfDataLineMake( &line );
    if (rc == 0)
    {   /* append to the vector */
        rc = VectorAppend( &self->lines, NULL, line );
        if (rc != 0)
        {
            SET_RC_FILE_FUNC_LINE(rc);
            Error(pb, "failed to append a line object");
        }
    }
    else
    {
        SET_RC_FILE_FUNC_LINE(rc);
        Error(pb, "failed to create a line object");
    }
}
static void DataItem(VCFParseBlock* pb, VCFToken* value)
{
    VcfReader* self = (VcfReader*)(pb->self);
    VcfDataLine* line;

    assert(pb);
    
    self = (VcfReader*)(pb->self);
    assert(self);
    
    line = (VcfDataLine*) VectorLast( & self->lines );
    assert(line);
    
    #define SAVE_TOKEN(field) StringInit( field, self->input + value->tokenStart, value->tokenLength, (uint32_t)string_size(value->tokenText) )

    switch (line->lastPopulated)
    {
    case 0: /*String      chromosome; */
        SAVE_TOKEN(&line->chromosome);
        break;
        
    case 1: /*uint32_t    position;  */
        {   
            char* endptr;
            long val = strtol(value->tokenText, &endptr, 10);
            if (*endptr || val < 0 || val > UINT32_MAX)
                Error(pb, "invalid numeric value for 'position'");
            else
                line->position = (uint32_t)val;
                
            break;
        }
    case 2: /*String      id; */
        SAVE_TOKEN(&line->id);
        break;
    case 3: /*String      refBases; */
        SAVE_TOKEN(&line->refBases);
        break;
    case 4: /*String      altBases;  */
        SAVE_TOKEN(&line->altBases);
        break;
    case 5: /*uint8_t     quality;  */
        {   
            char* endptr;
            long val = strtol(value->tokenText, &endptr, 10);
            if (*endptr || val < 0 || val > UINT8_MAX)
                Error(pb, "invalid numeric value for 'quality'");
            else
                line->quality = (uint32_t)val;
                
            break;
        }
    case 6: /*String      filter; */
        SAVE_TOKEN(&line->filter);
        break;
    case 7: /*String      info; */ 
        SAVE_TOKEN(&line->info);
        break;
    default: /* add to the genotypeFields */
        {
            rc_t rc = 0;
            String f;
            SAVE_TOKEN(&f);
            rc = VNamelistAppendString(line->genotypeFields, &f);
            if (rc != 0)
            {
                SET_RC_FILE_FUNC_LINE(rc);
                Error(pb, "failed to append a genotype field");
            }
            break;
        }
    }
    
    ++line->lastPopulated;
}
static void CloseDataLine(VCFParseBlock* pb)
{   
    /* check if the line had enough data fields */
    VcfReader* self = (VcfReader*)(pb->self);
    VcfDataLine* line;

    assert(pb);
    
    self = (VcfReader*)(pb->self);
    assert(self);
    
    line = (VcfDataLine*) VectorLast( & self->lines );
    assert(line);
    
    if (line->lastPopulated < MANDATORY_DATA_FIELDS_NUMBER)
    {
        -- pb->lastToken->line_no; /* this happens after the EOL has been processed, */
                                   /* and the line # reported by flex incremented; fix that for error reporting */
        Error(pb, "one or more of the 8 mandatory columns are missing");
    }
}


rc_t VcfReaderParse( struct VcfReader *self, struct KFile* inputFile, const struct VNamelist** messages)
{
    rc_t rc = 0;
    uint32_t messageCount;
    KMMap* mm;
    
    if ( self == NULL )
        return RC ( rcAlign, rcFile, rcParsing, rcSelf, rcNull );
        
    if (inputFile == NULL)
        return RC ( rcAlign, rcFile, rcParsing, rcParam, rcNull );
        
    VNameListCount ( self->messages, &messageCount );       
    if (messageCount > 0)
    {   /* blow away old mesages */
        rc = VNamelistRelease( self->messages );
        if (rc == 0)
            rc = VNamelistMake( &self->messages, MESSAGE_LIST_BLOCK_SIZE);
        if (rc != 0)
            return rc;
    }
        
    rc = KMMapMakeRead ( (const KMMap**)& mm, inputFile );
    if ( rc == 0 )
    {
        rc_t rc2 = 0;
        
        const void * ptr;
        rc = KMMapAddrRead ( mm, & ptr );
        if ( rc == 0 )
        {
            rc = KMMapSize ( mm, & self->inputSize);
            if ( rc == 0 )
            {
                /* make a 0-terminated copy for parsing */
                self->input = malloc(self->inputSize+1);
                if (self->input == 0)
                    rc = RC ( rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted );
                else
                {
                    string_copy(self->input, self->inputSize+1, ptr, self->inputSize);
                    self->curPos = 0;
                    VCFScan_yylex_init(&self->pb, false);
                    
                    if (VCF_parse(&self->pb) == 0)
                        rc = PARSE_ERROR;
                    else
                    {
                        VNameListCount ( self->messages, &messageCount );       
                        if (messageCount > 0)
                            rc = PARSE_ERROR;
                    }
                                        
                    *messages = (const struct VNamelist*)self->messages;
                        
                    VCFScan_yylex_destroy(&self->pb);
                }
            }
        }
        else if (rc == RC ( rcFS, rcMemMap, rcAccessing, rcMemMap, rcInvalid )) /* 0 size file */
        {
            VNamelistAppend(self->messages, "Empty file");
            *messages = (const struct VNamelist*)self->messages;
            rc = PARSE_ERROR;
        }
           
        rc2 = KMMapRelease ( mm );
        if (rc == 0)
            rc = rc2;
    }
    
    return rc;
}

rc_t VcfReaderGetDataLineCount( const VcfReader* self, uint32_t* count )
{
    if ( self == NULL )
        return RC ( rcAlign, rcFile, rcAccessing, rcSelf, rcNull );
        
    if ( count == NULL )
        return RC ( rcAlign, rcFile, rcAccessing, rcParam, rcNull );
        
    *count = VectorLength( & self->lines );
    
    return 0;
}

rc_t VcfReaderGetDataLine( const VcfReader* self, uint32_t index, const VcfDataLine** line )
{
    if ( self == NULL )
        return RC ( rcAlign, rcFile, rcAccessing, rcSelf, rcNull );
        
    if ( line == NULL )
        return RC ( rcAlign, rcFile, rcAccessing, rcParam, rcNull );
        
    *line = VectorGet( & self->lines, index );
    
    return 0;
}

