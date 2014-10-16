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
#include <klib/printf.h>
#include <klib/out.h>
#include <kproc/lock.h>
#include <kproc/thread.h>
#include <kns/manager.h>
#include <kns/http.h>

#include "log.h"
#include "remote-xml.h"
#include "file.h"
#include "node.h"

#include "remote-file.h"
#include "remote-directory.h"
#include "remote-link.h"
#include "remote-cache.h"

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

static unsigned int g_xml_sync = 0;
/* if g_xml_sync == 0 these are not used: */
static KTime_t g_xml_mtime = 0;
static char* g_xml_path = NULL;
static KThread* g_xml_thread = NULL;

static char g_fuse_version [ 128 ];
static char g_heart_beat_url_complete [ 4096 ];
static char * g_heart_beat_url = NULL;
static char * g_heart_beat_url_complete_p = NULL;

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
rc_t XML_ValidateNode(FSNode* parent, const KXMLNode* n, char* errmsg)
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
        rc = RemoteDirectoryNode_Make(n, &fsn, errmsg, g_start_dir, g_xml_mtime);
        children_allowed = true;
    } else if( strcmp(name, "File") == 0 ) {
        rc = RemoteFileNode_Make(n, &fsn, errmsg, g_start_dir);
    } else if( strcmp(name, "Link") == 0 ) {
        rc = RemoteLinkNode_Make(n, &fsn, errmsg, g_start_dir);
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
                            rc = XML_ValidateNode(fsn, ch, errmsg);
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
}   /* XML_ValidateNode() */

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

/*)))
 ///  I make that method becaue I need also to retrieve version
(((*/
static
rc_t _XML_DocumentOpenAndSomethingElse (
                            const char * Url,
                            const KXMLDoc ** Doc
)
{
    rc_t RCt;
    char * Buffer;
    uint64_t BufferSize;
    const KXMLDoc * XmlDoc;
    const KXMLNodeset * NodeSet;
    uint32_t NodeCount;
    size_t AttrSize;
    const KXMLNode * Node;

    RCt = 0;
    Buffer = NULL;
    BufferSize = 0;
    XmlDoc = NULL;
    NodeSet = NULL;
    NodeCount = AttrSize = 0;
    Node = NULL;

    if ( Url == NULL || Doc == NULL ) {
        return RC ( rcXML, rcDoc, rcConstructing, rcParam, rcNull );
    }
    * Doc = NULL;

    RCt = IsLocalPath ( Url )
            ? ReadLocalFileToMemory ( Url, & Buffer, & BufferSize )
            : ReadHttpFileToMemory ( Url, & Buffer, & BufferSize )
            ;
    if ( RCt == 0 ) {
        RCt = KXMLMgrMakeDocReadFromMemory (
                                        g_xmlmgr,
                                        & XmlDoc,
                                        Buffer,
                                        BufferSize
                                        );
        if ( RCt == 0 ) {
                /* Here we are ... retrieving '/FUSE' node
                 */
            RCt = KXMLDocOpenNodesetRead (
                                        XmlDoc,
                                        &NodeSet,
                                        "/FUSE"
                                        );
            if ( RCt == 0 ) {
                RCt = KXMLNodesetCount ( NodeSet, & NodeCount );
                if ( RCt == 0 ) {
                    if ( NodeCount != 1 ) {
                        RCt = RC ( rcXML, rcDoc, rcConstructing, rcFormat, rcInvalid );
                    }
                    if ( RCt == 0 ) {
                            /*  FUSE node is always alone
                             */
                        RCt = KXMLNodesetGetNodeRead (
                                                    NodeSet,
                                                    & Node,
                                                    0
                                                    );
                        if ( RCt == 0 ) {
                            RCt = KXMLNodeReadAttrCString (
                                        Node,
                                        "version",
                                        g_fuse_version,
                                        sizeof ( g_fuse_version ),
                                        & AttrSize
                                        );
                            if ( RCt != 0 ) {
                                /* There were no version defined */
                                RmOutMsg ( "WARNING: No version provided\n" );
                                RCt = 0;
                            }
                            else {
                                /* TODO : set version  */
                                if ( g_heart_beat_url != NULL ) {
                                    RCt = string_printf (
                                        g_heart_beat_url_complete,
                                        sizeof ( g_heart_beat_url_complete ),
                                        & AttrSize,
                                        g_heart_beat_url,
                                        g_fuse_version
                                        );
                                    if ( RCt == 0 ) {
                                        g_heart_beat_url_complete_p = g_heart_beat_url_complete;
                                    }
                                }
                            }

                            ReleaseComplain ( KXMLNodeRelease, Node );
                        }
                    }
                }

                ReleaseComplain ( KXMLNodesetRelease, NodeSet );
            }
        }

        free ( Buffer );
    }

    if ( RCt == 0 ) {
        * Doc = XmlDoc;
    }
    else {
        ReleaseComplain ( KXMLDocRelease, XmlDoc );
    }

    return RCt;
}   /* _XML_DocumentOpenAndSomethingElse () */

