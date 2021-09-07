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

typedef struct join {
    const char * accession_path;
    const char * accession_short;
    struct lookup_reader_t * lookup;  /* lookup_reader.h */
    struct index_reader_t * index;    /* index.h */
    struct join_results_t * results;  /* join_results.h */
    struct filter_2na_t * filter;     /* joint_reslts.h */
    SBuffer_t B1, B2;                 /* helper.h */
    uint64_t loop_nr;                 /* in which loop of this partial join are we? */
    uint32_t thread_id;               /* in which thread are we? */
    bool cmp_read_present;            /* do we have a cmp-read column? */
} join_t;


static void release_join_ctx( join_t* j ) {
    if ( NULL != j ) {
        release_index_reader( j-> index );
        release_lookup_reader( j -> lookup );     /* lookup_reader.c */
        release_SBuffer( &( j -> B1 ) );          /* helper.c */
        release_SBuffer( &( j -> B2 ) );          /* helper.c */
    }
}

static rc_t init_join( cmn_iter_params_t * cp,
                       struct join_results_t * results,
                       struct filter_2na_t * filter,
                       const char * lookup_filename,
                       const char * index_filename,
                       size_t buf_size,
                       bool cmp_read_present,
                       join_t * j ) {
    rc_t rc;

    j -> accession_path  = cp -> accession_path;
    j -> accession_short = cp -> accession_short;
    j -> lookup = NULL;
    j -> results = results;
    j -> filter = filter;
    j -> B1 . S . addr = NULL;
    j -> B2 . S . addr = NULL;
    j -> loop_nr = 0;
    j -> cmp_read_present = cmp_read_present;
    
    if ( NULL != index_filename ) {
        if ( file_exists( cp -> dir, "%s", index_filename ) ) {
            rc = make_index_reader( cp -> dir, &j -> index, buf_size, "%s", index_filename ); /* index.c */
        }
    } else {
        j -> index = NULL;
    }

    rc = make_lookup_reader( cp -> dir, j -> index, &( j -> lookup ), buf_size,
                             "%s", lookup_filename ); /* lookup_reader.c */
    if ( 0 == rc ) {
        rc = make_SBuffer( &( j -> B1 ), 4096 );  /* helper.c */
        if ( 0 != rc ) {
            ErrMsg( "init_join().make_SBuffer( B1 ) -> %R", rc );
        }
    }
    if ( 0 == rc ) {
        rc = make_SBuffer( &( j -> B2 ), 4096 );  /* helper.c */
        if ( 0 != rc ) {
            ErrMsg( "init_join().make_SBuffer( B2 ) -> %R", rc );
        }
    }

    /* we do not seek any more at the beginning of the begin of a join-slice
       the call to lookup_bases will perform an internal seek now if it is not pointed
       to the right location */

    if ( 0 != rc ) { release_join_ctx( j ); /* above! */ }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t print_special_1_read( special_rec_t * rec, join_t * j ) {
    rc_t rc;
    int64_t row_id = rec -> row_id;

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        /* read is unaligned, print what is in row->cmp_read ( !!! no lookup !!! ) */
        rc = join_results_print( j -> results, 0, "%ld\t%S\t%S\n",
                         row_id, &( rec -> cmp_read ), &( rec -> spot_group ) ); /* join_results.c */
    } else {
        /* read is aligned ( 1 lookup ) */
        bool reverse = false;
        rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
        if ( 0 == rc ) {
            rc = join_results_print( j -> results, 0, "%ld\t%S\t%S\n",
                                row_id, &( j -> B1.S ), &( rec -> spot_group ) ); /* join_results.c */
        }
    }
    return rc;
}

static rc_t print_special_2_reads( special_rec_t * rec, join_t * j ) {
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* both unaligned, print what is in row->cmp_read ( !!! no lookup !!! )*/
            rc = join_results_print( j -> results, 0, "%ld\t%S\t%S\n",
                             row_id, &( rec -> cmp_read ), &( rec -> spot_group ) ); /* join_results.c */
        } else {
            /* A0 is unaligned / A1 is aligned (lookup) */
            bool reverse = false;
            rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse ); /* lookup_reader.c */
            if ( 0 == rc ) {
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &( rec -> cmp_read ), &( j -> B2 . S ), &( rec -> spot_group ) ); /* join_results.c */
            }
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* A0 is aligned (lookup) / A1 is unaligned */
            bool reverse = false;
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
            if ( 0 == rc ) {
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &j->B1.S, &rec->cmp_read, &rec->spot_group ); /* join_results.c */
            }
        } else {
            /* A0 and A1 are aligned (2 lookups)*/
            bool reverse1 = false;
            bool reverse2 = false;
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse1 ); /* lookup_reader.c */
            if ( 0 == rc ) {
                rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse2 ); /* lookup_reader.c */
            } if ( 0 == rc ) {
                rc = join_results_print( j -> results, 0, "%ld\t%S%S\t%S\n",
                                 row_id, &( j -> B1 . S ), &( j -> B2 . S ), &( rec -> spot_group ) ); /* join_results.c */
            }
        }
    }
    return rc;
}

static bool is_reverse( const fastq_rec_t * rec, uint32_t read_id_0 ) {
    bool res = false;
    if ( rec -> num_read_type > read_id_0 ) {
        res = ( ( rec -> read_type[ read_id_0 ] & READ_TYPE_REVERSE ) == READ_TYPE_REVERSE );
    }
    return res;
}

static bool filter( join_stats_t * stats,
                    const fastq_rec_t * rec,
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

static rc_t print_fastq_1_read( join_stats_t * stats,
                                const fastq_rec_t * rec,
                                join_t * j,
                                const join_options_t * jo ) {
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    
    bool process = filter( stats, rec, jo, 0 );
    if ( !process ) { return rc; }

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        /* read is unaligned, print what is in rec -> cmp_read ( no lookup ) */
        if ( filter_2na_1( j -> filter, &( rec -> read ) ) ) { /* join-results.c */
            if ( rec -> read . len != rec -> quality . len ) {
                ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (A)\n", row_id,
                        rec -> read . len, rec -> quality . len );
                stats -> reads_invalid++;
                if ( jo -> terminate_on_invalid ) {
                    return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                }
            }
            if ( rec -> read . len > 0 ) {
                rc = join_results_print_fastq_v1( j -> results,
                                                  row_id,
                                                  0, /* dst_id ( into which file to write ) */
                                                  1, /* read_id for tag-line */
                                                  jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                  &( rec -> read ),
                                                  &( rec -> quality ) ); /* join_results.c */
                if ( 0 == rc ) { stats -> reads_written++; }
            }
        }
    } else {
        /* read is aligned, ( 1 lookup ) */    
        bool reverse = is_reverse( rec, 0 );
        rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
        if ( 0 == rc ) {
            if ( filter_2na_1( j -> filter, &( j -> B1 . S ) ) ) {/* join-results.c */
                if ( j -> B1 . S . len != rec -> quality . len ) {
                    ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (B)\n", row_id,
                             j -> B1 . S . len, rec -> quality . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid ) {
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                }
                if ( j -> B1 . S . len > 0 ) {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      0, /* dst_id ( into which file to write ) */
                                                      1, /* read_id for tag-line */
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &( j -> B1 . S ),
                                                      &( rec -> quality ) ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
            }
        }
    }
    return rc;
}

