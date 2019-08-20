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

#ifndef _h_id2name_
#define _h_id2name_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/vector.h>

struct KLock;

struct Id2name
{
    /* an array of KDataBuffer*, each one containing 0-terminated names.
    *  names do not cross the buffer boundary
    *  offset / MaxBuffer is the number of the buffer in the array
    *  offset % MaxBuffer is the offset from the start of the buffer
    **/
    Vector names;

    /* mapping between id keys and read names */
    KVector * ids;      /* uint64_t (key) -> uint64_t (offset into names) */

    uint64_t        first_free; /* offset to the first free byte in names */
    struct KLock    * lock;
};
typedef struct Id2name Id2name;

extern rc_t Id2Name_Init ( Id2name * self );
extern rc_t Id2Name_Whack ( Id2name * self );

extern rc_t Id2Name_Add ( Id2name * self, uint64_t id, const char * name ); /* expects 0-terminated name */
extern rc_t Id2Name_Get ( Id2name * self, uint64_t id, const char ** res ); /* the result is 0-terminated */

#ifdef __cplusplus
}
#endif

#endif

