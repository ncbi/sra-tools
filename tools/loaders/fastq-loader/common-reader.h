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

#ifndef _h_common_reader_
#define _h_common_reader_

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
typedef struct ReaderFile           ReaderFile;
typedef struct Record               Record;
typedef struct Sequence             Sequence;
typedef struct Alignment            Alignment;
typedef struct CGData               CGData;  
typedef struct Rejected             Rejected;
typedef struct ReferenceInfo        ReferenceInfo;

/*--------------------------------------------------------------------------
 ReaderFile
 */
rc_t CC ReaderFileAddRef ( const ReaderFile *self );
rc_t CC ReaderFileRelease ( const ReaderFile *self );

/* GetRecord
 * Parses the next record from the source. At the end of the file, rc == 0, *result == 0.
 */
rc_t CC ReaderFileGetRecord( const ReaderFile *self, const Record** result);

/* GetPathname
 * Returns input's pathname, if applicable.
 */
const char* CC ReaderFileGetPathname ( const ReaderFile *self );

/* GetProportionalPosition
 *  get the aproximate proportional position in the input file
 *  this is intended to be useful for computing progress
 *
 * NB - does not return rc_t
 */
float CC ReaderFileGetProportionalPosition ( const ReaderFile *self );

/* GetReferenceInfo
 *
 */
rc_t CC ReaderFileGetReferenceInfo ( const ReaderFile *self, const ReferenceInfo** result );

/*--------------------------------------------------------------------------
 Record
 */

/* AddRef
 * Release
 */
rc_t CC RecordAddRef ( const Record *self );
rc_t CC RecordRelease ( const Record *self );

rc_t CC RecordGetRejected ( const Record *self, const Rejected** result);
rc_t CC RecordGetSequence ( const Record *self, const Sequence** result);
rc_t CC RecordGetAlignment( const Record *self, const Alignment** result);

/*--------------------------------------------------------------------------
 Sequence
 */

/* AddRef
 * Release
 */
rc_t CC SequenceAddRef ( const Sequence *self );
rc_t CC SequenceRelease ( const Sequence *self );

/* GetReadLength
 *  get the sequence length
 *  i.e. the number of elements of both sequence and quality
 *
 *  "length" [ OUT ] - length in bases of query sequence and quality
 */
rc_t CC SequenceGetReadLength ( const Sequence *self, uint32_t *length );

/* GetRead
 *  get the sequence data [0..ReadLength)
 *  caller provides buffer of ReadLength bytes
 *
 *  "sequence" [ OUT ] - pointer to a buffer of at least ReadLength bytes
 */
rc_t CC SequenceGetRead( const Sequence *self, char *sequence );

/* GetRead2
 *  get the sequence data [0..ReadLength)
 *  caller provides buffer of ReadLength bytes
 *
 *  "sequence" [ OUT ] - pointer to a buffer of at least ReadLength bytes
 *
 *  "start" [ IN ] and "stop" [ IN ] - zero-based coordinates, half-closed interval; both have to be within ReadLength
 */
rc_t CC SequenceGetRead2 ( const Sequence *self, char *sequence, uint32_t start, uint32_t stop);

enum QualityType {
    QT_Unknown = 0,
    QT_Phred,
    QT_LogOdds
};
/* GetQuality
 *  get the raw quality data [0..ReadLength) from OQ if possible else from QUAL
 *  values are unsigned with 0xFF == missing
 *
 *  "quality" [ OUT ] - return param for quality sequence
 *   held internally, validity is guaranteed for the life of the sequence
 *  
 *  "offset" [ OUT ] - the zero point of quality (33, 64; 0 for binary)
 *  
 *  "qualType" [ OUT ] - quality type (phred, log-odds, unknown)
 */
rc_t CC SequenceGetQuality(const Sequence *self, const int8_t **quality, uint8_t *offset, int *qualType);

/* SequenceGetSpotGroup
 *  get the name of the spot group (e.g. accession)
 *
 *  "name" [ OUT ] - return param for group name
 *   held internally, validity is guaranteed for the life of the sequence
 *
 *  "length" [ OUT ] - return the number of bytes in "name"
 */