static rc_t print_fasta_1_read( join_stats_t * stats,
                                const fastq_rec_t * rec,
                                join_t * j,
                                const join_options_t * jo ) {
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    
    bool process = filter( stats, rec, jo, 0 );
    if ( !process ) { return rc; }

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        /* read is unaligned, print what is in rec -> cmp_read ( no lookup ) */
        if ( filter_2na_1( j -> filter, &( rec -> read ) ) ) { /* join-results.c */
            if ( rec -> read . len > 0 ) {
                rc = join_results_print_fastq_v1( j -> results,
                                                  row_id,
                                                  0, /* dst_id ( into which file to write ) */
                                                  1, /* read_id for tag-line */
                                                  jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                  &( rec -> read ),
                                                  NULL ); /* join_results.c */
                if ( 0 == rc ) { stats -> reads_written++; }
            }
        }
    } else {
        /* read is aligned, ( 1 lookup ) */    
        bool reverse = is_reverse( rec, 0 );
        rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
        if ( 0 == rc ) {
            if ( filter_2na_1( j -> filter, &( j -> B1 . S ) ) ) { /* join-results.c */
                if ( j -> B1 . S . len > 0 ) {
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      0, /* dst_id ( into which file to write ) */
                                                      1, /* read_id for tag-line */
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &( j -> B1 . S ),
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written++; }
                }
            }
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

static rc_t print_fastq_2_reads( join_stats_t * stats,
                                 const fastq_rec_t * rec,
                                 join_t * j,
                                 const join_options_t * jo ) {
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    uint32_t dst_id = 1;
    
    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* both unaligned, print what is in row->read (no lookup)*/        
            if ( filter_2na_1( j -> filter, &( rec -> read ) ) ) { /* join-results.c */
                if ( rec -> read . len != rec -> quality . len ) {
                    ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (C)\n", row_id,
                            rec -> read . len, rec -> quality . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid ) {
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                }
                rc = join_results_print_fastq_v1( j -> results,
                                                  row_id,
                                                  dst_id,
                                                  dst_id,
                                                  jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                  &( rec -> read ),
                                                  &( rec -> quality ) ); /* join_results.c */
                if ( 0 == rc ) { stats -> reads_written += 2; }
            }
        } else {
            /* A0 is unaligned / A1 is aligned (lookup) */
            bool reverse = is_reverse( rec, 1 );
            rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse ); /* lookup_reader.c */
            if ( 0 == rc ) {
                if ( filter_2na_2( j -> filter, &( rec -> read ), &( j -> B2 . S ) ) ) { /* join-results.c */
                    if ( j -> B2 . S. len + rec -> read . len != rec -> quality . len ) {
                        ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (D)\n", row_id,
                                j -> B2 . S. len, rec -> quality . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid ) {
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                        }
                    }
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( rec -> read ),
                                      &( j -> B2 . S ),
                                      &( rec -> quality ) ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written += 2; }
                }
            }
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* A0 is aligned (lookup) / A1 is unaligned */
            bool reverse = is_reverse( rec, 0 );
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
            if ( 0 == rc ) {
                if ( filter_2na_2( j -> filter, &( j -> B1 . S ), &( rec -> read ) ) ) { /* join-results.c */
                    uint32_t rl = j -> B1 . S . len + rec -> read . len;
                    if ( rl != rec -> quality . len ) {
                        ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (E)\n", row_id,
                                rl, rec -> quality . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid ) {
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                        }
                    }
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( j -> B1 . S ),
                                      &( rec -> read ),
                                      &( rec -> quality ) ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written += 2; }
                }
            }
        } else {
            /* A0 and A1 are aligned (2 lookups)*/
            bool reverse1 = is_reverse( rec, 0 );
            bool reverse2 = is_reverse( rec, 1 );
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse1 ); /* lookup_reader.c */
            if ( 0 == rc ) {
                rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse2 ); /* lookup_reader.c */
            }
            if ( 0 == rc ) {
                if ( filter_2na_2( j -> filter, &( j -> B1 . S ), &( j -> B2 . S ) ) ) {/* join-results.c */
                    uint32_t rl = j -> B1 . S . len + j -> B2 . S . len;
                    if ( rl != rec -> quality . len ) {
                        ErrMsg( "row #%ld : read.len(%u) != quality.len(%u) (F)\n", row_id,
                                rl, rec -> quality . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid ) {
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                        }
                    }
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( j -> B1 . S ),
                                      &( j -> B2 . S ),
                                      &( rec -> quality ) ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written += 2; }
                }
            }
        }
    }
    return rc;
}

