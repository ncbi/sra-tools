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
#include <klib/printf.h>
#include <kxml/xml.h>
#include <klib/container.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kproc/lock.h>

typedef struct DirectoryNode DirectoryNode;
#define FSNODE_IMPL DirectoryNode

#include "log.h"
#include "xml.h"
#include "directory.h"
#include "tar-node.h"
#include "kfile-accessor.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

typedef struct DirNodeChild_struct {
    BSTNode node;
    const char* full_name; /* full path to detected file */
    const char* name; /* file name only within full_name above */
    const FSNode* child;
} DirNodeChild;

static
int DirNodeChild_Sort(const BSTNode *item, const BSTNode *node)
{
    return strcmp(((const DirNodeChild*)item)->child->name, ((const DirNodeChild*)node)->child->name);
}

static
int DirNodeChild_FindByName(const void *item, const BSTNode *node)
{
    return strcmp((const char*)item, ((const DirNodeChild*)node)->child->name);
}

static
int DirNodeChild_FindBySrc(const void *item, const BSTNode *node)
{
    return strcmp((const char*)item, ((const DirNodeChild*)node)->name);
}

static
void DirNodeChild_Whack(BSTNode *node, void *data)
{
    DirNodeChild* n = (DirNodeChild*)node;

    if( n != NULL ) {
        DEBUG_LINE(8, "Releasing auto TAR %s", n->child->name);
        FSNode_Release(n->child);
        FREE(n);
    }
}

static
void DirNodeChild_Touch( BSTNode *node, void *data )
{
    DirNodeChild* n = (DirNodeChild*)node;
    BSTree* t = (BSTree*)data;
    if( FSNode_Touch(n->child) != 0 ) {
        BSTreeUnlink(t, node);
        DirNodeChild_Whack(node, NULL);
    }
}

struct DirectoryNode {
    FSNode node;
    char* path;
    KTime_t mtime;
    KRWLock* lock;
    KTime_t timestamp;
    /* list of physical files and dirs in 'path' minus those in children list */
    KNamelist* ls; 
    BSTree children;
};

struct DirNodeChild_List_Data {
    rc_t rc;
    const DirectoryNode* cself;
    FSNode_Dir_Visit func;
    void* data;
};

static
bool DirNodeChild_List( BSTNode *node, void *data )
{
    DirNodeChild* n = (DirNodeChild*)node;
    struct DirNodeChild_List_Data* d = (struct DirNodeChild_List_Data*)data;
    const FSNode* ch;
    bool hidden = true;

    if( ((d->rc = FSNode_FindChild(&d->cself->node, n->child->name, strlen(n->child->name), &ch, &hidden)) == 0 && hidden) ||
         (GetRCObject(d->rc) == rcName && GetRCState(d->rc) == rcNotFound) ) {
        d->rc = d->func(n->child->name, d->data);
    }
    return d->rc != 0;
}

static
rc_t DirectoryNode_Lock(const DirectoryNode* cself, bool exclusive)
{
    DEBUG_LINE(8, "Lock DirectoryNode %s %s", cself->path, exclusive ? "write" : "read");
    return exclusive ? KRWLockAcquireExcl(((DirectoryNode*)cself)->lock) : KRWLockAcquireShared(((DirectoryNode*)cself)->lock);
}

static
rc_t DirectoryNode_Unlock(const DirectoryNode* cself)
{
    DEBUG_LINE(8, "Unlocking DirectoryNode %s", cself->path);
    return KRWLockUnlock(((DirectoryNode*)cself)->lock);
}

static
rc_t DirectoryNode_IsChild(const DirectoryNode* cself, const char* subpath, const FSNode** node,
                           int (*finder)(const void *item, const BSTNode *node))
{
    DirNodeChild* n = (DirNodeChild*)BSTreeFind(&cself->children, subpath, finder);
    if( n != NULL ) {
        *node = n->child;
    } else {
        *node = NULL;
        return RC(rcExe, rcFile, rcAccessing, rcName, rcNotFound);
    }
    return 0;
}

