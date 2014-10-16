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
#include <ctype.h>

#include "log.h"
#include "node.h"

rc_t FSNode_Make(FSNode** self, const char* name, const FSNode_vtbl* vtbl)
{
    rc_t rc = 0;

    if( self == NULL || vtbl == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcParam, rcNull);
    } else if( vtbl->Attr == NULL ) {
        rc = RC(rcExe, rcNode, rcConstructing, rcInterface, rcInsufficient);
    } else {
        const char* x = name;
        while( rc == 0 && *x != '\0' ) {
            if( *x < 32 || !isascii(*x) || strchr("\"*/:<>?\\|", *x) != NULL ) {
                rc = RC(rcExe, rcDoc, rcValidating, rcDirEntry, rcInvalid);
            }
            x++;
        }
        if( rc == 0 ) {
            CALLOC(*self, 1, vtbl->type_size + (x - name + 1));
            if( *self == NULL ) {
                rc = RC(rcExe, rcNode, rcConstructing, rcMemory, rcExhausted);
            } else {
                char* x = (char*)(*self);
                (*self)->vtbl = vtbl;
                (*self)->name = &x[vtbl->type_size];
                strcpy(&x[vtbl->type_size], name);
            }
        }
    }
    return rc;
}

static
void FSNode_DropChildren(FSNode* self)
{
    if( self != NULL ) {
        const FSNode* c = self->children;
        while( c != NULL ) {
            const FSNode* sib = c->sibling;
            FSNode_Release(c);
            c = sib;
        }
        self->children = NULL;
    }
}

rc_t FSNode_AddChild(FSNode* self, const FSNode* child)
{
    rc_t rc = 0;
    if( self == NULL || child == NULL ) {
        rc = RC(rcExe, rcNode, rcAttaching, rcParam, rcNull);
    } else {
        DEBUG_MSG(8, ("Adding to %s child %s\n", self->name, child->name));
        if( child->sibling != NULL ) {
            rc = RC(rcExe, rcDoc, rcAttaching, rcDirEntry, rcAmbiguous);
        } else if( self->children == NULL ) {
            self->children = child;
        } else {
            const FSNode* ch = self->children;
            while( rc == 0 && ch != NULL ) {
                if( strcmp(child->name, ch->name) == 0 ) {
                    rc = RC(rcExe, rcDoc, rcAttaching, rcDirEntry, rcDuplicate);
                } else if( ch->sibling == NULL ) {
                    ((FSNode*)ch)->sibling = child;
                    break;
                }
                ch = ch->sibling;
            }
        }
    }
    return rc;
}

rc_t FSNode_GetName(const FSNode* cself, const char** name)
{
    rc_t rc = 0;

    if( cself == NULL || name == NULL ) {
        rc = RC(rcExe, rcNode, rcEvaluating, rcParam, rcNull);
    } else {
        *name = cself->name;
    }
    return rc;
}

rc_t FSNode_HasChildren(const FSNode* cself, bool* test)
{
    rc_t rc = 0;

    if( cself == NULL || test == NULL ) {
        rc = RC(rcExe, rcDirectory, rcListing, rcParam, rcNull);
    } else {
        *test = cself->children != NULL;
    }
    return rc;
}

rc_t FSNode_ListChildren(const FSNode* cself, FSNode_Dir_Visit func, void* data)
{
    rc_t rc = 0;

    if( cself == NULL || func == NULL ) {
        rc = RC(rcExe, rcDirectory, rcListing, rcParam, rcNull);
    } else {
        const FSNode* ch = cself->children;
        DEBUG_MSG(10, ("Children of %s\n", cself->name));
        while( rc == 0 && ch != NULL ) {
            if( ch->vtbl->HasChild ) {
                rc = FSNode_Dir(ch, NULL, func, data);
            } else if( (rc = func(ch->name, data)) == 0 ) {
                DEBUG_MSG(10, ("child '%s'\n", ch->name));
            }
            ch = ch->sibling;
        }
    }
    return rc;
}

