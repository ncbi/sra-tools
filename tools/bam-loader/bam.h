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

#ifndef _h_align_bam_
#define _h_align_bam_

#ifndef _h_align_extern_
#include <align/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct KDirectory;
struct KFile;
struct VPath;
struct AlignAccessDB;
struct AlignAccessAlignmentEnumerator;


/*--------------------------------------------------------------------------
 * BAMAlignment
 */
typedef struct BAMAlignment BAMAlignment;

    
/* GetBAMAlignment
 *  get property
 *
 * Release with BAMAlignmentRelease.
 */
ALIGN_EXTERN rc_t CC AlignAccessAlignmentEnumeratorGetBAMAlignment
    ( const struct AlignAccessAlignmentEnumerator *self, const BAMAlignment **result );


/* AddRef
 * Release
 */
ALIGN_EXTERN rc_t CC BAMAlignmentAddRef ( const BAMAlignment *self );
ALIGN_EXTERN rc_t CC BAMAlignmentRelease ( const BAMAlignment *self );


/* GetReadLength
 *  get the sequence length
 *  i.e. the number of elements of both sequence and quality
 *
 *  "length" [ OUT ] - length in bases of query sequence and quality
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetReadLength ( const BAMAlignment *self, uint32_t *length );


/* GetSequence
 *  get the sequence data [0..ReadLength)
 *  caller provides buffer of ReadLength bytes
 *
 *  "sequence" [ OUT ] - pointer to a buffer of at least ReadLength bytes
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetSequence ( const BAMAlignment *self, char *sequence );

/* GetSequence2
 *  get the sequence data [0..ReadLength)
 *  caller provides buffer of ReadLength bytes
 *
 *  "sequence" [ OUT ] - pointer to a buffer of at least ReadLength bytes
 *
 *  "start" [ IN ] and "stop" [ IN ] - zero-based coordinates, half-closed interval
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetSequence2 ( const BAMAlignment *self, char *sequence, uint32_t start, uint32_t stop);

    
/* GetQuality
 *  get the raw quality data [0..ReadLength)
 *  values are unsigned with 0xFF == missing
 *
 *  "quality" [ OUT ] - return param for quality sequence
 *   held internally, validity is guaranteed for the life of the BAMAlignment
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetQuality ( const BAMAlignment *self, const uint8_t **quality );

/* GetQuality2
 *  get the raw quality data [0..ReadLength) from OQ if possible else from QUAL
 *  values are unsigned with 0xFF == missing
 *
 *  "quality" [ OUT ] - return param for quality sequence
 *   held internally, validity is guaranteed for the life of the BAMAlignment
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetQuality2(const BAMAlignment *self, const uint8_t **quality, uint8_t *offset);

/* GetRefSeqId
 *  get id of reference sequence
 *  pass result into BAMFileGetRefSeqById to get the Reference Sequence record
 *
 *  "refSeqId" [ OUT ] - zero-based id of reference sequence
 *   returns -1 if set as invalid within BAM ( rc may be zero )
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetRefSeqId ( const BAMAlignment *self, int32_t *refSeqId );

/* GetMateRefSeqId
 *  get id of mate's reference sequence
 *  pass result into BAMFileGetRefSeqById to get the Reference Sequence record
 *
 *  "refSeqId" [ OUT ] - zero-based id of reference sequence
 *   returns -1 if invalid
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetMateRefSeqId ( const BAMAlignment *self, int32_t *refSeqId );


/* GetPosition
 *  get the aligned position on the ref. seq.
 *
 *  "n" [ IN ] - zero-based position index for cases of multiple alignments
 *
 *  "pos" [ OUT ] - zero-based position on reference sequence
 *  returns -1 if invalid
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetPosition ( const BAMAlignment *self, int64_t *pos );
    
/* GetPosition2
 *  get the aligned start position on the ref. seq.
 *  get the aligned length on the ref. seq.
 *
 *  "n" [ IN ] - zero-based position index for cases of multiple alignments
 *
 *  "pos" [ OUT ] - zero-based position on reference sequence
 *  returns -1 if invalid
 *
 *  "length" [ OUT ] - length of alignment on reference sequence
 *  returns 0 if invalid
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetPosition2 ( const BAMAlignment *self, int64_t *pos, uint32_t *length );
    

/* GetMatePosition
 *  starting coordinate of mate's alignment on ref. seq.
 *
 *  "pos" [ OUT ] - zero-based position on reference sequence
 *  returns -1 if invalid
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetMatePosition ( const BAMAlignment *self, int64_t *pos );


/* IsMapped
 *  is the alignment mapped to something
 */