static rc_t print_fasta_2_reads( join_stats_t * stats,
                                 const fastq_rec_t * rec,
                                 join_t * j,
                                 const join_options_t * jo ) {
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    uint32_t dst_id = 1;
    
    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* both unaligned, print what is in row->read (no lookup)*/        
            if ( filter_2na_1( j -> filter, &( rec -> read ) ) ) { /* join-results.c */
                rc = join_results_print_fastq_v1( j -> results,
                                                  row_id,
                                                  dst_id,
                                                  dst_id,
                                                  jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                  &( rec -> read ),
                                                  NULL ); /* join_results.c */
                if ( 0 == rc ) { stats -> reads_written += 2; }
            }
        } else {
            /* A0 is unaligned / A1 is aligned (lookup) */
            bool reverse = is_reverse( rec, 1 );
            rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse ); /* lookup_reader.c */
            if ( 0 == rc ) {
                if ( filter_2na_2( j -> filter, &( rec -> read ), &( j -> B2 . S ) ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( rec -> read ),
                                      &( j -> B2 . S ),
                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written += 2; }
                }
            }
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* A0 is aligned (lookup) / A1 is unaligned */
            bool reverse = is_reverse( rec, 0 );
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse ); /* lookup_reader.c */
            if ( 0 == rc ) {
                if ( filter_2na_2( j -> filter, &( j -> B1 . S ), &( rec -> read ) ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( j -> B1 . S ),
                                      &( rec -> read ),
                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written += 2; }
                }
            }
        } else {
            /* A0 and A1 are aligned (2 lookups)*/
            bool reverse1 = is_reverse( rec, 0 );
            bool reverse2 = is_reverse( rec, 1 );
            rc = lookup_bases( j -> lookup, row_id, 1, &( j -> B1 ), reverse1 ); /* lookup_reader.c */
            if ( 0 == rc ) {
                rc = lookup_bases( j -> lookup, row_id, 2, &( j -> B2 ), reverse2 ); /* lookup_reader.c */
            }
            if ( 0 == rc ) {
                if ( filter_2na_2( j -> filter, &( j -> B1 . S ), &( j -> B2 . S ) ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v2( j -> results,
                                      row_id,
                                      dst_id,
                                      dst_id,
                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                      &( j -> B1 . S ),
                                      &( j -> B2 . S ),
                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written += 2; }
                }
            }
        }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

/* FASTQ SPLIT */
static rc_t print_fastq_2_reads_splitted( join_stats_t * stats,
                                          const fastq_rec_t * rec,
                                          join_t * j,
                                          bool split_file,
                                          const join_options_t * jo ) {
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    uint32_t dst_id = 1;
    String Q1, Q2;
    bool process_0 = true;
    bool process_1 = true;
    
    if ( !split_file && jo -> skip_tech ) {
        process_0 = filter( stats, rec, jo, 0 ); /* above */
        process_1 = filter( stats, rec, jo, 1 ); /* above */
    }

    Q1 . addr = rec -> quality . addr;
    Q1 . size = rec -> read_len[ 0 ];
    Q1 . len  = ( uint32_t )Q1 . size;
    Q2 . addr = &rec -> quality . addr[ rec -> read_len[ 0 ] ];
    Q2 . size = rec -> read_len[ 1 ];
    Q2 . len  = ( uint32_t )Q2 . size;

    if ( ( Q1 . len + Q2 . len ) != rec -> quality . len ) {
        ErrMsg( "row #%ld : Q[1].len(%u) + Q[2].len(%u) != Q.len(%u)\n", row_id, Q1 . len, Q2 . len, rec -> quality . len );
        stats -> reads_invalid++;
        if ( jo -> terminate_on_invalid ) {
            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
        }
    }

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            String READ1, READ2;
            if ( process_0 ) {
                READ1 . addr = rec -> read . addr;
                READ1 . size = rec -> read_len[ 0 ];
                READ1 . len = ( uint32_t )READ1 . size;
                if ( READ1 . len != Q1 . len ) {
                    ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 . len, Q1 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid ) {
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                }
                process_0 = ( READ1 . len > 0 );
            }
            if ( !process_0 ) { dst_id = 0; }

            if ( process_1 ) {
                READ2 . addr = &rec -> read . addr[ rec -> read_len[ 0 ] ];
                READ2 . size = rec -> read_len[ 1 ];
                READ2 . len = ( uint32_t )READ2 . size;
                if ( READ2 . len != Q2 . len ) {
                    ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 . len, Q2 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid ) {
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                }
                process_1 = ( READ2 . len > 0 );
            }
            if ( !process_1 ) { dst_id = 0; }

            /* both unaligned, print what is in row -> cmp_read ( no lookup ) */
            if ( process_0 ) {
                if ( filter_2na_1( j -> filter, &READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( 0 == rc ) {
                        stats -> reads_written ++;
                        if ( split_file && dst_id > 0 ) { dst_id++; }
                    }
                }
            }

            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, &READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        } else {
            /* A0 is unaligned / A1 is aligned (lookup) */
            const String * READ1 = &( rec -> read );
            String * READ2 = NULL;
            if ( process_0 ) {
                if ( READ1 -> len != Q1 . len ) {
                    ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 -> len, Q1 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid ) {
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                }
                process_0 = ( READ1 -> len > 0 );
            }
            if ( !process_0 ) { dst_id = 0; }

            if ( process_1 ) {
                bool reverse = is_reverse( rec, 1 );
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ2 = &( j -> B2 . S );
                    if ( READ2 -> len != Q2 . len ) {
                        ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 -> len, Q2 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid ) {
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                        }
                    }
                    process_1 = ( READ2 -> len > 0 );
                }
            }
            if ( !process_1 ) { dst_id = 0; }

            if ( 0 == rc && process_0 ) {
                if ( filter_2na_1( j -> filter, READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                    if ( split_file && dst_id > 0 ) { dst_id++; }
                }
            }
            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* A0 is aligned (lookup) / A1 is unaligned */
            String * READ1 = NULL;
            const String * READ2 = &( rec -> read );

            if ( process_0 ) {
                bool reverse = is_reverse( rec, 0 );
                rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ1 = &j -> B1 . S;
                    if ( READ1 -> len != Q1 . len ) {
                        ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 -> len, Q1 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid ) {
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                        }
                    }
                    process_0 = ( READ1 -> len > 0 );
                }
            }
            if ( !process_0 ) { dst_id = 0; }

            if ( process_1 ) {
                if ( READ2 -> len != Q2 . len ) {
                    ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 -> len, Q2 . len );
                    stats -> reads_invalid++;
                    if ( jo -> terminate_on_invalid ) {
                        return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                    }
                }
                process_1 = ( READ2 -> len > 0 );
            }
            if ( !process_1 ) { dst_id = 0; }

            if ( 0 == rc && process_0 ) {
                if ( filter_2na_1( j -> filter, READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                    if ( split_file && dst_id > 0 ) { dst_id++; }
                }
            }
            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        } else {
            /* A0 and A1 are aligned (2 lookups)*/
            String * READ1 = NULL;
            String * READ2 = NULL;

            if ( process_0 ) {
                bool reverse = is_reverse( rec, 0 );
                rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ1 = &j -> B1 . S;
                    if ( READ1 -> len != Q1 . len ) {
                        ErrMsg( "row #%ld : R[1].len(%u) != Q[1].len(%u)\n", row_id, READ1 -> len, Q1 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid ) {
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                        }
                    }
                    process_0 = ( READ1 -> len > 0 );
                }
            }
            if ( !process_0 ) { dst_id = 0; }

            if ( 0 == rc && process_1 ) {
                bool reverse = is_reverse( rec, 1 );
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ2 = &j -> B2 . S;
                    if ( READ2 -> len != Q2 . len ) {
                        ErrMsg( "row #%ld : R[2].len(%u) != Q[2].len(%u)\n", row_id, READ2 -> len, Q2 . len );
                        stats -> reads_invalid++;
                        if ( jo -> terminate_on_invalid ) {
                            return SILENT_RC( rcApp, rcNoTarg, rcAccessing, rcRow, rcInvalid );
                        }
                    }
                    process_1 =( READ2 -> len > 0 );
                }
            }
            if ( !process_1 ) { dst_id = 0; }

            if ( 0 == rc && process_0 ) {
                if ( filter_2na_1( j -> filter, READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      &Q1 ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                    if ( split_file && dst_id > 0 ) { dst_id++; }
                }
            }
            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      &Q2 ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        }
    }
    return rc;
}

