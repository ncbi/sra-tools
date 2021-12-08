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

#include "flex_printer.h"

#include <string.h>     /* for strstr() */

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_dflt_defline_
#include "dflt_defline.h"
#endif

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_var_fmt_
#include "var_fmt.h"
#endif

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

/* the output is parameterized via var_fmt_t from helper.c */

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
        var_desc_list_add_str( res, "$ac",  sdi_acc, 0xFF );    /* accession  at #0 in the string-list */
        var_desc_list_add_str( res, "$sn",  sdi_sn,  idi_si );  /* spot-name  at #1 in the string-list ( has idi_si as alternative! ) */
        var_desc_list_add_str( res, "$sg",  sdi_sg,  0xFF );    /* spot-group at #2 in the string-list */
        var_desc_list_add_str( res, "$RD1", sdi_rd1, 0xFF );    /* READ#1     at #3 in the string-list */
        var_desc_list_add_str( res, "$RD2", sdi_rd2, 0xFF );    /* READ#2     at #4 in the string-list */
        var_desc_list_add_str( res, "$QA",  sdi_qa,  0xFF );    /* QUALITY    at #5 in the string-list */

        var_desc_list_add_int( res, "$si",  idi_si );    /* spot-id/row-nr at #0 in the int-list */
        var_desc_list_add_int( res, "$ri",  idi_ri );    /* read-nr     at #1 in the int-list */
        var_desc_list_add_int( res, "$rl",  idi_rl );    /* read-lenght at #2 in the int-list */
    }
    return res;
}

/* ------------------------------------------
   if qual-defline is NULL --> FASTA, else FASTQ
   if both def-lines are NULL --> pick default one's
   ------------------------------------------ */
static const String * make_flex_printer_format_string( const char * seq_defline,
                                                 const char * qual_defline,
                                                 size_t num_reads,
                                                 bool fasta ){
    const String * res = NULL;
    char buffer[ 4096 ];
    size_t num_writ;
    rc_t rc = 0;
    if ( fasta ) {   
        /* ===== FASTA ===== */
        if ( NULL == seq_defline ) { 
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
            ErrMsg( "make_flex_printer_format_string( FASTA ) seq_defline is NULL -> %R", rc );
        }
        if ( 0 == rc ) {
            if ( 1 == num_reads ) {
                /* seq_defline + '\n' + read1 + '\n' */
                rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                    "%s\n$RD1\n", seq_defline );
            } else if ( 2 == num_reads ) {
                /* seq_defline + '\n' + read1 + read2 + '\n' */
                rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                    "%s\n$RD1$RD2\n", seq_defline );
            }
            if ( 0 != rc ) {
                ErrMsg( "make_flex_printer_format_string( FASTA ) string_printf() failed -> %R", rc );
            }
        }
    } else {
        /* ===== FASTQ ===== */
        if ( NULL == seq_defline ) {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
            ErrMsg( "make_flex_printer_format_string( FASTQ ) seq_defline is NULL -> %R", rc );
        }
        if ( 0 == rc ) {
            if ( NULL == qual_defline ) {
                rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
                ErrMsg( "make_flex_printer_format_string( FASTQ ) qual_defline is NULL -> %R", rc );
            }
            if ( 0 == rc ) {
                if ( 1 == num_reads ) {
                    /* seq_defline + '\n' + read1 + '\n' + qual_defline + '\n' + qual + '\n' */
                    rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                        "%s\n$RD1\n%s\n$QA\n", seq_defline, qual_defline );
                } else if ( 2 == num_reads ) {
                    /* seq_defline + '\n' + read1 + read2 + '\n' + qual_defline + '\n' + qual + '\n' */
                    rc = string_printf ( buffer, sizeof buffer, &num_writ,
                                        "%s\n$RD1$RD2\n%s\n$QA\n", seq_defline, qual_defline );
                }
                if ( 0 != rc ) {
                    ErrMsg( "make_flex_printer_format_string( FASTQ ) string_printf() failed -> %R", rc );
                }
            }
        }
    }
    if ( 0 == rc ) {
        res = make_string_copy( buffer );
    }
    return res;
}

#if 0
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
            const String * flex_fmt1 = make_flex_printer_format_string( seq_defline, qual_defline, 1, fasta );
            const String * flex_fmt2 = make_flex_printer_format_string( seq_defline, qual_defline, 2, fasta );
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
#endif

