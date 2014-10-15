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
#include <klib/namelist.h>
#include <klib/container.h>
#include <klib/printf.h>
#include <klib/refcount.h>
#include <kxml/xml.h>
#include <kdb/namelist.h>
#include <kproc/thread.h>
#include <kproc/lock.h>
#include <kproc/cond.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>

#include <sra/sradb-priv.h>
#include <sra/impl.h>

#include "log.h"
#include "xml.h"
#include "sra-list.h"
#include "sra-directory.h"
#include "sra-node.h"
#include "formats.h"

#include <stdlib.h>
#include <strtol.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static KRWLock* g_lock = NULL;
static BSTree g_list;
static uint64_t g_version = 0;
static const SRAMgr* g_sra_mgr = NULL;
/* hack to prevent opentable collapse */
static KLock* g_sra_mgr_lock = NULL;
/* SRA tables states cache file for quick restart */
static char* g_cache_file = NULL;
/* tmp name for cache file */
static char* g_cache_file_tmp = NULL;
static const uint32_t g_cache_version = 4;

static unsigned int g_sync = 0;
static KThread* g_thread = NULL;
/*async update queue and thread */
static SLList g_queue;
static volatile uint64_t g_queue_depth;
static KCondition* g_queue_cond;
static KThread* g_queue_thread = NULL;
static KLock* g_queue_lock = NULL;
static pthread_mutex_t g_refresh_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_refresh_cond = PTHREAD_COND_INITIALIZER;

struct SRAListNode {
    BSTNode dad;
    atomic32_t refcount;
    char* accession;
    char* path;
    FileOptions* files;
    uint32_t files_count;
    KTime_t mtime; /* table mtime */
    KTime_t utime; /* node update time */
    uint32_t version; /* source xml version */
};

typedef struct SRAQueueNode_struct {
    SLNode dad;
    SRAListNode* node;
    KTime_t mtime; /* original node update time */
} SRAQueueNode;

void CC SRAQueue_Whack( SLNode *n, void *data )
{
    if( n != NULL ) {
        FREE(n);
    }
}

static
rc_t SRAList_Queue_Lock(void)
{
    DEBUG_LINE(8, "Lock SRA %s", "queue");
    return KLockAcquire(g_queue_lock);
}

static
void SRAList_Queue_Unlock(void)
{
    DEBUG_LINE(8, "Unlock SRA %s", "queue");
    ReleaseComplain(KLockUnlock, g_queue_lock);
}

static
rc_t SRAList_Lock(bool exclusive)
{
    DEBUG_LINE(8, "Lock SRA list %s", exclusive ? "write" : "read");
    return exclusive ? KRWLockAcquireExcl(g_lock) : KRWLockAcquireShared(g_lock);
}

static
void SRAList_Unlock(void)
{
    DEBUG_LINE(8, "Unlocked SRA list", "");
    ReleaseComplain(KRWLockUnlock, g_lock);
}

static
rc_t SRAList_Mgr(const SRAMgr** mgr)
{
    rc_t rc = 0;
    if( g_sra_mgr_lock == NULL && (rc = KLockMake(&g_sra_mgr_lock)) != 0 ) {
        g_sra_mgr_lock = NULL;
        LOGERR(klogErr, rc, "SRA manager lock");
    } else if( g_sra_mgr == NULL ) {
        PLOGMSG(klogInfo, (klogInfo, "VDB_CONFIG=$(var)", PLOG_S(var), getenv("VDB_CONFIG")));
        if( (rc = SRAMgrMakeRead(&g_sra_mgr)) != 0 ) {
            g_sra_mgr = NULL;
            LOGERR(klogErr, rc, "SRA manager");
        } else {
            DEBUG_LINE(8, "SRA manager created 0x%p", g_sra_mgr);
        }
    }
    if( rc == 0 ) {
        *mgr = g_sra_mgr;
    }
    return rc;
}

rc_t SRAListNode_TableMTime(const SRAListNode* cself, KTime_t* mtime)
{
    rc_t rc = 0;

    if( cself == NULL || mtime == NULL ) {
        rc = RC(rcExe, rcTable, rcResolving, rcParam, rcNull);
    } else {
        const SRAMgr* mgr;
        DEBUG_LINE(10, "%s path '%s'", cself->accession, cself->path);
        if( (rc = SRAList_Mgr(&mgr)) == 0 ) {
            rc = SRAMgrGetTableModDate(mgr, mtime, "%s", cself->path ? cself->path : cself->accession);
        }
    }
    return rc;
}

rc_t SRAListNode_TableOpen(const SRAListNode* cself, const SRATable** tbl)
{
    rc_t rc = 0;

    if( cself == NULL || tbl == NULL ) {
        rc = RC(rcExe, rcTable, rcOpening, rcParam, rcNull);
    } else {
        const SRAMgr* mgr;
        DEBUG_LINE(10, "%s path '%s'", cself->accession, cself->path);
        if( (rc = SRAList_Mgr(&mgr)) == 0 && (rc = KLockAcquire(g_sra_mgr_lock)) == 0 ) {
            KLogLevel lvl = KLogLevelGet();
            KLogLevelSet(klogInfo - 1);
            rc = SRAMgrOpenTableRead(mgr, tbl, "%s", cself->path ? cself->path : cself->accession);
            KLogLevelSet(lvl);
            ReleaseComplain(KLockUnlock, g_sra_mgr_lock);
        }
    }
    return rc;
}

static
rc_t SRAListNode_KConfigReload(void)
{
    rc_t rc = 0;

    if( BSTreeFirst(&g_list) != NULL ) {
        const SRAMgr* mgr;
        if( (rc = SRAList_Mgr(&mgr)) == 0 && (rc = KLockAcquire(g_sra_mgr_lock)) == 0 ) {
            DEBUG_LINE(10, "VDB_CONFIG=%s", getenv("VDB_CONFIG"));
            rc = SRAMgrConfigReload(mgr, NULL);
            ReleaseComplain(KLockUnlock, g_sra_mgr_lock);
            if( rc != 0 ) {
                PLOGERR(klogErr, (klogErr, rc, "SRA config reload with VDB_CONFIG=$(var)", PLOG_S(var), getenv("VDB_CONFIG")));
            }
        }
    }
    return rc;
}