static rc_t print_fasta_2_reads_splitted( join_stats_t * stats,
                                          const fastq_rec_t * rec,
                                          join_t * j,
                                          bool split_file,
                                          const join_options_t * jo ) {
    rc_t rc = 0;
    int64_t row_id = rec -> row_id;
    uint32_t dst_id = 1;
    bool process_0 = true;
    bool process_1 = true;
    
    if ( !split_file && jo -> skip_tech ) {
        process_0 = filter( stats, rec, jo, 0 ); /* above */
        process_1 = filter( stats, rec, jo, 1 ); /* above */
    }

    if ( 0 == rec -> prim_alig_id[ 0 ] ) {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            String READ1, READ2;
            if ( process_0 ) {
                READ1 . addr = rec -> read . addr;
                READ1 . size = rec -> read_len[ 0 ];
                READ1 . len = ( uint32_t )READ1 . size;
                process_0 = ( READ1 . len > 0 );
            }
            if ( !process_0 ) { dst_id = 0; }

            if ( process_1 ) {
                READ2 . addr = &rec -> read . addr[ rec -> read_len[ 0 ] ];
                READ2 . size = rec -> read_len[ 1 ];
                READ2 . len = ( uint32_t )READ2 . size;
                process_1 = ( READ2 . len > 0 );
            }
            if ( !process_1 ) { dst_id = 0; }

            /* both unaligned, print what is in row -> cmp_read ( no lookup ) */
            if ( process_0 ) {
                if ( filter_2na_1( j -> filter, &READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &READ1,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) {
                        stats -> reads_written ++;
                        if ( split_file && dst_id > 0 ) { dst_id++; }
                    }
                }
            }

            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, &READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      &READ2,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        } else {
            /* A0 is unaligned / A1 is aligned (lookup) */
            const String * READ1 = &( rec -> read );
            String * READ2 = NULL;
            if ( process_0 ) { process_0 = ( READ1 -> len > 0 ); }
            if ( !process_0 ) { dst_id = 0; }

            if ( process_1 ) {
                bool reverse = is_reverse( rec, 1 );
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ2 = &( j -> B2 . S );
                    process_1 = ( READ2 -> len > 0 );
                }
            }
            if ( !process_1 ) { dst_id = 0; }

            if ( 0 == rc && process_0 ) {
                if ( filter_2na_1( j -> filter, READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written++; }
                    if ( split_file && dst_id > 0 ) { dst_id++; }
                }
            }
            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        }
    } else {
        if ( 0 == rec -> prim_alig_id[ 1 ] ) {
            /* A0 is aligned (lookup) / A1 is unaligned */
            String * READ1 = NULL;
            const String * READ2 = &( rec -> read );

            if ( process_0 ) {
                bool reverse = is_reverse( rec, 0 );
                rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ1 = &j -> B1 . S;
                    process_0 = ( READ1 -> len > 0 );
                }
            }
            if ( !process_0 ) { dst_id = 0; }
            if ( process_1 ) { process_1 = ( READ2 -> len > 0 ); }
            if ( !process_1 ) { dst_id = 0; }

            if ( 0 == rc && process_0 ) {
                if ( filter_2na_1( j -> filter, READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                    if ( split_file && dst_id > 0 ) { dst_id++; }
                }
            }
            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        } else {
            /* A0 and A1 are aligned (2 lookups)*/
            String * READ1 = NULL;
            String * READ2 = NULL;

            if ( process_0 ) {
                bool reverse = is_reverse( rec, 0 );
                rc = lookup_bases( j -> lookup, row_id, 1, &j -> B1, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ1 = &j -> B1 . S;
                    process_0 = ( READ1 -> len > 0 );
                }
            }
            if ( !process_0 ) { dst_id = 0; }

            if ( 0 == rc && process_1 ) {
                bool reverse = is_reverse( rec, 1 );
                rc = lookup_bases( j -> lookup, row_id, 2, &j -> B2, reverse ); /* lookup_reader.c */
                if ( 0 == rc ) {
                    READ2 = &j -> B2 . S;
                    process_1 =( READ2 -> len > 0 );
                }
            }
            if ( !process_1 ) { dst_id = 0; }

            if ( 0 == rc && process_0 ) {
                if ( filter_2na_1( j -> filter, READ1 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      1,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ1,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                    if ( split_file && dst_id > 0 ) { dst_id++; }
                }
            }
            if ( 0 == rc && process_1 ) {
                if ( filter_2na_1( j -> filter, READ2 ) ) { /* join-results.c */
                    rc = join_results_print_fastq_v1( j -> results,
                                                      row_id,
                                                      dst_id,
                                                      2,
                                                      jo -> rowid_as_name ? NULL : &( rec -> name ),
                                                      READ2,
                                                      NULL ); /* join_results.c */
                    if ( 0 == rc ) { stats -> reads_written ++; }
                }
            }
        }
    }
    return rc;
}