static
rc_t CC DirectoryNode_AddTar( const KDirectory *dir, uint32_t type, const char *name, void *data )
{
    rc_t rc = 0;

    if( (type & ~kptAlias) == kptFile && name != NULL ) {
        size_t lnm = strlen(name);

        if( lnm > 8 && strncmp(&name[lnm - 8], ".tar.xml", 8) == 0 ) {
            DirectoryNode* self = (DirectoryNode*)data;
            DirNodeChild* n = (DirNodeChild*)BSTreeFind(&self->children, name, DirNodeChild_FindBySrc);
            if( n == NULL ) {
                char resolved[4096];
                if( (rc = KDirectoryResolvePath(dir, true, resolved, sizeof(resolved), "%s/%s", self->path, name)) == 0 ) {
                    size_t lrs = strlen(resolved);
                    CALLOC(n, 1, sizeof(*n) + lrs + 1);
                    if( n == NULL ) {
                        rc = RC(rcExe, rcArc, rcInserting, rcMemory, rcExhausted);
                    } else {
                        char* x = (char*)&n[1];
                        memcpy(x, resolved, lrs);
                        x[lrs] = '\0';
                        n->full_name = x;
                        n->name = &n->full_name[lrs - lnm];
                        if( (rc = TarNode_MakeAuto(&n->child, dir, self->path, name, n->full_name)) != 0 ||
                            (rc = BSTreeInsert(&self->children, &n->node, DirNodeChild_Sort)) != 0 ) {
                            DirNodeChild_Whack(&n->node, NULL);
                        }
                    }
                }
            }
        }
    }
    return rc;
}

static
bool DirectoryNode_LS( const KDirectory *dir, const char *name, void *data )
{
    size_t lnm = name ? strlen(name) : 0;
    bool ret;

    if( lnm > 8 && strncmp(&name[lnm - 8], ".tar.xml", 8) == 0 ) {
        DirectoryNode* self = (DirectoryNode*)data;
        ret = BSTreeFind(&self->children, name, DirNodeChild_FindBySrc) == NULL;
    } else if( lnm > 4 && strncmp(&name[lnm - 4], ".tar", 4) == 0 ) {
        DirectoryNode* self = (DirectoryNode*)data;
        const FSNode* child;
        /* virtual .tar file hides actual .tar file with same name */
        ret = DirectoryNode_IsChild(self, name, &child, DirNodeChild_FindByName) != 0;
    } else {
        ret = true;
    }
    return ret;
}

static
rc_t DirectoryNode_Touch(const DirectoryNode* cself)
{
    rc_t rc = 0;

    if( cself->path != NULL ) {
        if( (rc = DirectoryNode_Lock(cself, true)) == 0 ) {
            KDirectory* dir;
            /* drop disappeared items, update existing */
            BSTreeForEach(&cself->children, false, DirNodeChild_Touch, (void*)&cself->children);
            if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
                KTime_t dt = 0;
                if( (rc = KDirectoryDate(dir, &dt, "%s", cself->path)) == 0 && cself->timestamp != dt ) {
                    DirectoryNode* self = (DirectoryNode*)cself;
                    /* re-read directory */
                    ReleaseComplain(KNamelistRelease, self->ls);
                    self->ls = NULL;
                    if( (rc = KDirectoryVisit(dir, false, DirectoryNode_AddTar, self, "%s", self->path)) == 0 ) {
                        rc = KDirectoryList(dir, &self->ls, DirectoryNode_LS, self, "%s", self->path);
                    }
                    self->timestamp = rc == 0 ? dt : 0;
                }
                ReleaseComplain(KDirectoryRelease, dir);
            }
            ReleaseComplain(DirectoryNode_Unlock, cself);
        }
    }
    return rc;
}

