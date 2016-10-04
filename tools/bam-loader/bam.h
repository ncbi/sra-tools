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

/*--------------------------------------------------------------------------
 * BAM_Alignment
 */
typedef struct BAM_Alignment BAM_Alignment;

/* AddRef
 * Release
 */
rc_t BAM_AlignmentAddRef ( const BAM_Alignment *self );
rc_t BAM_AlignmentRelease ( const BAM_Alignment *self );

rc_t BAM_AlignmentCopy(const BAM_Alignment *self, BAM_Alignment **rslt);

/* GetReadLength
 *  get the sequence length
 *  i.e. the number of elements of both sequence and quality
 *
 *  "length" [ OUT ] - length in bases of query sequence and quality
 */
rc_t BAM_AlignmentGetReadLength ( const BAM_Alignment *self, uint32_t *length );


/* GetSequence
 *  get the sequence data [0..ReadLength)
 *  caller provides buffer of ReadLength bytes
 *
 *  "sequence" [ OUT ] - pointer to a buffer of at least ReadLength bytes
 */
rc_t BAM_AlignmentGetSequence ( const BAM_Alignment *self, char *sequence );

/* GetSequence2
 *  get the sequence data [0..ReadLength)
 *  caller provides buffer of ReadLength bytes
 *
 *  "sequence" [ OUT ] - pointer to a buffer of at least ReadLength bytes
 *
 *  "start" [ IN ] and "stop" [ IN ] - zero-based coordinates, half-closed interval
 */
rc_t BAM_AlignmentGetSequence2 ( const BAM_Alignment *self, char *sequence, uint32_t start, uint32_t stop);

    
/* GetQuality
 *  get the raw quality data [0..ReadLength)
 *  values are unsigned with 0xFF == missing
 *
 *  "quality" [ OUT ] - return param for quality sequence
 *   held internally, validity is guaranteed for the life of the BAM_Alignment
 */
rc_t BAM_AlignmentGetQuality ( const BAM_Alignment *self, const uint8_t **quality );

/* GetQuality2
 *  get the raw quality data [0..ReadLength) from OQ if possible else from QUAL
 *  values are unsigned with 0xFF == missing
 *
 *  "quality" [ OUT ] - return param for quality sequence
 *   held internally, validity is guaranteed for the life of the BAM_Alignment
 */
rc_t BAM_AlignmentGetQuality2(const BAM_Alignment *self, const uint8_t **quality, uint8_t *offset);

/* GetRefSeqId
 *  get id of reference sequence
 *  pass result into BAM_FileGetRefSeqById to get the Reference Sequence record
 *
 *  "refSeqId" [ OUT ] - zero-based id of reference sequence
 *   returns -1 if set as invalid within BAM ( rc may be zero )
 */
rc_t BAM_AlignmentGetRefSeqId ( const BAM_Alignment *self, int32_t *refSeqId );

/* GetMateRefSeqId
 *  get id of mate's reference sequence
 *  pass result into BAM_FileGetRefSeqById to get the Reference Sequence record
 *
 *  "refSeqId" [ OUT ] - zero-based id of reference sequence
 *   returns -1 if invalid
 */
rc_t BAM_AlignmentGetMateRefSeqId ( const BAM_Alignment *self, int32_t *refSeqId );


/* GetPosition
 *  get the aligned position on the ref. seq.
 *
 *  "n" [ IN ] - zero-based position index for cases of multiple alignments
 *
 *  "pos" [ OUT ] - zero-based position on reference sequence
 *  returns -1 if invalid
 */
rc_t BAM_AlignmentGetPosition ( const BAM_Alignment *self, int64_t *pos );
    
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
rc_t BAM_AlignmentGetPosition2 ( const BAM_Alignment *self, int64_t *pos, uint32_t *length );
    

/* GetMatePosition
 *  starting coordinate of mate's alignment on ref. seq.
 *
 *  "pos" [ OUT ] - zero-based position on reference sequence
 *  returns -1 if invalid
 */
