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

#include <unistd.h>

#include <klib/rc.h>

#include <insdc/sra.h>

#include "id2name.h"

struct SequenceWriter;
struct KLoadProgressbar;

/*--------------------------------------------------------------------------
 * ctx_value_t, FragmentInfo
 */
typedef struct {
/*    uint64_t spotId; */
    int64_t fragmentOffset;
    uint16_t fragmentSize;
    uint16_t seqHash[2];
    uint8_t  unmated: 1,
             has_a_read: 1,
             written: 1;
} ctx_value_t;

#define CTX_VALUE_SET_S_ID(A, B) ((void)(B))

typedef struct FragmentInfo
{
    uint32_t readlen;
    uint8_t  is_bad;
    uint8_t  orientation;
    uint8_t  otherReadNo;
    uint8_t  sglen;
    uint8_t  cskey;
} FragmentInfo;

typedef struct {
    int64_t id;
    FragmentInfo *data;
} FragmentEntry;

/*--------------------------------------------------------------------------
 * SpotAssembler
 */

#define FRAGMENT_HOT_COUNT (1024u * 1024u)
#define NUM_ID_SPACES (256u)

typedef struct SpotAssembler
{
    /* settings */
    size_t cache_size;
    const char * tmpfs;
    uint64_t pid;

    struct KBTree *key2id[NUM_ID_SPACES];
    char *key2id_names;

    struct MMArray *id2value;
    int64_t spotId;
    int64_t nextFragment;

    FragmentEntry * fragment; /* [FRAGMENT_HOT_COUNT] */

    Id2name id2name; /* idKey -> readname */

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

    int fragmentFd;
} SpotAssembler;

rc_t SpotAssemblerMake(SpotAssembler **ctx, size_t cache_size, const char * tmpfs, uint64_t pid);
void SpotAssemblerRelease(SpotAssembler * ctx);

ctx_value_t * SpotAssemblerGetCtxValue(SpotAssembler * self, rc_t *const prc, uint64_t const keyId);

FragmentEntry * SpotAssemblerGetFragmentEntry(SpotAssembler * self, uint64_t keyId);

rc_t SpotAssemblerGetKeyID(SpotAssembler *const ctx,
                           uint64_t *const rslt,
                           bool *const wasInserted,
                           char const key[],
                           char const name[],
                           size_t const o_namelen);

unsigned SeqHashKey(void const *const key, size_t const keylen);

void SpotAssemblerReleaseMemBank(SpotAssembler *ctx);

rc_t SpotAssemblerWriteSoloFragments(SpotAssembler * ctx,
                                     bool isColorSpace,
                                     INSDC_SRA_platform_id platform,
                                     bool keepMismatchQual,
                                     bool no_real_output,
                                     bool hasTI,
                                     const char * QualQuantizer,
                                     struct SequenceWriter * seq,
                                     const struct KLoadProgressbar *progress);

