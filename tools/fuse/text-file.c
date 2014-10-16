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

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "formats.h"
#include "text-file.h"

typedef struct TextFile TextFile;
#define KFILE_IMPL TextFile
#include <kfs/impl.h>

struct TextFile {
    KFile dad;
    char* data;
};

static
rc_t TextFile_Destroy(TextFile *self)
{
    FREE(self->data);
    FREE(self);
    return 0;
}

static
struct KSysFile* TextFile_GetSysFile(const TextFile *self, uint64_t *offset)
{
    *offset = 0;
    return NULL;
}

static
rc_t TextFile_RandomAccess(const TextFile *self)
{
    return 0;
}

static
uint32_t TextFile_Type(const TextFile *self)
{
    return kfdFile;
}

static
rc_t TextFile_Size(const TextFile *self, uint64_t *size)
{
    *size = strlen(self->data);
    return 0;
}

static
rc_t TextFile_SetSize(TextFile *self, uint64_t size)
{
    return RC(rcExe, rcFile, rcUpdating, rcInterface, rcUnsupported);
}

static
rc_t TextFile_Read(const TextFile *self, uint64_t pos, void *buffer, size_t bsize, size_t *num_read)
{
    *num_read = strlen(self->data);
    if( pos < *num_read ) {
        *num_read = (*num_read < bsize) ? *num_read : bsize;
        memcpy(buffer, &self->data[pos], *num_read);
    } else {
        *num_read = 0;
    }
    return 0;
}

static
rc_t TextFile_Write(TextFile *self, uint64_t pos, const void *buffer, size_t size, size_t *num_writ)
{
    return RC(rcExe, rcFile, rcWriting, rcInterface, rcUnsupported);
}

static KFile_vt_v1 TextFile_vtbl = {
    1, 1,
    TextFile_Destroy,
    TextFile_GetSysFile,
    TextFile_RandomAccess,
    TextFile_Size,
    TextFile_SetSize,
    TextFile_Read,
    TextFile_Write,
    TextFile_Type
};


rc_t TextFile_Open( const KFile** cself, const FileOptions* opt )
{
    rc_t rc = 0;
    TextFile* self;
    CALLOC(self, 1, sizeof(*self));
    if( self == NULL )
    {
        rc = RC(rcExe, rcFile, rcConstructing, rcMemory, rcExhausted );
    }
    else if ( ( rc = KFileInit( &self->dad, (const KFile_vt*)&TextFile_vtbl, "TextFile", "no-name", true, false ) ) != 0 ||
              ( rc = StrDup( opt->f.txt64b, &self->data ) ) != 0 )
    {
        KFileRelease( &self->dad );
    }
    else
    {
        *cself = &self->dad;
    }
    return rc;
}
