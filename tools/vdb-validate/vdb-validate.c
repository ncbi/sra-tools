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

#include <vfs/manager-priv.h> /* VFSManagerOpenFileReadDecrypt */
#include <vfs/manager.h> /* VFSManagerMake */
#include <vfs/resolver.h> /* VResolver */
#include <vfs/path.h> /* VPathMake */

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/log-xml.h>

#include <kdb/manager.h>
#include <kdb/database.h>
#include <kdb/table.h>
#include <kdb/meta.h>
#include <kdb/namelist.h>
#include <kdb/consistency-check.h>
#include <kdb/kdb-priv.h> /* KTableOpenDirectoryRead */

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/dependencies.h> /* UIError */
#include <vdb/vdb-priv.h> /* VTableOpenKTableRead */

#include <krypto/encfile.h> /* KEncFileValidate */
#include <krypto/wgaencrypt.h> /* KEncFileValidate */

#include <kfs/kfs-priv.h>
#include <kfs/sra.h>
#include <kfs/tar.h>
#include <kfs/file.h> /* KFileRelease */

#include <insdc/insdc.h>
#include <insdc/sra.h>
#include <sra/srapath.h>
#include <sra/sradb.h>
#include <sra/sraschema.h>

#include <klib/out.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/container.h>
#include <klib/text.h> /* String */
#include <klib/status.h> /* STSMSG */
#include <klib/debug.h>
#include <klib/data-buffer.h>
#include <klib/sort.h>

#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <math.h>

#ifndef MIN
#define MIN(a,b)    (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a,b)    (((a) > (b)) ? (a) : (b))
#endif

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

#define SDC_ROW_CHUNK_MAX 8ull*1024ull*1024ull

#if 0
#define DBG_MSG(args) KOutMsg args
#else
#define DBG_MSG(args)
#endif

static bool exhaustive;
static bool md5_required;
static bool ref_int_check;
static bool s_IndexOnly;
static size_t memory_suggestion = (2ull * 1024ull * 1024ull * 1024ull);

typedef struct node_s {
    int parent;
    int prvSibl;
    int nxtSibl;
    int firstChild;
    unsigned depth;
    unsigned name;
    uint32_t objType;
} node_t;
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
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc,
                "Index '$(index)': $(mesg)",
                "index=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_Index:
        return 0; /* continue with check */
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc,
                "File '$(file)' of index '$(index)' failed MD5 check",
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
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc,
                "Column '$(column)': $(mesg)",
                "column=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        else {
            (void)PLOGMSG(klogInfo, (klogInfo, "Column '$(column)': $(mesg)",
                "column=%s,mesg=%s", what->objName,
                what->info.done.mesg ? what->info.done.mesg : "checked"));
            ++ctx->num_columns;
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_Blob:
        return 0; /* continue with check */
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc,
                "File '$(file)' of column '$(column)' failed MD5 check",
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
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc,
                "Table '$(table)': $(mesg)",
                "table=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        else if (what->info.done.mesg) {
            if (strcmp(what->info.done.mesg, "missing md5 file") == 0
                && md5_required)
            {
                rc_t rc = RC(0, rcTable, rcValidating, rcChecksum, rcNotFound);
                (void)PLOGERR(klogErr, (klogErr, rc,
                    "Table '$(table)': is missing required md5 files",
                    "table=%s", what->objName));
                return rc;
            }
            else if (strncmp("unexpected object ",
                what->info.done.mesg, 18) == 0)
            {
                (void)PLOGMSG(klogWarn, (klogWarn,
                    "Table '$(tbl)': $(mesg)", "tbl=%s,mesg=%s",
                    what->objName, what->info.done.mesg));
            }
            else {
                (void)PLOGMSG(klogInfo, (klogInfo,
                    "Table '$(tbl)' metadata: $(mesg)",
                    "tbl=%s,mesg=%s", what->objName, what->info.done.mesg));
            }
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc,
                "File '$(file)' of table '$(table)' failed MD5 check",
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
            (void)PLOGERR(klogErr, (klogErr, what->info.done.rc,
                "Database '$(db)': $(mesg)",
                "db=%s,mesg=%s", what->objName, what->info.done.mesg));
            if (ctx->rc == 0)
                ctx->rc = what->info.done.rc;
        }
        else if (what->info.done.mesg) {
            if (strcmp(what->info.done.mesg, "missing md5 file") == 0
                && md5_required)
            {
                rc_t rc
                    = RC(rcExe, rcTable, rcValidating, rcChecksum, rcNotFound);
                (void)PLOGERR(klogErr, (klogErr, rc,
                    "Database '$(table)': is missing required md5 files",
                    "table=%s", what->objName));
                return rc;
            }
            else {
                (void)PLOGMSG(klogInfo, (klogInfo,
                    "Database '$(db)' metadata: $(mesg)",
                    "db=%s,mesg=%s", what->objName, what->info.done.mesg));
            }
        }
        return report_rtn (what->info.done.rc);
    case ccrpt_MD5:
        if (what->info.MD5.rc) {
            (void)PLOGERR(klogErr, (klogErr, what->info.MD5.rc,
                "File '$(file)' of database '$(db)' failed MD5 check",
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
    ctx->nextName += (unsigned)strlen(what->objName) + 1;
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
rc_t kdbcc ( const KDBManager *mgr, char const name[], uint32_t mode,
    KPathType *pathType, bool is_file, node_t nodes[], char names[],
    INSDC_SRA_platform_id platform )
{
    rc_t rc = 0;
    cc_context_t ctx;
    char const *objtype;

    uint32_t level = ( mode & 4 ) ? 3 : ( mode & 2 ) ? 1 : 0;
    if (s_IndexOnly)
        level |= CC_INDEX_ONLY;


    memset(&ctx, 0, sizeof(ctx));
    ctx.nodes = &nodes[0];
    ctx.names = &names[0];

    if (KDBManagerExists(mgr, kptDatabase, "%s", name))
        *pathType = kptDatabase;
    else if (KDBManagerExists(mgr, kptTable, "%s", name))
        *pathType = kptTable;
    else if (*pathType == kptDatabase || *pathType == kptTable)
        /* is known already: may be encrypted */ ( void) 0;
    else
    {
        rc = RC(rcExe, rcPath, rcValidating, rcType, rcUnknown);
        (void)PLOGERR(klogErr, (klogErr, rc, "Object '$(table)' "
            "has unknown type", "table=%s", name));
        return rc;
    }

    if (*pathType == kptDatabase)
    {
        const KDatabase *db;

        objtype = "database";
        rc = KDBManagerOpenDBRead ( mgr, & db, "%s", name );
        if ( rc == 0 )
        {
            rc = KDatabaseConsistencyCheck ( db, 0, level, report, & ctx );
            if ( rc == 0 )
            {
                rc = ctx.rc;
                if ( s_IndexOnly )
                    (void)LOGMSG(klogInfo, "Indices: checked");
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
            rc = KTableConsistencyCheck ( tbl, 0, level, report, & ctx, platform );
            if ( rc == 0 )
                rc = ctx.rc;

            if ( rc == 0 && s_IndexOnly )
                (void)LOGMSG(klogInfo, "Index: checked");

            KTableRelease ( tbl );
        }
    }

    if (rc == 0 && ctx.num_columns == 0 && !s_IndexOnly)
    {
        if (is_file)
        {
            (void)PLOGMSG(klogWarn, (klogWarn, "Nothing to validate; "
                                     "the file '$(file)' has no checksums or is truncated.",
                                     "file=%s", name));
        }
        else
        {
            (void)PLOGMSG(klogWarn, (klogWarn, "Nothing to validate; "
                                     "the $(type) '$(file)' has no checksums or is empty.",
                                     "type=%s,file=%s", objtype, name));
        }
    }

    return rc;
}

static
rc_t vdbcc ( const VDBManager *mgr, char const name[], uint32_t mode,
    KPathType *pathType, bool is_file)
{
#if 0
    if ( ( mode & 8) != 0 )
    {
        const VTable *tbl;
        rc_t rc = VDBManagerOpenTableRead(mgr, &tbl, NULL, "%s", name);
        if (rc == 0)
            rc = VTableConsistencyCheck(tbl, 2);
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

static rc_t CC get_sizes_cb(const KDirectory *dir,
    uint32_t type, const char *name, void *data)
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

static rc_t EncFileReadAll(const char *name,
    char *buffer, size_t bsize, size_t *num_read)
{
    rc_t rc = 0;

    VFSManager *mgr = NULL;
    VPath *path = NULL;
    const KFile *f = NULL;

    rc = VFSManagerMake(&mgr);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failed to VFSManagerMake()");
    }

    if (rc == 0) {
        rc = VFSManagerMakePath (mgr, &path, "%s", name);
        if (rc != 0) {
            (void)PLOGERR(klogErr, (klogErr, rc,
                "Failed to VPathMake($(name))", "name=%s", name));
        }
    }

    if (rc == 0) {
        rc = VFSManagerOpenFileReadDecrypt(mgr, &f, path);
        if (rc != 0) {
            (void)PLOGERR(klogErr, (klogErr, rc,
                "Failed to VFSManagerOpenFileReadDecrypt($(name))",
                "name=%s", name));
        }
    }

    if (rc == 0) {
        rc = KFileReadAll(f, 0, buffer, bsize, num_read);
        if (rc != 0) {
            (void)PLOGERR(klogErr, (klogErr, rc,
                "Failed to KFileReadAll($(name))", "name=%s", name));
        }
    }

    KFileRelease(f);
    VPathRelease(path);
    VFSManagerRelease(mgr);

    return rc;
}


#if 0
static rc_t verify_encryption(const KDirectory *dir, const char *name,
    bool *enc, bool *sra)
{
    rc_t rc = 0;
    const KFile *f = NULL;
    char buffer[24];
    size_t num_read = ~0;

    assert(enc && sra);
    *enc = false;
    *sra = true;

    rc = KDirectoryOpenFileRead(dir, &f, "%s", name);
    if (rc == 0) {
        rc = KFileReadAll(f, 0, &buffer, sizeof buffer, &num_read);
    }

    if (rc == 0) {
        size_t sz = num_read < 8 ? num_read : 8;
        assert(num_read <= sizeof buffer);
        rc = KFileIsEnc(buffer, sz);
        if (rc == 0)
        {
            (void)PLOGMSG(klogInfo, (klogInfo, "File '$(f)' is encrypted",
                "f=%s", name));
            *enc = true;
            rc = KEncFileValidate(f);
            if (rc != 0) {
                (void)PLOGERR(klogErr, (klogErr, rc, "Failure of encryption "
                    "validation of file '$(name)'", "name=%s", name));
            }
            else {
                (void)PLOGMSG(klogInfo, (klogInfo, "File '$(f)': encryption OK",
                    "f=%s", name));
                rc = EncFileReadAll(name, buffer, sizeof buffer, &num_read);
            }
        }
        else {
            rc = 0;
            (void)PLOGMSG(klogInfo, (klogInfo, "File '$(f)' is not encrypted",
                "f=%s", name));
        }
    }

    if (rc == 0) {
        rc = KFileIsSRA(buffer, num_read);
        if (rc == 0) {
            *sra = true;
        }
        else if (rc
            == SILENT_RC(rcFS, rcFile, rcIdentifying, rcBuffer, rcWrongType))
        {
            rc = 0;
            *sra = false;
        }
        else {
            (void)PLOGERR(klogErr, (klogErr, rc,
                "Failed to KFileIsSRA($(name))", "name=%s", name));
        }
    }

    KFileRelease(f);

    return rc;
}
#endif

static rc_t init_dbcc(KDirectory const *dir, char const name[], bool is_file,
    node_t **nodes, char **names, KPathType *pathType)
{
    KDirectory const *obj;
    unsigned nobj;
    size_t namesz;
    rc_t rc = 0;

    assert(pathType);

    if (is_file) {
        rc = KDirectoryOpenSraArchiveRead_silent(dir, &obj, false, "%s", name);
        if (rc != 0)
        {   rc = KDirectoryOpenTarArchiveRead_silent(dir, &obj, false, "%s", name); }
        if (rc != 0) {
            const VDBManager *mgr = NULL;
            const VTable *tbl = NULL;
            VSchema *sra_schema = NULL;
            rc = VDBManagerMakeRead(&mgr, dir);
            if (rc != 0)
            {   return rc; }
            for ( ; rc == 0; ) {
                rc = VDBManagerOpenTableRead(mgr, &tbl, sra_schema, "%s", name);
                VSchemaRelease(sra_schema);
                if (rc == 0) {
                    const KTable *ktbl = NULL;
                    rc = VTableOpenKTableRead(tbl, &ktbl);
                    if (rc == 0)
                    {   rc = KTableOpenDirectoryRead(ktbl, &obj); }
                    KTableRelease(ktbl);
                    *pathType = kptTable;
                    break;
                }
                else if (GetRCState(rc) == rcNotFound
                    && GetRCObject(rc) == (enum RCObject)rcSchema && sra_schema == NULL)
                {
                     rc = VDBManagerMakeSRASchema(mgr, &sra_schema);
                }
                else {
                    const VDatabase *db = NULL;
                    const KDatabase *kdb = NULL;
                    rc = VDBManagerOpenDBRead(mgr, &db, NULL, "%s", name);
                    if (rc == 0)
                    {   rc = VDatabaseOpenKDatabaseRead(db, &kdb); }
                    if (rc == 0)
                    {   rc = KDatabaseOpenDirectoryRead(kdb, &obj); }
                    if (rc == 0)
                    {   *pathType = kptDatabase; }
                    KDatabaseRelease(kdb);
                    VDatabaseRelease(db);
                    break;
                }
            }
            VTableRelease(tbl);
            VDBManagerRelease(mgr);
        }
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
        *nodes = calloc(1, nobj * sizeof(**nodes) + namesz);
        if (nodes)
            *names = (char *)&(*nodes)[nobj];
        else
            rc = RC(rcExe, rcSelf, rcConstructing, rcMemory, rcExhausted);
    }
    return rc;
}

static rc_t get_schema_info(KMetadata const *meta, char buffer[], size_t bsz,
    char **vers)
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
        KMDataNodeRelease(node);
    }
    return rc;
}

static rc_t get_tbl_schema_info(VTable const *tbl, char buffer[], size_t bsz,
    char **vers)
{
    KMetadata const *meta;
    rc_t rc = VTableOpenMetadataRead(tbl, &meta);

    *(*vers = &buffer[0]) = '\0';
    if (rc == 0) rc = get_schema_info(meta, buffer, bsz, vers);
    return 0;
}

static rc_t get_db_schema_info(VDatabase const *db, char buffer[], size_t bsz,
    char **vers)
{
    KMetadata const *meta;
    rc_t rc = VDatabaseOpenMetadataRead(db, &meta);

    *(*vers = &buffer[0]) = '\0';
    if (rc == 0)
    {
        rc = get_schema_info(meta, buffer, bsz, vers);
        KMetadataRelease(meta);
    }

    return rc;
}

static rc_t sra_dbcc_454(VTable const *tbl, char const name[])
{
    /* TODO: complete this */
    return 0;
}

typedef struct vdb_validate_params vdb_validate_params;
struct vdb_validate_params
{
    const KDirectory *wd;
    const KDBManager *kmgr;
    const VDBManager *vmgr;

    bool md5_chk;
    bool md5_chk_explicit;
    bool blob_crc;
    bool index_chk;
    bool consist_check;
    bool exhaustive;

    // secondary data checks parameters
    bool sdc_enabled;
    bool sdc_rows_in_percent;
    union
    {
        double percent;
        uint64_t number;
    } sdc_rows;

    bool sdc_pa_len_thold_in_percent;
    union
    {
        double percent;
        uint64_t number;
    } sdc_pa_len_thold;
};

static rc_t tableConsistCheck(const vdb_validate_params *pb, const VTable *tbl)
{
    rc_t rce = 0;

    const VCursor *curs = NULL;

    int64_t firstREAD_LEN = 0;
    uint64_t countREAD_LEN = 0;

    uint64_t i = 0;

    ColumnInfo readLen;
    ColumnInfo spotLen;

    memset(&readLen, 0, sizeof readLen);
    memset(&spotLen, 0, sizeof spotLen);

    assert(pb);

    if (!pb->consist_check) {
        return 0;
    }

    rce = VTableCreateCursorRead(tbl, &curs);
    if (rce != 0) {
        return rce;
    }
    if (rce == 0) {
        readLen.name = "READ_LEN";
        rce = VCursorAddColumn(curs, &readLen.idx, "%s", readLen.name);
    }
    if (rce == 0) {
        spotLen.name = "SPOT_LEN";
        rce = VCursorAddColumn(curs, &spotLen.idx, "%s", spotLen.name);
    }

    if (rce == 0) {
        assert(readLen.idx && spotLen.idx);
        if (readLen.idx == 0) {
            rce = RC(rcExe, rcTable, rcValidating, rcColumn, rcNotFound);
            LOGERR(klogErr, rce, "Cannot find 'READ_LEN' column");
        }
        else if (spotLen.idx == 0) {
            rce = RC(rcExe, rcTable, rcValidating, rcColumn, rcNotFound);
            LOGERR(klogErr, rce, "Cannot find 'SPOT_LEN' column");
        }
    }

    if (rce == 0) {
        rce = VCursorOpen(curs);
    }
    if (rce == 0) {
        rce =
            VCursorIdRange(curs, readLen.idx, &firstREAD_LEN, &countREAD_LEN);
    }
    if (rce == 0) {
        int64_t firstSPOT_LEN = 0;
        uint64_t countSPOT_LEN = 0;
        rce =
            VCursorIdRange(curs, spotLen.idx, &firstSPOT_LEN, &countSPOT_LEN);
        if (rce == 0) {
            if (firstREAD_LEN != firstSPOT_LEN) {
                rce = RC(rcExe, rcTable, rcValidating, rcColumn, rcInvalid);
                LOGERR(klogErr, rce, "The first ID-s "
                    "in READ_LEN and SPOT_LEN columns do not match");
            }
            else if (countREAD_LEN != countSPOT_LEN) {
                rce = RC(rcExe, rcTable, rcValidating, rcColumn, rcInvalid);
                LOGERR(klogErr, rce, "ID ranges "
                    "in READ_LEN and SPOT_LEN columns do not match");
            }
        }
    }

    for (i = 0; i < countREAD_LEN; ++i) {
        uint32_t n = 0;
        uint32_t j = 0;
        rc_t rc = VCursorCellDataDirect(curs, firstREAD_LEN + i,
            readLen.idx, &readLen.elem_bits,
            &readLen.value.vp, NULL, &readLen.elem_count);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "Cannot read 'READ_LEN' column at row $(row)",
                "row=%ld", firstREAD_LEN + i));
            if (rce == 0) {
                rce = rc;
            }
            if (!pb->exhaustive) {
                break;
            }
        }
        rc = VCursorCellDataDirect(curs, firstREAD_LEN + i,
            spotLen.idx, &spotLen.elem_bits,
            &spotLen.value.vp, NULL, &spotLen.elem_count);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "Cannot read 'SPOT_LEN' column at row $(row)",
                "row=%ld", firstREAD_LEN + i));
            if (rce == 0) {
                rce = rc;
            }
            if (!pb->exhaustive) {
                break;
            }
        }

        if (readLen.value.vp == NULL || spotLen.value.vp == NULL) {
            rc = RC(rcExe, rcTable, rcValidating, rcData, rcNull);
            PLOGERR(klogErr, (klogErr, rc,
                "Invalid 'READ_LEN' or 'SPOT_LEN' value at row $(row)",
                "row=%ld", firstREAD_LEN + i));
            if (rce == 0) {
                rce = rc;
            }
            if (!pb->exhaustive) {
                break;
            }
        }
        for (j = 0; j < readLen.elem_count; ++j) {
            n += readLen.value.u32[j];
        }
        if (n != *(spotLen.value.u32)) {
            rc = RC(rcExe, rcTable, rcValidating, rcData, rcCorrupt);
            PLOGERR(klogErr, (klogErr, rc,
                "Sum(READ_LEN) != SPOT_LEN in row $(row)",
                "row=%ld", firstREAD_LEN + i));
            if (rce == 0) {
                rce = rc;
            }
            if (!pb->exhaustive) {
                break;
            }
        }
    }

    if (rce == 0) {
        LOGMSG(klogInfo, "Columns 'READ_LEN' and 'SPOT_LEN' are consistent");
    }

    VCursorRelease(curs);

    return rce;
}