ALIGN_EXTERN bool CC BAMAlignmentIsMapped ( const BAMAlignment *self );


/* GetReadGroupName
 *  get the name of the read group (i.e. accession)
 *  pass result into BAMFileGetReadGroupByName to get the Read Group record
 *
 *  "name" [ OUT ] - return param for NUL-terminated read group name
 *   held internally, validity is guaranteed for the life of the BAMAlignment
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetReadGroupName ( const BAMAlignment *self, const char **name );


/* GetReadName
 *  get the read name (i.e. spot name)
 * GetReadName2
 *  get the read name and length in bytes
 *
 *  "name" [ OUT ] - return param for NUL-terminated read name
 *   held internally, validity is guaranteed for the life of the BAMAlignment
 *
 *  "length" [ OUT ] - return the number of bytes in "name"
 *   excluding terminating NUL.
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetReadName ( const BAMAlignment *self, const char **name );
ALIGN_EXTERN rc_t CC BAMAlignmentGetReadName2 ( const BAMAlignment *self, const char **name, size_t *length );
    
    
/* GetReadName3
 *  get the read name and length in bytes
 *  applies fixups to name
 *
 *  "name" [ OUT ] - return param for read name
 *   held internally, validity is guaranteed for the life of the BAMAlignment
 *
 *  "length" [ OUT ] - return the number of bytes in "name"
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetReadName3 ( const BAMAlignment *self, const char **name, size_t *length );

/* HasColorSpace
 *  Does the alignment have colorspace info
 */
ALIGN_EXTERN bool CC BAMAlignmentHasColorSpace ( const BAMAlignment *self );

/* GetCSKey
 *  get the colorspace key
 *
 *  "cskey" [ OUT ] - return param 
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetCSKey ( const BAMAlignment *self, char cskey[1] );

ALIGN_EXTERN rc_t CC BAMAlignmentGetCSSeqLen ( const BAMAlignment *self, uint32_t *seqLen );
/* GetCSSequence
 *  get the colorspace sequence data [0..seqLen)
 *  caller provides buffer of seqLen bytes
 *
 *  "csseq" [ OUT ] - pointer to a buffer of at least seqLen bytes
 *  "seqLen" [ IN ] - length of sequence from BAMAlignmentGetCSSeqLen
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetCSSequence ( const BAMAlignment *self, char *csseq, uint32_t seqLen );

ALIGN_EXTERN rc_t CC BAMAlignmentGetCSQuality(BAMAlignment const *cself, uint8_t const **quality, uint8_t *offset);


/* GetFlags
 *  return the raw "flags" bitmap word
 *
 *  "flags" [ OUT ] - return parameter for bitmap word
 */
enum BAMFlags
{
    BAMFlags_bit_WasPaired = 0,  /* was paired when sequenced */
    BAMFlags_bit_IsMappedAsPair,
    BAMFlags_bit_SelfIsUnmapped,
    BAMFlags_bit_MateIsUnmapped,
    BAMFlags_bit_SelfIsReverse,
    BAMFlags_bit_MateIsReverse,
    BAMFlags_bit_IsFirst,        /* and mate exists */
    BAMFlags_bit_IsSecond,       /* and mate exists */
    BAMFlags_bit_IsNotPrimary,   /* a read having split hits may have multiple primary alignments */
    BAMFlags_bit_IsLowQuality,   /* fails platform/vendor quality checks */
    BAMFlags_bit_IsDuplicate,    /* PCR or optical dup */
    
    BAMFlags_WasPaired      = (1 << BAMFlags_bit_WasPaired),
    BAMFlags_IsMappedAsPair	= (1 << BAMFlags_bit_IsMappedAsPair),
    BAMFlags_SelfIsUnmapped	= (1 << BAMFlags_bit_SelfIsUnmapped),
    BAMFlags_MateIsUnmapped	= (1 << BAMFlags_bit_MateIsUnmapped),
    BAMFlags_SelfIsReverse	= (1 << BAMFlags_bit_SelfIsReverse),
    BAMFlags_MateIsReverse	= (1 << BAMFlags_bit_MateIsReverse),
    BAMFlags_IsFirst        = (1 << BAMFlags_bit_IsFirst),
    BAMFlags_IsSecond       = (1 << BAMFlags_bit_IsSecond),
    BAMFlags_IsNotPrimary	= (1 << BAMFlags_bit_IsNotPrimary),
    BAMFlags_IsLowQuality	= (1 << BAMFlags_bit_IsLowQuality),
    BAMFlags_IsDuplicate	= (1 << BAMFlags_bit_IsDuplicate)
};

