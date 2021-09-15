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
#include "join_results.h"
#include "helper.h"
#include <klib/vector.h>
#include <klib/out.h>
#include <klib/printf.h>
#include <kfs/buffile.h>
#include <kproc/lock.h>

typedef struct join_printer_t {
    struct KFile * f;
    uint64_t file_pos;
} join_printer_t;

void set_file_printer_args( file_printer_args_t * self,
                            KDirectory * dir,
                            struct temp_registry_t * registry,
                            const char * output_base,
                            size_t buffer_size ) {
    self -> dir = dir;
    self -> registry = registry;
    self -> output_base = output_base;
    self -> buffer_size = buffer_size;
}

static void CC destroy_join_printer( void * item, void * data ) {
    if ( NULL != item ) {
        join_printer_t * p = item;
        if ( NULL != p -> f ) { release_file( p -> f, "join_results.c destroy_join_printer()" ); }
        free( item );
    }
}

static join_printer_t * make_join_printer_from_filename( KDirectory * dir, const char * filename, size_t buffer_size ) {
    join_printer_t * res = calloc( 1, sizeof * res );
    if ( NULL != res ) {
        struct KFile * f;
        rc_t rc = KDirectoryCreateFile( dir, &f, false, 0664, kcmInit, "%s", filename );
        if ( 0 != rc ) {
            ErrMsg( "make_join_printer_from_filename().KDirectoryCreateFile( '%s' ) -> %R", filename, rc );
        } else {
            if ( buffer_size > 0 ) {
                rc = wrap_file_in_buffer( &f, buffer_size, "join_results.c make_join_printer_from_filename()" ); /* helper.c */
                if ( 0 != rc ) { release_file( f, "join_results.c make_join_printer_from_filename()" ); } /* helper.c */
            }
            res -> f = f;
        }
    }
    return res;
}

static join_printer_t * make_join_printer( file_printer_args_t * file_args, uint32_t read_id ) {
    join_printer_t * res = NULL;
    char filename[ 4096 ];
    size_t num_writ;
    rc_t rc = string_printf( filename, sizeof filename, &num_writ, "%s.%u", file_args -> output_base, read_id );
    if ( 0 != rc ) {
        ErrMsg( "make_join_printer().string_printf() -> %R", rc );
    } else {
        res = make_join_printer_from_filename( file_args -> dir, filename, file_args -> buffer_size );
        if ( NULL != res ) {
            rc = register_temp_file( file_args -> registry, read_id, filename );
            if ( 0 != rc ) {
                destroy_join_printer( res, NULL );
                res = NULL;
            }
        }
    }
    return res;
}

/* ----------------------------------------------------------------------------------------------------- */

typedef rc_t ( * print_v1 )( struct join_results_t * self,
                             int64_t row_id,
                             uint32_t dst_id,
                             uint32_t read_id,
                             const String * name,
                             const String * read,
                             const String * quality );

typedef rc_t ( * print_v2 )( struct join_results_t * self,
                             int64_t row_id,
                             uint32_t dst_id,
                             uint32_t read_id,
                             const String * name,
                             const String * read_1,
                             const String * read_2,
                             const String * quality );

typedef struct join_results_t {
    file_printer_args_t file_printer_args;
    const char * accession_short;
    print_v1 v1_print_name_null;
    print_v1 v1_print_name_not_null;
    print_v2 v2_print_name_null;
    print_v2 v2_print_name_not_null;    
    SBuffer_t print_buffer;            /* we have only one print_buffer... */
    Vector printers;                 /* a vector of join-printers, used by regular join-results */
    bool print_frag_nr, print_name;
} join_results_t;

void destroy_join_results( join_results_t * self ) {
    if ( NULL != self ) {
        VectorWhack ( &self -> printers, destroy_join_printer, NULL );
        release_SBuffer( &self -> print_buffer );
        free( ( void * ) self );
    }
}