static rc_t sra_dbcc_fastq(const vdb_validate_params *pb,
    const VTable *tbl, char const name[])
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
                VCursorCellData(curs, cols[i].idx, &cols[i].elem_bits,
                    &cols[i].value.vp, NULL, &cols[i].elem_count);
            }
            if (   cols[0].idx == 0 || cols[0].elem_bits == 0
                                                    || cols[0].value.vp == NULL
                || cols[1].idx == 0 || cols[1].elem_bits == 0
                                                    || cols[1].value.vp == NULL)
            {
                rc = RC(rcExe, rcTable, rcValidating, rcColumn, rcNotFound);
            }
            else if (cols[2].idx == 0 || cols[2].elem_bits == 0
                                                    || cols[2].value.vp == NULL)
            {
                (void)PLOGERR(klogWarn, (klogWarn,
                    RC(rcExe, rcTable, rcValidating, rcColumn, rcNotFound),
                    "Table '$(name)' is usable for fasta only; no quality data",
                    "name=%s", name));
            }
        }
        VCursorRelease(curs);
    }

    if (rc == 0) {
        rc = tableConsistCheck(pb, tbl);
    }

    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc,
            "Table '$(name)' is damaged beyond any use", "name=%s", name));
    }

    return rc;
}

static rc_t VTable_get_platform(VTable const *tbl,
    INSDC_SRA_platform_id *rslt)
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

                rc = VCursorCellDataDirect(curs, 1, cid,
                    &ebits, &data, &boff, &ecnt);
                if (rc == 0) {
                    if (ebits == sizeof(platform) * 8 && boff == 0 && ecnt == 1)
                        platform = ((INSDC_SRA_platform_id *)data)[0];
                    else
                        rc =
                            RC(rcExe, rcTable, rcReading, rcType, rcUnexpected);
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

static rc_t verify_table(const vdb_validate_params *pb,
    const VTable *tbl, char const name[])
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
                (void)PLOGMSG(klogWarn, (klogWarn, "Couldn't determine "
                    "SRA Platform; type of table '$(name)' is indeterminate.",
                    "name=%s", name));
            }
            rc = sra_dbcc_fastq(pb, tbl, name);
            if (rc == 0 && platform == SRA_PLATFORM_454) {
                rc = sra_dbcc_454(tbl, name);
            }
        }
        else {
            (void)PLOGERR(klogErr, (klogErr, rc,
                "Failed to read table '$(name)'", "name=%s", name));
        }
    }
    return rc;
}

