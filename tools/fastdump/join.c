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
#include "file_printer.h"
#include "special_iter.h"
#include "fastq_iter.h"
#include "helper.h"

#include <klib/out.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <kproc/thread.h>

#include <stdio.h>

typedef struct join
{
    struct lookup_reader * lookup;  /* lookup_reader.h */
    struct file_printer * printer;  /* file_printer.h */
    SBuffer B1, B2;                 /* helper.h */
} join;


static void release_join_ctx( join * j )
{
    if ( j != NULL )
    {
        release_lookup_reader( j->lookup );     /* lookup_reader.c */
        destroy_file_printer( j->printer );     /* file_printer.c */
        release_SBuffer( &j->B1 );              /* helper.c */
        release_SBuffer( &j->B2 );              /* helper.c */
    }
}


static rc_t init_join( const join_params * jp, struct join * j, struct index_reader * index )
{
    rc_t rc;
    
    j->lookup = NULL;
    j->printer = NULL;
    j->B1.S.addr = NULL;
    j->B2.S.addr = NULL;
    
    rc = make_lookup_reader( jp->dir, index, &j->lookup, jp->buf_size, "%s", jp->lookup_filename ); /* lookup_reader.c */
    if ( rc == 0 )
    {
        if ( jp->output_filename != NULL )
            rc = make_file_printer( jp->dir, &j->printer, jp->buf_size, 4096 * 4, "%s", jp->output_filename ); /* file_printer.c */
        if ( rc != 0 )
            ErrMsg( "init_join().make_file_printer() -> %R", rc );
    }
    else
        ErrMsg( "init_join().make_lookup_reader() -> %R", rc );
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
    if ( rc == 0 && jp -> first > 1 )
    {
        uint64_t key_to_find = jp -> first << 1;
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


static void copy_join_params( join_params * dst, const join_params * src )
{
    dst->dir                = src->dir;
    dst->accession          = src->accession;
    dst->lookup_filename    = src->lookup_filename;
    dst->index_filename     = src->index_filename;
    dst->output_filename    = src->output_filename;
    dst->temp_path          = src->temp_path;
    dst->join_progress      = src->join_progress;
    dst->buf_size           = src->buf_size;
    dst->cur_cache          = src->cur_cache;
    dst->num_threads        = src->num_threads;
    dst->first              = src->first;
    dst->count              = src->count;
    dst->show_progress      = src->show_progress;
    dst->fmt                = src->fmt;
}

static void init_cmn_params( const join_params * jp, cmn_params * cmn )
{
    cmn->dir            = jp->dir;
    cmn->acc            = jp->accession;
    cmn->first          = jp->first;
    cmn->count          = jp->count;
    cmn->cursor_cache   = jp->cur_cache;
    cmn->show_progress  = jp->show_progress;
}


static rc_t print_special_1_read( special_rec * rec, join * j )
{
    rc_t rc = 0;
    int64_t row_id = rec->row_id;
    
    if ( rec->prim_alig_id[ 0 ] == 0 )
    {
        /* read is unaligned, print what is in row->cmp_read ( !!! no lookup !!! ) */
        rc = file_print( j->printer, "%ld\t%S\t%S\n", row_id, &rec->cmp_read, &rec->spot_group );
    }
    else
    {
        /* read is aligned ( 1 lookup ) */
        rc = lookup_bases( j->lookup, row_id, 1, &j->B1 );
        if ( rc == 0 )
            rc = file_print( j->printer, "%ld\t%S\t%S\n", row_id, &j->B1.S, &rec->spot_group );
    }
    return rc;
}


static rc_t print_special_2_reads( special_rec * rec, join * j )
{
    rc_t rc = 0;
    int64_t row_id = rec->row_id;
    
    if ( rec->prim_alig_id[ 0 ] == 0 )
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* both unaligned, print what is in row->cmp_read ( !!! no lookup !!! )*/
            rc = file_print( j->printer, "%ld\t%S\t%S\n", row_id, &rec->cmp_read, &rec->spot_group );
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            rc = lookup_bases( j->lookup, row_id, 2, &j->B2 );
            if ( rc == 0 )
                rc = file_print( j->printer, "%ld\t%S%S\t%S\n", row_id, &rec->cmp_read, &j->B2.S, &rec->spot_group );
        }
    }
    else
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( j->lookup, row_id, 1, &j->B1 );
            if ( rc == 0 )
                rc = file_print( j->printer, "%ld\t%S%S\t%S\n", row_id, &j->B1.S, &rec->cmp_read, &rec->spot_group );
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( j->lookup, row_id, 1, &j->B1 );
            if ( rc == 0 )
                rc = lookup_bases( j->lookup, row_id, 2, &j->B2 );
            if ( rc == 0 )
                rc = file_print( j->printer, "%ld\t%S%S\t%S\n", row_id, &j->B1.S, &j->B2.S, &rec->spot_group );
        }
    }
    return rc;
}