ALIGN_EXTERN rc_t CC BAMAlignmentGetFlags ( const BAMAlignment *self, uint16_t *flags );


/* GetMapQuality
 *  return the quality score of mapping
 *
 *  "qual" [ OUT ] - return param for quality score
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetMapQuality ( const BAMAlignment *self, uint8_t *qual );


/* GetAlignmentDetail
 *  get the alignment details
 *
 *  "rslt" [ OUT, NULL OKAY ] and "count" [ IN ] - array to hold detail records
 *
 *  "actual" [ OUT, NULL OKAY ] - number of elements written to "rslt"
 *   required if "rslt" is not NULL
 *
 *  "firstMatch" [ OUT, NULL OKAY ] - zero-based index into "rslt" of the first match to the refSeq
 *   or < 0 if invalid
 *
 *  "lastMatch" [ OUT, NULL OKAY ] - zero-based index into "rslt" of the last match to the refSeq
 *   or < 0 if invalid
 */
typedef uint32_t BAMCigarType;
enum BAMCigarTypes
{
    ct_Match    = 'M', /* 0 */
    ct_Insert   = 'I', /* 1 */
    ct_Delete   = 'D', /* 2 */
    ct_Skip     = 'N', /* 3 */
    ct_SoftClip = 'S', /* 4 */
    ct_HardClip = 'H', /* 5 */
    ct_Padded   = 'P', /* 6 */
    ct_Equal    = '=', /* 7 */
    ct_NotEqual = 'X', /* 8 */
    ct_Overlap  = 'B' /* Complete Genomics extension */
};

typedef struct BAMAlignmentDetail BAMAlignmentDetail;
struct BAMAlignmentDetail
{
    int64_t refSeq_pos; /* position on refSeq where this alignment region starts or -1 if NA */
    int32_t read_pos;   /* position on read where this alignment region starts or -1 if NA */
    uint32_t length;    /* length of alignment region */
    BAMCigarType type;  /* type of alignment */
};

ALIGN_EXTERN rc_t CC BAMAlignmentGetAlignmentDetail ( const BAMAlignment *self,
    BAMAlignmentDetail *rslt, uint32_t count, uint32_t *actual,
    int32_t *firstMatch, int32_t *lastMatch );


/* GetCigarCount
 *  the number of CIGAR elements
 *  a CIGAR element consists of the pair of matching op code and op length
 *
 *  "n" [ OUT ] - return param for cigar count
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetCigarCount ( const BAMAlignment *self, uint32_t *n );


ALIGN_EXTERN rc_t CC BAMAlignmentGetRawCigar(const BAMAlignment *cself, uint32_t const **rslt, uint32_t *length);

/* GetCigar
 *  get CIGAR element n [0..GetCigarCount)
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetCigar ( const BAMAlignment *self,
    uint32_t n, BAMCigarType *type, uint32_t *length );


/* GetInsertSize
 *  distance in bases to start of mate's alignment on ref. seq.
 *
 *  "size" [ OUT ] - >0 for first in pair, <0 for second
 */
ALIGN_EXTERN rc_t CC BAMAlignmentGetInsertSize ( const BAMAlignment *self, int64_t *size );

ALIGN_EXTERN rc_t CC BAMAlignmentFormatSAM(const BAMAlignment *self,
                                           size_t *actsize,
                                           size_t maxsize,
                                           char *buffer);

/* OptDataForEach
 *  DANGER
 *  these optional fields are the weakest part of BAM.
 *
 *  It is probably best to not use this info.
 *  You can't count on them being there.
 *  Moreover, you might need to interpret the types correctly.
 */
typedef uint32_t BAMOptDataValueType;
enum BAMOptDataValueTypes
{
    dt_CSTRING = 'Z',
    dt_INT8 = 'c',
    dt_UINT8 = 'C',
    dt_INT16 = 's',     
    dt_UINT16 = 'S',    
    dt_INT = 'i',
    dt_UINT = 'I',
    dt_FLOAT32 = 'f',
#if 0
    dt_FLOAT64 = 'd', /* removed? not in Dec 19 2013 version of SAMv1.pdf */
#endif
    dt_ASCII = 'A',
    dt_HEXSTRING = 'H',
    dt_NUM_ARRAY = 'B'
};

