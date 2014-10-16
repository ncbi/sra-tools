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
#include <kxml/xml.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <sra/sradb-priv.h>

typedef struct SRADirectoryNode SRADirectoryNode;
#define FSNODE_IMPL SRADirectoryNode

#include "log.h"
#include "xml.h"
#include "sra-list.h"
#include "sra-directory.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

struct SRADirectoryNode {
    FSNode node;
    const SRAListNode* sra;
};

static
rc_t SRADirectoryNode_Attr(const SRADirectoryNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    if( subpath != NULL ) {
        rc = RC(rcExe, rcFile, rcOpening, rcDirEntry, rcNotFound);
    } else {
        *type = kptDir;
        rc = SRAListNode_TableMTime(cself->sra, ts);
    }
    return rc;
}

static
rc_t SRADirectoryNode_Dir(const SRADirectoryNode* cself, const char* subpath, FSNode_Dir_Visit func, void* data)
{
    if( subpath != NULL ) {
        return RC(rcExe, rcSRA, rcEvaluating, rcDirEntry, rcInvalid);
    }
    return FSNode_ListChildren(&cself->node, func, data);
}

static
rc_t SRADirectoryNode_Release(SRADirectoryNode* self)
{
    if( self != NULL ) {
        SRAListNode_Release(self->sra);
    }
    return 0;
}

static FSNode_vtbl SRADirectoryNode_vtbl = {
    sizeof(SRADirectoryNode),
    NULL,
    NULL,
    SRADirectoryNode_Attr,
    SRADirectoryNode_Dir,
    NULL,
    NULL,
    SRADirectoryNode_Release
};

rc_t SRADirectoryNode_Make(FSNode** self, const char* name, const SRAListNode* sra)
{
    rc_t rc = 0;

    if( self == NULL || sra == NULL ) {
        rc = RC(rcExe, rcDirectory, rcConstructing, rcParam, rcNull);
    } else {
        if( (rc = FSNode_Make(self, name, &SRADirectoryNode_vtbl)) == 0 ) {
            SRAListNode_AddRef(sra);
            ((SRADirectoryNode*)*self)->sra = sra;
        }
    }
    return rc;
}
