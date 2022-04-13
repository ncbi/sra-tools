/*******************************************************************************
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
 * =============================================================================
 */

#include "xtoc-priv.h" /* XTocParseXml */

#include <kxml/xml.h> /* KXMLNode */
/* #include <klib/log.h> LOGERR */
#include <klib/debug.h> /* DBGMSG */
#include <klib/printf.h>
#include <klib/out.h>
#include <klib/rc.h> /* RC */

#include "os-native.h"

#include <sysalloc.h>

#include <assert.h>
#include <stdio.h> /* printf */
#include <stdlib.h> /* free */
#include <string.h> /* memset */
#include <time.h>      /* timegm */
#include <os-native.h> /* timegm */

typedef struct KFile KFile;

/*#define DISP_RC(rc, msg) (void) ( (rc == 0) ? 0 : LOGERR(klogInt, rc, msg) )*/
#define DISP_RC(rc, msg) (void) ( 0 )

/* copy-paste from libs/kfs/md5.c */

static
int hex_to_int ( char hex )
{
    int i = hex - '0';
    if ( hex > '9' )
    {
        if ( hex < 'a' )
            i = hex - 'A' + 10;
        else
            i = hex - 'a' + 10;

        if ( i > 15 )
            return -1;
    }
    return i;
}

static
rc_t MD5SumExtract(const char* str, uint8_t digest[16])
{
    rc_t rc = 0;

    int i = 0;
    /* parse checksum */
    for ( i = 0; i < 16; ++ i )
    {
        int l, u = hex_to_int ( str [ i + i + 0 ] );
        l = hex_to_int ( str [ i + i + 1 ] );
        if ( u < 0 || l < 0 )
        {
            rc = RC ( rcFS, rcXmlDoc, rcReading, rcFormat, rcInvalid );
            break;
        }

        digest [ i ] = ( uint8_t ) ( ( u << 4 ) | l );
    }

    return rc;
}

typedef enum NodeType {
     eUndefined
    ,eROOT
    ,eArchive
    ,eContainer
    ,eDirectory
    ,eFile
    ,eSymlink
} NodeType;
typedef struct NodeData {
    const char* nodeName;

/* need to free */
    char* nodeValue;

/* attributes */
    char* id;
    char* path;
    char* name;
    char* mtime;
    char* filetype;
    char* md5;
    char* crc32;
    char* size;
    char* offset;

/* parsed node information */
    NodeType nodeType;
    uint8_t digest[16];
    uint64_t iSize;
    uint64_t iOffset;
    KTime_t tMtime;
} NodeData;

static
NodeType GetNodeType(const char* nodeName)
{
    NodeType t = eUndefined;

    if (nodeName == NULL) {
        t = eUndefined;
    }
    else if (!strcmp(nodeName, "archive")) {
        t = eArchive;
    }
    else if (!strcmp(nodeName, "container")) {
        t = eContainer;
    }
    else if (!strcmp(nodeName, "directory")) {
        t = eDirectory;
    }
    else if (!strcmp(nodeName, "file")) {
        t = eFile;
    }
    else if (!strcmp(nodeName, "symlink")) {
        t = eSymlink;
    }
    else if (strstr(nodeName, "ROOT")) {
        t = eROOT;
    }

    return t;
}

#define DEBUG_PRINT false /* true false */

/* TODO: check possible errors */
/* Reverse to KTimePrint for tools/copycat/cctree-dump.c */
static
rc_t CC StrToKTime(const char* str, KTime_t* t)
{
    rc_t rc = 0;

    assert(t);

    if (str) {
        int y, m, d, hr, mn, sc;
        struct tm gmt;
        memset(&gmt, 0, sizeof gmt);
        sscanf(str, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                                &y, &m, &d, &hr, &mn, &sc);
        gmt.tm_year = y - 1900;
        gmt.tm_mon  = m - 1;
        gmt.tm_mday = d;
        gmt.tm_hour = hr;
        gmt.tm_min  = mn;
        gmt.tm_sec  = sc;
        *t = timegm(&gmt);

        if (DEBUG_PRINT) {
            time_t t2 = ( time_t ) * t;
            size_t len;
            char buffer [ 64 ];

            struct tm gmt;
#if SUN
	    gmt = * gmtime ( & t2 );
#else
            gmtime_r ( & t2, & gmt );
#endif
            rc = string_printf ( buffer, sizeof buffer, & len
                , "%04d-%02d-%02dT%02d:%02d:%02dZ"
                , gmt . tm_year + 1900
                , gmt . tm_mon + 1
                , gmt . tm_mday
                , gmt . tm_hour
                , gmt . tm_min
                , gmt . tm_sec
            );
            
            OUTMSG((">> %s\n", str));
            OUTMSG(("<< %s\n", buffer));
        }
    }

    return rc;
}

