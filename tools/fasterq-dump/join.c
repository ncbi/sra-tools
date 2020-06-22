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

#include "join.h"
#include "index.h"
#include "lookup_reader.h"
#include "special_iter.h"
#include "raw_read_iter.h"
#include "fastq_iter.h"
#include "cleanup_task.h"
#include "join_results.h"
#include "progress_thread.h"

#include <klib/out.h>
#include <kproc/thread.h>
#include <insdc/insdc.h> /* for READ_TYPE_BIOLOGICAL, READ_TYPE_REVERSE */

typedef struct join
{
    const char * accession_path;
    const char * accession_short;
    struct lookup_reader * lookup;  /* lookup_reader.h */
    struct index_reader * index;    /* index.h */
    struct join_results * results;  /* join_results.h */
    SBuffer B1, B2;                 /* helper.h */
    uint64_t loop_nr;               /* in which loop of this partial join are we? */
    uint32_t thread_id;             /* in which thread are we? */
    bool cmp_read_present;          /* do we have a cmp-read column? */
} join;


static void release_join_ctx( join * j )
{
    if ( j != NULL )
    {
        release_index_reader( j-> index );
        release_lookup_reader( j -> lookup );     /* lookup_reader.c */
        release_SBuffer( &( j -> B1 ) );          /* helper.c */
        release_SBuffer( &( j -> B2 ) );          /* helper.c */
    }
}

static rc_t init_join( cmn_params * cp,
                       struct join_results * results,
                       const char * lookup_filename,
                       const char * index_filename,
                       size_t buf_size,
                       bool cmp_read_present,
                       struct join * j )
{
    rc_t rc;
    
    j -> accession_path = cp -> accession;
    j -> lookup = NULL;
    j -> results = results;
    j -> B1 . S . addr = NULL;
    j -> B2 . S . addr = NULL;
    j -> loop_nr = 0;
    j -> cmp_read_present = cmp_read_present;
    
    if ( index_filename != NULL )
    {
        if ( file_exists( cp -> dir, "%s", index_filename ) )
            rc = make_index_reader( cp -> dir, &j -> index, buf_size, "%s", index_filename ); /* index.c */
    }
    else
        j -> index = NULL;
    
    rc = make_lookup_reader( cp -> dir, j -> index, &( j -> lookup ), buf_size,
                             "%s", lookup_filename ); /* lookup_reader.c */
    if ( rc == 0 )
    {
        rc = make_SBuffer( &( j -> B1 ), 4096 );  /* helper.c */
        if ( rc != 0 )
            ErrMsg( "init_join().make_SBuffer( B1 ) -> %R", rc );
    }
    if ( rc == 0 )
    {
        rc = make_SBuffer( &( j -> B2 ), 4096 );  /* helper.c */
        if ( rc != 0 )
            ErrMsg( "init_join().make_SBuffer( B2 ) -> %R", rc );
    }

    /* we do not seek any more at the beginning of the begin of a join-slice
       the call to lookup_bases will perform an internal seek now if it is not pointed
       to the right location */

    if ( rc != 0 )
        release_join_ctx( j ); /* above! */
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t print_special_1_read( special_rec * rec, join * j )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    
    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        /* read is unaligned, print what is in row->cmp_read ( !!! no lookup !!! ) */
        rc = join_results_print( j -> results, 0, "%ld\t%S\t%S\n",
                         row_id, &( rec -> cmp_read ), &( rec -> spot_group ) ); /* join_results.c */
    }
    else
    {
        /* read is aligned ( 1 lookup ) */
        bool reverse = false;
        rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
        if ( rc == 0 )
            rc = join_results_print( j -> results, 0, "%ld\t%S\t%S\n",
                             row_id, &( j -> B1.S ), &( rec -> spot_group ) ); /* join_results.c */
    }
    return rc;
}