#define OPT_TAG_X "X?" /* end user data */
#define OPT_TAG_Y "Y?" /* end user data */
#define OPT_TAG_Z "Z?" /* end user data */

#define OPT_TAG_ReadGroup   "RG" /* Read Group; same as BAMAlignmentGetReadGroupName */
#define OPT_TAG_Library     "LB" /* LIbrary; also BAMReadGroup */
#define OPT_TAG_Unit        "PU" /* Platform specific Unit; also BAMReadGroup */
#define OPT_TAG_Program     "PG" /* Alignment software name */
#define OPT_TAG_AlignScore  "AS" /* Alignment Score (MapQuality?) */
#define OPT_TAG_SecQual     "SQ" /* second called base:2 and quality:6; length == ReadLength? warning */
#define OPT_TAG_MateMapQual "MQ" /* map Quality of mate */
#define OPT_TAG_NumMismatch "NM" /* Number of Mismatches */
#define OPT_TAG_Hits0       "H0" /* Number of perfect hits */
#define OPT_TAG_Hits1       "H1" /* Number of off-by-one */
#define OPT_TAG_Hits2       "H2" /* Number of off-by-two */
#define OPT_TAG_CondQual    "UQ" /* conditional Quality of read */
#define OPT_TAG_CondQPair   "PQ" /* conditional Quality of pair */
#define OPT_TAG_ReadHits    "NH" /* Number of times this read (spot) aligns */
#define OPT_TAG_ReadHits2   "IH" /* Number of times this read (spot) aligns that are in this file */
#define OPT_TAG_HitIndex    "HI" /* n-th hit for this read in this file */
#define OPT_TAG_Match2      "MD" /* another sort of matching string like CIGAR but different? */
#define OPT_TAG_ColorKey    "CS" /* primer and first color */
#define OPT_TAG_ColorQual   "CQ" /* quality of above */
#define OPT_TAG_ColorMisses "CM" /* Number of color-space Mismatches */
#define OPT_TAG_SeqOverlap  "GS" 
#define OPT_TAG_QualOverlap "GQ" 
#define OPT_TAG_OverlapDesc "GC"
#define OPT_TAG_MateSeq     "R2" /* sequence of the mate */
#define OPT_TAG_MateQual    "Q2" /* quality scores of the mate */
#define OPT_TAG_OtherQual   "S2"
#define OPT_TAG_NextHitRef  "CC" /* Reference name of the next hit */
#define OPT_TAG_NextHitPos  "CP" /* coordinate of the next hit */
#define OPT_TAG_SingleMapQ  "SM" /* quality of mapping as if not paired */
#define OPT_TAG_AM          "AM"
#define OPT_TAG_MAQFlag     "MQ"


struct BAMOptData
{
    BAMOptDataValueType type;
    uint32_t element_count;
    union {
        int8_t i8[8];
        uint8_t u8[8];
        int16_t i16[4];
        uint16_t u16[4];
        int32_t i32[2];
        uint32_t u32[2];
        int64_t i64[2];
        uint64_t u64[2];
        float f32[2];
        double f64[1];
        char asciiz[8];
    } u;
};

typedef struct BAMOptData BAMOptData;

typedef rc_t ( CC * BAMOptionalDataFunction )
    ( void *ctx, const char tag[2], const BAMOptData *value );

ALIGN_EXTERN rc_t CC BAMAlignmentOptDataForEach
    ( const BAMAlignment *self, void *ctx, BAMOptionalDataFunction callback );

    
ALIGN_EXTERN bool CC BAMAlignmentHasCGData(BAMAlignment const *self);

ALIGN_EXTERN
rc_t CC BAMAlignmentCGReadLength(BAMAlignment const *self, uint32_t *readlen);

ALIGN_EXTERN
rc_t CC BAMAlignmentGetCGSeqQual(BAMAlignment const *self,
                                 char sequence[],
                                 uint8_t quality[]);

ALIGN_EXTERN
rc_t CC BAMAlignmentGetCGCigar(BAMAlignment const *self,
                               uint32_t *cigar,
                               uint32_t cig_max,
                               uint32_t *cig_act);
    
ALIGN_EXTERN rc_t BAMAlignmentGetTI(BAMAlignment const *self, uint64_t *ti);

