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

#ifndef _h_common_writer_
#define _h_common_writer_

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifndef _h_insdc_sra_
#include <insdc/sra.h>
#endif

#ifndef _h_mmarray_
#include <loader/mmarray.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */

struct VDBManager;
struct VDatabase;
struct KMemBank;
struct KBTree;
struct KLoadProgressbar;
struct ReaderFile;
struct CommonWriter;
struct SequenceWriter;
struct AlignmentWriter;
struct Reference;

/*--------------------------------------------------------------------------
 * CommonWriterSettings
 */
enum LoaderModes {
    mode_Archive,
    mode_Analysis
};

typedef struct CommonWriterSettings
{
    uint64_t numfiles;
    char const *inpath;
    char const *outpath;
    char const *tmpfs;
    
    struct KFile *noMatchLog;
    
    char const *schemaPath;
    char const *schemaIncludePath;
    
    char const *refXRefPath;
    
    char const *QualQuantizer;
    
    char const *refFilter;

    char const** refFiles; /* NULL-terminated array pointing to argv */
    
    char const *headerText;
    
    uint64_t maxAlignCount;
    size_t cache_size;

    uint64_t errCount;
    uint64_t maxErrCount;
    uint64_t maxErrPct;
    uint64_t maxWarnCount_NoMatch;
    uint64_t maxWarnCount_DupConflict;
    uint64_t pid;
    uint64_t minMatchCount; /* minimum number of matches to count as an alignment */
    int minMapQual;
    enum LoaderModes mode;
    uint32_t maxSeqLen;
    bool omit_aligned_reads;
    bool omit_reference_reads;
    bool no_real_output;
    bool expectUnsorted;
    bool noVerifyReferences;
    bool onlyVerifyReferences;
    bool useQUAL;
    bool limit2config;
    bool editAlignedQual;
    bool keepMismatchQual;
    bool acceptBadDups; /* accept spots with inconsistent PCR duplicate flags */
    bool acceptNoMatch; /* accept without any matching bases */
    uint8_t alignedQualValue;
    bool allUnaligned; /* treat all records as unaligned */
    bool noColorSpace;
    bool noSecondary;
    bool hasTI;
    bool acceptHardClip;
    INSDC_SRA_platform_id platform;
    bool parseSpotName;
    bool compressQuality;
    uint64_t maxMateDistance;
} CommonWriterSettings;

/*--------------------------------------------------------------------------
 * SpotAssembler
 */

#define FRAG_CHUNK_SIZE (128)

typedef struct SpotAssembler {
    const struct KLoadProgressbar *progress[4];
    struct KBTree *key2id[NUM_ID_SPACES];
    char *key2id_names;
    struct MMArray *id2value;
    struct KMemBank *fragsBoth; /*** mate will be there soon ***/
    struct KMemBank *fragsOne;  /*** mate may not be found soon or even show up ***/
    int64_t spotId;
    int64_t primaryId;
    int64_t secondId;
    uint64_t alignCount;
    
    uint32_t idCount[NUM_ID_SPACES];
    uint32_t key2id_hash[NUM_ID_SPACES];
    
    size_t key2id_max;
    size_t key2id_name_max;
    size_t key2id_name_alloc;
    size_t key2id_count;
    
    size_t key2id_name[NUM_ID_SPACES];
    /* this array is kept in name order */
    /* this maps the names to key2id and idCount */
    size_t key2id_oid[NUM_ID_SPACES];
    
    unsigned pass;
    bool isColorSpace;
    
} SpotAssembler;

INSDC_SRA_platform_id PlatformToId(const char* name);

/*--------------------------------------------------------------------------
 * CommonWriter
 */
typedef struct CommonWriter {
    CommonWriterSettings settings;
    SpotAssembler ctx;
    struct Reference* ref;
    struct SequenceWriter* seq;
    struct AlignmentWriter* align;
    bool had_alignments;
    bool had_sequences;
    unsigned err_count;
    bool commit;
} CommonWriter;

rc_t CommonWriterInit(CommonWriter* self, struct VDBManager *mgr, struct VDatabase *db, const CommonWriterSettings* settings);

rc_t CommonWriterArchive(CommonWriter* self, const struct ReaderFile *);
rc_t CommonWriterComplete(CommonWriter* self, bool quitting, uint64_t maxDistance);

rc_t CommonWriterWhack(CommonWriter* self);

#ifdef __cplusplus
}
#endif

#endif /* _h_common_writer_ */