static rc_t extract_seq_row_count( KDirectory * dir,
                                   const VDBManager * vdb_mgr,
                                   const char * accession_short,
                                   const char * accession_path,
                                   size_t cur_cache,
                                   uint64_t * res ) {
    cmn_iter_params_t cp = { dir, vdb_mgr, accession_short, accession_path, 0, 0, cur_cache };
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt = { false, false, false, false, false }; /* fastq_iter.h */
    rc_t rc = make_fastq_csra_iter( &cp, opt, &iter ); /* fastq_iter.c */
    if ( 0 == rc ) {
        *res = get_row_count_of_fastq_csra_iter( iter );
        destroy_fastq_csra_iter( iter );
    }
    return rc;
}

static rc_t extract_align_row_count( KDirectory * dir,
                                   const VDBManager * vdb_mgr,
                                   const char * accession_short,
                                   const char * accession_path,
                                   size_t cur_cache,
                                   uint64_t * res ) {
    cmn_iter_params_t cp = { dir, vdb_mgr, accession_short, accession_path, 0, 0, cur_cache };
    struct align_iter_t * iter;
    rc_t rc = make_align_iter( &cp, &iter ); /* fastq_iter.c */
    if ( 0 == rc ) {
        *res = get_row_count_of_align_iter( iter );
        destroy_align_iter( iter );
    }
    return rc;
}

static rc_t perform_special_join( cmn_iter_params_t * cp,
                                  join_t * j,
                                  struct bg_progress_t * progress ) {
    struct special_iter_t * iter;
    rc_t rc = make_special_iter( cp, &iter ); /* special_iter.c */
    if ( 0 == rc ) {
        special_rec_t rec;
        while ( 0 == rc && get_from_special_iter( iter, &rec, &rc ) ) {
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                if ( 1 == rec . num_reads ) {
                    rc = print_special_1_read( &rec, j ); /* above */
                } else {
                    rc = print_special_2_reads( &rec, j ); /* above */
                }
                j -> loop_nr ++;
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        if ( 0 != rc ) {
            set_quitting();     /* helper.c */
        }
        destroy_special_iter( iter ); /* special_iter.c */
    } else {
        ErrMsg( "perform_special_join().make_special_iter() -> %R", rc );
    }
    return rc;
}

static rc_t perform_fastq_whole_spot_join( cmn_iter_params_t * cp,
                                     join_stats_t * stats,
                                     join_t * j,
                                     struct bg_progress_t * progress,
                                     const join_options_t * jo ) {
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = false;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;
    
    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        join_options_t local_opt = { jo -> rowid_as_name,
                                   false, 
                                   jo -> print_read_nr,
                                   jo -> print_name,
                                   jo -> terminate_on_invalid,
                                   jo -> min_read_len,
                                   jo -> filter_bases };
        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fastq_1_read( stats, &rec, j, &local_opt ); /* above */
                } else {
                    rc = print_fastq_2_reads( stats, &rec, j, &local_opt ); /* above */
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting(); /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t perform_fastq_split_spot_join( cmn_iter_params_t * cp,
                                      join_stats_t * stats,
                                      join_t * j,
                                      struct bg_progress_t * progress,
                                      const join_options_t * jo ) {
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;

    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_spot_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fastq_1_read( stats, &rec, j, jo ); /* above */
                } else {
                    rc = print_fastq_2_reads_splitted( stats, &rec, j, false, jo ); /* above */
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting(); /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t perform_fastq_split_file_join( cmn_iter_params_t * cp,
                                      join_stats_t * stats,
                                      join_t * j,
                                      struct bg_progress_t * progress,
                                      const join_options_t * jo ) {
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;

    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_file_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        join_options_t local_opt = { 
                jo -> rowid_as_name,
                false,
                jo -> print_read_nr,
                jo -> print_name,
                jo -> terminate_on_invalid,
                jo -> min_read_len,
                jo -> filter_bases
            };

        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fastq_1_read( stats, &rec, j, &local_opt );
                } else {
                    rc = print_fastq_2_reads_splitted( stats, &rec, j, true, &local_opt );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting();     /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );
    }
    return rc;
}

static rc_t perform_fastq_split_3_join( cmn_iter_params_t * cp,
                                      join_stats_t * stats,
                                      join_t * j,
                                      struct bg_progress_t * progress,
                                      const join_options_t * jo ) {
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = true;

    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_3_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        rc_t rc_iter = 0;
        join_options_t local_opt = {
                jo -> rowid_as_name,
                false,
                jo -> print_read_nr,
                jo -> print_name,
                jo -> terminate_on_invalid,
                jo -> min_read_len,
                jo -> filter_bases
            };
        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fastq_1_read( stats, &rec, j, &local_opt );
                } else {
                    rc = print_fastq_2_reads_splitted( stats, &rec, j, true, &local_opt );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting();     /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );
        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
    }
    return rc;
}

static rc_t perform_fasta_whole_spot_join( cmn_iter_params_t * cp,
                                join_stats_t * stats,
                                join_t * j,
                                struct bg_progress_t * progress,
                                const join_options_t * jo ) {
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = false;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;
    
    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        join_options_t local_opt = { jo -> rowid_as_name,
                                   false, 
                                   jo -> print_read_nr,
                                   jo -> print_name,
                                   jo -> terminate_on_invalid,
                                   jo -> min_read_len,
                                   jo -> filter_bases };
        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fasta_1_read( stats, &rec, j, &local_opt ); /* above */
                } else {
                    rc = print_fasta_2_reads( stats, &rec, j, &local_opt ); /* above */
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting(); /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t perform_fasta_split_spot_join( cmn_iter_params_t * cp,
                                      join_stats_t * stats,
                                      join_t * j,
                                      struct bg_progress_t * progress,
                                      const join_options_t * jo )
{
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;

    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_spot_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fasta_1_read( stats, &rec, j, jo ); /* above */
                } else {
                    rc = print_fasta_2_reads_splitted( stats, &rec, j, false, jo ); /* above */
                }
                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting(); /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t perform_fasta_split_file_join( cmn_iter_params_t * cp,
                                      join_stats_t * stats,
                                      join_t * j,
                                      struct bg_progress_t * progress,
                                      const join_options_t * jo ) {
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;

    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fastq_split_file_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        join_options_t local_opt = { 
                jo -> rowid_as_name,
                false,
                jo -> print_read_nr,
                jo -> print_name,
                jo -> terminate_on_invalid,
                jo -> min_read_len,
                jo -> filter_bases
            };

        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fasta_1_read( stats, &rec, j, &local_opt );
                } else {
                    rc = print_fasta_2_reads_splitted( stats, &rec, j, true, &local_opt );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting();     /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );
    }
    return rc;
}

