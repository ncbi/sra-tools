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
#include <kfs/directory.h>
#include <kfs/file.h>
#include <klib/namelist.h>
#include <klib/container.h>
#include <klib/log.h>
#include <kproc/lock.h>
#include <kproc/thread.h>

#include "log.h"
#include "xml.h"
#include "file.h"
#include "directory.h"
#include "tar-node.h"
#include "sra-list.h"
#include "node.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>

static const KXMLMgr* g_xmlmgr = NULL;
static const FSNode* g_root = NULL;
static KRWLock* g_lock = NULL;
static const char* g_start_dir = NULL;
static uint32_t g_xml_validate = 0;

static unsigned int g_xml_sync = 0;
/* if g_xml_sync == 0 these are not used: */
static KTime_t g_xml_mtime = 0;
static char* g_xml_path = NULL;
static KThread* g_xml_thread = NULL;

static
rc_t XMLLock(bool exclusive)
{
    DEBUG_MSG(10, ("Lock XML tree %s\n", exclusive ? "write" : "read"));
    return exclusive ? KRWLockAcquireExcl(g_lock) : KRWLockAcquireShared(g_lock);
}

static
void XMLUnlock(void)
{
    DEBUG_MSG(10, ("Unlocked XML tree\n"));
    ReleaseComplain(KRWLockUnlock, g_lock);
}

void XML_FindRelease(void)
{
    XMLUnlock();
}

rc_t XML_FindLock(const char* path, bool recur, const FSNode** node, const char** subpath)
{
    rc_t rc = 0;
    size_t sz = 0;
    const char* p0 = NULL, *p = NULL;
    const FSNode* pn = NULL, *n = NULL;
    bool hidden = false;

    if( path == NULL || node == NULL || subpath == NULL ) {
        return RC(rcExe, rcPath, rcResolving, rcParam, rcNull);
    }
    sz = strlen(path);
    if( sz == 0 ) {
        return RC(rcExe, rcPath, rcResolving, rcParam, rcEmpty);
    }
    p0 = path;
    if( (rc = XMLLock(false)) != 0 ) {
        return rc;
    }
    pn = g_root;
    do {
        DEBUG_MSG(8, ("Path: '%s'\n", p0));
        while( *p0 == '/' && *p0 != '\0' ) {
            p0++;
        }
        if( *p0 == '\0' ) {
            break;
        }
        p = strchr(p0, '/');
        if( p == NULL ) {
            p = p0 + strlen(p0);
        }
        DEBUG_MSG(8, ("Push: '%.*s'\n", p - p0, p0));
        if( (rc = FSNode_FindChild(pn, p0, p - p0, &n, &hidden)) == 0 ) {
            if( hidden ) {
                pn = n;
                DEBUG_MSG(8, ("Match! hidden '%s' left '%s'\n", pn->name, p0));
                break;
            } else {
                DEBUG_MSG(8, ("Match! '%.*s' left '%s'\n", p - p0, p0, p));
            }
        } else if( GetRCState(rc) == rcNotFound ) {
            rc = 0;
            break;
        }
        pn = n;
        p0 = p;
    } while( rc == 0 && p0 < path + sz );

    if( rc == 0 ) {
        if( pn == NULL ) {
            rc = RC(rcExe, rcPath, rcResolving, rcDirEntry, rcNotFound);
            DEBUG_MSG(10, ("Not found: '%s', in '%s'\n", p0, path));
        } else {
            if( (rc = FSNode_Touch(pn)) != 0 ) {
                PLOGERR(klogWarn, (klogWarn, rc, "touch failed for $(n)", PLOG_S(n), pn->name));
                rc = 0;
            }
            *node = pn;
            *subpath = (p0 && p0[0] != '\0') ? p0 : NULL;
#if _DEBUGGING
            {
                const char* nm = NULL;
                FSNode_GetName(pn, &nm);
                DEBUG_MSG(10, ("Found: '%s', sub '%s'\n", nm, *subpath));
            }
#endif
        }
    }
    if( rc != 0 ) {
        XMLUnlock();
    }
    return rc;
}

