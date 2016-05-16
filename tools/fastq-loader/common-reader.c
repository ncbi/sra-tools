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

#include <loader/common-reader-priv.h>

#include <sysalloc.h>

#include <assert.h>
#include <stdlib.h>

#include <klib/rc.h>

/*--------------------------------------------------------------------------
 ReaderFile
 */
rc_t CC ReaderFileAddRef ( const ReaderFile *self )
{
    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "ReaderFile" ) )
        {
        case krefLimit:
            return RC ( RC_MODULE, rcFile, rcAttaching, rcRange, rcExcessive );
        }
    }
    return 0;
}

rc_t CC ReaderFileRelease ( const ReaderFile *self )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "ReaderFile" ) )
        {
        case krefWhack:
            return self->vt.v1->destroy( (ReaderFile *)self );
        case krefNegative:
            return RC ( RC_MODULE, rcFile, rcReleasing, rcRange, rcExcessive );
        }
    }
    return 0;
}

rc_t CC ReaderFileGetRecord( const ReaderFile *self, const Record** result)
{
    assert(self);
    assert(result);
    return self->vt.v1->getRecord( self, result );
}

float CC ReaderFileGetProportionalPosition ( const READERFILE_IMPL *self )
{
    assert(self);
    assert(self->vt.v1->getProportionalPosition);
    return self->vt.v1->getProportionalPosition( self );
}

rc_t CC ReaderFileGetReferenceInfo ( const ReaderFile *self, const ReferenceInfo** result )
{
    assert(self);
    assert(self->vt.v1->getReferenceInfo);
    return self->vt.v1->getReferenceInfo( self, result );
}

/* Init
 *  polymorphic parent constructor
 */
rc_t CC ReaderFileInit ( ReaderFile *self )
{
    if ( self == NULL )
        return RC ( RC_MODULE, rcFileFormat, rcConstructing, rcSelf, rcNull );

    KRefcountInit ( & self -> refcount, 1, "ReaderFile", "ReaderFileInit", "");

    self->pathname = NULL;

    return 0;
}

rc_t CC ReaderFileWhack ( ReaderFile *self )
{
    if ( self == NULL )
        return RC ( RC_MODULE, rcFileFormat, rcConstructing, rcSelf, rcNull );

    free( self->pathname );

    return 0;
}

const char* CC ReaderFileGetPathname ( const ReaderFile *self )
{
    if ( self == NULL )
        return NULL;
    return self->pathname;
}

/*--------------------------------------------------------------------------
 Record
 */

rc_t CC RecordAddRef ( const Record *self )
{
    assert(self);
    assert(self->vt.v1->addRef);
    return self->vt.v1->addRef(self);
}

rc_t CC RecordRelease ( const Record *self )
{
    assert(self);
    assert(self->vt.v1->release);
    return self->vt.v1->release(self);
}

rc_t CC RecordGetSequence ( const Record *self, const Sequence** result)
{
    assert(self);
    assert(result);
    assert(self->vt.v1->getSequence);
    return self->vt.v1->getSequence(self, result);
}

rc_t CC RecordGetAlignment( const Record *self, const Alignment** result)
{
    assert(self);
    assert(result);
    assert(self->vt.v1->getAlignment);
    return self->vt.v1->getAlignment(self, result);
}

rc_t CC RecordGetRejected ( const Record *self, const Rejected** result)
{
    assert(self);
    assert(result);
    assert(self->vt.v1->getRejected);
    return self->vt.v1->getRejected(self, result);
}

/*--------------------------------------------------------------------------
 Sequence
 */
rc_t CC SequenceAddRef ( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->addRef);
    return self->vt.v1->addRef(self);
}

rc_t CC SequenceRelease ( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->release);
    return self->vt.v1->release(self);
}

rc_t CC SequenceGetReadLength ( const Sequence *self, uint32_t *length )
{
    assert(self);
    assert(length);
    assert(self->vt.v1->getReadLength);
    return self->vt.v1->getReadLength(self, length);
}

