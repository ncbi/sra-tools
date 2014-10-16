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
#include <kfs/directory.h>
#include <kfs/file.h>

typedef struct TarNode TarNode;
#define FSNODE_IMPL TarNode

#include "log.h"
#include "xml.h"
#include "tar-node.h"
#include "tar-list.h"
#include "tar-file.h"
#include "kfile-accessor.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

struct TarNode {
    FSNode node;
    const TarFileList* files;
    const char* rel_path;
    /* used only in case of auto-detected .tar.xml file */
    KTime_t mtime;
    const char* xml_path;
};

static
rc_t TarNode_MakeFileList(const KXMLNode* xml_node, const TarFileList** files, char* errmsg, const char* rel_path, const char* name)
{
    rc_t rc = 0;
    time_t now = time(NULL);
    uint32_t count = 0;

    *files = NULL;
    if( (rc = KXMLNodeCountChildNodes(xml_node, &count)) == 0 ) {
        if( count == 0 ) {
            rc = RC(rcExe, rcDoc, rcValidating, rcData, rcEmpty);
        } else if( (rc = TarFileList_Make(files, count, name)) == 0 ) {
            uint32_t i = 0;
            while(rc == 0 && i < count) {
                const KXMLNode* n = NULL;
                const char* n_name;
                if( (rc = KXMLNodeGetNodeRead(xml_node, &n, i++)) == 0 && (rc = KXMLNodeElementName(n, &n_name)) == 0 ) {
                    if( strcmp(n_name, "Item") != 0 ) {
                        rc = RC(rcExe, rcDoc, rcValidating, rcNode, rcUnexpected);
                        strcpy(errmsg, n_name);
                    } else {
                        size_t sz_read;
                        char path[4096], name[4096];
                        KTime_t ts = now;
                        uint64_t fsz = 0;
                        bool exec = false;

                        if( (rc = KXMLNodeReadAttrCString(n, "path", name, sizeof(name), &sz_read)) == 0 ) {
                            if( name[0] == '\0' ) {
                                rc = RC(rcExe, rcDoc, rcValidating, rcAttr, rcEmpty);
                            } else if( name[0] == '/' ) {
                                memcpy(path, name, sz_read + 1);
                            } else {
                                KDirectory* dir = NULL;
                                if( (rc = KDirectoryNativeDir(&dir)) == 0 &&
                                    (rc = KDirectoryResolvePath(dir, true, path, sizeof(path), "%s/%s", rel_path, name)) == 0 ) {
                                    DEBUG_LINE(8, "%s/%s resolved to %s", rel_path, name, path);
                                }
                                KDirectoryRelease(dir);
                            }
                            if( rc != 0 ) {
                                strcpy(errmsg, "TAR/Item/@path");
                            }
                        }
                        if( rc == 0 && (rc = XML_ParseTimestamp(n, "timestamp", &ts, true)) != 0 ) {
                            strcpy(errmsg, "TAR/Item/@timestamp");
                        }
                        if( rc == 0 && (rc = XML_ParseBool(n, "executable", &exec, true)) != 0 ) {
                            strcpy(errmsg, "TAR/Item/@executable");
                        }
                        if( rc == 0 && (rc = KXMLNodeReadAttrAsU64(n, "size", &fsz)) != 0 ) {
                            strcpy(errmsg, "TAR/Item/@size");
                        }
                        if( rc == 0 ) {
                            name[0] = '\0';
                            rc = KXMLNodeReadAttrCString(n, "name", name, sizeof(name), &sz_read);
                            if( (GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) || name[0] == '\0' ) {
                                char* x = strrchr(path, '/');
                                strcpy(name, x ? x + 1 : path);
                                rc = 0;
                            } else if( rc == 0 && name[0] == '/' ) {
                                strcat(errmsg, "Item/@name cannot be absolute");
                                rc = RC(rcExe, rcDoc, rcValidating, rcName, rcInvalid);
                            } else if( rc != 0 ) {
                                strcpy(errmsg, "Item/@name");
                            }
                        }
                        if( rc == 0 ) {
                            const KNamelist* attr = NULL;
                            if( (rc = KXMLNodeListAttr(n, &attr)) == 0 ) {
                                uint32_t j = 0, count = 0;
                                if( (rc = KNamelistCount(attr, &count)) == 0 && count > 0 ) {
                                    while( rc == 0 && j < count ) {
                                        const char *attr_nm = NULL;
                                        if( (rc = KNamelistGet(attr, j++, &attr_nm)) != 0 ) {
                                            break;
                                        }
                                        if( strcmp("path", attr_nm) == 0 || strcmp("name", attr_nm) == 0 ||
                                            strcmp("timestamp", attr_nm) == 0 || strcmp("size", attr_nm) == 0 ||
                                            strcmp("executable", attr_nm) == 0 ) {
                                            continue;
                                        }
                                        rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
                                        strcpy(errmsg, "unknown attribute TAR/Item/@");
                                        strcat(errmsg, attr_nm);
                                    }
                                }
                                ReleaseComplain(KNamelistRelease, attr);
                            }
                        }
                        if( rc == 0 && (rc = TarFileList_Add(*files, path, name, fsz, ts, exec)) != 0 ) {
                            strcpy(errmsg, "adding to TAR");
                        }
                    }
                    ReleaseComplain(KXMLNodeRelease, n);
                }
            }
            if( rc != 0 ) {
                TarFileList_Release(*files);
                *files = NULL;
            }
        }
    }
    return rc;
}