static rc_t verify_mgr_table(const vdb_validate_params *pb, char const name[])
{
    rc_t rc = 0;

    const VTable *tbl = NULL;
    VSchema *sra_schema = NULL;

    const VDBManager *mgr = NULL;

    assert(pb);

    mgr = pb->vmgr;

    for ( ; ; ) {
        rc = VDBManagerOpenTableRead(mgr, &tbl, sra_schema, "%s", name);
        VSchemaRelease(sra_schema);
        if (rc == 0) {
            rc = verify_table(pb, tbl, name);
            break;
        }
        else if (GetRCState(rc) == rcNotFound && GetRCObject(rc) == (enum RCObject)rcSchema
            && sra_schema == NULL)
        {
            rc = VDBManagerMakeSRASchema(mgr, &sra_schema);
            if (rc) {
                (void)PLOGERR(klogErr, (klogErr, rc,
                    "Failed to open table '$(name)'", "name=%s", name));
                break;
            }
        }
        else {
            (void)PLOGERR(klogErr, (klogErr, rc,
                "Failed to open table '$(name)'", "name=%s", name));
            break;
        }
    }

    RELEASE(VSchema, sra_schema);
    RELEASE(VTable, tbl);

    return rc;
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
        (void)PLOGERR(klogErr, (klogErr, rc,
            "Failed to open table '$(name)'", "name=%s", name));
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
                VCursorCellData(curs, cols[i].idx, &cols[i].elem_bits,
                    &cols[i].value.vp, NULL, &cols[i].elem_count);
            }
            for (i = 0; i < n && rc == 0; ++i) {
                if (cols[i].idx == 0 || cols[i].elem_bits == 0) {
                    (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(name)' "
                        "could not be used to generate SAM", "name=%s", name));
                    break;
                }
            }
        }
        VCursorRelease(curs);
    }
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc,
            "Database '$(name)' is damaged beyond any use", "name=%s", name));
    }
    return rc;
}
#endif

typedef struct id_pair_s {
    int64_t first;
    int64_t second;
} id_pair_t;

static size_t work_chunk(uint64_t const count)
{
    size_t const max = memory_suggestion / (sizeof(id_pair_t));
    size_t chunk = (size_t)count;

#if 1
    /* do as many as possible at once */
    if (chunk > max)
        chunk = max;
#else
    /* break it up into chunks of about equal size */
    while (chunk > max)
        chunk /= 2;
#endif
    return chunk;
}

static void sort_key_pairs(size_t const N, id_pair_t array[/* N */])
{
    id_pair_t a;
    id_pair_t b;
    
#define GET(P, V) ((void)(V = ((id_pair_t const *)(P))[0]))
#define SET(P, V) ((void)((((id_pair_t *)(P))[0]) = V))
#define CMP(A, B) (((GET(A, a)),(GET(B, b))), (a.first  < b.first  ? -1 :      \
                                               b.first  < a.first  ?  1 :      \
                                               a.second < b.second ? -1 :      \
                                               b.second < a.second ?  1 : 0))
#define SWAP(A, B, C, D) do{GET(A, a); GET(B, b); SET(A, b); SET(B, a);}while(0)
    KSORT(array, N, sizeof(array[0]), 0, 0);
#undef SWAP
#undef CMP
#undef SET
#undef GET
}

static void sort_keys(size_t const N, int64_t array[/* N */])
{
#define INDEXOF(A) (((int64_t const *)(A)) - ((int64_t const *)(&array[0])))
#define CMP(A, B) (array[INDEXOF(A)] - array[INDEXOF(B)])
#define SWAP(A, B, C, D) do {                                                  \
    int64_t const a = array[INDEXOF(A)];                                       \
    int64_t const b = array[INDEXOF(B)];                                       \
    array[INDEXOF(A)] = b;                                                     \
    array[INDEXOF(B)] = a; } while(0)
    KSORT(array, N, sizeof(array[0]), 0, 0);
#undef SWAP
#undef CMP
#undef INDEXOF
}

#define CHECK_QUITTING do { rc_t const rc = Quitting(); if (rc) return rc; } while(0);

static size_t load_key_pairs(int64_t const startId,
                             int64_t const endId,
                             size_t const pairs,
                             id_pair_t pair[/* pairs */],
                             VCursor const *const acurs,
                             ColumnInfo *const aci,
                             int64_t plast[],
                             rc_t Rc[])
{
    int64_t last_fkey = INT64_MIN;
    int64_t row = startId;
    size_t j = 0;
    bool ordered = true;
    
    while (row < endId) {
        int64_t first;
        int64_t maybe_last;
        rc_t const rc1 = VCursorPageIdRange(acurs, aci->idx, row, &first, &maybe_last);
        int64_t const last = maybe_last < endId ? maybe_last : endId - 1;
        size_t const count = (last + 1) - first;

        if (rc1) {
            Rc[0] = rc1;
            return 0;
        }
        CHECK_QUITTING;
        
        if (first < row)
            first = row;
        if (row != startId && pairs < count + j)
            break;
        plast[0] = last;
        
        for ( ; j < pairs && row <= last; ++row) {
            rc_t const rc = VCursorCellDataDirect(acurs, row, aci->idx,
                                                  &aci->elem_bits, &aci->value.vp,
                                                  NULL, &aci->elem_count);
            if (rc == 0) {
                if (aci->elem_count == 1) {
                    int64_t const fkey = aci->value.i64[0];
                    
                    ordered &= (last_fkey <= fkey);
                    pair[j].second = row;
                    pair[j].first = fkey;
                    last_fkey = fkey;
                    ++j;
                }
                else {
                    Rc[0] = RC(rcExe, rcDatabase, rcValidating, rcData, rcUnexpected);
                    return 0;
                }
            }
            else if (!(GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound)) {
                Rc[0] = rc;
                return 0;
            }
            /* row not found might be an error but that won't be decided here */
        }
    }
    if (!ordered)
        sort_key_pairs(j, pair);
    
    Rc[0] = 0;
    return j;
}

static bool is_sorted(uint32_t const N, int64_t const key[/* N */])
{
    uint32_t i = 0;
    int64_t last = key[i];
    
    for (i = 1; i < N; ++i) {
        int64_t const cur = key[i];
        
        if (cur < last)
            return false;

        last = cur;
    }
    return true;
}

static rc_t ric_align_generic(int64_t const startId,
                              uint64_t const count,
                              size_t const pairs,
                              id_pair_t pair[/* pairs */],
                              void *scratch[],
                              VCursor const *const acurs,
                              ColumnInfo *const aci,
                              VCursor const *const bcurs,
                              ColumnInfo *const bci
                              )
{
    int64_t chunk;
    int64_t const endId = startId + count;
    size_t scratch_size = 0;

    for (chunk = startId; chunk < endId; ) {
        rc_t rc = 0;
        int64_t last;
        size_t const n = load_key_pairs(chunk, endId, pairs, pair, acurs, aci, &last, &rc);
        size_t i;
        int64_t cur_fkey = 0;
        uint32_t elem_count = 0;
        uint32_t current = 0;
        int64_t const *id = 0;

        if (rc) return rc;
        if (chunk == last)
            break;
        if (chunk != startId) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Referential Integrity: "
                                     "$(aname) <-> $(bname)"
                                     " $(pct)% complete",
                                     "aname=%s,bname=%s,pct=%5.1f",
                                     aci->name, bci->name,
                                     (100.0 * (chunk - startId)) / count));
        }
        chunk = last;
        for (i = 0; i < n; ++i) {
            int64_t const fkey = pair[i].first;
            int64_t const row = pair[i].second;
            
            if (cur_fkey != fkey) {
                uint32_t dummy;
                
                CHECK_QUITTING;
                
                rc = VCursorCellDataDirect(bcurs, fkey, bci->idx,
                                           &dummy, (void const **)&id,
                                           NULL, &elem_count);

                if (GetRCObject(rc) == rcRow && GetRCState(rc) == rcNotFound){
					(void)PLOGMSG(klogWarn, (klogWarn, "Referential Integrity: "
                                     "$(aname) <-> $(bname)"
                                     " failed to retrieve pair $(first) -> $(second)",
                                     "aname=%s,bname=%s,first=%ld,second=%ld",
                                     aci->name, bci->name,
                                     pair[i].first,pair[i].second));

                    return RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                } else if (rc) 
                    return rc;
                
                if (!is_sorted(elem_count, id)) {
                    if (scratch_size < elem_count) {
                        void *const temp = realloc(scratch[0], elem_count * sizeof(id[0]));
                        
                        if (temp == NULL)
                            return RC(rcExe, rcDatabase, rcValidating, rcMemory, rcExhausted);
                        
                        scratch[0] = temp;
                        scratch_size = elem_count;
                    }
                    memcpy(scratch[0], id, elem_count * sizeof(id[0]));
                    sort_keys(elem_count, scratch[0]);
                    id = scratch[0];
                }
                current = 0;
                cur_fkey = fkey;
                while (current < elem_count && id[current] < row) {
                    ++current;
                }
            }
            if (current >= elem_count || id[current] != row){
				  (void)PLOGMSG(klogWarn, (klogWarn, "Referential Integrity: "
                                     "$(aname) <-> $(bname)"
                                     " inconsistens pair $(first) -> $(second)",
                                     "aname=%s,bname=%s,first=%ld,second=%ld",
                                     aci->name, bci->name,
                                     pair[i].first,pair[i].second));

                return RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
			}
            ++current;
        }
    }
    return 0;
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
    VCursor const *acurs = NULL;
    VCursor const *bcurs = NULL;
    ColumnInfo aci;
    ColumnInfo bci;
    int64_t startId;
    uint64_t count;

    aci.name = "REF_ID";
    bci.name = id_col_name;
    
    rc = VTableCreateCursorRead(align, &acurs);
    if (rc == 0) {
        rc = VCursorAddColumn(acurs, &aci.idx, "%s", aci.name);
        if (rc == 0)
            rc = VCursorOpen(acurs);
        if (rc == 0)
            rc = VCursorIdRange(acurs, aci.idx, &startId, &count);
    }
    if (rc)
        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
            "alignment table can not be read", "name=%s", dbname));
    else {
        rc = VTableCreateCursorRead(ref, &bcurs);
        if (rc == 0)
            rc = VCursorAddColumn(bcurs, &bci.idx, "%s", bci.name);
        if (rc == 0)
            rc = VCursorOpen(bcurs);
        if (rc)
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                "reference table can not be read", "name=%s", dbname));
    }
    if (rc == 0) {
        size_t const chunk = work_chunk(count);
        id_pair_t *const pair = malloc(sizeof(id_pair_t) * chunk);

        if (pair) {
            void *scratch = NULL;
            
            rc = ric_align_generic(startId, count, chunk, pair, &scratch,
                                   acurs, &aci, bcurs, &bci);
            if (scratch)
                free(scratch);

            if (GetRCObject(rc) == (enum RCObject)rcData && GetRCState(rc) == rcUnexpected)
                (void)PLOGERR(klogErr, (klogErr, rc,
                    "Database '$(name)': failed referential "
                    "integrity check", "name=%s", dbname));
            else if (GetRCObject(rc) == (enum RCObject)rcData &&
                     GetRCState(rc) == rcInconsistent)
                (void)PLOGERR(klogErr, (klogErr, rc,
 "Database '$(name)': column '$(idcol)' failed referential integrity check",
 "name=%s,idcol=%s", dbname, id_col_name));
            else if (GetRCObject(rc) == (enum RCObject)rcData &&
                     GetRCState(rc) == rcTooBig)
                (void)PLOGERR(klogWarn, (klogWarn, rc = 0, "Database '$(name)':"
                         " referential integrity could not be checked, skipped",
                         "name=%s", dbname));
            else if (rc)
                (void)PLOGERR(klogErr, (klogErr, rc,
"Database '$(name)': reference table can not be read", "name=%s", dbname));

            free(pair);
        }
        else
            rc = RC(rcExe, rcDatabase, rcValidating, rcMemory, rcExhausted);
        
        if (GetRCObject(rc) == rcMemory && GetRCState(rc) == rcExhausted) {
            rc = 0;
            (void)PLOGERR(klogWarn, (klogWarn, rc, "Database '$(name)':"
                " referential integrity could not be checked, skipped",
                "name=%s", dbname));
        }
    }
    VCursorRelease(acurs);
    VCursorRelease(bcurs);
    return rc;
}

