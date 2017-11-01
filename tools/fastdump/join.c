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
#include "lookup_reader.h"
#include "index.h"
#include "special_iter.h"
#include "fastq_iter.h"
#include "cleanup_task.h"
#include "join_results.h"
#include "progress_thread.h"

#include <klib/out.h>
#include <kproc/thread.h>

typedef struct join
{
    const char * accession;
    struct lookup_reader * lookup;  /* lookup_reader.h */
    struct index_reader * index;    /* index.h */
    struct join_results * results;  /* join_results.h */
    SBuffer B1, B2;                 /* helper.h */
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
                       struct join * j )
{
    rc_t rc;
    
    j -> accession = cp -> accession;
    j -> lookup = NULL;
    j -> results = results;
    j -> B1 . S . addr = NULL;
    j -> B2 . S . addr = NULL;
    
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

    /* the rc-code of seek_lookup_reader is not checked, because if the row-id to be seeked to is in
       the range of the fully unaligned data - seek will fail, because the are no alignments = lookup-records
       in this area !*/
    if ( rc == 0 && cp -> first_row > 1 )
    {
        uint64_t key_to_find = cp -> first_row << 1;
        uint64_t key_found = 0;
        rc_t rc1 = seek_lookup_reader( j -> lookup, key_to_find, &key_found, true );  /* lookup_reader.c */
        if ( GetRCState( rc1 ) != rcTooBig && GetRCState( rc1 ) != rcNotFound )
            rc = rc1;
        if ( rc != 0 )
            ErrMsg( "init_join().seek_lookup_reader( %lu ) -> %R", key_to_find, rc );
    }
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
                         row_id, &( rec -> cmp_read ), &( rec -> spot_group ) ); /* file_printer.c */
    }
    else
    {
        /* read is aligned ( 1 lookup ) */
        rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ) );
        if ( rc == 0 )
            rc = join_results_print( j -> results, 0, "%ld\t%S\t%S\n",
                             row_id, &( j -> B1.S ), &( rec -> spot_group ) ); /* file_printer.c */
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
                             row_id, &( rec -> cmp_read ), &( rec -> spot_group ) ); /* file_printer.c */
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ) );
            if ( rc == 0 )
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &( rec -> cmp_read ), &( j -> B2 . S ), &( rec -> spot_group ) ); /* file_printer.c */
        }
    }
    else
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ) );
            if ( rc == 0 )
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &j->B1.S, &rec->cmp_read, &rec->spot_group ); /* file_printer.c */
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ) );
            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ) );
            if ( rc == 0 )
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &( j -> B1 . S ), &( j -> B2 . S ), &( rec -> spot_group ) ); /* file_printer.c */
        }
    }
    return rc;
}


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

static rc_t print_fastq_1_read( const fastq_csra_rec * rec, join * j, bool rowid_as_name )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    
    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        /* read is unaligned, print what is in rec -> cmp_read ( no lookup ) */
        if ( rowid_as_name )
            rc = print_fq2_read_1( j -> results, j -> accession, row_id, 0, &( rec -> cmp_read ), &( rec -> quality ) );
        else
            rc = print_fq2_read_2( j -> results, j -> accession, row_id, 0, &( rec -> name ), &( rec -> cmp_read ), &( rec -> quality ) );        
    }
    else
    {
        /* read is aligned, ( 1 lookup ) */    
        rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ) );
        if ( rc == 0 )
        {
            if ( rowid_as_name )
                rc = print_fq2_read_1( j -> results, j -> accession, row_id, 0, &( j -> B1 . S ), &( rec -> quality ) );
            else
                rc = print_fq2_read_2( j -> results, j -> accession, row_id, 0, &( rec -> name ), &( j -> B1 . S ), &( rec -> quality ) );            
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static const char * fmt_fq2s_1 = "@%s.%ld %ld length=%d\n%S%S\n+%s.%ld %ld length=%d\n%S\n";
static rc_t print_fq2_reads_1( struct join_results * results,
                              const char * accession,
                              int64_t row_id,
                              uint32_t read_id,
                              const String * read1,
                              const String * read2,
                              const String * quality )
{
    return join_results_print( results, read_id, fmt_fq2s_1,
                    accession, row_id, row_id, read1 -> len + read2 -> len, read1, read2,
                    accession, row_id, row_id, quality -> len, quality ); /* join_results.c */
}

static const char * fmt_fq2s_2    = "@%s.%ld %S length=%d\n%S%S\n+%s.%ld %S length=%d\n%S\n";
static rc_t print_fq2_reads_2( struct join_results * results,
                              const char * accession,
                              int64_t row_id,
                              uint32_t read_id,
                              const String * name,
                              const String * read1,
                              const String * read2,
                              const String * quality )
{
    return join_results_print( results, read_id, fmt_fq2s_2,
                    accession, row_id, name, read1 -> len + read2 -> len, read1, read2,
                    accession, row_id, name, quality -> len, quality ); /* join_results.c */
}

static rc_t print_fastq_2_reads( const fastq_csra_rec * rec, join * j, bool rowid_as_name )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    
    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* both unaligned, print what is in row->cmp_read (no lookup)*/
            if ( rowid_as_name )
                rc = print_fq2_read_1( j -> results, j -> accession, row_id, 0,
                                &( rec -> cmp_read ), &( rec -> quality ) );
            else
                rc = print_fq2_read_2( j -> results, j -> accession, row_id, 0,
                                &( rec -> name ), &( rec -> cmp_read ), &( rec -> quality ) );
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ) );
            if ( rc == 0 )
            {
                if ( rowid_as_name )
                    rc = print_fq2_reads_1( j -> results, j -> accession, row_id, 0,
                                &( rec -> cmp_read ), &( j -> B2 . S ), &( rec -> quality ) );
                else
                    rc = print_fq2_reads_2( j -> results, j -> accession, row_id, 0,
                                &( rec -> name ), &( rec -> cmp_read ), &( j -> B2 . S ), &( rec -> quality ) );
            }
        }
    }
    else
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ) );
            if ( rc == 0 )
            {
                if ( rowid_as_name )
                    rc = print_fq2_reads_1( j -> results, j -> accession, row_id, 0,
                                &( j -> B1 . S ), &( rec -> cmp_read ), &( rec -> quality ) );
                else
                    rc = print_fq2_reads_2( j -> results, j -> accession, row_id, 0,
                                &( rec -> name ), &( j -> B1 . S ), &( rec -> cmp_read ), &( rec -> quality ) );
            }
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ) );
            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ) );
            if ( rc == 0 )
            {
                if ( rowid_as_name )
                    rc = print_fq2_reads_1( j -> results, j -> accession, row_id, 0,
                                &( j -> B1 . S ), &( j -> B2 . S ), &( rec -> quality ) );
                else
                    rc = print_fq2_reads_2( j -> results, j -> accession, row_id, 0,
                                &( rec -> name ), &( j -> B1 . S ), &( j -> B2 . S ), &( rec -> quality ) );
            }
        }
    }
    return rc;
}