rc_t CC SequenceGetRead( const Sequence *self, char *sequence )
{
    assert(self);
    assert(sequence);
    assert(self->vt.v1->getRead);
    return self->vt.v1->getRead(self, sequence);
}

rc_t CC SequenceGetRead2 ( const Sequence *self, char *sequence, uint32_t start, uint32_t stop)
{
    assert(self);
    assert(sequence);
    assert(self->vt.v1->getRead2);
    return self->vt.v1->getRead2(self, sequence, start, stop);
}

rc_t CC SequenceGetQuality(const Sequence *self, const int8_t **quality, uint8_t *offset, int *qualType)
{
    assert(self);
    assert(quality);
    assert(offset);
    assert(qualType);
    assert(self->vt.v1->getQuality);
    return self->vt.v1->getQuality(self, quality, offset, qualType);
}

rc_t CC SequenceGetSpotGroup ( const Sequence *self, const char **name, size_t *length )
{
    assert(self);
    assert(name);
    assert(length);
    assert(self->vt.v1->getSpotGroup);
    return self->vt.v1->getSpotGroup(self, name, length);
}

rc_t CC SequenceGetSpotName ( const Sequence *self, const char **name, size_t *length )
{
    assert(self);
    assert(name);
    assert(length);
    assert(self->vt.v1->getSpotName);
    return self->vt.v1->getSpotName(self, name, length);
}

bool CC SequenceIsColorSpace ( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->isColorSpace);
    return self->vt.v1->isColorSpace(self);
}

rc_t CC SequenceGetCSKey ( const Sequence *self, char cskey[1] )
{
    assert(self);
    assert(self->vt.v1->getCSKey);
    return self->vt.v1->getCSKey(self, cskey);
}

rc_t CC SequenceGetCSReadLength ( const Sequence *self, uint32_t *length )
{
    assert(self);
    assert(self->vt.v1->getCSReadLength);
    return self->vt.v1->getCSReadLength(self, length);
}

rc_t CC SequenceGetCSRead( const Sequence *self, char *sequence )
{
    assert(self);
    assert(self->vt.v1->getCSRead);
    return self->vt.v1->getCSRead(self, sequence);
}

rc_t CC SequenceGetCSQuality(const Sequence *self, const int8_t **quality, uint8_t *offset, int *qualType)
{
    assert(self);
    assert(quality);
    assert(offset);
    assert(qualType);
    assert(self->vt.v1->getCSQuality);
    return self->vt.v1->getCSQuality(self, quality, offset, qualType);
}

bool CC SequenceWasPaired     ( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->wasPaired);
    return self->vt.v1->wasPaired(self);
}

int CC SequenceGetOrientationSelf( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->orientationSelf);
    return self->vt.v1->orientationSelf(self);
}
 
int CC SequenceGetOrientationMate( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->orientationMate);
    return self->vt.v1->orientationMate(self);
}

bool CC SequenceIsFirst       ( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->isFirst);
    return self->vt.v1->isFirst(self);
}

bool CC SequenceIsSecond      ( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->isSecond);
    return self->vt.v1->isSecond(self);
}

rc_t SequenceGetTI(Sequence const *self, uint64_t *ti)
{
    assert(self);
    assert(ti);
    assert(self->vt.v1->getTI);
    return self->vt.v1->getTI(self, ti);
}

bool CC SequenceIsDuplicate( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->isDuplicate);
    return self->vt.v1->isDuplicate(self);
}

bool CC SequenceIsLowQuality( const Sequence *self )
{
    assert(self);
    assert(self->vt.v1->isLowQuality);
    return self->vt.v1->isLowQuality(self);
}

/*--------------------------------------------------------------------------
 Alignment
 */

rc_t CC AlignmentAddRef ( const Alignment *self )
{
    assert(self);
    assert(self->vt.v1->addRef);
    return self->vt.v1->addRef(self);
}

rc_t CC AlignmentRelease ( const Alignment *self )
{
    assert(self);
    assert(self->vt.v1->release);
    return self->vt.v1->release(self);
}

