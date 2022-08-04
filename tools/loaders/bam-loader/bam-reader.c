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
 
typedef struct BamReaderFile    BamReaderFile;
typedef struct BamRecord        BamRecord;
typedef struct BamSequence      BamSequence;
typedef struct BamAlignment     BamAlignment;
typedef struct BamCGData        BamCGData;
typedef struct BamReferenceInfo BamReferenceInfo;

#define READERFILE_IMPL     BamReaderFile
#define RECORD_IMPL         BamRecord
#define SEQUENCE_IMPL       BamSequence
#define ALIGNMENT_IMPL      BamAlignment
#define CGDATA_IMPL         BamCGData
#define REFERENCEINFO_IMPL  BamReferenceInfo

#include "bam-reader.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <loader/common-reader-priv.h>

struct BamSequence
{
    Sequence    dadSeq;
    Alignment   dadAlign;
    CGData      dadCG;
    KRefcount   refcount;
    
    const BAMAlignment* bam;    
    uint16_t flags;
};

struct BamRecord
{
    Record  dad;

    BamSequence seq;
    Rejected*   rej; 
};
/*TODO: remove when done refactoring */
const BAMAlignment *ToBamAlignment(const Record* record)
{
   assert(record);
   return ((const BamRecord*)record)->seq.bam;
}

static rc_t BamSequenceInit(BamSequence* self);

/*--------------------------------------------------------------------------
 * BamRecord
 */
static rc_t BamRecordAddRef   ( const BamRecord* self );
static rc_t BamRecordRelease  ( const BamRecord* self );
static rc_t BamRecordGetSequence  ( const BamRecord* self, const Sequence** result);
static rc_t BamRecordGetAlignment ( const BamRecord* self, const Alignment** result);
static rc_t BamRecordGetRejected  ( const BamRecord* self, const Rejected** result);

static Record_vt_v1 BamRecord_vt = 
{
    1, 0, 
    /* start minor version == 0 */
    BamRecordAddRef,
    BamRecordRelease,
    BamRecordGetSequence,
    BamRecordGetAlignment,
    BamRecordGetRejected,
    /* end minor version == 0 */
};

static rc_t CC BamRecordInit ( BamRecord* self )
{
    assert(self);
    self->dad.vt.v1 = & BamRecord_vt; 
    self->rej = 0;
    return BamSequenceInit(& self->seq);
}

static rc_t BamRecordWhack( const BamRecord* self )
{
    rc_t rc = 0;
    assert(self);
    
    rc = BAMAlignmentRelease(self->seq.bam);

    if (rc != 0)
        RejectedRelease(self->rej);
    else
        rc = RejectedRelease(self->rej);

    free ( (BamRecord*) self );
    
    return rc;
}

static rc_t BamRecordAddRef( const BamRecord* self )
{
    assert(self);
    KRefcountAdd( & self->seq.refcount, "BamRecord" );
    /* TODO: handle rc from KRefcountAdd */
    return 0;
}

static rc_t BamRecordRelease( const BamRecord* cself )
{
    if ( cself != NULL )
    {
        BamRecord *self = ( BamRecord* ) cself;
        switch ( KRefcountDrop ( & self ->seq.refcount, "BamRecord" ) )
        {
        case krefWhack:
            BamRecordWhack( self );
            break;
        default:
            /* TODO: handle other values */
            break;
        }
    }
    return 0;
}

static rc_t BamRecordGetSequence( const BamRecord* self, const Sequence** result )
{
    rc_t rc = 0;
    assert(result);
    rc = BamRecordAddRef(self);
    if (rc == 0)
        *result = (const Sequence*) & self->seq;
    else
        *result = 0;
    return rc;
}

static rc_t BamRecordGetAlignment( const BamRecord* self, const Alignment** result)
{
    rc_t rc = 0;
    assert(result);

    /* do not allow access is flagged as unmapped*/
    if ((self->seq.flags & BAMFlags_SelfIsUnmapped) != 0)
        *result = 0;
    else
    {
        rc = BamRecordAddRef(self);
        if (rc == 0)
            *result = (const Alignment*) & self->seq.dadAlign;
        else
            *result = 0;
    }
    return rc;
}

static rc_t BamRecordGetRejected( const BamRecord* self, const Rejected** result)
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
 * BamSequence forwards
 */