static const char * fmt_fastq_v1_no_name_no_frag_nr = "@%s.%ld length=%u\n%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v1_no_name_no_frag_nr = ">%s.%ld length=%u\n%S\n";
static rc_t print_v1_no_name_no_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_no_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_no_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_no_name_frag_nr = "@%s.%ld/%u length=%u\n%S\n+%s.%ld/%u length=%u\n%S\n";
static const char * fmt_fasta_v1_no_name_frag_nr = ">%s.%ld/%u length=%u\n%S\n";
static rc_t print_v1_no_name_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_no_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_no_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_syn_name_no_frag_nr = "@%s.%ld %ld length=%u\n%S\n+%s.%ld %ld length=%u\n%S\n";
static const char * fmt_fasta_v1_syn_name_no_frag_nr = ">%s.%ld %ld length=%u\n%S\n";
static rc_t print_v1_syn_name_no_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_syn_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                row_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_syn_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_syn_name_frag_nr = "@%s.%ld/%u %ld length=%u\n%S\n+%s.%ld/%u %ld length=%u\n%S\n";
static const char * fmt_fasta_v1_syn_name_frag_nr = ">%s.%ld/%u %ld length=%u\n%S\n";
static rc_t print_v1_syn_name_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_syn_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_syn_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_real_name_no_frag_nr  = "@%s.%ld %S length=%u\n%S\n+%s.%ld %S length=%u\n%S\n";
static const char * fmt_fastq_v1_empty_name_no_frag_nr = "@%s.%ld length=%u\n%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v1_real_name_no_frag_nr  = ">%s.%ld %S length=%u\n%S\n";
static const char * fmt_fasta_v1_empty_name_no_frag_nr = ">%s.%ld length=%u\n%S\n";
static rc_t print_v1_real_name_no_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality ) {
    if ( NULL != quality ) {
        if ( name -> len > 0 ) {
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v1_real_name_no_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id,
                                    name,
                                    read -> len, read,
                                    /* QUALITY... */
                                    self -> accession_short, row_id,
                                    name,
                                    quality -> len, quality );
        }
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v1_empty_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read -> len, read,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    }
    if ( name -> len > 0 ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v1_real_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                name,
                                read -> len, read );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v1_empty_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read -> len, read );
}

static const char * fmt_fastq_v1_real_name_frag_nr  = "@%s.%ld/%u %S length=%u\n%S\n+%s.%ld/%u %S length=%u\n%S\n";
static const char * fmt_fastq_v1_empty_name_frag_nr = "@%s.%ld/%u length=%u\n%S\n+%s.%ld/%u length=%u\n%S\n";
static const char * fmt_fasta_v1_real_name_frag_nr  = ">%s.%ld/%u %S length=%u\n%S\n";
static const char * fmt_fasta_v1_empty_name_frag_nr = ">%s.%ld/%u length=%u\n%S\n";
static rc_t print_v1_real_name_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read,
                                         const String * quality ) {
    if ( NULL != quality ) {
        if ( name -> len > 0 ) {
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v1_real_name_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    read -> len, read,
                                    /* QUALITY... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    quality -> len, quality );
        }
        return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v1_empty_name_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id, read_id,
                                    read -> len, read,
                                    /* QUALITY... */
                                    self -> accession_short, row_id, read_id,
                                    quality -> len, quality );
    }
    if ( name -> len > 0 ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v1_real_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                name,
                                read -> len, read );
    }
    return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v1_empty_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read -> len, read );
}

static const char * fmt_fastq_v2_no_name_no_frag_nr = "@%s.%ld length=%u\n%S%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v2_no_name_no_frag_nr = ">%s.%ld length=%u\n%S%S\n";
static rc_t print_v2_no_name_no_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_no_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_no_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_no_name_frag_nr    = "@%s.%ld/%u length=%u\n%S%S\n+%s.%ld/%u %length=%u\n%S\n";
static const char * fmt_fasta_v2_no_name_frag_nr    = ">%s.%ld/%u length=%u\n%S%S\n";
static rc_t print_v2_no_name_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_no_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_no_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_syn_name_no_frag_nr = "@%s.%ld %ld length=%u\n%S%S\n+%s.%ld %ld length=%u\n%S\n";
static const char * fmt_fasta_v2_syn_name_no_frag_nr = ">%s.%ld %ld length=%u\n%S%S\n";
static rc_t print_v2_syn_name_no_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_syn_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                row_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_syn_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_syn_name_frag_nr = "@%s.%ld/%u %ld length=%u\n%S%S\n+%s.%ld/%u %ld length=%u\n%S\n";
static const char * fmt_fasta_v2_syn_name_frag_nr = ">%s.%ld/%u %ld length=%u\n%S%S\n";
static rc_t print_v2_syn_name_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality ) {
    if ( NULL != quality ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_syn_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                row_id,
                                quality -> len, quality );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_syn_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_real_name_no_frag_nr  = "@%s.%ld %S length=%u\n%S%S\n+%s.%ld %S length=%u\n%S\n";
static const char * fmt_fastq_v2_empty_name_no_frag_nr = "@%s.%ld length=%u\n%S%S\n+%s.%ld length=%u\n%S\n";
static const char * fmt_fasta_v2_real_name_no_frag_nr  = ">%s.%ld %S length=%u\n%S%S\n";
static const char * fmt_fasta_v2_empty_name_no_frag_nr = ">%s.%ld length=%u\n%S%S\n";
static rc_t print_v2_real_name_no_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality ) {
    if ( NULL != quality ) {
        if ( name -> len > 0 ) {
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v2_real_name_no_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id,
                                    name,
                                    read1 -> len + read2 -> len, read1, read2,
                                    /* QUALITY... */
                                    self -> accession_short, row_id,
                                    name,
                                    quality -> len, quality );
        }
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_empty_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id,
                                quality -> len, quality );
    }
    if ( name -> len > 0 ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v2_real_name_no_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id,
                                name,
                                read1 -> len + read2 -> len, read1, read2 );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_empty_name_no_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