static
rc_t TarNode_Touch(const TarNode* cself)
{
    rc_t rc = 0;

    if( cself->xml_path != NULL ) {
        KDirectory* dir = NULL;
        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            KTime_t dt;
            if( (rc = KDirectoryDate(dir, &dt, "%s", cself->xml_path)) == 0 ) {
                if( dt != cself->mtime ) {
                    const KFile* kfile = NULL;
                    DEBUG_MSG(8, ("%s: updating tar %s\n", __func__, cself->xml_path));
                    if( (rc = KDirectoryOpenFileRead(dir, &kfile, "%s", cself->xml_path)) == 0 ) {
                        const KXMLMgr* xmlmgr;
                        if( (rc = XML_MgrGet(&xmlmgr)) == 0 ) {
                            const KXMLDoc* xmldoc = NULL;
                            if( (rc = KXMLMgrMakeDocRead(xmlmgr, &xmldoc, kfile)) == 0 ) {
                                const KXMLNodeset* ns = NULL;
                                if( (rc = KXMLDocOpenNodesetRead(xmldoc, &ns, "/TAR")) == 0 ) {
                                    uint32_t count = 0;
                                    if( (rc = KXMLNodesetCount(ns, &count)) == 0 ) {
                                        if( count != 1 ) {
                                            rc = RC(rcExe, rcDoc, rcValidating, rcData, rcInvalid);
                                        } else {
                                            const KXMLNode* n = NULL;
                                            if( (rc = KXMLNodesetGetNodeRead(ns, &n, 0)) == 0 ) {
                                                char errmsg[4096];
                                                const TarFileList* new_files;
                                                if( (rc = TarNode_MakeFileList(n, &new_files, errmsg, cself->rel_path, cself->node.name)) != 0 ) {
                                                    LOGERR(klogErr, rc, errmsg);
                                                } else {
                                                    TarFileList_Release(cself->files);
                                                    ((TarNode*)cself)->files = new_files;
                                                    ((TarNode*)cself)->mtime = dt;
                                                }
                                                ReleaseComplain(KXMLNodeRelease, n);
                                            }
                                        }
                                    }
                                    ReleaseComplain(KXMLNodesetRelease, ns);
                                }
                                ReleaseComplain(KXMLDocRelease, xmldoc);
                            }
                        }
                        ReleaseComplain(KFileRelease, kfile);
                    }
                }
            }
            ReleaseComplain(KDirectoryRelease, dir);
        }
    }
    return rc;
}

static
rc_t TarNode_Attr(const TarNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcEvaluating, rcDirEntry, rcNotFound);
    } else {
        *type = kptFile;
        rc = TarFileList_Size(cself->files, file_sz);
        if( cself->xml_path != NULL ) {
            *ts = cself->mtime;
        }
    }
    return rc;
}

static
rc_t TarNode_Open(const TarNode* cself, const char* subpath, const SAccessor** accessor)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcNotFound);
    } else {
        const KFile* kf = NULL;
        if( (rc = TarFile_Open(&kf, cself->files)) == 0 ) {
            if( (rc = KFileAccessor_Make(accessor, cself->node.name, kf)) != 0 ) {
                ReleaseComplain(KFileRelease, kf);
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
rc_t TarNode_Release(TarNode* self)
{
    if( self != NULL ) {
        DEBUG_MSG(8, ("%s: %s\n", __func__, self->xml_path));
        TarFileList_Release(self->files);
    }
    return 0;
}

static FSNode_vtbl TarNode_vtbl = {
    sizeof(TarNode),
    NULL,
    TarNode_Touch,
    TarNode_Attr,
    NULL,
    NULL,
    TarNode_Open,
    TarNode_Release
};

rc_t TarNode_MakeXML(const KXMLNode* xml_node, FSNode** self, char* errmsg, const char* rel_path)
{
    rc_t rc = 0;

    if( xml_node == NULL || self == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcParam, rcNull);
    } else {
        char name[4096];
        size_t sz;

        rc = KXMLNodeReadAttrCString(xml_node, "name", name, sizeof(name), &sz);
        if( rc != 0 || sz == 0 || name[0] == '\0' ) {
            strcpy(errmsg, "attribute 'name'");
            rc = rc ? rc : RC(rcExe, rcDoc, rcValidating, rcAttr, rcInvalid);
        } else if( (rc = FSNode_Make(self, name, &TarNode_vtbl)) == 0 ) {
            ((TarNode*)*self)->rel_path = rel_path;
            strcpy(errmsg, "processing Item(s)");
            if( (rc = TarNode_MakeFileList(xml_node, &((TarNode*)*self)->files, errmsg, rel_path, (*self)->name)) != 0 ) {
                FSNode_Release(*self);
                *self = NULL;
            }
        }
    }
    return rc;
}

rc_t TarNode_MakeAuto(const FSNode** cself, const KDirectory* dir, const char* path, const char* file, const char* xml_path)
{
    rc_t rc = 0;

    if( cself == NULL || dir == NULL || path == NULL || file == NULL || xml_path == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcParam, rcNull);
    } else {
        if( (rc = FSNode_Make((FSNode**)cself, file, &TarNode_vtbl)) == 0 ) {
            ((char*)((*cself)->name))[strlen(file) - 4] = '\0'; /* -= .xml */
            ((TarNode*)*cself)->xml_path = xml_path;
            ((TarNode*)*cself)->rel_path = path;
            if( (rc = FSNode_Touch(*cself)) != 0 ) {
                FSNode_Release(*cself);
                *cself = NULL;
            }
        }
    }
    return rc;
}