static
rc_t XML_Open(const char* path, const FSNode** tree)
{
    rc_t rc = 0;
    char errmsg[4096] = "";
    const KXMLDoc* xmldoc = NULL;
    
    PLOGMSG(klogInfo, (klogInfo, "Reading XML file '$(x)'", PLOG_S(x), path));
    if( (rc = _XML_DocumentOpenAndSomethingElse(path, &xmldoc)) == 0 ) {

        if( (rc = FSNode_Make((FSNode**)tree, "ROOT", &RootNode_vtbl)) == 0 ) {
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
                                errmsg[0] = '\0';
                                rc = XML_ValidateNode((FSNode*)*tree, n, errmsg);
                                ReleaseComplain(KXMLNodeRelease, n);
                            }
                        }
                    }
                }
                ReleaseComplain(KXMLNodesetRelease, ns);
            }
        }
        if( rc != 0 ) {
            FSNode_Release(*tree);
            *tree = NULL;
        }
        ReleaseComplain(KXMLDocRelease, xmldoc);
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
    /* rc_t rc = 0; */

    PLOGMSG(klogInfo, (klogInfo, "Heart beat thread started with $(s) sec", PLOG_U32(s), g_xml_sync));

    do {
        sleep(g_xml_sync);

        PLOGMSG(klogInfo, (klogInfo, "Heart beat working $(s)", PLOG_S(s), g_heart_beat_url_complete_p));

        if ( g_heart_beat_url_complete_p != NULL ) {
            /* rc = */ ExecuteCGI ( g_heart_beat_url_complete_p );
        }
    } while( g_xml_sync > 0 );
    LOGMSG(klogInfo, "Heart beat thread ended");
    return 0;
}

rc_t XML_Make(KDirectory* dir, const char* const work_dir, const char* xml_path, const char *heart_beat_url, unsigned int sync)
{
    rc_t rc = 0; 

    g_xml_sync = sync;
    if( g_xmlmgr == NULL && (rc = KXMLMgrMakeRead(&g_xmlmgr)) != 0 ) {
        g_xmlmgr = NULL;
        LOGERR(klogErr, rc, "XML manager");
    } else {
        if( (rc = StrDup(xml_path, &g_xml_path)) == 0 ) {
            DEBUG_MSG(8, ("XML path set to '%s'\n", g_xml_path));
        }
        if ( heart_beat_url != NULL ) {
            if( (rc = StrDup(heart_beat_url, & g_heart_beat_url) ) == 0 ) {
                DEBUG_MSG(8, ("Heart Beat path set to '%s'\n", g_heart_beat_url));
            }
        }
        else {
            g_heart_beat_url = NULL;
        }

        g_start_dir = work_dir;
    }
    if( rc == 0 ) {
        rc = FSNode_Make((FSNode**)&g_root, "ROOT", &RootNode_vtbl);
    }
    return rc;
}

void FUSER_abort ();

void XML_Init(void)
{
    rc_t rc = 0;

    * g_fuse_version = 0;
    * g_heart_beat_url_complete = 0;

    if( g_lock == NULL && (rc = KRWLockMake(&g_lock)) != 0 ) {
        g_lock = NULL;
        LOGERR(klogErr, rc, "XML lock");
    }

    PLOGMSG(klogInfo, (klogInfo, "XML file is URL '$(x)' ok", PLOG_S(x), g_xml_path));

    rc = RemoteCacheCreate ();
    if ( rc == 0 ) {
        rc = XML_Open ( g_xml_path, & g_root );
        if ( rc == 0 ) {
                /*) Here we are starting special thtread, only if 
                 /  users set up URL
                (*/
            if ( rc == 0 && g_heart_beat_url_complete_p != NULL ) {
                if( (rc = KThreadMake(&g_xml_thread, XMLThread, NULL)) != 0 ) {
                    LOGERR(klogErr, rc, "XML sync thread");
                }
            }
            else {
                LOGMSG(klogInfo, "Skipping starting of heart beat thread");
            }
        }
    }

    if ( rc != 0 ) {
        FUSER_abort ();
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

    RemoteCacheDispose ();

    g_root = NULL;
    g_lock = NULL;
    g_start_dir = NULL;
    g_xml_mtime = 0;
    g_xml_path = NULL;
    g_xml_thread = NULL;

    * g_fuse_version = 0;
    if ( g_heart_beat_url != NULL ) {
        FREE(g_heart_beat_url);
    }
    g_heart_beat_url = NULL;
    * g_heart_beat_url_complete = 0;
    g_heart_beat_url_complete_p = NULL;
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