static rc_t perform_fasta_split_3_join( cmn_iter_params_t * cp,
                                      join_stats_t * stats,
                                      join_t * j,
                                      struct bg_progress_t * progress,
                                      const join_options_t * jo ) {
    rc_t rc;
    struct fastq_csra_iter_t * iter;
    fastq_iter_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = !( jo -> rowid_as_name );
    opt . with_read_type = true;
    opt . with_cmp_read = j -> cmp_read_present;
    opt . with_quality = false;

    rc = make_fastq_csra_iter( cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "perform_fasta_split_3_join().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        rc_t rc_iter = 0;

        join_options_t local_opt = {
                jo -> rowid_as_name,
                false,
                jo -> print_read_nr,
                jo -> print_name,
                jo -> terminate_on_invalid,
                jo -> min_read_len,
                jo -> filter_bases
            };

        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc_iter ) && 0 == rc_iter ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                stats -> spots_read++;
                stats -> reads_read += rec . num_alig_id;

                if ( 1 == rec . num_alig_id ) {
                    rc = print_fasta_1_read( stats, &rec, j, &local_opt );
                } else {
                    rc = print_fasta_2_reads_splitted( stats, &rec, j, true, &local_opt );
                }

                if ( 0 == rc ) {
                    j -> loop_nr ++;
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", j -> thread_id, j -> loop_nr, rec . row_id );
                    set_quitting();     /* helper.c */
                }
                bg_progress_inc( progress ); /* progress_thread.c (ignores NULL) */
            }
        }
        destroy_fastq_csra_iter( iter );

        if ( 0 == rc && 0 != rc_iter ) { rc = rc_iter; }
    }
    return rc;
}

/* ------------------------------------------------------------------------------------------ */

typedef struct join_thread_data {
    char part_file[ 4096 ];

    join_stats_t stats; /* helper.h */
    
    KDirectory * dir;
    const VDBManager * vdb_mgr;

    const char * accession_path;
    const char * accession_short;
    const char * lookup_filename;
    const char * index_filename;
    struct bg_progress_t * progress;
    struct temp_registry_t * registry;
    struct filter_2na_t * filter;

    KThread * thread;
    
    int64_t first_row;
    uint64_t row_count;
    size_t cur_cache;
    size_t buf_size;
    format_t fmt;
    uint32_t thread_id;
    bool cmp_read_present;

    const join_options_t * join_options;
    
} join_thread_data_t;

