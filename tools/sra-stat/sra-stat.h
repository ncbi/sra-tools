#ifndef _h_sra_stat_tools_
#define _h_sra_stat_tools_

/*==============================================================================
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

#include <klib/defs.h> /* rc_t */

struct KFile;
struct VCursor;
struct VTable;

typedef enum EMetaState {
    eMSNotFound,
    eMSFound
} EMetaState;

typedef struct QualityStats {
    struct Quality* QUALITY;
    size_t allocated;
    size_t used;
} QualityStats;

typedef struct TableCounts {
    EMetaState state;
    struct Counts* count;
    size_t allocated;
    size_t used;
} TableCounts;

typedef struct Ctx {
    const struct BSTree* tr;
    const struct MetaDataStats* meta_stats;
    const struct SraMeta* info;
    const struct SraSizeStats* sizes;
    const struct ArcInfo* arc_info;
    struct srastat_parms* pb;
    struct SraStatsTotal* total;

    const struct VDatabase * db;   /* sra-srat argument is a DB */
    const struct VTable    * tbl;  /* sra-srat argument is a table */

    const struct KMetadata* meta; /* from Table (when running on table) */
    QualityStats quality;
    TableCounts tables;

    uint64_t n;
    uint64_t l;
    uint64_t n50;
    uint64_t l50;
    uint64_t n90;
    uint64_t l90;
} Ctx;

rc_t CC VCursorColumnRead(const struct VCursor *self, int64_t id,
    uint32_t idx, const void **base, bitsz_t *offset, bitsz_t *size);

rc_t CC VTableMakeSingleFileArchive(const struct VTable *self,
    const struct KFile **sfa, bool lightweight);

rc_t CC CalculateNL ( const struct VDatabase * db, Ctx * ctx );

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)


#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))


#endif /* _h_sra_stat_tools_ */