rc_t SRAListNode_AddRef(const SRAListNode* cself)
{
    if( cself == NULL ) {
        return RC(rcExe, rcTable, rcAttaching, rcSelf, rcNull);
    }
    atomic32_add(&((SRAListNode*)cself)->refcount, 1);
    return 0;
}

void SRAListNode_Release(const SRAListNode* cself)
{
    SRAListNode* self = (SRAListNode*)cself;
    if( self != NULL && atomic32_dec_and_test(&self->refcount) ) {
        DEBUG_LINE(10, "%s: %s", self->accession, self->path);
        FREE(self->accession);
        FREE(self->path);
        FREE(self->files);
        FREE(self);
    }
}

static
void SRAList_Whack(BSTNode* node, void* data)
{
    SRAListNode_Release((SRAListNode*)node);
}

static
void SRAListNode_Version(BSTNode* node, void* data)
{
    SRAListNode* n = (SRAListNode*)node;
    uint32_t* qty = (uint32_t*)data;

    if( n->version < g_version ) {
        BSTreeUnlink(&g_list, node);
        SRAListNode_Release(n);
        *qty = *qty + 1;
    }
}

rc_t SRAList_NextVersion(void)
{
    rc_t rc = 0;
    uint32_t qty = 0;

    DEBUG_LINE(8, "SRA setting version to %u", g_version + 1);
    if( (rc = SRAList_Lock(true)) == 0 ) {
        g_version++;
        BSTreeForEach(&g_list, false, SRAListNode_Version, &qty);
        SRAList_Unlock();
        DEBUG_LINE(8, "SRA version set to %u, %u runs dropped", g_version, qty);
    }
    return rc;
}

typedef struct SRAListNode_FindData_struct {
    char* accession;
    char* path;
    KTime_t timestamp;
    struct {
        int type;
        const char* ext;
        uint64_t size;
        char md5[32 + 1];
    } attr[2];
} SRAListNode_FindData;

static
int SRAListNode_Cmp(const SRAListNode_FindData* l, const SRAListNode* r)
{
    int d = strcmp(l->accession, r->accession);
    if( d == 0 && l->path != r->path ) {
        if( l->path == NULL && r->path != NULL ) {
            d = -1024;
        } else if( l->path != NULL && r->path == NULL ) {
            d = 1024;
        } else {
            d = strcmp(l->path, r->path);
        }
    }
    return d;
}

static
int SRAListNode_Find(const void* item, const BSTNode* node)
{
    return SRAListNode_Cmp((SRAListNode_FindData*)item, (SRAListNode*)node);
}

static
int SRAListNode_Sort(const BSTNode *item, const BSTNode *node)
{
    SRAListNode* i = (SRAListNode*)item;
    SRAListNode_FindData data;

    data.accession = i->accession;
    data.path = i->path;

    return SRAListNode_Cmp(&data, (SRAListNode*)node);
}

static
rc_t SRAListNode_Insert(SRAListNode_FindData* data, uint64_t version, SRAListNode** found)
{
    rc_t rc = 0;

    *found = NULL; 
    if( (rc = SRAList_Lock(true)) == 0 ) {
        *found = (SRAListNode*)BSTreeFind(&g_list, data, SRAListNode_Find);
        if( *found != NULL ) {
            DEBUG_LINE(8, "found SRA list node %u. %s, %s", (*found)->version, data->accession, data->path);
            atomic32_add(&(*found)->refcount, 1); /* for here */
            if( version > (*found)->version ) {
                (*found)->version = version;
            }
        } else {
            CALLOC(*found, 1, sizeof(**found));
            if( *found == NULL ) {
                rc = RC(rcExe, rcTable, rcInserting, rcMemory, rcExhausted);
            } else {
                KRefcountInit(&((*found)->refcount), 1, "SRAListNode", "Make", "");
                (*found)->accession = data->accession;
                (*found)->path = data->path;
                atomic32_set(&(*found)->refcount, 2); /* one for the list and 1 for here */
                (*found)->version = version;
                DEBUG_LINE(8, "SRA list new node %u. %s, %s", (*found)->version, (*found)->accession, (*found)->path);
                BSTreeInsert(&g_list, &(*found)->dad, SRAListNode_Sort);
                data->accession = NULL;
                data->path = NULL;
            }
        }
        if( rc == 0 && data->timestamp != 0 && (*found)->mtime != data->timestamp ) {
            if( (*found)->files_count > 0 ) {
                uint32_t i, j;
                for(j = 0; rc == 0 && j < (*found)->files_count; j++) {
                    for(i = 0; rc == 0 && i < sizeof(data->attr) / sizeof(data->attr[0]); i++) {
                        if( (*found)->files[j].type == data->attr[i].type ) {
                            if( (rc = FileOptions_SRAArchiveUpdate(&(*found)->files[j], (*found)->accession,
                                data->timestamp, data->attr[i].size, data->attr[i].md5)) == 0 ) {
                                    DEBUG_LINE(10, "updated %s %lu %s", (*found)->accession,
                                        data->attr[i].size, data->attr[i].md5);
                            }
                        }
                    }
                }
            } else {
                uint32_t i;
                const SRAMgr* mgr;

                (*found)->files_count = sizeof(data->attr) / sizeof(data->attr[0]) * 2;
                if( (rc = FileOptions_Make(&(*found)->files, (*found)->files_count)) == 0 &&
                    (rc = SRAList_Mgr(&mgr)) == 0 ) {
                    for(i = 0; rc == 0 && i < sizeof(data->attr) / sizeof(data->attr[0]); i++) {
                        if( (rc = FileOptions_SRAArchiveInstant(&(*found)->files[i * 2], &(*found)->files[i * 2 + 1],
                           mgr, (*found)->accession, (*found)->path,
                           data->attr[i].type == eSRAFuseFmtArcLite, data->timestamp, data->attr[i].size, data->attr[i].md5)) == 0 ) {
                            DEBUG_LINE(10, "added %s %lu %s", (*found)->accession, data->attr[i].size, data->attr[i].md5);
                        }
                    }
                }
            }
            if( rc == 0 ) {
                (*found)->mtime = data->timestamp;
                DEBUG_LINE(10, "set %s timestamp %lu", (*found)->accession, (*found)->mtime);
            } else if( data->accession == NULL ) {
                FREE(*found);
            }
        }
        SRAList_Unlock();
    }
    return rc;
}

