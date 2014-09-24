/******************************************************************************/
#include <kapp/main.h> /* KMain */
#include <sra/wsradb.h> /* SRAMgr */
#include <kdb/table.h> /*  KTableRelease */
#include <kxml/xml.h> /* KXMLMgrRelease */
#include <kfs/file.h> /* KFileRelease */
#include <klib/container.h> /* BSTree */
#include <klib/log.h> /* LOGERR */
#include <klib/rc.h> /* RC */
#include <stdlib.h> /* calloc */
#include <string.h> /* memset */
#include <assert.h>
#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)
#define FREE(obj) do { free(obj); obj = NULL; } while (false)
typedef struct SValue {
    uint64_t val;
    bool found;
} SValue;
typedef struct Meta {
    SValue base_count;
    SValue base_count_bio;
    SValue spot_count;
    const char* member_name;
    bool found;
} Meta;
typedef struct MetaMember {
    BSTNode n;
    char* member_name;
    bool checked;
    Meta* meta;
} MetaMember;
typedef struct XmlMeta { /* XML data: Just one of root or tr is present */
    Meta root; /* sra-stat root node attributes */
    BSTree* tr; /* sra-stat spot-group nodes (Member) attributes */
} XmlMeta;
typedef struct SDbMeta { /* DB metadata */
    const Meta root; /* meta root */

    bool stats_found; /* STATS */
    const Meta* table; /* STATS/TABLE */
    const BSTree* tr; /* STATS/SPOT_GROUP */
} SDbMeta;
typedef struct AppPrm {
    const char* path;
} AppPrm;
typedef struct AppCtx {
    SRAMgr* s_mgr;
    const KXMLMgr* x_mgr;
    KDirectory* dir;
} AppCtx;
static rc_t AppCtxInit(rc_t rc, AppCtx* ctx) {
    assert(ctx);
    memset(ctx, 0 , sizeof *ctx);
    if (rc) {
        return rc;
    }
    if (rc == 0) {
        rc = SRAMgrMakeUpdate(&ctx->s_mgr, NULL);
        DISP_RC(rc, "while calling SRAMgrMakeUpdate");
    }
    if (rc == 0) {
        rc = KXMLMgrMakeRead(&ctx->x_mgr);
        DISP_RC(rc, "while calling KXMLMgrMakeRead");
    }
    if (rc == 0) {
        rc = KDirectoryNativeDir(&ctx->dir);
        DISP_RC(rc, "while calling KDirectoryNativeDir");
    }
    return rc;
}
static rc_t AppCtxDestroy(rc_t rc, AppCtx* ctx) {
    assert(ctx);
    RELEASE(KDirectory, ctx->dir);
    RELEASE(KXMLMgr, ctx->x_mgr);
    RELEASE(SRAMgr, ctx->s_mgr);
    return rc;
}
static int CC MetaMemberCmp(const void *item, const BSTNode *n) {
    const char *key = item;
    const MetaMember *a = ( const MetaMember* ) n;
    return strcmp(key, a->member_name);
}
static int CC MetaMemberSort(const BSTNode *item, const BSTNode *n) {
    const MetaMember *a = (const MetaMember*) item;
    return MetaMemberCmp(a->member_name, n);
}
static rc_t s_KXMLNodeReadAttr(rc_t rc, const KXMLNode* node, const char* name,
    SValue *val, bool required)
{
    return 0;
}
static rc_t ParseSraMetaNode(const KXMLNode* node,
    const char* name, const char* member_name, Meta* meta)
{
    rc_t rc = 0;
    bool required = member_name;
    rc = s_KXMLNodeReadAttr
        (rc, node, "base_count", &meta->base_count, required);
    rc = s_KXMLNodeReadAttr
        (rc, node, "base_count_bio", &meta->base_count_bio, required);
    rc = s_KXMLNodeReadAttr
        (rc, node, "spot_count", &meta->spot_count, required);
    if (rc == 0) {
        if (meta->base_count.found != meta->base_count_bio.found ||
            meta->base_count.found != meta->spot_count.found)
        {
            rc = RC(rcExe, rcMetadata, rcReading, rcNode, rcNotFound);
            if (member_name) {
                PLOGERR(klogErr, (klogErr, rc, "One of statistics attributes "
                    "in '$(name)|$(attr)' is missed",
                    "name=%s,attr=%s", name, member_name));
            }
            else {
                PLOGERR(klogErr, (klogErr, rc,
                    "One of statistics attributes in '$(name)' is missed",
                    "name=%s", name));
            }
        }
        else {
            meta->found = meta->base_count.found;
        }
    }
    return rc;
}
static rc_t XmlMetaInitMemser(BSTree* tr,
    const KXMLNode* node, const char* path, const char* member_name)
{
    rc_t rc = 0;
    MetaMember* member
        = (MetaMember*)BSTreeFind(tr, member_name, MetaMemberCmp);
    if (member) {
        rc = RC(rcExe, rcMetadata, rcReading, rcAttr, rcDuplicate);
        PLOGERR(klogErr, (
            klogErr, rc, "Member/@member_name='$(name)'",
            "name=%s", member_name));
    }
    else {
        member = calloc(1, sizeof(*member));
        if (member == NULL) {
            rc = RC(rcExe,
                rcStorage, rcAllocating, rcMemory, rcExhausted);
        }
        if (rc == 0) {
            member->member_name = strdup(member_name);
            if (member == NULL) {
                rc = RC(rcExe,
                    rcStorage, rcAllocating, rcMemory, rcExhausted);
            }
        }
        if (rc == 0) {
            member->meta = calloc(1, sizeof(*member->meta));
            if (member->meta == NULL) {
                rc = RC(rcExe,
                    rcStorage, rcAllocating, rcMemory, rcExhausted);
            }
        }
        if (rc == 0) {
            rc = ParseSraMetaNode(node, path, member_name, member->meta);
        }
        if (rc) {
            if (member) {
                FREE(member->member_name);
                FREE(member->meta);
                FREE(member);
            }
        }
        else {
            BSTreeInsert(tr, (BSTNode*)member, MetaMemberSort);
        }
    }
    return rc;
}
static rc_t XmlMetaInitMembers(BSTree* tr, const KXMLNode* node) {
    rc_t rc = 0;
    const char path[] = "Member";
    uint32_t count = 0;
    uint32_t idx = 0;
    const KXMLNodeset *members = NULL;
    if (rc == 0) {
        rc = KXMLNodeOpenNodesetRead(node, &members, path);
        DISP_RC(rc, path);
    }
    if (rc == 0) {
        rc = KXMLNodesetCount(members, &count);
        DISP_RC(rc, path);
    }
    for (idx = 0; idx < count && rc == 0; ++idx) {
        const KXMLNode *node = NULL;
        char member_name[256] = "";
        if (rc == 0) {
            rc = KXMLNodesetGetNodeRead(members, &node, idx);
            DISP_RC(rc, path);
        }
        if (rc == 0) {
            size_t size = 0;
            rc = KXMLNodeReadAttrCString(node, "member_name",
                member_name, sizeof member_name, &size);
            if (rc) {
                if (GetRCState(rc) == rcInsufficient) {
                    member_name[sizeof member_name - 1] = '\0';
                    rc = 0;
                }
            }
        }
        if (rc == 0) {
            rc = XmlMetaInitMemser(tr, node, path, member_name);
        }
        RELEASE(KXMLNode, node);
    }
    RELEASE(KXMLNodeset, members);
    return rc;
}
static rc_t XmlMetaInit(rc_t rc, XmlMeta* meta,
    const AppCtx* ctx, const char* xml)
{
    const KFile* f = NULL;
    const KXMLDoc* doc = NULL;
    const KXMLNodeset* ns = NULL;
    const KXMLNode* node = NULL;
    const char path[] = "/Run";
    memset(meta, 0, sizeof *meta);
    if (rc) {
        return rc;
    }
    if (rc == 0) {
        rc = KDirectoryOpenFileRead(ctx->dir, &f, "%s", xml);
        DISP_RC(rc, xml);
    }
    if (rc == 0) {
        rc = KXMLMgrMakeDocRead(ctx->x_mgr, &doc, f);
        DISP_RC(rc, xml);
    }
    if (rc == 0) {
        rc = KXMLDocOpenNodesetRead(doc, &ns, path);
        DISP_RC(rc, path);
    }
    if (rc == 0) {
        rc = KXMLNodesetGetNodeRead(ns, &node, 0);
        DISP_RC(rc, path);
    }
    if (rc == 0) {
        rc = ParseSraMetaNode(node, path, NULL, &meta->root);
    }
    if (rc == 0 && !meta->root.found) {
        meta->tr = calloc(1, sizeof *meta->tr);
        if (meta->tr == NULL) {
            rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
        }
        else {
            BSTreeInit(meta->tr);
            rc = XmlMetaInitMembers(meta->tr, node);
        }
    }
    RELEASE(KXMLNode, node);
    RELEASE(KXMLNodeset, ns);
    RELEASE(KXMLDoc, doc);
    RELEASE(KFile, f);
    return rc;
}
static rc_t XmlMetaDestroy(rc_t rc, const XmlMeta* meta) {
    return rc;
}
static int Size(const BSTree* tr) { return 0; }
static bool MetaEqual(const Meta* lhs, const Meta* rhs) { return false; }
static bool Equal(const Meta* xml, const BSTree* tr, const char* key)
{ return false; }
static bool BSTreeEqual(const BSTree* lhs, const BSTree* rhs) { return false; }
static rc_t CheckMeta(const XmlMeta* xml, const SDbMeta* db, bool* found) {
    rc_t rc = RC(rcExe, rcMetadata, rcReading, rcMetadata, rcUnequal);
    assert(xml && db && found);
    *found = false;
    if (xml->root.found) { /* has statistics in root node: no spot-groups */
        if (Size(xml->tr) > 0) { /* should have no Member nodes */
            rc = RC(rcExe, rcXmlDoc, rcReading, rcXmlDoc, rcInvalid);
        }
        else if (db->stats_found) { /* has STATS */
            if (!MetaEqual(&xml->root, db->table)) {
                /* xml: Run != STATS/TABLE */
                rc = RC(rcExe, rcMetadata, rcReading, rcMetadata, rcUnequal);
            }
            else if (!Equal(&xml->root, db->tr, "default")) {
                /* xml: Run != STATS/SPOT_GROUP/default */
                rc = RC(rcExe, rcMetadata, rcReading, rcMetadata, rcUnequal);
            }
            else if (Size(db->tr) != 1) {
                /* STATS/SPOT_GROUP should have just default subnode */
                rc = RC(rcExe, rcMetadata, rcReading, rcMetadata, rcInvalid);
            } else {
                *found = true;
                rc = 0;
            }
        }
        else { /* no STATS: check with meta in root */
            if (!db->root.found) { /* no root meta */
                rc = 0;
            }
            else if (!MetaEqual(&xml->root, &db->root)) {
                rc = RC(rcExe, rcMetadata, rcReading, rcMetadata, rcUnequal);
            }
            else {
                *found = true;
                rc = 0;
            }
        }
    }
    else { /* sra-stat xml has spotgroups info */
        if (db->root.found) { /* unknown: could it have root meta nodes? */
            rc = RC(rcExe, rcMetadata, rcReading, rcMetadata, rcInvalid);
        }
        else if (Size(db->tr) > 0) { /* exist STATS/SPOT_GROUP/... */
            if (!BSTreeEqual(xml->tr, db->tr)) {
                rc = RC(rcExe, rcMetadata, rcReading, rcMetadata, rcUnequal);
            }
            /* could add Equal(sum(xml->tr), meta->table) */
            else {
                *found = true;
                rc = 0;
            }
        }
        else { /* no statistics meta */
            rc = 0;
        }
    }
    return rc;
}
static rc_t fix_run_stat(const XmlMeta* xml) {
    rc_t rc = 0;
    bool found = false;
    const SRATable *tbl = NULL;
    SDbMeta db;
    memset(&db, 0, sizeof db);
    rc = CheckMeta(xml, &db, &found);
    RELEASE(SRATable, tbl);
    return rc;
}
const char UsageDefaultName[] = "fix-run-stat";
rc_t CC UsageSummary (const char * prog_name) { return 0; }
rc_t CC Usage ( const Args * args ) { return 0; }
ver_t CC KAppVersion ( void ) { return 0x01000000; }
rc_t KMain(int argc, char *argv []) {
    rc_t rc = 0;
    AppCtx ctx;
    XmlMeta meta;
    rc = AppCtxInit(rc, &ctx);
    rc = XmlMetaInit(rc,
        &meta, &ctx, "/home/klymenka/fix-run-stat/SRR331456/SRR331456.xml");
    if (rc == 0) {
        rc = fix_run_stat(&meta);
    }
    rc = XmlMetaDestroy(rc, &meta);
    rc = AppCtxDestroy(rc, &ctx);
    return rc;
}