static
rc_t DirectoryNode_Attr(const DirectoryNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    *type = kptDir;
    if( cself->path != NULL ) {
        KDirectory* dir = NULL;
        const KDirectory* sub = NULL;

        if( subpath != NULL && (rc = DirectoryNode_Lock(cself, false)) == 0 ) {
            const FSNode* child = NULL;
            if( (rc = DirectoryNode_IsChild(cself, subpath, &child, DirNodeChild_FindByName)) == 0 ) {
                rc = FSNode_Attr(child, NULL, type, ts, file_sz, access, block_sz);
            }
            ReleaseComplain(DirectoryNode_Unlock, cself);
            if( child != NULL ) {
                return rc;
            }
        }
        if( (rc = KDirectoryNativeDir(&dir)) == 0 &&
            (rc = KDirectoryOpenDirRead(dir, &sub, true, "%s", cself->path)) == 0 ) {
            const char* path = subpath ? subpath : ".";
            DEBUG_LINE(8, "Using full name %s/%s", cself->path, path);
            if( (rc = KDirectoryDate(sub, ts, "%s", path)) == 0 ) {
                *type = KDirectoryPathType(sub, "%s", path);
                if( *type != kptBadPath && *type != kptNotFound ) {
                    if( (rc = KDirectoryAccess(sub, access, "%s", path)) == 0 ) {
                        if( *type & kptAlias ) {
                            bool children;
                            if( (rc = FSNode_HasChildren(&cself->node, &children)) == 0 ) {
                                /* if it is pointer to an aliased directory and has no XML children than pass symlink on */
                                if( children == false || subpath != NULL ) {
                                    char r[4096];
                                    if( (rc = KDirectoryResolveAlias(sub, true, r, sizeof(r), "%s", path)) == 0 ) {
                                         DEBUG_LINE(8, "Symlink name %s", r);
                                        *file_sz = strlen(r);
                                    }
                                } else {
                                    /* otherwise remove alias bit */
                                    *type = *type & ~kptAlias;
                                }
                            }
                        } else if( *type == kptFile ) {
                            rc = KDirectoryFileSize(sub, file_sz, "%s", path);
                        }
                    }
                }
            }
        }
        ReleaseComplain(KDirectoryRelease, sub);
        ReleaseComplain(KDirectoryRelease, dir);
    } else if( subpath != NULL ) {
        rc = RC(rcExe, rcDirectory, rcEvaluating, rcDirEntry, rcNotFound);
    }
    if( subpath == NULL && cself->mtime != 0 ) {
        *ts = cself->mtime;
    }
    return rc;
}

struct DirectoryNode_DirVisit_Data {
    FSNode_Dir_Visit func;
    void* data;
};

static
rc_t CC DirectoryNode_DirVisit( const KDirectory *dir, uint32_t type, const char *name, void *data )
{
    struct DirectoryNode_DirVisit_Data* d = (struct DirectoryNode_DirVisit_Data*)data;
    return d->func(name, d->data);
}

static
rc_t DirectoryNode_Dir(const DirectoryNode* cself, const char* subpath, FSNode_Dir_Visit func, void* data)
{
    rc_t rc = 0;

    if( subpath == NULL ) {
        /* add XML tree children */
        if( (rc = FSNode_ListChildren(&cself->node, func, data)) == 0 ) {
            if( (rc = DirectoryNode_Lock(cself, false)) == 0 ) {
                if( cself->ls != NULL ) {
                    /* add ls result excluding XML children */
                    uint32_t i = 0, count = 0;
                    rc = KNamelistCount(cself->ls, &count);
                    while(rc == 0 && i < count) {
                        const char* nm = NULL;
                        const FSNode* ch;
                        bool hidden = true;
                        if( (rc = KNamelistGet(cself->ls, i++, &nm)) == 0 &&
                            (((rc = FSNode_FindChild(&cself->node, nm, strlen(nm), &ch, &hidden)) == 0 && hidden) ||
                             (GetRCObject(rc) == rcName && GetRCState(rc) == rcNotFound)) ) {
                            rc = func(nm, data);
                        }
                    }
                }
                if( rc == 0 ) {
                    struct DirNodeChild_List_Data d;
                    d.rc = 0;
                    d.cself = cself;
                    d.func = func;
                    d.data = data;
                    /* add detected child nodes excluding XML children */
                    BSTreeDoUntil(&cself->children, false, DirNodeChild_List, &d);
                    rc = d.rc;
                }
                ReleaseComplain(DirectoryNode_Unlock, cself);
            }
        }
    } else {
        KDirectory* dir = NULL;
        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            struct DirectoryNode_DirVisit_Data d;
            d.func = func;
            d.data = data;
            DEBUG_LINE(8, "Listing kdir path %s/%s", cself->path, subpath);
            rc = KDirectoryVisit(dir, false, DirectoryNode_DirVisit, &d, "%s/%s", cself->path, subpath);
            ReleaseComplain(KDirectoryRelease, dir);
        }
    }
    return rc;
}

