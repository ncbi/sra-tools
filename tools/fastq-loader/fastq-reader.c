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

typedef struct FastqReaderFile  FastqReaderFile;
typedef struct FastqRecord      FastqRecord;
typedef struct FastqSequence    FastqSequence;

#define READERFILE_IMPL FastqReaderFile
#define RECORD_IMPL     FastqRecord
#define SEQUENCE_IMPL   FastqSequence

#include "fastq-reader.h"
#include "fastq-parse.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <kapp/loader-file.h>
#include <kfs/directory.h>
#include <klib/log.h>
#include <klib/rc.h>

static rc_t FastqSequenceInit(FastqSequence* self);

/*--------------------------------------------------------------------------
 * FastqRecord
 */
static rc_t FastqRecordAddRef   ( const FastqRecord* self );
static rc_t FastqRecordRelease  ( const FastqRecord* self );
static rc_t FastqRecordGetSequence  ( const FastqRecord* self, const Sequence** result);
static rc_t FastqRecordGetAlignment ( const FastqRecord* self, const Alignment** result);
static rc_t FastqRecordGetRejected  ( const FastqRecord* self, const Rejected** result);

static Record_vt_v1 FastqRecord_vt = 
{
    1, 0, 
    /* start minor version == 0 */
    FastqRecordAddRef,
    FastqRecordRelease,
    FastqRecordGetSequence,
    FastqRecordGetAlignment,
    FastqRecordGetRejected,
    /* end minor version == 0 */
};

static rc_t CC FastqRecordInit ( FastqRecord* self )
{
    assert(self);
    self->dad.vt.v1 = & FastqRecord_vt; 
    KDataBufferMakeBytes ( & self->source, 0 );
    self->rej = 0;
    return FastqSequenceInit(& self->seq);
}

static rc_t FastqRecordWhack( const FastqRecord* cself )
{
    rc_t rc = 0;
    FastqRecord* self = (FastqRecord*)cself;
    assert(self);
    
    rc = KDataBufferWhack( & self->source );
    
    if (rc)
        RejectedRelease(self->rej);
    else
        rc = RejectedRelease(self->rej);
        
    free(self);    
      
    return rc;
}

static rc_t FastqRecordAddRef( const FastqRecord* self )
{
    assert(self);
    KRefcountAdd( & self->seq.refcount, "FastqRecord" );
    /* TODO: handle rc from KRefcountAdd */
    return 0;
}

static rc_t FastqRecordRelease( const FastqRecord* cself )
{
    if ( cself != NULL )
    {
        FastqRecord *self = ( FastqRecord* ) cself;
        switch ( KRefcountDrop ( & self ->seq.refcount, "FastqRecord" ) )
        {
        case krefWhack:
            FastqRecordWhack( self );
            break;
        default:
            /* TODO: handle other values */
            break;
        }
    }
    return 0;
}

static rc_t FastqRecordGetSequence( const FastqRecord* self, const Sequence** result )
{
    rc_t rc = 0;
    assert(result);
    *result = 0;
    if (self->rej == 0)
    {
        rc = FastqRecordAddRef(self);
        if (rc == 0)
            *result = (const Sequence*) & self->seq;
    }
    return rc;
}

static rc_t FastqRecordGetAlignment( const FastqRecord* self, const Alignment** result)
{
    *result = 0;
    return 0;
}

static rc_t FastqRecordGetRejected( const FastqRecord* self, const Rejected** result)
{
    rc_t rc = 0;
    assert(result);
    *result = 0;
    if (self->rej != 0)
    {
        rc = RejectedAddRef(self->rej);
        if (rc == 0)
            *result = self->rej;
    }
    return rc;
}

/*--------------------------------------------------------------------------
 * FastqSequence
 */

