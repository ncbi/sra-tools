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

#include "sra-dbcc.vers.h"

#include <kapp/main.h>
#include <kapp/args.h>

#include <insdc/insdc.h>
#include <insdc/sra.h>

#include <sra/sradb.h>
#include <sra/sraschema.h>
#include <sra/srapath.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/meta.h>
#include <kdb/consistency-check.h>
#include <kdb/namelist.h>

#include <vfs/manager.h> /* VFSManager */
#include <vfs/resolver.h> /* VResolver */
#include <vfs/path.h> /* VPath */

#include <kfs/sra.h>
#include <kfs/tar.h>
#include <kfs/kfs-priv.h>

#include <klib/container.h>
#include <klib/sort.h>
#include <klib/namelist.h>
#include <klib/data-buffer.h>
#include <klib/text.h> /* String */
#include <klib/out.h>
#include <klib/log.h>
#include <klib/debug.h>
#include <klib/rc.h>

#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#ifndef PATH_MAX
    #define PATH_MAX 4096
#endif

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

static bool exhaustive;
static bool md5_required;
static bool ref_int_check;
static bool s_IndexOnly = false;

struct node_s {
    int parent;
    int prvSibl;
    int nxtSibl;
    int firstChild;
    unsigned depth;
    unsigned name;
    uint32_t objType;
};
typedef struct node_s node_t;

typedef struct cc_context_s {
    node_t *nodes;
    char *names;
    rc_t rc;
    unsigned num_columns;
    unsigned nextNode;
    unsigned nextName;
} cc_context_t;

static
rc_t report_rtn ( rc_t rc )
{
    return exhaustive ? 0 : rc;
}

static rc_t report_index(CCReportInfoBlock const *what, cc_context_t *ctx)
{
    switch (what->type) {
    case ccrpt_Done:
        if (what->info.done.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc, "Index '$(index)': $(mesg)",
                                    "index=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_Index:
        return 0; /* continue with check */
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc, "File '$(file)' of index '$(index)' failed MD5 check",
                                    "file=%s,index=%s", what->info.MD5.file, what->objName));
            if (ctx->rc == 0)
                ctx->rc = what->info.MD5.rc;
        }
        return report_rtn (what->info.MD5.rc);
    default:
        return RC(rcExe, rcTable, rcVisiting, rcParam, rcUnexpected);
    }
}

static rc_t report_column(CCReportInfoBlock const *what, cc_context_t *ctx)
{
    switch (what->type) {
    case ccrpt_Done:
        if (what->info.done.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc, "Column '$(column)': $(mesg)",
                                    "column=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        else {
            (void)PLOGMSG(klogInfo, (klogInfo, "Column '$(column)': $(mesg)", "column=%s,mesg=%s",
                                     what->objName, what->info.done.mesg ? what->info.done.mesg : "checked"));
            ++ctx->num_columns;
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_Blob:
        return 0; /* continue with check */
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc, "File '$(file)' of column '$(column)' failed MD5 check",
                                    "file=%s,column=%s", what->info.MD5.file, what->objName));
            if (ctx->rc == 0)
                ctx->rc = what->info.MD5.rc;
        }
        return report_rtn (what->info.MD5.rc);
    default:
        return RC(rcExe, rcTable, rcVisiting, rcParam, rcUnexpected);
    }
}

static rc_t report_table(CCReportInfoBlock const *what, cc_context_t *ctx)
{
    switch (what->type) {
    case ccrpt_Done:
        if (what->info.done.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc, "Table '$(table)': $(mesg)",
                                    "table=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        else if (what->info.done.mesg) {
            if (strcmp(what->info.done.mesg, "missing md5 file") == 0 && md5_required) {
                rc_t rc = RC(0, rcTable, rcValidating, rcChecksum, rcNotFound);
                (void)PLOGERR(klogErr, (klogErr, rc, "Table '$(table)': is missing required md5 files",
                                        "table=%s", what->objName));
                return rc;
            }
            else if (strncmp("unexpected object ", what->info.done.mesg, 18) == 0) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Table '$(tbl)': $(mesg)", "tbl=%s,mesg=%s",
                                         what->objName, what->info.done.mesg));
            }
            else {
                (void)PLOGMSG(klogInfo, (klogInfo, "Table '$(tbl)' metadata: $(mesg)",
                                         "tbl=%s,mesg=%s", what->objName, what->info.done.mesg));
            }
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc, "File '$(file)' of table '$(table)' failed MD5 check",
                                    "file=%s,table=%s", what->info.MD5.file, what->objName));
            if (ctx->rc == 0)
                ctx->rc = what->info.MD5.rc;
        }
        return report_rtn (what->info.MD5.rc);
    default:
        return RC(rcExe, rcTable, rcVisiting, rcParam, rcUnexpected);
    }
}

static rc_t report_database(CCReportInfoBlock const *what, cc_context_t *ctx)
{
    switch (what->type) {
    case ccrpt_Done:
        if (what->info.done.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc, "Database '$(db)': $(mesg)",
                                    "db=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        else if (what->info.done.mesg) {
            if (strcmp(what->info.done.mesg, "missing md5 file") == 0 && md5_required) {
                rc_t rc = RC(0, rcTable, rcValidating, rcChecksum, rcNotFound);
                (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(table)': is missing required md5 files",
                                        "table=%s", what->objName));
                return rc;
            }
            else {
                (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(db)' metadata: $(mesg)",
                                         "db=%s,mesg=%s", what->objName, what->info.done.mesg));
            }
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc, "File '$(file)' of database '$(db)' failed MD5 check",
                                    "file=%s,db=%s", what->info.MD5.file, what->objName));
            if (ctx->rc == 0)
                ctx->rc = what->info.MD5.rc;
        }
        return report_rtn (what->info.MD5.rc);
    default:
        return RC(rcExe, rcTable, rcVisiting, rcParam, rcUnexpected);
    }
}

