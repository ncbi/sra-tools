/*==============================================================================
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
*/

#include "factory-file.h" /* CGFile15_Make ... */

typedef struct CGIgnored CGIgnored;
#define CGFILETYPE_IMPL CGIgnored

#include "file.h" /* CGLoaderFile */

#include <assert.h>

rc_t CGLoaderFile_CreateCGFile(CGLoaderFile* self,
    uint32_t FORMAT_VERSION, const char* TYPE)
{
    assert(self && !self->cg_file);

    switch (FORMAT_VERSION) {
        case 0x01030000:
            return CGFile13_Make(&self->cg_file, TYPE, self);
        case 0x01050000:
        case 0x01060000:
            return CGFile15_Make(&self->cg_file, TYPE, self);
        case 0x02000000:
            return CGFile20_Make(&self->cg_file, TYPE, self);
        case 0x02020000:
            return CGFile22_Make(&self->cg_file, TYPE, self);
        default: {
            const char* name = NULL;
            rc_t rc = CGLoaderFile_Filename(self, &name);
            if (rc != 0 || name == NULL)
            {   name = " | UNKNOWN | "; }
            rc = RC(rcRuntime, rcFile, rcConstructing, rcData, rcBadVersion);
            PLOGERR(klogErr, (klogErr, rc,
                "Unexpected #FORMAT_VERSION value '$(FORMAT_VERSION)' "
                "in CG file '$(name)'",
                "FORMAT_VERSION=%X,name=%s", FORMAT_VERSION, name));
            return rc;
        }
    }
}

struct CGIgnored {
    CGFileType dad;
};

static
void CC CGIgnored_Release(const CGIgnored* cself, uint64_t* records)
{
    if( cself != NULL ) {
        CGIgnored* self = (CGIgnored*)cself;
        if( records != NULL ) {
            *records = 0;
        }
        free(self);
    }
}

static
rc_t CC CGIgnored_Header(const CGIgnored* cself,
    const char* buf, const size_t len)
{
    return 0;
}

static const CGFileType_vt CGIgnored_vt =
{
    CGIgnored_Header,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL, /* tag_lfr */
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    CGIgnored_Release
};

rc_t CGLoaderFileMakeCGFileType(const CGLoaderFile* self, const char* type,
    const CGFileTypeFactory* factory, size_t factories,
    const CGFileType** ftype)
{
    rc_t rc = 0;
    CGIgnored* obj = NULL;
    uint32_t i;

    if (self == NULL || type == NULL ||
        factory == NULL || factories == 0 || ftype == NULL)
    {
        rc = RC(rcRuntime, rcFile, rcConstructing, rcParam, rcNull);
    }
    else {
        *ftype = NULL;

        for (i = 0; i < factories; ++i) {
            if (strcmp(type, factory[i].name) == 0) {
                if (factory[i].make != NULL) {
                    return factory[i].make(ftype, self);
                } else if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
                    rc = RC(rcRuntime,
                        rcFile, rcConstructing, rcMemory, rcExhausted);
                } else {
                    obj->dad.vt = &CGIgnored_vt;
                    obj->dad.type = factory[i].type;
                }
                break;
            }
        }

        if( obj == NULL ) {
            const char* name = NULL;
            rc = CGLoaderFile_Filename(self, &name);
            if (rc != 0 || name == NULL)
            {   name = " | UNKNOWN | "; }

            rc = RC(rcRuntime, rcFile, rcConstructing, rcItem, rcUnrecognized);
            PLOGERR(klogErr, (klogErr, rc,
                "Unexpected header #TYPE value '$(TYPE)' in CG file '$(name)'",
                "TYPE=%s,name=%s", type, name));
        }
    }

    if( rc == 0 ) {
        *ftype = &obj->dad;
    }
    else {
        CGIgnored_Release(obj, NULL);
    }

    return rc;
}