static const char * fmt_fastq_v2_real_name_frag_nr  = "@%s.%ld/%u %S length=%u\n%S%S\n+%s.%ld/%u %S length=%u\n%S\n";
static const char * fmt_fastq_v2_empty_name_frag_nr = "@%s.%ld/%u length=%u\n%S%S\n+%s.%ld/%u length=%u\n%S\n";
static const char * fmt_fasta_v2_real_name_frag_nr  = ">%s.%ld/%u %S length=%u\n%S%S\n";
static const char * fmt_fasta_v2_empty_name_frag_nr = ">%s.%ld/%u length=%u\n%S%S\n";
static rc_t print_v2_real_name_frag_nr( join_results_t * self,
                                         int64_t row_id,
                                         uint32_t dst_id,
                                         uint32_t read_id,
                                         const String * name,
                                         const String * read1,
                                         const String * read2,
                                         const String * quality ) {
    if ( NULL != quality ) {
        if ( name -> len > 0 ) {
            return join_results_print( self,
                                    dst_id, 
                                    fmt_fastq_v2_real_name_frag_nr,
                                    /* READ... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    read1 -> len + read2 -> len, read1, read2,
                                    /* QUALITY... */
                                    self -> accession_short, row_id, read_id,
                                    name,
                                    quality -> len, quality );
        }
        return join_results_print( self,
                                dst_id, 
                                fmt_fastq_v2_empty_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                read1 -> len + read2 -> len, read1, read2,
                                /* QUALITY... */
                                self -> accession_short, row_id, read_id,
                                quality -> len, quality );
    }
    if ( name -> len > 0 ) {
        return join_results_print( self,
                                dst_id, 
                                fmt_fasta_v2_real_name_frag_nr,
                                /* READ... */
                                self -> accession_short, row_id, read_id,
                                name,
                                read1 -> len + read2 -> len, read1, read2 );
    }
    return join_results_print( self,
                            dst_id, 
                            fmt_fasta_v2_empty_name_frag_nr,
                            /* READ... */
                            self -> accession_short, row_id, read_id,
                            read1 -> len + read2 -> len, read1, read2 );
}

rc_t make_join_results( struct KDirectory * dir, /* for join-printer */
                        join_results_t ** results,
                        struct temp_registry_t * registry, /* for join-printer */
                        const char * output_base, /* for join-printer */
                        const char * accession_short,
                        size_t file_buffer_size, /* for join-printer */
                        size_t print_buffer_size,
                        bool print_frag_nr,
                        bool print_name ) {
    rc_t rc = 0;
    join_results_t * p = calloc( 1, sizeof * p );
    *results = NULL;
    if ( NULL == p ) {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_join_results().calloc( %d ) -> %R", ( sizeof * p ), rc );
    } else {
        set_file_printer_args( &( p -> file_printer_args ), dir, registry, output_base, file_buffer_size );

        p -> accession_short = accession_short;
        p -> print_frag_nr = print_frag_nr;
        p -> print_name = print_name;

        if ( print_frag_nr ) {
            if ( print_name ) {
                p -> v1_print_name_null     = print_v1_syn_name_frag_nr;
                p -> v1_print_name_not_null = print_v1_real_name_frag_nr;
                p -> v2_print_name_null     = print_v2_syn_name_frag_nr;
                p -> v2_print_name_not_null = print_v2_real_name_frag_nr;
            } else {
                p -> v1_print_name_null     = print_v1_no_name_frag_nr;
                p -> v1_print_name_not_null = print_v1_no_name_frag_nr;
                p -> v2_print_name_null     = print_v2_no_name_frag_nr;
                p -> v2_print_name_not_null = print_v2_no_name_frag_nr;
            }
        } else {
            if ( print_name ) {
                p -> v1_print_name_null     = print_v1_syn_name_no_frag_nr;
                p -> v1_print_name_not_null = print_v1_real_name_no_frag_nr;
                p -> v2_print_name_null     = print_v2_syn_name_no_frag_nr;
                p -> v2_print_name_not_null = print_v2_real_name_no_frag_nr;
            } else {
                p -> v1_print_name_null     = print_v1_no_name_no_frag_nr;
                p -> v1_print_name_not_null = print_v1_no_name_no_frag_nr;
                p -> v2_print_name_null     = print_v2_no_name_no_frag_nr;
                p -> v2_print_name_not_null = print_v2_no_name_no_frag_nr;
            }
        }

        rc = make_SBuffer( &( p -> print_buffer ), print_buffer_size ); /* helper.c */
        if ( 0 == rc ) {
            VectorInit ( &p -> printers, 0, 4 );
            *results = p;
        }
    }
    return rc;
}