static rc_t BamSequenceAddRef                   ( const BamSequence* self );
static rc_t BamSequenceRelease                  ( const BamSequence* self );
static rc_t BamSequenceGetReadLength            ( const BamSequence *self, uint32_t *length );
static rc_t BamSequenceGetRead                  ( const BamSequence *self, char *sequence );
static rc_t BamSequenceGetRead2                 ( const BamSequence *self, char *sequence, uint32_t start, uint32_t stop );
static rc_t BamSequenceGetQuality               ( const BamSequence *self, const int8_t **quality, uint8_t *offset, int *qualType );
static rc_t BamSequenceGetSpotGroup             ( const BamSequence *self, const char **name, size_t *length );
static rc_t BamSequenceGetSpotName              ( const BamSequence *self, const char **name, size_t *length );
static bool BamSequenceIsColorSpace             ( const BamSequence *self );
static rc_t BamSequenceGetCSKey                 ( const BamSequence *self, char cskey[1] );
static rc_t BamSequenceGetCSReadLength          ( const BamSequence *self, uint32_t *length );
static rc_t BamSequenceGetCSRead                ( const BamSequence *self, char *sequence );
static rc_t BamSequenceGetCSQuality             ( const BamSequence *self, const int8_t **quality, uint8_t *offset, int *qualType );
static bool BamSequenceRecordWasPaired          ( const BamSequence *self );
static int  BamSequenceRecordOrientationSelf    ( const BamSequence *self );
static int  BamSequenceRecordOrientationMate    ( const BamSequence *self );
static bool BamSequenceRecordIsFirst            ( const BamSequence *self );
static bool BamSequenceRecordIsSecond           ( const BamSequence *self );
static bool BamSequenceRecordIsDuplicate        ( const BamSequence *self );
static bool BamSequenceIsLowQuality             ( const BamSequence *self ); 
static rc_t BamSequenceGetTI                    ( const BamSequence *self, uint64_t *ti );

/*--------------------------------------------------------------------------
 * BamCGData
 */

static rc_t BamCGDataAddRef         ( const BamCGData* self );
static rc_t BamCGDataRelease        ( const BamCGData* self );
static rc_t BamCGDataGetSeqQual     ( const BamCGData* self, char sequence[/* 35 */], uint8_t quality[/* 35 */] );
static rc_t BamCGDataGetCigar       ( const BamCGData* self, uint32_t *cigar, uint32_t cig_max, uint32_t *cig_act );
static rc_t BamCGDataGetAlignGroup  ( const BamCGData* self, char buffer[], size_t max_size, size_t *act_size );

/* CGGetAlignGroup
 */
rc_t CC CGDataGetAlignGroup ( const CGData* self,
                              char buffer[],
                              size_t max_size,
                              size_t *act_size );

static CGData_vt_v1 BamCGData_vt = {
    1, 0, 
    /* start minor version == 0 */
    BamCGDataAddRef,
    BamCGDataRelease,
    
    BamCGDataGetSeqQual,
    BamCGDataGetCigar,
    BamCGDataGetAlignGroup
    /* end minor version == 0 */
};

static const BamSequence*
BamCGDataToSequence(const BamCGData* cg) 
{
    return ( const BamSequence * ) ( (uint8_t*)cg- offsetof ( BamSequence, dadCG) );
}

static rc_t BamCGDataAddRef  ( const BamCGData* self )
{
    return BamSequenceAddRef(BamCGDataToSequence(self));
}

static rc_t BamCGDataRelease ( const BamCGData* self )
{
    return BamSequenceRelease(BamCGDataToSequence(self));
}

static rc_t BamCGDataGetSeqQual ( const BamCGData* self, char sequence[/* 35 */], uint8_t quality[/* 35 */] )
{
    return BAMAlignmentGetCGSeqQual(BamCGDataToSequence(self)->bam, sequence, quality);
}

static rc_t BamCGDataGetCigar ( const BamCGData* self, uint32_t *cigar, uint32_t cig_max, uint32_t *cig_act )
{
    return BAMAlignmentGetCGCigar(BamCGDataToSequence(self)->bam, cigar, cig_max, cig_act);
}

static rc_t BamCGDataGetAlignGroup ( const BamCGData* self, char buffer[], size_t max_size, size_t *act_size )
{
    return BAMAlignmentGetCGAlignGroup(BamCGDataToSequence(self)->bam, buffer, max_size, act_size);
}