static rc_t print_special_2_reads( special_rec * rec, join * j )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    
    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* both unaligned, print what is in row->cmp_read ( !!! no lookup !!! )*/
            rc = join_results_print( j -> results, 0, "%ld\t%S\t%S\n",
                             row_id, &( rec -> cmp_read ), &( rec -> spot_group ) ); /* join_results.c */
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            bool reverse = false;
            rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse ); /* lookup_reader.c */
            if ( rc == 0 )
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &( rec -> cmp_read ), &( j -> B2 . S ), &( rec -> spot_group ) ); /* join_results.c */
        }
    }
    else
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            bool reverse = false;
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
            if ( rc == 0 )
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &j->B1.S, &rec->cmp_read, &rec->spot_group ); /* join_results.c */
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            bool reverse1 = false;
            bool reverse2 = false;
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse1 ); /* lookup_reader.c */
            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse2 ); /* lookup_reader.c */
            if ( rc == 0 )
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &( j -> B1 . S ), &( j -> B2 . S ), &( rec -> spot_group ) ); /* join_results.c */
        }
    }
    return rc;
}

static bool is_reverse( const fastq_rec * rec, uint32_t read_id_0 )
{
    bool res = false;
    if ( rec -> num_read_type > read_id_0 )
        res = ( ( rec -> read_type[ read_id_0 ] & READ_TYPE_REVERSE ) == READ_TYPE_REVERSE );
    return res;
}

static bool filter( join_stats * stats,
                    const fastq_rec * rec,
                    const join_options * jo,
                    uint32_t read_id_0 )
{
    bool process = true;
    if ( jo -> skip_tech && rec -> num_read_type > read_id_0 )
    {
        process = ( rec -> read_type[ read_id_0 ] & READ_TYPE_BIOLOGICAL ) == READ_TYPE_BIOLOGICAL;
        if ( !process )
            stats -> reads_technical++;
    }
    if ( process && jo -> min_read_len > 0 )
    {
        process = ( rec -> read_len[ read_id_0 ] >= jo -> min_read_len );
        if ( !process )
            stats -> reads_too_short++;
    }
    return process;
}