rc_t BAM_AlignmentGetMatePosition ( const BAM_Alignment *self, int64_t *pos );


/* IsMapped
 *  is the alignment mapped to something
 */
bool BAM_AlignmentIsMapped ( const BAM_Alignment *self );


/* GetReadGroupName
 *  get the name of the read group (i.e. accession)
 *  pass result into BAM_FileGetReadGroupByName to get the Read Group record
 *
 *  "name" [ OUT ] - return param for NUL-terminated read group name
 *   held internally, validity is guaranteed for the life of the BAM_Alignment
 */
rc_t BAM_AlignmentGetReadGroupName ( const BAM_Alignment *self, const char **name );


/* GetReadName
 *  get the read name (i.e. spot name)
 * GetReadName2
 *  get the read name and length in bytes
 *
 *  "name" [ OUT ] - return param for NUL-terminated read name
 *   held internally, validity is guaranteed for the life of the BAM_Alignment
 *
 *  "length" [ OUT ] - return the number of bytes in "name"
 *   excluding terminating NUL.
 */
rc_t BAM_AlignmentGetReadName ( const BAM_Alignment *self, const char **name );
rc_t BAM_AlignmentGetReadName2 ( const BAM_Alignment *self, const char **name, size_t *length );
    
    
/* GetReadName3
 *  get the read name and length in bytes
 *  applies fixups to name
 *
 *  "name" [ OUT ] - return param for read name
 *   held internally, validity is guaranteed for the life of the BAM_Alignment
 *
 *  "length" [ OUT ] - return the number of bytes in "name"
 */
rc_t BAM_AlignmentGetReadName3 ( const BAM_Alignment *self, const char **name, size_t *length );

/* HasColorSpace
 *  Does the alignment have colorspace info
 */
bool BAM_AlignmentHasColorSpace ( const BAM_Alignment *self );

/* GetCSKey
 *  get the colorspace key
 *
 *  "cskey" [ OUT ] - return param 
 */
rc_t BAM_AlignmentGetCSKey ( const BAM_Alignment *self, char cskey[1] );

rc_t BAM_AlignmentGetCSSeqLen ( const BAM_Alignment *self, uint32_t *seqLen );
/* GetCSSequence
 *  get the colorspace sequence data [0..seqLen)
 *  caller provides buffer of seqLen bytes
 *
 *  "csseq" [ OUT ] - pointer to a buffer of at least seqLen bytes
 *  "seqLen" [ IN ] - length of sequence from BAM_AlignmentGetCSSeqLen
 */
rc_t BAM_AlignmentGetCSSequence ( const BAM_Alignment *self, char *csseq, uint32_t seqLen );

rc_t BAM_AlignmentGetCSQuality(BAM_Alignment const *cself, uint8_t const **quality, uint8_t *offset);


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
    BAMFlags_bit_IsSupplemental,
    
    BAMFlags_WasPaired      = (1 << BAMFlags_bit_WasPaired),
    BAMFlags_IsMappedAsPair	= (1 << BAMFlags_bit_IsMappedAsPair),
    BAMFlags_SelfIsUnmapped	= (1 << BAMFlags_bit_SelfIsUnmapped),
    BAMFlags_MateIsUnmapped	= (1 << BAMFlags_bit_MateIsUnmapped),
    BAMFlags_SelfIsReverse  = (1 << BAMFlags_bit_SelfIsReverse),
    BAMFlags_MateIsReverse  = (1 << BAMFlags_bit_MateIsReverse),
    BAMFlags_IsFirst        = (1 << BAMFlags_bit_IsFirst),
    BAMFlags_IsSecond       = (1 << BAMFlags_bit_IsSecond),
    BAMFlags_IsNotPrimary   = (1 << BAMFlags_bit_IsNotPrimary),
    BAMFlags_IsLowQuality   = (1 << BAMFlags_bit_IsLowQuality),
    BAMFlags_IsDuplicate    = (1 << BAMFlags_bit_IsDuplicate),
    BAMFlags_IsSupplemental = (1 << BAMFlags_bit_IsSupplemental)
};

