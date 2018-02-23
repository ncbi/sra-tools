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

#ifndef _h_ref_iter_
#define _h_ref_iter_

#include <klib/rc.h>
#include <klib/text.h>
#include <klib/vector.h>

struct ref_iter;

typedef struct RefT
{
    String rname;               /* the name of the reference ( points into volatile buffer ! ) */
    int64_t start_row_id;       /* the start-row-id of this reference in the REFERENCE-table */
    uint64_t count;             /* how many rows for this reference in the REFERENCE-table */
    uint64_t reflen;            /* how many bases does this reference have */
    uint32_t block_size;        /* how many ref-positions per REFERENCE-table-row */
} RefT;

/* construct an ref-iterator from an accession */
rc_t ref_iter_make( struct ref_iter ** self, const char * acc, size_t cache_capacity );

/* releae an ref-iterator */
rc_t ref_iter_release( struct ref_iter * self );

/* get the next alignemnt from the iter */
bool ref_iter_get( struct ref_iter * self, RefT * reference );

/* initialize the vector and fill it with RefT-structs... */
rc_t ref_iter_make_vector( Vector * vec, const char * acc, size_t cache_capacity );

/* free all RefT-structs in the vector, release the vector... */
void ref_iter_release_vector( Vector * vec );

/* fill out the RefT struct for a specific reference */
rc_t ref_iter_find( RefT ** ref, const char * acc, size_t cache_capacity, const String * rname );

rc_t RefT_copy( const RefT * src, RefT **dst );
void RefT_release( RefT * ref );

#endif