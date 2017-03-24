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

#ifndef _h_allele_dict_
#define _h_allele_dict_

#include <klib/rc.h>
#include <klib/text.h>

struct Writer;

rc_t writer_make( struct Writer ** wr, const char * filename );
rc_t writer_release( struct Writer * wr );
rc_t writer_write( struct Writer * wr, const char * fmt, ... );

/* -------------------------------------------------------------------------------------- */

struct Allele_Dict;

typedef struct counters
{
    uint32_t fwd, rev, t_pos, t_neg;
} counters;

typedef struct AlignmentT
{
    uint64_t pos; /* 1-based! */
    bool fwd;
    bool first;
    String rname;
    String cigar;
    String read;
} AlignmentT;

/* construct a allele-dictionary */
rc_t allele_dict_make( struct Allele_Dict ** ad, const String * rname );

/* releae a allele_dictionary */
rc_t allele_dict_release( struct Allele_Dict * ad );

/* put an event into the allele_dictionary */
rc_t allele_dict_put( struct Allele_Dict * ad, uint64_t position,
                      uint32_t deletes, uint32_t inserts, const char * bases, bool fwd, bool first );

typedef rc_t ( CC * on_ad_event )( const counters * count, const String * rname, uint64_t position,
                                    uint32_t deletes, uint32_t inserts, const char * bases,
                                    void * user_data );

/* call a callback for each event in the allele_dictionary */
rc_t allele_dict_visit_all_and_release( struct Allele_Dict * ad, on_ad_event f, void * user_data );

/* call a callback for each event until a certain purge-distance is reached */
rc_t allele_dict_visit_and_purge( struct Allele_Dict * ad, uint32_t purge_dist, on_ad_event f, void * user_data );

#endif
