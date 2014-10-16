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
#include <kfs/file.h>

#include "log.h"
#include "kfile-accessor.h"

typedef struct KFileAccessor_struct {
    const KFile* file;
} KFileAccessor;

static
rc_t KFileAccessor_Read(const SAccessor* cself, char* buf, size_t size, off_t offset, size_t* num_read)
{
    rc_t rc = 0;
    KFileAccessor* self = (KFileAccessor*)cself;
    size_t actual = 0;

    do {
        rc = KFileRead(self->file, offset + *num_read, &buf[*num_read], size - *num_read, &actual);
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
rc_t KFileAccessor_Release(const SAccessor* cself)
{
    rc_t rc = 0;
    if( cself != NULL ) {
        KFileAccessor* self = (KFileAccessor*)cself;
        rc = KFileRelease(self->file);
    }
    return rc;
}

rc_t KFileAccessor_Make(const SAccessor** accessor, const char* name, const KFile* kfile)
{
    rc_t rc = 0;

    if( (rc = SAccessor_Make(accessor, sizeof(KFileAccessor), name, KFileAccessor_Read, KFileAccessor_Release)) == 0 ) {
        ((KFileAccessor*)(*accessor))->file = kfile;
    }
    return rc;
}