static rc_t print_fastq_1_read( fastq_rec * rec, join * j, const char * acc )
{
    rc_t rc = 0;
    int64_t row_id = rec->row_id;
    
    if ( rec->prim_alig_id[ 0 ] == 0 )
    {
        /* read is unaligned, print what is in row->cmp_read (no lookup)*/
        const char * fmt = "@%s.%ld %ld length=%d\n%S\n+%s.%ld %ld length=%d\n%S\n";
        rc = file_print( j->printer, fmt,
            acc, row_id, row_id, rec->cmp_read.len, &rec->cmp_read,
            acc, row_id, row_id, rec->quality.len, &rec->quality );
    }
    else
    {
        /* read is aligned, ( 1 lookup ) */    
        rc = lookup_bases( j->lookup, row_id, 1, &j->B1 );
        if ( rc == 0 )
        {
            const char * fmt = "@%s.%ld %ld length=%d\n%S\n+%s.%ld %ld length=%d\n%S\n";
            rc = file_print( j->printer, fmt,
                acc, row_id, row_id, j->B1.S.len, &j->B1.S,
                acc, row_id, row_id, rec->quality.len, &rec->quality );
        }
    }
    return rc;
}

/* FASTQ NOT-SPLITTED UNALIGNED UNALIGNED*/
static const char * fmt_fastq_ns_uu = "@%s.%ld %ld length=%d\n%S\n+%s.%ld %ld length=%d\n%S\n";

/* FASTQ NOT-SPLITTED UNALIGNED/ALIGNED*/
static const char * fmt_fastq_ns = "@%s.%ld %ld length=%d\n%S%S\n+%s.%ld %ld length=%d\n%S\n";

static rc_t print_fastq_2_reads( fastq_rec * rec, join * j, const char * acc )
{
    rc_t rc = 0;
    int64_t row_id = rec->row_id;
    
    if ( rec->prim_alig_id[ 0 ] == 0 )
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* both unaligned, print what is in row->cmp_read (no lookup)*/
            rc = file_print( j->printer, fmt_fastq_ns_uu,
                acc, row_id, row_id, rec->cmp_read.len, &rec->cmp_read,
                acc, row_id, row_id, rec->quality.len, &rec->quality );
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            rc = lookup_bases( j->lookup, row_id, 2, &j->B2 );
            if ( rc == 0 )
                rc = file_print( j->printer, fmt_fastq_ns,
                    acc, row_id, row_id, rec->cmp_read.len + j->B2.S.len, &rec->cmp_read, &j->B2.S,
                    acc, row_id, row_id, rec->quality.len, &rec->quality );
        }
    }
    else
    {
        if ( rec->prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( j->lookup, row_id, 1, &j->B1 );
            if ( rc == 0 )
                rc = file_print( j->printer, fmt_fastq_ns,
                    acc, row_id, row_id, rec->cmp_read.len + j->B1.S.len, &j->B1.S, &rec->cmp_read,
                    acc, row_id, row_id, rec->quality.len, &rec->quality );
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( j->lookup, row_id, 1, &j->B1 );
            if ( rc == 0 )
                rc = lookup_bases( j->lookup, row_id, 2, &j->B2 );
            if ( rc == 0 )
                rc = file_print( j->printer, fmt_fastq_ns,
                    acc, row_id, row_id, j->B1.S.len + j->B2.S.len, &j->B1.S, &j->B2.S,
                    acc, row_id, row_id, rec->quality.len, &rec->quality );
        }
    }
    return rc;
}

/* FASTQ SPLITTED*/
static const char * fmt_fastq_s = "@%s.%ld %ld length=%d\n%S\n+%s.%ld %ld length=%d\n%S\n";

