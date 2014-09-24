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
#include <klib/rc.h>

#include "loader-file.h"
#include "debug.h"

#include <stdlib.h>
#include <string.h>

struct SRALoaderFile
{
    const DataBlock* data_block;
    const DataBlockFileAttr* file_attr;
    const KLoaderFile *lfile;
};

rc_t SRALoaderFile_IsEof(const SRALoaderFile* cself, bool* eof)
{
    return KLoaderFile_IsEof(cself ? cself->lfile : NULL, eof);
}

rc_t SRALoaderFile_LOG(const SRALoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    rc = KLoaderFile_VLOG(cself ? cself->lfile : NULL, lvl, rc, msg, fmt, args);
    va_end(args);
    return rc;
}

rc_t SRALoaderFile_Offset(const SRALoaderFile* cself, uint64_t* offset)
{
    return KLoaderFile_Offset(cself ? cself->lfile : NULL, offset);
}

rc_t SRALoaderFileReadline(const SRALoaderFile* cself, const void** buffer, size_t* length)
{
    return KLoaderFile_Readline(cself ? cself->lfile : NULL, buffer, length);
}

rc_t SRALoaderFileRead(const SRALoaderFile* cself, size_t advance, size_t size, const void** buffer, size_t* length)
{
    return KLoaderFile_Read(cself ? cself->lfile : NULL, advance, size, buffer, length);
}

rc_t SRALoaderFileName(const SRALoaderFile *cself, const char **name)
{
    return KLoaderFile_Name(cself ? cself->lfile : NULL, name);
}

rc_t SRALoaderFileFullName(const SRALoaderFile *cself, const char **name)
{
    return KLoaderFile_FullName(cself ? cself->lfile : NULL, name);
}

rc_t SRALoaderFileResolveName(const SRALoaderFile *self, char *resolved, size_t rsize)
{
    return KLoaderFile_ResolveName(self ? self->lfile : NULL, resolved, rsize);
}

rc_t SRALoaderFileBlockName ( const SRALoaderFile *cself, const char **block_name )
{
    if( cself == NULL || block_name == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *block_name = cself->data_block->name;
    return 0;
}

rc_t SRALoaderFileMemberName(const SRALoaderFile *cself, const char **member_name)
{
    if( cself == NULL || member_name == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *member_name = cself->data_block->member_name;
    return 0;
}

rc_t SRALoaderFileSector( const SRALoaderFile *cself, int64_t* sector)
{
    if( cself == NULL || sector == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *sector = cself->data_block->sector;
    return 0;
}

rc_t SRALoaderFileRegion( const SRALoaderFile *cself, int64_t* region)
{
    if( cself == NULL || region == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *region = cself->data_block->region;
    return 0;
}

rc_t SRALoaderFile_QualityScoringSystem( const SRALoaderFile *cself, ExperimentQualityType* quality_scoring_system )
{
    if( cself == NULL || quality_scoring_system == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *quality_scoring_system = cself->file_attr->quality_scoring_system;
    return 0;
}

rc_t SRALoaderFile_QualityEncoding( const SRALoaderFile *cself, ExperimentQualityEncoding* quality_encoding )
{
    if( cself == NULL || quality_encoding == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *quality_encoding = cself->file_attr->quality_encoding;
    return 0;
}

rc_t SRALoaderFile_AsciiOffset( const SRALoaderFile *cself, uint8_t* ascii_offset )
{
    if( cself == NULL || ascii_offset == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *ascii_offset = cself->file_attr->ascii_offset;
    return 0;
}

rc_t SRALoaderFile_FileType( const SRALoaderFile *cself, ERunFileType* filetype )
{
    if( cself == NULL || filetype == NULL ) {
        return RC(rcSRA, rcFile, rcAccessing, rcParam, rcNull);
    }
    *filetype = cself->file_attr->filetype;
    return 0;
}

rc_t SRALoaderFile_Release(const SRALoaderFile* cself)
{
    rc_t rc = 0;

    if( cself ) {
        SRALoaderFile* self = (SRALoaderFile*)cself;
        /* may return md5 check error here */
        rc = KLoaderFile_Release(self->lfile, false);
        free(self);
    }
    return rc;
}

rc_t SRALoaderFile_Make(const SRALoaderFile **cself, const KDirectory* dir, const char* filename,
                        const DataBlock* block, const DataBlockFileAttr* fileattr, const uint8_t* md5_digest, bool read_ahead)
{
    rc_t rc = 0;
    SRALoaderFile* obj;

    if( cself == NULL || block == NULL || fileattr == NULL ) {
        rc = RC(rcSRA, rcFile, rcConstructing, rcParam, rcNull);
    } else {
        if( (obj = calloc(1, sizeof(*obj))) == NULL ) {
            rc = RC(rcSRA, rcFile, rcConstructing, rcMemory, rcExhausted);
        } else if( (rc = KLoaderFile_Make(&obj->lfile, dir, filename, md5_digest, read_ahead)) == 0 ) {
            obj->data_block = block;
            obj->file_attr = fileattr;
            *cself = obj;
        }
        if( rc != 0 ) {
            *cself = NULL;
            SRALoaderFile_Release(obj);
        }
    }
    return rc;
}