static rc_t visiting(CCReportInfoBlock const *what, cc_context_t *ctx)
{
    unsigned const nn = ctx->nextNode++;
    node_t *const nxt = &ctx->nodes[nn];
    node_t *const cur = nxt - 1;
    
    nxt->parent = nxt->prvSibl = nxt->nxtSibl = nxt->firstChild = -1;
    nxt->depth = what->info.visit.depth;
    nxt->objType = what->objType;
    nxt->name = ctx->nextName;
    ctx->nextName += strlen(what->objName) + 1;
    strcpy(&ctx->names[nxt->name], what->objName);
    
    if (nn) {
        if (cur->depth == nxt->depth) {
            nxt->parent = cur->parent;
            nxt->prvSibl = nn - 1;
            cur->nxtSibl = nn;
        }
        else if (cur->depth < nxt->depth) {
            nxt->parent = nn - 1;
            cur->firstChild = nn;
        }
        else {
            unsigned sibling = cur->parent;
            
            while (ctx->nodes[sibling].depth > nxt->depth)
                sibling = ctx->nodes[sibling].parent;
            nxt->parent = ctx->nodes[sibling].parent;
            nxt->prvSibl = sibling;
            ctx->nodes[sibling].nxtSibl = nn;
        }
    }
    return 0;
}

static rc_t CC report(CCReportInfoBlock const *what, void *Ctx)
{
    cc_context_t *ctx = Ctx;
    rc_t rc = Quitting();
    
    if (rc)
        return rc;
    
    if (what->type == ccrpt_Visit)
        return visiting(what, ctx);

    switch (what->objType) {
    case kptDatabase:
        return report_database(what, ctx);
    case kptTable:
        return report_table(what, ctx);
    case kptColumn:
        return report_column(what, ctx);
    case kptIndex:
        return report_index(what, ctx);
    default:
        return RC(rcExe, rcTable, rcVisiting, rcParam, rcUnexpected);
    }
}

static
rc_t kdbcc(const KDirectory *dir, char const name[], uint32_t mode, bool *is_db,
    bool is_file, node_t nodes[], char names[], INSDC_SRA_platform_id platform)
{
    const KDBManager *mgr;
    rc_t rc = KDBManagerMakeRead ( & mgr, NULL );

    if ( rc == 0 )
    {
        cc_context_t ctx;
        char const *objtype;

        uint32_t level = ( mode & 4 ) ? 3 : ( mode & 2 ) ? 1 : 0;
        if (s_IndexOnly) {
            level |= CC_INDEX_ONLY;
        }

        memset(&ctx, 0, sizeof(ctx));
        ctx.nodes = &nodes[0];
        ctx.names = &names[0];

        * is_db = KDBManagerExists ( mgr, kptDatabase, "%s", name );
        if ( * is_db )
        {
            const KDatabase *db;
            
            objtype = "database";
            rc = KDBManagerOpenDBRead ( mgr, & db, "%s", name );
            if ( rc == 0 )
            {
                rc = KDatabaseConsistencyCheck ( db, 0, level, report, & ctx );
                if ( rc == 0 ) {
                    rc = ctx.rc;
                    if (s_IndexOnly)
                    {   (void)LOGMSG(klogInfo, "Indices: checked"); }
                }
                KDatabaseRelease ( db );
            }
        }
        else
        {
            const KTable *tbl;

            objtype = "table";
            rc = KDBManagerOpenTableRead ( mgr, & tbl, "%s", name );
            if ( rc == 0 )
            {
                rc = KTableConsistencyCheck ( tbl, 0, level, report, & ctx,
                    platform );
                if ( rc == 0 ) {
                    rc = ctx.rc;
                }
                if ( rc == 0 && s_IndexOnly ) {
                    (void)LOGMSG(klogInfo, "Index: checked");
                }
                KTableRelease ( tbl );
            }
        }
        if (rc == 0 && ctx.num_columns == 0 && !s_IndexOnly) {
            if (is_file) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Nothing to validate; the file '$(file)' has no checksums or is truncated.", "file=%s", name));
            }
            else {
                (void)PLOGMSG(klogWarn, (klogWarn, "Nothing to validate; the $(type) '$(file)' has no checksums or is empty.", "type=%s,file=%s", objtype, name));
            }
        }
        
        KDBManagerRelease ( mgr );
    }
    return rc;
}

static
rc_t vdbcc(const KDirectory *dir, char const name[], uint32_t mode, bool is_db, bool is_file)
{
#if 0
    if (mode & 8) {
        const VDBManager *mgr;
        rc_t rc = VDBManagerMakeRead(&mgr, dir);
        
        if (rc == 0) {
            const VTable *tbl;
            
            rc = VDBManagerOpenTableRead(mgr, &tbl, NULL, "%s", name);
            if (rc == 0)
                rc = VTableConsistencyCheck(tbl, 2);
        }
        return rc;
    }
#endif
    return 0;
}

typedef struct ColumnInfo_s {
    char const *name;
    union {
        void     const *vp;
        char     const *string;
        bool     const *tf;
        int8_t   const *i8;
        uint8_t  const *u8;
        int16_t  const *i16;
        uint16_t const *u16;
        int32_t  const *i32;
        uint32_t const *u32;
        int64_t  const *i64;
        uint64_t const *u64;
        float    const *f32;
        double   const *f64;
    } value;
    uint32_t idx;
    uint32_t elem_bits;
    uint32_t elem_count;
} ColumnInfo;

static rc_t CC get_sizes_cb(const KDirectory *dir, uint32_t type, const char *name, void *data)
{
    struct {
        unsigned count;
        size_t size;
    } *pb = data;

    ++pb->count;    
    pb->size += strlen(name) + 1;
    
    return 0;
}

static rc_t get_sizes(KDirectory const *dir, unsigned *nobj, size_t *namesz)
{
    rc_t rc;
    struct {
        unsigned count;
        size_t size;
    } pb;
    
    memset(&pb, 0, sizeof(pb));
    rc = KDirectoryVVisit(dir, true, get_sizes_cb, &pb, NULL, NULL);
    if (rc)
        memset(&pb, 0, sizeof(pb));
    *nobj = pb.count;
    *namesz = pb.size;
    return rc;
}

