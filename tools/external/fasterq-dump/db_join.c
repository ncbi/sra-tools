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

#include "db_join.h"

#ifndef _h_err_msg_
#include "err_msg.h"
#endif

#ifndef _h_dflt_defline_
#include "dflt_defline.h"
#endif

#ifndef _h_index_
#include "index.h"
#endif

#ifndef _h_file_tools_
#include "file_tools.h"
#endif

#ifndef _h_lookup_reader_
#include "lookup_reader.h"
#endif

#ifndef _h_raw_read_iter_
#include "raw_read_iter.h"
#endif

#ifndef _h_fq_seq_csra_iter_
#include "fq_seq_csra_iter.h"
#endif

#ifndef _h_align_iter_
#include "align_iter.h"
#endif

#ifndef _h_flex_printer_
#include "flex_printer.h"
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_insdc_insdc_
#include <insdc/insdc.h> /* for READ_TYPE_BIOLOGICAL, READ_TYPE_REVERSE */
#endif

typedef struct dbj_cmn_t {
    const char * accession_path;
    const char * accession_short;
    join_stats_t * stats;
    const join_options_t * join_options;
    struct bg_progress_t * progress;
    struct lookup_reader_t * lookup;        /* lookup_reader.h */
    struct index_reader_t * index;          /* index.h */
    struct flp_t * flex_printer;            /* flex_printer.h */
    struct filter_2na_t * filter;           /* helper.h */
    SBuffer_t looked_up_bases_1;            /* helper.h */
    SBuffer_t looked_up_bases_2;            /* helper.h */
    uint64_t loop_nr;                       /* in which loop of this partial join are we? */
    uint32_t thread_id;                     /* in which thread are we? */
    bool cmp_read_present;                  /* do we have a cmp-read column? */
} dbj_cmn_t;

static void dbj_release_cmn_data( dbj_cmn_t* j ) {
    if ( NULL != j ) {
        release_index_reader( j-> index );
        release_lookup_reader( j -> lookup );               /* lookup_reader.c */
        release_SBuffer( &( j -> looked_up_bases_1 ) );     /* helper.c */
        release_SBuffer( &( j -> looked_up_bases_2 ) );     /* helper.c */
    }
}

static rc_t dbj_init_cmn_data( dbj_cmn_t * j,
                        join_stats_t * stats,
                        const join_options_t * join_options,
                        struct bg_progress_t * progress,
                        cmn_iter_params_t * cp,
                        struct flp_t * flex_printer,
                        struct filter_2na_t * filter,
                        const char * lookup_filename,
                        const char * index_filename,
                        size_t buf_size,
                        bool cmp_read_present ) {
    rc_t rc;

    j -> accession_path  = cp -> accession_path;
    j -> accession_short = cp -> accession_short;
    j -> stats = stats;
    j -> join_options = join_options;
    j -> progress = progress;
    j -> lookup = NULL;
    j -> flex_printer = flex_printer;
    j -> filter = filter;
    j -> looked_up_bases_1 . S . addr = NULL;
    j -> looked_up_bases_2 . S . addr = NULL;
    j -> loop_nr = 0;
    j -> cmp_read_present = cmp_read_present;

    if ( NULL != index_filename ) {
        if ( ft_file_exists( cp -> dir, "%s", index_filename ) ) {
            rc = make_index_reader( cp -> dir, &j -> index, buf_size, "%s", index_filename ); /* index.c */
        }
    } else {
        j -> index = NULL;
    }

    rc = make_lookup_reader( cp -> dir, j -> index, &( j -> lookup ), buf_size,
                             "%s", lookup_filename ); /* lookup_reader.c */
    if ( 0 == rc ) {
        rc = make_SBuffer( &( j -> looked_up_bases_1 ), 4096 );  /* helper.c */
        if ( 0 != rc ) {
            ErrMsg( "init_join().make_SBuffer( looked_up_bases_1 ) -> %R", rc );
        }
    }
    if ( 0 == rc ) {
        rc = make_SBuffer( &( j -> looked_up_bases_2 ), 4096 );  /* helper.c */
        if ( 0 != rc ) {
            ErrMsg( "init_join().make_SBuffer( looked_up_bases_2 ) -> %R", rc );
        }
    }

    /* we do not seek any more at the beginning of the begin of a join-slice
       the call to lookup_bases will perform an internal seek now if it is not pointed
       to the right location */

    if ( 0 != rc ) { dbj_release_cmn_data( j ); /* above! */ }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

/* read_id_0 means read-id zero based */
static bool dbj_is_reverse( const fq_seq_csra_rec_t * rec, uint32_t read_id_0 ) {
    bool res = false;
    if ( rec -> num_read_type > read_id_0 ) {
        res = ( ( rec -> read_type[ read_id_0 ] & READ_TYPE_REVERSE ) == READ_TYPE_REVERSE );
    }
    return res;
}

/* read_id_0 means read-id zero based */
static bool dbj_filter( join_stats_t * stats,
                        const fq_seq_csra_rec_t * rec,
                        const join_options_t * jo,
                        uint32_t read_id_0 ) {
    bool process = true;
    if ( jo -> skip_tech && rec -> num_read_type > read_id_0 ) {
        process = READ_TYPE_BIOLOGICAL == ( rec -> read_type[ read_id_0 ] & READ_TYPE_BIOLOGICAL );
        if ( !process ) { stats -> reads_technical++; }
    }
    if ( process && jo -> min_read_len > 0 ) {
        process = ( rec -> read_len[ read_id_0 ] >= jo -> min_read_len );
        if ( !process ) { stats -> reads_too_short++; }
    }
    return process;
}

static void dbj_slice_rec( const String * src, String * S1, String * S2, uint32_t * l ) {
    if ( NULL != S1 ) {
        S1 -> addr = src -> addr;
        S1 -> size = l[ 0 ];
        S1 -> len  = l[ 0 ];
    }
    if ( NULL != S2 ) {
        S2 -> addr = &( src -> addr[ l[ 0 ] ] );
        S2 -> size = l[ 1 ];
        S2 -> len  = l[ 1 ];
    }
}

static uint32_t dbj_calc_spot_len( const fq_seq_csra_rec_t * rec ) {
    uint32_t res = 0;
    uint32_t i;
    for ( i = 0; i < rec -> num_read_len; i++ ) {
        res += rec -> read_len[ i ];
    }
    return res;
}

static void dbj_slice_read( const fq_seq_csra_rec_t * rec, String * S1, String * S2 ) {
    dbj_slice_rec( &( rec -> read ), S1, S2, rec -> read_len );
}

static void dbj_slice_qual( const fq_seq_csra_rec_t * rec, String * S1, String * S2 ) {
    dbj_slice_rec( &( rec -> quality ), S1, S2, rec -> read_len );
}

static rc_t dbj_print_data( struct flp_t * printer,
                        const fq_seq_csra_rec_t * rec,
                        const join_options_t * jo,
                        uint32_t read_id,
                        uint32_t dst_id,
                        const String * R1,
                        const String * R2,
                        const String * Q ) {
    flp_data_t data;
    data . row_id = rec -> row_id;
    data . read_id = read_id;
    data . dst_id = dst_id;
    data . spotname = jo -> rowid_as_name ? NULL : &( rec -> name );
    data . spotgroup = &( rec -> spotgroup );
    data . read1 = R1;
    data . read2 = R2;
    data . quality = Q;
    return flp_print( printer, &data ); /* flex_printer.c */
}

static rc_t dbj_lookup1( dbj_cmn_t * j, const fq_seq_csra_rec_t * rec, const String ** res ) {
    bool reverse = dbj_is_reverse( rec, 0 );
    rc_t rc = lookup_bases( j -> lookup, rec -> row_id, 1, &j -> looked_up_bases_1, reverse ); /* lookup_reader.c */
    if ( 0 == rc ) {
        *res = &( j -> looked_up_bases_1 . S );
    }
    return rc;
}

static rc_t dbj_lookup2( dbj_cmn_t * j, const fq_seq_csra_rec_t * rec, const String ** res ) {
    bool reverse = dbj_is_reverse( rec, 1 );
    rc_t rc = lookup_bases( j -> lookup, rec -> row_id, 2, &j -> looked_up_bases_2, reverse ); /* lookup_reader.c */
    if ( 0 == rc ) {
        *res = &( j -> looked_up_bases_2 . S );
    }
    return rc;
}

typedef enum dbj_split_mode_t { sm_none, sm_file, sm_3 } dbj_split_mode_t;

/* ------------------------------------------------------------------------------------------ */

static rc_t dbj_print_fastq_1_read_unaligned( const fq_seq_csra_rec_t * rec,
                                dbj_cmn_t * j,
                                dbj_split_mode_t sm ) {
    /* read is unaligned, print what is in rec -> cmp_read ( no lookup ) */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    if ( hlp_filter_2na_1( j -> filter, CMP_READ ) ) {
        const String * QUALITY = &( rec -> quality );
        if ( CMP_READ -> len != QUALITY -> len ) {
            ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (A)\n", rec -> row_id,
                    CMP_READ -> len, QUALITY -> len );
            j -> stats -> reads_invalid++;
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
        }
        if ( CMP_READ -> len > 0 ) {
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                             /* read_id */ 1,
                             /* dst_id */ ( sm_file == sm ) ? 1 : 0,
                             /* R1 */ CMP_READ,
                             /* R2 */ NULL,
                             /* Q  */ QUALITY );
            if ( 0 == rc ) { j -> stats -> reads_written++; }
        }
    }
    return rc;
}