rc_t BAM_AlignmentGetFlags ( const BAM_Alignment *self, uint16_t *flags );


/* GetMapQuality
 *  return the quality score of mapping
 *
 *  "qual" [ OUT ] - return param for quality score
 */
rc_t BAM_AlignmentGetMapQuality ( const BAM_Alignment *self, uint8_t *qual );


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

typedef struct BAM_AlignmentDetail BAM_AlignmentDetail;
struct BAM_AlignmentDetail
{
    int64_t refSeq_pos; /* position on refSeq where this alignment region starts or -1 if NA */
    int32_t read_pos;   /* position on read where this alignment region starts or -1 if NA */
    uint32_t length;    /* length of alignment region */
    BAMCigarType type;  /* type of alignment */
};

rc_t BAM_AlignmentGetAlignmentDetail ( const BAM_Alignment *self,
    BAM_AlignmentDetail *rslt, uint32_t count, uint32_t *actual,
    int32_t *firstMatch, int32_t *lastMatch );


/* GetCigarCount
 *  the number of CIGAR elements
 *  a CIGAR element consists of the pair of matching op code and op length
 *
 *  "n" [ OUT ] - return param for cigar count
 */
rc_t BAM_AlignmentGetCigarCount ( const BAM_Alignment *self, uint32_t *n );


rc_t BAM_AlignmentGetRawCigar(const BAM_Alignment *cself, uint32_t const **rslt, uint32_t *length);

/* GetCigar
 *  get CIGAR element n [0..GetCigarCount)
 */
rc_t BAM_AlignmentGetCigar ( const BAM_Alignment *self,
    uint32_t n, BAMCigarType *type, uint32_t *length );


/* GetInsertSize
 *  distance in bases to start of mate's alignment on ref. seq.
 *
 *  "size" [ OUT ] - >0 for first in pair, <0 for second
 */
rc_t BAM_AlignmentGetInsertSize ( const BAM_Alignment *self, int64_t *size );