static rc_t init_dbcc(KDirectory const *dir, char const name[], bool is_file, node_t **nodes, char **names)
{
    KDirectory const *obj;
    unsigned nobj;
    size_t namesz;
    rc_t rc;
    
    if (is_file) {
        rc = KDirectoryOpenSraArchiveRead_silent(dir, &obj, false, "%s", name);
        if (rc)
            rc = KDirectoryOpenTarArchiveRead_silent(dir, &obj, false, "%s", name);
    }
    else {
        rc = KDirectoryOpenDirRead(dir, &obj, false, "%s", name);
    }
    if (rc)
        return rc;
    rc = get_sizes(obj, &nobj, &namesz);
    KDirectoryRelease(obj);
    if (rc) {
        *nodes = NULL;
        *names = NULL;
    }
    else {
        *nodes = malloc(nobj * sizeof(**nodes) + namesz);
        if (nodes)
            *names = (char *)&(*nodes)[nobj];
        else
            rc = RC(rcExe, rcSelf, rcConstructing, rcMemory, rcExhausted);
    }
    return rc;
}

static rc_t get_schema_info(KMetadata const *meta, char buffer[], size_t bsz, char **vers)
{
    KMDataNode const *node;
    rc_t rc = KMetadataOpenNodeRead(meta, &node, "schema");
    
    if (rc == 0) {
        size_t sz;
        
        rc = KMDataNodeReadAttr(node, "name", buffer, bsz, &sz);
        if (rc == 0) {
            buffer[sz] = '\0';
            *vers = &buffer[sz];
            while (sz) {
                --sz;
                if (buffer[sz] == '#') {
                    buffer[sz] = '\0';
                    *vers = &buffer[sz + 1];
                    break;
                }
            }
        }
    }
    return rc;
}

static rc_t get_tbl_schema_info(VTable const *tbl, char buffer[], size_t bsz, char **vers)
{
    KMetadata const *meta;
    rc_t rc = VTableOpenMetadataRead(tbl, &meta);
    
    *(*vers = &buffer[0]) = '\0';
    if (rc == 0) rc = get_schema_info(meta, buffer, bsz, vers);
    return 0;
}

static rc_t get_db_schema_info(VDatabase const *db, char buffer[], size_t bsz, char **vers)
{
    KMetadata const *meta;
    rc_t rc = VDatabaseOpenMetadataRead(db, &meta);
    
    *(*vers = &buffer[0]) = '\0';
    if (rc == 0) rc = get_schema_info(meta, buffer, bsz, vers);
    return rc;
}

static rc_t sra_dbcc_454(VTable const *tbl, char const name[])
{
    /* TODO: complete this */
    return 0;
}

static rc_t sra_dbcc_fastq(VTable const *tbl, char const name[])
{
    static char const *const cn_FastQ[] = {
        "READ", "QUALITY", "SPOT_LEN", "READ_START", "READ_LEN", "READ_TYPE"
    };
    
    VCursor const *curs;
    rc_t rc = VTableCreateCursorRead(tbl, &curs);
    
    if (rc == 0) {
        unsigned const n = sizeof(cn_FastQ)/sizeof(cn_FastQ[0]);
        ColumnInfo cols[sizeof(cn_FastQ)/sizeof(cn_FastQ[0])];
        unsigned i;

        memset(cols, 0, sizeof(cols));
        for (i = 0; i < n; ++i) {
            cols[i].name = cn_FastQ[i];
            VCursorAddColumn(curs, &cols[i].idx, "%s", cols[i].name);
        }
        rc = VCursorOpen(curs);
        if (rc == 0) {
            rc = VCursorOpenRow(curs);
            for (i = 0; i < n && rc == 0; ++i) {
                VCursorCellData(curs, cols[i].idx, &cols[i].elem_bits, &cols[i].value.vp, NULL, &cols[i].elem_count);
            }
            if (   cols[0].idx == 0 || cols[0].elem_bits == 0 || cols[0].value.vp == NULL
                || cols[1].idx == 0 || cols[1].elem_bits == 0 || cols[1].value.vp == NULL)
            {
                rc = RC(rcExe, rcTable, rcValidating, rcColumn, rcNotFound);
            }
            else if (cols[2].idx == 0 || cols[2].elem_bits == 0 || cols[2].value.vp == NULL) {
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcExe, rcTable, rcValidating, rcColumn, rcNotFound),
                                         "Table '$(name)' is usable for fasta only; no quality data", "name=%s", name));
            }
        }
        VCursorRelease(curs);
    }
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Table '$(name)' is damaged beyond any use", "name=%s", name));
    }
    return rc;
}

static rc_t VTable_get_platform(VTable const *tbl, INSDC_SRA_platform_id *rslt)
{
    rc_t rc;
    VCursor const *curs;
    INSDC_SRA_platform_id platform = -1;
    
    rc = VTableCreateCursorRead(tbl, &curs);
    if (rc == 0) {
        uint32_t cid;
        
        rc = VCursorAddColumn(curs, &cid, "("sra_platform_id_t")PLATFORM");
        if (rc == 0) {
            rc = VCursorOpen(curs);
            if (rc == 0) {
                uint32_t ebits;
                void const *data;
                uint32_t boff;
                uint32_t ecnt;
                
                rc = VCursorCellDataDirect(curs, 1, cid, &ebits, &data, &boff, &ecnt);
                if (rc == 0) {
                    if (ebits == sizeof(platform) * 8 && boff == 0 && ecnt == 1)
                        platform = ((INSDC_SRA_platform_id *)data)[0];
                    else
                        rc = RC(rcExe, rcTable, rcReading, rcType, rcUnexpected);
                }
            }
        }
        else
            rc = 0;
        VCursorRelease(curs);
    }
    rslt[0] = platform;
    return rc;
}

static rc_t verify_table(VTable const *tbl, char const name[])
{
    char schemaName[1024];
    char *schemaVers = NULL;
    rc_t rc = 0;
    
    get_tbl_schema_info(tbl, schemaName, sizeof(schemaName), &schemaVers);
    
    if (schemaName[0] == '\0' || strncmp(schemaName, "NCBI:SRA:", 9) == 0) {
        /* SRA or legacy SRA */
        INSDC_SRA_platform_id platform;
        
        rc = VTable_get_platform(tbl, &platform);
        if (rc == 0) {
            if (platform == (INSDC_SRA_platform_id)-1) {
                (void)PLOGMSG(klogWarn, (klogWarn, "Couldn't determine SRA Platform; type of table '$(name)' is indeterminate.", "name=%s", name));
            }
            rc = sra_dbcc_fastq(tbl, name);
            if (rc == 0 && platform == SRA_PLATFORM_454) {
                rc = sra_dbcc_454(tbl, name);
            }
        }
        else {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to read table '$(name)'", "name=%s", name));
        }
    }
    return rc;
}

