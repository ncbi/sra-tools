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

#ifndef _h_alig_consumer_
#define _h_alig_consumer_

#include <klib/rc.h>
#include "common.h"
#include "allele_lookup.h"
#include "slice.h"
#include "expandCIGAR.h"

struct alig_consumer;

/* construct an alignmet-iterator from an accession */
rc_t alig_consumer_make( struct alig_consumer ** ac, uint32_t min_count, const counters * limits,
                         struct Allele_Lookup * lookup, const slice * slice, struct cFastaFile * fasta, uint32_t purge );

/* releae an alignment-iterator */
rc_t alig_consumer_release( struct alig_consumer * ac );

rc_t alig_consumer_consume_alignment( struct alig_consumer * self, AlignmentT * al );

rc_t alig_consumer_visit_and_purge( struct alig_consumer * self, uint32_t purge );

bool alig_consumer_get_unsorted( struct alig_consumer * self );

#endif