static
rc_t SRAConfigParse(const KXMLNode* xml_node, SRAConfigFlags* flags, char* errmsg)
{
    rc_t rc = 0;
    uint32_t i;
    char at[4096];
    size_t sz;
    char* attr_name[]         = { "run-directory", "SRA-archive",   "SRA-archive-lite",  "fastq",           "SFF" };
    SRAConfigFlags attr_val[] = { eSRAFuseRunDir,  eSRAFuseFileArc, eSRAFuseFileArcLite, eSRAFuseFileFastq, eSRAFuseFileSFF };

    if( *flags & eSRAFuseInitial ) {
        *flags = 0;
    }
    for(i = 0; rc == 0 && i < sizeof(attr_name)/sizeof(attr_name[0]); i++) {
        if( (rc = KXMLNodeReadAttrCString(xml_node, attr_name[i], at, sizeof(at), &sz)) == 0 ) {
            if( strcasecmp(at, "true") == 0 ) {
                *flags = *flags | attr_val[i];
            } else if( strcasecmp(at, "false") == 0 ) {
                *flags = *flags & ~(attr_val[i]);
            } else {
                strcpy(errmsg, "SRAConfig attribute ");
                strcat(errmsg,  attr_name[i]);
                strcat(errmsg, " value '");
                strcat(errmsg, at);
                strcat(errmsg, "'");
                rc = RC(rcExe, rcDoc, rcReading, rcData, rcInvalid);
            }
        } else if( GetRCState(rc) == rcNotFound ) {
            rc = 0;
        }
    }
    return rc;
}

static
rc_t XML_ValidateNode(FSNode* parent, const KXMLNode* n, SRAConfigFlags flags, char* errmsg)
{
    rc_t rc = 0;
    const char* name = NULL;
    FSNode* fsn = NULL;
    bool children_allowed = false, should_have_children = false, ignore_children = false;

    if( (rc = KXMLNodeElementName(n, &name)) != 0 ) {
        return rc;
    }
    DEBUG_MSG(8, ("Node: %s\n", name));
    if( name == NULL ) {
        return RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcNull);
    }

    if( strcmp(name, "Directory") == 0 ) {
        rc = DirectoryNode_Make(n, &fsn, errmsg, g_start_dir, g_xml_mtime, g_xml_validate);
        children_allowed = true;
    } else if( strcmp(name, "File") == 0 ) {
        rc = FileNode_Make(n, &fsn, errmsg, g_start_dir, g_xml_validate);
    } else if( strcmp(name, "SRA") == 0 ) {
        if( (rc = SRAListNode_Make(n, parent, flags, errmsg, g_start_dir, g_xml_validate)) == 0 ) {
            fsn = parent;
        }
    } else if( strcmp(name, "TAR") == 0 ) {
        /* tar nodes do not validate on creation */
        rc = TarNode_MakeXML(n, &fsn, errmsg, g_start_dir);
        children_allowed = true;
        ignore_children = true;
    } else if( strcmp(name, "SRAConfig") == 0 ) {
        if( (rc = SRAConfigParse(n, &flags, errmsg)) == 0 ) {
            fsn = parent;
            children_allowed = true;
            should_have_children = true;
        }
    } else {
        strcpy(errmsg, name);
        rc = RC(rcExe, rcDoc, rcValidating, rcTag, rcUnknown);
    }
    if( rc == 0 ) {
        strcpy(errmsg, name);
        if( fsn == parent || (rc = FSNode_AddChild(parent, fsn)) == 0 ) {
            uint32_t count = 0;
            if( (rc = KXMLNodeCountChildNodes(n, &count)) == 0 && count > 0 ) {
                if( !children_allowed ) {
                    if( fsn != NULL ) {
                        FSNode_GetName(fsn, &name);
                    }
                    rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
                    strcpy(errmsg, name);
                } else if( !ignore_children ) {
                    uint32_t i = 0;
                    const KXMLNode* ch = NULL;
                    while( rc == 0 && i < count ) {
                        if( (rc = KXMLNodeGetNodeRead(n, &ch, i++)) == 0 ) {
                            rc = XML_ValidateNode(fsn, ch, flags, errmsg);
                            ReleaseComplain(KXMLNodeRelease, ch);
                        }
                    }
                }
            } else if( count == 0 && should_have_children ) {
                PLOGMSG(klogWarn, (klogWarn, "$(n) may have children", PLOG_S(n), name));
            }
        }
    }
    return rc;
}

static
rc_t RootNode_Attr(const FSNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    *type = kptDir;
    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcEvaluating, rcDirEntry, rcNotFound);
    }
    return rc;
}

static
rc_t RootNode_Dir(const FSNode* cself, const char* subpath, FSNode_Dir_Visit func, void* data)
{
    if( subpath != NULL ) {
        return RC(rcExe, rcFile, rcListing, rcDirEntry, rcNotFound);
    }
    return FSNode_ListChildren(cself, func, data);
}