static rc_t print_fastq_1_read( join_stats * stats,
                                const fastq_rec * rec,
                                join * j,
                                const join_options * jo )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    
    bool process = filter( stats, rec, jo, 0 );
    if ( !process )
            return rc;

    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        /* read is unaligned, print what is in rec -> cmp_read ( no lookup ) */
        if ( join_results_match( j -> results, &( rec -> read ) ) ) /* join-results.c */
        {
            if ( rec -> read . len != rec -> quality . len )
            {
                ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (A)\n", row_id,
                        rec -> read . len, rec -> quality . len );
                stats -> reads_invalid++;
                if ( jo -> terminate_on_invalid )
                    return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
        
            if ( rec -> read . len > 0 )
            {
                rc = join_results_print_fastq_v1( j -> results,
                                                  row_id,
                                                  0, /* dst_id ( into which file to write ) */
                                                  1, /* read_id for tag-line */
                                                  jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                  &( rec -> read ),
                                                  &( rec -> quality ) ); /* join_results.c */
                if ( rc == 0 )
                    stats -> reads_written++;
            }
        }
    }
    else
    {
        /* read is aligned, ( 1 lookup ) */    
        bool reverse = is_reverse( rec, 0 );
        rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
        if ( rc == 0 )
        {
            if ( join_results_match( j -> results, &( j -> B1 . S ) ) ) /* join-results.c */
            {
                if ( j -> B1 . S . len != rec -> quality . len )
                {
                    ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (B)\n", row_id,
                             j -> B1 . S . len, rec -> quality . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid )
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                }

                if ( j -> B1 . S . len > 0 )
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      0, /* dst_id ( into which file to write ) */
                                                      1, /* read_id for tag-line */
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &( j -> B1 . S ),
                                                      &( rec -> quality ) ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written++;
                }
            }
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t print_fastq_2_reads( join_stats * stats,
                                 const fastq_rec * rec,
                                 join * j,
                                 const join_options * jo )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    uint32_t dst_id = 1;
    
    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* both unaligned, print what is in row->read (no lookup)*/        
            if ( join_results_match( j -> results, &( rec -> read ) ) ) /* join-results.c */
            {
                if ( rec -> read . len != rec -> quality . len )
                {
                    ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (C)\n", row_id,
                            rec -> read . len, rec -> quality . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid )
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                }
            
                rc = join_results_print_fastq_v1( j -> results,
                                                  row_id,
                                                  dst_id,
                                                  dst_id,
                                                  jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                  &( rec -> read ),
                                                  &( rec -> quality ) ); /* join_results.c */
                if ( rc == 0 )
                    stats -> reads_written += 2;
            }
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            bool reverse = is_reverse( rec, 1 );
            rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse ); /* lookup_reader.c */
            if ( rc == 0 )
            {
                if ( join_results_match2( j -> results, &( rec -> read ), &( j -> B2 . S ) ) ) /* join-results.c */
                {
                    if ( j -> B2 . S. len + rec -> read . len != rec -> quality . len )
                    {
                        ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (D)\n", row_id,
                                j -> B2 . S. len, rec -> quality . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid )
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( rec -> read ),
                                      &( j -> B2 . S ),
                                      &( rec -> quality ) ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written += 2;
                }
            }
        }
    }
    else
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            bool reverse = is_reverse( rec, 0 );
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
            if ( rc == 0 )
            {
                if ( join_results_match2( j -> results, &( j -> B1 . S ), &( rec -> read ) ) ) /* join-results.c */
                {
                    uint32_t rl = j -> B1 . S . len + rec -> read . len;
                    if ( rl != rec -> quality . len )
                    {
                        ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (E)\n", row_id,
                                rl, rec -> quality . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid )
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                    
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( j -> B1 . S ),
                                      &( rec -> read ),
                                      &( rec -> quality ) ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written += 2;
                }
            }
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            bool reverse1 = is_reverse( rec, 0 );
            bool reverse2 = is_reverse( rec, 1 );
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse1 ); /* lookup_reader.c */
            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse2 ); /* lookup_reader.c */
            if ( rc == 0 )
            {
                if ( join_results_match2( j -> results, &( j -> B1 . S ), &( j -> B2 . S ) ) ) /* join-results.c */
                {
                    uint32_t rl = j -> B1 . S . len + j -> B2 . S . len;
                    if ( rl != rec -> quality . len )
                    {
                        ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (F)\n", row_id,
                                rl, rec -> quality . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid )
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( j -> B1 . S ),
                                      &( j -> B2 . S ),
                                      &( rec -> quality ) ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written += 2;
                }
            }
        }
    }
    return rc;
}