static
rc_t SRAListNode_MakeXML(const KXMLNode* xml_node, SRAListNode_FindData* data, char* errmsg, const char* rel_path, EXMLValidate validate)
{
    rc_t rc = 0;

    if( xml_node == NULL || data == NULL ) {
        rc = RC(rcExe, rcTable, rcConstructing, rcParam, rcNull);
    } else {
        uint32_t i, has_attrs = 0;
        if( (rc = KXMLNodeReadAttrCStr(xml_node, "accession", &data->accession, NULL)) != 0 || data->accession[0] == '\0') {
            strcpy(errmsg, "attribute 'accession'");
            rc = rc ? rc : RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
        }
        if( rc == 0 ) {
            data->timestamp = 0;
            if( (rc = XML_ParseTimestamp(xml_node, "timestamp", &data->timestamp, true)) != 0 ) {
                strcpy(errmsg, "attribute 'timestamp'");
            } else if( data->timestamp != 0 ) {
                has_attrs++;
            }
        }
        for(i = 0; rc == 0 && i < sizeof(data->attr) / sizeof(data->attr[0]); i++) {
            size_t num_read;
            char b[128];

            if( (rc = string_printf(b, sizeof(b) - 1, NULL, "md5%s", data->attr[i].ext)) == 0 ) {
                if( (rc = KXMLNodeReadAttrCString(xml_node, b, data->attr[i].md5, sizeof(data->attr[i].md5), &num_read)) == 0 ) {
                    has_attrs++;
                    if( num_read != sizeof(data->attr[i].md5) - 1) {
                        rc = RC(rcExe, rcDoc, rcValidating, rcChecksum, rcInvalid);
                    }
                } else if( GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound ) {
                    rc = 0;
                }
            }
            if( rc != 0 ) {
                strcpy(errmsg, "attribute 'md5");
                strcat(errmsg, data->attr[i].ext);
                strcat(errmsg, "'");
            } else {
                if( (rc = string_printf(b, sizeof(b) - 1, NULL, "size%s", data->attr[i].ext)) == 0 ) {
                    if( (rc = KXMLNodeReadAttrCString(xml_node, b, b, sizeof(b), &num_read)) == 0 ) {
                        char* end;
                        has_attrs++;
                        data->attr[i].size = strtou64(b, &end, 10);
                        if( end - b != num_read ) {
                            rc = RC(rcExe, rcDoc, rcValidating, rcSize, rcInvalid);
                        }
                    } else if( GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound ) {
                        rc = 0;
                    }
                }
                if( rc != 0 ) {
                    strcpy(errmsg, "attribute 'size");
                    strcat(errmsg, data->attr[i].ext);
                    strcat(errmsg, "'");
                }
            }
        }
        if( rc == 0 && has_attrs != 0 && has_attrs != 3 && has_attrs != 5 ) {
            if( validate > eXML_NoFail ) {
                rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
            } else {
                data->timestamp = 0;
                PLOGMSG(klogErr, (klogErr, "SRA node $(a) attributes incomplete", "a=%s", data->accession));
            }
        }
        if( rc == 0 ) {
            if( (rc = KXMLNodeReadAttrCStr(xml_node, "path", &data->path, "")) == 0 ) {
                if( data->path[0] == '\0' ) {
                    free(data->path);
                    data->path = NULL;
                } else {
                    KDirectory* dir = NULL;
                    if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
                        char resolved[4096];
                        size_t num_writ;
                        if( data->path[0] == '/' ) {
                            rc = string_printf(resolved, sizeof(resolved), &num_writ, "%s", data->path);
                        } else if( (rc = KDirectoryResolvePath(dir, true, resolved, sizeof(resolved),
                                                                      "%s%s", rel_path, data->path)) == 0 ) {
                            DEBUG_LINE(8, "%s%s resolved to %s", rel_path, data->path, resolved);
                        }
                        if( rc == 0 && validate > eXML_NoCheck ) {
                            uint32_t typ = KDirectoryPathType(dir, resolved);
                            if( typ != kptDir && typ != (kptDir | kptAlias) &&
                                typ != kptFile && typ != (kptFile | kptAlias)) {
                                if( validate > eXML_NoFail ) {
                                    rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, typ == kptNotFound ? rcNotFound : rcInvalid);
                                } else {
                                    PLOGMSG(klogErr, (klogErr, "SRA path '$(p)' not found", "p=%s", resolved));
                                }
                            }
                        }
                        if( rc == 0 ) {
                            free(data->path);
                            rc = StrDup(resolved, &data->path);
                        }
                        ReleaseComplain(KDirectoryRelease, dir);
                    }
                }
            }
        }
        if( rc == 0 ) {
            struct KNamelist const* attr = NULL;
            if( (rc = KXMLNodeListAttr(xml_node, &attr)) == 0 ) {
                uint32_t i = 0, count = 0;
                if( (rc = KNamelistCount(attr, &count)) == 0 && count > 0 ) {
                    while( rc == 0 && i < count ) {
                        const char *attr_nm = NULL;
                        if( (rc = KNamelistGet(attr, i++, &attr_nm)) != 0 ) {
                            break;
                        }
                        if( strcmp("path", attr_nm) == 0 || strcmp("accession", attr_nm) == 0 ||
                            strcmp("timestamp", attr_nm) == 0 ||
                            strncmp("md5.", attr_nm, 4) == 0 || strncmp("size.", attr_nm, 5) == 0 ) {
                            continue;
                        }
                        rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
                        strcpy(errmsg, "unknown attribute '");
                        strcat(errmsg, attr_nm);
                        strcat(errmsg, "'");
                    }
                }
                ReleaseComplain(KNamelistRelease, attr);
            }
        }
    }
    return rc;
}