/*--------------------------------------------------------------------------
 * BamAlignment
 */

static rc_t BamAlignmentAddRef  ( const BamAlignment* self );
static rc_t BamAlignmentRelease ( const BamAlignment* self );

static rc_t BamAlignmentGetRefSeqId         ( const BamAlignment *self, int32_t *refSeqId );
static rc_t BamAlignmentGetMateRefSeqId     ( const BamAlignment *self, int32_t *refSeqId );
static rc_t BamAlignmentGetPosition         ( const BamAlignment *self, int64_t *pos );
static rc_t BamAlignmentGetMatePosition     ( const BamAlignment *self, int64_t *pos );
static rc_t BamAlignmentGetMapQuality       ( const BamAlignment *self, uint8_t *qual );
static rc_t BamAlignmentGetAlignmentDetail  ( const BamAlignment *self, AlignmentDetail *rslt, uint32_t count, uint32_t *actual, int32_t *firstMatch, int32_t *lastMatch );
static rc_t BamAlignmentGetAlignOpCount     ( const BamAlignment *self, uint32_t *n );
static rc_t BamAlignmentGetInsertSize       ( const BamAlignment *self, int64_t *size );
static rc_t BamAlignmentGetCGData           ( const BamAlignment *self, const CGData** cg);
static rc_t BamAlignmentGetBAMCigar         ( const BamAlignment *self, uint32_t const **rslt, uint32_t *length );
static bool BamAlignmentIsSecondary         ( const BamAlignment *self );

static Alignment_vt_v1 BamAlignment_vt = {
    1, 0, 
    /* start minor version == 0 */
    BamAlignmentAddRef,
    BamAlignmentRelease,
    BamAlignmentGetRefSeqId,
    BamAlignmentGetMateRefSeqId,
    BamAlignmentGetPosition,
    BamAlignmentGetMatePosition,
    BamAlignmentGetMapQuality,
    BamAlignmentGetAlignmentDetail,
    BamAlignmentGetAlignOpCount,
    BamAlignmentGetInsertSize,
    BamAlignmentGetCGData,
    BamAlignmentGetBAMCigar,
    BamAlignmentIsSecondary,
    /* end minor version == 0 */
};

static const BamSequence*
BamAlignmentToSequence(const BamAlignment* align) 
{
    return ( const BamSequence * ) ( (uint8_t*)align - offsetof ( BamSequence, dadAlign ) );
}

static rc_t BamAlignmentAddRef  ( const BamAlignment* self )
{
    return BamSequenceAddRef(BamAlignmentToSequence(self));
}

static rc_t BamAlignmentRelease ( const BamAlignment* self )
{
    return BamSequenceRelease(BamAlignmentToSequence(self));
}

static rc_t BamAlignmentGetRefSeqId        ( const BamAlignment *self, int32_t *refSeqId )
{
    return BAMAlignmentGetRefSeqId(BamAlignmentToSequence(self)->bam, refSeqId);
}

static rc_t BamAlignmentGetMateRefSeqId    ( const BamAlignment *self, int32_t *refSeqId )
{
    return BAMAlignmentGetMateRefSeqId(BamAlignmentToSequence(self)->bam, refSeqId);
}

static rc_t BamAlignmentGetPosition        ( const BamAlignment *self, int64_t *pos )
{
    return BAMAlignmentGetPosition(BamAlignmentToSequence(self)->bam, pos);
}

static rc_t BamAlignmentGetMatePosition     ( const BamAlignment *self, int64_t *pos )
{
    return BAMAlignmentGetMatePosition(BamAlignmentToSequence(self)->bam, pos);
}

static rc_t BamAlignmentGetMapQuality      ( const BamAlignment *self, uint8_t *qual )
{
    return BAMAlignmentGetMapQuality(BamAlignmentToSequence(self)->bam, qual);
}

static rc_t BamAlignmentGetAlignmentDetail ( const BamAlignment *self, AlignmentDetail *rslt, uint32_t count, uint32_t *actual, int32_t *firstMatch, int32_t *lastMatch )
{
    return BAMAlignmentGetAlignmentDetail ( BamAlignmentToSequence(self)->bam, (BAMAlignmentDetail*)rslt, count, actual, firstMatch, lastMatch );
}

static rc_t BamAlignmentGetAlignOpCount    ( const BamAlignment *self, uint32_t *n )
{
    return BAMAlignmentGetCigarCount ( BamAlignmentToSequence(self)->bam, n );
}

