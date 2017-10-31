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
#include "tbl_join.h"
#include "fastq_iter.h"
#include "cleanup_task.h"
#include "join_results.h"

#include <klib/out.h>
#include <kproc/thread.h>

static const char * fmt_fq2_1 = "@%s.%ld %ld length=%d\n%S\n+%s.%ld %ld length=%d\n%S\n";
static rc_t print_fq2_read_1( struct join_results * results,
                              const char * accession,
                              int64_t row_id,
                              uint32_t read_id,
                              const String * read,
                              const String * quality )
{
    return join_results_print( results, read_id, fmt_fq2_1,
                    accession, row_id, row_id, read -> len, read,
                    accession, row_id, row_id, quality -> len, quality ); /* join_results.c */
}

static const char * fmt_fq2_2 = "@%s.%ld %S length=%d\n%S\n+%s.%ld %S length=%d\n%S\n";
static rc_t print_fq2_read_2( struct join_results * results,
                              const char * accession,
                              int64_t row_id,
                              uint32_t read_id,
                              const String * name,
                              const String * read,
                              const String * quality )
{
    return join_results_print( results, read_id, fmt_fq2_2,
                    accession, row_id, name, read -> len, read,
                    accession, row_id, name, quality -> len, quality ); /* join_results.c */
}