static rc_t ric_align_seq_and_pri(char const dbname[],
                                  VTable const *seq,
                                  VTable const *pri)
{
    rc_t rc;
    VCursor const *acurs = NULL;
    VCursor const *bcurs = NULL;
    ColumnInfo aci;
    ColumnInfo bci;
    int64_t startId;
    uint64_t count;

    aci.name = "SEQ_SPOT_ID";
    bci.name = "PRIMARY_ALIGNMENT_ID";
    
    rc = VTableCreateCursorRead(pri, &acurs);
    if (rc == 0)
        rc = VCursorAddColumn(acurs, &aci.idx, "%s", aci.name);
    if (rc == 0)
        rc = VCursorOpen(acurs);
    if (rc == 0)
        rc = VCursorIdRange(acurs, aci.idx, &startId, &count);
    if (rc)
        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
            "alignment table can not be read", "name=%s", dbname));
    else {
        rc = VTableCreateCursorRead(seq, &bcurs);
        if (rc == 0)
            rc = VCursorAddColumn(bcurs, &bci.idx, "%s", bci.name);
        if (rc == 0)
            rc = VCursorOpen(bcurs);
        if (rc)
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                "sequence table can not be read", "name=%s", dbname));
    }
    if (rc == 0) {
        size_t const chunk = work_chunk(count);
        id_pair_t *const pair = malloc((sizeof(id_pair_t)+sizeof(int64_t)) * chunk);

        if (pair) {
            void *scratch = NULL;
            
            rc = ric_align_generic(startId, count, chunk, pair, &scratch,
                                   acurs, &aci, bcurs, &bci);
            if (scratch)
                free(scratch);
            
            if (GetRCObject(rc) == (enum RCObject)rcData && GetRCState(rc) == rcUnexpected)
                (void)PLOGERR(klogErr, (klogErr, rc,
                    "Database '$(name)': failed referential "
                    "integrity check", "name=%s", dbname));
            else if (GetRCObject(rc) == (enum RCObject)rcData &&
                     GetRCState(rc) == rcInconsistent)
                (void)PLOGERR(klogErr, (klogErr, rc,
"Database '$(name)': column 'SEQ_SPOT_ID' failed referential integrity check",
"name=%s", dbname));
            else if (GetRCObject(rc) == (enum RCObject)rcData &&
                     GetRCState(rc) == rcTooBig)
                (void)PLOGERR(klogWarn, (klogWarn, rc = 0, "Database '$(name)':"
                         " referential integrity could not be checked, skipped",
                         "name=%s", dbname));
            else if (rc)
                (void)PLOGERR(klogErr, (klogErr, rc,
"Database '$(name)': sequence table can not be read", "name=%s", dbname));

            free(pair);
        }
        else
            (void)PLOGERR(klogWarn, (klogWarn, rc = 0, "Database '$(name)':"
                         " referential integrity could not be checked, skipped",
                         "name=%s", dbname));
    }
    VCursorRelease(acurs);
    VCursorRelease(bcurs);
    return rc;
}