static rc_t BamAlignmentGetInsertSize      ( const BamAlignment *self, int64_t *size )
{
    return BAMAlignmentGetInsertSize ( BamAlignmentToSequence(self)->bam, size );
}

static rc_t BamAlignmentGetCGData ( const BamAlignment *self, const CGData** cg)
{
    rc_t rc = 0;
    if (BAMAlignmentHasCGData(BamAlignmentToSequence(self)->bam))
    {
        rc_t rc = BamAlignmentAddRef(self);
        if (rc == 0)
            *cg = (const CGData*) & BamAlignmentToSequence(self)->dadCG;
        else
            *cg = 0;
    }
    else
        *cg = 0;
    return rc;
}

static rc_t BamAlignmentGetBAMCigar        ( const BamAlignment *self, uint32_t const **rslt, uint32_t *length )
{
    return BAMAlignmentGetRawCigar(BamAlignmentToSequence(self)->bam, rslt, length);
}

static bool BamAlignmentIsSecondary( const BamAlignment *self )
{
    return (BamAlignmentToSequence(self)->flags & BAMFlags_IsNotPrimary) != 0;
}

/*--------------------------------------------------------------------------
 * BamSequence 
 */

static Sequence_vt_v1 BamSequence_vt = 
{
    1, 0, 
    /* start minor version == 0 */
    BamSequenceAddRef,
    BamSequenceRelease,
    BamSequenceGetReadLength,
    BamSequenceGetRead,  
    BamSequenceGetRead2,
    BamSequenceGetQuality,
    BamSequenceGetSpotGroup,
    BamSequenceGetSpotName, 
    BamSequenceIsColorSpace,
    BamSequenceGetCSKey,
    BamSequenceGetCSReadLength,
    BamSequenceGetCSRead,
    BamSequenceGetCSQuality,
    BamSequenceRecordWasPaired,
    BamSequenceRecordOrientationSelf,
    BamSequenceRecordOrientationMate,
    BamSequenceRecordIsFirst,
    BamSequenceRecordIsSecond,
    BamSequenceRecordIsDuplicate,
    BamSequenceIsLowQuality,
    BamSequenceGetTI,
    /* end minor version == 0 */
};

static
rc_t
BamSequenceInit(BamSequence* self)
{
    self->dadSeq.vt.v1 = & BamSequence_vt;
    self->dadAlign.vt.v1 = & BamAlignment_vt;
    self->dadCG.vt.v1 = & BamCGData_vt;
    KRefcountInit ( & self -> refcount, 1, "BamSequence", "BamSequenceInit", "");
    self->bam= 0;
    self->flags = 0;

    return 0;
}

static const BamRecord*
BamSequenceToRecord(const BamSequence* seq) 
{
    return ( const BamRecord * ) ( (uint8_t*)seq - offsetof ( BamRecord, seq ) );
}

static rc_t BamSequenceAddRef( const BamSequence* self )
{
    switch (KRefcountAdd( & self->refcount, "BamSequence" ))
    {
    case krefLimit:
        return RC ( RC_MODULE, rcData, rcAttaching, rcRange, rcExcessive );
    case krefNegative:
        return RC ( RC_MODULE, rcData, rcAttaching, rcRefcount, rcInvalid );
    }
    return 0;
}

static rc_t BamSequenceRelease( const BamSequence* self )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "BamSequence" ) )
        {
        case krefWhack:
            return BamRecordWhack ( BamSequenceToRecord(self) );

        case krefNegative:
            return RC ( RC_MODULE, rcData, rcDestroying, rcSelf, rcDestroyed );
        }
    }

    return 0;

}

static rc_t BamSequenceGetReadLength ( const BamSequence *self, uint32_t *length )
{
    assert(self);
    assert(self->bam);
    return BAMAlignmentGetReadLength(self->bam, length);
}

static rc_t BamSequenceGetRead ( const BamSequence *self, char *sequence )
{
    assert(self);
    assert(self->bam);
    assert(sequence);
    return BAMAlignmentGetSequence(self->bam, sequence);
}

static rc_t BamSequenceGetRead2 ( const BamSequence *self, char *sequence, uint32_t start, uint32_t stop )
{
    assert(self);
    assert(self->bam);
    assert(sequence);
    return BAMAlignmentGetSequence2(self->bam, sequence, start, stop);
}