/* FASTQ SPLIT */
static rc_t print_fastq_2_reads_splitted( const fastq_csra_rec * rec, join * j, bool split_file, bool rowid_as_name )
{
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    String Q1, Q2;
    uint32_t read_id = 0;
    
    Q1. addr = rec -> quality . addr;
    Q1 . len = Q1 . size = rec -> read_len[ 0 ];
    Q2 . addr = &rec -> quality . addr[ rec -> read_len[ 0 ] ];
    Q2 . len = Q2 . size = rec -> read_len[ 1 ];

    if ( rec -> prim_alig_id[ 0 ] == 0 )
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            String READ;
            /* both unaligned, print what is in row->cmp_read (no lookup) */
            READ . addr = rec -> cmp_read . addr;
            READ . len = READ . size = rec -> read_len[ 0 ];

            if ( rowid_as_name )
                rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &READ, &Q1 );
            else
                rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &READ, &Q1 );

            if ( rc == 0 )
            {
                READ . addr = &rec -> cmp_read . addr[ rec -> read_len[ 0 ] ];
                READ . len = READ . size = rec -> read_len[ 1 ];
                if ( split_file )
                    read_id++;

                if ( rowid_as_name )
                    rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &READ, &Q2 );
                else
                    rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &READ, &Q2 );
            }
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            if ( rowid_as_name )
                rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &rec -> cmp_read, &Q1 );
            else
                rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &rec -> cmp_read, &Q1 );

            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2 );
            if ( rc == 0 )
            {
                if ( split_file )
                    read_id++;
            
                if ( rowid_as_name )
                    rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &j -> B2 . S, &Q2 );
                else
                    rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &j -> B2 . S, &Q2 );
            }
        }
    }
    else
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1 );
            if ( rc == 0 )
            {
                if ( rowid_as_name )
                    rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &j -> B1 . S, &Q1 );
                else
                    rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &j -> B1 . S, &Q1 );
            }
            if ( rc == 0 )
            {
                if ( split_file )
                    read_id++;

                if ( rowid_as_name )
                    rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &rec -> cmp_read, &Q2 );
                else
                    rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &rec -> cmp_read, &Q2 );
            }
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1 );
            if ( rc == 0 )
            {
                if ( rowid_as_name )
                    rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &j -> B1 . S, &Q1 );
                else
                    rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &j -> B1 . S, &Q1 );
            }
            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, & j -> B2 );
            if ( rc == 0 )
            {
                if ( split_file )
                    read_id++;

                if ( rowid_as_name )
                    rc = print_fq2_read_1( j -> results, j -> accession, row_id, read_id, &j -> B2 . S, &Q2 );
                else
                    rc = print_fq2_read_2( j -> results, j -> accession, row_id, read_id, &rec -> name, &j -> B2 . S, &Q2 );
            }
        }
    }
    return rc;
}