rc_t join_results_print( struct join_results_t * self, uint32_t read_id, const char * fmt, ... ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcSelf, rcNull );
        ErrMsg( "join_results_print() -> %R", rc );
    } else if ( NULL == fmt ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "join_results_print() -> %R", rc );
    } else {
        /* create or get the printer, based on the read_id */
        join_printer_t * p = VectorGet ( &self -> printers, read_id );
        if ( NULL == p ) {
            p = make_join_printer( &( self -> file_printer_args ), read_id );
            if ( NULL != p ) {
                rc = VectorSet ( &self -> printers, read_id, p );
                if ( 0 != rc ) {
                    destroy_join_printer( p, NULL );
                }
            } else {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            }
        }

        if ( 0 == rc && NULL != p ) {
            bool done = false;
            uint32_t cnt = 4;

            /* produce the line to be printed in self->print_buffer*/
            while ( 0 == rc && !done && cnt-- > 0 ) {
                va_list args;
                va_start ( args, fmt );
                rc = print_to_SBufferV( & self -> print_buffer, fmt, args );
                /* do not print failed rc, because it is used to increase the buffer */
                va_end ( args );

                done = ( 0 == rc );
                if ( !done ) {
                    rc = try_to_enlarge_SBuffer( & self -> print_buffer, rc );
                }
            }

            if ( 0 != rc ) {
                ErrMsg( "join_results_print().failed to enlarge buffer -> %R", rc );
            } else {
                /* write it to the file-handle in the printer */
                size_t num_writ, to_write;
                to_write = self -> print_buffer . S . size;
                const char * src = self -> print_buffer . S . addr;
                rc = KFileWriteAll( p -> f, p -> file_pos, src, to_write, &num_writ );
                if ( 0 != rc ) {
                    ErrMsg( "join_results_print().KFileWriteAll( at %lu ) -> %R", p -> file_pos, rc );
                }
                else if ( num_writ != to_write ) {
                    rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
                    ErrMsg( "join_results_print().KFileWriteAll( at %lu ) ( %d vs %d ) -> %R", p -> file_pos, to_write, num_writ, rc );
                } else {
                    p -> file_pos += num_writ;
                }
            }
        }
    }
    return rc;
}

rc_t join_results_print_fastq_v1( join_results_t * self,
                                  int64_t row_id,
                                  uint32_t dst_id,
                                  uint32_t read_id,
                                  const String * name,
                                  const String * read,
                                  const String * quality ) {
    if ( NULL == name ) {
        return self -> v1_print_name_null( self, row_id, dst_id, read_id, name, read, quality );
    }
    return self -> v1_print_name_not_null( self, row_id, dst_id, read_id, name, read, quality );
}

rc_t join_results_print_fastq_v2( join_results_t * self,
                                  int64_t row_id,
                                  uint32_t dst_id,
                                  uint32_t read_id,
                                  const String * name,
                                  const String * read1,
                                  const String * read2,
                                  const String * quality ) {
    if ( NULL == name ) {
        return self -> v2_print_name_null( self, row_id, dst_id, read_id, name, read1, read2, quality );
    }
    return self -> v2_print_name_not_null( self, row_id, dst_id, read_id, name, read1, read2, quality );
}

/* --------------------------------------------------------------------------------------------------- */

typedef struct filter_2na_t {
    struct Buf2NA_t * filter_buf2na;        /* the 2na-filter */
} filter_2na_t;

filter_2na_t * make_2na_filter( const char * filter_bases ) {
    filter_2na_t * res = NULL;
    if ( NULL != filter_bases ) {
        res = calloc( 1, sizeof * res );
        if ( NULL != res ) {
            rc_t rc = make_Buf2NA( &( res -> filter_buf2na ), 512, filter_bases ); /* helper.c */
            if ( 0 != rc ) {
                ErrMsg( "make_2na_filter().error creating nucstrstr-filter from ( %s ) -> %R", filter_bases, rc );
                free( ( void * )res );
                res = NULL;
            }
        }
    }
    return res;
}

void release_2na_filter( filter_2na_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> filter_buf2na ) {
            release_Buf2NA( self -> filter_buf2na );
        }
    }
}

bool filter_2na_1( filter_2na_t * self, const String * bases ) {
    bool res = true;
    if ( NULL != self && NULL != bases ) {
        res = match_Buf2NA( self -> filter_buf2na, bases ); /* helper.c */
    }
    return res;
}