static rc_t FastqSequenceAddRef              ( const SEQUENCE_IMPL* self );
static rc_t FastqSequenceRelease             ( const SEQUENCE_IMPL* self );
static rc_t FastqSequenceGetReadLength       ( const SEQUENCE_IMPL *self, uint32_t *length );
static rc_t FastqSequenceGetRead             ( const SEQUENCE_IMPL *self, char *sequence );
static rc_t FastqSequenceGetRead2            ( const SEQUENCE_IMPL *self, char *sequence, uint32_t start, uint32_t stop );
static rc_t FastqSequenceGetQuality          ( const SEQUENCE_IMPL *self, const int8_t **quality, uint8_t *offset, int *qualType );
static rc_t FastqSequenceGetSpotGroup        ( const SEQUENCE_IMPL *self, const char **name, size_t *length );
static rc_t FastqSequenceGetSpotName         ( const SEQUENCE_IMPL *self, const char **name, size_t *length );
static bool FastqSequenceIsColorSpace        ( const SEQUENCE_IMPL *self );
static rc_t FastqSequenceGetCSKey            ( const SEQUENCE_IMPL *self, char cskey[1] );
static rc_t FastqSequenceGetCSReadLength     ( const SEQUENCE_IMPL *self, uint32_t *length );
static rc_t FastqSequenceGetCSRead           ( const SEQUENCE_IMPL *self, char *sequence );
static rc_t FastqSequenceGetCSQuality        ( const SEQUENCE_IMPL *self, const int8_t **quality, uint8_t *offset, int *qualType );
static bool FastqSequenceRecordWasPaired     ( const SEQUENCE_IMPL *self );
static int  FastqSequenceGetOrientationSelf  ( const SEQUENCE_IMPL *self );
static int  FastqSequenceGetOrientationMate  ( const SEQUENCE_IMPL *self );
static bool FastqSequenceRecordIsFirst       ( const SEQUENCE_IMPL *self );
static bool FastqSequenceRecordIsSecond      ( const SEQUENCE_IMPL *self );
static bool FastqSequenceIsDuplicate         ( const SEQUENCE_IMPL *self ); 
static bool FastqSequenceIsLowQuality        ( const SEQUENCE_IMPL *self ); 
static rc_t FastqSequenceGetTI               ( const SEQUENCE_IMPL *self, uint64_t *ti );

static Sequence_vt_v1 FastqSequence_vt = 
{
    1, 0, 
    /* start minor version == 0 */
    FastqSequenceAddRef,
    FastqSequenceRelease,
    FastqSequenceGetReadLength,
    FastqSequenceGetRead,
    FastqSequenceGetRead2,
    FastqSequenceGetQuality,
    FastqSequenceGetSpotGroup,
    FastqSequenceGetSpotName,
    FastqSequenceIsColorSpace,
    FastqSequenceGetCSKey,
    FastqSequenceGetCSReadLength,
    FastqSequenceGetCSRead,
    FastqSequenceGetCSQuality,
    FastqSequenceRecordWasPaired,
    FastqSequenceGetOrientationSelf,
    FastqSequenceGetOrientationMate,
    FastqSequenceRecordIsFirst,
    FastqSequenceRecordIsSecond,
    FastqSequenceIsDuplicate,  
    FastqSequenceIsLowQuality, 
    FastqSequenceGetTI,        
    /* end minor version == 0 */
};

static
rc_t
FastqSequenceInit(FastqSequence* self)
{
    self->sequence_vt.v1 = & FastqSequence_vt;
    KRefcountInit ( & self -> refcount, 1, "FastqSequence", "FastqSequenceInit", "");

    StringInit(&self->spotname, 0, 0, 0);
    StringInit(&self->spotgroup, 0, 0, 0);
    self->readnumber = 0; 
    StringInit(&self->read, 0, 0, 0);
    self->is_colorspace = false;
    StringInit(&self->quality, 0, 0, 0);
    self->qualityOffset = 0;
    self->lowQuality = false;

    return 0;
}

static const FastqRecord*
FastqSequenceToRecord(const FastqSequence* seq) 
{
    return ( const FastqRecord * ) ( (uint8_t*)seq - offsetof ( FastqRecord, seq ) );
}

static rc_t FastqSequenceAddRef( const FastqSequence* self )
{
    switch (KRefcountAdd( & self->refcount, "FastqSequence" ))
    {
    case krefLimit:
        return RC ( RC_MODULE, rcData, rcAttaching, rcRange, rcExcessive );
    case krefNegative:
        return RC ( RC_MODULE, rcData, rcAttaching, rcRefcount, rcInvalid );
    }
    return 0;
}

static rc_t FastqSequenceRelease( const FastqSequence* self )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "FastqSequence" ) )
        {
        case krefWhack:
            return FastqRecordWhack ( FastqSequenceToRecord(self) );

        case krefNegative:
            return RC ( RC_MODULE, rcData, rcDestroying, rcSelf, rcDestroyed );
        }
    }

    return 0;

}

static rc_t FastqSequenceGetReadLength ( const FastqSequence *self, uint32_t *length )
{
    assert(self);
    if ( self->is_colorspace )
    {
        *length = 0;
    }
    else
    {
        *length = self->read.len;
    }
    return 0;
}

