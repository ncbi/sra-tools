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
#include <stdlib.h>
#include <string.h>

#include "accessor.h"
#include "log.h"

/* used to detect correct object pointers */
const uint32_t SAccessor_MAGIC = 0xFACE5550;

struct SAccessor {
    uint32_t magic;
    char* name;
    AccessorRead* Read;
    AccessorRelease* Release;
};

rc_t SAccessor_Make(const SAccessor** cself, size_t size, const char* name, AccessorRead* read, AccessorRelease* release)
{
    rc_t rc = 0;
    SAccessor* self = NULL;

    if( cself == NULL ) {
        rc = RC(rcExe, rcFile, rcConstructing, rcSelf, rcNull);
    } else {
        CALLOC(self, 1, sizeof(*self) + size);
        if( self == NULL ) {
            rc = RC(rcExe, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else {
            if( (rc = StrDup(name, &self->name)) == 0 ) {
                self->magic = SAccessor_MAGIC;
                self->Read = read;
                self->Release = release;
                /* shift pointer to after hidden structure */
                DEBUG_MSG(8, ("%s: %s\n", __func__, self->name));
                self++;
            } else {
                FREE(self);
            }
        }
        *cself = rc ? NULL : self;
    }
    return rc;
}

static
rc_t SAccessor_ResolveSelf(const SAccessor* self, enum RCContext ctx, SAccessor** resolved)
{
    if( self == NULL || resolved == NULL ) {
        return RC(rcSRA, rcFile, ctx, rcSelf, rcNull);
    }
    *resolved = (SAccessor*)--self;
    /* just to validate that it is full instance */
    if( (*resolved)->magic != SAccessor_MAGIC ) {
        *resolved = NULL;
        return RC(rcSRA, rcFile, ctx, rcSelf, rcCorrupt);
    }
    return 0;
}


rc_t SAccessor_Read(const SAccessor* cself, char* buf, size_t size, off_t offset, size_t* num_read)
{
    rc_t rc = 0;
    SAccessor* self = NULL;

    if( buf == NULL || num_read == NULL ) {
        return RC(rcExe, rcFile, rcReading, rcParam, rcNull);
    } else if( (rc = SAccessor_ResolveSelf(cself, rcEvaluating, &self)) == 0 ) {
        DEBUG_MSG(8, ("%s: %s\n", __func__, self->name));
        if( self->Read ) {
            *num_read = 0;
            rc = self->Read(cself, buf, size, offset, num_read);
        } else {
            rc = RC(rcExe, rcFile, rcReading, rcInterface, rcUnsupported);
        }
    }
    return rc;
}

rc_t SAccessor_GetName(const SAccessor* cself, const char** name)
{
    rc_t rc = 0;
    SAccessor* self = NULL;

    if( name == NULL ) {
        rc = RC(rcExe, rcFile, rcEvaluating, rcParam, rcInvalid);
    } else if( (rc = SAccessor_ResolveSelf(cself, rcEvaluating, &self)) == 0 ) {
        *name = self->name;
    }
    return rc;
}

rc_t SAccessor_Release(const SAccessor* cself)
{
    rc_t rc = 0;
    if( cself != NULL ) {
        SAccessor* self = NULL;

        if( (rc = SAccessor_ResolveSelf(cself, rcReleasing, &self)) == 0 ) {
            if( self->Release ) {
                rc = self->Release(cself);
            }
            DEBUG_MSG(8, ("%s: %s\n", __func__, self->name));
            FREE(self->name);
            FREE(self);
        }
    }
    return rc;
}
