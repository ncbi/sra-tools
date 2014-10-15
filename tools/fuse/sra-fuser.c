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
#include <kapp/args.h>
#include <klib/out.h>

#include <stdlib.h>
#include <string.h>

#include "sra-fuser.vers.h"
#include "xml.h"
#include "sra-fuser.h"
#include "log.h"
#include "node.h"
#include "accessor.h"
#include "sra-list.h"

typedef struct SRequest_struct {
    const FSNode* node;
    const char* subpath;
} SRequest;

static char* g_work_dir = NULL;

static
rc_t SRequestMake(SRequest* request, const char* path, bool recur)
{
    rc_t rc = 0;

    if( request == NULL ) {
        rc = RC(rcExe, rcFileDesc, rcConstructing, rcParam, rcNull);
    } else {
        rc = XML_FindLock(path, recur, &request->node, &request->subpath);
    }
    return rc;
}

static
void SRequestRelease(SRequest* request)
{
    if( request != NULL ) {
        XML_FindRelease();
    }
}

rc_t Initialize(unsigned int sra_sync, const char* xml_path, unsigned int xml_sync,
                const char* SRA_cache_path, const char* xml_root, EXMLValidate xml_validate)
{
    rc_t rc = 0;
    KDirectory* dir = NULL;

    if( (rc = KDirectoryNativeDir(&dir)) == 0 ) {
        char buf[4096];
        if( (rc = KDirectoryResolvePath(dir, true, buf, 4096, xml_root)) == 0 ) {
            /* replace /. at the end to  just / */
            if( strcmp(&buf[strlen(buf) - 2], "/.") == 0 ) {
                buf[strlen(buf) - 1] = '\0';
            }
            /* add / to the end if missing */
            if( buf[strlen(buf) - 1] != '/' ) {
                buf[strlen(buf) + 1] = '\0';
                buf[strlen(buf)] = '/';
            }
            if( (rc = StrDup(buf, &g_work_dir)) == 0 ) {
                DEBUG_MSG(8, ("Current directory set to '%s'\n", g_work_dir));
            }
        }
        if( rc == 0 && (rc = SRAList_Make(dir, sra_sync, SRA_cache_path)) != 0 ) {
            LOGERR(klogErr, rc, "SRA");
        } else {
            rc = XML_Make(dir, g_work_dir, xml_path, xml_sync, xml_validate);
        }
        ReleaseComplain(KDirectoryRelease, dir);
    }
    return rc;
}

/* =============================================================================== */
/* system call handlers */
/* =============================================================================== */

const char UsageDefaultName[] = "sra-fuser";

rc_t CC UsageSummary (const char* progname)
{
    return KOutMsg("Usage:\n"
        "\t%s [options] -o [FUSE options] -x file -m path\n"
        "\t%s [options] -u -m path\n\n", progname, progname);
}

rc_t CC Usage ( const Args * args )
{
    /* dummy for newer args system will be filled when system is complete!!! */
    return 0;
}

uint32_t KAppVersion(void)
{
    return SRA_FUSER_VERS;
}

void SRA_FUSER_Init(void)
{
    rc_t rc = 0;
    /* reopen log file and start watch thread(s) */
    if( (rc = LogFile_Init(NULL, 0, true, NULL)) != 0 ) {
        LOGERR(klogErr, rc, "log file");
    }
    SRAList_Init(); /* this preceeeds XML_Init */
    XML_Init();     /* or SRAList may become corrupt */
    LOGMSG(klogInfo, "Started");
}

void SRA_FUSER_Fini(void)
{
    SRAList_Fini();
    XML_Fini();
    LOGMSG(klogInfo, "Stopped");
    LogFile_Fini();
    FREE(g_work_dir);
}

rc_t SRA_FUSER_GetDir(const char* path, FSNode_Dir_Visit func, void* data)
{
    rc_t rc = 0;
    SRequest request;

    if( (rc = SRequestMake(&request, path, true)) == 0 ) {
        rc = FSNode_Dir(request.node, request.subpath, func, data);
        SRequestRelease(&request);
    }
    return rc;
}

rc_t SRA_FUSER_GetAttr(const char* path, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;
    SRequest request;

    if( (rc = SRequestMake(&request, path, false)) == 0 ) {
        rc = FSNode_Attr(request.node, request.subpath, type, ts, file_sz, access, block_sz);
        SRequestRelease(&request);
    }
    return rc;
}

rc_t SRA_FUSER_ResolveLink(const char* path, char* buf, size_t buf_sz)
{
    rc_t rc = 0;
    SRequest request;

    if( (rc = SRequestMake(&request, path, false)) == 0 ) {
        rc = FSNode_Link(request.node, request.subpath, buf, buf_sz);
        SRequestRelease(&request);
    }
    return rc;
}

rc_t SRA_FUSER_OpenNode(const char* path, const void** data)
{
    rc_t rc = 0;
    SRequest request;

    if( (rc = SRequestMake(&request, path, false)) == 0 ) {
        rc = FSNode_Open(request.node, request.subpath, (const SAccessor**)data);
        SRequestRelease(&request);
    }
    return rc;
}

rc_t SRA_FUSER_ReadNode(const char* path, const void* data, char *buf, size_t size, off_t offset, size_t* num_read)
{
    return SAccessor_Read(data, buf, size, offset, num_read);
}

rc_t SRA_FUSER_CloseNode(const char* path, const void* data)
{
    return SAccessor_Release(data);
}