static rc_t print_fastq_2_reads_splitted( fastq_rec * rec, join * j, const char * acc )
{
    rc_t rc = 0;
    int64_t row_id = rec->row_id;
    String Q1, Q2;

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

            rc = file_print( j -> printer, fmt_fastq_s,
                acc, row_id, row_id, READ . len, &READ,
                acc, row_id, row_id, Q1 . len, &Q1 );
            if ( rc == 0 )
            {
                READ . addr = &rec -> cmp_read . addr[ rec -> read_len[ 0 ] ];
                READ . len = READ . size = rec -> read_len[ 1 ];
                rc = file_print( j -> printer, fmt_fastq_s,
                    acc, row_id, row_id, READ . len, &READ,
                    acc, row_id, row_id, Q2 . len, &Q2 );
            }
        }
        else
        {
            /* A0 is unaligned / A1 is aligned (lookup) */
            rc = file_print( j->printer, fmt_fastq_s,
                acc, row_id, row_id, rec -> cmp_read . len, &rec -> cmp_read,
                acc, row_id, row_id, Q1 . len, &Q1 );
            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2 );
            if ( rc == 0 )
                rc = file_print( j -> printer, fmt_fastq_s,
                    acc, row_id, row_id, j -> B2 . S . len, &j -> B2 . S,
                    acc, row_id, row_id, Q2 . len, &Q2 );
        }
    }
    else
    {
        if ( rec -> prim_alig_id[ 1 ] == 0 )
        {
            /* A0 is aligned (lookup) / A1 is unaligned */
            rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1 );
            if ( rc == 0 )
                rc = file_print( j -> printer, fmt_fastq_s,
                    acc, row_id, row_id, j -> B1 . S . len, &j -> B1 . S,
                    acc, row_id, row_id, Q1 . len, &Q1 );
            if ( rc == 0 )
                rc = file_print( j -> printer, fmt_fastq_s,
                    acc, row_id, row_id, rec -> cmp_read . len, &rec -> cmp_read,
                    acc, row_id, row_id, Q2 . len, &Q2 );
        }
        else
        {
            /* A0 and A1 are aligned (2 lookups)*/
            rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1 );
            if ( rc == 0 )
                rc = file_print( j -> printer, fmt_fastq_s,
                    acc, row_id, row_id, j -> B1 . S . len, &j -> B1 . S,
                    acc, row_id, row_id, Q1 . len, &Q1 );
            if ( rc == 0 )
                rc = lookup_bases( j -> lookup, row_id, 2, & j -> B2 );
            if ( rc == 0 )
                rc = file_print( j -> printer, fmt_fastq_s,
                    acc, row_id, row_id, j -> B2 . S . len, &j -> B2 . S,
                    acc, row_id, row_id, Q2 . len, &Q2 );
        }
    }
    return rc;
}


static rc_t extract_row_count_cmn( const join_params * jp, uint64_t * row_count )
{
    rc_t rc = 0;
    cmn_params cmn;
    
    init_cmn_params( jp, &cmn ); /* above */
    switch( jp->fmt )
    {
        case ft_special : {
                                struct special_iter * iter;
                                rc = make_special_iter( &cmn, &iter ); /* special_iter.c */
                                if ( rc == 0 )
                                {
                                    *row_count = get_row_count_of_special_iter( iter );
                                    destroy_special_iter( iter );
                                }
                           } break;
                           
        case ft_fastq   : {
                                struct fastq_iter * iter;
                                rc = make_fastq_iter( &cmn, &iter, false ); /* fastq_iter.c */
                                if ( rc == 0 )
                                {
                                    *row_count = get_row_count_of_fastq_iter( iter );
                                    destroy_fastq_iter( iter );
                                }
                           } break;
                           
        case ft_fastq_split : {
                                struct fastq_iter * iter;
                                rc = make_fastq_iter( &cmn, &iter, true ); /* fastq_iter.c */
                                if ( rc == 0 )
                                {
                                    *row_count = get_row_count_of_fastq_iter( iter );
                                    destroy_fastq_iter( iter );
                                }
                           } break;

        default : break;
    }
    return rc;
}

rc_t CC Quitting();

static rc_t perform_special_join( const join_params * jp, struct index_reader * index )
{
    rc_t rc;
    struct special_iter * iter;
    cmn_params cmn;

    init_cmn_params( jp, &cmn ); /* above */
    rc = make_special_iter( &cmn, &iter );
    if ( rc == 0 )
    {
        join j;
        rc = init_join( jp, &j, index );
        if ( rc == 0 )
        {
            special_rec rec;
            while ( get_from_special_iter( iter, &rec, &rc ) && rc == 0 )
            {
                rc = Quitting();
                if ( rc == 0 )
                {
                    if ( rec.num_reads == 1 )
                        rc = print_special_1_read( &rec, &j );
                    else
                        rc = print_special_2_reads( &rec, &j );

                    if ( jp->join_progress != NULL )
                        atomic_inc( jp->join_progress );
                }
            }
            release_join_ctx( &j );
        }
        else
            ErrMsg( "init_join() -> %R", rc );
        destroy_special_iter( iter );
    }
    else
        ErrMsg( "make_special_iter() -> %R", rc );
    return rc;
}