static const FSNode_vtbl RootNode_vtbl = {
    sizeof(FSNode),
    NULL,
    NULL,
    RootNode_Attr,
    RootNode_Dir,
    NULL,
    NULL,
    NULL
};

static
rc_t XML_Open(const char* path, const FSNode** tree)
{
    rc_t rc = 0;
    char errmsg[4096] = "";
    KDirectory *dir = NULL;
    
    PLOGMSG(klogInfo, (klogInfo, "Reading XML file '$(x)'", PLOG_S(x), path));
    if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
        const KFile* file = NULL;
        if( (rc = KDirectoryOpenFileRead(dir, &file, "%s", path)) == 0 ) {
            if( (rc = FSNode_Make((FSNode**)tree, "ROOT", &RootNode_vtbl)) == 0 ) {
                const KXMLDoc* xmldoc = NULL;
                if( (rc = KXMLMgrMakeDocRead(g_xmlmgr, &xmldoc, file)) == 0 ) {
                    const KXMLNodeset* ns = NULL;
                    if( (rc = KXMLDocOpenNodesetRead(xmldoc, &ns, "/FUSE/*")) == 0 ) {
                        uint32_t count = 0;
                        if( (rc = KXMLNodesetCount(ns, &count)) == 0 ) {
                            if( count == 0 ) {
                                rc = RC(rcExe, rcDoc, rcValidating, rcData, rcEmpty);
                            } else {
                                uint32_t i = 0;
                                while(rc == 0 && i < count) {
                                    const KXMLNode* n = NULL;
                                    if( (rc = KXMLNodesetGetNodeRead(ns, &n, i++)) == 0 ) {
                                        SRAConfigFlags flags = ~0;
                                        errmsg[0] = '\0';
                                        rc = XML_ValidateNode((FSNode*)*tree, n, flags, errmsg);
                                        ReleaseComplain(KXMLNodeRelease, n);
                                    }
                                }
                                if( rc == 0 ) {
                                    rc = SRAList_NextVersion();
                                }
                            }
                        }
                        ReleaseComplain(KXMLNodesetRelease, ns);
                    }
                    ReleaseComplain(KXMLDocRelease, xmldoc);
                }
                if( rc != 0 ) {
                    FSNode_Release(*tree);
                    *tree = NULL;
                }
            }
            ReleaseComplain(KFileRelease, file);
        }
        ReleaseComplain(KDirectoryRelease, dir);
    }
    if( rc == 0 ) {
        PLOGMSG(klogInfo, (klogInfo, "XML file '$(x)' ok", PLOG_S(x), path));
    } else {
        if( strlen(errmsg) < 1 ) {
            strcpy(errmsg, path);
        }
        LOGERR(klogErr, rc, errmsg);
    }
    return rc;
}

static
rc_t XMLThread( const KThread *self, void *data )
{
    KDirectory *dir = NULL;

    PLOGMSG(klogInfo, (klogInfo, "XML sync thread started with $(s) sec", PLOG_U32(s), g_xml_sync));
    do {
        rc_t rc = 0;
        KTime_t dt = 0;

        DEBUG_MSG(8, ("XML sync thread checking %s\n", g_xml_path));
        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            rc = KDirectoryDate(dir, &dt, "%s", g_xml_path);
            ReleaseComplain(KDirectoryRelease, dir);
        }
        if( rc == 0 ) {
            if( dt != g_xml_mtime ) {
                const FSNode* new_root = NULL;
                PLOGMSG(klogInfo, (klogInfo, "File $(f) changed ($(m) <> $(d)), updating...",
                    PLOG_3(PLOG_S(f),PLOG_I64(m),PLOG_I64(d)), g_xml_path, g_xml_mtime, dt));
                if( XML_Open(g_xml_path, &new_root) == 0 ) {
                    if( (rc = XMLLock(true)) == 0 ) {
                        const FSNode* old_root = g_root;
                        g_root = new_root;
                        g_xml_mtime = dt;
                        XMLUnlock();
                        FSNode_Release(old_root);
                        PLOGMSG(klogInfo, (klogInfo, "Data from $(f) updated successfully", PLOG_S(f), g_xml_path));
                    }
                }
            } else {
                DEBUG_MSG(8, ("XML sync thread up-to-date %s\n", g_xml_path));
            }
        } else {
            LOGERR(klogErr, rc, g_xml_path);
        }
        SRAList_PostRefresh();
        sleep(g_xml_sync);
    } while( g_xml_sync > 0 );
    LOGMSG(klogInfo, "XML sync thread ended");
    return 0;
}