static rc_t dbj_print_fastq_1_read_aligned( const fq_seq_csra_rec_t * rec,
                                dbj_cmn_t * j,
                                dbj_split_mode_t sm ) {
    /* read is aligned, ( 1 lookup ) */
    const String * LOOKED_UP = NULL;
    rc_t rc = dbj_lookup1( j, rec, &LOOKED_UP );
    if ( 0 == rc ) {
        if ( hlp_filter_2na_1( j -> filter, LOOKED_UP ) ) {
            const String * QUALITY = &( rec -> quality );
            if ( LOOKED_UP -> len != QUALITY -> len ) {
                ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (B)\n", rec -> row_id,
                            LOOKED_UP -> len, QUALITY -> len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
            if ( LOOKED_UP -> len > 0 ) {
                rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                                /* read_id */ 1,
                                 /* dst_id */ ( sm_file == sm ) ? 1 : 0,
                                /* R1 */ LOOKED_UP,
                                /* R2 */ NULL,
                                /* Q  */ QUALITY );
                if ( 0 == rc ) { j -> stats -> reads_written++; }
            }
        }
    }
    return rc;
}

static rc_t dbj_print_fastq_1_read( const fq_seq_csra_rec_t * rec,
                                dbj_cmn_t * j,
                                dbj_split_mode_t sm ) {
    rc_t rc = 0;
    bool process = dbj_filter( j -> stats, rec, j -> join_options, 0 );
    if ( process ) {
        if ( 0 == rec -> prim_alig_id[ 0 ] ) {
            rc = dbj_print_fastq_1_read_unaligned( rec, j, sm );
        } else {
            rc = dbj_print_fastq_1_read_aligned( rec, j, sm );
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_1_read_unaligned( const fq_seq_csra_rec_t * rec,
                                dbj_cmn_t * j,
                                dbj_split_mode_t sm ) {
    /* read is unaligned, print what is in rec -> cmp_read ( no lookup ) */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    if ( hlp_filter_2na_1( j -> filter, CMP_READ ) ) {
        if ( CMP_READ -> len > 0 ) {
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                            /* read_id */ 1,
                            /* dst_id */ sm == sm_file ? 1 : 0,
                            /* R1 */ CMP_READ,
                            /* R2 */ NULL,
                            /* Q  */ NULL );
            if ( 0 == rc ) { j -> stats -> reads_written++; }
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_1_read_aligned( const fq_seq_csra_rec_t * rec,
                                dbj_cmn_t * j,
                                dbj_split_mode_t sm ) {
    /* read is aligned, ( 1 lookup ) */
    const String * LOOKED_UP = NULL;
    rc_t rc = dbj_lookup1( j, rec, &LOOKED_UP );
    if ( 0 == rc ) {
        if ( hlp_filter_2na_1( j -> filter, LOOKED_UP ) ) {
            if ( LOOKED_UP -> len > 0 ) {
                rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                                /* read_id */ 1,
                                /* dst_id */ sm == sm_file ? 1 : 0,
                                /* R1 */ LOOKED_UP,
                                /* R2 */ NULL,
                                /* Q  */ NULL );
                if ( 0 == rc ) { j -> stats -> reads_written++; }
            }
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_1_read( const fq_seq_csra_rec_t * rec,
                                dbj_cmn_t * j,
                                dbj_split_mode_t sm ) {
    rc_t rc = 0;
    bool process = dbj_filter( j -> stats, rec, j -> join_options, 0 );
    if ( process ) {
        if ( 0 == rec -> prim_alig_id[ 0 ] ) {
            rc = dbj_print_fasta_1_read_unaligned( rec, j, sm );
        } else {
            rc = dbj_print_fasta_1_read_aligned( rec, j, sm );
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t dbj_print_fastq_whole_spot_unaligned( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* >>>>> FULLY UNALIGNED <<<<<
        print what is in rec -> read ( no lookups! ) */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    if ( hlp_filter_2na_1( j -> filter, CMP_READ ) ) {
        const String * QUALITY = &( rec -> quality );
        if ( CMP_READ -> len != QUALITY -> len ) {
            ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (C)\n", rec -> row_id,
                    CMP_READ -> len, QUALITY -> len );
            j -> stats -> reads_invalid++;
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
        }
        rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                        /* read_id */ 1,
                         /* dst_id */ 0,
                        /* R1 */ CMP_READ,
                        /* R2 */ NULL,
                        /* Q  */ QUALITY );
        if ( 0 == rc ) { j -> stats -> reads_written += 2; }
    }
    return rc;
}

static rc_t dbj_print_fastq_whole_spot_half_aligned_1( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* >>>>> HALF ALIGNED <<<<<
        A0 is unaligned ( read and quality from rec )
        A1 is aligned   ( read form j -> lookup, quality from rec ) */
    const String * LOOKED_UP = NULL;
    rc_t rc = dbj_lookup2( j, rec, &LOOKED_UP );
    if ( 0 == rc ) {
        const String * CMP_READ = &( rec -> read );
        const String * QUALITY  = &( rec -> quality );
        String sliced;
        /* test for special case: CMP_READ contains BOTH halfs: */
        if ( CMP_READ -> len == QUALITY -> len ) {
            dbj_slice_read( rec, &sliced, NULL );
            CMP_READ = &sliced;
        }
        if ( hlp_filter_2na_2( j -> filter, CMP_READ, LOOKED_UP ) ) {
            uint32_t combined_len = CMP_READ -> len + LOOKED_UP -> len;
            if ( combined_len != QUALITY -> len ) {
                ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (D)\n", rec -> row_id,
                        combined_len, QUALITY -> len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                            /* read_id */ 1,
                            /* dst_id */ 0,
                            /* R1 */ CMP_READ,
                            /* R2 */ LOOKED_UP,
                            /* Q  */ QUALITY );
            if ( 0 == rc ) { j -> stats -> reads_written += 2; }
        }
    }
    return rc;
}

static rc_t dbj_print_fastq_whole_spot_half_aligned_2( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* >>>>> HALF ALIGNED <<<<<
        A0 is aligned ( read and quality from rec )
        A1 is unaligned ( read form lookup quality from rec */
    const String * LOOKED_UP = NULL;
    rc_t rc = dbj_lookup1( j, rec, &LOOKED_UP );
    if ( 0 == rc ) {
        const String * CMP_READ = &( rec -> read );
        const String * QUALITY  = &( rec -> quality );
        String sliced;
        /* test for special case: CMP_READ contains BOTH halfs: */
        if ( CMP_READ -> len == QUALITY -> len ) {
            dbj_slice_read( rec, NULL, &sliced );
            CMP_READ = &sliced;
        }
        if ( hlp_filter_2na_2( j -> filter, LOOKED_UP, CMP_READ ) ) {
            uint32_t combined_len = LOOKED_UP -> len + CMP_READ -> len;
            if ( combined_len != QUALITY -> len ) {
                ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (E)\n", rec -> row_id,
                        combined_len, QUALITY -> len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                            /* read_id */ 1,
                            /* dst_id */ 0,
                            /* R1 */ LOOKED_UP,
                            /* R2 */ CMP_READ,
                            /* Q  */ QUALITY );
            if ( 0 == rc ) { j -> stats -> reads_written += 2; }
        }
    }
    return rc;
}

static rc_t dbj_print_fastq_whole_spot_aligned( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* >>>>> FULLY ALIGNED <<<<<  2 lookups! */
    const String * LOOKED_UP1 = NULL;
    const String * LOOKED_UP2 = NULL;
    rc_t rc = dbj_lookup1( j, rec, &LOOKED_UP1 );
    if ( 0 == rc ) {
        rc = dbj_lookup2( j, rec, &LOOKED_UP2 );
    }
    if ( 0 == rc ) {
        if ( hlp_filter_2na_2( j -> filter, LOOKED_UP1, LOOKED_UP2 ) ) {
            const String * QUALITY = &( rec -> quality );
            uint32_t combined_len = LOOKED_UP1 -> len + LOOKED_UP2 -> len;
            if ( combined_len != QUALITY -> len ) {
                ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (F)\n", rec -> row_id,
                        combined_len, QUALITY -> len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                            /* read_id */ 1,
                            /* dst_id */ 0,
                            /* R1 */ LOOKED_UP1,
                            /* R2 */ LOOKED_UP2,
                            /* Q  */ QUALITY );
            if ( 0 == rc ) { j -> stats -> reads_written += 2; }
        }
    }
    return rc;
}

static rc_t dbj_print_fastq_whole_spot( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    rc_t rc = 0;
    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fastq_whole_spot_unaligned( rec, j );
        } else {
            rc = dbj_print_fastq_whole_spot_half_aligned_1( rec, j );
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fastq_whole_spot_half_aligned_2( rec, j );
        } else {
            rc = dbj_print_fastq_whole_spot_aligned( rec, j );
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_whole_spot_unaligned( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* both unaligned, print what is in row->read ( no lookup )
        also no check for READ.len == QUAL.len, because FASTA, aka no QUALITY */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    if ( hlp_filter_2na_1( j -> filter, CMP_READ ) ) {
        rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                        /* read_id */ 1,
                        /* dst_id */ 0,
                        /* R1 */ CMP_READ,
                        /* R2 */ NULL,
                        /* Q  */ NULL );
        if ( 0 == rc ) { j -> stats -> reads_written += 2; }
    }
    return rc;
}

static rc_t dbj_print_fasta_whole_spot_half_aligned_1( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* A0 is unaligned / A1 is aligned (lookup) */
    const String * LOOKED_UP = NULL;
    rc_t rc = dbj_lookup2( j, rec, &LOOKED_UP );
    if ( 0 == rc ) {
        const String * CMP_READ = &( rec -> read );
        String sliced;
        /* test for special case: CMP_READ contains BOTH halfs: */
        if ( CMP_READ -> len == dbj_calc_spot_len( rec ) ) {
            dbj_slice_read( rec, &sliced, NULL );
            CMP_READ = &sliced;
        }
        if ( hlp_filter_2na_2( j -> filter, CMP_READ, LOOKED_UP ) ) {
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                            /* read_id */ 1,
                            /* dst_id */ 0,
                            /* R1 */ CMP_READ,
                            /* R2 */ LOOKED_UP,
                            /* Q  */ NULL );
            if ( 0 == rc ) { j -> stats -> reads_written += 2; }
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_whole_spot_half_aligned_2( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* A0 is aligned (lookup) / A1 is unaligned */
    const String * LOOKED_UP = NULL;

    rc_t rc = dbj_lookup1( j, rec, &LOOKED_UP );
    if ( 0 == rc ) {
        const String * CMP_READ = &( rec -> read );
        String sliced;
        /* test for special case: CMP_READ contains BOTH halfs: */
        if ( CMP_READ -> len == dbj_calc_spot_len( rec ) ) {
            dbj_slice_read( rec, NULL, &sliced );
            CMP_READ = &sliced;
        }
        if ( hlp_filter_2na_2( j -> filter, LOOKED_UP, CMP_READ ) ) {
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                            /* read_id */ 1,
                            /* dst_id */ 0,
                            /* R1 */ LOOKED_UP,
                            /* R2 */ CMP_READ,
                            /* Q  */ NULL );
            if ( 0 == rc ) { j -> stats -> reads_written += 2; }
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_whole_spot_aligned( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    /* A0 and A1 are aligned ( 2 lookups ) */
    const String * LOOKED_UP1 = NULL;
    const String * LOOKED_UP2 = NULL;
    rc_t rc = dbj_lookup1( j, rec, &LOOKED_UP1 );
    if ( 0 == rc ) {
        rc = dbj_lookup2( j, rec, &LOOKED_UP2 );
    }
    if ( 0 == rc ) {
        if ( hlp_filter_2na_2( j -> filter, LOOKED_UP1, LOOKED_UP2 ) ) {
            rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                            /* read_id */ 1,
                            /* dst_id */ 0,
                            /* R1 */ LOOKED_UP1,
                            /* R2 */ LOOKED_UP2,
                            /* Q  */ NULL );
            if ( 0 == rc ) { j -> stats -> reads_written += 2; }
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_whole_spot( const fq_seq_csra_rec_t * rec,
                                 dbj_cmn_t * j ) {
    rc_t rc = 0;
    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fasta_whole_spot_unaligned( rec, j );         /* UU */
        } else {
            rc = dbj_print_fasta_whole_spot_half_aligned_1( rec, j );    /* UA */
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fasta_whole_spot_half_aligned_2( rec, j );    /* AU */
        } else {
            rc = dbj_print_fasta_whole_spot_aligned( rec, j );           /* AA */
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static uint32_t dbj_calc_dst_id_1( dbj_split_mode_t sm, bool process_0, bool process_1 ) {
    switch ( sm ) {
        case sm_none  : return 0; break;
        case sm_file  : return 1; break;
        case sm_3 : return ( process_0 && process_1 ) ? 1 : 0; break;
    }
    return 0;
}

static uint32_t dbj_calc_dst_id_2( dbj_split_mode_t sm, bool process_0, bool process_1 ) {
    switch ( sm ) {
        case sm_none  : return 0; break;
        case sm_file  : return 2; break;
        case sm_3 : return ( process_0 && process_1 ) ? 2 : 0; break;
    }
    return 0;
}

static rc_t dbj_print_fastq_splitted_cmn( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1,
                                  const String * R1,
                                  const String * R2,
                                  const String * Q1,
                                  const String * Q2 ) {
    rc_t rc = 0;
    process_0 = process_0 && R1 -> len > 0 &&
                hlp_filter_2na_1( j -> filter, R1 );
    process_1 = process_1 && R2 -> len > 0 &&
                hlp_filter_2na_1( j -> filter, R2 );
    /* both unaligned, print what is in row -> cmp_read ( no lookup ) */
    if ( process_0 ) {
        rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                         /* read_id */ 1,
                         /* dst_id */ dbj_calc_dst_id_1( sm, process_0, process_1 ),
                         /* R1 */ R1,
                         /* R2 */ NULL,
                        /* Q  */ Q1 );
        if ( 0 == rc ) { j -> stats -> reads_written ++;  }
    }
    if ( process_1 && 0 == rc ) {
        rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                         /* read_id */ 2,
                         /* dst_id */ dbj_calc_dst_id_2( sm, process_0, process_1 ),
                         /* R1 */ R2,
                         /* R2 */ NULL,
                         /* Q  */ Q2 );
        if ( 0 == rc ) { j -> stats -> reads_written ++; }
    }
    return rc;
}

static rc_t dbj_print_fastq_splitted_unaligned( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1,
                                  const String * Q1,
                                  const String * Q2 ) {
    /* fully unaligned */
    String CMP_READ1, CMP_READ2;
    dbj_slice_read( rec, &CMP_READ1, &CMP_READ2 );
    if ( process_0 ) {
        if ( CMP_READ1 . len != Q1 -> len ) {
            ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", rec -> row_id, CMP_READ1 . len, Q1 -> len );
            j -> stats -> reads_invalid++;
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
        }
    }
    if ( process_1 ) {
        if ( CMP_READ2 . len != Q2 -> len ) {
            ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", rec -> row_id, CMP_READ2 . len, Q2 -> len );
            j -> stats -> reads_invalid++;
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
        }
    }
    return dbj_print_fastq_splitted_cmn( rec, j, sm, process_0, process_1,
                                   &CMP_READ1, &CMP_READ2, Q1, Q2 );
}

static rc_t dbj_print_fastq_splitted_half_aligned_1( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1,
                                  const String * Q1,
                                  const String * Q2 ) {
    /* A0 is unaligned / A1 is aligned (lookup) */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    const String * LOOKED_UP = NULL;
    String sliced;
    uint32_t spot_len = dbj_calc_spot_len( rec );
    /* special case: CMP_READ contains both! */
    if ( CMP_READ -> len == spot_len ) {
        dbj_slice_read( rec, &sliced, NULL );
        CMP_READ = &sliced;
    }
    if ( process_0 ) {
        if ( CMP_READ -> len != Q1 -> len ) {
            ErrMsg( "fastq.splitted.half1 row #%ld : CMP_READ.len(%u) != Q1.len(%u) / spotlen=%u\n",
                    rec -> row_id, CMP_READ -> len, Q1 -> len, spot_len );
            j -> stats -> reads_invalid++;
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
        }
    }
    if ( process_1 ) {
        rc = dbj_lookup2( j, rec, &LOOKED_UP );
        if ( 0 == rc ) {
            if ( LOOKED_UP -> len != Q2 -> len ) {
                ErrMsg( "fastq.splitted.half1 row #%ld : LOOKED_UP.len(%u) != Q2.len(%u) / spotlen=%u\n",
                        rec -> row_id, LOOKED_UP -> len, Q2 -> len, spot_len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
        }
    }
    if ( 0 == rc ) {
        rc = dbj_print_fastq_splitted_cmn( rec, j, sm, process_0, process_1,
                                        CMP_READ, LOOKED_UP, Q1, Q2 );
    }
    return rc;
}

static rc_t dbj_print_fastq_splitted_half_aligned_2( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1,
                                  const String * Q1,
                                  const String * Q2 ) {
    /* A0 is aligned (lookup) / A1 is unaligned */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    const String * LOOKED_UP = NULL;
    String sliced;
    uint32_t spot_len = dbj_calc_spot_len( rec );
    /* special case: CMP_READ contains both! */
    if ( CMP_READ -> len == spot_len ) {
        dbj_slice_read( rec, NULL, &sliced );
        CMP_READ = &sliced;
    }
    if ( process_0 ) {
        rc = dbj_lookup1( j, rec, &LOOKED_UP );
        if ( 0 == rc ) {
            if ( LOOKED_UP -> len != Q1 -> len ) {
                ErrMsg( "fastq.splitted.half2 row #%ld : LOOKED_UP.len(%u) != Q1.len(%u) / spotlen=%u\n",
                        rec -> row_id, LOOKED_UP -> len, Q1 -> len, spot_len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
        }
    }
    if ( process_1 ) {
        if ( CMP_READ -> len != Q2 -> len ) {
            ErrMsg( "fastq.splitted.half2 row #%ld : CMP_READ.len(%u) != Q2.len(%u) / spotlen=%u\n",
                    rec -> row_id, CMP_READ -> len, Q2 -> len, spot_len );
            j -> stats -> reads_invalid++;
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
        }
    }
    if ( 0 == rc ) {
        rc = dbj_print_fastq_splitted_cmn( rec, j, sm, process_0, process_1,
                                        LOOKED_UP, CMP_READ, Q1, Q2 );
    }
    return rc;
}

static rc_t dbj_print_fastq_splitted_aligned( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1,
                                  const String * Q1,
                                  const String * Q2 ) {
    /* A0 and A1 are aligned (2 lookups) */
    rc_t rc = 0;
    const String * LOOKED_UP1 = NULL;
    const String * LOOKED_UP2 = NULL;
    if ( process_0 ) {
        rc = dbj_lookup1( j, rec, &LOOKED_UP1 );
        if ( 0 == rc ) {
            if ( LOOKED_UP1 -> len != Q1 -> len ) {
                ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", rec -> row_id, LOOKED_UP1 -> len, Q1 -> len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
        }
    }
    if ( 0 == rc && process_1 ) {
        rc = dbj_lookup2( j, rec, &LOOKED_UP2 );
        if ( 0 == rc ) {
            if ( LOOKED_UP2 -> len != Q2 -> len ) {
                ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", rec -> row_id, LOOKED_UP2 -> len, Q2 -> len );
                j -> stats -> reads_invalid++;
                return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
            }
        }
    }
    if ( 0 == rc ) {
        rc = dbj_print_fastq_splitted_cmn( rec, j, sm, process_0, process_1,
                                        LOOKED_UP1, LOOKED_UP2, Q1, Q2 );
    }
    return rc;
}

static rc_t dbj_print_fastq_splitted( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm ) {
    rc_t rc = 0;
    String Q1, Q2;
    bool process_0 = true;
    bool process_1 = true;

    if ( sm != sm_none && j -> join_options -> skip_tech ) {
        process_0 = dbj_filter( j -> stats, rec, j -> join_options, 0 );
        process_1 = dbj_filter( j -> stats, rec, j -> join_options, 1 );
    }

    /* slice the quality rec -> quality ---> Q1, Q2 */
    dbj_slice_qual( rec, &Q1, &Q2 );

    /* check if the 2 slices of quality add up to the available qualities */
    if ( ( Q1 . len + Q2 . len ) != rec -> quality . len ) {
        ErrMsg( "row #%ld : Q[1].len(%u) + Q[2].len(%u) != Q.len(%u)\n",
                rec -> row_id, Q1 . len, Q2 . len, rec -> quality . len );
        j -> stats -> reads_invalid++;
        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
    }

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fastq_splitted_unaligned( rec, j, sm,
                                                 process_0, process_1, &Q1, &Q2 );
        } else {
            rc = dbj_print_fastq_splitted_half_aligned_1( rec, j, sm,
                                                 process_0, process_1, &Q1, &Q2 );
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fastq_splitted_half_aligned_2( rec, j, sm,
                                                 process_0, process_1, &Q1, &Q2 );
        } else {
            rc = dbj_print_fastq_splitted_aligned( rec, j, sm,
                                                process_0, process_1, &Q1, &Q2 );
        }
    }
    return rc;
}

static rc_t dbj_print_fasta_splitted_cmn( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1,
                                  const String * R1,
                                  const String * R2 ) {
    rc_t rc = 0;
    process_0 = process_0 && R1 -> len > 0 && hlp_filter_2na_1( j -> filter, R1 );
    process_1 = process_1 && R2 -> len > 0 && hlp_filter_2na_1( j -> filter, R2 );
    /* both unaligned, print what is in row -> cmp_read ( no lookup ) */
    if ( process_0 ) {
        rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                         /* read_id */ 1,
                         /* dst_id */ dbj_calc_dst_id_1( sm, process_0, process_1 ),
                         /* R1 */ R1,
                         /* R2 */ NULL,
                         /* Q  */ NULL );
        if ( 0 == rc ) { j -> stats -> reads_written ++; }
    }
    if ( process_1 && 0 == rc ) {
        rc = dbj_print_data( j -> flex_printer, rec, j -> join_options,
                         /* read_id */ 2,
                         /* dst_id */ dbj_calc_dst_id_2( sm, process_0, process_1 ),
                         /* R1 */ R2,
                         /* R2 */ NULL,
                         /* Q  */ NULL );
            if ( 0 == rc ) { j -> stats -> reads_written ++; }
    }
    return rc;
}

static rc_t dbj_print_fasta_splitted_unaligned( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1 ) {
    /* fully unaligned */
    String CMP_READ1;
    String CMP_READ2;
    dbj_slice_read( rec, &CMP_READ1, &CMP_READ2 );
    return dbj_print_fasta_splitted_cmn( rec, j, sm, process_0, process_1,
                                     &CMP_READ1, &CMP_READ2 );
}

static rc_t dbj_print_fasta_splitted_half_aligned_1( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1 ) {
    /* A0 is unaligned / A1 is aligned (lookup) */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    const String * LOOKED_UP = NULL;
    String sliced;
    /* test for special case: CMP_READ contains BOTH halfs: */
    if ( CMP_READ -> len == dbj_calc_spot_len( rec ) ) {
        dbj_slice_read( rec, &sliced, NULL );
        CMP_READ = &sliced;
    }
    if ( process_1 ) { rc = dbj_lookup2( j, rec, &LOOKED_UP ); }
    if ( 0 == rc ) {
        rc = dbj_print_fasta_splitted_cmn( rec, j, sm, process_0, process_1,
                                       CMP_READ, LOOKED_UP );
    }
    return rc;
}

static rc_t dbj_print_fasta_splitted_half_aligned_2( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1 ) {
    /* A0 is aligned (lookup) / A1 is unaligned */
    rc_t rc = 0;
    const String * CMP_READ = &( rec -> read );
    const String * LOOKED_UP = NULL;
    String sliced;
    /* test for special case: CMP_READ contains BOTH halfs: */
    if ( CMP_READ -> len == dbj_calc_spot_len( rec ) ) {
        dbj_slice_read( rec, NULL, &sliced );
        CMP_READ = &sliced;
    }
    if ( process_0 ) { rc = dbj_lookup1( j, rec, &LOOKED_UP ); }
    if ( 0 == rc ) {
        rc = dbj_print_fasta_splitted_cmn( rec, j, sm, process_0, process_1,
                                       LOOKED_UP, CMP_READ );
    }
    return rc;
}

static rc_t dbj_print_fasta_splitted_aligned( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm,
                                  bool process_0,
                                  bool process_1 ) {
    /* A0 and A1 are aligned (2 lookups)*/
    rc_t rc = 0;
    const String * LOOKED_UP1 = NULL;
    const String * LOOKED_UP2 = NULL;
    if ( process_0 ) {
        rc = dbj_lookup1( j, rec, &LOOKED_UP1 );
    }
    if ( 0 == rc && process_1 ) {
        rc = dbj_lookup2( j, rec, &LOOKED_UP2 );
    }
    if ( 0 == rc ) {
        rc = dbj_print_fasta_splitted_cmn( rec, j, sm, process_0, process_1,
                                       LOOKED_UP1, LOOKED_UP2 );
    }
    return rc;
}

static rc_t dbj_print_fasta_splitted( const fq_seq_csra_rec_t * rec,
                                  dbj_cmn_t * j,
                                  dbj_split_mode_t sm ) {
    rc_t rc = 0;
    bool process_0 = true;
    bool process_1 = true;

    if ( sm != sm_none && j -> join_options -> skip_tech ) {
        process_0 = dbj_filter( j -> stats, rec, j -> join_options, 0 );
        process_1 = dbj_filter( j -> stats, rec, j -> join_options, 1 );
    }

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fasta_splitted_unaligned( rec, j, sm,
                                                 process_0, process_1 );
        } else {
            rc = dbj_print_fasta_splitted_half_aligned_1( rec, j, sm,
                                                 process_0, process_1 );
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            rc = dbj_print_fasta_splitted_half_aligned_2( rec, j, sm,
                                                 process_0, process_1 );
        } else {
            rc = dbj_print_fasta_splitted_aligned( rec, j, sm,
                                                 process_0, process_1 );
        }
    }
    return rc;
}

static rc_t dbj_perform_fastq_whole_spot_join( cmn_iter_params_t * cp,
                                     dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    opt . with_read_len = false;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;
    opt . with_spotgroup = j -> join_options -> print_spotgroup;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        join_options_t local_opt = { j -> join_options -> rowid_as_name,
                                   false,
                                   j -> join_options -> print_spotgroup,
                                   j -> join_options -> min_read_len,
                                   j -> join_options -> filter_bases };
        j -> join_options = &local_opt;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc ) ) {
            rc = hlp_get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fastq_1_read( &rec, j, sm_none );
                } else {
                    rc = dbj_print_fastq_whole_spot( &rec, j );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );
    }
    return rc;
}

static rc_t dbj_perform_fastq_split_spot_join( cmn_iter_params_t * cp,
                                      dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;
    opt . with_spotgroup = j -> join_options -> print_spotgroup;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_spot_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc ) ) {
            rc = hlp_get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fastq_1_read( &rec, j, sm_none );
                } else {
                    rc = dbj_print_fastq_splitted( &rec, j, sm_none );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );
    }
    return rc;
}

static rc_t dbj_perform_fastq_split_file_join( cmn_iter_params_t * cp,
                                      dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_file_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        join_options_t local_opt = {
                j -> join_options -> rowid_as_name,
                false,
                j -> join_options -> print_spotgroup,
                j -> join_options -> min_read_len,
                j -> join_options -> filter_bases
            };
        j -> join_options = & local_opt;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc ) ) {
            rc = hlp_get_quitting();
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fastq_1_read( &rec, j, sm_file );
                } else {
                    rc = dbj_print_fastq_splitted( &rec, j, sm_file );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld",
                            j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );
    }
    return rc;
}

static rc_t dbj_perform_fastq_split_3_join( cmn_iter_params_t * cp,
                                      dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;
    opt . with_spotgroup = j -> join_options -> print_spotgroup;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_3_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        rc_t rc_iter = 0;
        join_options_t local_opt = {
            j -> join_options -> rowid_as_name,
                false,
                j -> join_options -> print_spotgroup,
                j -> join_options -> min_read_len,
                j -> join_options -> filter_bases
            };
        j -> join_options = &local_opt;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc_iter ) && 0 == rc_iter ) {
            rc = hlp_get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fastq_1_read( &rec, j, sm_3 );
                } else {
                    rc = dbj_print_fastq_splitted( &rec, j, sm_3 );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld",
                            j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
    }
    return rc;
}

static rc_t dbj_perform_fasta_whole_spot_join( cmn_iter_params_t * cp,
                                dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;
    opt . with_spotgroup = j -> join_options -> print_spotgroup;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        join_options_t local_opt = { j -> join_options -> rowid_as_name,
                                   false,
                                   j -> join_options -> print_spotgroup,
                                   j -> join_options -> min_read_len,
                                   j -> join_options -> filter_bases };
        j -> join_options = & local_opt;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc ) ) {
            rc = hlp_get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fasta_1_read( &rec, j, sm_none );
                } else {
                    rc = dbj_print_fasta_whole_spot( &rec, j );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld",
                            j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );
    }
    return rc;
}