static rc_t FastqSequenceGetRead ( const FastqSequence *self, char *sequence )
{
    assert(self);
    assert(sequence);
    if ( !self->is_colorspace )
        string_copy(sequence, self->read.len, self->read.addr, self->read.len);
    return 0;
}

static rc_t FastqSequenceGetRead2 ( const FastqSequence *self, char *sequence, uint32_t start, uint32_t stop )
{
    assert(self);
    assert(sequence);
    if ( self->is_colorspace )
    {
        return RC( RC_MODULE, rcData, rcAccessing, rcItem, rcEmpty );
    }
    else
    {
        uint32_t length = self->read.len;
        if (start >= length || stop >= length || start >= stop)
        {
            return RC( RC_MODULE, rcData, rcAccessing, rcRange, rcInvalid);
        }
        string_copy(sequence, stop - start, self->read.addr + start, stop - start);
    }
    return 0;
}

static rc_t FastqSequenceGetQuality ( const FastqSequence *self, const int8_t **quality, uint8_t *offset, int *qualType )
{
    assert(self);
    assert(quality);
    assert(offset);
    assert(qualType);
    *quality = NULL;
    if ( self->quality.size != 0)
    {
        uint32_t length = self->read.len;
        if (self->quality.size != length)
            return RC(rcAlign, rcRow, rcReading, rcData, rcInconsistent);
            
        *quality = (const int8_t *)self->quality.addr;
        *offset = self->qualityOffset;
    }
    else
    {
        *quality = NULL;
        *offset = 0;
    }
    *qualType = QT_Phred;
    return 0;
}

static rc_t FastqSequenceGetSpotGroup ( const FastqSequence *self, const char **name, size_t *length )
{
    assert(self);
    assert(name);
    *name = self->spotgroup.addr;
    *length = self->spotgroup.len;
    return 0;
}

static rc_t FastqSequenceGetSpotName ( const FastqSequence *self, const char **name, size_t *length )
{
    assert(self);
    assert(name);
    *name = self->spotname.addr;
    *length = self->spotname.len;
    return 0;
}

static bool FastqSequenceIsColorSpace ( const FastqSequence *self )
{
    assert(self);
    return self->is_colorspace;
}

static rc_t FastqSequenceGetCSKey ( const FastqSequence *self, char cskey[1] )
{
    assert(self);
    if (self->is_colorspace)
        cskey[0] = self->read.addr[0];
    return 0;
}

static rc_t FastqSequenceGetCSReadLength ( const FastqSequence *self, uint32_t *length )
{
    assert(self);
    if (self->is_colorspace)
        *length = self->read.len - 1;
    return 0;
}

static rc_t FastqSequenceGetCSRead ( const FastqSequence *self, char *sequence )
{
    assert(self);
    assert(sequence);
    if ( self->is_colorspace )
        string_copy(sequence, self->read.len - 1, self->read.addr + 1, self->read.len - 1);
    return 0;
}

static rc_t FastqSequenceGetCSQuality ( const FastqSequence *self, const int8_t **quality, uint8_t *offset, int *qualType )
{
    assert(self);
    assert(quality);
    assert(offset);
    assert(qualType);
    *quality = NULL;
    if ( self->quality.size != 0 && self->is_colorspace )
    {
        *quality = (const int8_t *)self->quality.addr + 1;
        *offset = self->qualityOffset;
    }
    else
    {
        *quality = NULL;
        *offset = 0;
    }
    *qualType = QT_Phred;
    return 0;
}

static bool FastqSequenceRecordWasPaired ( const FastqSequence *self )
{
    assert(self);
    return self->readnumber != 0;
}

static int FastqSequenceGetOrientationSelf ( const FastqSequence *self )
{
    assert(self);
    return ReadOrientationUnknown;
}

static int FastqSequenceGetOrientationMate ( const FastqSequence *self )
{
    assert(self);
    return ReadOrientationUnknown;
}

static bool FastqSequenceRecordIsFirst ( const FastqSequence *self )
{
    assert(self);
    return self->readnumber == 1;
}

static bool FastqSequenceRecordIsSecond ( const FastqSequence *self )
{
    assert(self);
    return self->readnumber == 2;
}

static bool FastqSequenceIsDuplicate         ( const SEQUENCE_IMPL *self )
{
    assert(self);
    return 0;
}

static bool FastqSequenceIsLowQuality        ( const SEQUENCE_IMPL *self )
{
    assert(self);
    return self->lowQuality;
}

static rc_t FastqSequenceGetTI               ( const SEQUENCE_IMPL *self, uint64_t *ti )
{
    assert(self);
    return false;
}

/*--------------------------------------------------------------------------
 * FastqReaderFile
 */

