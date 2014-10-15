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

typedef struct RemoteFileNode RemoteFileNode;
#define FSNODE_IMPL RemoteFileNode

#include "log.h"
#include "xml.h"
#include "remote-file.h"
#include "remote-cache.h"
#include "kfile-accessor.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

rc_t RemoteFileAccessor_Make(const SAccessor** accessor, const char* name, struct RCacheEntry * rentry, uint64_t size);

struct RemoteFileNode {
    FSNode node;
    char* path;
    KTime_t mtime;
    uint64_t file_sz;
};

static
rc_t RemoteFileNode_Attr(const RemoteFileNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcEvaluating, rcDirEntry, rcNotFound);
    } else {
            /* Here we are: all attributes from XML */
        * type = kptFile;
        * ts = cself -> mtime;
        * file_sz = cself -> file_sz; 
        * access = 0444;
        * block_sz = ( 32 * 1024 ); /* <<-- Sorry, I borrowed that
                                     * value from KCacheTeeFile
                                     */
    }

    return rc;
}

static
rc_t RemoteFileNode_Open(
                    const RemoteFileNode* cself,
                    const char* subpath,
                    const SAccessor** accessor
)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcNotFound);
    } else {
        struct RCacheEntry * ke = NULL;
        if ( ( rc = RemoteCacheFindOrCreateEntry( cself->path, &ke )) == 0 ) {
            if( rc == 0 ) {
                if ( ( rc = RemoteFileAccessor_Make(
                                                accessor,
                                                cself->node.name,
                                                ke,
                                                cself->file_sz
                                                ) ) != 0 ) {
                    ReleaseComplain(RCacheEntryRelease, ke);
                }
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
rc_t RemoteFileNode_Release(RemoteFileNode* self)
{
    if( self != NULL ) {
        FREE(self->path);
    }
    return 0;
}

static FSNode_vtbl RemoteFileNode_vtbl = {
    sizeof(RemoteFileNode),
    NULL,
    NULL,
    RemoteFileNode_Attr,
    NULL,
    NULL,   /* Unlike FileNode, there are no links */
    RemoteFileNode_Open,
    RemoteFileNode_Release
};

rc_t RemoteFileNode_Make(const KXMLNode* xml_node, FSNode** cself, char* errmsg, const char* rel_path)
{
    rc_t rc = 0;

    if( xml_node == NULL || cself == NULL || errmsg == NULL || rel_path == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcParam, rcNull);
    } else {
        char* path = NULL, *name = NULL, name_buf[4096];
        KTime_t ktm = 0;
        uint64_t fsz = 0;
        RemoteFileNode* ff = NULL;
        size_t attribute_sz = 0;

        if( (rc = KXMLNodeReadAttrCStr(xml_node, "path", &path, NULL)) == 0 ) {
            if( path[0] == '\0' ) {
                rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
            } else {
                /* NOTE: we do not check path for existence */
                if ( ! IsRemotePath ( path ) ) {
                    rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
                }
            }
        }
        if( rc != 0 ) {
            strcpy(errmsg, "File/@path: '");
            strcat(errmsg, path ? path : "(null)");
            strcat(errmsg, "'");
        }
        if( rc == 0 ) {
            rc = KXMLNodeReadAttrCString(xml_node, "name", name_buf, sizeof(name_buf), &attribute_sz);
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
        if( rc == 0 && (rc = KXMLNodeReadAttrAsU64(xml_node, "size", &fsz)) != 0 ) {
            strcpy(errmsg, "File/@size");
        }
/* Attributes "size" and "timestamp" are mandatory */
        if ( fsz <= 0 ) {
            strcpy(errmsg, "File/@size");
            rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
        }
        if( rc == 0 && (rc = XML_ParseTimestamp(xml_node, "timestamp", &ktm, true)) != 0 ) {
            strcpy(errmsg, "File/@timestamp");
        }
        if ( ktm == 0 ) {
            strcpy(errmsg, "File/@timestamp");
            rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
        }
/* Attributes "size" and "timestamp" are mandatory */
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
                        if( strcmp("path", attr_nm) == 0
                            || strcmp("name", attr_nm) == 0
                            || strcmp("timestamp", attr_nm) == 0
                            || strcmp("size", attr_nm) == 0 ) {
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
            if( (rc = FSNode_Make((FSNode**)&ff, name, &RemoteFileNode_vtbl)) == 0 ) {
                ff->path = path;
                ff->mtime = ktm;
                ff->file_sz = fsz;
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

/*)))
 ///  Remote file accessor. Read behaviour differs from plain file
(((*/

typedef struct RemoteFileAccessor_struct {
    struct RCacheEntry* rentry;
    uint64_t size;
} RemoteFileAccessor;

static
rc_t RemoteFileAccessor_Read(const SAccessor* cself, char* buf, size_t size, off_t offset, size_t* num_read)
{
    rc_t rc = 0;
    RemoteFileAccessor* self = (RemoteFileAccessor*)cself;
    size_t actual = 0;
    uint64_t actual_file_sz = self -> size;

        /* Here we are truncating size if it is needed */
    if ( actual_file_sz < offset + size ) {
        size = actual_file_sz - offset;
    }
    do {
        rc = RCacheEntryRead (
                    self -> rentry,
                    &buf[*num_read],
                    size - * num_read,
                    offset + * num_read,
                    &actual
                    );
        if( rc == 0 && actual == 0 ) {
            /* EOF */
            break;
        }
        *num_read += actual;
    } while(rc == 0 && *num_read < size);
    DEBUG_MSG(10, ("From %lu read %lu bytes\n", offset, *num_read));

    return rc;
}

static
rc_t RemoteFileAccessor_Release(const SAccessor* cself)
{
    rc_t rc = 0;
    if( cself != NULL ) {
        RemoteFileAccessor* self = (RemoteFileAccessor*)cself;
        rc = RCacheEntryRelease(self->rentry);
    }
    return rc;
}

rc_t
RemoteFileAccessor_Make (
                    const SAccessor** accessor,
                    const char* name,
                    struct RCacheEntry * rentry,
                    uint64_t size
)
{
    rc_t rc = 0;

    if( (rc = SAccessor_Make(accessor, sizeof(RemoteFileAccessor), name, RemoteFileAccessor_Read, RemoteFileAccessor_Release)) == 0 ) {
        ((RemoteFileAccessor*)(*accessor))->rentry = rentry;
        ((RemoteFileAccessor*)(*accessor))->size = size;

        RCacheEntryAddRef ( rentry );
    }
    return rc;
}
