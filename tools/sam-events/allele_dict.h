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

struct Allele_Dict;

typedef struct AlignmentT
{
    uint64_t pos; /* 1-based! */
    String rname;
    String cigar;
    String read;
} AlignmentT;

/* construct a allele-dictionary */
rc_t allele_dict_make( struct Allele_Dict ** ad, const String * rname );

/* releae a allele_dictionary */
rc_t allele_dict_release( struct Allele_Dict * ad );

/* put an event into the allele_dictionary */
rc_t allele_dict_put( struct Allele_Dict * ad, size_t position, uint32_t deletes, uint32_t inserts, const char * bases );

typedef rc_t ( CC * on_ad_event )( uint32_t count, const String * rname, size_t position,
                                    uint32_t deletes, uint32_t inserts, const char * bases, void * user_data );

/* call a callback for each event in the allele_dictionary */
rc_t allele_dict_visit( struct Allele_Dict * ad, uint64_t pos, on_ad_event f, void * user_data );

/* get min and max - positions from the dictionary */
rc_t allele_get_min_max( struct Allele_Dict * ad, uint64_t * min_pos, uint64_t * max_pos );

/* call the callback-function for every entry smaller then the given position and then purge everything the has been visited */
rc_t allele_dict_purge( struct Allele_Dict * ad, uint64_t pos );


#endif