/* referential integrity and data checks for secondary alignment table */
static rc_t ridc_align_sec(const vdb_validate_params *pb,
                          char const dbname[],
                          VTable const *seq,
                          VTable const *pri,
                          VTable const *sec)
{
    rc_t rc = 0, rc2;
    VCursor const *seq_cursor = NULL;
    VCursor const *pri_cursor = NULL;
    VCursor const *sec_cursor = NULL;
    VCursor const *sec_cursor2 = NULL;

    uint32_t seq_read_len_idx;
    uint32_t seq_pa_id_idx;
    uint32_t pri_has_ref_offset_idx;
    uint32_t sec_has_ref_offset_idx;
    uint32_t sec_seq_spot_id_idx;
    uint32_t sec_seq_read_id_idx;
    uint32_t sec_tmp_mismatch_idx;
    bool has_tmp_mismatch;

    int64_t sec_id_first;
    uint64_t sec_row_count;

    size_t chunk_size;
    id_pair_t *pri_id_pairs = NULL;
    id_pair_t * pri_len_pairs = NULL;
    id_pair_t *seq_spot_id_pairs = NULL;
    id_pair_t *seq_spot_read_id_pairs = NULL;
    uint32_t *seq_read_lens = NULL;

    // SEQUENCE cursor
    if (rc == 0)
    {
        rc2 = VTableCreateCursorRead(seq, &seq_cursor);
        if (rc2 == 0)
            rc2 = VCursorAddColumn(seq_cursor, &seq_read_len_idx, "%s", "READ_LEN");
        if (rc2 == 0)
            rc2 = VCursorAddColumn(seq_cursor, &seq_pa_id_idx, "%s", "PRIMARY_ALIGNMENT_ID");
        if (rc2 == 0)
            rc2 = VCursorOpen(seq_cursor);
        if (rc2 != 0)
        {
            rc = rc2;
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                        "alignment table SEQUENCE can not be read", "name=%s", dbname));
        }
    }

    // PRIMARY_ALIGNMENT cursor
    if (rc == 0)
    {
        if (rc2 == 0)
            rc2 = VTableCreateCursorRead(pri, &pri_cursor);
        if (rc2 == 0)
            rc2 = VCursorAddColumn(pri_cursor, &pri_has_ref_offset_idx, "%s", "(bool)HAS_REF_OFFSET");
        if (rc2 == 0)
            rc2 = VCursorOpen(pri_cursor);
        if (rc2 != 0)
        {
            rc = rc2;
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                        "alignment table PRIMARY_ALIGNMENT can not be read", "name=%s", dbname));
        }
    }

    // SECONDARY_ALIGNMENT cursor
    if (rc == 0)
    {
        if (rc2 == 0)
            rc2 = VTableCreateCursorRead(sec, &sec_cursor);
        if (rc2 == 0)
            rc2 = VCursorAddColumn(sec_cursor, &sec_has_ref_offset_idx, "%s", "(bool)HAS_REF_OFFSET");
        if (rc2 == 0)
        {
            rc2 = VCursorAddColumn(sec_cursor, &sec_tmp_mismatch_idx, "%s", "TMP_MISMATCH");
            if (rc2 == 0)
                has_tmp_mismatch = true;
            else
            {
                has_tmp_mismatch = false;
                rc2 = 0;
            }
        }
        if (rc2 == 0)
            rc2 = VCursorOpen(sec_cursor);
        if (rc2 != 0)
        {
            rc = rc2;
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                        "alignment table SECONDARY_ALIGNMENT can not be read", "name=%s", dbname));
        }
    }
    if (rc == 0)
    {
        if (rc2 == 0)
            rc2 = VTableCreateCursorRead(sec, &sec_cursor2);
        if (rc2 == 0)
            rc2 = VCursorAddColumn(sec_cursor2, &sec_seq_spot_id_idx, "%s", "SEQ_SPOT_ID");
        if (rc2 == 0)
            rc2 = VCursorAddColumn(sec_cursor2, &sec_seq_read_id_idx, "%s", "SEQ_READ_ID");
        if (rc2 == 0)
            rc2 = VCursorOpen(sec_cursor2);
        if (rc2 != 0)
        {
            rc = rc2;
            (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                        "alignment table SECONDARY_ALIGNMENT can not be read", "name=%s", dbname));
        }
    }

    if (rc == 0)
        rc = VCursorIdRange(sec_cursor, sec_has_ref_offset_idx, &sec_id_first, &sec_row_count);
    if (rc != 0)
        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
            "alignment table can not be read", "name=%s", dbname));

    if (rc == 0)
    {
        chunk_size = sec_row_count > SDC_ROW_CHUNK_MAX ? SDC_ROW_CHUNK_MAX : sec_row_count;

        pri_id_pairs = malloc(sizeof(*pri_id_pairs) * chunk_size);
        pri_len_pairs = malloc(sizeof(*pri_len_pairs) * chunk_size);
        seq_spot_id_pairs = malloc(sizeof(*seq_spot_id_pairs) * chunk_size);
        seq_spot_read_id_pairs = malloc(sizeof(*seq_spot_read_id_pairs) * chunk_size);
        seq_read_lens = malloc(sizeof(*seq_read_lens) * chunk_size);

        if (seq_spot_id_pairs == NULL)
        {
            rc = RC(rcExe, rcDatabase, rcValidating, rcMemory, rcExhausted);
        }
    }

    if (rc == 0)
    {
        bool reported_about_no_pa = false;
        uint64_t pa_longer_sa_rows = 0;
        uint64_t pa_longer_sa_limit;
        uint64_t sec_row_lmit;

        int64_t sec_row_id_start = sec_id_first;
        int64_t sec_row_id_end;

        int64_t chunk;

        // set limits from params
        if (pb->sdc_pa_len_thold_in_percent)
            pa_longer_sa_limit = ceil( pb->sdc_pa_len_thold.percent * sec_row_count );
        else if (pb->sdc_pa_len_thold.number == 0 || pb->sdc_pa_len_thold.number > sec_row_count)
            pa_longer_sa_limit = sec_row_count;
        else
            pa_longer_sa_limit = pb->sdc_pa_len_thold.number;

        if (pb->sdc_rows_in_percent)
            sec_row_lmit = ceil( pb->sdc_rows.percent * sec_row_count );
        else if (pb->sdc_rows.number == 0 || pb->sdc_rows.number > sec_row_count)
            sec_row_lmit = sec_row_count;
        else
            sec_row_lmit = pb->sdc_rows.number;

        sec_row_id_end = sec_id_first + MIN(sec_row_count, sec_row_lmit);

        for ( chunk = sec_row_id_start; chunk < sec_row_id_end; chunk += chunk_size )
        {
            int64_t i;
            int64_t i_count = MIN(chunk_size, sec_row_id_end - chunk);
            const void * data_ptr = NULL;
            uint32_t data_len;
            int64_t last_seq_spot_id = INT64_MIN;
            int64_t last_pri_row_id = INT64_MIN;
            bool ordered = true;

            // Load chunk of SEQ_SPOT_ID and sort ids for faster data retrieval
            for ( i = 0; i < i_count; ++i )
            {
                int64_t seq_spot_id;
                int64_t sec_row_id = i + chunk;

                // SECONDARY_ALIGNMENT:SEQ_SPOT_ID
                rc = VCursorCellDataDirect ( sec_cursor2, sec_row_id, sec_seq_spot_id_idx, NULL, (const void**)&data_ptr, NULL, &data_len );
                if ( rc != 0 || data_ptr == NULL || data_len != 1 )
                {
                    if (rc == 0)
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                            "VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, SEQ_SPOT_ID column, row_id: $(ROW_ID)",
                                            "name=%s,ROW_ID=%ld", dbname, sec_row_id));
                    break;
                }

                seq_spot_id = *(const int64_t *)data_ptr;
                DBG_MSG(("SECONDARY_ALIGNMENT:%ld SEQ_SPOT_ID column = %ld\n", sec_row_id, seq_spot_id));
                if (seq_spot_id == 0)
                {
                    rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "SECONDARY_ALIGNMENT:$(ROW_ID) has SEQ_SPOT_ID = 0", "name=%s,ROW_ID=%ld", dbname, sec_row_id));
                    break;
                }

                ordered &= last_seq_spot_id <= seq_spot_id;
                last_seq_spot_id = seq_spot_id;

                seq_spot_id_pairs[i].first = seq_spot_id;
                seq_spot_id_pairs[i].second = sec_row_id;

                // SECONDARY_ALIGNMENT:SEQ_READ_ID
                rc = VCursorCellDataDirect ( sec_cursor2, sec_row_id, sec_seq_read_id_idx, NULL, (const void**)&data_ptr, NULL, &data_len );
                if ( rc != 0 || data_ptr == NULL || data_len != 1 )
                {
                    if (rc == 0)
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, SEQ_READ_ID column, row_id: $(ROW_ID)",
                                "name=%s,ROW_ID=%ld", dbname, sec_row_id));
                    break;
                }
                DBG_MSG(("SECONDARY_ALIGNMENT:%ld SEQ_READ_ID column = %d\n", sec_row_id, *(const int32_t *)data_ptr));

                // one-based read index
                seq_spot_read_id_pairs[i].first = seq_spot_id;
                seq_spot_read_id_pairs[i].second = *(const int32_t *)data_ptr;
            }
            if (rc != 0)
                break;

            if (!ordered)
            {
                sort_key_pairs(i_count, seq_spot_id_pairs);
            }

            // Load chunk of PRIMARY_ALIGNMENT_ID (and some other fields) and sort ids for faster data retrieval
            ordered = true;
            for ( i = 0; i < i_count; ++i )
            {
                int64_t pri_row_id;
                int64_t sec_row_id = seq_spot_id_pairs[i].second;
                int64_t seq_spot_id = seq_spot_id_pairs[i].first;
                int32_t seq_read_id = seq_spot_read_id_pairs[sec_row_id - chunk].second;

                // SEQUENCE:PRIMARY_ALIGNMENT_ID
                rc = VCursorCellDataDirect ( seq_cursor, seq_spot_id, seq_pa_id_idx, NULL, (const void**)&data_ptr, NULL, &data_len );
                if ( rc != 0 || data_ptr == NULL )
                {
                    if (rc == 0)
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                            "VCursorCellDataDirect() failed on SEQUENCE table, PRIMARY_ALIGNMENT_ID column, spot_id: $(SPOT_ID)",
                                            "name=%s,SPOT_ID=%ld", dbname, seq_spot_id));
                    break;
                }

                if ( seq_read_id < 1 || (uint32_t)seq_read_id > data_len )
                {
                    rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "SECONDARY_ALIGNMENT:$(SEC_ROW_ID) SEQ_READ_ID value ($(SEQ_READ_ID)) - 1 based, is out of SEQUENCE:$(SEQ_SPOT_ID) PRIMARY_ALIGNMENT range ($(PRIMARY_ALIGNMENT_LEN))",
                                "name=%s,SEC_ROW_ID=%ld,SEQ_READ_ID=%d,SEQ_SPOT_ID=%ld,PRIMARY_ALIGNMENT_LEN=%u", dbname, sec_row_id, seq_read_id, seq_spot_id, data_len));
                    break;
                }

                pri_row_id = ((const int64_t *)data_ptr)[seq_read_id - 1];
                DBG_MSG(("SEQUENCE:%ld PRIMARY_ALIGNMENT_ID column = %ld\n", seq_spot_id, pri_row_id));
                if (pri_row_id == 0)
                {
                    if (!reported_about_no_pa)
                    {
                        PLOGMSG (klogWarn, (klogWarn, "Database '$(name)' has secondary alignments without primary", "name=%s", dbname));
                        reported_about_no_pa = true;
                    }
                }

                ordered &= last_pri_row_id <= pri_row_id;
                last_pri_row_id = pri_row_id;

                pri_id_pairs[i].first = pri_row_id;
                pri_id_pairs[i].second = sec_row_id;

                // SEQUENCE:READ_LEN
                rc = VCursorCellDataDirect ( seq_cursor, seq_spot_id, seq_read_len_idx, NULL, (const void**)&data_ptr, NULL, &data_len );
                if ( rc != 0 || data_ptr == NULL )
                {
                    if (rc == 0)
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "VCursorCellDataDirect() failed on SEQUENCE table, READ_LEN column, row_id: $(ROW_ID)",
                                "name=%s,ROW_ID=%ld", dbname, seq_spot_id));
                    break;
                }

                if ( seq_read_id < 1 || (uint32_t)seq_read_id > data_len )
                {
                    rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "SECONDARY_ALIGNMENT:$(SEC_ROW_ID) SEQ_READ_ID value ($(SEQ_READ_ID)) - 1 based, is out of SEQUENCE:$(SEQ_SPOT_ID) READ_LEN range ($(SEQ_READ_LEN_LEN))",
                                "name=%s,SEC_ROW_ID=%ld,SEQ_READ_ID=%d,SEQ_SPOT_ID=%ld,SEQ_READ_LEN_LEN=%u", dbname, sec_row_id, seq_read_id, seq_spot_id, data_len));
                    break;
                }

                seq_read_lens[sec_row_id - chunk] = ((const uint32_t *)data_ptr)[seq_read_id - 1];
                DBG_MSG(("SEQUENCE:%ld READ_LEN column = %u\n", seq_spot_id, seq_read_lens[sec_row_id - chunk]));
            }

            if (rc != 0)
                break;

            if (!ordered)
            {
                sort_key_pairs(i_count, pri_id_pairs);
            }

            for ( i = 0; i < i_count; ++i )
            {
                uint32_t pri_len;
                int sec_i_orig = pri_id_pairs[i].second - chunk;
                pri_len_pairs[sec_i_orig].first = pri_id_pairs[i].first;
                if (pri_id_pairs[i].first == 0)
                {
                    pri_len_pairs[sec_i_orig].second = -1;
                    continue;
                }

                // PRIMARY_ALIGNMENT:HAS_REF_OFFSET
                rc = VCursorCellDataDirect ( pri_cursor, pri_len_pairs[sec_i_orig].first, pri_has_ref_offset_idx, NULL, &data_ptr, NULL, &pri_len );
                if ( rc != 0 )
                {
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                            "VCursorCellDataDirect() failed on PRIMARY_ALIGNMENT table, HAS_REF_OFFSET column, row_id: $(ROW_ID)",
                                            "name=%s,ROW_ID=%ld", dbname, pri_len_pairs[sec_i_orig].first));
                    break;
                }
                pri_len_pairs[sec_i_orig].second = pri_len;
                DBG_MSG(("PRIMARY_ALIGNMENT:%ld HAS_REF_OFFSET column len = %u\n", pri_len_pairs[sec_i_orig].first, pri_len_pairs[sec_i_orig].second));
            }
            if (rc != 0)
                break;

            // Iterate over SECONDARY_ALIGNMENT chunk, having data from other table chunks already loaded
            for ( i = 0; i < i_count; ++i )
            {
                int64_t pri_row_id = pri_len_pairs[i].first;
                int64_t sec_row_id = i + chunk;

                int64_t seq_spot_id = seq_spot_read_id_pairs[i].first;
                int32_t seq_read_id = seq_spot_read_id_pairs[i].second;

                uint32_t seq_read_len = seq_read_lens[i];

                uint32_t pri_row_len = pri_len_pairs[i].second;
                uint32_t sec_row_len;

                // SECONDARY_ALIGNMENT:HAS_REF_OFFSET
                rc = VCursorCellDataDirect ( sec_cursor, sec_row_id, sec_has_ref_offset_idx, NULL, (const void**)&data_ptr, NULL, &sec_row_len );
                if ( rc != 0 )
                {
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                            "VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, HAS_REF_OFFSET column, row_id: $(ROW_ID)",
                                            "name=%s,ROW_ID=%ld", dbname, sec_row_id));
                    break;
                }
                DBG_MSG(("SECONDARY_ALIGNMENT:%ld HAS_REF_OFFSET column len = %u\n", sec_row_id, sec_row_len));

                if ( has_tmp_mismatch )
                {
                    const char * p_sa_tmp_mismatch;
                    // SECONDARY_ALIGNMENT:TMP_MISMATCH
                    rc = VCursorCellDataDirect ( sec_cursor, sec_row_id, sec_tmp_mismatch_idx, NULL, (const void**)&p_sa_tmp_mismatch, NULL, &data_len );
                    if ( rc != 0 || p_sa_tmp_mismatch == NULL )
                    {
                        if (rc == 0)
                            rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                                "VCursorCellDataDirect() failed on SECONDARY_ALIGNMENT table, TMP_MISMATCH column, row_id: $(ROW_ID)",
                                                "name=%s,ROW_ID=%ld", dbname, sec_row_id));
                        break;
                    }

                    if (string_chr(p_sa_tmp_mismatch, data_len, '=') != NULL)
                    {
                        rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                        (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                    "SECONDARY_ALIGNMENT:$(ROW_ID) TMP_MISMATCH column contains '='",
                                    "name=%s,ROW_ID=%ld", dbname, sec_row_id));
                        break;
                    }
                }

                DBG_MSG(("Performing length check SA:%ld len = %u\t PA:%ld len = %u\t SEQ:%ld len = %u\n", sec_row_id, sec_row_len, pri_row_id, pri_row_len, seq_spot_id, seq_read_len));
                // move on when there is no primary or PRIMARY_ALIGNMENT.len equal to SECONDARY_ALIGNMENT.len
                if (pri_row_id == 0 || pri_row_len == sec_row_len)
                    continue;

                if (pri_row_len < sec_row_len)
                {
                    rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "PRIMARY_ALIGNMENT:$(PRI_ROW_ID) HAS_REF_OFFSET length ($(PRI_LEN)) less than SECONDARY_ALIGNMENT:$(SEC_ROW_ID) HAS_REF_OFFSET length ($(SEC_LEN))",
                                "name=%s,PRI_ROW_ID=%ld,SEC_ROW_ID=%ld,PRI_LEN=%u,SEC_LEN=%u", dbname, pri_row_id, sec_row_id, pri_row_len, sec_row_len));
                    break;
                }

                // we already know that pri_row_len > sec_row_len
                ++pa_longer_sa_rows;

                if (pri_row_len != seq_read_len)
                {
                    rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "PRIMARY_ALIGNMENT:$(PRI_ROW_ID) HAS_REF_OFFSET length ($(PRI_LEN)) does not match its SEQUENCE:$(SEQ_SPOT_ID) READ_LEN[$(SEQ_READ_ID)] value ($(SEQ_READ_LEN))",
                                "name=%s,PRI_ROW_ID=%ld,PRI_LEN=%u,SEQ_SPOT_ID=%ld,SEQ_READ_ID=%d,SEQ_READ_LEN=%u", dbname, pri_row_id, pri_row_len, seq_spot_id, seq_read_id, seq_read_len));
                    break;
                }

                if (pa_longer_sa_rows >= pa_longer_sa_limit)
                {
                    rc = RC(rcExe, rcDatabase, rcValidating, rcData, rcInconsistent);
                    (void)PLOGERR(klogErr, (klogErr, rc, "Database '$(name)': "
                                "Limit violation (pa_longer_sa): there are at least $(PA_LONGER_SA_ROWS) alignments where HAS_REF_OFFSET column is longer in PRIMARY_ALIGNMENT than in SECONDARY_ALIGNMENT",
                                "name=%s,PA_LONGER_SA_ROWS=%lu", dbname, pa_longer_sa_rows));
                    break;
                }
            }
        }
    }

    free(pri_id_pairs);
    free(pri_len_pairs);
    free(seq_spot_id_pairs);
    free(seq_spot_read_id_pairs);
    free(seq_read_lens);

    VCursorRelease(sec_cursor2);
    VCursorRelease(sec_cursor);
    VCursorRelease(pri_cursor);
    VCursorRelease(seq_cursor);
    return rc;

}