static rc_t BamSequenceGetQuality ( const BamSequence *self, const int8_t **quality, uint8_t *offset, int *qualType )
{
    assert(self);
    assert(self->bam);
    assert(quality);
    assert(offset);
    assert(qualType);
    {
        rc_t rc = BAMAlignmentGetQuality2(self->bam, (const uint8_t **)quality, offset);
        if (rc != 0)
        {
            *quality = NULL;
            *offset = 0;
            *qualType = 0;
        }
        else
            *qualType = QT_Phred;
        return rc;
    }
}

static rc_t BamSequenceGetSpotGroup ( const BamSequence *self, const char **name, size_t *length )
{
    assert(self);
    assert(self->bam);
    assert(name);
    {
        rc_t rc = BAMAlignmentGetReadGroupName ( self->bam, name );
        if (rc != 0)
        {
            *name = NULL;
            *length = 0;
        }
        else
            *length = string_size(*name);
        return rc;
    }
}

static rc_t BamSequenceGetSpotName ( const BamSequence *self, const char **name, size_t *length )
{
    assert(self);
    assert(self->bam);
    assert(name);
    {
        rc_t rc = BAMAlignmentGetReadName ( self->bam, name );
        if (rc != 0)
        {
            *name = NULL;
            *length = 0;
        }
        else
            *length = string_size(*name);
        return rc;
    }
}

static bool BamSequenceIsColorSpace ( const BamSequence *self )
{
    assert(self);
    assert(self->bam);
    return BAMAlignmentHasColorSpace(self->bam);
}

static rc_t BamSequenceGetCSKey ( const BamSequence *self, char cskey[1] )
{
    assert(self);
    assert(self->bam);
    return BAMAlignmentGetCSKey(self->bam, cskey);
}

static rc_t BamSequenceGetCSReadLength     ( const BamSequence *self, uint32_t *length )
{
    assert(self);
    assert(self->bam);
    assert(length);
    return BAMAlignmentGetCSSeqLen(self->bam, length);
}

static rc_t BamSequenceGetCSRead ( const BamSequence *self, char *sequence )
{
    assert(self);
    assert(self->bam);
    assert(sequence);
    {
        uint32_t length;
        rc_t rc = BAMAlignmentGetCSSeqLen(self->bam, &length);
        return rc ? rc : BAMAlignmentGetCSSequence(self->bam, sequence, length);
    }
}

static rc_t BamSequenceGetCSQuality ( const BamSequence *self, const int8_t **quality, uint8_t *offset, int *qualType )
{
    assert(self);
    assert(self->bam);
    assert(quality);
    assert(offset);
    assert(qualType);
    {
        rc_t rc = BAMAlignmentGetCSQuality(self->bam, (const uint8_t **)quality, offset);
        if (rc != 0)
        {
            *quality = NULL;
            *offset = 0;
            *qualType = 0;
        }
        else
            *qualType = QT_Phred;
        return rc;
    }
}

static bool BamSequenceRecordWasPaired ( const BamSequence *self )
{
    return self->flags & BAMFlags_WasPaired;
}

static int BamSequenceRecordOrientationSelf ( const BamSequence *self )
{
    return (self->flags & BAMFlags_SelfIsReverse) ? ReadOrientationReverse : ReadOrientationForward;
}

static int BamSequenceRecordOrientationMate ( const BamSequence *self )
{
    return (self->flags & BAMFlags_MateIsReverse) ? ReadOrientationReverse : ReadOrientationForward;;
}

static bool BamSequenceRecordIsFirst ( const BamSequence *self )
{
    return self->flags & BAMFlags_IsFirst;
}

static bool BamSequenceRecordIsSecond ( const BamSequence *self )
{
    return self->flags & BAMFlags_IsSecond;
}

static bool BamSequenceRecordIsDuplicate ( const BamSequence *self )
{
    return self->flags & BAMFlags_IsDuplicate;
}

static bool BamSequenceIsLowQuality ( const BamSequence *self )
{
    return self->flags & BAMFlags_IsLowQuality;
}

static rc_t BamSequenceGetTI ( const BamSequence *self, uint64_t *ti )
{
    return BAMAlignmentGetTI(self->bam, ti);
}

 /*--------------------------------------------------------------------------
 * BamReferenceInfo forwards
 */