/* FASTQ SPLIT */
static rc_t print_fastq_2_reads_splitted( join_stats * stats,
                                          const fastq_rec * rec,
                                          join * j,
                                          bool split_file,
                                          const join_options * jo )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    uint32_t dst_id = 1;
    String Q1, Q2;
    bool process_0 = true;
    bool process_1 = true;
    
    if ( !split_file && jo -> skip_tech )
    {
        process_0 = filter( stats, rec, jo, 0 ); /* above */
        process_1 = filter( stats, rec, jo, 1 ); /* above */
    }
    
    Q1 . addr = rec -> quality . addr;
    Q1 . size = rec -> read_len[ 0 ];
    Q1 . len  = ( uint32_t )Q1 . size;
    Q2 . addr = &rec -> quality . addr[ rec -> read_len[ 0 ] ];
    Q2 . size = rec -> read_len[ 1 ];
    Q2 . len  = ( uint32_t )Q2 . size;

    if ( ( Q1 . len + Q2 . len ) != rec -> quality . len )
    {
        ErrMsg( "row #%ld : Q[1].len(%u) + Q[2].len(%u) != Q.len(%u)\n", row_id, Q1 . len, Q2 . len, rec -> quality . len );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid )
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }
    
    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            String READ1, READ2;
            if ( process_0 )
            {
                READ1 . addr = rec -> read . addr;
                READ1 . size = rec -> read_len[ 0 ];
                READ1 . len = ( uint32_t )READ1 . size;
                if ( READ1 . len != Q1 . len )
                {
                    ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 . len, Q1 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid )
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                }
                process_0 = ( READ1 . len > 0 );
            }
            if ( !process_0 )
                dst_id = 0;

            if ( process_1 )
            {
                READ2 . addr = &rec -> read . addr[ rec -> read_len[ 0 ] ];
                READ2 . size = rec -> read_len[ 1 ];
                READ2 . len = ( uint32_t )READ2 . size;
                if ( READ2 . len != Q2 . len )
                {
                    ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 . len, Q2 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid )
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                }
                process_1 = ( READ2 . len > 0 );
            }
            if ( !process_1 )
                dst_id = 0;

            /* both unaligned, print what is in row -> cmp_read ( no lookup ) */
            if ( process_0 )
            {
                if ( join_results_match( j -> results, &READ1 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( rc == 0 )
                    {
                        stats -> reads_written ++;
                        if ( split_file && dst_id > 0 )
                            dst_id++;
                    }
                }
            }

            if ( rc == 0 && process_1 )
            {
                if ( join_results_match( j -> results, &READ2 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written ++;
                }
            }
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            const String * READ1 = &( rec -> read );
            String * READ2 = NULL;
            if ( process_0 )
            {
                if ( READ1 -> len != Q1 . len )
                {
                    ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 -> len, Q1 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid )
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                }
                process_0 = ( READ1 -> len > 0 );
            }
            if ( !process_0 )
                dst_id = 0;

            if ( process_1 )
            {
                bool reverse = is_reverse( rec, 1 );
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2, reverse ); /* lookup_reader.c */
                if ( rc == 0 )
                {
                    READ2 = &( j -> B2 . S );
                    if ( READ2 -> len != Q2 . len )
                    {
                        ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 -> len, Q2 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid )
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                    process_1 = ( READ2 -> len > 0 );
                }
            }
            if ( !process_1 )
                dst_id = 0;

            if ( rc == 0 && process_0 )
            {
                if ( join_results_match( j -> results, READ1 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written ++;
                    if ( split_file && dst_id > 0 )
                        dst_id++;
                }
            }
            if ( rc == 0 && process_1 )
            {
                if ( join_results_match( j -> results, READ2 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written ++;
                }
            }
        }
    }
    else
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            String * READ1 = NULL;
            const String * READ2 = &( rec -> read );
            
            if ( process_0 )
            {
                bool reverse = is_reverse( rec, 0 );
                rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1, reverse ); /* lookup_reader.c */
                if ( rc == 0 )
                {
                    READ1 = &j -> B1 . S;
                    if ( READ1 -> len != Q1 . len )
                    {
                        ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 -> len, Q1 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid )
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                    process_0 = ( READ1 -> len > 0 );
                }
            }
            if ( !process_0 )
                dst_id = 0;

            if ( process_1 )
            {
                if ( READ2 -> len != Q2 . len )
                {
                    ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 -> len, Q2 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid )
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                }
                process_1 = ( READ2 -> len > 0 );
            }
            if ( !process_1 )
                dst_id = 0;
           
            if ( rc == 0 && process_0 )
            {
                if ( join_results_match( j -> results, READ1 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written ++;
                    if ( split_file && dst_id > 0 )
                        dst_id++;
                }
            }
            if ( rc == 0 && process_1 )
            {
                if ( join_results_match( j -> results, READ2 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written ++;
                }
            }
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            String * READ1 = NULL;
            String * READ2 = NULL;

            if ( process_0 )
            {
                bool reverse = is_reverse( rec, 0 );
                rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1, reverse ); /* lookup_reader.c */
                if ( rc == 0 )
                {
                    READ1 = &j -> B1 . S;
                    if ( READ1 -> len != Q1 . len )
                    {
                        ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 -> len, Q1 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid )
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                    process_0 = ( READ1 -> len > 0 );
                }
            }
            if ( !process_0 )
                dst_id = 0;

            if ( rc == 0 && process_1 )
            {
                bool reverse = is_reverse( rec, 1 );
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2, reverse ); /* lookup_reader.c */
                if ( rc == 0 )
                {
                    READ2 = &j -> B2 . S;
                    if ( READ2 -> len != Q2 . len )
                    {
                        ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 -> len, Q2 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid )
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                    process_1 =( READ2 -> len > 0 );
                }
            }
            if ( !process_1 )
                dst_id = 0;
                
            if ( rc == 0 && process_0 )
            {
                if ( join_results_match( j -> results, READ1 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written ++;
                    if ( split_file && dst_id > 0 )
                        dst_id++;
                }
            }
            if ( rc == 0 && process_1 )
            {
                if ( join_results_match( j -> results, READ2 ) ) /* join-results.c */
                {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( rc == 0 )
                        stats -> reads_written ++;
                }
            }
        }
    }
    return rc;
}