/* strand = '+', '-', or ' ' */
ALIGN_EXTERN rc_t BAMAlignmentGetRNAStrand(BAMAlignment const *self, uint8_t *strand);

ALIGN_EXTERN rc_t BAMAlignmentGetCGAlignGroup(BAMAlignment const *self,
                                              char buffer[],
                                              size_t max_size,
                                              size_t *act_size);
    
    
/*--------------------------------------------------------------------------
 * BAMFile
 */
typedef struct BAMFile BAMFile;

typedef struct BAMRefSeq BAMRefSeq;
struct BAMRefSeq
{
    uint64_t length;
    const char *name; /* not null unique */
    const char *assemblyId;
    const uint8_t *checksum;
    const char *uri;
    const char *species;
    uint32_t id;
    uint8_t checksum_array[16];
};

typedef struct BAMReadGroup BAMReadGroup;
struct BAMReadGroup
{
    const char *name; /* not null unique, accession e.g. SRR001138 */
    const char *sample; /* not null */
    const char *library;
    const char *description;
    const char *unit; /* platform specific identifier, e.g. BI.080214_SL-XAJ_0001_FC2044KAAXX.7 */
    const char *insertSize;
    const char *center; /* e.g. BI */
    const char *runDate;
    const char *platform; /* e.g. ILLUMINA */
    uint32_t id;
};


/* 64-bit structure stored as an integer
 * The high-order 48 bits store the position in the file at which a 
 * compressed block starts.  The low-order 16 bits store the position
 * in the decompressed block at which a record starts.  This is the
 * way that positions are represented in BAM indices.
 */
typedef uint64_t BAMFilePosition;


/* Make
 *  open the BAM file specified by path
 *
 *  "path" [ IN ] - NUL terminated string or format
 */
ALIGN_EXTERN rc_t CC BAMFileMake ( const BAMFile **result, const char *path, ... );

ALIGN_EXTERN rc_t CC BAMFileMakeWithHeader ( const BAMFile **result,
                                            char const headerText[],
                                            char const path[], ... );

/* MakeWithDir
 *  open the BAM file specified by path and supplied directory
 *
 *  "dir" [ IN ] - directory object used to open file
 *
 *  "path" [ IN ] - NUL terminated string or format
 */
ALIGN_EXTERN rc_t CC BAMFileMakeWithDir ( const BAMFile **result,
    struct KDirectory const *dir, const char *path, ... );
ALIGN_EXTERN rc_t CC BAMFileVMakeWithDir ( const BAMFile **result,
    struct KDirectory const *dir, const char *path, va_list args );

/* Make
 *  open the BAM file specified by file
 *
 *  "file" [ IN ] - an open KFile
 */
ALIGN_EXTERN rc_t CC BAMFileMakeWithKFile(const BAMFile **result,
    struct KFile const *file);

/* Make
 *  open the BAM file specified by file
 *
 *  "file" [ IN ] - an open KFile
 */
ALIGN_EXTERN rc_t CC BAMFileMakeWithVPath(const BAMFile **result,
    struct VPath const *path);

/* ExportBAMFile
 *  export the BAMFile object in use by the AlignAccessDB, if any
 *  must be released via BAMFileRelease
 */
ALIGN_EXTERN rc_t CC AlignAccessDBExportBAMFile ( struct AlignAccessDB const *self,
    const BAMFile **result );


/* AddRef
 * Release
 */
ALIGN_EXTERN rc_t CC BAMFileAddRef ( const BAMFile *self );
ALIGN_EXTERN rc_t CC BAMFileRelease ( const BAMFile *self );


/* GetPosition
 *  get the position of the about-to-be read alignment
 *  this position can be stored
 *  this position can be passed into SetPosition to seek to the same alignment
 *
 *  "pos" [ OUT ] - return parameter for position
 */
ALIGN_EXTERN rc_t CC BAMFileGetPosition ( const BAMFile *self, BAMFilePosition *pos );


/* GetProportionalPosition
 *  get the aproximate proportional position in the input file
 *  this is intended to be useful for computing progress
 *
 * NB - does not return rc_t
 */
ALIGN_EXTERN float CC BAMFileGetProportionalPosition ( const BAMFile *self );


/* Read
 *  read an aligment
 *
 *  "result" [ OUT ] - return param for BAMAlignment object
 *   must be released with BAMAlignmentRelease
 *
 *  returns RC(..., ..., ..., rcRow, rcNotFound) at end
 */
