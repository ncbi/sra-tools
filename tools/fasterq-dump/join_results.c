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

typedef struct join_printer_args_t {
    KDirectory * dir;
    struct temp_registry_t * registry;
    const char * output_base;
    size_t buffer_size;
} join_printer_args_t;

static void set_join_printer_args( join_printer_args_t * self,
                            KDirectory * dir,
                            struct temp_registry_t * registry,
                            const char * output_base,
                            size_t buffer_size ) {
    self -> dir = dir;
    self -> registry = registry;
    self -> output_base = output_base;
    self -> buffer_size = buffer_size;
}

static rc_t release_file( struct KFile * f ) {
    rc_t rc = KFileRelease( f );
    if ( 0 != rc ) {
        ErrMsg( "release_file().KFileRelease() -> %R", rc );
    }
    return rc;
}

static void CC destroy_join_printer( void * item, void * data ) {
    if ( NULL != item ) {
        join_printer_t * p = item;
        if ( NULL != p -> f ) { release_file( p -> f ); }
        free( item );
    }
}

static rc_t wrap_file_in_buffer( struct KFile ** f, size_t buffer_size ) {
    struct KFile * temp_file = *f;
    rc_t rc = KBufFileMakeWrite( &temp_file, *f, false, buffer_size );
    if ( 0 != rc ) {
        ErrMsg( "wrap_file_in_buffer().KBufFileMakeWrite() -> %R", rc );
    } else {
        rc = release_file( *f );
        if ( 0 == rc ) { *f = temp_file; }
    }
    return rc;
}