static rc_t verify_mgr_table(VDBManager const *mgr, char const name[])
{
    VTable const *tbl;
    VSchema *sra_schema = NULL;
    
    for ( ; ; ) {
        rc_t rc = VDBManagerOpenTableRead(mgr, &tbl, sra_schema, "%s", name);
        VSchemaRelease(sra_schema);
        if (rc == 0) {
            rc = verify_table(tbl, name);
            VTableRelease(tbl);
            return rc;
        }
        else if (GetRCState(rc) == rcNotFound && GetRCObject(rc) == rcSchema && sra_schema == NULL) {
            rc = VDBManagerMakeSRASchema(mgr, &sra_schema);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open table '$(name)'", "name=%s", name));
                return rc;
            }
        }
        else {
            (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open table '$(name)'", "name=%s", name));
            return rc;
        }
    }
}

#if 0
static rc_t verify_db_table(VDatabase const *db, char const name[])
{
    VTable const *tbl;
    rc_t rc = VDatabaseOpenTableRead(db, &tbl, "%s", name);
    
    if (rc == 0) {
        rc = verify_table(tbl, name);
        VTableRelease(tbl);
    }
    else {
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to open table '$(name)'", "name=%s", name));
    }
    return rc;
}

static rc_t align_dbcc_primary_alignment(VTable const *tbl, char const name[])
{
    VCursor const *curs;
    rc_t rc = VTableCreateCursorRead(tbl, &curs);
    
    if (rc == 0) {
        static char const *const cn_SAM[] = {
            "SEQ_NAME",
            "SAM_FLAGS",
            "REF_NAME",
            "REF_POS",
            "MAPQ",
            "HAS_MISMATCH",
            "HAS_REF_OFFSET",
            "REF_OFFSET",
            "MATE_REF_NAME",
            "MATE_REF_POS",
            "TEMPLATE_LEN",
            "READ",
            "QUALITY"
        };
        unsigned const n = sizeof(cn_SAM)/sizeof(cn_SAM[0]);
        ColumnInfo cols[sizeof(cn_SAM)/sizeof(cn_SAM[0])];
        unsigned i;
        
        memset(cols, 0, sizeof(cols));
        for (i = 0; i < n; ++i) {
            cols[i].name = cn_SAM[i];
            VCursorAddColumn(curs, &cols[i].idx, "%s", cols[i].name);
        }
        rc = VCursorOpen(curs);
        if (rc == 0) {
            rc = VCursorOpenRow(curs);
            for (i = 0; i < n && rc == 0; ++i) {
                VCursorCellData(curs, cols[i].idx, &cols[i].elem_bits, &cols[i].value.vp, NULL, &cols[i].elem_count);
            }
            for (i = 0; i < n && rc == 0; ++i) {
                if (cols[i].idx == 0 || cols[i].elem_bits == 0) {
                    (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(name)' could not be used to generate SAM", "name=%s", name));
                    break;
                }
            }
        }
        VCursorRelease(curs);
    }
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)' is damaged beyond any use", "name=%s", name));
    }
    return rc;
}
#endif

typedef struct id_pair_s {
    int64_t first;
    int64_t second;
} id_pair_t;

static int CC id_pair_cmp(void const *A, void const *B, void *ignored)
{
    id_pair_t const *a = A;
    id_pair_t const *b = B;
    
    return a->first < b->first ? -1 : a->first == b->first ? a->second < b->second ? -1 : a->second == b->second ? 0 : 1 : 1;
}

static rc_t ric_align_ref_and_align(char const dbname[],
                                    VTable const *ref,
                                    VTable const *align,
                                    int which)
{
    char const *const id_col_name = which == 0 ? "PRIMARY_ALIGNMENT_IDS"
                                  : which == 1 ? "SECONDARY_ALIGNMENT_IDS"
                                  : which == 2 ? "EVIDENCE_ALIGNMENT_IDS"
                                  : NULL;
    rc_t rc;
    VCursor const *curs = NULL;
    ColumnInfo ci;
    int64_t startId;
    uint64_t count;
    
    rc = VTableCreateCursorRead(align, &curs);
    if (rc == 0) {
        rc = VCursorAddColumn(curs, &ci.idx, "REF_ID");
        if (rc == 0)
            rc = VCursorOpen(curs);
        if (rc == 0)
            rc = VCursorIdRange(curs, ci.idx, &startId, &count);
    }
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': alignment table can not be read", "name=%s", dbname));
    }
    else {
        id_pair_t *const id_pair = malloc(sizeof(id_pair_t) * count);
        
        if (id_pair) {
            uint64_t i;
            uint64_t j;
            
            for (j = i = 0; i < count && rc == 0; ++i) {
                int64_t const row = startId + i;
                
                rc = VCursorCellDataDirect(curs, row, ci.idx, &ci.elem_bits, &ci.value.vp, NULL, &ci.elem_count);
                if (rc == 0) {
                    if (ci.elem_count != 1) {
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcUnexpected);
                        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': failed referential integrity check", "name=%s", dbname));
                        break;
                    }
                    else {
                        id_pair[j].second = row;
                        id_pair[j].first = ci.value.i64[0];
                        ++j;
                    }
                }
                else if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                    rc = 0;
            }
            VCursorRelease(curs); curs = NULL;
            if (rc == 0) {
                bool ooo_warned = false;
                bool failed = false;
                
                ksort(id_pair, count, sizeof(id_pair_t), id_pair_cmp, NULL);
                
                rc = VTableCreateCursorRead(ref, &curs);
                if (rc == 0)
                    rc = VCursorAddColumn(curs, &ci.idx, "%s", id_col_name);
                if (rc == 0)
                    rc = VCursorOpen(curs);
                if (rc == 0)
                    rc = VCursorIdRange(curs, ci.idx, &startId, &count);
                if (rc == 0) {
                    for (i = j = 0; rc == 0 && i < count; ++i) {
                        int64_t const row = startId + i;
                        
                        rc = VCursorCellDataDirect(curs, row, ci.idx, &ci.elem_bits, &ci.value.vp, NULL, &ci.elem_count);
                        if (rc == 0) {
                            unsigned k;
                            int64_t prvId = ci.value.i64[0];
                            
                            for (k = 0; rc == 0 && k < ci.elem_count; ++k, ++j) {
                                int64_t const alignId = ci.value.i64[k];
                                
                                if (!ooo_warned && prvId > alignId) {
                                    (void)PLOGMSG(klogWarn, (klogWarn, "Database '$(name)': column '$(idcol)' is not ordered", "name=%s,idcol=%s", dbname, id_col_name));
                                    ooo_warned = true;
                                }
                                if (id_pair[j].first != row) {
                                    if (!failed) {
                                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                                        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': column '$(idcol)' failed referential integrity check", "name=%s,idcol=%s", dbname, id_col_name));
                                    }
                                    failed = true;
                                }
                                else if (id_pair[j].second != alignId) {
                                    if (!ooo_warned) {
                                        (void)PLOGMSG(klogWarn, (klogWarn, "Database '$(name)': column '$(idcol)' might fail referential integrity check", "name=%s,idcol=%s", dbname, id_col_name));
                                    }
                                }
                                prvId = alignId;
                            }
                        }
                        else if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                            rc = 0;
                    }
                    if (!failed && i < count) {
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': column '$(idcol)' failed referential integrity check", "name=%s,idcol=%s", dbname, id_col_name));
                    }
                }
                else
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': reference table can not be read", "name=%s", dbname));
            }
            free(id_pair);
            VCursorRelease(curs);
        }
        else {
            rc = RC(rcExe, rcDatabase, rcValidating, rcMemory, rcExhausted);
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': referential integrity could not be checked", "name=%s", dbname));
        }
    }
    return rc;
}