rc_t SRAListNode_Make(const KXMLNode* xml_node, FSNode* parent, SRAConfigFlags flags, char* errmsg,
                      const char* rel_path, EXMLValidate validate)
{
    rc_t rc = 0;

    if( xml_node == NULL || parent == NULL || errmsg == NULL || rel_path == NULL ) {
        rc = RC(rcExe, rcTable, rcUpdating, rcParam, rcNull);
    } else {
        SRAListNode_FindData data;
        memset(&data, 0, sizeof(data));
        data.attr[0].ext = ".sra";
        data.attr[0].type = eSRAFuseFmtArc;
        data.attr[1].ext = ".lite.sra";
        data.attr[1].type = eSRAFuseFmtArcLite;
        if( (rc = SRAListNode_MakeXML(xml_node, &data, errmsg, rel_path, validate)) == 0 ) {
            SRAListNode* found;
            if( (rc = SRAListNode_Insert(&data, g_version + 1, &found)) == 0 ) {
                FSNode* d = parent;
                if( flags & eSRAFuseRunDir ) {
                    if( (rc = SRADirectoryNode_Make(&d, found->accession, found)) == 0 ) {
                        if( (rc = FSNode_AddChild(parent, d)) != 0 ) {
                            FSNode_Release(d);
                        }
                    }
                }
                if( rc == 0 ) {
                    rc = SRANode_Make(d, found->accession, found, flags);
                }
            }
            SRAListNode_Release(found);
        }
        free(data.accession);
        free(data.path);
    }
    return rc;
}

static
rc_t SRAListNode_Update(SRAListNode* self, KTime_t now)
{
    rc_t rc = 0;
    KTime_t ts = 0;
    /* weekly forced update */
    bool forced = now != 0 && (now - self->utime) > (60*24*60*60) && self->utime != 0 /* not new */;

    if( (self->mtime == 0 || now != 0) && (rc = SRAListNode_TableMTime(self, &ts)) == 0 ) {
        if( self->mtime != ts || forced ) {
            const SRATable* tbl = NULL;
            FileOptions* new_files = NULL, *old_files = NULL;
            uint32_t new_files_count = 0;
            PLOGMSG(klogInfo, (klogInfo, "Updating sra node, self->mtime = $(mtime), self->utime = $(utime), ts = $(ts), now = $(now), forced = $(forced)",
                        PLOG_5(PLOG_I64(mtime), PLOG_I64(utime), PLOG_I64(ts), PLOG_I32(forced), PLOG_I64(now)),
                        (long long)self->mtime,
                        (long long)self->utime,
                        (long long)ts,
                        (int)forced,
                        (long long)now)
                    );
            if( (rc = SRAListNode_TableOpen(self, &tbl)) == 0 ) {
                const KMDataNode* meta = NULL;
                if( SRATableOpenMDataNodeRead(tbl, &meta, "/FUSE/root/File") != 0 ) {
                    rc = SRATableOpenMDataNodeRead(tbl, &meta, "/FUSE");
                }
                if( rc == 0 ) {
                    struct KNamelist* files = NULL;
                    DEBUG_LINE(10, "Opened SRA table meta %s '%s'", self->accession, self->path);
                    if( (rc = KMDataNodeListChild(meta, &files)) == 0 ) {
                        uint32_t files_count = 0;
                        if( (rc = KNamelistCount(files, &files_count)) == 0 && files_count > 0 ) {
                            new_files_count = (2 + files_count) * 2; /* (2 are for .sra types + count in meta) + md5 for each in list */
                            if( (rc = FileOptions_Make(&new_files, new_files_count)) == 0 ) {
                                uint32_t f = 0;
                                new_files_count = 4; /* step down for 2 .sra + md5 for them */
                                while( rc == 0 && f < files_count ) {
                                    const char *suffix = NULL;
                                    if( (rc = KNamelistGet(files, f, &suffix)) == 0 ) {
                                        const KMDataNode* fn = NULL;
                                        if( (rc = KMDataNodeOpenNodeRead(meta, &fn, suffix)) == 0 ) {
                                            DEBUG_LINE(10, "Adding %s file type '%s'", self->accession, suffix);
                                            if( (rc = FileOptions_ParseMeta(&new_files[new_files_count], fn, tbl, ts, suffix)) != 0 ) {
                                                PLOGERR(klogErr, (klogErr, rc, " node '$(f)'", PLOG_S(f), suffix));
                                                rc = 0;
                                            } else {
                                                if( new_files[new_files_count].md5[0] != '\0' ) {
                                                    if( (rc = FileOptions_AttachMD5(&new_files[new_files_count],
                                                                self->accession, &new_files[new_files_count + 1])) != 0 ||
                                                        (rc = FileOptions_UpdateMD5(&new_files[new_files_count], self->accession)) != 0) {
                                                        PLOGERR(klogErr, (klogErr, rc, " node md5 '$(f)'", PLOG_S(f), suffix));
                                                        rc = 0;
                                                    } else {
                                                        new_files_count++;
                                                    }
                                                }
                                                new_files_count++;
                                            }
                                        }
                                        f++;
                                        ReleaseComplain(KMDataNodeRelease, fn);
                                    }
                                }
                            }
                        }
                        ReleaseComplain(KNamelistRelease, files);
                    }
                    ReleaseComplain(KMDataNodeRelease, meta);
                } else {
                    PLOGMSG(klogWarn, (klogWarn, "FUSE meta block not found in '$(t)'", PLOG_S(t), self->accession));
                    rc = 0;
                }
                if( rc == 0 ) {
                    if( new_files_count == 0 ) {
                        /* for 2 .sra types + its .md5's */
                        new_files_count = 4;
                        rc = FileOptions_Make(&new_files, new_files_count);
                    }
                    if( rc == 0 ) {
                        /* add information for sra archives */
                        DEBUG_LINE(10, "Adding SRA archive type %s and its .md5", self->accession);
                        if( (rc = FileOptions_SRAArchive(&new_files[0], tbl, ts, false)) == 0 &&
                            (rc = FileOptions_AttachMD5(&new_files[0], self->accession, &new_files[1])) == 0 ) {
                            DEBUG_LINE(10, "Adding SRA lite archive type %s and its .md5", self->accession);
                            if( (rc = FileOptions_SRAArchive(&new_files[2], tbl, ts, true)) == 0 ) {
                                rc = FileOptions_AttachMD5(&new_files[2], self->accession, &new_files[3]);
                            }
                        }
                    }
                }
                ReleaseComplain(SRATableRelease, tbl);
            }
            old_files = new_files;
            if( rc == 0 ) {
                SRAQueueNode* q;
                MALLOC(q, sizeof(*q));
                if( q == NULL ) {
                    rc = RC(rcExe, rcTable, rcUpdating, rcMemory, rcExhausted);
                } else if( (rc = SRAList_Lock(true)) == 0 ) {
                    if( self->mtime != ts || forced ) {
                        self->mtime = ts;
                        old_files = self->files;
                        self->files = new_files;
                        self->files_count = new_files_count;
#if 0
                        if( self->utime != 0 /* not new */ && g_queue_cond != NULL && (rc = SRAList_Queue_Lock()) == 0 ) {
                            SRAListNode_AddRef(self);
                            q->node = self;
                            q->mtime = self->mtime;
                            SLListPushTail(&g_queue, &q->dad);
                            g_queue_depth++;
                            KConditionSignal(g_queue_cond);
                            SRAList_Queue_Unlock();
                            q = NULL;
                            DEBUG_LINE(10, "%s table queued for async update, queue %lu", self->accession, g_queue_depth);
                        }
#endif
                        self->utime = now ? now : time(NULL);
                    }
                    SRAList_Unlock();
                    DEBUG_LINE(10, "%s table updated %lu %lu", self->accession, self->mtime, self->utime);
                }
                FREE(q);
            }
            FileOptions_Release(old_files);
        } else {
            DEBUG_LINE(10, "%s table is up-to-date: %lu, updated %lu, now %lu",
                           self->accession, self->mtime, self->utime, time(NULL));
            rc = RC(rcExe, rcTable, rcUpdating, rcMessage, rcCanceled);
        }
    }
    if( rc != 0 && GetRCState(rc) != rcCanceled ) {
        PLOGERR(klogErr, (klogErr, rc, "SRA refresh $(a)", PLOG_S(a), self->accession));
    }
    return rc;
}