static rc_t extract_csra_row_count( KDirectory * dir,
                                    const char * accession,
                                    size_t cur_cache,
                                    uint64_t * res )
{
    cmn_params cp = { dir, accession, 0, 0, cur_cache };
    struct fastq_csra_iter * iter;
    rc_t rc = make_fastq_csra_iter( &cp, &iter, false, false ); /* fastq_iter.c */
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
    rc_t rc = make_special_iter( cp, &iter );
    if ( rc == 0 )
    {
        special_rec rec;
        while ( get_from_special_iter( iter, &rec, &rc ) && rc == 0 )
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                if ( rec . num_reads == 1 )
                    rc = print_special_1_read( &rec, j );
                else
                    rc = print_special_2_reads( &rec, j );

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_special_iter( iter );
    }
    else
        ErrMsg( "make_special_iter() -> %R", rc );
    return rc;
}


static rc_t perform_fastq_join( cmn_params * cp,
                                join * j,
                                struct bg_progress * progress,
                                bool rowid_as_name )
{
    struct fastq_csra_iter * iter;
    bool with_read_len = false;
    bool with_name = !rowid_as_name;
    rc_t rc = make_fastq_csra_iter( cp, &iter, with_read_len, with_name ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_join().make_fastq_csra_iter() -> %R", rc );
    else
    {
        fastq_csra_rec rec;
        while ( get_from_fastq_csra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                if ( rec . num_reads == 1 )
                    rc = print_fastq_1_read( &rec, j, rowid_as_name );
                else
                    rc = print_fastq_2_reads( &rec, j, rowid_as_name );

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );
    }
    return rc;
}

static rc_t perform_fastq_join_split( cmn_params * cp,
                                      join * j,
                                      struct bg_progress * progress,
                                      bool split_file,
                                      bool rowid_as_name )
{
    struct fastq_csra_iter * iter;
    bool with_read_len = true;
    bool with_name = !rowid_as_name;
    rc_t rc = make_fastq_csra_iter( cp, &iter, with_read_len, with_name ); /* fastq-iter.c */
    if ( rc != 0 )
        ErrMsg( "perform_fastq_join_split().make_fastq_csra_iter() -> %R", rc );
    else
    {
        fastq_csra_rec rec;
        while ( get_from_fastq_csra_iter( iter, &rec, &rc ) && rc == 0 ) /* fastq-iter.c */
        {
            rc = Quitting();
            if ( rc == 0 )
            {
                if ( rec . num_reads == 1 )
                    rc = print_fastq_1_read( &rec, j, rowid_as_name );
                else
                    rc = print_fastq_2_reads_splitted( &rec, j, split_file, rowid_as_name );

                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );
    }
    return rc;
}


/* ------------------------------------------------------------------------------------------ */

typedef struct join_thread_data
{
    char part_file[ 4096 ];
        
    KDirectory * dir;
    const char * accession;
    const char * lookup_filename;
    const char * index_filename;
    struct bg_progress * progress;
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
        join j;
        cmn_params cp = { jtd -> dir, jtd -> accession, jtd -> first_row, jtd -> row_count, jtd -> cur_cache };

        rc = init_join( &cp, results, jtd -> lookup_filename, jtd -> index_filename, jtd -> buf_size, &j ); /* above */
        if ( rc == 0 )
        {
            switch( jtd -> fmt )
            {
                case ft_special     : rc = perform_special_join( &cp,
                                                &j,
                                                jtd -> progress ); break; /* above */
                                                
                case ft_fastq       : rc = perform_fastq_join( &cp,
                                                &j,
                                                jtd -> progress,
                                                jtd -> rowid_as_name ); break; /* above */
                                                
                case ft_fastq_split : rc = perform_fastq_join_split( &cp,
                                                &j,
                                                jtd -> progress,
                                                jtd -> split_file,
                                                jtd -> rowid_as_name ); break; /* above */

                default : break;
            }
            release_join_ctx( &j );
        }
        destroy_join_results( results );
    }
    
    free( ( void * ) data );
    return rc;
}

rc_t execute_db_join( KDirectory * dir,
                    const char * accession,
                    const char * lookup_filename,
                    const char * index_filename,
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
        rc = extract_csra_row_count( dir, accession, cur_cache, &row_count );

        if ( rc == 0 && row_count > 0 )
        {
            Vector threads;
            int64_t row = 1;
            uint64_t thread_id;
            uint64_t rows_per_thread = ( row_count / num_threads ) + 1;
            struct bg_progress * progress = NULL;

            VectorInit( &threads, 0, num_threads );
            
            if ( show_progress )
                rc = bg_progress_make( &progress, row_count, 0, 0 ); /* progress_thread.c */
            
            for ( thread_id = 0; rc == 0 && thread_id < num_threads; ++thread_id )
            {
                join_thread_data * jtd = calloc( 1, sizeof * jtd );
                if ( jtd == NULL )
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                else
                {
                    KThread * thread;
                    
                    jtd -> dir              = dir;
                    jtd -> accession        = accession;
                    jtd -> lookup_filename  = lookup_filename;
                    jtd -> index_filename   = index_filename;
                    jtd -> first_row        = row;
                    jtd -> row_count        = rows_per_thread;
                    jtd -> cur_cache        = cur_cache;
                    jtd -> buf_size         = buf_size;
                    jtd -> progress         = progress;
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
            bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/
        }
    }
    return rc;
}
