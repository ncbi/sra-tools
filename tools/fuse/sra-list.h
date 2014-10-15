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
#ifndef _h_sra_fuse_sra_list_
#define _h_sra_fuse_sra_list_

#include <sra/sradb.h>

#include "xml.h"
#include "formats.h"

typedef enum {
    eSRAFuseInitial     = 0x1000,
    eSRAFuseRunDir      = 0x0001,
    eSRAFuseFileArc     = 0x0002,
    eSRAFuseFileArcLite = 0x0004,
    eSRAFuseFileFastq   = 0x0008,
    eSRAFuseFileSFF     = 0x0010
} SRAConfigFlags;

typedef struct SRAListNode SRAListNode;

/*
 * Initialize run list
 * if seconds > 0 than a thread is launched to updated run info
 */
rc_t SRAList_Make(KDirectory* dir, unsigned int seconds, const char* cache_path);

void SRAList_Init(void);

void SRAList_Fini(void);

/* after processing updated XML set next version so 
 * that runs dropped from XML could be dropped from the list
 */
rc_t SRAList_NextVersion(void);

/*
 * Attaches to parent file nodes based on flags
 * xml_node is of type SRA with accession and optional path
 * maintains internal list of SRA objects keyed on acession-path pairs
 */
rc_t SRAListNode_Make(const KXMLNode* xml_node, FSNode* parent, SRAConfigFlags flags, char* errmsg,
                      const char* rel_path, EXMLValidate validate);

rc_t SRAListNode_TableMTime(const SRAListNode* cself, KTime_t* mtime);

rc_t SRAListNode_TableOpen(const SRAListNode* cself, const SRATable** tbl);

/*
 * Get type info based on file extension
 */
rc_t SRAListNode_GetType(const SRAListNode* cself, SRAConfigFlags flags, const char* suffix, struct FileOptions const** options);

/*
 * Get list of files using prefix + suffixes filtering by types
 */
rc_t SRAListNode_ListFiles(const SRAListNode* cself, const char* prefix, int types, FSNode_Dir_Visit func, void* data);

rc_t SRAListNode_AddRef(const SRAListNode* cself);
void SRAListNode_Release(const SRAListNode* cself);
void SRAList_PostRefresh();

#endif /* _h_sra_fuse_sra_list_ */