typedef struct SRAListNode_UpdateData_struct {
    rc_t rc;
    KTime_t now;
    KFile* file;
    uint64_t pos;
    uint32_t qty, recs;
} SRAListNode_UpdateData;

static
bool SRAListNode_Updater(BSTNode* node, void* data)
{
    SRAListNode* n = (SRAListNode*)node;
    SRAListNode_UpdateData* d = (SRAListNode_UpdateData*)data;

    if( n->version >= g_version ) {
        if( SRAListNode_Update(n, d->now) == 0 ) {
            d->qty = d->qty + 1;
        }
    }
    if( d->file != NULL ) {
        size_t num_writ;
        uint16_t acc_sz = n->accession ? strlen(n->accession) : 0;
        uint16_t path_sz = n->path ? strlen(n->path) : 0;

        if( d->recs++ == 0 ) {
            if( (d->rc = KFileWrite(d->file, d->pos, &g_cache_version, sizeof(g_cache_version), &num_writ)) == 0 ) {
                d->pos += num_writ;
            }
        }
        if( (d->rc = KFileWrite(d->file, d->pos, &acc_sz, sizeof(acc_sz), &num_writ)) == 0 ) {
            d->pos += num_writ;
            if( (d->rc = KFileWrite(d->file, d->pos, n->accession, acc_sz, &num_writ)) == 0 ) {
                d->pos += num_writ;
            }
        }
        if( (d->rc = KFileWrite(d->file, d->pos, &path_sz, sizeof(path_sz), &num_writ)) == 0 ) {
            d->pos += num_writ;
            if( (d->rc = KFileWrite(d->file, d->pos, n->path, path_sz, &num_writ)) == 0 ) {
                d->pos += num_writ;
            }
        }
        if( (d->rc = KFileWrite(d->file, d->pos, &n->files_count, sizeof(n->files_count), &num_writ)) == 0 ) {
            d->pos += num_writ;
            if( n->files_count > 0 &&
                (d->rc = KFileWrite(d->file, d->pos, n->files, n->files_count * sizeof(*(n->files)), &num_writ)) == 0 ) {
                d->pos += num_writ;
            }
        }
        if( (d->rc = KFileWrite(d->file, d->pos, &n->mtime, sizeof(n->mtime), &num_writ)) == 0 ) {
            d->pos += num_writ;
        }
        if( (d->rc = KFileWrite(d->file, d->pos, &n->utime, sizeof(n->utime), &num_writ)) == 0 ) {
            d->pos += num_writ;
        }
        /* a'la validation: write record id */
        if( (d->rc = KFileWrite(d->file, d->pos, &d->recs, sizeof(d->recs), &num_writ)) == 0 ) {
            d->pos += num_writ;
        }
    }
    return g_sync == 0;
}

void SRAList_PostRefresh()
{
    if (pthread_cond_signal(&g_refresh_cond) == -1)
        assert(0);
}

