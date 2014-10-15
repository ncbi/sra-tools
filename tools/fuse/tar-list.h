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
#ifndef _h_sra_fuse_tar_list_
#define _h_sra_fuse_tar_list_

#include <klib/rc.h>

#define TAR_BLOCK_SIZE (512)

typedef union TarBlock TarBlock;

typedef struct TarFileList TarFileList;

rc_t TarFileList_Make(const TarFileList** cself, uint32_t max_file_count, const char* name);

void TarFileList_Release(const TarFileList* cself);

rc_t TarFileList_Add(const TarFileList* cself, const char* path, const char* name, uint64_t size, KTime_t mtime, bool executable);

rc_t TarFileList_Size(const TarFileList* cself, uint64_t* file_sz);

rc_t TarFileList_Open(const TarFileList* cself);

rc_t TarFileList_Read(const TarFileList* cself, uint64_t pos, uint8_t* buffer, size_t size, size_t* num_read);

#endif /* _h_sra_fuse_tar_list_ */