static rc_t FastqReaderFileWhack( READERFILE_IMPL* self );
static rc_t FastqReaderFileGetRecord ( const READERFILE_IMPL *self, const Record** result );
static float FastqReaderFileGetProportionalPosition ( const READERFILE_IMPL *self );
static rc_t FastqReaderFileGetReferenceInfo ( const READERFILE_IMPL *self, const ReferenceInfo** result );

static ReaderFile_vt_v1 FastqReaderFile_vt = 
{
    1, 0, 
    /* start minor version == 0 */
    FastqReaderFileWhack,
    FastqReaderFileGetRecord,
    FastqReaderFileGetProportionalPosition,
    FastqReaderFileGetReferenceInfo,
    /* end minor version == 0 */
};

struct FastqReaderFile
{
    ReaderFile dad;

    FASTQParseBlock pb; 
    const KLoaderFile* reader;

    const char* recordStart; /* raw source of the record being currently parsed */
    size_t curPos;           /* current tokenization position relative to recordStart */
    bool lastEol;
    bool eolInserted;
};

rc_t FastqReaderFileWhack( FastqReaderFile* f )
{
    FastqReaderFile* self = (FastqReaderFile*) f;

    FASTQScan_yylex_destroy(& self->pb);

    if (self->reader)
        KLoaderFile_Release ( self->reader, true );

    ReaderFileWhack( &self->dad );

    free ( self );
    return 0;
}

void FASTQ_ParseBlockInit ( FASTQParseBlock* pb )
{
    pb->length = 0;
    pb->spotNameOffset = 0;
    pb->spotNameLength = 0;
    pb->spotNameDone = false;
    pb->spotGroupOffset = 0;
    pb->spotGroupLength = 0;
    pb->readOffset = 0;
    pb->readLength = 0;
    pb->qualityOffset = 0;
    pb->qualityLength = 0;
}

rc_t FastqReaderFileGetRecord ( const FastqReaderFile *f, const Record** result )
{
    rc_t rc;
    FastqReaderFile* self = (FastqReaderFile*) f;
    
    if (self->pb.fatalError)
        return 0;

    self->pb.record = (FastqRecord*)malloc(sizeof(FastqRecord));
    if (self->pb.record == NULL)
    {
        rc = RC ( RC_MODULE, rcData, rcAllocating, rcMemory, rcExhausted );
        return 0;
    }
    rc = FastqRecordInit(self->pb.record);
    if (rc != 0)
        return rc;

    FASTQ_ParseBlockInit( & self->pb );
    
    if ( FASTQ_parse( & self->pb ) == 0 && self->pb.record->rej == 0 )
    {   /* normal end of input */
        RecordRelease((const Record*)self->pb.record);
        *result = 0;
        return 0;
    }
    
    /*TODO: remove? compensate for an artificially inserted trailing \n */
    if ( self->eolInserted )
    {
        -- self->pb.length;
        self->eolInserted = false;
    }

    if (self->pb.record->rej != 0) /* had error(s) */
    {   /* save the complete raw source in the Rejected object */
        StringInit(& self->pb.record->rej->source, string_dup(self->recordStart, self->pb.length), self->pb.length, (uint32_t)self->pb.length);
        self->pb.record->rej->fatal = self->pb.fatalError;
    }

    if (rc == 0 && self->reader != 0)
    {   
        /* advance the record start pointer beyond the last token */ 
        size_t length;
        rc = KLoaderFile_Read( self->reader, self->pb.length, 0, (const void**)& self->recordStart, & length);
        if (rc != 0)
            LogErr(klogErr, rc, "FastqReaderFileGetRecord failed");

        self->curPos -= self->pb.length;
    }

    StringInit( & self->pb.record->seq.spotname,    (const char*)self->pb.record->source.base + self->pb.spotNameOffset,    self->pb.spotNameLength, (uint32_t)self->pb.spotNameLength);
    StringInit( & self->pb.record->seq.spotgroup,   (const char*)self->pb.record->source.base + self->pb.spotGroupOffset,   self->pb.spotGroupLength, (uint32_t)self->pb.spotGroupLength);
    StringInit( & self->pb.record->seq.read,        (const char*)self->pb.record->source.base + self->pb.readOffset,        self->pb.readLength, (uint32_t)self->pb.readLength); 
    StringInit( & self->pb.record->seq.quality,     (const char*)self->pb.record->source.base + self->pb.qualityOffset,     self->pb.qualityLength, (uint32_t)self->pb.qualityLength); 
    self->pb.record->seq.qualityOffset = self->pb.phredOffset;
    
    if (self->pb.record->seq.readnumber == 0)
        self->pb.record->seq.readnumber = self->pb.defaultReadNumber;
    
    *result = (const Record*) self->pb.record;

    return rc;
}

