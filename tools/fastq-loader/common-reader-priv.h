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

#ifndef _h_common_reader_priv_
#define _h_common_reader_priv_

#include <loader/common-reader.h>

#include <klib/refcount.h>
#include <klib/text.h>
#include <klib/data-buffer.h>

#ifdef __cplusplus
extern "C" {
#endif

/*TODO: add module rcLoader to rc.h? */
#define RC_MODULE rcAlign

/*--------------------------------------------------------------------------
 ReaderFile
 */
#ifndef READERFILE_IMPL
    #define READERFILE_IMPL struct ReaderFile
#endif

typedef struct ReaderFile_vt_v1_struct {
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    rc_t ( *destroy   )                 ( READERFILE_IMPL* self );
    rc_t ( *getRecord )                 ( const READERFILE_IMPL *self, const Record** result );
    float ( *getProportionalPosition )  ( const READERFILE_IMPL *self );
    rc_t ( *getReferenceInfo )          ( const READERFILE_IMPL *self, const ReferenceInfo** result );
    /* end minor version == 0 */

} ReaderFile_vt_v1;

typedef union ReaderFile_vt {
    ReaderFile_vt_v1* v1;
} ReaderFile_vt;

struct ReaderFile
{
    ReaderFile_vt vt;
    KRefcount refcount;
    char* pathname;
};

/* Init
 *  polymorphic parent constructor
 */
rc_t CC ReaderFileInit ( READERFILE_IMPL *self );

/* Whack
 *  destructor
 */
rc_t CC ReaderFileWhack ( ReaderFile *self );

/*--------------------------------------------------------------------------
 Record
 */
#ifndef RECORD_IMPL
    #define RECORD_IMPL struct Record
#endif

typedef struct Record_vt_v1_struct {
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    rc_t ( *addRef  ) ( const RECORD_IMPL* self );
    rc_t ( *release ) ( const RECORD_IMPL* self );
    rc_t ( *getSequence  ) ( const RECORD_IMPL *self, const Sequence** result );
    rc_t ( *getAlignment ) ( const RECORD_IMPL *self, const Alignment** result );
    rc_t ( *getRejected  ) ( const RECORD_IMPL *self, const Rejected** result );
    /* end minor version == 0 */

} Record_vt_v1;

typedef union Record_vt {
    Record_vt_v1* v1;
} Record_vt;

struct Record
{
    Record_vt vt;
};

/*--------------------------------------------------------------------------
 Sequence
 */
#ifndef SEQUENCE_IMPL
    #define SEQUENCE_IMPL struct Sequence
#endif

typedef struct Sequence_vt_v1_struct {
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    rc_t ( *addRef  ) ( const SEQUENCE_IMPL* self );
    rc_t ( *release ) ( const SEQUENCE_IMPL* self );

    rc_t ( *getReadLength   )   ( const SEQUENCE_IMPL *self, uint32_t *length );
    rc_t ( *getRead         )   ( const SEQUENCE_IMPL *self, char *sequence );
    rc_t ( *getRead2        )   ( const SEQUENCE_IMPL *self, char *sequence, uint32_t start, uint32_t stop );
    rc_t ( *getQuality      )   ( const SEQUENCE_IMPL *self, const int8_t **quality, uint8_t *offset, int *qualType );
    rc_t ( *getSpotGroup    )   ( const SEQUENCE_IMPL *self, const char **name, size_t *length );
    rc_t ( *getSpotName     )   ( const SEQUENCE_IMPL *self, const char **name, size_t *length );
    bool ( *isColorSpace    )   ( const SEQUENCE_IMPL *self );
    rc_t ( *getCSKey        )   ( const SEQUENCE_IMPL *self, char cskey[1] );
    rc_t ( *getCSReadLength )   ( const SEQUENCE_IMPL *self, uint32_t *length );
    rc_t ( *getCSRead       )   ( const SEQUENCE_IMPL *self, char *sequence );
    rc_t ( *getCSQuality    )   ( const SEQUENCE_IMPL *self, const int8_t **quality, uint8_t *offset, int *qualType );
                          
    bool ( *wasPaired     )     ( const SEQUENCE_IMPL *self );
    int  ( *orientationSelf )   ( const SEQUENCE_IMPL *self );
    int  ( *orientationMate )   ( const SEQUENCE_IMPL *self );
    bool ( *isFirst       )     ( const SEQUENCE_IMPL *self );
    bool ( *isSecond      )     ( const SEQUENCE_IMPL *self );
    bool ( *isDuplicate   )     ( const SEQUENCE_IMPL *self ); 
    bool ( *isLowQuality   )    ( const SEQUENCE_IMPL *self ); 

    rc_t ( *getTI ) ( const SEQUENCE_IMPL *self, uint64_t *ti );
    
    /* end minor version == 0 */

} Sequence_vt_v1;

typedef union Sequence_vt {
    Sequence_vt_v1* v1;
} Sequence_vt;

struct Sequence
{
    Sequence_vt vt;
};

/*--------------------------------------------------------------------------
 Alignment
 */
#ifndef ALIGNMENT_IMPL
    #define ALIGNMENT_IMPL struct Alignment
#endif

typedef struct Alignment_vt_v1_struct {
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    
    rc_t ( *addRef  ) ( const ALIGNMENT_IMPL* self );
    rc_t ( *release ) ( const ALIGNMENT_IMPL* self );
    
    rc_t ( *getRefSeqId         )   ( const ALIGNMENT_IMPL *self, int32_t *refSeqId );
    rc_t ( *getMateRefSeqId     )   ( const ALIGNMENT_IMPL *self, int32_t *refSeqId );
    rc_t ( *getPosition         )   ( const ALIGNMENT_IMPL *self, int64_t *pos );
    rc_t ( *getMatePosition     )   ( const ALIGNMENT_IMPL *self, int64_t *pos );
    rc_t ( *getMapQuality       )   ( const ALIGNMENT_IMPL *self, uint8_t *qual );
    rc_t ( *getAlignmentDetail  )   ( const ALIGNMENT_IMPL *self, AlignmentDetail *rslt, uint32_t count, uint32_t *actual, int32_t *firstMatch, int32_t *lastMatch );
    rc_t ( *getAlignOpCount     )   ( const ALIGNMENT_IMPL *self, uint32_t *n );
    rc_t ( *getInsertSize       )   ( const ALIGNMENT_IMPL *self, int64_t *size );
    rc_t ( *getCG               )   ( const ALIGNMENT_IMPL *self, const CGData** result );
    rc_t ( *getBAMCigar         )   ( const ALIGNMENT_IMPL *self, uint32_t const **rslt, uint32_t *length );
    
    bool ( *isSecondary ) ( const ALIGNMENT_IMPL *self ); 
    
    /* end minor version == 0 */

} Alignment_vt_v1;

typedef union Alignment_vt {
    Alignment_vt_v1* v1;
} Alignment_vt;

struct Alignment
{
    Alignment_vt vt;
};

/*--------------------------------------------------------------------------
 CGData
 */
#ifndef CGDATA_IMPL
    #define CGDATA_IMPL struct CGData
#endif

typedef struct CGData_vt_v1_struct {
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    
    rc_t ( *addRef  ) ( const CGDATA_IMPL* self );
    rc_t ( *release ) ( const CGDATA_IMPL* self );

    rc_t ( * getSeqQual )       ( const CGDATA_IMPL* self, char sequence[/* 35 */], uint8_t quality[/* 35 */] );
    rc_t ( * getCigar )         ( const CGDATA_IMPL* self, uint32_t *cigar, uint32_t cig_max, uint32_t *cig_act );
    rc_t ( * getAlignGroup )    ( const CGDATA_IMPL* self, char buffer[], size_t max_size, size_t *act_size);
    /* end minor version == 0 */

} CGData_vt_v1;

typedef union CGData_vt {
    CGData_vt_v1* v1;
} CGData_vt;

struct CGData
{
    CGData_vt vt;
};

/*--------------------------------------------------------------------------
 Rejected
 */

struct Rejected {
    KRefcount   refcount;

    String      source;
    const char* message;
    uint64_t    line;
    uint64_t    column;
    bool        fatal;
};

rc_t CC RejectedInit ( Rejected *self );

/*--------------------------------------------------------------------------
 ReferenceInfo
 */

#ifndef REFERENCEINFO_IMPL
    #define REFERENCEINFO_IMPL struct ReferenceInfo
#endif

typedef struct ReferenceInfo_vt_v1_struct {
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    
    rc_t ( *addRef  ) ( const REFERENCEINFO_IMPL* self );
    rc_t ( *release ) ( const REFERENCEINFO_IMPL* self );

    rc_t ( *getRefSeqCount )        ( const REFERENCEINFO_IMPL* self, uint32_t* count );
    rc_t ( *getRefSeq )             ( const REFERENCEINFO_IMPL* self, uint32_t n, ReferenceSequence* result );
    rc_t ( *getReadGroupCount )     ( const REFERENCEINFO_IMPL* self, uint32_t* count );
    rc_t ( *getReadGroup )          ( const REFERENCEINFO_IMPL* self, unsigned n, ReadGroup* result );
    rc_t ( *getReadGroupByName )    ( const REFERENCEINFO_IMPL* self, const char* name, ReadGroup* result );

    /* end minor version == 0 */

} ReferenceInfo_vt_v1;

typedef union ReferenceInfo_vt {
    ReferenceInfo_vt_v1* v1;
} ReferenceInfo_vt;

struct ReferenceInfo
{
    ReferenceInfo_vt vt;
};

#ifdef __cplusplus
}
#endif

#endif /* _h_common_reader_priv_ */