static rc_t perform_fastq_join( const join_params * jp, struct index_reader * index )
{
    rc_t rc;
    struct fastq_iter * iter;
    cmn_params cmn;

    init_cmn_params( jp, &cmn ); /* above */
    rc = make_fastq_iter( &cmn, &iter, false /* not spliting  */ );
    if ( rc == 0 )
    {
        join j;
        
        rc = init_join( jp, &j, index );
        if ( rc == 0 )
        {
            fastq_rec rec;
            uint64_t n = 0;
            
            while ( get_from_fastq_iter( iter, &rec, &rc ) && rc == 0 )
            {
                rc = Quitting();
                if ( rc == 0 )
                {
                    if ( rec.num_reads == 1 )
                        rc = print_fastq_1_read( &rec, &j, jp->accession );
                    else
                        rc = print_fastq_2_reads( &rec, &j, jp->accession );

                    if ( jp->join_progress != NULL )
                        atomic_inc( jp->join_progress );
                    n++;
                }
            }
            release_join_ctx( &j );
        }
        else
            ErrMsg( "init_join() -> %R", rc );
        destroy_fastq_iter( iter );
    }
    else
        ErrMsg( "make_fastq_iter() -> %R", rc );
    return rc;
}

static rc_t perform_fastq_join_splitted( const join_params * jp, struct index_reader * index )
{
    rc_t rc;
    struct fastq_iter * iter;
    cmn_params cmn;

    init_cmn_params( jp, &cmn ); /* above */
    rc = make_fastq_iter( &cmn, &iter, true /* spliting  */ );
    if ( rc == 0 )
    {
        join j;
        
        rc = init_join( jp, &j, index );
        if ( rc == 0 )
        {
            fastq_rec rec;
            uint64_t n = 0;
            
            while ( get_from_fastq_iter( iter, &rec, &rc ) && rc == 0 )
            {
                rc = Quitting();
                if ( rc == 0 )
                {
                    if ( rec.num_reads == 1 )
                        rc = print_fastq_1_read( &rec, &j, jp->accession );
                    else
                        rc = print_fastq_2_reads_splitted( &rec, &j, jp->accession );

                    if ( jp->join_progress != NULL )
                        atomic_inc( jp->join_progress );
                    n++;
                }
            }
            release_join_ctx( &j );
        }
        else
            ErrMsg( "init_join() -> %R", rc );
        destroy_fastq_iter( iter );
    }
    else
        ErrMsg( "make_fastq_iter() -> %R", rc );
    return rc;
}


/* ------------------------------------------------------------------------------------------ */

typedef struct join_thread_data
{
    const join_params * jp;
    int64_t first;
    uint64_t count;
    uint32_t idx;
} join_thread_data;


static const char * leaf_of( const char * src )
{
    const char * last_slash = string_rchr( src, string_size ( src ), '/' );
    if ( last_slash != NULL )
        return last_slash + 1;
    return src;
}

static rc_t make_part_filename( const join_params * jp, char * buffer, size_t bufsize, uint32_t id )
{
    rc_t rc;
    size_t num_writ;
    if ( jp->temp_path != NULL )
    {
        uint32_t l = string_measure( jp->temp_path, NULL );
        if ( l == 0 )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            ErrMsg( "make_part_filename.string_measure() = 0 -> %R", rc );
        }
        else
        {
            const char * output_file_leaf = leaf_of( jp->output_filename ); /* above */
            
            if ( jp->temp_path[ l-1 ] == '/' )
                rc = string_printf( buffer, bufsize, &num_writ, "%s%s.%d",
                        jp->temp_path, output_file_leaf, id );
            else
                rc = string_printf( buffer, bufsize, &num_writ, "%s/%s.%d",
                        jp->temp_path, output_file_leaf, id );
        }
    }
    else
        rc = string_printf( buffer, bufsize, &num_writ, "%s.%d", jp->output_filename, id );
        
    if ( rc != 0 )
        ErrMsg( "make_part_filename.string_printf() -> %R", rc );
    return rc;
}