static
rc_t SRAList_Thread(const KThread *self, void *data)
{

    PLOGMSG(klogInfo, (klogInfo, "SRA sync thread started with $(s) sec", PLOG_U32(s), g_sync));
    while( g_sync > 0 ) {
        KDirectory* dir = NULL;
        SRAListNode_UpdateData data;
        struct timespec timeout = {0};
        if (clock_gettime(CLOCK_REALTIME, &timeout) == -1)
            assert(0);
        timeout.tv_sec += g_sync;
        if (pthread_mutex_lock(&g_refresh_mutex) != 0)
            assert(0);
        switch (pthread_cond_timedwait(&g_refresh_cond, &g_refresh_mutex, &timeout))
        {
            case 0:
            case ETIMEDOUT:
                break;
            default:
                assert(0);
        }
        if (pthread_mutex_unlock(&g_refresh_mutex) != 0)
            assert(0);
        LOGMSG(klogInfo, "Begin refreshing sra list");
        if( g_lock == NULL ) {
            break;
        }
        memset(&data, 0, sizeof(data));
        data.now = time(NULL);
        DEBUG_LINE(10, "SRA sync thread with %u sec, updating version %u @ %lu", g_sync, g_version, data.now);
        if( g_cache_file != NULL ) {
            DEBUG_LINE(10, "SRA sync thread writing cache file %s", g_cache_file_tmp);
            if( (data.rc = KDirectoryNativeDir(&dir)) == 0 ) {
                data.rc = KDirectoryCreateFile(dir, &data.file, false, 0644, kcmInit, "%s", g_cache_file_tmp);
            }
            if( data.rc != 0 ) {
                data.file = NULL;
                PLOGERR(klogErr, (klogErr, data.rc, "SRA cache file $(s)", PLOG_S(s), g_cache_file_tmp));
            }
        }
        SRAListNode_KConfigReload();
        /* scan in reverse to avoid reading threads following update */
        BSTreeDoUntil(&g_list, true, SRAListNode_Updater, &data);
        if( data.qty > 0 ) {
            PLOGMSG(klogInfo, (klogInfo, "SRA sync updated $(q) runs", PLOG_U32(q), data.qty));
        } else {
            DEBUG_LINE(10, "SRA sync updated %u runs", data.qty);
        }
        if( data.file != NULL ) {
            rc_t rc;
            ReleaseComplain(KFileRelease, data.file);
            rc = KDirectoryRename(dir, true, g_cache_file_tmp, g_cache_file);
            ReleaseComplain(KDirectoryRelease, dir);
            data.rc = data.rc ? data.rc : rc;
            if( data.rc != 0 ) {
                PLOGERR(klogErr, (klogErr, data.rc, "writing SRA cache file $(s) $(n) records", PLOG_2(PLOG_S(s),PLOG_U32(n)), g_cache_file, data.recs));
            } else {
                PLOGMSG(klogInfo, (klogInfo, "created SRA cache file $(s) $(n) records", PLOG_2(PLOG_S(s),PLOG_U32(n)), g_cache_file, data.recs));
            }
        }
    }
    PLOGMSG(klogInfo, (klogInfo, "SRA sync thread ended v$(s)", PLOG_U64(s), g_version));
    return 0;
}

static
rc_t SRAList_Queue(const KThread *self, void *data)
{
    LOGMSG(klogInfo, "SRA queue thread started");
    while( g_queue_cond != NULL ) {
        DEBUG_LINE(10, "SRA queue %s", "running");
        do {
            SRAQueueNode* q = NULL;
            if( SRAList_Queue_Lock() == 0 ) {
                if( g_queue_depth > 0 ) {
                    q = (SRAQueueNode*)SLListPopHead(&g_queue);
                    g_queue_depth--;
                }
                SRAList_Queue_Unlock();
                if( q != NULL ) {
                    if( SRAList_Lock(false) == 0 ) {
                        PLOGMSG(klogInfo, (klogInfo, "SRA queue $(s) updating", PLOG_S(s), q->node->accession));
                        if( atomic32_read(&q->node->refcount) == 1 ) {
                            /* restore value of release below will go negative and leak */
                            DEBUG_LINE(10, "SRA queue %s dropped - not updated", q->node->accession);
                        } else if( q->node->mtime == q->mtime ) {
                            /* update only if not changed since it was put into queue */
                            uint32_t i, count = q->node->files_count;
                            FileOptions* opt;
                            if( FileOptions_Clone(&opt, q->node->files, count) == 0 ) {
                                SRAList_Unlock();
                                for(i = 0; i < count; i++) {
                                    if( opt[i].md5_file != 0 ) {
                                        rc_t rc = FileOptions_CalcMD5(&opt[i], q->node->accession, q->node);
                                        if( rc != 0 ) {
                                            PLOGERR(klogErr, (klogErr, rc, "SRA queue $(s) while obtaining md5",
                                                PLOG_S(s), q->node->accession));
                                        } else if( (rc = FileOptions_UpdateMD5(&opt[i], q->node->accession)) != 0 ) {
                                            PLOGERR(klogErr, (klogErr, rc, "SRA queue $(s) while assigning md5",
                                                PLOG_S(s), q->node->accession));
                                        }
                                    }
                                }
                                if( SRAList_Lock(true) == 0 && q->node->mtime == q->mtime ) {
                                    /* actual update if not changed */
                                    FREE(q->node->files);
                                    q->node->files = opt;
                                    q->node->files_count = count;
                                    PLOGMSG(klogInfo, (klogInfo, "SRA queue $(s) updated", PLOG_S(s), q->node->accession));
                                } else {
                                    FREE(opt);
                                    DEBUG_LINE(10, "SRA queue %s changed during - not updated", q->node->accession);
                                }
                            }
                        } else {
                            DEBUG_LINE(10, "SRA queue %s changed - not updated", q->node->accession);
                        }
                        SRAList_Unlock();
                        SRAListNode_Release(q->node);
                        FREE(q);
                    }
                } else {
                    break;
                }
            }
        } while( g_queue_cond != NULL );
        if( g_queue_cond != NULL ) {
            DEBUG_LINE(10, "SRA queue %s", "waiting");
            SRAList_Queue_Lock();
            KConditionWait(g_queue_cond, g_queue_lock);
            SRAList_Queue_Unlock();
        }
    }
    LOGMSG(klogInfo, "SRA queue thread ended");
    return 0;
}