rc_t CC AlignmentGetRefSeqId ( const Alignment *self, int32_t *refSeqId )
{
    assert(self);
    assert(refSeqId);
    assert(self->vt.v1->getRefSeqId);
    return self->vt.v1->getRefSeqId(self, refSeqId);
}

rc_t CC AlignmentGetMateRefSeqId ( const Alignment *self, int32_t *refSeqId )
{
    assert(self);
    assert(refSeqId);
    assert(self->vt.v1->getMateRefSeqId);
    return self->vt.v1->getMateRefSeqId(self, refSeqId);
}

rc_t CC AlignmentGetPosition ( const Alignment *self, int64_t *pos )
{
    assert(self);
    assert(pos);
    assert(self->vt.v1->getPosition);
    return self->vt.v1->getPosition(self, pos);
}

rc_t CC AlignmentGetMatePosition ( const Alignment *self, int64_t *pos )
{
    assert(self);
    assert(pos);
    assert(self->vt.v1->getMatePosition);
    return self->vt.v1->getMatePosition(self, pos);
}

rc_t CC AlignmentGetMapQuality ( const Alignment *self, uint8_t *qual )
{
    assert(self);
    assert(qual);
    assert(self->vt.v1->getMapQuality);
    return self->vt.v1->getMapQuality(self, qual);
}

rc_t CC AlignmentGetAlignmentDetail ( const Alignment *self,
                                      AlignmentDetail *rslt, 
                                      uint32_t count, 
                                      uint32_t *actual,
                                      int32_t *firstMatch, 
                                      int32_t *lastMatch )
{
    assert(self);
    assert(self->vt.v1->getAlignmentDetail);
    return self->vt.v1->getAlignmentDetail(self, rslt, count, actual, firstMatch, lastMatch);
}

rc_t CC AlignmentGetAlignOpCount ( const Alignment *self, uint32_t *n )
{
    assert(self);
    assert(n);
    assert(self->vt.v1->getAlignOpCount);
    return self->vt.v1->getAlignOpCount(self, n);
}

rc_t CC AlignmentGetInsertSize ( const Alignment *self, int64_t *size )
{
    assert(self);
    assert(size);
    assert(self->vt.v1->getInsertSize);
    return self->vt.v1->getInsertSize(self, size);
}

rc_t AlignmentGetCGData ( const ALIGNMENT_IMPL *self, const CGData** result)
{
    assert(self);
    assert(result);
    assert(self->vt.v1->getCG);
    return self->vt.v1->getCG(self, result);
}

rc_t AlignmentGetBAMCigar ( const ALIGNMENT_IMPL *self, uint32_t const **rslt, uint32_t *length )
{
    assert(self);
    assert(rslt);
    assert(length);
    assert(self->vt.v1->getBAMCigar);
    return self->vt.v1->getBAMCigar(self, rslt, length);
}

bool AlignmentIsSecondary( const Alignment *self )
{
    assert(self);
    assert(self->vt.v1->isSecondary);
    return self->vt.v1->isSecondary(self);
}

/*--------------------------------------------------------------------------
 * CGData
 */
rc_t CC CGDataAddRef ( const CGData *self )
{
    assert(self);
    assert(self->vt.v1->addRef);
    return self->vt.v1->addRef(self);
}

rc_t CC CGDataRelease ( const CGData *self )
{
    assert(self);
    assert(self->vt.v1->release);
    return self->vt.v1->release(self);
}

/* CGGetSeqQual
 */
rc_t CC CGDataGetSeqQual ( const CGData* self,
                           char sequence[/* 35 */],
                           uint8_t quality[/* 35 */] )
{
    assert(self);
    assert(sequence);
    assert(quality);
    assert(self->vt.v1->getSeqQual);
    return self->vt.v1->getSeqQual(self, sequence, quality); 
}                       

/* CGGetCigar
 */