static struct flex_printer_t * make_flex_printer_cmn( flex_printer_t * self,
                        const char * accession,
                        const char * seq_defline,
                        const char * qual_defline,
                        bool fasta ) {
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
        const String * flex_fmt1 = make_flex_printer_format_string( seq_defline, qual_defline, 1, fasta );
        const String * flex_fmt2 = make_flex_printer_format_string( seq_defline, qual_defline, 2, fasta );
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
    return self;
}
                                             
struct flex_printer_t * make_flex_printer_1( file_printer_args_t * file_args,
                        const char * accession,
                        const char * seq_defline,
                        const char * qual_defline,
                        bool fasta ) {
    flex_printer_t * self = NULL;
    if ( NULL == file_args || NULL == seq_defline || NULL == accession ) {
        return NULL;
    }
    if ( !fasta && NULL == qual_defline ) {
        return NULL;
    }
    self = calloc( 1, sizeof * self );
    if ( NULL != self ) {
        self -> file_args = file_args;
        VectorInit ( &( self -> printers ), 0, 4 );
        self = make_flex_printer_cmn( self, accession, seq_defline, qual_defline, fasta );
    }
    return self;
}

struct flex_printer_t * make_flex_printer_2( struct multi_writer_t * multi_writer,
                        const char * accession,
                        const char * seq_defline,
                        const char * qual_defline,
                        bool fasta ) {
    flex_printer_t * self = NULL;
    if ( NULL == multi_writer || NULL == seq_defline || NULL == accession ) {
        return NULL;
    }
    if ( !fasta && NULL == qual_defline ) {
        return NULL;
    }
    self = calloc( 1, sizeof * self );
    if ( NULL != self ) {
        self -> multi_writer = multi_writer;
        self = make_flex_printer_cmn( self, accession, seq_defline, qual_defline, fasta );
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

/* submit a buffer to the multi-writer ( via a queue into a different thread! ) */
static rc_t flex_submit( struct flex_printer_t * self, SBuffer_t * t ) {
    rc_t rc = 0;
    if ( NULL == self -> block ) {
        self -> block = multi_writer_get_empty_block( self -> multi_writer );
    }
    if ( NULL == self -> block ) {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "flex_submit() could not get block from multi-writer -> %R", rc );
    } else {
        if ( !multi_writer_block_append( self -> block, t -> S. addr, t -> S . len ) ) {
            /* block was not big enough to hold the new data : */
            if ( !multi_writer_submit_block( self -> multi_writer, self -> block ) ) {
                rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                ErrMsg( "flex_submit() cannot submit block to multi-writer -> %R", rc );
            } else {
                self -> block = multi_writer_get_empty_block( self -> multi_writer );
                if ( NULL == self -> block ) {
                    rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                    ErrMsg( "flex_submit() could not get block from multi-writer -> %R", rc );
                } else {
                    if ( !multi_writer_block_append( self -> block, t -> S. addr, t -> S . len ) ) {
                        /* oops the data does not fit into an new, empty block... */
                        if ( ! multi_writer_block_expand( self -> block, t -> S . len + 1 ) ) {
                            rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                            ErrMsg( "flex_submit() could not expand block from multi-writer -> %R", rc );
                        } else {
                            if ( !multi_writer_block_append( self -> block, t -> S. addr, t -> S . len ) ) {
                                rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                                ErrMsg( "flex_submit() still cannot append block to multi-writer -> %R", rc );
                            }
                        }
                    }
                }
            }
        }
    }
    return rc;
}

rc_t flex_print( struct flex_printer_t * self, const flex_printer_data_t * data ) {
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
                rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
                ErrMsg( "flex_print() cannot create printer -> %R", rc );
            }
        } else if ( NULL != self -> multi_writer ) {
            /* we are in multi-writer-mode */
            SBuffer_t * t = var_fmt_to_buffer( fmt, 
                                               self -> string_data, sdi_qa + 1,
                                               self -> int_data, idi_rl + 1 );
            if ( NULL != t ) {
                rc = flex_submit( self, t );
            } else {
                rc = RC( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
                ErrMsg( "flex_print() cannot format data into buffer -> %R", rc );
            }
       }
    }
    return rc;
}