bool filter_2na_2( filter_2na_t * self, const String * bases1, const String * bases2 ) {
    bool res = true;
    if ( NULL != self && NULL != bases1 && NULL != bases2 ) {
        res = ( match_Buf2NA( self -> filter_buf2na, bases1 ) || match_Buf2NA( self -> filter_buf2na, bases2 ) ); /* helper.c */
    }
    return res;
}


/* ----------------------------------------------------------------------------------------------------- */

/* the output is parameterized via var_fmt_t from helper.c */

static bool spot_group_requested_1( const char * defline ) {
    if ( NULL == defline ) {
        return false;
    } else {
        return ( NULL != strstr( defline, "$sg" ) );
    }
}

bool spot_group_requested( const char * seq_defline, const char * qual_defline ) {
    return ( spot_group_requested_1( seq_defline ) || spot_group_requested_1( qual_defline ) );
}

typedef struct flex_printer_t {
    file_printer_args_t * file_args;        /* dir, registry, output-base, buffer_size */
    Vector printers;                        /* container for printers, one for each read-id ( used if registry is not NULL ) */
    struct multi_writer_t * multi_writer;   /* from copy-machine, multi-threaded common-file writer */
    struct multi_writer_block_t * block;    /* keep a block at hand... */
    bool fasta;                             /* flag if FASTA or FASTQ */
    struct var_fmt_t * fmt_v1;              /* var-printer for 1-READ-data */
    struct var_fmt_t * fmt_v2;              /* var-printer for 2-READ-data */
    const String * string_data[ 8 ];        /* vector of strings, idx has to match var_desc_list */
    uint64_t int_data[ 4 ];                 /* vector of ints, idx has to match var_desc_list */
} flex_printer_t;

typedef enum string_data_index_t { sdi_acc = 0, sdi_sn = 1, sdi_sg = 2, sdi_rd1 = 3, sdi_rd2 = 4, sdi_qa = 5 } string_data_index_t;
typedef enum int_data_index_t { idi_si = 0, idi_ri = 1, idi_rl = 2 } int_data_index_t;

void release_flex_printer( struct flex_printer_t * self ) {
    if ( NULL != self ) {
        if ( NULL != self -> multi_writer && NULL != self -> block ) {
            if ( !multi_writer_submit_block( self -> multi_writer, self -> block ) ) {
                /* TBD: cannot submit last block to multi-writer */
                self -> block = NULL;
            }
        }
        if ( NULL != self -> file_args ) {  VectorWhack ( &self -> printers, destroy_join_printer, NULL ); }
        if ( NULL != self -> string_data[ sdi_acc ] ) StringWhack( self -> string_data[ 0 ] );
        if ( NULL != self -> fmt_v1 ) { release_var_fmt( self -> fmt_v1 ); }
        if ( NULL != self -> fmt_v2 ) { release_var_fmt( self -> fmt_v2 ); }
        free( ( void * )self );
    }
}

static struct var_desc_list_t * make_flex_printer_vars( void ) {
    struct var_desc_list_t * res = create_var_desc_list();
    if ( NULL != res ) {
        var_desc_list_add_str( res, "$ac",  sdi_acc );    /* accession  at #0 in the string-list */
        var_desc_list_add_str( res, "$sn",  sdi_sn  );    /* spot-name  at #1 in the string-list */
        var_desc_list_add_str( res, "$sg",  sdi_sg  );    /* spot-group at #2 in the string-list */
        var_desc_list_add_str( res, "$RD1", sdi_rd1 );    /* READ#1     at #3 in the string-list */
        var_desc_list_add_str( res, "$RD2", sdi_rd2 );    /* READ#2     at #4 in the string-list */
        var_desc_list_add_str( res, "$QA",  sdi_qa  );    /* QUALITY    at #5 in the string-list */

        var_desc_list_add_int( res, "$si",  idi_si );    /* spot-id/row-nr at #0 in the int-list */
        var_desc_list_add_int( res, "$ri",  idi_ri );    /* read-nr     at #1 in the int-list */
        var_desc_list_add_int( res, "$rl",  idi_rl );    /* read-lenght at #2 in the int-list */
    }
    return res;
}

/* move that one to helper.c */
static const String * make_string_copy( const char * src )
{
    const String * res = NULL;
    if ( NULL != src ) {
        String tmp;
        StringInitCString( &tmp, src );
        StringCopy( &res, &tmp );
    }
    return res;
}