/* database referential integrity check for alignment database */
static rc_t dbric_align(const vdb_validate_params *pb,
                        char const dbname[],
                        VTable const *pri,
                        VTable const *sec,
                        VTable const *seq,
                        VTable const *ref)
{
    rc_t rc = 0;

    if ((rc == 0 || exhaustive) && (pri != NULL && seq != NULL)) {
        rc_t rc2 = ric_align_seq_and_pri(dbname, seq, pri);

        if (rc2 == 0) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(dbname)': "
               "SEQUENCE.PRIMARY_ALIGNMENT_ID <-> PRIMARY_ALIGNMENT.SEQ_SPOT_ID"
               " referential integrity ok", "dbname=%s", dbname));
        }
        if (rc == 0) {
            rc = rc2;
        }
    }
    if ((rc == 0 || exhaustive) && (pri != NULL && ref != NULL)) {
        rc_t rc2 = ric_align_ref_and_align(dbname, ref, pri, 0);

        if (rc2 == 0) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(dbname)': "
                "REFERENCE.PRIMARY_ALIGNMENT_IDS <-> PRIMARY_ALIGNMENT.REF_ID "
                "referential integrity ok", "dbname=%s", dbname));
        }
        if (rc == 0) {
            rc = rc2;
        }
    }
    if (pb->sdc_enabled && (rc == 0 || exhaustive) && (pri != NULL && sec != NULL && seq != NULL)) {
        rc_t rc2 = ridc_align_sec(pb, dbname, seq, pri, sec);
        if (rc2 == 0) {
            (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(dbname)': "
                "SECONDARY_ALIGNMENT table checks ok", "dbname=%s", dbname));
        }
        if (rc == 0) {
            rc = rc2;
        }
    }
    return rc;
}


static rc_t verify_database_align(const vdb_validate_params *pb, VDatabase const *db,
    char const name[], node_t const nodes[], char const names[])
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
                    (void)PLOGERR(klogWarn, (klogWarn, RC(
                        rcExe, rcDatabase, rcValidating, rcTable, rcUnexpected),
                      "Database '$(name)' contains unexpected table '$(table)'",
                      "name=%s,table=%s", name, tname));
                }
                tables |= this_table;
            }
            else {
                (void)PLOGERR(klogWarn, (klogWarn, RC(
                    rcExe, rcDatabase, rcValidating, rcType, rcUnexpected),
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
        (void)PLOGMSG(klogInfo, (klogInfo, "Database '$(name)' "
            "contains only unaligned reads", "name=%s", name));
    }
    else if (   (tables & tbReference) == 0
             || (tables & tbPrimaryAlignment) == 0)
    {
        /* missing reference or primary alignment */
        rc = RC(rcExe, rcDatabase, rcValidating, rcDatabase, rcIncomplete);
        (void)PLOGERR(klogWarn, (klogWarn, rc,
            "Database '$(name)' does not contain all required tables",
            "name=%s", name));
    }
    else if (   ((tables & tbEvidenceAlignment) != 0)
             != ((tables & tbEvidenceInterval ) != 0))
    {
        /* both must be present or both must be absent */
        rc = RC(rcExe, rcDatabase, rcValidating, rcDatabase, rcIncomplete);
        (void)PLOGERR(klogWarn, (klogWarn, rc,
            "Database '$(name)' does not contain all required tables",
            "name=%s", name));
    }
    while (ref_int_check) {
        VTable const *pri = NULL;
        VTable const *sec = NULL;
        VTable const *seq = NULL;
        VTable const *ref = NULL;

        if ((tables & tbPrimaryAlignment) != 0) {
            rc = VDatabaseOpenTableRead(db, &pri, "PRIMARY_ALIGNMENT");
            if (rc) break;
        }
        if ((tables & tbSecondaryAlignment) != 0) {
            rc = VDatabaseOpenTableRead(db, &sec, "SECONDARY_ALIGNMENT");
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
        rc = dbric_align(pb, name, pri, sec, seq, ref);

        RELEASE(VTable, ref);
        RELEASE(VTable, seq);
        RELEASE(VTable, sec);
        RELEASE(VTable, pri);

        break;
    }

    return rc;
}

static rc_t verify_database(const vdb_validate_params *pb, VDatabase const *db,
    char const name[], node_t const nodes[], char const names[])
{
    char schemaName[1024];
    char *schemaVers = NULL;
    rc_t rc;

    rc = get_db_schema_info(db, schemaName, sizeof(schemaName), &schemaVers);
    if (rc) {
        (void)PLOGERR(klogErr, (klogErr, rc,
            "Failed to find database schema for '$(name)'", "name=%s", name));
    }
    else if (strncmp(schemaName, "NCBI:var:db:", 12) == 0) {
        /* TODO: verify NCBI:var:db:* */
    }
    else if (strncmp(schemaName, "NCBI:WGS:db:", 12) == 0) {
        /* TODO: verify NCBI:WGS:db:* */
    }
    else if (strncmp(schemaName, "NCBI:align:db:", 14) == 0) {
        rc = verify_database_align(pb, db, name, nodes, names);
    }
    else if (strcmp(schemaName, "NCBI:SRA:PacBio:smrt:db") == 0) {
        /* TODO: verify NCBI:SRA:PacBio:smrt:db */
    }
    else {
        (void)PLOGERR(klogWarn, (klogWarn,
            RC(rcExe, rcDatabase, rcValidating, rcType, rcUnrecognized),
            "Database '$(name)' has unrecognized type '$(type)'",
            "name=%s,type=%s", name, schemaName));
    }
    return rc;
}

static rc_t verify_mgr_database(const vdb_validate_params *pb,
    char const name[], node_t const nodes[], char const names[])
{
    rc_t rc = 0;
    VDatabase const *child;
    const VDBManager *mgr = NULL;
    
    assert(pb);

    mgr = pb->vmgr;

    rc = VDBManagerOpenDBRead(mgr, &child, NULL, "%s", name);

    if (rc == 0) {
        rc = verify_database(pb, child, name, nodes, names);
        VDatabaseRelease(child);
    }

    return rc;
}

static rc_t sra_dbcc(const vdb_validate_params *pb,
    char const name[], node_t const nodes[], char const names[])
{
    rc_t rc = 0;

    if (nodes[0].objType == kptDatabase)
        rc = verify_mgr_database(pb, name, nodes, names);
    else
        rc = verify_mgr_table(pb, name);

    return rc;
}

static
rc_t get_platform(const VDBManager *mgr,
    const VTable *aTbl, char const name[],
    INSDC_SRA_platform_id *platform)
{
    rc_t rc = 0;
    const VTable *tbl = aTbl;
    assert(name && platform);
    if (tbl == NULL) {
        VSchema *sra_schema = NULL;
        for ( ; rc == 0; ) {
            rc = VDBManagerOpenTableRead(mgr, &tbl, sra_schema, "%s", name);
            VSchemaRelease(sra_schema);
            if (rc == 0) {
                rc = VTable_get_platform(tbl, platform);
                break;
            }
            else if (GetRCState(rc) == rcNotFound && GetRCObject(rc) == (enum RCObject)rcSchema
                && sra_schema == NULL)
            {
                rc = VDBManagerMakeSRASchema(mgr, &sra_schema);
            }
        }
    }

    if (aTbl == NULL)
        VTableRelease(tbl);

    return rc;
}

static
rc_t dbcc ( const vdb_validate_params *pb, const char *path, bool is_file )
{
    char *names;
    KPathType pathType = kptNotFound;
    node_t *nodes = NULL;
    const char *obj_type, *obj_name;

    rc_t rc = init_dbcc ( pb -> wd, path, is_file, & nodes, & names, & pathType );
    if ( rc == 0 )
    {
        /* construct mode */
        uint32_t mode = ( pb -> md5_chk ? 1 : 0 )
                      | ( pb -> blob_crc ? 2 : 0 )
                      | ( pb -> index_chk ? 4 : 0 )
                      ;

        INSDC_SRA_platform_id platform = SRA_PLATFORM_UNDEFINED;
        get_platform ( pb -> vmgr, NULL, path, & platform );

        /* check as kdb object */
        rc = kdbcc ( pb -> kmgr, path, mode, & pathType, is_file, nodes, names, platform );
        if ( rc == 0 )
            rc = vdbcc ( pb -> vmgr, path, mode, & pathType, is_file );
        if ( rc == 0 )
            rc = sra_dbcc(pb, path, nodes, names);
    }

    obj_type = ( pathType == kptDatabase ) ? "Database" : "Table";
    obj_name = strrchr ( path, '/' );
    if ( obj_name ++ == NULL )
        obj_name = path;

    if ( rc != 0 )
    {
        PLOGERR ( klogErr, ( klogErr, rc,
                             "$(objType) '$(objName)' check failed"
                             , "objType=%s,objName=%s"
                             , obj_type, obj_name ) );
    }
    else
    {
        PLOGMSG ( klogInfo, ( klogInfo,
                              "$(objType) '$(objName)' is consistent"
                             , "objType=%s,objName=%s"
                             , obj_type, obj_name ) );
    }

    free ( nodes );
    return rc;
}

static
const char *generate_relpath ( const vdb_validate_params *pb, const KDirectory *dir,
    char *buffer, size_t bsize, const char *path )
{
    if ( dir != pb -> wd )
    {
        char full [ 4096 ];
        rc_t rc = KDirectoryResolvePath ( dir, true, full, sizeof full, "%s", path );
        if ( rc == 0 )
        {
            rc = KDirectoryResolvePath ( pb -> wd, false, buffer, bsize, "%s", full );
            if ( rc == 0 )
                path = buffer;
        }
    }

    return path;
}

static
rc_t vdb_validate_file ( const vdb_validate_params *pb, const KDirectory *dir, const char *path )
{
    char buffer [ 4096 ];
    const char *relpath = generate_relpath ( pb, dir, buffer, sizeof buffer, path );

    const KFile *f;
    rc_t rc = KDirectoryOpenFileRead ( dir, & f, "%s", path );
    if ( rc != 0 )
        PLOGERR ( klogErr, ( klogErr, rc, "File '$(fname)' could not be opened", "fname=%s", relpath ) );
    else
    {
        bool is_sra = false;
        enum EEncrypted {
            eNo,
            eEncrypted,
            eWGA
        } encrypted = eNo;

        size_t num_read;
        char buffer [ 4096 ];
        rc = KFileReadAll ( f, 0, buffer, sizeof buffer, & num_read );
        if ( rc != 0 )
            PLOGERR ( klogErr, ( klogErr, rc, "File '$(fname)' could not be read", "fname=%s", relpath ) );
        else {
            /* special kludge to prevent code from looking too far at header */
            size_t hdr_bytes = num_read;
            if ( num_read > 8 )
                hdr_bytes = 8;

            /* check for encrypted file */
            if ( KFileIsEnc ( buffer, hdr_bytes ) == 0 ) {
                encrypted = eEncrypted;
            }
            else if ( KFileIsWGAEnc ( buffer, hdr_bytes ) == 0 ) {
                encrypted = eWGA;
            }

            if (encrypted != eNo) {
                PLOGMSG ( klogInfo, ( klogInfo,
                    "Validating $(type)encrypted file '$(fname)'...",
                    "type=%s,fname=%s",
                    encrypted == eWGA ? "WGA " : " ", relpath ) );
                switch (encrypted) {
                    case eEncrypted:
                        rc = KEncFileValidate(f);
                        break;
                    case eWGA: {
                        VFSManager *mgr = NULL;
                        rc = VFSManagerMake(&mgr);
                        if (rc != 0) {
                            LOGERR(klogInt, rc, "Cannot VFSManagerMake");
                        }
                        else {
                            rc = VFSManagerWGAValidateHack(mgr, f, relpath);
                        }
                        VFSManagerRelease(mgr);
                        break;
                    }
                    default:
                        assert(0);
                }
                if ( rc != 0 ) {
                    PLOGERR ( klogErr, ( klogErr, rc,
                        "Encrypted file '$(fname)' could not be validated",
                        "fname=%s", relpath ) );
                }
                else {
                    PLOGMSG ( klogInfo, ( klogInfo,
                        "Encrypted file '$(fname)' appears valid",
                        "fname=%s", relpath ) );

                    rc = EncFileReadAll ( relpath, buffer, sizeof buffer,
                        & num_read );
                    if ( rc == 0 && KFileIsSRA ( buffer, num_read ) == 0 )
                        is_sra = true;
                }
            }
            else if ( KFileIsSRA ( buffer, num_read ) == 0 ) {
                is_sra = true;
            }
        }

        KFileRelease ( f );

        if ( rc == 0 && is_sra )
            rc = dbcc ( pb, relpath, true );
    }

    return rc;
}

static
rc_t vdb_validate_database ( const vdb_validate_params *pb, const KDirectory *dir, const char *path )
{
    char buffer [ 4096 ];
    const char *relpath = generate_relpath ( pb, dir, buffer, sizeof buffer, path );
    return dbcc ( pb, relpath, false );
}

static
rc_t vdb_validate_table ( const vdb_validate_params *pb, const KDirectory *dir, const char *path )
{
    char buffer [ 4096 ];
    const char *relpath = generate_relpath ( pb, dir, buffer, sizeof buffer, path );
    return dbcc ( pb, relpath, false );
}

static
KPathType vdb_subdir_type ( const vdb_validate_params *pb, const KDirectory *dir, const char *name )
{
    char full [ 4096 ];
    rc_t rc = KDirectoryResolvePath ( dir, true, full, sizeof full, "%s", name );
    if ( rc == 0 )
    {
        switch ( KDBManagerPathType ( pb -> kmgr, "%s", full ) )
        {
        case kptDatabase:
            return kptDatabase;
        case kptTable:
            return kptTable;
        }
    }
    return kptDir;
}

static
rc_t CC vdb_validate_dir ( const KDirectory *dir, uint32_t type, const char *name, void *data )
{
    switch ( type & ~ kptAlias )
    {
    case kptFile:
        return vdb_validate_file ( data, dir, name );
    case kptDir:
        switch ( vdb_subdir_type ( data, dir, name ) )
        {
        case kptDatabase:
            return vdb_validate_database ( data, dir, name );
        case kptTable:
            return vdb_validate_table ( data, dir, name );
        default:
            return KDirectoryVisit ( dir, false, vdb_validate_dir, data, "%s", name );
        }
    }

    return 0;
}

#define rcResolver   rcTree
static bool NotFoundByResolver(rc_t rc) {
    if (GetRCModule(rc) == rcVFS) {
        if (GetRCTarget(rc) == rcResolver) {
            if (GetRCContext(rc) == rcResolving) {
                if (GetRCState(rc) == rcNotFound) {
                    return true;
                }
            }
        }
    }
    return false;
}

static rc_t vdb_validate(const vdb_validate_params *pb, const char *aPath) {
    bool bad = false;
    const String *local = NULL;
    VFSManager *mgr = NULL;
    VResolver *resolver = NULL;
    const char *path = aPath;
    KPathType pt = kptNotFound;

    rc_t rc = VFSManagerMake(&mgr);
    if (rc != 0) {
        LOGERR(klogInt, rc, "Cannot VFSManagerMake");
        return rc;
    }

    rc = VFSManagerGetResolver(mgr, &resolver);
    if (rc != 0) {
        LOGERR(klogInt, rc, "Cannot VFSManagerGetResolver");
    }
    else {
        VResolverRemoteEnable(resolver, vrAlwaysDisable);
    }

    /* what type of thing is this path? */
    pt = KDirectoryPathType(pb->wd, "%s", path);

    if ((pt & ~kptAlias) == kptNotFound) {
        const VPath *pLocal = NULL;
        VPath *acc = NULL;
        bad = true;
        rc = VFSManagerMakePath (mgr, &acc, "%s", path);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "VPathMake($(path)) failed", PLOG_S(path), path));
        }
        else {
            rc = VResolverLocal(resolver, acc, &pLocal);
            if (rc != 0 && NotFoundByResolver(rc)) {
                bad = false;
                PLOGMSG(klogInfo, (klogInfo,
                    "'$(fname)' could not be found", "fname=%s", path));
            }
        }

        if (rc == 0) {
            rc = VPathMakeString(pLocal, &local);
            if (rc != 0) {
                PLOGERR(klogErr, (klogErr, rc,
                    "VPathMake(local $(path)) failed", PLOG_S(path), path));
            }
        }

        if (rc == 0) {
            path = local->addr;
            PLOGMSG(klogInfo, (klogInfo,
                "Validating '$(path)'...", PLOG_S(path), path));
            pt = KDirectoryPathType(pb -> wd, "%s", path);
            bad = false;
        }

        RELEASE(VPath, acc);
        RELEASE(VPath, pLocal);
    }
    RELEASE(VFSManager, mgr);
    RELEASE(VResolver, resolver);

    if (rc == 0) {
        switch (pt & ~kptAlias) {
            case kptNotFound:
                rc = RC(rcExe, rcPath, rcValidating, rcPath, rcNotFound);
                bad = true;
                break;
            case kptBadPath:
                rc = RC(rcExe, rcPath, rcValidating, rcPath, rcInvalid);
                bad = true;
                break;
            case kptFile:
                rc = vdb_validate_file(pb, pb->wd, path);
                break;
            case kptDir:
                switch(KDBManagerPathType (pb->kmgr, "%s", path)) {
                    case kptDatabase:
                        rc = vdb_validate_database(pb, pb->wd, path);
                        break;
                    case kptTable:
                    case kptPrereleaseTbl:
                        rc = vdb_validate_table(pb, pb->wd, path);
                        break;
                    case kptIndex:
                    case kptColumn:
                        rc = RC(rcExe, rcPath, rcValidating,
                            rcType, rcUnsupported);
                        bad = true;
                        break;
                    default:
                        rc = KDirectoryVisit(pb -> wd, false,
                                             vdb_validate_dir, (void*)pb, "%s", path);
                        break;
                }
                break;
            default:
                break;
        }
    }

    free((void*)local);

    if (bad) {
        PLOGMSG ( klogWarn, ( klogWarn,
            "Path '$(fname)' could not be validated", "fname=%s", path ) );
    }

    return rc;
}

