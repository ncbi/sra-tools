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
#ifndef _h_sra_fuse_fuser_
#define _h_sra_fuse_fuser_

#include "node.h"

uint32_t KAppVersion(void);

rc_t Initialize(unsigned int sra_sync, const char* xml_path, unsigned int xml_sync,
                const char* SRA_cache_path, const char* xml_root, EXMLValidate xml_validate);

/* FUSE call backs */
void SRA_FUSER_Init(void);
void SRA_FUSER_Fini(void);

rc_t SRA_FUSER_GetAttr(const char* path, uint32_t* type, KTime_t* date, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz);
rc_t SRA_FUSER_GetDir(const char* path, FSNode_Dir_Visit func, void* data);
rc_t SRA_FUSER_ResolveLink(const char* path, char* buf, size_t buf_sz);

rc_t SRA_FUSER_OpenNode(const char* path, const void** data);
rc_t SRA_FUSER_ReadNode(const char* path, const void* data, char *buf, size_t size, off_t offset, size_t* num_read);
rc_t SRA_FUSER_CloseNode(const char* path, const void* data);

#endif /* _h_sra_fuse_fuser_ */