static const char * dflt_seq_defline_fastq_use_name_ri = "@$ac.$si/$ri $sn length=$rl";
static const char * dflt_seq_defline_fastq_syn_name_ri = "@$ac.$si/$ri $si length=$rl";
static const char * dflt_seq_defline_fastq_no_name_ri = "@$ac.$si/$ri length=$rl";
static const char * dflt_seq_defline_fasta_use_name_ri = ">$ac.$si/$ri $sn length=$rl";
static const char * dflt_seq_defline_fasta_syn_name_ri = ">$ac.$si/$ri $si length=$rl";
static const char * dflt_seq_defline_fasta_no_name_ri = ">$ac.$si/$ri length=$rl";

static const char * dflt_seq_defline_fastq_use_name = "@$ac.$si $sn length=$rl";
static const char * dflt_seq_defline_fastq_syn_name = "@$ac.$si $si length=$rl";
static const char * dflt_seq_defline_fastq_no_name = "@$ac.$si length=$rl";
static const char * dflt_seq_defline_fasta_use_name = ">$ac.$si $sn length=$rl";
static const char * dflt_seq_defline_fasta_syn_name = ">$ac.$si $si length=$rl";
static const char * dflt_seq_defline_fasta_no_name = ">$ac.$si length=$rl";

const char * dflt_seq_defline( flex_printer_name_mode_t name_mode, bool use_read_id, bool fasta ) {
    if ( use_read_id ) {
        if ( fasta ) {
            switch ( name_mode ) {
                case fpnm_use_name : return dflt_seq_defline_fasta_use_name_ri; break;
                case fpnm_syn_name : return dflt_seq_defline_fasta_syn_name_ri; break;
                case fpnm_no_name  : return dflt_seq_defline_fasta_no_name_ri; break;
            }
        } else {
            switch ( name_mode ) {
                case fpnm_use_name : return dflt_seq_defline_fastq_use_name_ri; break;
                case fpnm_syn_name : return dflt_seq_defline_fastq_syn_name_ri; break;
                case fpnm_no_name  : return dflt_seq_defline_fastq_no_name_ri; break;
            }
        }
    } else {
        if ( fasta ) {
            switch ( name_mode ) {
                case fpnm_use_name : return dflt_seq_defline_fasta_use_name; break;
                case fpnm_syn_name : return dflt_seq_defline_fasta_syn_name; break;
                case fpnm_no_name  : return dflt_seq_defline_fasta_no_name; break;
            }
        } else {
            switch ( name_mode ) {
                case fpnm_use_name : return dflt_seq_defline_fastq_use_name; break;
                case fpnm_syn_name : return dflt_seq_defline_fastq_syn_name; break;
                case fpnm_no_name  : return dflt_seq_defline_fastq_no_name; break;
            }
        }
    }
    return NULL;
}

static const char * dflt_qual_defline_use_name_ri = "+$ac.$si/$ri $sn length=$rl";
static const char * dflt_qual_defline_syn_name_ri = "+$ac.$si/$ri $si length=$rl";
static const char * dflt_qual_defline_no_name_ri = "+$ac.$si/$ri length=$rl";

static const char * dflt_qual_defline_use_name = "+$ac.$si $sn length=$rl";
static const char * dflt_qual_defline_syn_name = "+$ac.$si $si length=$rl";
static const char * dflt_qual_defline_no_name = "+$ac.$si length=$rl";

const char * dflt_qual_defline( flex_printer_name_mode_t name_mode, bool use_read_id ) {
    if ( use_read_id ) {
        switch ( name_mode ) {
            case fpnm_use_name : return dflt_qual_defline_use_name_ri; break;
            case fpnm_syn_name : return dflt_qual_defline_syn_name_ri; break;
            case fpnm_no_name  : return dflt_qual_defline_no_name_ri; break;
        }
    } else {
        switch ( name_mode ) {
            case fpnm_use_name : return dflt_qual_defline_use_name; break;
            case fpnm_syn_name : return dflt_qual_defline_syn_name; break;
            case fpnm_no_name  : return dflt_qual_defline_no_name; break;
        }
    }
    return NULL;
}

/* ------------------------------------------
   if qual-defline is NULL --> FASTA, else FASTQ
   if both def-lines are NULL --> pick default one's
   ------------------------------------------ */