static rc_t ric_align_seq_and_pri(char const dbname[],
                                  VTable const *seq,
                                  VTable const *pri)
{
    rc_t rc;
    VCursor const *curs = NULL;
    ColumnInfo ci;
    int64_t startId;
    uint64_t count;
    
    rc = VTableCreateCursorRead(pri, &curs);
    if (rc == 0) {
        rc = VCursorAddColumn(curs, &ci.idx, "SEQ_SPOT_ID");
        if (rc == 0)
            rc = VCursorOpen(curs);
        if (rc == 0)
            rc = VCursorIdRange(curs, ci.idx, &startId, &count);
    }
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': alignment table can not be read", "name=%s", dbname));
    }
    else {
        id_pair_t *const id_pair = malloc(sizeof(id_pair_t) * count);

        if (id_pair) {
            uint64_t i;
            uint64_t j;
            
            for (j = i = 0; i < count && rc == 0; ++i) {
                int64_t const row = startId + i;
                
                rc = VCursorCellDataDirect(curs, row, ci.idx, &ci.elem_bits, &ci.value.vp, NULL, &ci.elem_count);
                if (rc == 0) {
                    if (ci.elem_count != 1) {
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcUnexpected);
                        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': failed referential integrity check", "name=%s", dbname));
                        break;
                    }
                    else {
                        id_pair[j].second = row;
                        id_pair[j].first = ci.value.i64[0];
                        ++j;
                    }
                }
                else if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)
                    rc = 0;
            }
            VCursorRelease(curs); curs = NULL;
            if (rc == 0) {
                ksort(id_pair, count, sizeof(id_pair_t), id_pair_cmp, NULL);
                
                rc = VTableCreateCursorRead(seq, &curs);
                if (rc == 0)
                    rc = VCursorAddColumn(curs, &ci.idx, "PRIMARY_ALIGNMENT_ID");
                if (rc == 0)
                    rc = VCursorOpen(curs);
                if (rc == 0) {
                    for (i = 0; rc == 0 && i < count; ++i) {
                        int64_t const row = id_pair[i].first;
                        int64_t const alignId = id_pair[i].second;
                        
                        rc = VCursorCellDataDirect(curs, row, ci.idx, &ci.elem_bits, &ci.value.vp, NULL, &ci.elem_count);
                        if (rc == 0) {
                            bool found = false;
                            
                            for (j = 0; j < ci.elem_count; ++j)
                                found |= (alignId == ci.value.i64[j]);
                            
                            if (!found) {
                                rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                                (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': column 'SEQ_SPOT_ID' failed referential integrity check", "name=%s", dbname));
                            }
                        }
                        else if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound) {
                            rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': column 'SEQ_SPOT_ID' failed referential integrity check", "name=%s", dbname));
                        }
                        else
                            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': sequence table can not be read", "name=%s", dbname));
                    }
                }
                else
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': sequence table can not be read", "name=%s", dbname));
            }
            free(id_pair);
        }
        else {
            rc = RC(rcExe, rcDatabase, rcValidating, rcMemory, rcExhausted);
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': referential integrity could not be checked", "name=%s", dbname));
        }
        VCursorRelease(curs);
    }
    return rc;
}

/* database referential integrity check for alignment database */
static rc_t dbric_align(char const dbname[],
                        VTable const *pri,
                        VTable const *seq,
                        VTable const *ref)
{
    rc_t rc = 0;
    
    if ((rc == 0 || exhaustive) && (pri != NULL && seq != NULL)) {
        rc_t rc2 = ric_align_seq_and_pri(dbname, seq, pri);
        
        if (rc2 == 0) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(dbname)': SEQUENCE.PRIMARY_ALIGNMENT_ID <-> PRIMARY_ALIGNMENT.SEQ_SPOT_ID referential integrity ok", "dbname=%s", dbname));
        }
        if (rc == 0) {
            rc = rc2;
        }
    }
    if ((rc == 0 || exhaustive) && (pri != NULL && ref != NULL)) {
        rc_t rc2 = ric_align_ref_and_align(dbname, ref, pri, 0);
        
        if (rc2 == 0) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(dbname)': REFERENCE.PRIMARY_ALIGNMENT_IDS <-> PRIMARY_ALIGNMENT.REF_ID referential integrity ok", "dbname=%s", dbname));
        }
        if (rc == 0) {
            rc = rc2;
        }
    }
    return rc;
}
                        