#define READ_ATTR(NODE, V) \
    if (rc == 0) { \
        rc = KXMLNodeReadAttrCStr(NODE, #V, &data->V, NULL); \
        if (rc != 0) { \
            if (GetRCState(rc) == rcNotFound) \
                rc = 0; \
        } \
    }
/*          else if (data->nodeName) \
                PLOGERR(klogErr, (klogErr, rc, \
                    "while calling KXMLNodeReadAttrCStr($(name)/@$(attr))", \
                    "name=%s,attr=%s", data->nodeName, #V)); */

#define PRN_ATTR(NM, V) \
    if (DEBUG_PRINT && data->V) OUTMSG(("%s=\"%s\" ", NM, data->V))

#define GET_ATTR(NODE, V) READ_ATTR(NODE, V); PRN_ATTR(#V, V)

static rc_t CC NodeDataReadAttribs(NodeData* data, const KXMLNode* node,
    const char* parentName, uint32_t idx)
{
    rc_t rc = 0;

    assert(node && parentName && data);

    if (DEBUG_PRINT) OUTMSG(("<%s ", data->nodeName));

    GET_ATTR(node, id);
    GET_ATTR(node, path);
    GET_ATTR(node, name);
    GET_ATTR(node, mtime);
    GET_ATTR(node, filetype);
    GET_ATTR(node, md5);
    GET_ATTR(node, crc32);

    READ_ATTR(node, size);
    READ_ATTR(node, offset);

    if (rc == 0)
    {
/* TODO: what size is negative no error is detected */
        char * attr = "size";
        rc = KXMLNodeReadAttrAsU64(node, attr, &data->iSize);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound) {
                rc = 0;
            }
        }

        attr = "offset";
        rc = KXMLNodeReadAttrAsU64(node, attr, &data->iOffset);
        if (rc != 0) {
            if (GetRCState(rc) == rcNotFound) {
                rc = 0;
            }
        }
/*          else if (data->nodeName) {
                PLOGERR(klogErr, (klogErr, rc,
                    "while calling KXMLNodeReadAttrAsU64($(name)/@$(attr))",
                    "name=%s,attr=%s", data->nodeName, attr));
            }*/
        if (DEBUG_PRINT) 
        {
            OUTMSG(("%s=\"%lu\" ", attr, data->iSize));
            OUTMSG(("%s=\"%lu\" ", attr, data->iOffset));
        }
    }

    if (DEBUG_PRINT) OUTMSG((">"));
    if (DEBUG_PRINT && data->nodeValue) OUTMSG(("%s", data->nodeValue));
    if (DEBUG_PRINT) OUTMSG(("</%s>\n", data->nodeName));

    return rc;
}

#define FREE(V) free(data->V); data->V = NULL

static
rc_t CC NodeDataDestroy(NodeData* data)
{
    assert(data);

    FREE(nodeValue);
    FREE(id);
    FREE(path);
    FREE(name);
    FREE(mtime);
    FREE(filetype);
    FREE(md5);
    FREE(crc32);
    FREE(size);
    FREE(offset);

    return 0;
}