static rc_t dbj_perform_fasta_split_spot_join( cmn_iter_params_t * cp,
                                      dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;
    opt . with_spotgroup = j -> join_options -> print_spotgroup;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_spot_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc ) ) {
            rc = hlp_get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fasta_1_read( &rec, j, sm_none );
                } else {
                    rc = dbj_print_fasta_splitted( &rec, j, sm_none );
                }
                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld",
                            j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );
    }
    return rc;
}

static rc_t dbj_perform_fasta_split_file_join( cmn_iter_params_t * cp,
                                      dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;

    opt . with_read_len = true;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;
    opt . with_spotgroup = j -> join_options -> print_spotgroup;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_file_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        join_options_t local_opt = { j -> join_options -> rowid_as_name,
                                    false,
                                    j -> join_options -> print_spotgroup,
                                    j -> join_options -> min_read_len,
                                    j -> join_options -> filter_bases
                                };
        j -> join_options = &local_opt;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc ) ) {
            rc = hlp_get_quitting();
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fasta_1_read( &rec, j, sm_file );
                } else {
                    rc = dbj_print_fasta_splitted( &rec, j, sm_file );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld",
                            j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );
    }
    return rc;
}

static rc_t dbj_perform_fasta_split_3_join( cmn_iter_params_t * cp,
                                      dbj_cmn_t * j ) {
    rc_t rc;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    
    opt . with_read_len = true;
    opt . with_name = !( j -> join_options -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;
    opt . with_spotgroup = j -> join_options -> print_spotgroup;

    rc = fq_seq_csra_iter_make( cp, opt, &iter );
    if ( 0 != rc ) {
        ErrMsg( "perform_fasta_split_3_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fq_seq_csra_rec_t rec;
        rc_t rc_iter = 0;

        join_options_t local_opt = { j -> join_options -> rowid_as_name,
                                    false,
                                    j -> join_options -> print_spotgroup,
                                    j -> join_options -> min_read_len,
                                    j -> join_options -> filter_bases
                                };
        j -> join_options = &local_opt;
        while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc_iter ) && 0 == rc_iter ) {
            rc = hlp_get_quitting();
            if ( 0 == rc ) {
                j -> stats -> spots_read++;
                j -> stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = dbj_print_fasta_1_read( &rec, j, sm_3 );
                } else {
                    rc = dbj_print_fasta_splitted( &rec, j, sm_3 );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld",
                            j -> thread_id, j -> loop_nr, rec . row_id );
                    hlp_set_quitting();
                }
                bg_progress_inc( j -> progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        fq_seq_csra_iter_release( iter );

        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

typedef struct dbj_thread_data_t {
    char part_file[ 4096 ];

    join_stats_t stats; /* helper.h */

    KDirectory * dir;
    const VDBManager * vdb_mgr;

    const char * accession_path;
    const char * accession_short;
    const char * lookup_filename;
    const char * index_filename;
    const char * seq_defline;
    const char * qual_defline;
    struct bg_progress_t * progress;
    struct temp_registry_t * registry;
    struct filter_2na_t * filter;

    KThread * thread;

    int64_t first_row;
    uint64_t row_count;
    uint64_t row_limit;
    size_t cur_cache;
    size_t buf_size;
    format_t fmt;
    uint32_t thread_id;
    bool cmp_read_present;

    const join_options_t * join_options;
    struct multi_writer_t * multi_writer;
} dbj_thread_data_t;

static rc_t CC dbj_sorted_thread( const KThread * self, void * data ) {
    rc_t rc = 0;
    dbj_thread_data_t * jtd = data;
    const join_options_t * jo = jtd -> join_options;
    struct filter_2na_t * filter = hlp_make_2na_filter( jo -> filter_bases ); /* helper.c */
    struct flp_t * flex_printer = NULL;
    flp_args_t file_args;
    
    flp_initialize_args( &file_args,
                         jtd -> dir,
                         jtd -> registry,
                         jtd -> part_file,
                         jtd -> buf_size );
    /* make_flex_printer() is in flex_printer.c */
    flex_printer = flp_create_1( &file_args,
                jtd -> accession_short,             /* we need that for the flexible defline! */
                jtd -> seq_defline,                 /* the seq-defline */
                jtd -> qual_defline,                /* the qual-defline */
                hlp_is_format_fasta( jtd -> fmt ) );    /* fasta-mode */
    if ( 0 == rc && NULL != flex_printer ) {
        dbj_cmn_t j;
        cmn_iter_params_t cp;
        cmn_iter_populate_params( &cp,
                                  jtd -> dir,
                                  jtd -> vdb_mgr,
                                  jtd -> accession_short,
                                  jtd -> accession_path,
                                  jtd -> cur_cache,
                                  jtd -> first_row,
                                  jtd -> row_limit > 0 ? jtd -> row_limit : jtd -> row_count );
        rc = dbj_init_cmn_data( &j,
                        &jtd -> stats,
                        jtd -> join_options,
                        jtd -> progress,
                        &cp,
                        flex_printer,
                        filter,
                        jtd -> lookup_filename,
                        jtd -> index_filename,
                        jtd -> buf_size,
                        jtd -> cmp_read_present );
        if ( 0 == rc ) {
            j . thread_id = jtd -> thread_id;

            switch ( jtd -> fmt ) {
                case ft_fastq_whole_spot    : rc = dbj_perform_fastq_whole_spot_join( &cp, &j ); break;
                case ft_fastq_split_spot    : rc = dbj_perform_fastq_split_spot_join( &cp, &j ); break;
                case ft_fastq_split_file    : rc = dbj_perform_fastq_split_file_join( &cp, &j ); break;
                case ft_fastq_split_3       : rc = dbj_perform_fastq_split_3_join( &cp, &j ); break;

                case ft_fasta_whole_spot    : rc = dbj_perform_fasta_whole_spot_join( &cp, &j ); break;
                case ft_fasta_split_spot    : rc = dbj_perform_fasta_split_spot_join( &cp, &j ); break;
                case ft_fasta_split_file    : rc = dbj_perform_fasta_split_file_join( &cp, &j ); break;
                case ft_fasta_split_3       : rc = dbj_perform_fasta_split_3_join( &cp, &j ); break;

                case ft_unknown : break;                /* this should never happen */
                case ft_fasta_us_split_spot : break;    /* neither should this */
                case ft_fasta_ref_tbl : break;          /* or this */
                case ft_fasta_concat : break;           /* or this */                    
                case ft_ref_report : break;             /* or this */                    
            }
            dbj_release_cmn_data( &j );
        }
        flp_release( flex_printer ); /* flex_printer.c */
    }
    hlp_release_2na_filter( filter );   /* helper.c */
    return rc;
}

static rc_t dbj_collect_threads_and_stats( Vector * threads, join_stats_t * stats ) {
    rc_t rc = 0;
    /* collect the threads, and add the join_stats */
    uint32_t i, n = VectorLength( threads );
    for ( i = VectorStart( threads ); i < n; ++i ) {
        dbj_thread_data_t * jtd = VectorGet( threads, i );
        if ( NULL != jtd ) {
            rc_t rc_thread;
            KThreadWait( jtd -> thread, &rc_thread );
            if ( 0 != rc_thread ) { rc = rc_thread; }
            KThreadRelease( jtd -> thread );
            hlp_add_join_stats( stats, &jtd -> stats ); /* helper.c */
            free( jtd );
        }
    }
    VectorWhack ( threads, NULL, NULL );
    return rc;
}

rc_t dbj_create_sorted_fastq_fasta( const dbj_sorted_fastq_fasta_args_t * args ) {
    rc_t rc = 0;

    if ( args -> show_progress ) {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "join   :" );
        KOutHandlerSetStdOut();
    }

    if ( rc == 0 ) {
        uint64_t seq_row_count = args -> insp_output -> seq . row_count;
        bool name_column_present, cmp_read_column_present;
        const char * seq_tbl_name = args -> insp_output -> seq . tbl_name;

        rc = cmn_iter_check_db_column( args -> dir, args -> vdb_mgr, args -> accession_short,
                                  args -> accession_path, seq_tbl_name,
                                  "NAME", &name_column_present ); /* cmn_iter.c */
        if ( 0 == rc ) {
            rc = cmn_iter_check_db_column( args -> dir, args -> vdb_mgr, args -> accession_short,
                                      args -> accession_path, seq_tbl_name,
                                      "CMP_READ", &cmp_read_column_present ); /* cmn_iter.c */
        }

        if ( 0 == rc && seq_row_count > 0 ) {
            Vector threads;
            int64_t row = 1;
            uint64_t rows_per_thread;
            uint32_t thread_id;
            uint32_t num_threads2 = args -> num_threads;

            struct bg_progress_t * progress = NULL;
            join_options_t corrected_join_options;

            hlp_correct_join_options( &corrected_join_options, args -> join_options,
                                      name_column_present ); /* helper.c */
            corrected_join_options . print_spotgroup = spot_group_requested( args -> seq_defline,
                                                                             args -> qual_defline ); /* flex_printer.c */
            VectorInit( &threads, 0, args -> num_threads );
            rows_per_thread = hlp_calculate_rows_per_thread( &num_threads2, seq_row_count );

            /* we need the row-count for that... */
            if ( args -> show_progress ) {
                rc = bg_progress_make( &progress, seq_row_count, 0, 0 ); /* progress_thread.c */
            }

            for ( thread_id = 0; 0 == rc && thread_id < num_threads2; ++thread_id ) {
                dbj_thread_data_t * jtd = calloc( 1, sizeof * jtd );
                if ( NULL == jtd ) {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                } else {
                    jtd -> dir              = args -> dir;
                    jtd -> vdb_mgr          = args -> vdb_mgr;
                    jtd -> accession_path   = args -> accession_path;
                    jtd -> accession_short  = args -> accession_short;
                    jtd -> lookup_filename  = args -> lookup_filename;
                    jtd -> index_filename   = args -> index_filename;
                    jtd -> seq_defline      = args -> seq_defline;
                    jtd -> qual_defline     = args -> qual_defline;
                    jtd -> first_row        = row;
                    jtd -> row_count        = rows_per_thread;
                    jtd -> row_limit        = args -> row_limit;
                    jtd -> cur_cache        = args -> cursor_cache;
                    jtd -> buf_size         = args -> buf_size;
                    jtd -> progress         = progress;
                    jtd -> registry         = args -> registry;
                    jtd -> fmt              = args -> fmt;
                    jtd -> join_options     = &corrected_join_options;
                    jtd -> thread_id        = thread_id;
                    jtd -> cmp_read_present = cmp_read_column_present;

                    rc = make_joined_filename( args -> temp_dir, jtd -> part_file, sizeof jtd -> part_file,
                                               args -> accession_short, thread_id ); /* temp_dir.c */
                    if ( 0 == rc ) {
                        rc = hlp_make_thread( &jtd -> thread, dbj_sorted_thread, jtd, THREAD_BIG_STACK_SIZE );
                        if ( 0 != rc ) {
                            ErrMsg( "join.c helper_make_thread( fastq/special #%d ) -> %R", thread_id, rc );
                        } else {
                            rc = VectorAppend( &threads, NULL, jtd );
                            if ( 0 != rc ) {
                                ErrMsg( "join.c VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                            }
                        }
                        row += rows_per_thread;
                    }
                }
            }
            rc = dbj_collect_threads_and_stats( &threads, args -> stats ); /* above */
            bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/
        }
    }
    return rc;
}

/*
    test-function to check if each row in the alignment-table can be found by referencing the seq-row-id and req-read-id
    in the lookup-file
*/
#if 0
rc_t dbj_check_lookup( const KDirectory * dir,
                   size_t buf_size,
                   size_t cursor_cache,
                   const char * lookup_filename,
                   const char * index_filename,
                   const char * accession_short,
                   const char * accession_path ) {
    struct index_reader_t * index;
    rc_t rc = make_index_reader( dir, &index, buf_size, "%s", index_filename );
    if ( 0 == rc ) {
        struct lookup_reader_t * lookup;  /* lookup_reader.h */
        rc =  make_lookup_reader( dir, index, &lookup, buf_size, "%s", lookup_filename ); /* lookup_reader.c */
        if ( 0 == rc ) {
            struct raw_read_iter_t * iter; /* raw_read_iter.h */
            cmn_iter_params_t params; /* helper.h */

            params . dir = dir;
            params . accession_short = accession_short;
            params . accession_path  = accession_path;

            params . first_row = 0;
            params . row_count = 0;
            params . cursor_cache = cursor_cache;

            rc = make_raw_read_iter( &params, &iter ); /* raw_read_iter.c */
            if ( 0 == rc ) {
                SBuffer_t buffer;
                rc = make_SBuffer( &buffer, 4096 );
                if ( 0 == rc ) {
                    raw_read_rec_t rec; /* raw_read_iter.h */
                    uint64_t loop = 0;
                    bool running = get_from_raw_read_iter( iter, &rec, &rc ); /* raw_read_iter.c */
                    while( 0 == rc && running ) {
                        rc = lookup_bases( lookup, rec . seq_spot_id, rec . seq_read_id, &buffer, false );
                        if ( 0 != rc ) {
                            KOutMsg( "lookup_bases( %lu.%u ) --> %R\n", rec . seq_spot_id, rec . seq_read_id, rc );
                        } else {
                            running = get_from_raw_read_iter( iter, &rec, &rc ); /* raw_read_iter.c */
                            if ( 0 == loop % 1000 ) {
                                KOutMsg( "[loop #%lu] ", loop / 1000 );
                            }
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

rc_t dbj_check_lookup_this( const KDirectory * dir,
                       size_t buf_size,
                       size_t cursor_cache,
                       const char * lookup_filename,
                       const char * index_filename,
                       uint64_t seq_spot_id,
                       uint32_t seq_read_id ) {
    struct index_reader_t * index;
    rc_t rc = make_index_reader( dir, &index, buf_size, "%s", index_filename );
    if ( 0 == rc ) {
        struct lookup_reader_t * lookup;  /* lookup_reader.h */
        rc =  make_lookup_reader( dir, index, &lookup, buf_size, "%s", lookup_filename ); /* lookup_reader.c */
        if ( 0 == rc ) {
            SBuffer_t buffer;
            rc = make_SBuffer( &buffer, 4096 );
            if ( 0 == rc ) {
                rc = lookup_bases( lookup, seq_spot_id, seq_read_id, &buffer, false );
                if ( 0 != rc ) {
                    KOutMsg( "lookup_bases( %lu.%u ) --> %R\n", seq_spot_id, seq_read_id, rc );
                }
                release_SBuffer( &buffer );
            }
            release_lookup_reader( lookup ); /* lookup_reader.c */
        }
        release_index_reader( index );
    }
    return rc;
}
#endif

/* ======================================================================================================================
    the unsorted FASTA approach for cSRA databases ...
   ====================================================================================================================== */

/* iterate over the ALIGN-table, using the align-iter from fastq_iter.c */
static rc_t CC dbj_unsorted_fasta_align_thread( const KThread * self, void * data )
{
    rc_t rc = 0;
    dbj_thread_data_t * jtd = data;
    join_stats_t * stats = &( jtd -> stats );
    cmn_iter_params_t cp;
    struct alit_t * align_iter;
    uint64_t loop_nr = 0;
    struct flp_t * flex_printer;
    
    cmn_iter_populate_params( &cp,
                              jtd -> dir,
                              jtd -> vdb_mgr,
                              jtd -> accession_short,
                              jtd -> accession_path,
                              jtd -> cur_cache,
                              jtd -> first_row,
                              jtd -> row_limit > 0 ? jtd -> row_limit : jtd -> row_count );
   
    flex_printer = flp_create_2( jtd -> multi_writer,          /* passed in multi-writer */
                            jtd -> accession_short,       /* the accession to be printed */
                            jtd -> seq_defline,           /* if seq-defline is NULL, use default */
                            NULL,                         /* qual-defline is NULL ( not used - FASTA! ) */
                            true );                       /* fasta-mode */
    if ( NULL == flex_printer ) {
        return rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    else {
        bool uses_read_id = read_id_requested( jtd -> seq_defline, NULL ); /* dflt_defline.c */
        rc = alit_create( &cp, &align_iter, uses_read_id ); /* fastq-iter.c */
        if ( 0 != rc ) {
            ErrMsg( "fast_align_thread_func().make_align_iter() -> %R", rc );
        } else {
            alit_rec_t rec;
            while ( 0 == rc && alit_get_rec( align_iter, &rec, &rc ) ) { /* fastq-iter.c */
                rc = hlp_get_quitting(); /* helper.c */

                if ( 0 == rc ) {
                    if ( rec . read . len > 0 ) {
                        flp_data_t data;
                        stats -> reads_read += 1;

                        /* fill out the data-record for the flex-printer */
                        data . row_id = rec . spot_id;
                        data . read_id = rec . read_id;
                        data . dst_id = 0;          /* not used, because registry=NULL - output to common final file */

                        data . spotname = NULL;     /* we do not have the spot-name in the align-table */
                        data . spotgroup = NULL;    /* align-rec does not have spot-group! */
                        data . read1 = &( rec . read );
                        data . read2 = NULL;
                        data . quality = NULL;

                        /* finally print it out */
                        rc = flp_print( flex_printer, &data ); /* flex_printer.c */
                        if ( 0 == rc ) { stats -> reads_written++; }
                    } else {
                        stats -> reads_zero_length++;
                    }

                    if ( 0 == rc ) {
                        loop_nr ++;
                        bg_progress_inc( jtd -> progress ); /* progress_thread.c (ignores NULL) */
                    } else {
                        ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", jtd -> thread_id, loop_nr, rec . row_id );
                        hlp_set_quitting(); /* helper.c */
                    }
                }
            }
            alit_release( align_iter );
        }
        flp_release( flex_printer );
    }
    return rc;
}

static rc_t dbj_create_unsorted_fasta_from_align( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    const char * seq_defline,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    const join_options_t * join_options,
                    uint64_t row_count,
                    uint64_t row_limit,
                    struct bg_progress_t * progress,
                    struct multi_writer_t * multi_writer,
                    struct filter_2na_t * filter,
                    Vector * threads ) {
    rc_t rc = 0;
    int64_t row = 1;
    uint32_t thread_id;
    uint64_t rows_per_thread = hlp_calculate_rows_per_thread( &num_threads, row_count );
    join_options_t corrected_join_options; /* helper.h */
    hlp_correct_join_options( &corrected_join_options, join_options, false );
    corrected_join_options . print_spotgroup = spot_group_requested( seq_defline, NULL ); /* flex_printer.c */

    for ( thread_id = 0; 0 == rc && thread_id < num_threads; ++thread_id ) {
        dbj_thread_data_t * jtd = calloc( 1, sizeof * jtd );
        if ( NULL != jtd ) {
            jtd -> dir              = dir;
            jtd -> vdb_mgr          = vdb_mgr;
            jtd -> accession_path   = accession_path;
            jtd -> accession_short  = accession_short;
            jtd -> seq_defline      = seq_defline;
            jtd -> first_row        = row;
            jtd -> row_count        = rows_per_thread;
            jtd -> row_limit        = row_limit;
            jtd -> cur_cache        = cur_cache;
            jtd -> buf_size         = buf_size;
            jtd -> progress         = progress;
            jtd -> multi_writer     = multi_writer;
            jtd -> fmt              = ft_fasta_us_split_spot; /* we handle only this one... */
            jtd -> join_options     = &corrected_join_options;
            jtd -> thread_id        = thread_id;
            jtd -> cmp_read_present = true;
            jtd -> filter           = filter;

            if ( 0 == rc ) {
                rc = hlp_make_thread( &jtd -> thread, dbj_unsorted_fasta_align_thread,
                                      jtd, THREAD_BIG_STACK_SIZE );
                if ( 0 != rc ) {
                    ErrMsg( "join.c helper_make_thread( fasta #%d ) -> %R", thread_id, rc );
                } else {
                    rc = VectorAppend( threads, NULL, jtd );
                    if ( 0 != rc ) {
                        ErrMsg( "join.c VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                    }
                }
                row += rows_per_thread;
            }
        }
    }
    return rc;
}

/* iterate over the SEQ-table, but only use what is half/fully unaligned... */
static rc_t CC dbj_unsorted_fasta_seq_thread( const KThread * self, void * data ) {
    rc_t rc = 0;
    dbj_thread_data_t * jtd = data;
    const join_options_t * jo = jtd -> join_options;
    join_stats_t * stats = &( jtd -> stats );
    cmn_iter_params_t cp;
    struct fq_seq_csra_iter_t * iter;
    fq_seq_csra_opt_t opt;
    uint64_t loop_nr = 0;
    struct flp_t * flex_printer = NULL;

    cmn_iter_populate_params( &cp,
                              jtd -> dir,
                              jtd -> vdb_mgr,
                              jtd -> accession_short,
                              jtd -> accession_path,
                              jtd -> cur_cache,
                              jtd -> first_row,
                              jtd -> row_limit > 0 ? jtd -> row_limit : jtd -> row_count );
    
    opt . with_read_len = true;
    opt . with_name = false;
    opt . with_read_type = true;
    opt . with_cmp_read = jtd -> cmp_read_present;
    opt . with_quality = false;
    opt . with_spotgroup = jo -> print_spotgroup;

    /* make_flex_printer() is in flex_printer.c */
    flex_printer = flp_create_2( jtd -> multi_writer, /* passed in multi-writer */
                    jtd -> accession_short,       /* the accession to be printed */
                    jtd -> seq_defline,           /* if seq-defline is NULL, use default */
                    NULL,                         /* qual-defline not used ( FASTA! ) */
                    true );                       /* fasta-mode */
    if ( NULL == flex_printer ) {
        return rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    else {
        rc = fq_seq_csra_iter_make( &cp, opt, &iter );
        if ( 0 != rc ) {
            ErrMsg( "fast_seq_thread_func().make_fastq_csra_iter() -> %R", rc );
        } else {
            fq_seq_csra_rec_t rec;
            while ( 0 == rc && fq_seq_csra_iter_get_data( iter, &rec, &rc ) ) {
                rc = hlp_get_quitting(); /* helper.c */
                if ( 0 == rc ) {
                    uint32_t read_id_0 = 0;
                    uint32_t offset = 0;
                    stats -> spots_read++;

                    while ( 0 == rc && read_id_0 < rec . num_read_len ) {
                        if ( rec . read_len[ read_id_0 ] > 0 ) {
                            if ( 0 == rec . prim_alig_id[ read_id_0 ] ) {
                                String R;
                                flp_data_t data;

                                stats -> reads_read += 1;

                                R . addr = &rec . read . addr[ offset ];
                                R . size = rec . read_len[ read_id_0 ];
                                R . len  = ( uint32_t )R . size;

                                /* fill out the data-record for the flex-printer */
                                data . row_id = rec . row_id;
                                data . read_id = read_id_0 + 1;
                                data . dst_id = 0;  /* not used, because registry=NULL - output to common final file */

                                data . spotname  = &( rec . name );
                                data . spotgroup = &( rec . spotgroup );
                                data . read1 = &R;
                                data . read2 = NULL;
                                data . quality = NULL;

                                /* finally print it out */
                                rc = flp_print( flex_printer, &data ); /* flex_printer.c */
                                if ( 0 == rc ) { stats -> reads_written++; }
                                offset += rec . read_len[ read_id_0 ];
                            }
                        }
                        else { stats -> reads_zero_length++; }
                        read_id_0++;
                    }

                    if ( 0 == rc ) {
                        loop_nr ++;
                        bg_progress_inc( jtd -> progress ); /* progress_thread.c (ignores NULL) */
                    } else {
                        ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", jtd -> thread_id, loop_nr, rec . row_id );
                        hlp_set_quitting(); /* helper.c */
                    }
                }
            }
            fq_seq_csra_iter_release( iter );
        }
        flp_release( flex_printer );
    }
    return rc;
}

static rc_t dbj_create_unsorted_fasta_from_seq( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    const char * seq_defline,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    const join_options_t * join_options,
                    uint64_t seq_row_count,
                    uint64_t row_limit,
                    bool cmp_read_column_present,
                    struct bg_progress_t * progress,
                    struct multi_writer_t * multi_writer,
                    struct filter_2na_t * filter,
                    Vector * threads ) {
    rc_t rc = 0;
    int64_t row = 1;
    uint32_t thread_id;
    uint64_t rows_per_thread = hlp_calculate_rows_per_thread( &num_threads, seq_row_count );
    join_options_t corrected_join_options; /* helper.h */
    hlp_correct_join_options( &corrected_join_options, join_options, false );
    corrected_join_options . print_spotgroup = spot_group_requested( seq_defline, NULL ); /* flex_printer.c */

    for ( thread_id = 0; 0 == rc && thread_id < num_threads; ++thread_id ) {
        dbj_thread_data_t * jtd = calloc( 1, sizeof * jtd );
        if ( NULL != jtd ) {
            jtd -> dir              = dir;
            jtd -> vdb_mgr          = vdb_mgr;
            jtd -> accession_path   = accession_path;
            jtd -> accession_short  = accession_short;
            jtd -> seq_defline      = seq_defline;
            jtd -> first_row        = row;
            jtd -> row_count        = rows_per_thread;
            jtd -> row_limit        = row_limit;
            jtd -> cur_cache        = cur_cache;
            jtd -> buf_size         = buf_size;
            jtd -> progress         = progress;
            jtd -> multi_writer     = multi_writer;
            jtd -> fmt              = ft_fasta_us_split_spot; /* we handle only this one... */
            jtd -> join_options     = &corrected_join_options;
            jtd -> thread_id        = thread_id;
            jtd -> cmp_read_present = cmp_read_column_present;
            jtd -> filter           = filter;

            if ( 0 == rc ) {
                rc = hlp_make_thread( &jtd -> thread, dbj_unsorted_fasta_seq_thread,
                                      jtd, THREAD_BIG_STACK_SIZE );
                if ( 0 != rc ) {
                    ErrMsg( "join.c helper_make_thread( fasta #%d ) -> %R", thread_id, rc );
                } else {
                    rc = VectorAppend( threads, NULL, jtd );
                    if ( 0 != rc ) {
                        ErrMsg( "join.c VectorAppend( sort-thread #%d ) -> %R", thread_id, rc );
                    }
                }
                row += rows_per_thread;
            }
        }
    }
    return rc;
}

rc_t dbj_create_unsorted_fasta( const dbj_unsorted_fasta_args_t * args ) {
    rc_t rc = 0;
    if ( args -> show_progress ) {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "read :" );
        KOutHandlerSetStdOut();
    }

    if ( 0 == rc ) {
        /* for the SEQUENCE-table */
        uint64_t seq_row_count = args -> insp_output -> seq . row_count;
        uint64_t align_row_count = args -> insp_output -> align . row_count;
        bool cmp_read_column_present;
        const char * seq_tbl_name = args -> insp_output -> seq . tbl_name;

        rc = cmn_iter_check_db_column( args -> dir, args -> vdb_mgr, args -> accession_short, args -> accession_path,
                                  seq_tbl_name, "CMP_READ", &cmp_read_column_present ); /* cmn_iter.c */

        if ( 0 == rc && ( seq_row_count > 0 || align_row_count > 0 ) ) {
            struct multi_writer_t * multi_writer = mw_create( args -> dir,
                    args -> output_filename,
                    args -> buf_size,
                    0,                          /* q_wait_time, if 0 --> use default = 5 ms */
                    args -> num_threads * 3,    /* q_num_blocks, if 0 use default = 8 */
                    0 );                        /* q_block_size, if 0 use default = 4 MB */
            if ( NULL != multi_writer ) {
                struct bg_progress_t * progress = NULL;
                struct filter_2na_t * filter = hlp_make_2na_filter( args -> join_options -> filter_bases ); /* helper.c */

                uint32_t num_threads2 = args -> num_threads;
                if ( args -> only_unaligned || args -> only_aligned ) {
                    num_threads2 = args -> num_threads >> 1;
                    if ( 1 == ( args -> num_threads & 0x01 ) ) { num_threads2 += 1; }
                }

                /* we need the row-count for that... ( that is why we first detected the row-count ) */
                if ( args -> show_progress ) {
                    rc = bg_progress_make( &progress, seq_row_count + align_row_count, 0, 0 ); /* progress_thread.c */
                }

                /* we now have:
                    - row-count for SEQ- and ALIGN table
                    - know if the CMP_READ-column exists in the SEQ-table
                    - optionally a progress-bar ( set to the sum of the row-counts in the SEQ- and ALIGN-table )
                */
                Vector seq_threads;
                VectorInit( &seq_threads, 0, args -> num_threads );

                if ( !( args -> only_aligned ) ) {
                    rc = dbj_create_unsorted_fasta_from_seq( args -> dir,
                                            args -> vdb_mgr,
                                            args -> accession_short,
                                            args -> accession_path,
                                            args -> seq_defline,
                                            args -> cur_cache,
                                            args -> buf_size,
                                            num_threads2,
                                            args -> join_options,
                                            seq_row_count,
                                            args -> row_limit,
                                            cmp_read_column_present,
                                            progress,
                                            multi_writer,
                                            filter,
                                            &seq_threads );
                }
                if ( 0 == rc && !( args -> only_unaligned ) ) {
                    Vector align_threads;
                    VectorInit( &align_threads, 0, args -> num_threads );

                    rc = dbj_create_unsorted_fasta_from_align( args -> dir,
                                        args -> vdb_mgr,
                                        args -> accession_short,
                                        args -> accession_path,
                                        args -> seq_defline,
                                        args -> cur_cache,
                                        args -> buf_size,
                                        num_threads2,
                                        args -> join_options,
                                        align_row_count,
                                        args -> row_limit,
                                        progress,
                                        multi_writer,
                                        filter,
                                        &align_threads );
                    if ( 0 == rc ) {
                        rc = dbj_collect_threads_and_stats( &align_threads, args -> stats ); /* above */
                    }
                }
                if ( 0 == rc ) {
                    rc = dbj_collect_threads_and_stats( &seq_threads, args -> stats ); /* above */
                }
                hlp_release_2na_filter( filter ); /* helper.c */
                bg_progress_release( progress ); /* progress_thread.c ( ignores NULL ) */
                mw_release( multi_writer ); /* ( ignores NULL ) */ 

            } /* if ( NULL != multi-writer )*/
        } /*  if ( 0 == rc ) && seq_req_count > 0 ) */
    }
    return rc;
}