static rc_t print_fastq_1_read( const char * accession,
                                struct join_results * results,
                                const fastq_sra_rec * rec,
                                bool rowid_as_name )
{
    rc_t rc;
    
    /* read is unaligned, print what is in rec -> cmp_read ( no lookup ) */
    if ( rowid_as_name )
        rc = print_fq2_read_1( results, accession, rec -> row_id, 0, &( rec -> read ), &( rec -> quality ) );
    else
        rc = print_fq2_read_2( results, accession, rec -> row_id, 0, &( rec -> name ), &( rec -> read ), &( rec -> quality ) );

    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t print_fastq_2_reads_splitted( const char * accession,
                                          struct join_results * results,
                                          const fastq_sra_rec * rec,
                                          bool split_file,
                                          bool rowid_as_name )
{
    rc_t rc;
    int64_t row_id = rec -> row_id;
    String Q1, Q2, R1, R2;
    uint32_t read_id = 0;
    
    R1 . addr = rec -> read . addr;
    R1 . len  = R1 . size = rec -> read_len[ 0 ];
    
    R2 . addr = &rec -> read . addr[ rec -> read_len[ 0 ] ];
    R2 . len  = R2 . size = rec -> read_len[ 1 ];

    Q1 . addr = rec -> quality . addr;
    Q1 . len  = Q1 . size = rec -> read_len[ 0 ];

    Q2 . addr = &rec -> quality . addr[ rec -> read_len[ 0 ] ];
    Q2 . len  = Q2 . size = rec -> read_len[ 1 ];

    if ( rowid_as_name )
        rc = print_fq2_read_1( results, accession, row_id, read_id, &R1, &Q1 );
    else
        rc = print_fq2_read_2( results, accession, row_id, read_id, &rec -> name, &R1, &Q1 );

    if ( split_file )
        read_id++;
        
    if ( rc == 0 )
    {
        if ( rowid_as_name )
            rc = print_fq2_read_1( results, accession, row_id, read_id, &R2, &Q2 );
        else
            rc = print_fq2_read_2( results, accession, row_id, read_id, &rec -> name, &R2, &Q2 );
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t perform_fastq_join( cmn_params * cp,
                                const char * accession,
                                const char * tbl_name,
                                struct join_results * results,
                                atomic_t * join_progress,
                                bool rowid_as_name )
{
    struct fastq_sra_iter * iter;
    bool with_read_len = false;
    bool with_name = !rowid_as_name;
    rc_t rc = make_fastq_sra_iter( cp, tbl_name, &iter, with_read_len, with_name ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_join().make_fastq_iter() -> %R", rc );
    else
    {
        fastq_sra_rec rec;
        while ( get_from_fastq_sra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                rc = print_fastq_1_read( accession, results, &rec, rowid_as_name );
                
                if ( join_progress != NULL )
                    atomic_inc( join_progress );
            }
        }
        destroy_fastq_sra_iter( iter );
    }
    return rc;
}

static rc_t perform_fastq_join_split( cmn_params * cp,
                                      const char * accession,
                                      const char * tbl_name,
                                      struct join_results * results,
                                      atomic_t * join_progress,
                                      bool split_file,
                                      bool rowid_as_name )
{
    struct fastq_sra_iter * iter;
    bool with_read_len = true;
    bool with_name = !rowid_as_name;
    rc_t rc = make_fastq_sra_iter( cp, tbl_name, &iter, with_read_len, with_name ); /* fastq-iter.c */
    if ( rc == 0 )
    {
        fastq_sra_rec rec;
        while ( get_from_fastq_sra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                if ( rec . num_reads == 1 )
                    rc = print_fastq_1_read( accession, results, &rec, rowid_as_name );
                else
                    rc = print_fastq_2_reads_splitted( accession, results, &rec, split_file, rowid_as_name );

                if ( join_progress != NULL )
                    atomic_inc( join_progress );
            }
        }
        destroy_fastq_sra_iter( iter );
    }
    else
        ErrMsg( "make_fastq_iter() -> %R", rc );
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

typedef struct join_thread_data
{
    char part_file[ 4096 ];
        
    KDirectory * dir;
    const char * accession;
    const char * tbl_name;
    atomic_t * join_progress;
    struct temp_registry * registry;
    
    int64_t first_row;
    uint64_t row_count;
    size_t cur_cache;
    size_t buf_size;
    bool split_file;    
    format_t fmt;
    bool rowid_as_name;

} join_thread_data;

static rc_t CC cmn_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    join_thread_data * jtd = data;
    struct join_results * results = NULL;
    
    if ( rc == 0 )
        rc = make_join_results( jtd -> dir, &results, jtd -> registry, jtd -> part_file, jtd -> buf_size, 4096 );
    
    if ( rc == 0 && results != NULL )
    {
        cmn_params cp = { jtd -> dir, jtd -> accession, jtd -> first_row, jtd -> row_count, jtd -> cur_cache };
        switch( jtd -> fmt )
        {
            case ft_fastq       : rc = perform_fastq_join( &cp,
                                            jtd -> accession,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> join_progress,
                                            jtd -> rowid_as_name ); break; /* above */
                                            
            case ft_fastq_split : rc = perform_fastq_join_split( &cp,
                                            jtd -> accession,
                                            jtd -> tbl_name,
                                            results,
                                            jtd -> join_progress,
                                            jtd -> split_file,
                                            jtd -> rowid_as_name ); break; /* above */

            default : break;
        }
        destroy_join_results( results );
    }
    
    free( ( void * ) data );
    return rc;
}

static rc_t extract_sra_row_count( KDirectory * dir,
                                   const char * accession,
                                   const char * tbl_name,
                                   size_t cur_cache,
                                   uint64_t * res )
{
    cmn_params cp = { dir, accession, 0, 0, cur_cache };
    struct fastq_sra_iter * iter;
    rc_t rc = make_fastq_sra_iter( &cp, tbl_name, &iter, false, false ); /* fastq_iter.c */
    if ( rc == 0 )
    {
        *res = get_row_count_of_fastq_sra_iter( iter );
        destroy_fastq_sra_iter( iter );
    }
    return rc;
}

rc_t execute_tbl_join( KDirectory * dir,
                    const char * accession,
                    const char * tbl_name,
                    const tmp_id * tmp_id,
                    struct temp_registry * registry,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    bool show_progress,
                    bool split_file,
                    format_t fmt,
                    bool rowid_as_name )
{
    rc_t rc = 0;
    
    if ( show_progress )
        rc = KOutMsg( "join   :" );

    if ( rc == 0 )
    {
        uint64_t row_count = 0;
        rc = extract_sra_row_count( dir, accession, tbl_name, cur_cache, &row_count );

        if ( rc == 0 && row_count > 0 )
        {
            Vector threads;
            int64_t row = 1;
            uint64_t thread_id;
            uint64_t rows_per_thread = ( row_count / num_threads ) + 1;
            KThread * progress_thread = NULL;
            atomic_t * join_progress = NULL;
            multi_progress progress;

            VectorInit( &threads, 0, num_threads );
            
            if ( show_progress )
            {
                init_progress_data( &progress, row_count ); /* helper.c */
                join_progress = &( progress . progress_rows );
                rc = start_multi_progress( &progress_thread, &progress ); /* helper.c */
            }
            
            for ( thread_id = 0; rc == 0 && thread_id < num_threads; ++thread_id )
            {
                join_thread_data * jtd = calloc( 1, sizeof * jtd );
                if ( jtd != NULL )
                {
                    KThread * thread;
                    
                    jtd -> dir              = dir;
                    jtd -> accession        = accession;
                    jtd -> tbl_name         = tbl_name;
                    jtd -> first_row        = row;
                    jtd -> row_count        = rows_per_thread;
                    jtd -> cur_cache        = cur_cache;
                    jtd -> buf_size         = buf_size;
                    jtd -> join_progress    = join_progress;
                    jtd -> registry         = registry;
                    jtd -> split_file       = split_file;
                    jtd -> fmt              = fmt;
                    jtd -> rowid_as_name    = rowid_as_name;
                    
                    rc = make_joined_filename( jtd -> part_file, sizeof jtd -> part_file,
                                accession, tmp_id, thread_id ); /* helper.c */

                    if ( rc == 0 )
                    {
                        rc = KThreadMake( &thread, cmn_thread_func, jtd );
                        if ( rc != 0 )
                            ErrMsg( "KThreadMake( fastq/special #%d ) -> %R", thread_id, rc );
                        else
                        {
                            rc = VectorAppend( &threads, NULL, thread );
                            if ( rc != 0 )
                                ErrMsg( "VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                        }
                        row += rows_per_thread;
                    }
                }
            }
            join_and_release_threads( &threads ); /* helper.c */
            
            if ( show_progress )
                join_multi_progress( progress_thread, &progress ); /* helper.c */
        }
    }
    return rc;
}