/*static char const* const defaultLogLevel =
#if _DEBUGGING
"debug5";
#else
"info";
#endif*/

/******************************************************************************
 * Usage
 ******************************************************************************/
const char UsageDefaultName[] = "vdb-validate";

rc_t CC UsageSummary(const char *prog_name)
{
    return KOutMsg ( "Usage: %s [options] path [ path... ]\n"
                     "\n"
                     , prog_name );
}

/*static char const *help_text[] =
{
    "Check components md5s if present, "
            "fail unless other checks are requested (default: yes)", NULL,
    "Check blobs CRC32 (default: no)", NULL,
    "Check 'skey' index (default: no)", NULL,
    "Continue checking object for all possible errors (default: no)", NULL,
    "Check data referential integrity for databases (default: no)", NULL,
    "Check index-only with blobs CRC32 (default: no)", NULL
};*/

#define ALIAS_md5  "5"
#define OPTION_md5 "md5"
static const char *USAGE_MD5[] = { "Check components md5s if present, "
    "fail unless other checks are requested (default: yes)", NULL };
/*
#define ALIAS_MD5  "M"
#define OPTION_MD5 "MD5"
*/
#define ALIAS_blob_crc  "b"
#define OPTION_blob_crc "blob-crc"
static const char *USAGE_BLOB_CRC[] =
{ "Check blobs CRC32 (default: yes)", NULL };

#define ALIAS_BLOB_CRC  "B"
#define OPTION_BLOB_CRC "BLOB-CRC"

#define ALIAS_CNS_CHK  "C"
#define OPTION_CNS_CHK "CONSISTENCY-CHECK"
static const char *USAGE_CNS_CHK[] =
{ "Deeply check data consistency for tables (default: no)", NULL };

#if CHECK_INDEX
#define ALIAS_INDEX  "i"
#define OPTION_INDEX "index"
static const char *USAGE_INDEX[] = { "Check 'skey' index (default: no)", NULL };
#endif

#define ALIAS_EXHAUSTIVE  "x"
#define OPTION_EXHAUSTIVE "exhaustive"
static const char *USAGE_EXHAUSTIVE[] =
{ "Continue checking object for all possible errors (default: false)", NULL };

#define ALIAS_ref_int  "d"
#define OPTION_ref_int "referential-integrity"
static const char *USAGE_REF_INT[] =
{ "Check data referential integrity for databases (default: yes)", NULL };

#define ALIAS_REF_INT  "I"
#define OPTION_REF_INT "REFERENTIAL-INTEGRITY"

#define OPTION_SDC_ROWS "sdc:rows"
static const char *USAGE_SDC_ROWS[] =
{ "Specify maximum amount of secondary alignment rows to look at before saying accession is good, default 100000.",
  "Specifying 0 will iterate the whole table. Can be in percent (e.g. 5%)",
  NULL };

#define OPTION_SDC_PLEN_THOLD "sdc:plen_thold"
static const char *USAGE_SDC_PLEN_THOLD[] =
{ "Specify a threshold for amount of secondary alignment which are shorter (hard-clipped) than corresponding primaries, default 1%.", NULL };


static const char *USAGE_DRI[] =
{ "Do not check data referential integrity for databases", NULL };

static const char *USAGE_IND_ONLY[] =
{ "Check index-only with blobs CRC32 (default: no)", NULL };

static OptDef options [] =
{                                                    /* needs_value, required */
/*  { OPTION_MD5     , ALIAS_MD5     , NULL, USAGE_MD5     , 1, true , false }*/
    { OPTION_BLOB_CRC, ALIAS_BLOB_CRC, NULL, USAGE_BLOB_CRC, 1, true , false }
#if CHECK_INDEX
  , { OPTION_INDEX   , ALIAS_INDEX   , NULL, USAGE_INDEX   , 1, false, false }
#endif
  , { OPTION_EXHAUSTIVE,
                   ALIAS_EXHAUSTIVE, NULL, USAGE_EXHAUSTIVE, 1, false, false }
  , { OPTION_REF_INT , ALIAS_REF_INT , NULL, USAGE_REF_INT , 1, true , false }
  , { OPTION_CNS_CHK , ALIAS_CNS_CHK , NULL, USAGE_CNS_CHK , 1, true , false }

    /* secondary alignment table data check options */
  , { OPTION_SDC_ROWS, NULL          , NULL, USAGE_SDC_ROWS, 1, true , false }
  , { OPTION_SDC_PLEN_THOLD, NULL    , NULL, USAGE_SDC_PLEN_THOLD, 1, true , false }

    /* not printed by --help */
  , { "dri"          , NULL          , NULL, USAGE_DRI     , 1, false, false }
  , { "index-only"   ,NULL           , NULL, USAGE_IND_ONLY, 1, false, false }

    /* obsolete options for backward compatibility */
  , { OPTION_md5     , ALIAS_md5     , NULL, USAGE_MD5     , 1, true , false }
  , { OPTION_blob_crc, ALIAS_blob_crc, NULL, USAGE_BLOB_CRC, 1, false, false }
  , { OPTION_ref_int , ALIAS_ref_int , NULL, USAGE_REF_INT , 1, false, false }
};

/*
#define NUM_SILENT_TRAILING_OPTIONS 5

static const char *option_params [] =
{
    NULL
  , NULL
#if CHECK_INDEX
  , NULL
#endif
  , NULL
  , NULL
  , NULL
  , NULL
};
*/
rc_t CC Usage ( const Args * args )
{
/*  uint32_t i; */
    const char *progname, *fullpath;
    rc_t rc = ArgsProgram ( args, & fullpath, & progname );
    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "  Examine directories, files and VDB objects,\n"
              "  reporting any problems that can be detected.\n"
              "\n"
              "Components md5s are always checked if present.\n"
              "\n"
              "Options:\n"
        );

