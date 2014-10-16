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
#include <krypto/encfile.h>
#include <krypto/key.h>

typedef struct FileNode FileNode;
#define FSNODE_IMPL FileNode

#include "log.h"
#include "xml.h"
#include "file.h"
#include "kfile-accessor.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

struct FileNode {
    FSNode node;
    char* path;
    KTime_t mtime;
    KKey key;
};

static
rc_t FileNode_Attr(const FileNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;
    KDirectory* dir = NULL;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcEvaluating, rcDirEntry, rcNotFound);
    } else if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
        *type = KDirectoryPathType(dir, "%s", cself->path);
        DEBUG_LINE(8, "file type %x", *type);
        if( cself->mtime != 0 ) {
            *ts = cself->mtime;
        } else if( (rc = KDirectoryDate(dir, ts, "%s", cself->path)) == 0 ) {
            DEBUG_LINE(8, "file mtime %u", *ts);
        }
        if( rc == 0 && (rc = KDirectoryAccess(dir, access, "%s", cself->path)) == 0 ) {
            DEBUG_LINE(8, "file access %x", *access);
            if( *type & kptAlias ) {
                char r[10240];
                if( (rc = KDirectoryResolveAlias(dir, true, r, sizeof(r), "%s", cself->path)) == 0 ) {
                    *file_sz = strlen(r);
                }
            } else if( *type == kptFile ) { 
                rc = KDirectoryFileSize(dir, file_sz, "%s", cself->path);
            }
        }
        ReleaseComplain(KDirectoryRelease, dir);
    }
    return rc;
}

static
rc_t FileNode_Link(const FileNode* cself, const char* subpath, char* buf, size_t buf_sz)
{
    rc_t rc = 0;
    KDirectory* dir = NULL;

    if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
        rc = KDirectoryResolveAlias(dir, true, buf, buf_sz, "%s", cself->path);
        ReleaseComplain(KDirectoryRelease, dir);
    }
    return rc;
}

static
rc_t FileNode_Open(const FileNode* cself, const char* subpath, const SAccessor** accessor)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcNotFound);
    } else {
        KDirectory* dir = NULL;
        if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
            const KFile* kf = NULL;
            const KFile* enc_kf = NULL;
            const KFile* immediate = NULL;
            if( (rc = KDirectoryOpenFileRead(dir, &kf, "%s", cself->path)) == 0 ) {
                immediate = kf;
                if( cself->key.type != kkeyNone ) {
                    /* TODO: what is the correct way to release KFile objects */
                    rc = KEncFileMakeRead (&enc_kf, kf, &cself->key);
                    immediate = enc_kf;
                }
                if( rc == 0 ) {
                    if( (rc = KFileAccessor_Make(accessor, cself->node.name, immediate)) != 0 ) {
                        ReleaseComplain(KFileRelease, immediate);
                    }
                }
                
            }
            ReleaseComplain(KDirectoryRelease, dir);
        }
    }
    if( rc != 0 ) {
        SAccessor_Release(*accessor);
        *accessor = NULL;
    }
    return rc;
}

static
rc_t FileNode_Release(FileNode* self)
{
    if( self != NULL ) {
        FREE(self->path);
    }
    return 0;
}

static FSNode_vtbl FileNode_vtbl = {
    sizeof(FileNode),
    NULL,
    NULL,
    FileNode_Attr,
    NULL,
    FileNode_Link,
    FileNode_Open,
    FileNode_Release
};

rc_t FileNode_Make(const KXMLNode* xml_node, FSNode** cself, char* errmsg, const char* rel_path, EXMLValidate validate)
{
    rc_t rc = 0;

    if( xml_node == NULL || cself == NULL || errmsg == NULL || rel_path == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcParam, rcNull);
    } else {
        char* path = NULL, *name = NULL, name_buf[4096], password[4096];
        KTime_t ktm = 0;
        FileNode* ff = NULL;
        size_t password_sz = 0;

        if( (rc = KXMLNodeReadAttrCStr(xml_node, "path", &path, NULL)) == 0 ) {
            if( path[0] == '\0' ) {
                rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
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
                        if( (t != kptFile && t != (kptFile | kptAlias)) &&
                            (t != kptCharDev && t != (kptCharDev | kptAlias)) &&
                            (t != kptBlockDev && t != (kptBlockDev | kptAlias)) &&
                            (t != kptFIFO && t != (kptFIFO | kptAlias))  ) {
                            if( validate > eXML_NoFail ) {
                                rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, t == kptNotFound ? rcNotFound : rcInvalid);
                            } else {
                                PLOGMSG(klogErr, (klogErr, "File path '$(p)' not found", "p=%s", path));
                            }
                        }
                    }
                    ReleaseComplain(KDirectoryRelease, dir);
                }
            }
        }
        if( rc != 0 ) {
            strcpy(errmsg, "File/@path: '");
            strcat(errmsg, path ? path : "(null)");
            strcat(errmsg, "'");
        }
        if( rc == 0 ) {
            rc = KXMLNodeReadAttrCString(xml_node, "name", name_buf, sizeof(name_buf), &password_sz);
            if( rc == 0 && name_buf[0] != '\0' ) {
                name = name_buf;
            } else if( GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound ) {
                rc = 0;
            }
            if( rc != 0 ) {
                strcpy(errmsg, "File/@name");
            } else if( name == NULL ) {
                name = strrchr(path, '/');
                name = name ? name + 1 : path;
            }
        }
        if( rc == 0 && (rc = XML_ParseTimestamp(xml_node, "timestamp", &ktm, true)) != 0 ) {
            strcpy(errmsg, "File/@timestamp");
        }
        if( rc == 0 ) {
            rc = KXMLNodeReadAttrCString(xml_node, "password", password, sizeof(password), &password_sz);
            if( rc == 0 || (GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
                rc = 0;
                password_sz = 0;
            } else {
                strcpy(errmsg, "File/@password");
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
                        if( strcmp("path", attr_nm) == 0 || strcmp("name", attr_nm) == 0 ||
                            strcmp("timestamp", attr_nm) == 0 || strcmp("password", attr_nm) == 0 ) {
                            continue;
                        }
                        rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
                        strcpy(errmsg, "unknown attribute File/@");
                        strcat(errmsg, attr_nm);
                    }
                }
                ReleaseComplain(KNamelistRelease, attr);
            }
        }
        if( rc == 0 ) {
            if( (rc = FSNode_Make((FSNode**)&ff, name, &FileNode_vtbl)) == 0 ) {
                ff->path = path;
                ff->mtime = ktm;
                if( password_sz > 0 ) {
                    rc = KKeyInitRead(&ff->key, kkeyAES128, password, password_sz);
                } else {
                    memset(&ff->key, 0, sizeof ff->key);
                    ff->key.type = kkeyNone;
                }
            } else {
                strcpy(errmsg, "File '");
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
