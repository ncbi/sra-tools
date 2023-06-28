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

#ifndef _h_vdb_dump_inspect_
#define _h_vdb_dump_inspect_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif
    
/*
* the object can be:
*    A: the path of a directory
*        >   check if the directory is a valid VDB-object
*            if it is : we have a directory to inspect
*    B: the path of a file
*        >   check if the file is a valid VDB-object
*            if it is : transform it into a directory, which we can inspect
*    C: URL
*        >   open the url as a HTTP-file
*            check if it is a valid VDB-object
*            if it is : transform it into a directory, which we can inspect
*    D: accession
*        >   resolve the accession
*            if it resolves into a URL : continue with C
*            if it resolves into a local path : continue with A
*/
  
rc_t vdi_inspect( const KDirectory * dir, const VDBManager *mgr,
                  const char * object, bool with_compression, dump_format_t format );

#ifdef __cplusplus
}
#endif

#endif