static rc_t extract_csra_row_count( KDirectory * dir,
                                    const VDBManager * vdb_mgr,
                                    const char * accession_path,
                                    size_t cur_cache,
                                    uint64_t * res )
{
    cmn_params cp = { dir, vdb_mgr, accession_path, 0, 0, cur_cache };
    struct fastq_csra_iter * iter;
    fastq_iter_opt opt = { false, false, false, false }; /* fastq_iter.h */
    rc_t rc = make_fastq_csra_iter( &cp, opt, &iter ); /* fastq_iter.c */
    if ( rc == 0 )
    {
        *res = get_row_count_of_fastq_csra_iter( iter );
        destroy_fastq_csra_iter( iter );
    }
    return rc;
}

static rc_t perform_special_join( cmn_params * cp,
                                  join * j,
                                  struct bg_progress * progress )
{
    struct special_iter * iter;
    rc_t rc = make_special_iter( cp, &iter ); /* special_iter.c */
    if ( rc == 0 )
    {
        special_rec rec;
        while ( rc == 0 && get_from_special_iter( iter, &rec, &rc ) )
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                if ( rec . num_reads == 1 )
                    rc = print_special_1_read( &rec, j ); /* above */
                else
                    rc = print_special_2_reads( &rec, j ); /* above */

                j -> loop_nr ++;

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_special_iter( iter ); /* special_iter.c */
    }
    else
        ErrMsg( "make_special_iter() -> %R", rc );
    return rc;
}


