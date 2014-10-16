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
#include <kfs/file.h>

#include <stdlib.h>

#include "log.h"
#include "tar-list.h"
#include "tar-file.h"

typedef struct TarFile TarFile;
#define KFILE_IMPL TarFile
#include <kfs/impl.h>

struct TarFile {
    KFile dad;
    const TarFileList* list;
};

static
rc_t TarFile_Destroy(TarFile *self)
{
    TarFileList_Release(self->list);
    FREE(self);
    return 0;
}

static
struct KSysFile* TarFile_GetSysFile(const TarFile *self, uint64_t *offset)
{
    *offset = 0;
    return NULL;
}

static
rc_t TarFile_RandomAccess(const TarFile *self)
{
    return 0;
}

static
uint32_t TarFile_Type(const TarFile *self)
{
    return kfdFile;
}

static
rc_t TarFile_Size(const TarFile *self, uint64_t *size)
{
    return TarFileList_Size(self->list, size);
}

static
rc_t TarFile_SetSize(TarFile *self, uint64_t size)
{
    return RC(rcExe, rcFile, rcUpdating, rcInterface, rcUnsupported);
}

static
rc_t TarFile_Read(const TarFile* self, uint64_t pos, void *buffer, size_t size, size_t *num_read)
{
    return TarFileList_Read(self->list, pos, buffer, size, num_read);
}

static
rc_t TarFile_Write(TarFile *self, uint64_t pos, const void *buffer, size_t size, size_t *num_writ)
{
    return RC(rcExe, rcFile, rcWriting, rcInterface, rcUnsupported);
}

static KFile_vt_v1 TarFile_vtbl = {
    1, 1,
    TarFile_Destroy,
    TarFile_GetSysFile,
    TarFile_RandomAccess,
    TarFile_Size,
    TarFile_SetSize,
    TarFile_Read,
    TarFile_Write,
    TarFile_Type
};

rc_t TarFile_Open( const KFile** cself, const TarFileList* list )
{
    rc_t rc = 0;
    TarFile* self;

    CALLOC(self, 1, sizeof(*self));

    if ( self == NULL )
    {
        rc = RC(rcExe, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    else if ( ( rc = KFileInit( &self->dad, (const KFile_vt*)&TarFile_vtbl, "TarFile", "no-name", true, false ) ) == 0 &&
              ( rc = TarFileList_Open( list ) ) == 0 )
    {
        self->list = list;
        *cself = &self->dad;
    }
    else
    {
        KFileRelease( &self->dad );
    }
    return rc;
}