static rc_t verify_database_align(VDatabase const *db, char const name[], node_t const nodes[], char const names[])
{
    rc_t rc = 0;
    unsigned tables = 0;
    node_t const *tbl = &nodes[nodes[0].firstChild];
    enum table_bits {
        tbEvidenceInterval   = ( 1u << 0 ),
        tbEvidenceAlignment  = ( 1u << 1 ),
        tbPrimaryAlignment   = ( 1u << 2 ),
        tbReference          = ( 1u << 3 ),
        tbSequence           = ( 1u << 4 ),
        tbSecondaryAlignment = ( 1u << 5 )
    };
    
    if (nodes[0].firstChild) {
        for ( ; ; ) {
            char const *tname = &names[tbl->name];
            unsigned this_table = 0;
            
            if (tbl->objType == kptTable) {
                switch (tname[0]) {
                case 'E':
                    if (strcmp(tname, "EVIDENCE_INTERVAL") == 0)
                        this_table |= tbEvidenceInterval;
                    else if (strcmp(tname, "EVIDENCE_ALIGNMENT") == 0)
                        this_table |= tbEvidenceAlignment;
                    break;
                case 'P':
                    if (strcmp(tname, "PRIMARY_ALIGNMENT") == 0)
                        this_table |= tbPrimaryAlignment;
                    break;
                case 'R':
                    if (strcmp(tname, "REFERENCE") == 0)
                        this_table |= tbReference;
                    break;
                case 'S':
                    if (strcmp(tname, "SEQUENCE") == 0)
                        this_table |= tbSequence;
                    else if (strcmp(tname, "SECONDARY_ALIGNMENT") == 0)
                        this_table |= tbSecondaryAlignment;
                    break;
                }
                if (this_table == 0) {
                    (void)PLOGERR(klogWarn, (klogWarn, RC(rcExe, rcDatabase, rcValidating, rcTable, rcUnexpected),
                                             "Database '$(name)' contains unexpected table '$(table)'",
                                             "name=%s,table=%s", name, tname));
                }
                tables |= this_table;
            }
            else {
                (void)PLOGERR(klogWarn, (klogWarn, RC(rcExe, rcDatabase, rcValidating, rcType, rcUnexpected),
                                         "Database '$(name)' contains unexpected object '$(obj)'",
                                         "name=%s,obj=%s", name, tname));
            }
            if (tbl->nxtSibl > 0)
                tbl = &nodes[tbl->nxtSibl];
            else
                break;
        }
    }
    if (tables == tbSequence) {
        /* sequence data only */
        (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(name)' contains only unaligned reads", "name=%s", name));
    }
    else if (   (tables & tbReference) == 0
             || (tables & tbPrimaryAlignment) == 0)
    {
        /* missing reference or primary alignment */
        (void)PLOGERR(klogWarn, (klogWarn, rc = RC(rcExe, rcDatabase, rcValidating, rcDatabase, rcIncomplete),
                                 "Database '$(name)' does not contain all required tables",
                                 "name=%s", name));
    }
    else if (   ((tables & tbEvidenceAlignment) != 0)
             != ((tables & tbEvidenceInterval ) != 0))
    {
        /* both must be present or both must be absent */
        (void)PLOGERR(klogWarn, (klogWarn, rc = RC(rcExe, rcDatabase, rcValidating, rcDatabase, rcIncomplete),
                                 "Database '$(name)' does not contain all required tables",
                                 "name=%s", name));
    }
    while (ref_int_check) {
        VTable const *pri = NULL;
        VTable const *seq = NULL;
        VTable const *ref = NULL;
        
        if ((tables & tbPrimaryAlignment) != 0) {
            rc = VDatabaseOpenTableRead(db, &pri, "PRIMARY_ALIGNMENT");
            if (rc) break;
        }
        if ((tables & tbSequence) != 0) {
            rc = VDatabaseOpenTableRead(db, &seq, "SEQUENCE");
            if (rc) break;
        }
        if ((tables & tbReference) != 0) {
            rc = VDatabaseOpenTableRead(db, &ref, "REFERENCE");
            if (rc) break;
        }
        rc = dbric_align(name, pri, seq, ref);
        break;
    }
    return rc;    
}

static rc_t verify_database(VDatabase const *db, char const name[], node_t const nodes[], char const names[])
{
    char schemaName[1024];
    char *schemaVers = NULL;
    rc_t rc;
    
    rc = get_db_schema_info(db, schemaName, sizeof(schemaName), &schemaVers);
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc, "Failed to find database schema for '$(name)'", "name=%s", name));
    }
    else if (strncmp(schemaName, "NCBI:var:db:", 12) == 0) {
        /* TODO: verify NCBI:var:db:* */
    }
    else if (strncmp(schemaName, "NCBI:WGS:db:", 12) == 0) {
        /* TODO: verify NCBI:WGS:db:* */
    }
    else if (strncmp(schemaName, "NCBI:align:db:", 14) == 0) {
        rc = verify_database_align(db, name, nodes, names);
    }
    else if (strcmp(schemaName, "NCBI:SRA:PacBio:smrt:db") == 0) {
        /* TODO: verify NCBI:SRA:PacBio:smrt:db */
    }
    else {
        (void)PLOGERR(klogWarn, (klogWarn, RC(rcExe, rcDatabase, rcValidating, rcType, rcUnrecognized),
                                 "Database '$(name)' has unrecognized type '$(type)'",
                                 "name=%s,type=%s", name, schemaName));
    }
    return rc;
}

static rc_t verify_mgr_database(VDBManager const *mgr, char const name[], node_t const nodes[], char const names[])
{
    VDatabase const *child;
    rc_t rc = VDBManagerOpenDBRead(mgr, &child, NULL, "%s", name);
    
    if (rc == 0) {
        rc = verify_database(child, name, nodes, names);
        VDatabaseRelease(child);
    }
    return rc;
}

static rc_t sra_dbcc(KDirectory const *dir, char const name[], node_t const nodes[], char const names[])
{
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead(&mgr, NULL);
    
    if (rc) return rc;
    
    if (nodes[0].objType == kptDatabase)
        rc = verify_mgr_database(mgr, name, nodes, names);
    else
        rc = verify_mgr_table(mgr, name);
    VDBManagerRelease(mgr);
    
    return rc;
}

