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

#ifndef _h_coverage_iter_
#define _h_coverage_iter_

#include <klib/rc.h>
#include <klib/text.h>

#include "ref_iter.h"
#include "slice.h"

struct simple_coverage_iter;

typedef struct SimpleCoverageT
{
    int64_t ref_row_id;
    uint64_t start_pos;
    uint32_t len;
    uint32_t prim;       /* how many primary id's */
    uint32_t sec;        /* how many secondary id's */
    int64_t * prim_ids;  /* the primary alignment id's */
    int64_t * sec_ids;   /* the primary alignment id's */
} SimpleCoverageT;

/* construct an coverage-iterator from an accession */
rc_t simple_coverage_iter_make( struct simple_coverage_iter ** self,
                         const char * src,
                         size_t cache_capacity,
                         const RefT * ref );

/* releae an ref-iterator */
rc_t simple_coverage_iter_release( struct simple_coverage_iter * self );

/* get the next alignemnt from the iter */
bool simple_coverage_iter_get( struct simple_coverage_iter * self, SimpleCoverageT * ct );

bool simple_coverage_iter_get_capped( struct simple_coverage_iter * self, SimpleCoverageT * ct, uint32_t min, uint32_t max );


typedef struct DetailedCoverage
{
    uint64_t start_pos;
    uint32_t * coverage;
    uint32_t len;
} DetailedCoverage;


rc_t detailed_coverage_make( const char * src,
                             size_t cache_capacity,
                             const slice * slice,
                             DetailedCoverage * coverage );

rc_t detailed_coverage_release( DetailedCoverage * self );
                             
#endif