static rc_t perform_whole_spot_join( cmn_params * cp,
                                     join_stats * stats,
                                     join * j,
                                     struct bg_progress * progress,
                                     const join_options * jo )
{
    rc_t rc;
    struct fastq_csra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = false;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    
    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_join().make_fastq_csra_iter() -> %R", rc );
    else
    {
        fastq_rec rec; /* fastq_iter.h */
        join_options local_opt = { jo -> rowid_as_name,
                                   false, 
                                   jo -> print_read_nr,
                                   jo -> print_name,
                                   jo -> terminate_on_invalid,
                                   jo -> min_read_len,
                                   jo -> filter_bases };
        while ( rc == 0 && get_from_fastq_csra_iter( iter, &rec, &rc ) ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;
            
                if ( rec . num_alig_id == 1 )
                    rc = print_fastq_1_read( stats, &rec, j, &local_opt ); /* above */
                else
                    rc = print_fastq_2_reads( stats, &rec, j, &local_opt ); /* above */

                if ( rc == 0 )
                    j -> loop_nr ++;
                else
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t perform_fastq_split_spot_join( cmn_params * cp,
                                      join_stats * stats,
                                      join * j,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_csra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    
    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_split_spot_join().make_fastq_csra_iter() -> %R", rc );
    else
    {
        fastq_rec rec; /* fastq_iter.h */
        while ( rc == 0 && get_from_fastq_csra_iter( iter, &rec, &rc ) ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;
            
                if ( rec . num_alig_id == 1 )
                    rc = print_fastq_1_read( stats, &rec, j, jo ); /* above */
                else
                    rc = print_fastq_2_reads_splitted( stats, &rec, j, false, jo ); /* above */

                if ( rc == 0 )
                    j -> loop_nr ++;
                else
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t perform_fastq_split_file_join( cmn_params * cp,
                                      join_stats * stats,
                                      join * j,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_csra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    
    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_split_file_join().make_fastq_csra_iter() -> %R", rc );
    else
    {
        fastq_rec rec; /* fastq_iter.h */
        join_options local_opt =
            { 
                jo -> rowid_as_name,
                false,
                jo -> print_read_nr,
                jo -> print_name,
                jo -> terminate_on_invalid,
                jo -> min_read_len,
                jo -> filter_bases
            };
            
        while ( rc == 0 && get_from_fastq_csra_iter( iter, &rec, &rc ) ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;
            
                if ( rec . num_alig_id == 1 )
                    rc = print_fastq_1_read( stats, &rec, j, &local_opt );
                else
                    rc = print_fastq_2_reads_splitted( stats, &rec, j, true, &local_opt );

                if ( rc == 0 )
                    j -> loop_nr ++;
                else
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );
    }
    return rc;
}

static rc_t perform_fastq_split_3_join( cmn_params * cp,
                                      join_stats * stats,
                                      join * j,
                                      struct bg_progress * progress,
                                      const join_options * jo )
{
    rc_t rc;
    struct fastq_csra_iter * iter;
    fastq_iter_opt opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    
    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_split_3_join().make_fastq_csra_iter() -> %R", rc );
    else
    {
        fastq_rec rec; /* fastq_iter.h */
        rc_t rc_iter = 0;
        
        join_options local_opt =
            {
                jo -> rowid_as_name,
                false,
                jo -> print_read_nr,
                jo -> print_name,
                jo -> terminate_on_invalid,
                jo -> min_read_len,
                jo -> filter_bases
            };

        while ( rc == 0 && get_from_fastq_csra_iter( iter, &rec, &rc_iter ) && rc_iter == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( rec . num_alig_id == 1 )
                    rc = print_fastq_1_read( stats, &rec, j, &local_opt );
                else
                    rc = print_fastq_2_reads_splitted( stats, &rec, j, true, &local_opt );

                if ( rc == 0 )
                    j -> loop_nr ++;
                else
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );
        
        if ( rc == 0 && rc_iter != 0 )
            rc = rc_iter;
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

typedef struct join_thread_data
{
    char part_file[ 4096 ];

    join_stats stats; /* helper.h */
    
    KDirectory * dir;
    const VDBManager * vdb_mgr;

    const char * accession_path;
    const char * accession_short;
    const char * lookup_filename;
    const char * index_filename;
    struct bg_progress * progress;
    struct temp_registry * registry;
    KThread * thread;
    
    int64_t first_row;
    uint64_t row_count;
    size_t cur_cache;
    size_t buf_size;
    format_t fmt;
    uint32_t thread_id;
    bool cmp_read_present;

    const join_options * join_options;
    
} join_thread_data;

static rc_t CC cmn_thread_func( const KThread * self, void * data )
{
    rc_t rc = 0;
    join_thread_data * jtd = data;
    struct join_results * results = NULL;
    
    if ( rc == 0 )
        rc = make_join_results( jtd -> dir,
                                &results,
                                jtd -> registry,
                                jtd -> part_file,
                                jtd -> accession_short,
                                jtd -> buf_size,
                                4096,
                                jtd -> join_options -> print_read_nr,
                                jtd -> join_options -> print_name,
                                jtd -> join_options -> filter_bases );
    
    if ( rc == 0 && results != NULL )
    {
        join j;
        cmn_params cp = { jtd -> dir, jtd -> vdb_mgr,
                          jtd -> accession_path, jtd -> first_row, jtd -> row_count, jtd -> cur_cache };

        rc = init_join( &cp,
                        results,
                        jtd -> lookup_filename,
                        jtd -> index_filename,
                        jtd -> buf_size,
                        jtd -> cmp_read_present,
                        &j ); /* above */
        if ( rc == 0 )
        {
            j . thread_id = jtd -> thread_id;

            switch ( jtd -> fmt )
            {
                case ft_special             : rc = perform_special_join( &cp,
                                                        &j,
                                                        jtd -> progress ); break;

                case ft_whole_spot          : rc = perform_whole_spot_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                case ft_fastq_split_spot    : rc = perform_fastq_split_spot_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                case ft_fastq_split_file    : rc = perform_fastq_split_file_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                case ft_fastq_split_3       : rc = perform_fastq_split_3_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                default : break;
            }
            release_join_ctx( &j );
        }
        destroy_join_results( results );
    }
    return rc;
}

rc_t execute_db_join( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_path,
                    const char * accession_short,
                    join_stats * stats,
                    const char * lookup_filename,
                    const char * index_filename,
                    const struct temp_dir * temp_dir,
                    struct temp_registry * registry,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    bool show_progress,
                    format_t fmt,
                    const join_options * join_options )
{
    rc_t rc = 0;
    
    if ( show_progress )
    {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "join   :" );
        KOutHandlerSetStdOut();
    }
    
    if ( rc == 0 )
    {
        uint64_t row_count = 0;
        bool name_column_present, cmp_read_column_present;
        
        rc = cmn_check_db_column( dir, vdb_mgr, accession_path, "SEQUENCE", "NAME", &name_column_present ); /* cmn_iter.c */
        if ( rc == 0 )
            rc = cmn_check_db_column( dir, vdb_mgr, accession_path, "SEQUENCE", "CMP_READ", &cmp_read_column_present ); /* cmn_iter.c */
        
        rc = extract_csra_row_count( dir, vdb_mgr, accession_path, cur_cache, &row_count );
        if ( rc == 0 && row_count > 0 )
        {
            Vector threads;
            int64_t row = 1;
            uint32_t thread_id;
            uint64_t rows_per_thread;
            struct bg_progress * progress = NULL;
            struct join_options corrected_join_options;
            
            VectorInit( &threads, 0, num_threads );

            corrected_join_options . rowid_as_name = name_column_present ? join_options -> rowid_as_name : true;
            corrected_join_options . skip_tech = join_options -> skip_tech;
            corrected_join_options . print_read_nr = join_options -> print_read_nr;
            corrected_join_options . print_name = join_options -> print_name;
            corrected_join_options . min_read_len = join_options -> min_read_len;
            corrected_join_options . filter_bases = join_options -> filter_bases;
            corrected_join_options . terminate_on_invalid = join_options -> terminate_on_invalid;
            
            if ( row_count < ( num_threads * 100 ) )
            {
                num_threads = 1;
                rows_per_thread = row_count;
            }
            else
            {
                rows_per_thread = ( row_count / num_threads ) + 1;
            }

            if ( show_progress )
                rc = bg_progress_make( &progress, row_count, 0, 0 ); /* progress_thread.c */
            
            for ( thread_id = 0; rc == 0 && thread_id < num_threads; ++thread_id )
            {
                join_thread_data * jtd = calloc( 1, sizeof * jtd );
                if ( jtd == NULL )
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                else
                {
                    jtd -> dir              = dir;
                    jtd -> vdb_mgr          = vdb_mgr;
                    jtd -> accession_path   = accession_path;
                    jtd -> accession_short  = accession_short;
                    jtd -> lookup_filename  = lookup_filename;
                    jtd -> index_filename   = index_filename;
                    jtd -> first_row        = row;
                    jtd -> row_count        = rows_per_thread;
                    jtd -> cur_cache        = cur_cache;
                    jtd -> buf_size         = buf_size;
                    jtd -> progress         = progress;
                    jtd -> registry         = registry;
                    jtd -> fmt              = fmt;
                    jtd -> join_options     = &corrected_join_options;
                    jtd -> thread_id        = thread_id;
                    jtd -> cmp_read_present = cmp_read_column_present;

                    rc = make_joined_filename( temp_dir, jtd -> part_file, sizeof jtd -> part_file,
                                               accession_short, thread_id ); /* temp_dir.c */

                    if ( rc == 0 )
                    {
                        rc = helper_make_thread( &jtd -> thread, cmn_thread_func, jtd, THREAD_BIG_STACK_SIZE );
                        if ( rc != 0 )
                            ErrMsg( "join.c helper_make_thread( fastq/special #%d ) -> %R", thread_id, rc );
                        else
                        {
                            rc = VectorAppend( &threads, NULL, jtd );
                            if ( rc != 0 )
                                ErrMsg( "join.c VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                        }
                        row += rows_per_thread;
                    }
                }
            }
            
            {
                /* collect the threads, and add the join_stats */
                uint32_t i, n = VectorLength( &threads );
                for ( i = VectorStart( &threads ); i < n; ++i )
                {
                    join_thread_data * jtd = VectorGet( &threads, i );
                    if ( jtd != NULL )
                    {
                        rc_t rc_thread;
                        KThreadWait( jtd -> thread, &rc_thread );
                        if ( rc_thread != 0 )
                            rc = rc_thread;

                        KThreadRelease( jtd -> thread );

                        add_join_stats( stats, &jtd -> stats ); /* helper.c */

                        free( jtd );
                    }
                }
                VectorWhack ( &threads, NULL, NULL );
            }
            bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/
        }
    }
    return rc;
}

/*
    test-function to check if each row in the alignment-table can be found by referencing the seq-row-id and req-read-id
    in the lookup-file
*/
rc_t check_lookup( const KDirectory * dir,
                   size_t buf_size,
                   size_t cursor_cache,
                   const char * lookup_filename,
                   const char * index_filename,
                   const char * accession )
{
    struct index_reader * index;
    rc_t rc = make_index_reader( dir, &index, buf_size, "%s", index_filename );
    if ( rc == 0 )
    {
        struct lookup_reader * lookup;  /* lookup_reader.h */
        rc =  make_lookup_reader( dir, index, &lookup, buf_size, "%s", lookup_filename ); /* lookup_reader.c */
        if ( rc == 0 )
        {
            struct raw_read_iter * iter; /* raw_read_iter.h */
            cmn_params params; /* helper.h */
            
            params . dir = dir;
            params . accession = accession;
            params . first_row = 0;
            params . row_count = 0;
            params . cursor_cache = cursor_cache;
            
            rc = make_raw_read_iter( &params, &iter ); /* raw_read_iter.c */
            if ( rc == 0 )
            {
                SBuffer buffer;
                rc = make_SBuffer( &buffer, 4096 );
                if ( rc == 0 )
                {
                    raw_read_rec rec; /* raw_read_iter.h */
                    uint64_t loop = 0;
                    bool running = get_from_raw_read_iter( iter, &rec, &rc ); /* raw_read_iter.c */
                    while( rc == 0 && running )
                    {
                        rc = lookup_bases( lookup, rec . seq_spot_id, rec . seq_read_id, &buffer, false );
                        if ( rc != 0 )
                            KOutMsg( "lookup_bases( %lu.%u ) --> %R\n", rec . seq_spot_id, rec . seq_read_id, rc );
                        else
                        {
                            running = get_from_raw_read_iter( iter, &rec, &rc ); /* raw_read_iter.c */
                            if ( 0 == loop % 1000 )
                                KOutMsg( "[loop #%lu] ", loop / 1000 );
                            loop++;
                        }
                    }
                    release_SBuffer( &buffer );                
                }
                destroy_raw_read_iter( iter ); /* raw_read_iter.c */
            }
            release_lookup_reader( lookup ); /* lookup_reader.c */
        }
        release_index_reader( index );
    }
    return rc;
}

rc_t check_lookup_this( const KDirectory * dir,
                        size_t buf_size,
                        size_t cursor_cache,
                        const char * lookup_filename,
                        const char * index_filename,
                        uint64_t seq_spot_id,
                        uint32_t seq_read_id )
{
    struct index_reader * index;
    rc_t rc = make_index_reader( dir, &index, buf_size, "%s", index_filename );
    if ( rc == 0 )
    {
        struct lookup_reader * lookup;  /* lookup_reader.h */
        rc =  make_lookup_reader( dir, index, &lookup, buf_size, "%s", lookup_filename ); /* lookup_reader.c */
        if ( rc == 0 )
        {
            SBuffer buffer;
            rc = make_SBuffer( &buffer, 4096 );
            if ( rc == 0 )
            {
                rc = lookup_bases( lookup, seq_spot_id, seq_read_id, &buffer, false );
                if ( rc != 0 )
                    KOutMsg( "lookup_bases( %lu.%u ) --> %R\n", seq_spot_id, seq_read_id, rc );
                release_SBuffer( &buffer );                
            }
            release_lookup_reader( lookup ); /* lookup_reader.c */
        }
        release_index_reader( index );
    }
    return rc;
}