static
rc_t DirectoryNode_Link(const DirectoryNode* cself, const char* subpath, char* buf, size_t buf_sz)
{
    rc_t rc = 0;
    if( cself->path == NULL ) {
        rc = RC(rcExe, rcPath, rcAliasing, rcDirEntry, rcUnsupported);
    } else {
        KDirectory* dir = NULL;
        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            if( subpath == NULL ) {
                DEBUG_LINE(8, "Resolving %s", cself->path);
                rc = KDirectoryResolveAlias(dir, true, buf, buf_sz, "%s", cself->path);
            } else {
                DEBUG_LINE(8, "Resolving %s/%s", cself->path, subpath);
                rc = KDirectoryResolveAlias(dir, true, buf, buf_sz, "%s/%s", cself->path, subpath);
            }
            DEBUG_LINE(8, "Resolved %s", rc == 0 ? buf : NULL);
            ReleaseComplain(KDirectoryRelease, dir);
        }
    }
    return rc;
}

static
rc_t DirectoryNode_Open(const DirectoryNode* cself, const char* subpath, const SAccessor** accessor)
{
    rc_t rc = 0;

    if( subpath == NULL ) {
        rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcNotFound);
    } else {
        const char* nm = NULL;

        if( (rc = DirectoryNode_Lock(cself, false)) == 0 ) {
            const FSNode* child = NULL;
            if( (rc = DirectoryNode_IsChild(cself, subpath, &child, DirNodeChild_FindByName)) == 0 ) {
                rc = FSNode_Open(child, NULL, accessor);
            }
            ReleaseComplain(DirectoryNode_Unlock, cself);
            if( child != NULL ) {
                return rc;
            }
        }
        if( (rc = FSNode_GetName(&cself->node, &nm)) == 0 ) {
            KDirectory* dir = NULL;
            if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
                const KFile* kf = NULL;
                if( (rc = KDirectoryOpenFileRead(dir, &kf, "%s/%s", cself->path, subpath)) == 0 ) {
                    if( (rc = KFileAccessor_Make(accessor, nm, kf)) != 0 ) {
                        ReleaseComplain(KFileRelease, kf);
                    }
                }
                ReleaseComplain(KDirectoryRelease, dir);
            }
        }
    }
    if( rc != 0 ) {
        SAccessor_Release(*accessor);
        *accessor = NULL;
    }
    return rc;
}

static
rc_t DirectoryNode_Release(DirectoryNode* self)
{
    rc_t rc = 0;
    if( self != NULL ) {
        BSTreeWhack(&self->children, DirNodeChild_Whack, NULL);
        ReleaseComplain(KRWLockRelease, self->lock);
        ReleaseComplain(KNamelistRelease, self->ls);
        FREE(self->path);
    }
    return rc;
}

static FSNode_vtbl DirectoryNode_vtbl = {
    sizeof(DirectoryNode),
    NULL,
    DirectoryNode_Touch,
    DirectoryNode_Attr,
    DirectoryNode_Dir,
    DirectoryNode_Link,
    DirectoryNode_Open,
    DirectoryNode_Release
};


