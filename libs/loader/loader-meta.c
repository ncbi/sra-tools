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
*
*/
#include <kapp/extern.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/rc.h>
#include <kdb/meta.h>
#include <kapp/main.h>

#include <loader/loader-meta.h>

#include <string.h>
#include <assert.h>
#include <time.h>

static
rc_t MakeVersion(ver_t vers, char* b, size_t bsize)
{
    assert(b != NULL && bsize != 0);
    return string_printf(b, bsize, NULL, "%V", vers);
}

LIB_EXPORT rc_t CC KLoaderMeta_Write(KMDataNode* root,
                                     const char* argv0, const char* argv0_date,
                                     const char* app_name, ver_t app_version)
{
    return KLoaderMeta_WriteWithVersion ( root, argv0, argv0_date, GetKAppVersion(), app_name, app_version );
}

rc_t CC KLoaderMeta_WriteWithVersion(struct KMDataNode* root,
                                     const char* argv0, const char* argv0_date, ver_t argv0_version,
                                     const char* app_name, ver_t app_version)
{
    rc_t rc = 0;
    KMDataNode *node = NULL;

    if( root == NULL || argv0 == NULL || argv0_date == NULL ) {
        rc = RC(rcApp, rcMetadata, rcWriting, rcParam, rcNull);
    } else if( (rc = KMDataNodeOpenNodeUpdate(root, &node, "SOFTWARE")) == 0 ) {
        char str_vers[64];
        KMDataNode *subNode = NULL;
        if( (rc = KMDataNodeOpenNodeUpdate(node, &subNode, "loader")) == 0 ) {
            if( (rc = MakeVersion(argv0_version, str_vers, sizeof(str_vers))) == 0 ) {
                rc = KMDataNodeWriteAttr(subNode, "vers", str_vers);
            }
            if(rc == 0) {
                rc = KMDataNodeWriteAttr(subNode, "date", argv0_date);
            }
            if(rc == 0) {
                const char* tool_name = strrchr(argv0, '/');
                const char* r = strrchr(argv0, '\\');
                if( tool_name != NULL && r != NULL && tool_name < r ) {
                    tool_name = r;
                }
                if( tool_name++ == NULL) {
                    tool_name = argv0;
                }
                rc = KMDataNodeWriteAttr(subNode, "name", tool_name);
            }
            KMDataNodeRelease(subNode);
        }
        if(rc == 0 && (rc = KMDataNodeOpenNodeUpdate(node, &subNode, "formatter")) == 0 ) {
            if( (rc = MakeVersion(app_version, str_vers, sizeof(str_vers))) == 0 ) {
                rc = KMDataNodeWriteAttr(subNode, "vers", str_vers);
            }
            if(rc == 0) {
                rc = KMDataNodeWriteAttr(subNode, "name", app_name ? app_name : "internal");
            }
            KMDataNodeRelease(subNode);
        }
        KMDataNodeRelease(node);
    }
    if( rc == 0 && (rc = KMDataNodeOpenNodeUpdate(root, &node, "LOAD")) == 0 ) {
        KMDataNode *subNode = NULL;
        if( (rc = KMDataNodeOpenNodeUpdate(node, &subNode, "timestamp")) == 0 ) {
            time_t t = time(NULL);
            rc = KMDataNodeWrite(subNode, &t, sizeof(t));
            KMDataNodeRelease(subNode);
        }
        KMDataNodeRelease(node);
    }
    return rc;
}