/*  HelpOptionLine(ALIAS_MD5     , OPTION_MD5     , "yes | no", USAGE_MD5); */
    HelpOptionLine(ALIAS_BLOB_CRC, OPTION_BLOB_CRC, "yes | no", USAGE_BLOB_CRC);
#if CHECK_INDEX
    HelpOptionLine(ALIAS_INDEX   , OPTION_INDEX   , "yes | no", USAGE_INDEX);
#endif
    HelpOptionLine(ALIAS_REF_INT , OPTION_REF_INT , "yes | no", USAGE_REF_INT);
    HelpOptionLine(ALIAS_CNS_CHK , OPTION_CNS_CHK , "yes | no", USAGE_CNS_CHK);
    HelpOptionLine(ALIAS_EXHAUSTIVE, OPTION_EXHAUSTIVE, NULL, USAGE_EXHAUSTIVE);
    HelpOptionLine(NULL          , OPTION_SDC_ROWS, "rows"    , USAGE_SDC_ROWS);
    HelpOptionLine(NULL          , OPTION_SDC_PLEN_THOLD, "threshold", USAGE_SDC_PLEN_THOLD);

/*
#define NUM_LISTABLE_OPTIONS \
    ( sizeof options / sizeof options [ 0 ] - NUM_SILENT_TRAILING_OPTIONS )

    for ( i = 0; i < NUM_LISTABLE_OPTIONS; ++i )
    {
        HelpOptionLine ( options [ i ] . aliases, options [ i ] . name,
            option_params [ i ], options [ i ] . help );
    }
*/
    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion () );

    return 0;
}

static
rc_t parse_args ( vdb_validate_params *pb, Args *args )
{
    const char *dummy = NULL;
    rc_t rc;
    uint32_t cnt;

    pb -> md5_chk = true;
    pb->consist_check = false;
    ref_int_check = pb -> blob_crc
        = pb -> md5_chk_explicit = md5_required = true;
    pb -> sdc_rows_in_percent = false;
    pb -> sdc_rows.number = 100000;
    pb -> sdc_pa_len_thold_in_percent = true;
    pb -> sdc_pa_len_thold.percent = 0.01;

  {
    rc = ArgsOptionCount(args, OPTION_CNS_CHK, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_CNS_CHK "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_CNS_CHK, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc,
                "Failure to get '" OPTION_CNS_CHK "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'y') {
            pb->consist_check = true;
        }
    }
  }
  {
    rc = ArgsOptionCount ( args, "exhaustive", & cnt );
    if ( rc != 0 )
        return rc;
    exhaustive = cnt != 0;
  }
  {
    rc = ArgsOptionCount(args, OPTION_REF_INT, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_REF_INT "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_REF_INT, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc,
                "Failure to get '" OPTION_REF_INT "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'n') {
            ref_int_check = false;
        }
    }
  }
  {
    rc = ArgsOptionCount ( args, "dri", & cnt );
    if ( rc != 0 )
        return rc;
    if (cnt != 0) {
        ref_int_check = false;
    }
  }
#if CHECK_INDEX
  {
    rc = ArgsOptionCount ( args, "index", & cnt );
    if ( rc != 0 )
        return rc;
    pb -> index_chk = cnt != 0;
  }
#endif
  {
    rc = ArgsOptionCount ( args, "index-only", & cnt );
    if ( rc != 0 )
        return rc;
    if ( cnt != 0 )
        s_IndexOnly = pb -> blob_crc = true;
  }
  {
    rc = ArgsOptionCount( args, OPTION_BLOB_CRC, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_BLOB_CRC "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_BLOB_CRC, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc,
                "Failure to get '" OPTION_BLOB_CRC "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'n') {
            pb -> blob_crc = false;
        }
    }
  }
  {
    rc = ArgsOptionCount(args, OPTION_md5, &cnt);
    if (rc != 0) {
        LOGERR(klogErr, rc, "Failure to get '" OPTION_md5 "' argument");
        return rc;
    }
    if (cnt != 0) {
        rc = ArgsOptionValue(args, OPTION_md5, 0, (const void **)&dummy);
        if (rc != 0) {
            LOGERR(klogErr, rc, "Failure to get '" OPTION_md5 "' argument");
            return rc;
        }
        assert(dummy && dummy[0]);
        if (dummy[0] == 'n') {
            pb -> md5_chk = pb -> md5_chk_explicit = md5_required = false;
        }
    }
  }
  {
      rc = ArgsOptionCount ( args, OPTION_SDC_ROWS, &cnt );
      if (rc)
      {
          LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SDC_ROWS);
          return rc;
      }

      if (cnt > 0)
      {
          uint64_t value;
          size_t value_size;
          rc = ArgsOptionValue ( args, OPTION_SDC_ROWS, 0, (const void **) &dummy );
          if (rc)
          {
              LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SDC_ROWS);
              return rc;
          }

          pb->sdc_enabled = true;

          value_size = string_size ( dummy );
          if ( value_size >= 1 && dummy[value_size - 1] == '%' )
          {
              value = string_to_U64 ( dummy, value_size - 1, &rc );
              if (rc)
              {
                  LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_ROWS);
                  return rc;
              }
              else if (value == 0 || value > 100)
              {
                  rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                  LOGERR (klogInt, rc, OPTION_SDC_ROWS " has illegal percentage value (has to be 1-100%)" );
                  return rc;
              }

              pb->sdc_rows_in_percent = true;
              pb->sdc_rows.percent = (double)value / 100;
          }
          else
          {
              value = string_to_U64 ( dummy, value_size, &rc );
              if (rc)
              {
                  LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_ROWS);
                  return rc;
              }
              pb->sdc_rows_in_percent = false;
              pb->sdc_rows.number = value;
          }
      }
  }
  {
        rc = ArgsOptionCount ( args, OPTION_SDC_PLEN_THOLD, &cnt );
        if (rc)
        {
            LOGERR (klogInt, rc, "ArgsOptionCount() failed for " OPTION_SDC_PLEN_THOLD);
            return rc;
        }

        if (cnt > 0)
        {
            uint64_t value;
            size_t value_size;
            rc = ArgsOptionValue ( args, OPTION_SDC_PLEN_THOLD, 0, (const void **) &dummy );
            if (rc)
            {
                LOGERR (klogInt, rc, "ArgsOptionValue() failed for " OPTION_SDC_PLEN_THOLD);
                return rc;
            }

            pb->sdc_enabled = true;

            value_size = string_size ( dummy );
            if ( value_size >= 1 && dummy[value_size - 1] == '%' )
            {
                value = string_to_U64 ( dummy, value_size - 1, &rc );
                if (rc)
                {
                    LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_PLEN_THOLD);
                    return rc;
                }
                else if (value == 0 || value > 100)
                {
                    rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                    LOGERR (klogInt, rc, OPTION_SDC_PLEN_THOLD " has illegal percentage value (has to be 1-100%)" );
                    return rc;
                }

                pb->sdc_pa_len_thold_in_percent = true;
                pb->sdc_pa_len_thold.percent = (double)value / 100;
            }
            else
            {
                value = string_to_U64 ( dummy, value_size, &rc );
                if (rc)
                {
                    LOGERR (klogInt, rc, "string_to_U64() failed for " OPTION_SDC_PLEN_THOLD);
                    return rc;
                }
                pb->sdc_pa_len_thold_in_percent = false;
                pb->sdc_pa_len_thold.number = value;
            }
        }
    }

    if ( pb -> blob_crc || pb -> index_chk )
        pb -> md5_chk = pb -> md5_chk_explicit;

    pb->exhaustive = exhaustive;

    return 0;
}

static
void vdb_validate_params_whack ( vdb_validate_params *pb )
{
    VDBManagerRelease ( pb -> vmgr );
    KDBManagerRelease ( pb -> kmgr );
    KDirectoryRelease ( pb -> wd );
    memset ( pb, 0, sizeof * pb );
}

static
rc_t vdb_validate_params_init ( vdb_validate_params *pb )
{
    rc_t rc;
    KDirectory *wd;

    memset ( pb, 0, sizeof * pb );

    rc = KDirectoryNativeDir ( & wd );
    if ( rc == 0 )
    {
        pb -> wd = wd;
        rc = VDBManagerMakeRead ( & pb -> vmgr, wd );
        if ( rc == 0 )
        {
            rc = VDBManagerOpenKDBManagerRead ( pb -> vmgr, & pb -> kmgr );
            if ( rc == 0 )
                return 0;
        }
    }

    vdb_validate_params_whack ( pb );
    return rc;
}

static rc_t main_with_args(Args *const args)
{
    XMLLogger const *xlogger = NULL;
    rc_t rc = XMLLogger_Make(&xlogger, NULL, args);

    if (rc) {
        LOGERR(klogErr, rc, "Failed to make XML logger");
    }
    else {
        uint32_t pcount;
        rc = ArgsParamCount ( args, & pcount );
        if ( rc != 0 )
            LOGERR ( klogErr, rc, "Failed to count command line parameters" );
        else if ( pcount == 0 )
        {
            rc = RC ( rcExe, rcPath, rcValidating, rcParam, rcInsufficient );
            LOGERR ( klogErr, rc, "No paths to validate" );
            MiniUsage ( args );
        }
        else
        {
            vdb_validate_params pb;
            rc = vdb_validate_params_init ( & pb );
            if ( rc != 0 )
                LOGERR ( klogErr, rc, "Failed to initialize internal managers" );
            else
            {
                rc = parse_args ( & pb, args );
                if ( rc != 0 )
                    LOGERR ( klogErr, rc, "Failed to extract command line options" );
                else
                {
                    rc = KLogLevelSet ( klogInfo );
                    if ( rc != 0 )
                        LOGERR ( klogErr, rc, "Failed to set log level" );
                    else
                    {
                        uint32_t i;

                        md5_required = false;

                        STSMSG(2, ("exhaustive = %d", exhaustive));
                        STSMSG(2, ("ref_int_check = %d", ref_int_check));
                        STSMSG(2, ("md5_required = %d", md5_required));
                        STSMSG(2, ("P {"));
                        STSMSG(2, ("\tmd5_chk = %d", pb.md5_chk));
                        STSMSG(2, ("\tmd5_chk_explicit = %d",
                            pb.md5_chk_explicit));
                        STSMSG(2, ("\tblob_crc = %d", pb.blob_crc));
                        STSMSG(2, ("\tconsist_check = %d", pb.consist_check));
                        STSMSG(2, ("}"));
                        for ( i = 0; i < pcount; ++ i )
                        {
                            rc_t rc2;
                            const char *path;
                            rc = ArgsParamValue ( args, i, (const void **)& path );
                            if ( rc != 0 )
                            {
                                LOGERR ( klogErr, rc, "Failed to extract command line options" );
                                break;
                            }

                            rc2 = vdb_validate ( & pb, path );
                            if ( rc == 0 )
                                rc = rc2;
                        }
                    }
                }

                vdb_validate_params_whack ( & pb );
            }
        }
        XMLLogger_Release(xlogger);
    }
    return rc;
}

rc_t CC KMain(int argc, char *argv[])
{
    Args *args = NULL;
    rc_t rc = ArgsMakeAndHandle(&args, argc, argv, 2,
                                options, sizeof(options)/sizeof(options[0]),
                                XMLLogger_Args, XMLLogger_ArgsQty);

    if ( rc != 0 )
        LOGERR ( klogErr, rc, "Failed to parse command line" );
    else
    {
        rc = main_with_args(args);
        ArgsWhack ( args );
    }

    return rc;
}
