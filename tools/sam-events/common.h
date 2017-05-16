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

#ifndef _h_common_
#define _h_common_

#include <klib/rc.h>
#include <vdb/cursor.h>
#include <kapp/args.h>

typedef struct counters
{
    uint32_t fwd, rev, t_pos, t_neg, total;
} counters;

typedef struct row_range
{
    int64_t first_row;
    uint64_t row_count;
} row_range;

typedef struct AlignmentT
{
    uint64_t pos; /* 1-based! */
    bool fwd;
    bool first;
    String rname;
    String cigar;
    String read;
} AlignmentT;


rc_t log_err( const char * t_fmt, ... );

rc_t add_cols_to_cursor( const VCursor *curs, uint32_t * idx_array,
        const char * tbl_name, const char * acc, uint32_t n, ... );

void inspect_sam_flags( AlignmentT * al, uint32_t sam_flags );

rc_t get_bool( const Args * args, const char *option, bool *value );
rc_t get_charptr( const Args * args, const char *option, const char ** value );
rc_t get_uint32( const Args * args, const char *option, uint32_t * value, uint32_t dflt );
rc_t get_size_t( const Args * args, const char *option, size_t * value, size_t dflt );

struct Writer;

rc_t writer_release( struct Writer * wr );
rc_t writer_make( struct Writer ** wr, const char * filename );
rc_t writer_write( struct Writer * wr, const char * fmt, ... );


AlignmentT * copy_alignment( const AlignmentT * src );
void free_alignment_copy( AlignmentT * src );

void clear_recorded_errors( void );

#endif