rc_t SRAList_Make(KDirectory* dir, unsigned int seconds, const char* cache_path)
{
    rc_t rc = 0;
    BSTreeInit(&g_list);
    SLListInit(&g_queue);
    g_sync = seconds;

    assert(dir != NULL);

    if( cache_path != NULL ) {
        char buf[4096];
        size_t len;
        const char* path, *slash = strrchr(cache_path, '/');

        if( slash == NULL ) {
            slash = cache_path;
            path = ".";
            len = 1;
        } else {
            path = cache_path;
            len = slash++ - cache_path;
        }
        if( (rc = KDirectoryResolvePath(dir, true, buf, 4096, "%.*s", len, path)) == 0 ) {
            size_t i = strlen(buf) - 1;
            while( buf[i] == '.' || buf[i] == '/' ) {
                i--;
            }
            if( i + 2 + strlen(slash) + 4 > sizeof(buf) ) {
                rc = RC(rcExe, rcPath, rcConstructing, rcBuffer, rcInsufficient);
            } else {
                buf[++i] = '/';
                buf[++i] = '\0';
                strcat(buf, slash);
                if( (rc = StrDup(buf, &g_cache_file)) == 0 ) {
                    DEBUG_LINE(10, "SRA cache file path set to '%s'", g_cache_file);
                    strcat(buf, ".tmp");
                    if( (rc = StrDup(buf, &g_cache_file_tmp)) == 0 ) {
                        DEBUG_LINE(10, "SRA tmp cache file path set to '%s'", g_cache_file_tmp);
                    }
                }
            }
        }
    }
    return rc;
}