static
rc_t get_platform(const KDirectory *dir, const VDBManager *aMgr,
    const VTable *aTbl, char const name[], INSDC_SRA_platform_id *platform)
{
    rc_t rc = 0;
    const VDBManager *mgr = aMgr;
    const VTable *tbl = aTbl;
    assert(name && platform);
    if (mgr == NULL) {
        rc = VDBManagerMakeRead(&mgr, NULL);
        if (rc != 0) {
            return rc;
        }
    }
    if (tbl == NULL) {
        VSchema *sra_schema = NULL;
        for ( ; rc == 0; ) {
            rc = VDBManagerOpenTableRead(mgr, &tbl, sra_schema, "%s", name);
            VSchemaRelease(sra_schema);
            if (rc == 0) {
                rc = VTable_get_platform(tbl, platform);
                break;
            }
            else if (GetRCState(rc) == rcNotFound && GetRCObject(rc) == rcSchema
                && sra_schema == NULL)
            {
                rc = VDBManagerMakeSRASchema(mgr, &sra_schema);
            }
        }
    }
    if (aTbl == NULL) {
        VTableRelease(tbl);
    }
    if (aMgr == NULL) {
        VDBManagerRelease(mgr);
    }
    return rc;
}

static
rc_t dbcc(const char *src_path, const KDirectory *dir,
    char const name[], uint32_t mode, bool *is_db, bool is_file)
{
    rc_t rc;
    node_t *nodes = NULL;
    char *names;

    assert(src_path);

    rc = init_dbcc(dir, name, is_file, &nodes, &names);
    if (rc == 0) {
        INSDC_SRA_platform_id platform = SRA_PLATFORM_UNDEFINED;
        get_platform(dir, NULL, NULL, src_path, &platform);
        rc = kdbcc(dir, src_path, mode, is_db, is_file, nodes, names, platform);
        if ( rc == 0 )
            rc = vdbcc(dir, src_path, mode, * is_db, is_file);
    }
    if (rc == 0)
        rc = sra_dbcc(dir, src_path, nodes, names);

    free(nodes);

    return rc;
}

static char const* const defaultLogLevel = 
#if _DEBUGGING
"debug5";
#else
"info";
#endif

/******************************************************************************
 * Usage
 ******************************************************************************/
const char UsageDefaultName[] = "sra-dbcc";

rc_t CC UsageSummary(const char *prog_name)
{
    return KOutMsg("Usage: %s [options] table\n\n", prog_name);
}

char const *help_text[] = 
{
    "Check components md5s if present, fail unless other checks are requested (default: yes)", NULL,
    "Check blobs CRC32 (default: no)", NULL,
    "Check 'skey' index (default: no)", NULL,
    "Continue checking table for all possible errors (default: no)", NULL,
    "Check data referential integrity for databases (default: no)", NULL,
    "Check index-only with blobs CRC32 (default: no)", NULL
};

OptDef Options[] = 
{
    { "md5"       , "5", NULL, &help_text[0], 1, false, false },
    { "blob-crc"  , "b", NULL, &help_text[2], 1, false, false },
#if CHECK_INDEX
    { "index"     , "i", NULL, &help_text[4], 1, false, false },
#endif
    { "exhaustive", "x", NULL, &help_text[6], 1, false, false },
    { "dri"       , "d", NULL, &help_text[8], 1, false, false },

    /* should be the last one here: it is not printed by --help */
    { "index-only",NULL, NULL, &help_text[10], 1, false, false }
};
#define NOPTS ( sizeof(Options)/sizeof(Options[0]))

rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;
    unsigned i;
    
    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;
    
    UsageSummary (progname);
    
    KOutMsg ("Options:\n");
    
    for (i = 0; i != NOPTS - 1; ++i) {
        HelpOptionLine(Options[i].aliases, Options[i].name, NULL, Options[i].help);
    }
    
    HelpOptionsStandard ();
    
    HelpVersion (fullpath, KAppVersion());
    
    return 0;
}

uint32_t CC KAppVersion(void)
{
    return SRA_DBCC_VERS;
}

#ifdef MAC
#define SRAPathMake(...) (RC(rcRuntime, rcDylib, rcSearching, rcFunction, rcUnsupported))
#define SRAPathFind(...) (RC(rcRuntime, rcDylib, rcSearching, rcFunction, rcUnsupported))
#define SRAPathRelease(...) ((rc_t)(0))
#endif

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    char *src_dir_path = NULL;
    char *src_path = NULL;
    char *obj_name = NULL;
    bool md5_chk = true, md5_chk_explicit = false;
    bool blob_crc = false;
    bool index_chk = false;
    bool is_file = false;
    const KDirectory *src_dir = NULL;