static join_printer_t * make_join_printer( join_printer_args_t * args, uint32_t read_id ) {
    join_printer_t * res = NULL;
    if ( NULL != args ) {
        res = calloc( 1, sizeof * res );
        if ( NULL != res ) {
            char filename[ 4096 ];
            size_t num_writ;

            rc_t rc = string_printf( filename, sizeof filename, &num_writ, "%s.%u", args -> output_base, read_id );
            if ( 0 != rc ) {
                ErrMsg( "make_join_printer().string_printf() -> %R", rc );
            } else {
                struct KFile * f;
                rc = KDirectoryCreateFile( args -> dir, &f, false, 0664, kcmInit, "%s", filename );
                if ( 0 != rc ) {
                    ErrMsg( "make_join_printer().KDirectoryCreateFile( '%s' ) -> %R", filename, rc );
                } else {
                    if ( args -> buffer_size > 0 ) {
                        rc = wrap_file_in_buffer( &f, args -> buffer_size );
                        if ( 0 != rc ) { release_file( f ); }
                    }
                    if ( 0 == rc ) {
                        rc = register_temp_file( args -> registry, read_id, filename );
                        if ( 0 == rc ) { res -> f = f; }  /* pos is already 0, because calloc() */
                    }
                }
            }
            if ( 0 != rc ) {
                free( ( void * )res );
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
    join_printer_args_t join_printer_args;
    struct Buf2NA_t * filter_buf2na;
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
        if ( NULL != self -> filter_buf2na ) {
            release_Buf2NA( self -> filter_buf2na );
        }
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
                        bool print_name,
                        const char * filter_bases ) {
    rc_t rc = 0;
    struct Buf2NA_t * filter_buf2na = NULL;
    if ( filter_bases != NULL ) {
        rc = make_Buf2NA( &filter_buf2na, 512, filter_bases );
        if ( 0 != rc ) {
            ErrMsg( "make_join_results().error creating nucstrstr-filter from ( %s ) -> %R", filter_bases, rc );
        }
    }
    if ( rc == 0 ) {
        join_results_t * p = calloc( 1, sizeof * p );
        *results = NULL;
        if ( NULL == p ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_join_results().calloc( %d ) -> %R", ( sizeof * p ), rc );
        } else {
            set_join_printer_args( &( p -> join_printer_args ), dir, registry, output_base, file_buffer_size );

            p -> accession_short = accession_short;
            p -> print_frag_nr = print_frag_nr;
            p -> print_name = print_name;
            p -> filter_buf2na = filter_buf2na;
            
            /* available:
                print_v1_no_name_no_frag_nr()       print_v2_no_name_no_frag_nr()
                print_v1_no_name_frag_nr()          print_v2_no_name_frag_nr()
                print_v1_syn_name_no_frag_nr()      print_v2_syn_name_no_frag_nr()
                print_v1_syn_name_frag_nr()         print_v2_syn_name_frag_nr()
                print_v1_real_name_no_frag_nr()     print_v2_real_name_no_frag_nr()
                print_v1_real_name_frag_nr()        print_v2_real_name_frag_nr()
            */
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
    }
    if ( 0 != rc && NULL != filter_buf2na ) {
        release_Buf2NA( filter_buf2na );
    }
    return rc;
}

bool join_results_filter( join_results_t * self, const String * bases ) {
    bool res = true;
    if ( NULL != self && NULL != bases && NULL != self -> filter_buf2na ) {
        res = match_Buf2NA( self -> filter_buf2na, bases ); /* helper.c */
    }
    return res;
}

bool join_results_filter2( struct join_results_t * self, const String * bases1, const String * bases2 ) {
    bool res = true;
    if ( NULL != self && NULL != bases1 && NULL != bases2 && NULL != self -> filter_buf2na ) {
        res = ( match_Buf2NA( self -> filter_buf2na, bases1 ) || match_Buf2NA( self -> filter_buf2na, bases2 ) ); /* helper.c */
    }
    return res;
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
            p = make_join_printer( &( self -> join_printer_args ), read_id );
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

/* ----------------------------------------------------------------------------------------------------- */

typedef struct common_join_results_t {
    KDirectory * dir;
    struct Buf2NA_t * filter_buf2na;
    SBuffer_t print_buffer;   /* we have only one print_buffer... */
    size_t buffer_size;
    struct KFile * f;
    struct KLock * lock;
    uint64_t file_pos;
} common_join_results_t;

void destroy_common_join_results( struct common_join_results_t * self ) {
    if ( NULL != self ) {
        release_SBuffer( &self -> print_buffer );
        if ( NULL != self -> filter_buf2na ) { release_Buf2NA( self -> filter_buf2na ); }
        if ( NULL != self -> f ) {
            rc_t rc = KFileRelease( self -> f );
            if ( 0 != rc ) {
                ErrMsg( "destroy_common_join_results().KFileRelease() -> %R", rc );
            }
        }
        if ( NULL != self -> lock ) {
            rc_t rc = KLockRelease( self -> lock );
            if ( 0 != rc ) {
                ErrMsg( "destroy_common_join_results().KLockRelease() -> %R", rc );
            }
        }
        free( ( void * ) self );
    }
}

static rc_t make_common_file( common_join_results_t * self, const char * output_filename, bool force ) {
    struct KFile * f;
    KCreateMode create_mode = force ? kcmInit : kcmCreate;
    rc_t rc = KDirectoryCreateFile( self -> dir, &f, false, 0664, create_mode, "%s", output_filename );
    if ( 0 != rc ) {
        ErrMsg( "make_common_file().KDirectoryVCreateFile() -> %R", rc );
    } else {
        if ( self -> buffer_size > 0 ) {
            struct KFile * temp_file = f;
            rc = KBufFileMakeWrite( &temp_file, f, false, self -> buffer_size );
            if ( 0 != rc ) {
                ErrMsg( "make_common_file().KBufFileMakeWrite() -> %R", rc );
            }
            {
                rc_t rc2 = KFileRelease( f );
                if ( 0 != rc2 ) {
                    ErrMsg( "make_common_file().KFileRelease().1 -> %R", rc2 );
                    rc = ( 0 == rc ) ? rc2 : rc;
                }
            }
            f = temp_file;
        }
        if ( 0 == rc ) {
            rc = KLockMake ( &self -> lock );
            if ( 0 != rc ) {
                ErrMsg( "make_common_file().KLockMake() -> %R", rc );
                KFileRelease( f );
                f = NULL;
                self -> lock = NULL;
            }
        }
        if ( 0 == rc ) {
            self -> f = f;
        }
    }
    return rc;
}

rc_t make_common_join_results( struct KDirectory * dir,
                        struct common_join_results_t ** results,
                        size_t file_buffer_size,
                        size_t print_buffer_size,
                        const char * filter_bases,
                        const char * output_filename,
                        bool force ) {
    rc_t rc = 0;
    struct Buf2NA_t * filter_buf2na = NULL;
    if ( filter_bases != NULL ) {
        rc = make_Buf2NA( &filter_buf2na, 512, filter_bases );
        if ( 0 != rc ) {
            ErrMsg( "make_common_join_results().error creating nucstrstr-filter from ( %s ) -> %R", filter_bases, rc );
        }
    }
    if ( rc == 0 ) {
        common_join_results_t * p = calloc( 1, sizeof * p );
        *results = NULL;
        if ( NULL == p ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_common_join_results().calloc( %d ) -> %R", ( sizeof * p ), rc );
        } else {
            p -> dir = dir;
            p -> buffer_size = file_buffer_size;
            p -> filter_buf2na = filter_buf2na;

            rc = make_SBuffer( &( p -> print_buffer ), print_buffer_size ); /* helper.c */
            if ( 0 == rc ) {
                if ( NULL != output_filename ) { rc = make_common_file( p, output_filename, force ); }
                if ( 0 == rc ) { *results = p; }
            }
        }
    }
    if ( 0 != rc ) { destroy_common_join_results( *results ); }
    return rc;
}

bool common_join_results_filter( struct common_join_results_t * self, const String * bases ) {
    bool res = true;
    if ( NULL != self && NULL != bases && NULL != self -> filter_buf2na ) {
        res = match_Buf2NA( self -> filter_buf2na, bases ); /* helper.c */
    }
    return res;
}

rc_t common_join_results_print( struct common_join_results_t * self, const char * fmt, ... ) {
    rc_t rc = 0;
    if ( NULL == self ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcSelf, rcNull );
        ErrMsg( "join_results_print() -> %R", rc );
    }
    else if ( NULL == fmt ) {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "join_results_print() -> %R", rc );
    } else {
        if ( NULL == self -> f ) {
            /* print directly to stdout via KOutVMsg... */
            va_list args;
            va_start ( args, fmt );
            rc = KOutVMsg ( fmt, args );
            va_end ( args );
        } else {
            rc = KLockAcquire ( self -> lock );
            if ( 0 == rc ) {
                /* print into the print-buffer and then write into file */
                bool done = false;
                uint32_t cnt = 4; /* we try 4 times to increase the print-buffer until it fits */

                while ( 0 == rc && !done && cnt-- > 0 ) {
                    va_list args;
                    va_start ( args, fmt );
                    rc = print_to_SBufferV( & self -> print_buffer, fmt, args );
                    /* do not print failed rc, because it is used to increase the buffer */
                    va_end ( args );

                    done = ( 0 == rc );
                    if ( !done ) { rc = try_to_enlarge_SBuffer( & self -> print_buffer, rc ); }
                }

                if ( 0 != rc ) {
                    ErrMsg( "join_results_print().failed to enlarge buffer -> %R", rc );
                } else {
                    size_t num_writ, to_write;
                    to_write = self -> print_buffer . S . size;
                    const char * src = self -> print_buffer . S . addr;
                    rc = KFileWriteAll( self -> f, self -> file_pos, src, to_write, &num_writ );
                    if ( 0 != rc ) {
                        ErrMsg( "join_results_print().KFileWriteAll( at %lu ) -> %R", self -> file_pos, rc );
                    } else if ( num_writ != to_write ) {
                        rc = RC( rcVDB, rcNoTarg, rcWriting, rcFormat, rcInvalid );
                        ErrMsg( "join_results_print().KFileWriteAll( at %lu ) ( %d vs %d ) -> %R", self -> file_pos, to_write, num_writ, rc );
                    } else {
                        self -> file_pos += num_writ;
                    }
                }
                KLockUnlock( self -> lock );
            }
        }
    }
    return rc;
}

/* ----------------------------------------------------------------------------------------------------- */

/* the output is parameterized via var_fmt_t from helper.c */

typedef struct flex_printer_t {
    join_printer_args_t join_printer_args;
    const char * accession_short;
    struct Buf2NA_t * filter_buf2na;
    Vector printers;                        /* a vector of join-printers */
} flex_printer_t;

struct flex_printer_t * make_flex_printer( struct KDirectory * dir,
                        struct temp_registry_t * registry,
                        const char * output_base,
                        size_t file_buffer_size,
                        const char * accession_short,
                        const char * filter_bases ) {
    flex_printer_t * self = calloc( 1, sizeof * self );
    if ( NULL == self ) {
        set_join_printer_args( &( self -> join_printer_args ), dir, registry, output_base, file_buffer_size );

    }
    return self;
}