void SRAList_Init(void)
{
    rc_t rc = 0;

    if( g_lock == NULL && (rc = KRWLockMake(&g_lock)) != 0 ) {
        g_lock = NULL;
        LOGERR(klogErr, rc, "SRA lock");
    }
    if( g_queue_cond == NULL && (rc = KConditionMake(&g_queue_cond)) != 0 ) {
        g_queue_cond = NULL;
        LOGERR(klogErr, rc, "SRA queue condition");
    } else if( g_queue_lock == NULL && (rc = KLockMake(&g_queue_lock)) != 0 ) {
        g_queue_lock = NULL;
        LOGERR(klogErr, rc, "SRA queue lock");
    }
    if( g_cache_file != NULL ) {
        /* try to load cache */
        KDirectory* dir = NULL;

        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            const KFile* f;
            uint32_t recs = 0;

            if( (rc = KDirectoryOpenFileRead(dir, &f, "%s", g_cache_file)) == 0 ) {
                uint64_t pos = 0;
                uint32_t ver = 0, recid;
                uint16_t obj_sz;
                size_t num_read = 0;
                SRAListNode_FindData data;
                SRAListNode* found;

                assert(sizeof(ver) == sizeof(g_cache_version));
                do {
                    memset(&data, 0, sizeof(data));
                    if( pos == 0 ) {
                        if( (rc = KFileRead(f, pos, &ver, sizeof(ver), &num_read)) != 0 ||
                            num_read != sizeof(ver) || ver == 0 || ver > g_cache_version ) {
                                rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData,
                                                  ver != g_cache_version ? rcBadVersion : rcTooShort);
                            break;
                        }
                        pos += num_read;
                    }
                    if( (rc = KFileRead(f, pos, &obj_sz, sizeof(obj_sz), &num_read)) != 0 || num_read != sizeof(obj_sz) ) {
                        if( num_read != 0 ) {
                            /* if num_read is 0 than it is proper EOF */
                            rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                        }
                        break;
                    }
                    pos += num_read;
                    MALLOC(data.accession, obj_sz + 1);
                    if( data.accession == NULL ) {
                        rc = RC(rcExe, rcTable, rcReading, rcMemory, rcExhausted);
                        break;
                    }
                    if( (rc = KFileRead(f, pos, data.accession, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                        rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                        break;
                    }
                    data.accession[obj_sz] = '\0';
                    pos += num_read;
                    if( (rc = KFileRead(f, pos, &obj_sz, sizeof(obj_sz), &num_read)) != 0 || num_read != sizeof(obj_sz) ) {
                        rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                        break;
                    }
                    pos += num_read;
                    if( obj_sz > 0 ) {
                        MALLOC(data.path, obj_sz + 1);
                        if( data.path == NULL ) {
                            rc = RC(rcExe, rcTable, rcReading, rcMemory, rcExhausted);
                            break;
                        }
                        if( (rc = KFileRead(f, pos, data.path, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                            rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                            break;
                        }
                        data.path[obj_sz] = '\0';
                        pos += num_read;
                    }
                    if( (rc = SRAListNode_Insert(&data, 0, &found)) != 0 ) {
                        break;
                    } else if( (rc = SRAList_Lock(true)) == 0 ) {
                        do {
                            obj_sz = sizeof(found->files_count);
                            if( (rc = KFileRead(f, pos, &found->files_count, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                                rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                                break;
                            }
                            pos += num_read;
                            if( found->files_count > 0 ) {
                                if( ver < 4 ) {
                                    /* size was different for older version */
                                    obj_sz = sizeof(FileOptionsOld) * found->files_count;
                                    
                                } else {
                                    obj_sz = sizeof(*found->files) * found->files_count;
                                }
                                MALLOC(found->files, obj_sz);
                                if( found->files == NULL ) {
                                    rc = RC(rcExe, rcTable, rcReading, rcMemory, rcExhausted);
                                    break;
                                }
                                if( (rc = KFileRead(f, pos, found->files, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                                    rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                                    break;
                                }
                                pos += num_read;
                                if( ver < 4 ) {
                                    FileOptionsOld* fOld = (FileOptionsOld*)found->files;
                                    if( (rc = FileOptions_Make(&found->files, found->files_count)) != 0 ) {
                                        FREE(fOld);
                                        break;
                                    } else {
                                        uint32_t i;
                                        /* old struct size */
                                        obj_sz = sizeof(FileOptionsOld);
                                        for(i = 0; i < found->files_count; i++ ) {
                                            memcpy(&found->files[i], &fOld[i], obj_sz);
                                            memset(found->files[i].md5, 0, sizeof(found->files[i].md5));
                                            found->files[i].md5_file = 0;
                                        }
                                        FREE(fOld);
                                    }
                                }
                            }
                            obj_sz = sizeof(found->mtime);
                            if( (rc = KFileRead(f, pos, &found->mtime, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                                rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                                break;
                            }
                            pos += num_read;
                            if( ver >= 3 ) {
                                obj_sz = sizeof(found->utime);
                                if( (rc = KFileRead(f, pos, &found->utime, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                                    rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                                    break;
                                }
                                pos += num_read;
                            }
                            SRAList_Unlock();
                            if( ver == 1 ) {
                                KTime_t l;
                                obj_sz = sizeof(l);
                                if( (rc = KFileRead(f, pos, &l, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                                    rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                                    break;
                                }
                                pos += num_read;
                            }
                            obj_sz = sizeof(recid);
                            if( (rc = KFileRead(f, pos, &recid, obj_sz, &num_read)) != 0 || num_read != obj_sz ) {
                                rc = rc ? rc : RC(rcExe, rcTable, rcReading, rcData, rcTooShort);
                                break;
                            }
                            pos += num_read;
                            if( ++recs != recid ) {
                                rc = RC(rcExe, rcTable, rcReading, rcData, rcInconsistent);
                                break;
                            }
                        } while(false);
                    }
                    FREE(data.accession);
                    FREE(data.path);
                    SRAListNode_Release(found);
                } while(rc == 0);
                ReleaseComplain(KFileRelease, f);
            } else if( GetRCState(rc) == rcNotFound ) {
                rc = 0;
            }
            if( rc != 0 ) {
                PLOGERR(klogErr, (klogErr, rc, "reading SRA cache file $(s) $(n) records",
                        PLOG_2(PLOG_S(s),PLOG_U32(n)), g_cache_file, recs));
            } else if( recs > 0 ) {
                PLOGMSG(klogInfo, (klogInfo, "loaded SRA cache file $(s) $(n) records",
                        PLOG_2(PLOG_S(s),PLOG_U32(n)), g_cache_file, recs));
            }
            ReleaseComplain(KDirectoryRelease, dir);
        }
    }
    if( g_lock != NULL ) {
        if( g_sync > 0 && (rc = KThreadMake(&g_thread, SRAList_Thread, NULL)) != 0 ) {
            LOGERR(klogErr, rc, "SRA sync thread");
        }
        if( g_queue_cond != NULL && g_queue_lock != NULL &&
            (rc = KThreadMake(&g_queue_thread, SRAList_Queue, NULL)) != 0 ) {
            LOGERR(klogErr, rc, "SRA queue thread");
        }
    }
}

void SRAList_Fini(void)
{
    g_sync = 0;
    if( g_thread != NULL ) {
        ReleaseComplain(KThreadCancel, g_thread);
        ReleaseComplain(KThreadRelease, g_thread);
    }
    if( g_queue_thread != NULL ) {
        KCondition* x = g_queue_cond;
        g_queue_cond = NULL;
        SRAList_Queue_Lock();
        KConditionSignal(x);
        SRAList_Queue_Unlock();
        ReleaseComplain(KThreadCancel, g_queue_thread);
        ReleaseComplain(KThreadRelease, g_queue_thread);
        g_queue_cond = x;
        g_queue_thread = NULL;
    }
    ReleaseComplain(KConditionRelease, g_queue_cond);
    ReleaseComplain(KLockRelease, g_queue_lock);
    pthread_cond_destroy(&g_refresh_cond);
    pthread_mutex_destroy(&g_refresh_mutex);
    if( g_lock != NULL ) {
        SRAList_Lock(true);
    }
    BSTreeWhack(&g_list, SRAList_Whack, NULL);
    SLListWhack(&g_queue, SRAQueue_Whack, NULL);
    if( g_lock != NULL ) {
        SRAList_Unlock();
        ReleaseComplain(KRWLockRelease, g_lock);
    }
    ReleaseComplain(SRAMgrRelease, g_sra_mgr);
    ReleaseComplain(KLockRelease, g_sra_mgr_lock);
    FREE(g_cache_file);
    FREE(g_cache_file_tmp);

    g_queue_cond = NULL;
    g_queue_lock = NULL;
    g_version = 0;
    g_lock = NULL;
    g_thread = NULL;
    g_sra_mgr = NULL;
}

rc_t SRAListNode_GetType(const SRAListNode* cself, SRAConfigFlags flags, const char* suffix, const FileOptions** options)
{
    rc_t rc = 0;
    if( cself == NULL || suffix == NULL || options == NULL ) {
        rc = RC(rcExe, rcTable, rcSearching, rcParam, rcNull);
    } else {
        if( (rc = SRAListNode_Update((SRAListNode*)cself, 0)) == 0 || GetRCState(rc) == rcCanceled ) {
            DEBUG_LINE(10, "'%s'", suffix);
            if( (rc = SRAList_Lock(false)) == 0 ) {
                size_t i;
                *options = NULL;
                for(i = 0; i < cself->files_count; i++) {
                    if( (cself->files[i].type & flags) && strcmp(suffix, cself->files[i].suffix) == 0 ) {
                        *options = &cself->files[i];
                        break;
                    }
                }
                SRAList_Unlock();
            }
        }
    }
    if( rc == 0 && *options == NULL ) {
        rc = RC(rcExe, rcTable, rcSearching, rcName, rcNotFound);
    }
    return rc;
}

rc_t SRAListNode_ListFiles(const SRAListNode* cself, const char* prefix, int types, FSNode_Dir_Visit func, void* data)
{
    rc_t rc = 0;
    if( cself == NULL ) {
        rc = RC(rcExe, rcTable, rcListing, rcSelf, rcNull);
    } else {
        if( (rc = SRAListNode_Update((SRAListNode*)cself, 0)) == 0 || GetRCState(rc) == rcCanceled ) {
            DEBUG_LINE(10, "%s", prefix);
            if( (rc = SRAList_Lock(false)) == 0 ) {
                char buf[1024];
                size_t i;
                for(i = 0; rc == 0 && i < cself->files_count; i++) {
                    if( (cself->files[i].type & types) && cself->files[i].file_sz > 0 ) {
                        strcpy(buf, prefix);
                        strcat(buf, cself->files[i].suffix);
                        rc = func(buf, data);
                    }
                }
                SRAList_Unlock();
            }
        }
    }
    return rc;
}