rc_t XML_Make(KDirectory* dir, const char* const work_dir, const char* xml_path, unsigned int sync, uint32_t xml_validate)
{
    rc_t rc = 0; 

    g_xml_sync = sync;
    if( g_xmlmgr == NULL && (rc = KXMLMgrMakeRead(&g_xmlmgr)) != 0 ) {
        g_xmlmgr = NULL;
        LOGERR(klogErr, rc, "XML manager");
    } else {
        char buf[4096];
        if( (rc = KDirectoryResolvePath(dir, true, buf, 4096, "%s", xml_path)) == 0 ) {
            if( (rc = StrDup(buf, &g_xml_path)) == 0 ) {
                DEBUG_MSG(8, ("XML path set to '%s'\n", g_xml_path));
            }
        }
        g_start_dir = work_dir;
        g_xml_validate = xml_validate;
    }
    if( rc == 0 ) {
        rc = FSNode_Make((FSNode**)&g_root, "ROOT", &RootNode_vtbl);
    }
    return rc;
}

void XML_Init(void)
{
    rc_t rc = 0;

    if( g_lock == NULL && (rc = KRWLockMake(&g_lock)) != 0 ) {
        g_lock = NULL;
        LOGERR(klogErr, rc, "XML lock");
    }
    if( (rc = KThreadMake(&g_xml_thread, XMLThread, NULL)) != 0 ) {
        LOGERR(klogErr, rc, "XML sync thread");
    }
}

void XML_Fini(void)
{
    g_xml_sync = 0;
    if( g_xml_thread != NULL ) {
        ReleaseComplain(KThreadCancel, g_xml_thread);
        ReleaseComplain(KThreadRelease, g_xml_thread);
    }
    ReleaseComplain(KXMLMgrRelease, g_xmlmgr);
    XMLLock(true);
    FSNode_Release(g_root);
    XMLUnlock();
    ReleaseComplain(KRWLockRelease, g_lock);
    FREE(g_xml_path);

    g_root = NULL;
    g_lock = NULL;
    g_start_dir = NULL;
    g_xml_mtime = 0;
    g_xml_path = NULL;
    g_xml_thread = NULL;
}

rc_t XML_MgrGet(const KXMLMgr** xmlmgr)
{
    if( xmlmgr == NULL ) {
        return RC(rcExe, rcDoc, rcAccessing, rcParam, rcNull);
    }
    if( g_xmlmgr == NULL ) {
        return RC(rcExe, rcPath, rcAccessing, rcMgr, rcNull);
    }
    *xmlmgr = g_xmlmgr;
    return 0;
}

rc_t XML_ParseTimestamp(const KXMLNode* xml_node, const char* attr, KTime_t* timestamp, bool optional)
{
    rc_t rc;
    char ts[128];
    size_t sz;

    if( (rc = KXMLNodeReadAttrCString(xml_node, attr, ts, sizeof(ts), &sz)) == 0 ) {
        struct tm tm;
        memset(&tm, 0, sizeof(tm));
        if( strptime(ts, "%Y-%m-%dT%H:%M:%S", &tm) != NULL ) {
            *timestamp = mktime(&tm);
        } else {
            rc = RC(rcExe, rcDoc, rcReading, rcAttr, rcInvalid);
        }
    } else if( optional && GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound ) {
        rc = 0;
    }
    return rc;
}

rc_t XML_WriteTimestamp(char* dst, size_t bsize, size_t *num_writ, KTime_t ts)
{
    struct tm* tm = localtime(&ts);

    *num_writ = strftime(dst, bsize, "%a %Y-%m-%d %H:%M:%S %Z", tm);
    if( *num_writ < 1 || *num_writ >= bsize ) {
        return RC(rcExe, rcDoc, rcWriting, rcBuffer, rcInsufficient);
    }
    return 0;
}

rc_t XML_ParseBool(const KXMLNode* xml_node, const char* attr, bool* val, bool optional)
{
    rc_t rc;
    char b[16];
    size_t sz;

    if( (rc = KXMLNodeReadAttrCString(xml_node, attr, b, sizeof(b), &sz)) == 0 ) {
        if( strcasecmp(b, "true") == 0 || strcasecmp(b, "on") == 0 || strcasecmp(b, "yes") == 0 ) {
            *val = true;
        } else if( strcasecmp(b, "false") == 0 || strcasecmp(b, "off") == 0 || strcasecmp(b, "no") == 0 ) {
            *val = false;
        } else {
            rc = RC(rcExe, rcDoc, rcReading, rcAttr, rcInvalid);
        }
    } else if( optional && GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound ) {
        rc = 0;
    }
    return rc;
}