static rc_t BamReferenceAddRef                  ( const BamReferenceInfo* self );
static rc_t BamReferenceRelease                 ( const BamReferenceInfo* self );
static rc_t BamReferenceInfoGetRefSeqCount      ( const BamReferenceInfo* self, uint32_t* count );
static rc_t BamReferenceInfoGetRefSeq           ( const BamReferenceInfo* self, uint32_t n, ReferenceSequence* result );
static rc_t BamReferenceInfoGetReadGroupCount   ( const BamReferenceInfo* self, uint32_t* count );
static rc_t BamReferenceInfoGetReadGroup        ( const BamReferenceInfo* self, unsigned n, ReadGroup* result );
static rc_t BamReferenceInfoGetReadGroupByName  ( const BamReferenceInfo* self, const char* name, ReadGroup* result );

static ReferenceInfo_vt_v1 BamReferenceInfo_vt = 
{
    1, 0, 
    /* start minor version == 0 */
    BamReferenceAddRef,
    BamReferenceRelease,
    BamReferenceInfoGetRefSeqCount,
    BamReferenceInfoGetRefSeq,
    BamReferenceInfoGetReadGroupCount,
    BamReferenceInfoGetReadGroup,
    BamReferenceInfoGetReadGroupByName,
    /* end minor version == 0 */
};

/*--------------------------------------------------------------------------
 * BamReaderFile
 */
static rc_t BamReaderFileWhack                      ( BamReaderFile* self );
static rc_t BamReaderFileGetRecord                  ( const BamReaderFile *self, const Record** result );
static float BamReaderFileGetProportionalPosition   ( const BamReaderFile *self );
static rc_t BamReaderFileGetReferenceInfo           ( const BamReaderFile *self, const ReferenceInfo** result );

static ReaderFile_vt_v1 BamReaderFile_vt = 
{
    1, 0, 
    /* start minor version == 0 */
    BamReaderFileWhack,
    BamReaderFileGetRecord,
    BamReaderFileGetProportionalPosition,
    BamReaderFileGetReferenceInfo,
    /* end minor version == 0 */
};

struct BamReaderFile
{
    ReaderFile dad;
    ReferenceInfo refInfo;
    
    const BAMFile* reader;
};

const BAMFile* ToBam(const ReaderFile* reader)
{
   assert(reader);
   return ((const BamReaderFile*)reader)->reader;
}

float BamReaderFileGetProportionalPosition ( const BamReaderFile *f )
{
    BamReaderFile* self = (BamReaderFile*) f;
    return BAMFileGetProportionalPosition(self->reader);
} 

rc_t BamReaderFileGetReferenceInfo ( const BamReaderFile *self, const ReferenceInfo** result )
{
    *result = & self->refInfo;
    return ReaderFileAddRef( & self->dad );
}

rc_t BamReaderFileWhack( BamReaderFile* f )
{
    BamReaderFile* self = (BamReaderFile*) f;
    
    BAMFileRelease(self->reader);
    
    free ( (void*)self->dad.pathname );
    free ( self );
    
    return 0;   
}