rc_t FSNode_FindChild(const FSNode* cself, const char* name, size_t name_len, const FSNode** child, bool* hidden)
{
    rc_t rc = 0;

    if( cself == NULL || name == NULL || name_len == 0 || child == NULL || hidden == NULL ) {
        rc = RC(rcExe, rcDirectory, rcSearching, rcParam, rcInvalid);
    } else {
        const FSNode* ch = cself->children;
        *child = NULL;
        *hidden = false;
        while( rc == 0 && ch != NULL && *child == NULL ) {
            if( ch->vtbl->HasChild ) {
                if( (rc = ch->vtbl->HasChild(ch, name, name_len)) == 0 ) {
                    *hidden = true;
                    *child = ch;
                } else if( GetRCState(rc) == rcNotFound ) {
                    rc = 0;
                }
            } else if( strlen(ch->name) == name_len && strncmp(ch->name, name, name_len) == 0 ) {
                *child = ch;
            }
            ch = ch->sibling;
        }
    }
    if( rc == 0 && *child == NULL ) {
        rc = RC(rcExe, rcDirectory, rcEvaluating, rcName, rcNotFound);
    }
    return rc;
}

rc_t FSNode_Touch(const FSNode* cself)
{
    rc_t rc = 0;

    if( cself == NULL ) {
        rc = RC(rcExe, rcNode, rcUpdating, rcParam, rcNull);
    } else if( cself->vtbl->Touch ) {
        DEBUG_MSG(10, ("%s: %s\n", __func__, cself->name));
        rc = cself->vtbl->Touch(cself);
    }
    return rc;
}

rc_t FSNode_Attr(const FSNode* cself, const char* subpath, uint32_t* type, KTime_t* ts, uint64_t* file_sz, uint32_t* access, uint64_t* block_sz)
{
    rc_t rc = 0;

    if( cself == NULL || type == NULL || ts == NULL ) {
        rc = RC(rcExe, rcNode, rcClassifying, rcParam, rcNull);
    } else {
        DEBUG_MSG(10, ("%s: %s/%s\n", __func__, cself->name, subpath));
        if( cself->vtbl->Attr ) {
            rc = cself->vtbl->Attr(cself, subpath, type, ts, file_sz, access, block_sz);
        } else {
            rc = RC(rcExe, rcNode, rcClassifying, rcInterface, rcUnsupported);
        }
    }
    return rc;
}

rc_t FSNode_Dir(const FSNode* cself, const char* subpath, FSNode_Dir_Visit func, void* data)
{
    rc_t rc = 0;

    if( cself == NULL || func == NULL ) {
        rc = RC(rcExe, rcDirectory, rcListing, rcParam, rcNull);
    } else {
        DEBUG_MSG(10, ("%s: %s/%s\n", __func__, cself->name, subpath));
        if( cself->vtbl->Dir ) {
            rc = cself->vtbl->Dir(cself, subpath, func, data);
        } else {
            rc = RC(rcExe, rcDirectory, rcListing, rcInterface, rcUnsupported);
        }
    }
    return rc;
}

rc_t FSNode_Link(const FSNode* cself, const char* subpath, char* buf, size_t buf_sz)
{
    rc_t rc = 0;

    if( cself == NULL || buf == NULL || buf_sz < 1 ) {
        rc = RC(rcExe, rcPath, rcAliasing, rcParam, rcInvalid);
    } else {
        DEBUG_MSG(10, ("%s: %s/%s\n", __func__, cself->name, subpath));
        if( cself->vtbl->Link ) {
            rc = cself->vtbl->Link(cself, subpath, buf, buf_sz);
            DEBUG_MSG(10, ("%s: %s\n", __func__, buf));
        } else {
            rc = RC(rcExe, rcNode, rcAliasing, rcInterface, rcUnsupported);
        }
    }
    return rc;
}

rc_t FSNode_Open(const FSNode* cself, const char* subpath, const SAccessor** accessor)
{
    rc_t rc = 0;

    if( cself == NULL || accessor == NULL ) {
        rc = RC(rcExe, rcFile, rcOpening, rcParam, rcNull);
    } else {
        DEBUG_MSG(10, ("%s: %s/%s\n", __func__, cself->name, subpath));
        if( cself->vtbl->Open ) {
            rc = cself->vtbl->Open(cself, subpath, accessor);
        } else {
            rc = RC(rcExe, rcFile, rcOpening, rcInterface, rcUnsupported);
        }
    }
    return rc;
}

void FSNode_Release(const FSNode* cself)
{
    if( cself != NULL ) {
        FSNode* self = (FSNode*)cself;
        FSNode_DropChildren(self);
        if( self->vtbl->Release ) {
            ReleaseComplain(self->vtbl->Release, self);
        }
        DEBUG_MSG(10, ("Release FSNode %s\n", self->name));
        FREE(self);
    }
}