rc_t DirectoryNode_Make(const KXMLNode* xml_node, FSNode** cself, char* errmsg, const char* rel_path,
                        KTime_t dflt_ktm, EXMLValidate validate)
{
    rc_t rc = 0;

    if( xml_node == NULL || cself == NULL ) {
        rc = RC(rcExe, rcDirectory, rcConstructing, rcParam, rcNull);
    } else {
        char* path = NULL, *name = NULL, name_buf[4096];
        KTime_t ktm = 0;
        DirectoryNode* ff = NULL;

        if( (rc = KXMLNodeReadAttrCStr(xml_node, "path", &path, NULL)) == 0 ) {
            if( path[0] == '\0' ) {
                FREE(path);
                path = NULL;
            } else {
                KDirectory* dir = NULL;
                if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
                    if( path[0] != '/' ) {
                        char resolved[4096];
                        if( (rc = KDirectoryResolvePath(dir, true, resolved, sizeof(resolved),
                                                                  "%s%s", rel_path, path)) == 0 ) {
                            DEBUG_LINE(8, "%s%s resolved to %s", rel_path, path, resolved);
                            FREE(path);
                            rc = StrDup(resolved, &path);
                        }
                    }
                    if( rc == 0 && validate > eXML_NoCheck ) {
                        uint32_t t = KDirectoryPathType(dir, "%s", path);
                        if( t != kptDir && t != (kptDir | kptAlias) ) {
                            if( validate > eXML_NoFail ) {
                                rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, t == kptNotFound ? rcNotFound : rcInvalid);
                            } else {
                                PLOGMSG(klogErr, (klogErr, "Directory path '$(p)' not found", "p=%s", path));
                            }
                        }
                    }
                    ReleaseComplain(KDirectoryRelease, dir);
                }
            }
        } else if( GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound ) {
            rc = 0;
        }
        if( rc != 0 ) {
            strcpy(errmsg, "Directory/@path: '");
            strcat(errmsg, path ? path : "(null)");
            strcat(errmsg, "'");
        }
        if( rc == 0 ) {
            size_t sz;
            rc = KXMLNodeReadAttrCString(xml_node, "name", name_buf, sizeof(name_buf), &sz);
            if( rc == 0 && name_buf[0] != '\0' ) {
                name = name_buf;
            } else if( (GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound)  ) {
                rc = 0;
            }
            if( rc != 0 ) {
                strcpy(errmsg, "Directory/@name");
            } else if( name == NULL ) {
                if( path == NULL ) {
                    rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
                } else {
                    name = strrchr(path, '/');
                    name = name ? name + 1 : path;
                }
            }
        }
        if( rc == 0 && (rc = XML_ParseTimestamp(xml_node, "timestamp", &ktm, true)) != 0 ) {
            strcpy(errmsg, "Directory/@timestamp");
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
                        if( strcmp("path", attr_nm) == 0 || strcmp("name", attr_nm) == 0 || strcmp("timestamp", attr_nm) == 0 ) {
                            continue;
                        }
                        rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
                        strcpy(errmsg, "unknown attribute Directory/@");
                        strcat(errmsg, attr_nm);
                    }
                }
                ReleaseComplain(KNamelistRelease, attr);
            }
        }
        if( rc == 0 ) {
            if( (rc = FSNode_Make((FSNode**)&ff, name, &DirectoryNode_vtbl)) == 0 &&
                (rc = KRWLockMake(&ff->lock)) == 0 ) {
                ff->path = path;
                ff->mtime = ktm != 0 ? ktm : (path ? 0 : dflt_ktm);
                BSTreeInit(&ff->children);
            } else {
                strcpy(errmsg, "Directory '");
                strcat(errmsg, name);
                strcat(errmsg, "'");
            }
        }
        if( rc == 0 ) {
            *cself = &ff->node;
        } else {
            FREE(path);
        }
    }
    return rc;
}