static const String * make_flex_printer_format_string( const char * seq_defline,
                                                 const char * qual_defline,
                                                 size_t num_reads,
                                                 flex_printer_name_mode_t name_mode,
                                                 bool use_read_id,
                                                 bool fasta ){
    const String * res = NULL;
    char buffer[ 4096 ];
    size_t num_writ;
    rc_t rc = 0;
    if ( fasta ) {   
        /* ===== FASTA ===== */
        const char * a_seq_defline = seq_defline;
        if ( NULL == a_seq_defline ) { a_seq_defline = dflt_seq_defline( name_mode, use_read_id, fasta ); }
        if ( NULL == a_seq_defline ) { /* TBD: rc = RC() */  }
        if ( 1 == num_reads ) {
            /* seq_defline + '\n' + read1 + '\n' */
            rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                "%s\n$RD1\n", a_seq_defline );
        } else if ( 2 == num_reads ) {
            /* seq_defline + '\n' + read1 + read2 + '\n' */
            rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                "%s\n$RD1$RD2\n", a_seq_defline );
        }
    } else {
        /* ===== FASTQ ===== */
        const char * a_seq_defline = seq_defline;
        const char * a_qual_defline = qual_defline;
        if ( NULL == a_seq_defline ) { a_seq_defline = dflt_seq_defline( name_mode, use_read_id, fasta ); }
        if ( NULL == a_qual_defline ) { a_qual_defline = dflt_qual_defline( name_mode, use_read_id ); }
        if ( NULL == a_seq_defline ) { /* TBD: rc = RC() */  }
        if ( NULL == a_qual_defline ) { /* TBD: rc = RC() */  }
        if ( 1 == num_reads ) {
            /* seq_defline + '\n' + read1 + '\n' + qual_defline + '\n' + qual + '\n' */
            rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                "%s\n$RD1\n%s\n$QA\n", a_seq_defline, a_qual_defline );
        } else if ( 2 == num_reads ) {
            /* seq_defline + '\n' + read1 + read2 + '\n' + qual_defline + '\n' + qual + '\n' */
            rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                "%s\n$RD1$RD2\n%s\n$QA\n", a_seq_defline, a_qual_defline );
        }
    }
    if ( 0 == rc ) {
        res = make_string_copy( buffer );
    }
    return res;
}

static bool flex_printer_args_valid( struct file_printer_args_t * file_args,
                                     struct multi_writer_t * multi_writer ) {
    if ( NULL == file_args && NULL == multi_writer ) {
        return false;
    }
    if ( NULL != file_args && NULL != multi_writer ) {
        return false;
    }
    return true;
}

struct flex_printer_t * make_flex_printer( file_printer_args_t * file_args,
                        struct multi_writer_t * multi_writer,
                        const char * accession,
                        const char * seq_defline,
                        const char * qual_defline,
                        flex_printer_name_mode_t name_mode,
                        bool use_read_id, /* needed for picking a default, split...true, whole...false */
                        bool fasta ) {
    if ( !flex_printer_args_valid( file_args, multi_writer ) ) {
        return NULL;
    }
    flex_printer_t * self = calloc( 1, sizeof * self );
    if ( NULL != self ) {
        if ( NULL != file_args ) {
            self -> file_args = file_args;
            VectorInit ( &( self -> printers ), 0, 4 );
        }
        if ( NULL != multi_writer ) { self -> multi_writer = multi_writer; }
        self -> fasta = fasta;

        /* pre-set the accession-variable in the string-data, this one does not change during the
           lifetime of the flex-printer */
        self -> string_data[ sdi_acc ] = make_string_copy( accession );

        /* construct the 2 flex-formats ( one with 1xREAD/1xQUAL, one with 2xREAD/2xQUAL ) */
        /* ------------------------------------------------------------------------------- */
        /* first create the variable-definitions ( and their indexes ) */
        struct var_desc_list_t * vdl = make_flex_printer_vars();
        if ( NULL == vdl ) {
            release_flex_printer( self );
            self = NULL;
        } else {
            /* join seq_defline and qual_defline into one format-definition, describing a whole spot or read */
            const String * flex_fmt1 = make_flex_printer_format_string( seq_defline, qual_defline, 1, name_mode, use_read_id, fasta );
            const String * flex_fmt2 = make_flex_printer_format_string( seq_defline, qual_defline, 2, name_mode, use_read_id, fasta );
            if ( NULL == flex_fmt1 || NULL == flex_fmt2 ) {
                release_flex_printer( self );
                self = NULL;
            } else {
                self -> fmt_v1 = create_var_fmt( flex_fmt1, vdl );
                self -> fmt_v2 = create_var_fmt( flex_fmt2, vdl );
                if ( NULL == self -> fmt_v1 || NULL == self -> fmt_v2 ) {
                    release_flex_printer( self );
                    self = NULL;
                }
            }
            if ( NULL != flex_fmt1 ) { StringWhack( flex_fmt1 ); }  /* create_var_fmt makes a copy! */
            if ( NULL != flex_fmt2 ) { StringWhack( flex_fmt2 ); }  /* create_var_fmt makes a copy! */
            release_var_desc_list( vdl );   /* has been copied into fmt1 and fmt2 */
        }
    }
    return self;
}

static join_printer_t * get_or_make_join_printer( Vector * v, uint32_t read_id,
                                                  file_printer_args_t * file_args ) {
    join_printer_t * res = VectorGet ( v, read_id );
    if ( NULL == res ) {
        res = make_join_printer( file_args, read_id );
        if ( NULL != res ) { VectorSet ( v, read_id, res ); }
    }
    return res;
}