static rc_t CC NodeDataInit(NodeData* data, const KXMLNode* node,
    const char* parentName, uint32_t idx)
{
    rc_t rc = 0;

    assert(node && parentName && data);

    memset(data, 0, sizeof *data);

    rc = KXMLNodeGetName(node, &data->nodeName);
/*  if (rc != 0) {
        PLOGERR(klogErr, (klogErr, rc,
            "while calling KXMLNodeGetName($(parent)[$(i)]",
            "parent=%s,i=%d", parentName, idx));
    }*/

    if (rc == 0
        && data->nodeName && !strcmp(data->nodeName, "symlink"))
    {
        rc = KXMLNodeReadCStr(node, &data->nodeValue, NULL);
/*      if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "while calling KXMLNodeReadCStr($(name))",
                "name=%s", data->nodeName));
        }*/
    }

    if (rc == 0) {
        rc = NodeDataReadAttribs(data, node, parentName, idx);
    }

    if (rc == 0) {
        data->nodeType = GetNodeType(data->nodeName);

        if (data->md5) {
            if (strlen(data->md5) != 32) {
                rc = RC(rcFS, rcXmlDoc, rcReading, rcFormat, rcInvalid);
/*              PLOGERR(klogErr, (klogErr, rc,
                    "md5sum '$(md5)'", "md5=%s", data->md5));*/
            }
            else {
                rc = MD5SumExtract(data->md5, data->digest);
            }
        }
    }

    if (rc == 0) {
        rc = StrToKTime(data->mtime, &data->tMtime);
    }

    return rc;
}

static
rc_t CC NodeDataAddToXToc(NodeData* data,
                          struct XTocEntry* xSelf, struct XTocEntry* xContainer,
                          struct XTocEntry** xEntry, bool *isContainer)
{
    rc_t rc = 0;

    assert(data && xEntry && isContainer);

    *isContainer = false;

    switch (data->nodeType) {
        case eUndefined:
            rc = RC(rcFS, rcTocEntry, rcInflating, rcData, rcInvalid);
/*          LOGERR(klogErr, rc, "undefined node type in DataXAdd");*/
            break;
        case eROOT: /* ignore root node */
            *xEntry = xSelf;
            break;
        case eArchive:
            *isContainer = true;
            rc = XTocTreeAddArchive(xSelf, xEntry, xContainer,
                data->name, data->tMtime, data->id, 
                data->filetype, data->iSize, data->iOffset, data->digest);
            break;
        case eContainer:
            *isContainer = true;
            rc = XTocTreeAddContainer(xSelf, xEntry, xContainer,
                data->name, data->tMtime, data->id, 
                data->filetype, data->iSize, data->iOffset, data->digest);
            break;
        case eDirectory:
            rc = XTocTreeAddDir(xSelf, xEntry, xContainer,
                data->name, data->tMtime);
            break;
        case eFile:
            rc = XTocTreeAddFile(xSelf, xEntry, xContainer,
                data->name, data->tMtime,  data->id, 
                data->filetype, data->iSize, data->iOffset, data->digest);
            break;
        case eSymlink:
            rc = XTocTreeAddSymlink(xSelf, xEntry, xContainer,
                data->name, data->tMtime, data->nodeValue);
            break;
    }

    return rc;
}

static
rc_t ProcessNode(const KXMLNode* node,
    const char* parentName, uint32_t idx,
    struct XTocEntry* xSelf, struct XTocEntry* xContainer)
{
    rc_t rc = 0;
    uint32_t count = 0;
    uint32_t i = 0;
    struct XTocEntry* xEntry = NULL;
    bool isContainer = false;
    NodeData data;

    assert(node && parentName);

    if (rc == 0) {
        rc = NodeDataInit(&data, node, parentName, idx);
    }

    if (rc == 0) {
        DBGMSG(DBG_APP,DBG_COND_1,
            ("%s[%i] = %s\n", parentName, idx, data.nodeName));
        rc = NodeDataAddToXToc(&data, xSelf, xContainer, &xEntry, &isContainer);
        if (isContainer) {
            xContainer = xEntry;
        }
    }

    if (rc == 0) {
        rc = KXMLNodeCountChildNodes(node, &count);
        if (rc != 0) {
/*          PLOGERR(klogErr, (klogErr, rc,
                "while calling KXMLNodeCountChildNodes($(parent)[$(i)])",
                "parent=%s,i=%d", parentName, idx));*/
        }
        else {
/*          DBGMSG(DBG_APP,DBG_COND_1,
                ("KXMLNodeCountChildNodes(%s) = %i\n", data.nodeName, count));*/
        }
    }