rc_t BAM_AlignmentFormatSAM(const BAM_Alignment *self,
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

#define OPT_TAG_ReadGroup   "RG" /* Read Group; same as BAM_AlignmentGetReadGroupName */
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

typedef rc_t ( * BAMOptionalDataFunction )
    ( void *ctx, const char tag[2], const BAMOptData *value );

rc_t BAM_AlignmentOptDataForEach
    ( const BAM_Alignment *self, void *ctx, BAMOptionalDataFunction callback );

    
bool BAM_AlignmentHasCGData(BAM_Alignment const *self);

rc_t BAM_AlignmentCGReadLength(BAM_Alignment const *self, uint32_t *readlen);

rc_t BAM_AlignmentGetCGSeqQual(BAM_Alignment const *self,
                                 char sequence[],
                                 uint8_t quality[]);

rc_t BAM_AlignmentGetCGCigar(BAM_Alignment const *self,
                               uint32_t *cigar,
                               uint32_t cig_max,
                               uint32_t *cig_act);
    
rc_t BAM_AlignmentGetTI(BAM_Alignment const *self, uint64_t *ti);

/* strand = '+', '-', or ' ' */
rc_t BAM_AlignmentGetRNAStrand(BAM_Alignment const *self, uint8_t *strand);

rc_t BAM_AlignmentGetCGAlignGroup(BAM_Alignment const *self,
                                              char buffer[],
                                              size_t max_size,
                                              size_t *act_size);

rc_t BAM_AlignmentGetLinkageGroup(BAM_Alignment const *self,
                                  char const ** BX,
                                  char const ** CB,
                                  char const ** UB);
    
rc_t BAM_AlignmentGetBarCode(BAM_Alignment const *self,
                                  char const **BC);

    
/*--------------------------------------------------------------------------
 * BAM_File
 */
typedef struct BAM_File BAM_File;

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
typedef uint64_t BAM_FilePosition;


/* Make
 *  open the BAM file specified by path
 *
 *  "path" [ IN ] - NUL terminated string or format
 */
rc_t BAM_FileMake(const BAM_File **result,
                  KFile *defer,
                  char const headerText[],
                  char const path[], ... );

/* AddRef
 * Release
 */
rc_t BAM_FileAddRef ( const BAM_File *self );
rc_t BAM_FileRelease ( const BAM_File *self );


/* GetPosition
 *  get the position of the about-to-be read alignment
 *  this position can be stored
 *  this position can be passed into SetPosition to seek to the same alignment
 *
 *  "pos" [ OUT ] - return parameter for position
 */
rc_t BAM_FileGetPosition ( const BAM_File *self, BAM_FilePosition *pos );


/* GetProportionalPosition
 *  get the aproximate proportional position in the input file
 *  this is intended to be useful for computing progress
 *
 * NB - does not return rc_t
 */
float BAM_FileGetProportionalPosition ( const BAM_File *self );

    
/* Read
 *  read an aligment
 *
 *  "result" [ OUT ] - return param for BAM_Alignment object
 *   must be released with BAM_AlignmentRelease, is invalidated or contents
 *   change on next call to BAM_FileRead2. Unlike with BAM_FileRead, no attempt is
 *   made to preserve this object.
 *
 *  returns:
 *    RC(..., ..., ..., rcRow, rcNotFound) at end
 *    RC(..., ..., ..., rcRow, rcInvalid) and RC(..., ..., ..., rcRow, rcEmpty)
 *      are not fatal and are resumable
 *
 *  tries to use static buffers and will log messages about parsing errors
 */
rc_t BAM_FileRead2 ( const BAM_File *self, const BAM_Alignment **result );


/* GetRefSeqCount
 *  get the number of Reference Sequences refered to in the header
 *  this is not necessarily the number of Reference Sequences referenced
 *  by the alignments
 */
rc_t BAM_FileGetRefSeqCount ( const BAM_File *self, uint32_t *count );


/* GetRefSeq
 *  get the n'th Ref. Seq. where n is [0..RefSeqCount)
 *  the resulting pointer is static-like; it is freed when the BAM_File is.
 *  IOW, it is good for precisely at long as the BAM_File is.
 */
rc_t BAM_FileGetRefSeq ( const BAM_File *self, uint32_t n, const BAMRefSeq **result );


/* GetRefSeqById
 *  get a Ref. Seq. by its id
 *  the resulting pointer is static-like; it is freed when the BAM_File is.
 *  IOW, it is good for precisely at long as the BAM_File is.
 */
rc_t BAM_FileGetRefSeqById ( const BAM_File *self, int32_t id, const BAMRefSeq **result );


/* GetReadGroupCount
 *  get the number of Read Groups (accessions, etc.) refered to in the header
 *  this is not necessarily the number of Read Groups referenced
 *  by the alignments
 */
rc_t BAM_FileGetReadGroupCount ( const BAM_File *self, uint32_t *count );

/* GetReadGroup
 *  get the n'th Read Group where n is [0..ReadGroupCount)
 *  the resulting pointer is static-like; it is freed when the BAM_File is.
 *  IOW, it is good for precisely at long as the BAM_File is.
 */
rc_t BAM_FileGetReadGroup ( const BAM_File *self, unsigned n, const BAMReadGroup **result );
    
/* GetHeaderText
 *  get the text of the BAM header file
 *  the resulting pointer is static-like; it is freed when the BAM_File is.
 *  IOW, it is good for precisely at long as the BAM_File is.
 */
rc_t BAM_FileGetHeaderText(BAM_File const *cself, char const **header, size_t *header_len);
    

/* GetReadGroupByName
 *  get a Read Group by its name
 *  the resulting pointer is static-like; it is freed when the BAM_File is.
 *  IOW, it is good for precisely at long as the BAM_File is.
 */
rc_t BAM_FileGetReadGroupByName ( const BAM_File *self,
    const char *name, const BAMReadGroup **result );