void CC FASTQ_error(struct FASTQParseBlock* sb, const char* msg)
{
    if (sb->record->rej == 0)
    {   /* save the error information in the Rejected object */
        sb->record->rej = (Rejected*)malloc(sizeof(Rejected));
        RejectedInit(sb->record->rej);

        sb->record->rej->message    = string_dup(msg, strlen(msg));
        sb->record->rej->line       = sb->lastToken->line_no;
        sb->record->rej->column     = sb->lastToken->column_no;
    }
    /* subsequent errors in this record will be ignored */
}

size_t CC FASTQ_input(FASTQParseBlock* pb, char* buf, size_t max_size)
{
    FastqReaderFile* self = (FastqReaderFile*)pb->self;
    size_t length;

    rc_t rc = KLoaderFile_Read( self->reader, 0, self->curPos + max_size, (const void**)& self->recordStart, & length);

    if ( rc != 0 )
    {
        LogErr(klogErr, rc, "FASTQ_input failed");
        return 0;
    }
    
    length -= self->curPos;
    if ( length == 0 ) /* nothing new read = end of file */
    {   /* insert an additional \n before the end of file if missing */
        if ( !self->lastEol )
        {
            buf[0] = '\n';
            self->eolInserted = true;
            self->lastEol = true;
            return 1;
        }
        else
        {   
            return 0; /* signal EOF to flex */
        }
    }

    memcpy(buf, self->recordStart + self->curPos, length);

    self->lastEol = ( buf[length-1] == '\n' );
    self->curPos += length;
    
    return length;
}

rc_t CC FastqReaderFileMake( const ReaderFile **reader, 
                             const KDirectory* dir, 
                             const char* file, 
                             uint8_t phredOffset, 
                             uint8_t phredMax, 
                             int8_t defaultReadNumber,
                             bool ignoreSpotGroups)
{
    rc_t rc;
    FastqReaderFile* self = (FastqReaderFile*) malloc ( sizeof * self );
    if ( self == NULL )
    {
         rc = RC ( RC_MODULE, rcFileFormat, rcAllocating, rcMemory, rcExhausted );
    }
    else
    {
        memset(self, 0, sizeof(*self));
        rc = ReaderFileInit ( self );
        self->dad.vt.v1 = & FastqReaderFile_vt;

        self->dad.pathname = string_dup(file, strlen(file)+1);
        if ( self->dad.pathname == NULL )
        {
            rc = RC ( RC_MODULE, rcFileFormat, rcAllocating, rcMemory, rcExhausted );
        }
        else
        {
            rc = KLoaderFile_Make( & self->reader, dir, file, 0, true );
        }
        if (rc == 0)
        {
            self->pb.self = self;
            self->pb.input = FASTQ_input;    
            self->pb.phredOffset = phredOffset;
            self->pb.maxPhred = phredMax;
            /* TODO: 
                if phredOffset is 0, 
                    guess based on the raw values on the first quality line:
                        if all values are above MAX_PHRED_33, phredOffset = 64
                        if all values are in MIN_PHRED_33..MAX_PHRED_33, phredOffset = 33
                        if any value is below MIN_PHRED_33, abort
                    if the guess is 33 and proven wrong (a raw quality value >MAX_PHRED_33 is encountered and no values below MIN_PHRED_64 ever seen)
                        reopen the file, 
                        phredOffset = 64
                        try to parse again
                        if a value below MIN_PHRED_64 seen, abort 
            */
            self->pb.defaultReadNumber = defaultReadNumber;
            self->pb.secondaryReadNumber = 0;
            self->pb.ignoreSpotGroups = ignoreSpotGroups;
            
            rc = FASTQScan_yylex_init(& self->pb, false); 
            if (rc == 0)
            {
                *reader = (const ReaderFile *) self;
            } 
            else
            {
                KLoaderFile_Release( self->reader, true );
                ReaderFileRelease( & self->dad );
                *reader = 0;
            }
        }
        else
        {
            ReaderFileRelease( & self->dad );
            *reader = 0;
        }
    }
 
    return rc;
}

float FastqReaderFileGetProportionalPosition ( const READERFILE_IMPL *self )
{
    return 0.0f;
}

rc_t FastqReaderFileGetReferenceInfo ( const READERFILE_IMPL *self, const ReferenceInfo** result )
{
    *result = NULL;
    return 0;
}