rc_t CC SequenceGetSpotGroup ( const Sequence *self, const char **name, size_t *length );


/* SequenceGetSpotName
 *  get the read name and length in bytes
 *
 *  "name" [ OUT ] - return param for read group name
 *   held internally, validity is guaranteed for the life of the sequence
 *
 *  "length" [ OUT ] - return the number of bytes in "name"
 */
rc_t CC SequenceGetSpotName ( const Sequence *self, const char **name, size_t *length );

/* IsColorSpace
 *  Does the sequence have colorspace info
 */
bool CC SequenceIsColorSpace ( const Sequence *self );

/* GetCSKey
 *  get the colorspace key
 *
 *  "cskey" [ OUT ] - return param 
 *
 *  return: if no colorspace info, RC is 0 but the value of cskey is undefined
 */
rc_t CC SequenceGetCSKey ( const Sequence *self, char cskey[1] );

/* GetCSReadLength
 *  get the color space sequence length
 *  i.e. the number of elements of both sequence and quality
 *
 *  "length" [ OUT ] - length in bases of query sequence and quality
 */
rc_t CC SequenceGetCSReadLength ( const Sequence *self, uint32_t *length );

/* GetCSRead
 *  get the color space sequence data [0..ReadLength)
 *  caller provides buffer of ReadLength bytes
 *
 *  "sequence" [ OUT ] - pointer to a buffer of at least ReadLength bytes
 */
rc_t CC SequenceGetCSRead( const Sequence *self, char *sequence );

/* GetCSQuality
 *  get the color spaqce sequence's raw quality data [0..ReadLength) from OQ if possible else from QUAL
 *  values are unsigned with 0xFF == missing
 *
 *  "quality" [ OUT ] - return param for quality sequence
 *   held internally, validity is guaranteed for the life of the sequence
 *  
 *  "offset" [ OUT ] - the zero point of quality (33, 64; 0 for binary)
 *  
 *  "qualType" [ OUT ] - quality type (phred, log-odds, unknown)
 */
rc_t CC SequenceGetCSQuality(const Sequence *self, const int8_t **quality, uint8_t *offset, int *qualType);


/* WasPaired
 * true if read number is present and not 0 
 */ 
bool CC SequenceWasPaired     ( const Sequence *self ); 

enum ReadOrientation {
    ReadOrientationUnknown,
    ReadOrientationForward,
    ReadOrientationReverse
};
/* SequenceGetOrientationSelf
 */ 
int CC SequenceGetOrientationSelf( const Sequence *self ); 
/* SequenceGetOrientationMate
 */ 
int CC SequenceGetOrientationMate( const Sequence *self ); 

/* IsFirst
 * fastq: read number is present and equal to 1
 */ 
bool CC SequenceIsFirst       ( const Sequence *self ); 
/* IsSecond
 * fastq: read number is present and equal to 2
 */
bool CC SequenceIsSecond      ( const Sequence *self ); 
/* IsDuplicate
 * 
 */
bool CC SequenceIsDuplicate( const Sequence *self ); 
/* IsLowQuality
 * 
 */
bool CC SequenceIsLowQuality( const Sequence *self ); 

/*  RecordGetTI
 *
 */
rc_t SequenceGetTI(Sequence const *self, uint64_t *ti);

/*--------------------------------------------------------------------------
 Alignment
 */

/* AddRef
 * Release
 */
rc_t CC AlignmentAddRef ( const Alignment *self );
rc_t CC AlignmentRelease ( const Alignment *self );

/* GetRefSeqId
 *  get id of reference sequence
 *  pass result into BAMFileGetRefSeqById to get the Reference Sequence record
 *
 *  "refSeqId" [ OUT ] - zero-based id of reference sequence
 *   returns -1 if set is invalid within BAM ( rc may be zero )
 */
rc_t CC AlignmentGetRefSeqId ( const Alignment *self, int32_t *refSeqId );

