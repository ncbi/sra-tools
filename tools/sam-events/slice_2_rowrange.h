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

#ifndef _h_slice_to_row_range_
#define _h_slice_to_row_range_

#include <klib/rc.h>
#include "common.h"    /* because of row_range */
#include "slice.h"     /* because of slice */
#include <vdb/database.h> /* because of VDatabase */

rc_t slice_2_row_range( const struct slice * slice, const char * acc, struct row_range * range );
rc_t slice_2_row_range_db( const struct slice * slice, const char * acc, const VDatabase * db, struct row_range * range );


typedef struct BlockInfo
{
    int64_t first_alignment_id;
    uint32_t count;
    bool sorted;
} BlockInfo;

typedef struct RefInfo
{
    const String * name;
    const String * seq_id;
    int64_t first_row_in_reftable;
    uint64_t row_count_in_reftable;
    int64_t first_alignment_id;
    uint64_t alignment_id_count;
    uint64_t len;
    Vector blocks; /* vector of pointers to BlockInfo structs */
    uint32_t blocksize;    
    bool circular;    
    bool sorted;
} RefInfo;

rc_t extract_reference_info( const char * acc, Vector * infos );
void clear_reference_info( Vector * infos );

#endif