rc_t CC CGDataGetCigar ( const CGData* self,
                         uint32_t *cigar,
                         uint32_t cig_max,
                         uint32_t *cig_act )
{
    assert(self);
    assert(cigar);
    assert(cig_act);
    assert(self->vt.v1->getCigar);
    return self->vt.v1->getCigar(self, cigar, cig_max, cig_act);
}                     

/* CGGetAlignGroup
 */
rc_t CC CGDataGetAlignGroup ( const CGData* self,
                              char buffer[],
                              size_t max_size,
                              size_t *act_size )
{
    assert(self);
    assert(act_size);
    assert(self->vt.v1->getAlignGroup);
    return self->vt.v1->getAlignGroup(self, buffer, max_size, act_size);
}
                          
/*--------------------------------------------------------------------------
 * Rejected
 */

rc_t CC RejectedInit ( Rejected *self )
{
    KRefcountInit ( & self -> refcount, 1, "Rejected", "RejectedInit", "");
    StringInit(&self->source, 0, 0, 0);
    self->message = 0;
    self->column = 0;
    self->line = 0;
    self->fatal = false;
    return 0;
}

static rc_t CC RejectedWhack ( const Rejected *self )
{
    free( (void*)self->source.addr );
    free( (void*)self->message );
    free( (void*)self );
    return 0;
}

rc_t CC RejectedAddRef ( const Rejected *self )
{
    assert(self);
    KRefcountAdd( & self->refcount, "Rejected" );
    /* TODO: handle rc from KRefcountAdd */
    return 0;
}

rc_t CC RejectedRelease ( const Rejected *self )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "Rejected" ) )
        {
        case krefWhack:
            RejectedWhack( self );
            break;
        default:
            /* TODO: handle other values */
            break;
        }
    }
    return 0;
}

rc_t CC RejectedGetError( const Rejected* self, const char** text, uint64_t* line, uint64_t* column, bool* fatal )
{
    assert(self);
    assert(text);
    assert(line);
    assert(column);
    *text = self->message;
    *line = self->line;
    *column = self->column;
    *fatal = self->fatal;
    return 0;
}

rc_t CC RejectedGetData( const Rejected* self, const void** data, size_t* length )
{
    assert(self);
    assert(data);
    assert(length);
    *data = self->source.addr;
    *length = StringSize( & self->source );
    return 0;
}

/*--------------------------------------------------------------------------
 * ReferenceInfo
 */
rc_t CC ReferenceInfoAddRef ( const ReferenceInfo *self )
{
    assert(self);
    assert(self->vt.v1->addRef);
    return self->vt.v1->addRef(self);
}

rc_t CC ReferenceInfoRelease ( const ReferenceInfo *self )
{
    assert(self);
    assert(self->vt.v1->release);
    return self->vt.v1->release(self);
}

rc_t CC ReferenceInfoGetRefSeqCount ( const ReferenceInfo *self, uint32_t* count )
{
    assert(self);
    assert(self->vt.v1->getRefSeqCount);
    return self->vt.v1->getRefSeqCount(self, count);
}

rc_t CC ReferenceInfoGetRefSeq ( const ReferenceInfo *self, uint32_t n, ReferenceSequence *result )
{
    assert(self);
    assert(self->vt.v1->getRefSeq);
    return self->vt.v1->getRefSeq(self, n, result);
}

rc_t CC ReferenceInfoGetReadGroupCount ( const ReferenceInfo *self, uint32_t *count )
{
    assert(self);
    assert(self->vt.v1->getReadGroupCount);
    return self->vt.v1->getReadGroupCount(self, count);
}

rc_t CC ReferenceInfoGetReadGroup ( const ReferenceInfo *self, unsigned n, ReadGroup *result )
{
    assert(self);
    assert(self->vt.v1->getReadGroup);
    return self->vt.v1->getReadGroup(self, n, result);
}

rc_t CC ReferenceInfoGetReadGroupByName ( const ReferenceInfo *self, const char *name, ReadGroup *result )
{
    assert(self);
    assert(self->vt.v1->getReadGroupByName );
    return self->vt.v1->getReadGroupByName(self, name, result);
}