/* GetMateRefSeqId
 *  get id of mate's reference sequence
 *  pass result into BAMFileGetRefSeqById to get the Reference Sequence record
 *
 *  "refSeqId" [ OUT ] - zero-based id of reference sequence
 *   returns -1 if invalid
 */
rc_t CC AlignmentGetMateRefSeqId ( const Alignment *self, int32_t *refSeqId );

/* GetPosition
 *  get the aligned position on the ref. seq.
 *
 *  "n" [ IN ] - zero-based position index for cases of multiple alignments
 *
 *  "pos" [ OUT ] - zero-based position on reference sequence
 *  returns -1 if invalid
 */
rc_t CC AlignmentGetPosition ( const Alignment *self, int64_t *pos );

/* GetMatePosition
 *  starting coordinate of mate's alignment on ref. seq.
 *
 *  "pos" [ OUT ] - zero-based position on reference sequence
 *  returns -1 if invalid
 */
rc_t CC AlignmentGetMatePosition ( const Alignment *self, int64_t *pos );

/* GetMapQuality
 *  return the quality score of mapping
 *
 *  "qual" [ OUT ] - return param for quality score
 */
rc_t CC AlignmentGetMapQuality ( const Alignment *self, uint8_t *qual );

/* GetAlignmentDetail
 *  get the alignment details
 *
 *  "rslt" [ OUT, NULL OKAY ] and "count" [ IN ] - array to hold detail records
 *
 *  "actual" [ OUT, NULL OKAY ] - number of elements written to "rslt"
 *   required if "rslt" is NULL
 *
 *  "firstMatch" [ OUT, NULL OKAY ] - zero-based index into "rslt" of the first match to the refSeq
 *   or < 0 if invalid
 *
 *  "lastMatch" [ OUT, NULL OKAY ] - zero-based index into "rslt" of the last match to the refSeq
 *   or < 0 if invalid
 */
typedef uint32_t AlignOpType;
enum AlignOpTypes
{
    align_Match    = 'M', /* 0 */
    align_Insert   = 'I', /* 1 */
    align_Delete   = 'D', /* 2 */
    align_Skip     = 'N', /* 3 */
    align_SoftClip = 'S', /* 4 */
    align_HardClip = 'H', /* 5 */
    align_Padded   = 'P', /* 6 */
    align_Equal    = '=', /* 7 */
    align_NotEqual = 'X', /* 8 */
    align_Overlap  = 'B' /* Complete Genomics extension */
};

typedef struct AlignmentDetail AlignmentDetail;
struct AlignmentDetail
{
    int64_t refSeq_pos; /* position on refSeq where this alignment region starts or -1 if NA */
    int32_t read_pos;   /* position on read where this alignment region starts or -1 if NA */
    uint32_t length;    /* length of alignment region */
    AlignOpType type;  /* type of alignment */
};

rc_t CC AlignmentGetAlignmentDetail ( const Alignment *self,
                                      AlignmentDetail *rslt, 
                                      uint32_t count, 
                                      uint32_t *actual,
                                      int32_t *firstMatch, 
                                      int32_t *lastMatch );


/* GetCigarCount
 *  the number of CIGAR elements
 *  a CIGAR element consists of the pair of matching op code and op length
 *
 *  "n" [ OUT ] - return param for cigar count
 */
rc_t CC AlignmentGetAlignOpCount ( const Alignment *self, uint32_t *n );


/* GetInsertSize
 *  distance in bases to start of mate's alignment on ref. seq.
 *
 *  "size" [ OUT ] - >0 for first in pair, <0 for second
 */
rc_t CC AlignmentGetInsertSize ( const Alignment *self, int64_t *size );

/* GetBAMCigar
 *
 */
rc_t CC AlignmentGetBAMCigar(const Alignment *cself, uint32_t const **rslt, uint32_t *length);

/* IsSecondary
 * 
 */
bool CC AlignmentIsSecondary( const Alignment *self ); 


/* AlignmentGetCG
 * rc_t == 0, result == 0 if no CG data 
 */
