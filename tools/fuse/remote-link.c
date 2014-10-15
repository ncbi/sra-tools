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

typedef struct RemoteLinkNode RemoteLinkNode;
#define FSNODE_IMPL RemoteLinkNode

#include "log.h"
#include "xml.h"
#include "remote-link.h"
#include "remote-cache.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

/**************************************************************
 * UMBROCHUHVYR
 * There is some info about links in that application
 * We suppose that Link is a symbolic link, which have only one
 * parameter path to the referred node. That path could be :
 *   absolute - in that case node will be searched from root of
 *              XML document describing that directory
 *   relative - in that case node will be searched from current
 *              link location.
 **************************************************************/

struct RemoteLinkNode {
    FSNode node;
    char* path;
    KTime_t mtime;
};

static
rc_t RemoteLinkNode_Attr(const RemoteLinkNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcEvaluating, rcDirEntry, rcNotFound);
    } else {
            /* Here we are: all attributes from XML */
        * type = kptFile | kptAlias;
        * ts = cself -> mtime;
        * file_sz = 5; 
        * access = 0777;
        * block_sz = ( 32 * 1024 ); /* <<-- Sorry, I borrowed that
                                     * value from KCacheTeeFile
                                     */
    }

    return rc;
}

static
rc_t RemoteLinkNode_Link(
                    const RemoteLinkNode* cself,
                    const char* subpath,
                    char * Buffer,
                    size_t BufferSize
)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcNotFound);
    } else {
        size_t pathSize = string_measure ( cself -> path, NULL );
        if ( Buffer == NULL || BufferSize <= pathSize ) {
            rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcInvalid);
        }
        else {
            string_copy ( Buffer, BufferSize, cself -> path, pathSize );
        }
    }

    return rc;
}

static
rc_t RemoteLinkNode_Release(RemoteLinkNode* self)
{
    if( self != NULL ) {
        FREE(self->path);
    }
    return 0;
}

static FSNode_vtbl RemoteLinkNode_vtbl = {
    sizeof(RemoteLinkNode),
    NULL,
    NULL,
    RemoteLinkNode_Attr,
    NULL,
    RemoteLinkNode_Link,
    NULL,
    RemoteLinkNode_Release
};

rc_t RemoteLinkNode_Make(const KXMLNode* xml_node, FSNode** cself, char* errmsg, const char* rel_path)
{
    rc_t rc = 0;

    if( xml_node == NULL || cself == NULL || errmsg == NULL || rel_path == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcParam, rcNull);
    } else {
        char* path = NULL, *name = NULL, name_buf[4096];
        KTime_t ktm = 0;
        RemoteLinkNode* ff = NULL;
        size_t attribute_sz = 0;

        if( (rc = KXMLNodeReadAttrCStr(xml_node, "path", &path, NULL)) == 0 ) {
            if( path[0] == '\0' ) {
                rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
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
        if( rc == 0 && (rc = XML_ParseTimestamp(xml_node, "timestamp", &ktm, true)) != 0 ) {
            strcpy(errmsg, "File/@timestamp");
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
                        if( strcmp("path", attr_nm) == 0
                            || strcmp("name", attr_nm) == 0
                            || strcmp("timestamp", attr_nm) == 0
                            ) {
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
            if( (rc = FSNode_Make((FSNode**)&ff, name, &RemoteLinkNode_vtbl)) == 0 ) {
                ff->path = path;
                ff->mtime = ktm;
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