ALIGN_EXTERN rc_t CC BAMFileRead ( const BAMFile *self, const BAMAlignment **result );

    
/* Read
 *  read an aligment
 *
 *  "result" [ OUT ] - return param for BAMAlignment object
 *   must be released with BAMAlignmentRelease, is invalidated or contents
 *   change on next call to BAMFileRead2. Unlike with BAMFileRead, no attempt is
 *   made to preserve this object.
 *
 *  returns:
 *    RC(..., ..., ..., rcRow, rcNotFound) at end
 *    RC(..., ..., ..., rcRow, rcInvalid) and RC(..., ..., ..., rcRow, rcEmpty)
 *      are not fatal and are resumable
 *
 *  tries to use static buffers and will log messages about parsing errors
 */
ALIGN_EXTERN rc_t CC BAMFileRead2 ( const BAMFile *self, const BAMAlignment **result );


/* Rewind
 *  reset the position back to the first aligment in the file
 */
ALIGN_EXTERN rc_t CC BAMFileRewind ( const BAMFile *self );


/* SetPosition
 *  set the position to a particular alignment
 *  pass in the values from GetPosition
 */
ALIGN_EXTERN rc_t CC BAMFileSetPosition ( const BAMFile *self, const BAMFilePosition *pos );


/* GetRefSeqCount
 *  get the number of Reference Sequences refered to in the header
 *  this is not necessarily the number of Reference Sequences referenced
 *  by the alignments
 */
ALIGN_EXTERN rc_t CC BAMFileGetRefSeqCount ( const BAMFile *self, uint32_t *count );


/* GetRefSeq
 *  get the n'th Ref. Seq. where n is [0..RefSeqCount)
 *  the resulting pointer is static-like; it is freed when the BAMFile is.
 *  IOW, it is good for precisely at long as the BAMFile is.
 */
ALIGN_EXTERN rc_t CC BAMFileGetRefSeq ( const BAMFile *self, uint32_t n, const BAMRefSeq **result );


/* GetRefSeqById
 *  get a Ref. Seq. by its id
 *  the resulting pointer is static-like; it is freed when the BAMFile is.
 *  IOW, it is good for precisely at long as the BAMFile is.
 */
ALIGN_EXTERN rc_t CC BAMFileGetRefSeqById ( const BAMFile *self, int32_t id, const BAMRefSeq **result );


/* GetReadGroupCount
 *  get the number of Read Groups (accessions, etc.) refered to in the header
 *  this is not necessarily the number of Read Groups referenced
 *  by the alignments
 */
ALIGN_EXTERN rc_t CC BAMFileGetReadGroupCount ( const BAMFile *self, uint32_t *count );

/* GetReadGroup
 *  get the n'th Read Group where n is [0..ReadGroupCount)
 *  the resulting pointer is static-like; it is freed when the BAMFile is.
 *  IOW, it is good for precisely at long as the BAMFile is.
 */
ALIGN_EXTERN rc_t CC BAMFileGetReadGroup ( const BAMFile *self, unsigned n, const BAMReadGroup **result );
    
/* GetHeaderText
 *  get the text of the BAM header file
 *  the resulting pointer is static-like; it is freed when the BAMFile is.
 *  IOW, it is good for precisely at long as the BAMFile is.
 */
ALIGN_EXTERN rc_t CC BAMFileGetHeaderText(BAMFile const *cself, char const **header, size_t *header_len);
    

/* GetReadGroupByName
 *  get a Read Group by its name
 *  the resulting pointer is static-like; it is freed when the BAMFile is.
 *  IOW, it is good for precisely at long as the BAMFile is.
 */
ALIGN_EXTERN rc_t CC BAMFileGetReadGroupByName ( const BAMFile *self,
    const char *name, const BAMReadGroup **result );


/* OpenIndex
 *  takes a simple path...
 */
ALIGN_EXTERN rc_t CC BAMFileOpenIndex ( const BAMFile *self, const char *path );

ALIGN_EXTERN rc_t CC BAMFileOpenIndexWithVPath ( const BAMFile *self, struct VPath const *path );

/* IsIndexed
 *  returns true if true
 */
ALIGN_EXTERN bool CC BAMFileIsIndexed ( const BAMFile *self );


/* IndexHasRefSeqId
 */
ALIGN_EXTERN bool CC BAMFileIndexHasRefSeqId ( const BAMFile *self, uint32_t refSeqId );