rc_t CC AlignmentGetCGData ( const Alignment *self, const CGData** result);

/*--------------------------------------------------------------------------
 * CGData
 */
rc_t CC CGDataAddRef ( const CGData *self );
rc_t CC CGDataRelease ( const CGData *self );

/* CGGetSeqQual
 */
rc_t CC CGDataGetSeqQual ( const CGData* self,
                           char sequence[/* 35 */],
                           uint8_t quality[/* 35 */] );

/* CGGetCigar
 */
rc_t CC CGDataGetCigar ( const CGData* self,
                         uint32_t *cigar,
                         uint32_t cig_max,
                         uint32_t *cig_act );

/* CGGetAlignGroup
 */
rc_t CC CGDataGetAlignGroup ( const CGData* self,
                              char buffer[],
                              size_t max_size,
                              size_t *act_size );

/*--------------------------------------------------------------------------
 * Rejected
 */

/* AddRef
 * Release
 */
rc_t CC RejectedAddRef ( const Rejected *self );
rc_t CC RejectedRelease ( const Rejected *self );

/* GetError
 *  "text" [ OUT ] - NUL-terminated error message, held internally
 *  "line" [ OUT ] - 1-based line # in the source (0 for binary formats)
 *  "column" [ OUT ] - 1-based column # in the source (offset from the start of the file for binary formats)
 *  "fatal" [ OUT ] - no further parsing should be done (likely an unsupported format)
 */
rc_t CC RejectedGetError( const Rejected* self, const char** text, uint64_t* line, uint64_t* column, bool* fatal );

/* GetData
 *  "data" [ OUT ] - raw input representing the rejected record. held internally
 *  "length" [ OUT ] - size of the data buffer
 */
rc_t CC RejectedGetData( const Rejected* self, const void** text, size_t* length );

/*--------------------------------------------------------------------------
 * ReferenceInfo
 */
typedef struct ReferenceSequence 
{
    uint64_t length;
    const char *name; /* not null unique */
    const uint8_t *checksum;
} ReferenceSequence;

typedef struct ReadGroup
{
    const char *name; /* not null unique, accession e.g. SRR001138 */
    const char *platform; /* e.g. ILLUMINA */
} ReadGroup;

rc_t CC ReferenceInfoAddRef ( const ReferenceInfo *self );
rc_t CC ReferenceInfoRelease ( const ReferenceInfo *self );

/* GetRefSeqCount
 *  get the number of Reference Sequences refered to in the header
 *  this is not necessarily the number of Reference Sequences referenced
 *  by the alignments
 */
rc_t CC ReferenceInfoGetRefSeqCount ( const ReferenceInfo *self, uint32_t* count );

/* GetRefSeq
 *  get the n'th Ref. Seq. where n is [0..RefSeqCount)
 *  the result is populated with pointers that are good for precisely at long as the ReferenceInfo exists.
 */
rc_t CC ReferenceInfoGetRefSeq ( const ReferenceInfo *self, uint32_t n, ReferenceSequence *result );

/* GetReadGroupCount
 *  get the number of Read Groups (accessions, etc.) refered to in the header
 *  this is not necessarily the number of Read Groups referenced
 *  by the alignments
 */
rc_t CC ReferenceInfoGetReadGroupCount ( const ReferenceInfo *self, uint32_t *count );

/* GetReadGroup
 *  get the n'th Read Group where n is [0..ReadGroupCount)
 *  the result is populated with pointers that are good for precisely at long as the ReferenceInfo exists.
 */
rc_t CC ReferenceInfoGetReadGroup ( const ReferenceInfo *self, unsigned n, ReadGroup *result );

/* GetReadGroupByName
 *  get a Read Group by its name
 *  the result is populated with pointers that are good for precisely at long as the ReferenceInfo exists.
 */
rc_t CC ReferenceInfoGetReadGroupByName ( const ReferenceInfo *self, const char *name, ReadGroup *result );

#ifdef __cplusplus
}
#endif

#endif /* _h_common_reader_ */