static uint64_t calc_read_length( const flex_printer_data_t * data ) {
    uint64_t res = 0;
    if ( NULL != data -> read1 ) { res += data -> read1 -> len; }
    if ( NULL != data -> read2 ) { res += data -> read2 -> len; }
    return res;
}

static struct var_fmt_t * flex_printer_prepare_data( struct flex_printer_t * self, const flex_printer_data_t * data ) {
    /* pick the right format, depending if data-read2 is NULL or not */
    struct var_fmt_t * fmt = ( NULL == data -> read2 ) ? self -> fmt_v1 : self -> fmt_v2;

    /* prepare string-data, common between FASTA and FASTQ ( accession aka #0 is set by constructor ) */
    self -> string_data[ sdi_sn ] = ( NULL != data -> spotname ) ? data -> spotname : NULL;
    self -> string_data[ sdi_sg ] = ( NULL != data -> spotgroup ) ? data -> spotgroup : NULL;
    self -> string_data[ sdi_rd1 ] = ( NULL != data -> read1 ) ? data -> read1 : NULL;
    self -> string_data[ sdi_rd2 ] = ( NULL != data -> read2 ) ? data -> read2 : NULL;
    self -> string_data[ sdi_qa ] = ( NULL != data -> quality ) ? data -> quality : NULL;
    /* prepare int-data, common between FASTA and FASTQ */
    self -> int_data[ idi_si ] = ( uint64_t )data -> row_id;
    self -> int_data[ idi_ri ] = ( uint64_t )data -> read_id;
    self -> int_data[ idi_rl ] = calc_read_length( data );

    return fmt;
}

static bool join_result_flex_submit( struct flex_printer_t * self, SBuffer_t * t ) {
    bool res = false;
    if ( NULL == self -> block ) {
        self -> block = multi_writer_get_empty_block( self -> multi_writer );
    }
    res = ( NULL != self -> block );
    if ( !res ) {
        /* TBD : error could not get block from multi-writer... */
    } else {
        res = multi_writer_block_append( self -> block, t -> S. addr, t -> S . len );
        if ( !res ) {
            /* block was not big enough to hold the new data : */
            res = multi_writer_submit_block( self -> multi_writer, self -> block );
            if ( !res ) {
                /* TBD : error cannot submit! */
            } else {
                self -> block = multi_writer_get_empty_block( self -> multi_writer );
                res = ( NULL != self -> block );
                if ( !res ) {
                    /* TBD : error could not get block from multi-writer... */
                } else {
                    res = multi_writer_block_append( self -> block, t -> S. addr, t -> S . len );
                    if ( !res ) {
                        /* oops the data does not fit into an new, empty block... */
                        res = multi_writer_block_expand( self -> block, t -> S . len + 1 );
                        if ( !res ) {
                            /* TBD: cannot expand block... */
                        } else {
                            res = multi_writer_block_append( self -> block, t -> S. addr, t -> S . len );
                            if ( !res ) {
                                /* TBD: still cannot write to expanded block */
                            }
                        }
                    }
                }
            }
        }
        //rc = multi_writer_write( self -> multi_writer, t -> S . addr, t -> S . len );
    }
    return res;
}

rc_t join_result_flex_print( struct flex_printer_t * self, const flex_printer_data_t * data ) {
    rc_t rc = 0;
    if ( NULL == self || data == NULL ) {
        rc = RC( rcVDB, rcNoTarg, rcReading, rcParam, rcInvalid );
        ErrMsg( "join_results.c join_result_flex_print() -> %R", rc );
    } else {
        /* pick the right format, depending if data-read2 is NULL or not */
        struct var_fmt_t * fmt = flex_printer_prepare_data( self, data );
        if ( NULL != self -> file_args ) {
            /* we are in file-per-read-id--mode */
            join_printer_t * printer = get_or_make_join_printer( &( self -> printers ), 
                            data -> dst_id, self -> file_args );
            if ( NULL != printer ) {
                rc = var_fmt_to_file( fmt,
                                    printer -> f, &( printer -> file_pos ),
                                    self -> string_data, sdi_qa + 1,
                                    self -> int_data, idi_rl + 1 );
            } else {
                /* TBD error */
            }
        } else if ( NULL != self -> multi_writer ) {
           /* we are in multi-writer-mode */
            SBuffer_t * t = var_fmt_to_buffer( fmt, 
                                               self -> string_data, sdi_qa + 1,
                                               self -> int_data, idi_rl + 1 );
            if ( NULL != t ) {
                join_result_flex_submit( self, t );
            }
       }
    }
    return rc;
}