rc_t BamReaderFileGetRecord ( const BamReaderFile *f, const Record** result )
{
    rc_t rc;
    BamReaderFile* self = (BamReaderFile*) f;

    BamRecord* record = (BamRecord*)malloc(sizeof(BamRecord));
    if (record == NULL)
    {
        rc = RC ( RC_MODULE, rcData, rcAllocating, rcMemory, rcExhausted );
        return 0;
    }
    rc = BamRecordInit(record);
    if (rc != 0)
    {
        free(record);
        return rc;
    }
    
    rc = BAMFileRead2(self->reader, &record->seq.bam);
    if (rc)
    {
        if (GetRCModule(rc) == rcAlign && GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
        {   /* end of input */
            rc = 0; 
        }
        *result = 0;
        BamRecordRelease(record);
    }    
    else
    {
        *result = (const Record*)record;
        BAMAlignmentGetFlags(record->seq.bam, &record->seq.flags);
    }
        
    return rc;
}

rc_t CC BamReaderFileMake( const ReaderFile **reader, char const headerText[], char const path[])
{
    rc_t rc;
    BamReaderFile* self = (BamReaderFile*) malloc ( sizeof * self );
    if ( self == NULL )
    {
        rc = RC ( RC_MODULE, rcFileFormat, rcAllocating, rcMemory, rcExhausted );
        *reader = 0;
    }
    else
    {
        rc = ReaderFileInit ( self );
        self->dad.vt.v1 = & BamReaderFile_vt;
        self->refInfo.vt.v1 = & BamReferenceInfo_vt;

        self->dad.pathname = string_dup(path, strlen(path)+1);
        if ( self->dad.pathname == NULL )
        {
            rc = RC ( RC_MODULE, rcFileFormat, rcAllocating, rcMemory, rcExhausted );
        }
        
        rc = BAMFileMakeWithHeader ( &self->reader, headerText, "%s", self->dad.pathname );
        
        if (rc != 0)
        {
            BamReaderFileWhack( self );
            *reader = 0;
        }
        else
        {
            *reader = (const ReaderFile *) self;
        }
    }
 
    return rc;
}

/*--------------------------------------------------------------------------
 * BamReferenceInfo
 */
static const BamReaderFile*
BamRefInfoToReader(const BamReferenceInfo* ref) 
{
    return ( const BamReaderFile * ) ( (uint8_t*)ref - offsetof ( BamReaderFile, refInfo ) );
}

rc_t BamReferenceAddRef ( const BamReferenceInfo* self )
{
    return ReaderFileAddRef( & BamRefInfoToReader(self)->dad );
}

rc_t BamReferenceRelease ( const BamReferenceInfo* self )
{
    return ReaderFileRelease ( & BamRefInfoToReader(self)->dad );
}

rc_t BamReferenceInfoGetRefSeqCount ( const BamReferenceInfo* self, uint32_t* count )
{
    return BAMFileGetRefSeqCount( BamRefInfoToReader(self)->reader, count );
}

rc_t BamReferenceInfoGetRefSeq ( const BamReferenceInfo *self, uint32_t n, ReferenceSequence *result )
{
    const BAMRefSeq *refSeq;
    rc_t rc = BAMFileGetRefSeq( BamRefInfoToReader(self)->reader, n, &refSeq );
    if (rc != 0 || refSeq == NULL)
    {
        return RC ( RC_MODULE, rcHeader, rcAccessing, rcParam, rcOutofrange );
    }
    
    result->length = refSeq->length;
    result->name = refSeq->name;
    result->checksum = refSeq->checksum;
    
    return 0;
}

rc_t BamReferenceInfoGetReadGroupCount ( const BamReferenceInfo *self, uint32_t *count )
{
    return BAMFileGetReadGroupCount ( BamRefInfoToReader(self)->reader, count );
}

rc_t BamReferenceInfoGetReadGroup ( const BamReferenceInfo *self, unsigned n, ReadGroup* result )
{
    const BAMReadGroup *rg;
    rc_t rc = BAMFileGetReadGroup( BamRefInfoToReader(self)->reader, n, &rg );
    if (rc != 0 || rg == NULL)
    {
        return RC ( RC_MODULE, rcHeader, rcAccessing, rcParam, rcOutofrange );
    }
        
    result->name = rg->name;
    result->platform = rg->platform;
    
    return 0;
}

rc_t BamReferenceInfoGetReadGroupByName ( const BamReferenceInfo *self, const char *name, ReadGroup *result )
{
    const BAMReadGroup *rg;
    rc_t rc = BAMFileGetReadGroupByName( BamRefInfoToReader(self)->reader, name, &rg );
    if (rc != 0 || rg == NULL)
    {
        return RC ( RC_MODULE, rcHeader, rcAccessing, rcParam, rcOutofrange );
    }
    
    result->name = rg->name;
    result->platform = rg->platform;
    
    return 0;
}





 
#ifdef TENTATIVE 
/**************************** future parsing-on-a-thread code *****************************************/

#include "bam-reader.h"

#include <atomic32.h>
#include <stdlib.h>
#include <string.h>

#include <klib/rc.h>
#include <kproc/lock.h>
#include <kproc/cond.h>
#include <kproc/thread.h>

#define BUFFER_COUNT (3)

struct BAMReader
{
    atomic32_t refcount;
    const BAMFile* file;
    
    KLock *lock;
    KCondition *have_data;
    KCondition *need_data;
    KThread *th;

    const BAMAlignment* que[BUFFER_COUNT];
    unsigned volatile nque;
    rc_t volatile rc;
    
    bool eof;
};

static rc_t BAMReaderThreadMain(KThread const *const th, void *const vp);

#define END_OF_DATA RC(rcAlign, rcFile, rcReading, rcRow, rcNotFound)

rc_t BAMReaderMake( const BAMReader **result,
                    char const headerText[],
                    char const path[] )
{
    rc_t rc;
    BAMReader *self = malloc(sizeof(BAMReader));
    if ( self == NULL )
    {
        *result = NULL;
        return RC(rcAlign, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    else
    {
        atomic32_set( & self->refcount, 1 );
        rc = BAMFileMakeWithHeader( & self->file, headerText, "%s", path);
        if ( rc != 0 )
        {
            free(self);
            *result = 0;
        }
        else
            *result = self;
    }
    
    self->nque = 0;
    self->rc = 0;
    self->eof = false;
    
    rc = KLockMake(&self->lock);
    if (rc == 0) 
    {
        rc = KConditionMake(&self->have_data);
        if (rc == 0) 
        {
            rc = KConditionMake(&self->need_data);
            if (rc == 0) 
            {
                rc = KThreadMake(&self->th, BAMReaderThreadMain, self);
                if (rc == 0) 
                    return 0;
                KConditionRelease(self->need_data);
            }
            KConditionRelease(self->have_data);
        }
        KLockRelease(self->lock);
    }
    
    return rc;
}           

static void BAMReaderWhack(BAMReader *const self)
{
    KThreadCancel(self->th);
    KThreadWait(self->th, NULL);
    BAMFileRelease(self->file);
    KConditionRelease(self->need_data);
    KConditionRelease(self->have_data);
    KLockRelease(self->lock);
    KThreadRelease(self->th);
}
        

/* AddRef
 * Release
 */
rc_t BAMReaderAddRef ( const BAMReader *self )
{
    if (self != NULL)
        atomic32_inc(&((BAMReader *)self)->refcount);
    return 0;
}

rc_t BAMReaderRelease ( const BAMReader *cself )
{
    BAMReader *self = (BAMReader *)cself;
    
    if (cself != NULL) 
    {
        if (atomic32_dec_and_test(&self->refcount)) 
        {
            BAMReaderWhack(self);
            free(self);
        }
    }
    return 0;
}

/* GetBAMFile
 */
const BAMFile* BAMReaderGetBAMFile ( const BAMReader *self )
{
    if (self == NULL)
        return NULL;
    return self->file;
}

static rc_t BAMReaderThreadMain(KThread const *const th, void *const vp)
{
    BAMReader *const self = (BAMReader *)vp;
    rc_t rc = 0;
    const BAMAlignment* rec;
    
    KLockAcquire(self->lock);
    do 
    {
        while (self->nque == BUFFER_COUNT)
            KConditionWait(self->need_data, self->lock);

        {
            rc = BAMFileRead( self->file, &rec);
            if (rc == END_OF_DATA)
            {
                rec = NULL;
                rc = 0;
            }
            else if (rc)
                break;

            self->que[self->nque] = rec;
            ++self->nque;
            KConditionSignal(self->have_data);
        }
    }
    while (rec);
    self->rc = rc;
    KLockUnlock(self->lock);
    return 0;
}

/* Read
 *  read an aligment
 *
 *  "result" [ OUT ] - return param for BAMAlignment object
 *   must be released with BAMAlignmentRelease
 *
 *  returns RC(..., ..., ..., rcRow, rcNotFound) at end
 */
rc_t BAMReaderRead ( const BAMReader *cself, const BAMAlignment **result )
{
    rc_t rc;
    BAMReader *self = (BAMReader *)cself;
    
    if (self == NULL)
        return RC(rcAlign, rcFile, rcReading, rcParam, rcNull);
    if (self->eof)
        return RC(rcAlign, rcFile, rcReading, rcData, rcInsufficient);
        
    KLockAcquire(self->lock);
    if ((rc = self->rc) == 0) 
    {
        while (self->nque == 0 && (rc = self->rc) == 0)
            KConditionWait(self->have_data, self->lock);
        if (rc == 0) 
        {
            *result = self->que[0];
            
            if (*result) 
            {
                --self->nque;
                memmove(&self->que[0], &self->que[1], self->nque * sizeof(self->que[0]));
                KConditionSignal(self->need_data);
            }
            else 
            {
                self->eof = true;
                rc = END_OF_DATA;
            }
        }
    }
    KLockUnlock(self->lock);

    return rc;
}

#endif