    for (i = 0; i < count && rc == 0; ++i) {
        const KXMLNode* child = NULL;

        rc = KXMLNodeGetNodeRead(node, &child, i);
        if (rc != 0) {
/*          PLOGERR(klogErr, (klogErr, rc,
                "while calling KXMLNodeCountChildNodes($(parent)[$(i)][$(j)]",
                "parent=%s,i=%d,j=%d", parentName, idx, i));*/
        }
        else {
/*          DBGMSG(DBG_APP,DBG_COND_1,
                ("KXMLNodeGetNodeRead(%s[%i])\n", data.nodeName, idx, i));*/
            ProcessNode(child, data.nodeName, i, xEntry, xContainer);
        }

        {
            rc_t rc2 = KXMLNodeRelease(child);
            if (rc == 0)
            {   rc = rc2; }
            child = NULL;
        }
    }

    NodeDataDestroy(&data);

    return rc;
}

rc_t XTocParseXml(struct XTocEntry* xRoot, const KFile* file)
{
    rc_t rc = 0;

    const KXMLMgr* mgr = NULL;
    const KXMLDoc* doc = NULL;
    const KXMLNodeset* root = NULL;
    const char rootName[] = "/ROOT";

/**** INIT ****/

    if (rc == 0) {
        rc = KXMLMgrMakeRead(&mgr);
        DISP_RC(rc, "while calling KXMLMgrMakeRead");
    }

    if (rc == 0) {
        KXMLMgrMakeDocRead(mgr, &doc, file);
        DISP_RC(rc, "while calling KXMLMgrMakeDocRead");
    }

    if (rc == 0) {
        rc = KXMLDocOpenNodesetRead(doc, &root, rootName);
        if (rc != 0) {
/*          PLOGERR(klogErr, (klogErr, rc,
                "while calling KXMLDocOpenNodesetRead $(name)",
                "name=%s", rootName));*/
        }
    }

    if (rc == 0) {
        uint32_t count = 0;
        rc = KXMLNodesetCount(root, &count);
        if (rc != 0) {
/*          PLOGERR(klogErr, (klogErr, rc,
                "while calling KXMLNodesetCount($(name))", "name=%s",
                rootName));*/
        }
        else if (count != 1) {
            rc = RC(rcFS, rcXmlDoc, rcReading,
                rcTag, count ? rcExcessive : rcNotFound);
/*          PLOGERR(klogErr, (klogErr, rc, "$(name)", "name=%s", rootName));*/
        }
        else if (false) {
            DBGMSG(DBG_APP,DBG_COND_1,
                ("KXMLNodesetCount(%s)=%d\n", rootName, count));
        }
    }

/**** READ AND PROCESS THE ROOT XML NODE ****/

    if (rc == 0) {
        uint32_t i = 0;
        const KXMLNode *node = NULL;

        rc = KXMLNodesetGetNodeRead(root, &node, i);
        if (rc == 0) {
            ProcessNode(node, rootName, i, xRoot, xRoot);
        }
/*      else {
*            PLOGERR(klogErr, (klogErr, rc,
                "while calling KXMLNodesetGetNodeRead($(name), $(i))",
                "name=%s,i=%d", rootName, i));
        }*/

        {
            rc_t rc2 = KXMLNodeRelease(node);
            if (rc == 0)
            {   rc = rc2; }
            node = NULL;
        }
    }

/**** RELEASE ****/

    {
        rc_t rc2 = KXMLDocRelease(doc);
        if (rc == 0)
        {   rc = rc2; }
        doc = NULL;
    }

    {
        rc_t rc2 = KXMLNodesetRelease(root);
        if (rc == 0)
        {   rc = rc2; }
        root = NULL;
    }

    {
        rc_t rc2 = KXMLMgrRelease(mgr);
        if (rc == 0)
        {   rc = rc2; }
        mgr = NULL;
    }

    return rc;
}

/************************************ EOF ****************** ******************/
