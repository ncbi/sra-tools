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
#ifndef _h_sra_fuse_node_
#define _h_sra_fuse_node_

#include "accessor.h"

struct KDirectory;

typedef struct FSNode_vtbl FSNode_vtbl;
typedef struct FSNode FSNode;

/* this struct must be 1st element in child objects structs! */
struct FSNode {
    const FSNode_vtbl* vtbl;
    const char* name;
    const FSNode* children;
    const FSNode* sibling;
};

typedef rc_t ( CC * FSNode_Dir_Visit ) ( const char *name, void *data );

#ifndef FSNODE_IMPL
#define FSNODE_IMPL FSNode
#endif

struct FSNode_vtbl {

    /* object size */
    const size_t type_size;

    /* for hidden node asks if a name is within, it doesn't mean that file is really present */
    rc_t (*HasChild)(const FSNODE_IMPL* cself, const char* name, size_t name_len);

    /* called to update node internal state if found by path search */
    rc_t (*Touch)(const FSNODE_IMPL* cself);

    /* the only mandatory method for the node */
    rc_t (*Attr)(const FSNODE_IMPL* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz);

    /* fill lst with subnodes names, terminate with NULL if not filled the list up to lst_sz */
    rc_t (*Dir)(const FSNODE_IMPL* cself, const char* subpath, FSNode_Dir_Visit func, void* data);

    /* resolve path to real name for a symlink, fill in buf with name */
    rc_t (*Link)(const FSNODE_IMPL* cself, const char* subpath, char* buf, size_t buf_sz);

    /* open a file for reading */
    rc_t (*Open)(const FSNODE_IMPL* cself, const char* subpath, const SAccessor** accessor);

    /* releases the object */
    rc_t (*Release)(FSNODE_IMPL* cself);
};

/* constructor */

rc_t FSNode_Make(FSNode** self, const char* name, const FSNode_vtbl* vtbl);

/* static methods */

rc_t FSNode_AddChild(FSNode* self, const FSNode* child);

rc_t FSNode_GetName(const FSNode* cself, const char** name);

rc_t FSNode_HasChildren(const FSNode* cself, bool* test);

rc_t FSNode_ListChildren(const FSNode* cself, FSNode_Dir_Visit func, void* data);

rc_t FSNode_FindChild(const FSNode* cself, const char* name, size_t name_len, const FSNode** child, bool* hidden);

/* virtual methods */

rc_t FSNode_Touch(const FSNode* cself);

rc_t FSNode_Attr(const FSNode* cself, const char* subpath, uint32_t* type, KTime_t* ts,
                 uint64_t* file_sz, uint32_t* access, uint64_t* block_sz);

rc_t FSNode_Dir(const FSNode* cself, const char* subpath, FSNode_Dir_Visit func, void* data);

rc_t FSNode_Link(const FSNode* cself, const char* subpath, char* buf, size_t buf_sz);

rc_t FSNode_Open(const FSNode* cself, const char* subpath, const SAccessor** accessor);

void FSNode_Release(const FSNode* self);

#endif /* _h_sra_fuse_node_ */
