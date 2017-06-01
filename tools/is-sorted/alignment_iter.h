/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnologmsgy Information
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

#ifndef _h_alig_iter_
#define _h_alig_iter_

#include <klib/rc.h>
#include "common.h"    /* because of AlignmentT */

struct alig_iter;

typedef struct AlignmentT
{
    int64_t row_id;
    uint64_t pos;   /* 1-based! */
    String rname;
} AlignmentT;

/* construct an alignmet-iterator from an accession */
rc_t alig_iter_make( struct alig_iter ** self,
                     const char * acc,
                     size_t cache_capacity );

/* releae an alignment-iterator */
rc_t alig_iter_release( struct alig_iter * self );

/* get the next alignemnt from the iter */
bool alig_iter_get( struct alig_iter * self, AlignmentT * alignment );

#endif