/*  char path_buffer [ 4096 ]; */
    VFSManager *mgr = NULL;
    VResolver *resolver = NULL;

    rc_t rc = KLogLevelSet ( klogInfo );

    if ( rc == 0 )
        rc = ArgsMakeAndHandle(&args, argc, argv, 1, Options, NOPTS);
    
    if (rc == 0) {
        rc = VFSManagerMake(&mgr);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failed to VFSManagerMake()");
        }
    }

    if (rc == 0) {
        rc = VFSManagerGetResolver(mgr, &resolver);
        if (rc != 0) {
            LOGERR(klogInt, rc, "Cannot VFSManagerGetResolver");
        }
        else {
            VResolverRemoteEnable(resolver, vrAlwaysDisable);
        }
    }

    RELEASE(VFSManager, mgr);

    while (rc == 0) {
        uint32_t pcount;
        
        rc = ArgsParamCount(args, &pcount); if (rc) break;
        if (pcount == 1) {
            KDirectory *dir;
            rc = KDirectoryNativeDir(&dir);
            if (rc == 0)
            {
                char const *value;
                
                rc = ArgsParamValue (args, 0, &value); if (rc) break;
                src_dir_path = strdup(value);
                src_path = strdup(value);
                obj_name = strrchr(src_dir_path, '/');
                if ( obj_name != NULL && obj_name [ 1 ] == 0 )
                {
                    * obj_name = 0;
                    obj_name = strrchr(src_dir_path, '/');
                }
                if (obj_name != NULL)
                {
                    if (obj_name == src_dir_path) {
                        src_dir_path[0] = '/';
                        src_dir_path[1] = '\0';
                    }
                    *obj_name++ = '\0';
                }
                else
                {
                    uint32_t type
                        = KDirectoryPathType(dir, "%s", src_path) & ~kptAlias;
                    if (type != kptFile && type != kptDir) {
                        /* check for accession */
                        VPath *acc = NULL;
                        const VPath *pLocal = NULL;
                        const String *local = NULL;

                        if (rc == 0) {
                            rc = VPathMake(&acc, src_path);
                            if (rc != 0) {
                                PLOGERR(klogErr, (klogErr, rc,
                                    "VPathMake($(path)) failed",
                                    PLOG_S(path), src_path));
                            }
                            else {
                               rc = VResolverLocal(resolver, acc, &pLocal);
                            }
                        }
                        if (rc == 0) {
                            rc = VPathMakeString(pLocal, &local);
                            if (rc != 0) {
                                PLOGERR(klogErr, (klogErr, rc,
                                    "VPathMake(local $(path)) failed",
                                    PLOG_S(path), src_path));
                            }
                        }
                        if ( rc == 0 )
                        {
                            PLOGMSG(klogInfo, (klogInfo,
                                "Validating '$(path)'...", PLOG_S(path),
                                local->addr));
                            /* use mapped path */
                            free(src_path);
                            src_path = string_dup(local->addr, local->size);
                            free(src_dir_path);
                            src_dir_path = string_dup(local->addr, local->size);
                            obj_name = strrchr(src_dir_path, '/');
                            if ( obj_name == NULL )
                                obj_name = src_dir_path;
                            else
                            {
                                if ( obj_name == src_dir_path ) {
                                    src_dir_path[0] = '/';
                                    src_dir_path[1] = '\0';
                                }
                                * obj_name ++ = '\0';
                            }
                        }

                        RELEASE(VPath, pLocal);
                        RELEASE(VPath, acc);
                        free((void*)local);
                    }
                    else {
                        char full[PATH_MAX];
                        rc = KDirectoryResolvePath(dir, true,
                                                   full, sizeof full, "%s", src_path);
                        if (rc == 0) {
                            PLOGMSG(klogInfo, (klogInfo,
                                "Validating '$(path)'...", PLOG_S(path),
                                full));
                            /* use mapped path */
                            free(src_path);
                            src_path = string_dup(full, strlen(full));
                            free(src_dir_path);
                            src_dir_path = string_dup(full, strlen(full));
                            obj_name = strrchr(src_dir_path, '/');
                            if ( obj_name == NULL )
                                obj_name = src_dir_path;
                            else
                            {
                                if ( obj_name == src_dir_path ) {
                                    src_dir_path[0] = '/';
                                    src_dir_path[1] = '\0';
                                }
                                * obj_name ++ = '\0';
                            }
                        }
                    }

                    if ( rc != 0 )
                    {
                        /* appears to be a simple name */
                        obj_name = strdup(src_dir_path);
                        src_dir_path[0] = '.';
                        src_dir_path[1] = '\0';
                    }
                }
                
                rc = KDirectoryOpenDirRead(dir, &src_dir, false, "%s", src_dir_path);
                KDirectoryRelease(dir);
                if (rc) {
                    (void)PLOGERR(klogErr, (klogErr, rc,
                        "Failed to open '$(dir)'", "dir=%s", src_dir_path));
                    break;
                }
                else {
                    uint32_t const obj_type = KDirectoryPathType(src_dir, "%s", obj_name) & (~((uint32_t)kptAlias));
                    
                    switch (obj_type) {
                    case kptFile:
                        is_file = true;
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        else {
            MiniUsage(args);
            ArgsWhack(args);
            exit(EXIT_FAILURE);
        }

        rc = ArgsOptionCount(args, "md5", &pcount); if (rc) break;
        md5_chk_explicit = md5_required = (pcount == 1);
        if (pcount > 1) {
            KOutMsg("argument given too many times\n");
            MiniUsage(args);
            break;
        }
        
        rc = ArgsOptionCount(args, "blob-crc", &pcount); if (rc) break;
        blob_crc = (pcount == 1);
        if (pcount > 1) {
            KOutMsg("argument given too many times\n");
            MiniUsage(args);
            break;
        }
        
        rc = ArgsOptionCount(args, "exhaustive", &pcount); if (rc) break;
        exhaustive = (pcount == 1);
        if (pcount > 1) {
            KOutMsg("argument given too many times\n");
            MiniUsage(args);
            break;
        }
        
        rc = ArgsOptionCount(args, "dri", &pcount); if (rc) break;
        ref_int_check = (pcount == 1);
        if (pcount > 1) {
            KOutMsg("argument given too many times\n");
            MiniUsage(args);
            break;
        }
        
#if CHECK_INDEX
        rc = ArgsOptionCount(args, "index", &pcount); if (rc) break;
        index_chk = (pcount == 1);
        if (pcount > 1) {
            KOutMsg("argument given too many times\n");
            MiniUsage(args);
            break;
        }
#endif

        rc = ArgsOptionCount(args, "index-only", &pcount);
        if (rc) {
            break;
        }
        if (pcount > 0) {
            s_IndexOnly = blob_crc = true;
        }
            

        if (blob_crc || index_chk)
        {
            /* if blob or index is requested, md5 is off unless explicitly requested */
            md5_chk = md5_chk_explicit;
        }
        
        {
            bool is_db = false;
            rc = dbcc(src_path, src_dir, obj_name, (md5_chk ? 1 : 0)|(blob_crc ? 2 : 0)|(index_chk ? 4 : 0), & is_db, is_file);
            if ( is_db )
            {
                if ( rc != 0 )
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(db)' check failed", PLOG_S(db), obj_name));
                else
                    (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(db)' is consistent", PLOG_S(db), obj_name));
            }
            else
            {
                if ( rc != 0 )
                    (void)PLOGERR(klogErr, (klogErr, rc, "Table '$(table)' check failed", PLOG_S(table), obj_name));
                else
                    (void)PLOGMSG(klogInfo, (klogInfo, "Table '$(table)' is consistent", PLOG_S(table), obj_name));
            }
        }
        break;
    }

    RELEASE(VResolver, resolver);
    RELEASE(KDirectory, src_dir);
    free(src_path);
   
    return rc;
}