static rc_t concat_part_files( const join_params * jp, uint32_t count )
{
    struct VNamelist * files;
    rc_t rc = VNamelistMake( &files, count );
    if ( rc == 0 )
    {
        uint32_t idx;
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            char part_file[ 4096 ];
            rc = make_part_filename( jp, part_file, sizeof part_file, idx ); /* above */
            if ( rc == 0 )
                rc = VNamelistAppend( files, part_file );
        }
        if ( rc == 0 )
        {
            if ( jp -> print_to_stdout )
                rc = print_files( jp -> dir, files, jp -> buf_size );
            else
                rc = concat_files( jp -> dir, files, jp -> buf_size, jp -> output_filename,
                                   jp -> show_progress, jp -> gzip ); /* helper.c */
        }
        if ( rc == 0 )
            rc = delete_files( jp->dir, files );
        VNamelistRelease( files );
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t CC cmn_thread_func( const KThread *self, void *data )
{
    rc_t rc = 0;
    join_thread_data * jtd = data;
    const join_params * jp = jtd->jp;
    struct index_reader * index = NULL;
    
    if ( jp->index_filename != NULL )
    {
        if ( file_exists( jp->dir, "%s", jp->index_filename ) )
            rc = make_index_reader( jp->dir, &index, jp->buf_size, "%s", jp->index_filename ); /* index.c */
    }

    if ( rc == 0 && index != NULL )
    {
        char part_file[ 4096 ];
        rc = make_part_filename( jp, part_file, sizeof part_file, jtd->idx ); /* above */
        if ( rc == 0 )
        {
            join_params cjp;

            copy_join_params( &cjp, jp );
            cjp.num_threads = 0;
            cjp.first = jtd->first;
            cjp.count = jtd->count;
            cjp.output_filename = part_file;
            cjp.show_progress = false;
            
            switch( jp->fmt )
            {
                case ft_special     : rc = perform_special_join( &cjp, index ); break; /* above */
                case ft_fastq       : rc = perform_fastq_join( &cjp, index ); break; /* above */
                case ft_fastq_split : rc = perform_fastq_join_splitted( &cjp, index ); break; /* above */
                default : break;
                
            }
        }
        release_index_reader( index ); /* index.c */    
    }
    
    free( ( void * ) data );
    return rc;
}


rc_t execute_join( const join_params * jp )
{
    rc_t rc = 0;
    
    if ( jp->show_progress )
        KOutMsg( "join   :" );

    if ( jp->num_threads < 2 )
    {
        /* on the main thread */
        switch( jp->fmt )
        {
            case ft_special     : rc = perform_special_join( jp, NULL ); break; /* above */
            case ft_fastq       : rc = perform_fastq_join( jp, NULL ); break; /* above */
            case ft_fastq_split : rc = perform_fastq_join_splitted( jp, NULL ); break; /* above */
            default : break;
        }
    }
    else
    {
        uint64_t row_count = 0;
        rc = extract_row_count_cmn( jp, &row_count ); /* above */
        if ( rc == 0 && row_count > 0 )
        {
            Vector threads;
            int64_t first = 1;
            uint64_t i, per_thread = ( row_count / jp->num_threads ) + 1;
            KThread * progress_thread = NULL;
            multi_progress progress;

            init_progress_data( &progress, row_count ); /* helper.c */
            VectorInit( &threads, 0, jp->num_threads );
            
            if ( jp->show_progress )
            {
                join_params * nc_jp = ( join_params * )jp;
                nc_jp->join_progress = &progress.progress_rows;
                rc = start_multi_progress( &progress_thread, &progress ); /* helper.c */
            }
            for ( i = 0; rc == 0 && i < jp->num_threads; ++i )
            {
                join_thread_data * jtd = calloc( 1, sizeof * jtd );
                if ( jtd != NULL )
                {
                    KThread * thread;
                    
                    jtd->jp = jp;
                    jtd->first = first;
                    jtd->count = per_thread;
                    jtd->idx = i;

                    rc = KThreadMake( &thread, cmn_thread_func, jtd );
                    if ( rc != 0 )
                        ErrMsg( "KThreadMake( fastq/special #%d ) -> %R", i, rc );
                    else
                    {
                        rc = VectorAppend( &threads, NULL, thread );
                        if ( rc != 0 )
                            ErrMsg( "VectorAppend( sort-thread #%d ) -> %R", i, rc );
                    }
                    first += per_thread;
                }
            }
            
            join_and_release_threads( &threads ); /* helper.c */
            join_multi_progress( progress_thread, &progress ); /* helper.c */
            
            if ( jp->show_progress )
                KOutMsg( "concat :" );
            
            rc = concat_part_files( jp, jp->num_threads ); /* above */
        }
    }
    return rc;
}