/* Seek
 *  seeks a half-open zero-based interval on a particular reference
 *  rcSelf, rcIncomplete
 */
ALIGN_EXTERN rc_t CC BAMFileSeek ( const BAMFile *self, uint32_t refSeqId, uint64_t alignStart, uint64_t alignEnd );

typedef uint32_t BAMValidateOption;
enum BAMValidateOptions {
    /* this is the minimum level of BAM file validation; just walks the compressed block headers */
    bvo_BlockHeaders        = 1,
    
    /* decompresses each block */
    bvo_BlockCompression    = 2,
    
    /* within each block, walks the records without examining the contents */
    bvo_BlockStructure      = 3,
    
    /* within each record, validate the structure vis-a-vis the record size */
    bvo_RecordStructure     = 4,
    
    /* verify that the extra fields a parsable */
    bvo_ExtraFields         = 8,
    
    /* confirm that no alignment starts before the alignment preceeding it */
    bvo_Sorted              = 16,
    
    /* verify that flags are consistent with itself and the other fields in the record
     * NB. can not verify secondary alignment flag 0x100
     */
    bvo_FlagsConsistency    = 32,
    
    /* verify CIGAR against sequence length */
    bvo_CIGARConsistency    = 64,
    
    /* verify that bin number corresponds to position and alignment length */
    bvo_BinConsistency      = 128,
    
    /* verify that mapQ is consistent with flags and refSeqID */
    bvo_MapQuality          = 256,
    
    /* verify Quality values >= 33 */
    bvo_QualityValues       = 512,
    
    /* verify that sequence length != 0 */
    bvo_MissingSequence     = 1024,
    
    /* verify that Quality values != 255 */
    bvo_MissingQuality      = 2048,
    
    /* compute flagstats */
    bvo_FlagsStats          = 4096,
    
    /* verify that index is parsable
     * NB. this can be done without a BAM file
     */
    bvo_IndexStructure      = 1 << 16,
    
    /* verify that the file offsets in the index are valid for the given BAM file
     * NB. does not cause decompression of the BAM file but will cause referenced
     * block headers to be validated
     */
    bvo_IndexOffsets1       = 2 << 16,
    
    /* in addition to verifying that the file offsets in the index are valid for
     * the given BAM file, verify that the record offsets with the blocks are valid.
     * NB. will cause referenced blocks to be decompressed and structurally validated.
     */
    bvo_IndexOffsets2       = 3 << 16,
    
    /* verify that index's reference number and bin number agree with the
     * referenced record
     */
    bvo_IndexBins           = 4 << 16,
    
    bvo_IndexOptions        = 7 << 16
};

typedef struct BAMValidateStats BAMValidateStats;
typedef struct BAMValidateStatsRow BAMValidateStatsRow;

struct BAMValidateStatsRow {
    uint64_t good;
    uint64_t warning;
    uint64_t error;
};

struct BAMValidateStats {
    uint64_t bamFileSize;
    uint64_t bamFilePosition;
    uint64_t baiFileSize;
    uint64_t baiFilePosition;
    BAMValidateStatsRow blockHeaders;
    BAMValidateStatsRow blockCompression;
    BAMValidateStatsRow blockStructure;
    BAMValidateStatsRow recordStructure;
    BAMValidateStatsRow extraFields;
    BAMValidateStatsRow inOrder;
    BAMValidateStatsRow flags[16];
    BAMValidateStatsRow CIGAR;
    BAMValidateStatsRow bin;
    BAMValidateStatsRow quality;
    BAMValidateStatsRow hasSequence;
    BAMValidateStatsRow hasQuality;
    BAMValidateStatsRow indexFileOffset;
    BAMValidateStatsRow indexBlockOffset;
    BAMValidateStatsRow indexBin;
    bool bamHeaderIsGood;
    bool bamHeaderIsBad;
    bool indexStructureIsGood;
    bool indexStructureIsBad;
};

typedef rc_t (CC *BAMValidateCallback)(void *ctx, rc_t result, const BAMValidateStats *stats);

/* Validate
 */
ALIGN_EXTERN rc_t CC BAMValidate ( struct VPath const *bam,
                                   struct VPath const *bai,
                                   BAMValidateOption options,
                                   BAMValidateCallback callback,
                                   void *callbackContext
                                  );


#ifdef __cplusplus
}
#endif

#endif /* _h_align_bam_ */
