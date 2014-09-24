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

#ifndef _h_ref_exclude_
#define _h_ref_exclude_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <klib/container.h>
#include <klib/text.h>
#include <klib/log.h>
#include <kfs/directory.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

typedef struct ref_exclude
{
    BSTree ref_nodes;
    BSTree translations;
    VDBManager *mgr;
    char * path;
    void * last_used_ref_node;
    bool info;
} ref_exclude;


rc_t make_ref_exclude( ref_exclude *exclude, KDirectory *dir,
                       const char * path, bool info );


rc_t get_ref_exclude( ref_exclude *exclude, 
                      const String * name,
                      int32_t ref_offset, uint32_t ref_len,
                      uint8_t *exclude_vector,
                      uint32_t *active );

rc_t whack_ref_exclude( ref_exclude *exclude );

#ifdef __cplusplus
}
#endif

#endif