static rc_t CC cmn_thread_func( const KThread * self, void * data ) {
    join_thread_data_t * jtd = data;
    struct filter_2na_t * filter = make_2na_filter( jtd -> join_options -> filter_bases ); /* join_results.c */
    struct join_results_t * results = NULL;

    rc_t rc = make_join_results( jtd -> dir,
                                &results,
                                jtd -> registry,
                                jtd -> part_file,
                                jtd -> accession_short,
                                jtd -> buf_size,
                                4096,
                                jtd -> join_options -> print_read_nr,
                                jtd -> join_options -> print_name ); /* join_results.c */

    if ( 0 == rc && NULL != results ) {
        join_t j;
        cmn_iter_params_t cp = { jtd -> dir, jtd -> vdb_mgr,
                          jtd -> accession_short, jtd -> accession_path,
                          jtd -> first_row, jtd -> row_count, jtd -> cur_cache };
        rc = init_join( &cp,
                        results,
                        filter,
                        jtd -> lookup_filename,
                        jtd -> index_filename,
                        jtd -> buf_size,
                        jtd -> cmp_read_present,
                        &j ); /* above */
        if ( 0 == rc ) {
            j . thread_id = jtd -> thread_id;

            switch ( jtd -> fmt ) {
                case ft_special             : rc = perform_special_join( &cp,
                                                        &j,
                                                        jtd -> progress ); break;

                case ft_fastq_whole_spot    : rc = perform_fastq_whole_spot_join( &cp,
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

                case ft_fasta_whole_spot    : rc = perform_fasta_whole_spot_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                case ft_fasta_split_spot    : rc = perform_fasta_split_spot_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                case ft_fasta_split_file    : rc = perform_fasta_split_file_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                case ft_fasta_split_3       : rc = perform_fasta_split_3_join( &cp,
                                                        &jtd -> stats,
                                                        &j,
                                                        jtd -> progress,
                                                        jtd -> join_options ); break;

                case ft_unknown : break;                /* this should never happen */
                case ft_fasta_us_split_spot : break;    /* nether should this */
            }
            release_join_ctx( &j );
        }
        destroy_join_results( results ); /* join_results.c */
    }
    release_2na_filter( filter );   /* join_results.c */
    return rc;
}

static rc_t join_threads_collect_stats( Vector * threads, join_stats_t * stats ) {
    rc_t rc = 0;
    /* collect the threads, and add the join_stats */
    uint32_t i, n = VectorLength( threads );
    for ( i = VectorStart( threads ); i < n; ++i ) {
        join_thread_data_t * jtd = VectorGet( threads, i );
        if ( NULL != jtd ) {
            rc_t rc_thread;
            KThreadWait( jtd -> thread, &rc_thread );
            if ( 0 != rc_thread ) { rc = rc_thread; }
            KThreadRelease( jtd -> thread );
            add_join_stats( stats, &jtd -> stats ); /* helper.c */
            free( jtd );
        }
    }
    VectorWhack ( threads, NULL, NULL );
    return rc;
}

rc_t execute_db_join( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_path,
                    const char * accession_short,
                    join_stats_t * stats,
                    const char * lookup_filename,
                    const char * index_filename,
                    const struct temp_dir_t * temp_dir,
                    struct temp_registry_t * registry,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    bool show_progress,
                    format_t fmt,
                    const join_options_t * join_options ) {
    rc_t rc = 0;

    if ( show_progress ) {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "join   :" );
        KOutHandlerSetStdOut();
    }

    if ( rc == 0 ) {
        uint64_t seq_row_count = 0;
        bool name_column_present, cmp_read_column_present;

        rc = cmn_check_db_column( dir, vdb_mgr, accession_short, accession_path, "SEQUENCE", "NAME", &name_column_present ); /* cmn_iter.c */
        if ( 0 == rc ) {
            rc = cmn_check_db_column( dir, vdb_mgr, accession_short, accession_path, "SEQUENCE", "CMP_READ", &cmp_read_column_present ); /* cmn_iter.c */
        }

        rc = extract_seq_row_count( dir, vdb_mgr, accession_short, accession_path, cur_cache, &seq_row_count ); /* above */
        if ( 0 == rc && seq_row_count > 0 ) {
            Vector threads;
            int64_t row = 1;
            uint32_t thread_id;
            uint64_t rows_per_thread;
            struct bg_progress_t * progress = NULL;
            join_options_t corrected_join_options;

            correct_join_options( &corrected_join_options, join_options, name_column_present ); /* helper.c */
            VectorInit( &threads, 0, num_threads );
            rows_per_thread = calculate_rows_per_thread( &num_threads, seq_row_count ); /* helper.c */

            /* we need the row-count for that... */
            if ( show_progress ) {
                rc = bg_progress_make( &progress, seq_row_count, 0, 0 ); /* progress_thread.c */
            }

            for ( thread_id = 0; 0 == rc && thread_id < num_threads; ++thread_id ) {
                join_thread_data_t * jtd = calloc( 1, sizeof * jtd );
                if ( NULL == jtd ) {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                } else {
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
                    if ( 0 == rc ) {
                        rc = helper_make_thread( &jtd -> thread, cmn_thread_func, jtd, THREAD_BIG_STACK_SIZE );
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
            rc = join_threads_collect_stats( &threads, stats ); /* above */
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

rc_t check_lookup_this( const KDirectory * dir,
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

/* ---------------------------------------------------------------------------------------------------- */

/* iterate over the ALIGN-table, using the align-iter from fastq_iter.c */
static rc_t CC fast_align_thread_func( const KThread * self, void * data )
{
    rc_t rc = 0;
    join_thread_data_t * jtd = data;
    struct common_join_results_t * results = ( struct common_join_results_t * ) jtd -> registry;
    join_stats_t * stats = &( jtd -> stats );
    cmn_iter_params_t cp = { jtd -> dir, jtd -> vdb_mgr,
                        jtd -> accession_short, jtd -> accession_path,
                        jtd -> first_row, jtd -> row_count, jtd -> cur_cache };
    const char * acc = jtd -> accession_short;
    struct align_iter_t * iter;
    uint64_t loop_nr = 0;

    rc = make_align_iter( &cp, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "fast_align_thread_func().make_align_iter() -> %R", rc );
    } else {
        align_rec_t rec; /* fastq_iter.h */
        while ( 0 == rc && get_from_align_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                if ( rec . read . len > 0 ) {
                    stats -> reads_read += 1;
                    rc = common_join_results_print( results, ">%s.%lu %lu length=%u\n%S\n",
                        acc, rec . spot_id, rec . spot_id, rec . read . len, &( rec . read ) );
                    if ( 0 == rc ) { stats -> reads_written++; }
                } else { 
                    stats -> reads_zero_length++;
                }

                if ( 0 == rc ) {
                    loop_nr ++;
                    bg_progress_inc( jtd -> progress ); /* progress_thread.c (ignores NULL) */
                } else {
                    ErrMsg( "terminated in loop_nr #%u.%lu for SEQ-ROWID #%ld", jtd -> thread_id, loop_nr, rec . row_id );
                    set_quitting(); /* helper.c */
                }
            }
        }
        destroy_align_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t start_fast_join_align( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    const join_options_t * join_options,
                    uint64_t row_count,
                    struct bg_progress_t * progress,
                    struct common_join_results_t * results,
                    struct filter_2na_t * filter,
                    Vector * threads ) {
    rc_t rc = 0;
    int64_t row = 1;
    uint32_t thread_id;
    uint64_t rows_per_thread = calculate_rows_per_thread( &num_threads, row_count ); /* helper.c */
    join_options_t corrected_join_options; /* helper.h */
    correct_join_options( &corrected_join_options, join_options, false );

    for ( thread_id = 0; 0 == rc && thread_id < num_threads; ++thread_id ) {
        join_thread_data_t * jtd = calloc( 1, sizeof * jtd );
        if ( NULL != jtd ) {
            jtd -> dir              = dir;
            jtd -> vdb_mgr          = vdb_mgr;
            jtd -> accession_path   = accession_path;
            jtd -> accession_short  = accession_short;
            jtd -> lookup_filename  = NULL;
            jtd -> index_filename   = NULL;
            jtd -> first_row        = row;
            jtd -> row_count        = rows_per_thread;
            jtd -> cur_cache        = cur_cache;
            jtd -> buf_size         = buf_size;
            jtd -> progress         = progress;
            jtd -> registry         = ( void * )results;    /* use the common-output here */
            jtd -> fmt              = ft_fasta_us_split_spot; /* we handle only this one... */
            jtd -> join_options     = &corrected_join_options;
            jtd -> thread_id        = thread_id;
            jtd -> cmp_read_present = true;
            jtd -> filter           = filter;

            if ( 0 == rc ) {
                rc = helper_make_thread( &jtd -> thread, fast_align_thread_func, jtd, THREAD_BIG_STACK_SIZE ); /* helper.c */
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
static rc_t CC fast_seq_thread_func( const KThread * self, void * data ) {
    rc_t rc = 0;
    join_thread_data_t * jtd = data;
    struct common_join_results_t * results = ( struct common_join_results_t * ) jtd -> registry;
    join_stats_t * stats = &( jtd -> stats );
    cmn_iter_params_t cp = { jtd -> dir, jtd -> vdb_mgr,
                        jtd -> accession_short, jtd -> accession_path,
                        jtd -> first_row, jtd -> row_count, jtd -> cur_cache };
    const char * acc = jtd -> accession_short;
    struct fastq_csra_iter_t * iter;
    uint64_t loop_nr = 0;
    fastq_iter_opt_t opt;
    opt . with_read_len = true;
    opt . with_name = false;
    opt . with_read_type = true;
    opt . with_cmp_read = jtd -> cmp_read_present;
    opt . with_quality = false;

    rc = make_fastq_csra_iter( &cp, opt, &iter ); /* fastq-iter.c */
    if ( 0 != rc ) {
        ErrMsg( "fast_seq_thread_func().make_fastq_csra_iter() -> %R", rc );
    } else {
        fastq_rec_t rec; /* fastq_iter.h */
        while ( 0 == rc && get_from_fastq_csra_iter( iter, &rec, &rc ) ) { /* fastq-iter.c */
            rc = get_quitting(); /* helper.c */
            if ( 0 == rc ) {
                uint32_t read_id_0 = 0;
                uint32_t offset = 0;
                stats -> spots_read++;
                while ( 0 == rc && read_id_0 < rec . num_read_len ) {
                    if ( rec . read_len[ read_id_0 ] > 0 ) {
                        if ( 0 == rec . prim_alig_id[ read_id_0 ] ) {
                            String R;

                            stats -> reads_read += 1;

                            R . addr = &rec . read . addr[ offset ];
                            R . size = rec . read_len[ read_id_0 ];
                            R . len  = ( uint32_t )R . size;

                            rc = common_join_results_print( results, ">%s.%lu %lu length=%u\n%S\n",
                                acc, rec . row_id, rec . row_id, R . len, &R );

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
                    set_quitting(); /* helper.c */
                }
            }
        }
        destroy_fastq_csra_iter( iter ); /* fastq-iter.c */
    }
    return rc;
}

static rc_t start_fast_join_seq( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    const join_options_t * join_options,
                    uint64_t seq_row_count,
                    bool cmp_read_column_present,
                    struct bg_progress_t * progress,
                    struct common_join_results_t * results,
                    struct filter_2na_t * filter,
                    Vector * threads ) {
    rc_t rc = 0;
    int64_t row = 1;
    uint32_t thread_id;
    uint64_t rows_per_thread = calculate_rows_per_thread( &num_threads, seq_row_count ); /* helper.c */
    join_options_t corrected_join_options; /* helper.h */
    correct_join_options( &corrected_join_options, join_options, false );

    for ( thread_id = 0; 0 == rc && thread_id < num_threads; ++thread_id ) {
        join_thread_data_t * jtd = calloc( 1, sizeof * jtd );
        if ( NULL != jtd ) {
            jtd -> dir              = dir;
            jtd -> vdb_mgr          = vdb_mgr;
            jtd -> accession_path   = accession_path;
            jtd -> accession_short  = accession_short;
            jtd -> lookup_filename  = NULL;
            jtd -> index_filename   = NULL;
            jtd -> first_row        = row;
            jtd -> row_count        = rows_per_thread;
            jtd -> cur_cache        = cur_cache;
            jtd -> buf_size         = buf_size;
            jtd -> progress         = progress;
            jtd -> registry         = ( void * )results;    /* use the common-output here */
            jtd -> fmt              = ft_fasta_us_split_spot; /* we handle only this one... */
            jtd -> join_options     = &corrected_join_options;
            jtd -> thread_id        = thread_id;
            jtd -> cmp_read_present = cmp_read_column_present;
            jtd -> filter           = filter;

            if ( 0 == rc ) {
                rc = helper_make_thread( &jtd -> thread, fast_seq_thread_func, jtd, THREAD_BIG_STACK_SIZE ); /* helper.c */
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

rc_t execute_fast_join( KDirectory * dir,
                    const VDBManager * vdb_mgr,
                    const char * accession_short,
                    const char * accession_path,
                    join_stats_t * stats,
                    size_t cur_cache,
                    size_t buf_size,
                    uint32_t num_threads,
                    bool show_progress,
                    const char * output_filename, /* NULL for stdout! */
                    const join_options_t * join_options,
                    bool force ) {
    rc_t rc = 0;
    if ( show_progress ) {
        KOutHandlerSetStdErr();
        rc = KOutMsg( "read :" );
        KOutHandlerSetStdOut();
    }

    if ( 0 == rc ) {
        /* for the SEQUENCE-table */
        uint64_t seq_row_count = 0;
        uint64_t align_row_count = 0;
        bool cmp_read_column_present;

        rc = cmn_check_db_column( dir, vdb_mgr, accession_short, accession_path, "SEQUENCE", "CMP_READ", &cmp_read_column_present ); /* cmn_iter.c */

        if ( 0 == rc ) {
            rc = extract_seq_row_count( dir, vdb_mgr, accession_short, accession_path, cur_cache, &seq_row_count ); /* above */
        }
        if ( 0 == rc ) {
            rc = extract_align_row_count( dir, vdb_mgr, accession_short, accession_path, cur_cache, &align_row_count ); /* above */
        }

        if ( 0 == rc && seq_row_count > 0 ) {
            struct bg_progress_t * progress = NULL;
            struct filter_2na_t * filter = make_2na_filter( join_options -> filter_bases ); /* join_results.c */
            struct common_join_results_t * results = NULL;

            /* we need the row-count for that... ( that is why we first detected the row-count ) */
            if ( show_progress ) {
                rc = bg_progress_make( &progress, seq_row_count + align_row_count, 0, 0 ); /* progress_thread.c */
            }

            rc = make_common_join_results( dir,
                                    &results,
                                    buf_size,
                                    4096,
                                    output_filename,
                                    force ); /* join_results.c */
            if ( 0 == rc ) {
                /* we now have:
                    - row-count for SEQ- and ALIGN table
                    - know if the CMP_READ-column exists in the SEQ-table
                    - the common-output-results, ready to beeing used via common_join_results_print()
                    - optionally a progress-bar ( set to the sum of the row-counts in the SEQ- and ALIGN-table )
                */
                Vector seq_threads;
                VectorInit( &seq_threads, 0, num_threads );

                rc = start_fast_join_seq( dir,
                                          vdb_mgr,
                                          accession_short,
                                          accession_path,
                                          cur_cache,
                                          buf_size,
                                          num_threads,
                                          join_options,
                                          seq_row_count,
                                          cmp_read_column_present,
                                          progress,
                                          results,
                                          filter,
                                          &seq_threads );
                if ( 0 == rc ) {
                    Vector align_threads;
                    VectorInit( &align_threads, 0, num_threads );

                    rc = start_fast_join_align( dir,
                                        vdb_mgr,
                                        accession_short,
                                        accession_path,
                                        cur_cache,
                                        buf_size,
                                        num_threads,
                                        join_options,
                                        align_row_count,
                                        progress,
                                        results,
                                        filter,
                                        &align_threads );
                    if ( 0 == rc ) {
                        rc = join_threads_collect_stats( &align_threads, stats ); /* above */
                    }
                    if ( 0 == rc ) {
                        rc = join_threads_collect_stats( &seq_threads, stats ); /* above */
                    }
                }
                destroy_common_join_results( results );
            }

            release_2na_filter( filter ); /* join_results.c */
            bg_progress_release( progress ); /* progress_thread.c ( ignores NULL ) */
        }
    }
    return rc;
}
