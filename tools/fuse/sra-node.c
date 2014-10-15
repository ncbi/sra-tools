/*===========================================================================
 *
 *                            Public DOMAIN NOTICE
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
 */
#include <klib/rc.h>
#include <kfs/directory.h>

#include <sra/sradb-priv.h>

typedef struct SRANode SRANode;
#define FSNODE_IMPL SRANode

#include "log.h"
#include "xml.h"
#include "sra-list.h"
#include "formats.h"
#include "kfile-accessor.h"

#include <stdlib.h>
#include <string.h>

struct SRANode {
    FSNode node;
    SRAConfigFlags flags;
    const SRAListNode* sra;
    const char* prefix;
    size_t prefix_len;
};

static
rc_t SRANode_HasChild(const SRANode* cself, const char* name, size_t name_len)
{
    rc_t rc = 0;

    if( name == NULL || cself->prefix_len >= name_len || strncmp(cself->prefix, name, cself->prefix_len) != 0 ) {
        rc = RC(rcExe, rcSRA, rcSearching, rcDirEntry, rcNotFound);
    } else {
        const FileOptions* opt;
        rc = SRAListNode_GetType(cself->sra, cself->flags, &name[cself->prefix_len], &opt);
    }
    return rc;
}

static
rc_t SRANode_Attr(const SRANode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    if( subpath == NULL || strncmp(cself->prefix, subpath, cself->prefix_len) != 0 ) {
        rc = RC(rcExe, rcSRA, rcEvaluating, rcDirEntry, rcInvalid);
    } else {
        const FileOptions* opt;
        if( (rc = SRAListNode_GetType(cself->sra, cself->flags, &subpath[cself->prefix_len], &opt)) == 0 &&
            (rc = SRAListNode_TableMTime(cself->sra, ts)) == 0 ) {
            *type = kptFile;
            *file_sz = opt->file_sz;
        }
    }
    return rc;
}

static
rc_t SRANode_Dir(const SRANode* cself, const char* subpath, FSNode_Dir_Visit func, void* data)
{
    if( subpath != NULL ) {
        return RC(rcExe, rcDirectory, rcListing, rcDirEntry, rcInvalid);
    }
    return SRAListNode_ListFiles(cself->sra, cself->prefix, cself->flags, func, data);
}

static
rc_t SRANode_Open(const SRANode* cself, const char* subpath, const SAccessor** accessor)
{
    rc_t rc = 0;

    if( subpath == NULL || strncmp(cself->prefix, subpath, cself->prefix_len) != 0 ) {
        rc = RC(rcExe, rcSRA, rcEvaluating, rcDirEntry, rcInvalid);
    } else {
        const FileOptions* opt;
        if( (rc = SRAListNode_GetType(cself->sra, cself->flags, &subpath[cself->prefix_len], &opt)) == 0 ) {
            const KFile* kf = NULL;
            if( (rc = FileOptions_OpenFile(opt, cself->sra, &kf)) == 0 &&
                (rc = KFileAccessor_Make(accessor, cself->prefix, kf)) != 0 ) {
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
rc_t SRANode_Release(SRANode* self)
{
    if( self != NULL ) {
        SRAListNode_Release(self->sra);
    }
    return 0;
}

static FSNode_vtbl SRANode_vtbl = {
    sizeof(SRANode),
    SRANode_HasChild,
    NULL,
    SRANode_Attr,
    SRANode_Dir,
    NULL,
    SRANode_Open,
    SRANode_Release
};

rc_t SRANode_Make(FSNode* parent, const char* prefix, const SRAListNode* sra, SRAConfigFlags flags)
{
    rc_t rc = 0;

    if( parent == NULL || prefix == NULL || sra == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcParam, rcNull);
    } else {
        SRANode* ff = NULL;
        char buf[128];

        strcpy(buf, "SRA-");
        strcat(buf, prefix);
        DEBUG_MSG(8, ("Adding %s\n", buf));
        if( (rc = FSNode_Make((FSNode**)&ff, buf, &SRANode_vtbl)) == 0 ) {
            ff->flags |= (flags & eSRAFuseFileArc) ? (eSRAFuseFmtArc | eSRAFuseFmtArcMD5) : 0;
            ff->flags |= (flags & eSRAFuseFileArcLite) ? (eSRAFuseFmtArcLite | eSRAFuseFmtArcLiteMD5) : 0;
            ff->flags |= (flags & eSRAFuseFileFastq) ? (eSRAFuseFmtFastq | eSRAFuseFmtFastqGz | eSRAFuseFmtFastqMD5) : 0;
            ff->flags |= (flags & eSRAFuseFileSFF) ? (eSRAFuseFmtSFF | eSRAFuseFmtSFFGz | eSRAFuseFmtSFFMD5) : 0;
            ff->prefix = prefix;
            ff->prefix_len = strlen(prefix);
            if( (rc = SRAListNode_AddRef(sra)) == 0 ) {
                ff->sra = sra;
                rc = FSNode_AddChild(parent, &ff->node);
            }
            if( rc != 0 ) {
                FSNode_Release(&ff->node);
            }
        }
    }
    return rc;
}
