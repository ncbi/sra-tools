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
#ifndef _h_join_results_
#define _h_join_results_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_kfs_file_
#include <kfs/file.h>
#endif

#ifndef _h_temp_registry_
#include "temp_registry.h"
#endif

#ifndef _h_copy_machine_
#include "copy_machine.h"
#endif

struct join_results_t;

void destroy_join_results( struct join_results_t * self );

rc_t make_join_results( struct KDirectory * dir,
                        struct join_results_t ** results,
                        struct temp_registry_t * registry,
                        const char * output_base,
                        const char * accession_short,
                        size_t file_buffer_size,
                        size_t print_buffer_size,
                        bool print_frag_nr,
                        bool print_name );

/* --------------------------------------------------------------------------------------------------- */

struct filter_2na_t;

struct filter_2na_t * make_2na_filter( const char * filter_bases );
void release_2na_filter( struct filter_2na_t * self );

/* return true if no filter set, or filter matches the bases */
bool filter_2na_1( struct filter_2na_t * self, const String * bases );
bool filter_2na_2( struct filter_2na_t * self, const String * bases1, const String * bases2 );

/* --------------------------------------------------------------------------------------------------- */

bool spot_group_requested( const char * seq_defline, const char * qual_defline );

const char * dflt_seq_defline( bool use_name, bool use_read_id, bool fasta );
const char * dflt_qual_defline( bool use_name, bool use_read_id );

struct flex_printer_t;

typedef struct flex_printer_data_t {
    int64_t row_id;
    uint32_t read_id;    /* to be printed */
    uint32_t dst_id;     /* into which file to print */
    const String * spotname;
    const String * spotgroup;
    const String * read1;
    const String * read2;
    const String * quality;
} flex_printer_data_t;

typedef struct file_printer_args_t {
    KDirectory * dir;
    struct temp_registry_t * registry;
    const char * output_base;
    size_t buffer_size;
} file_printer_args_t;

void set_file_printer_args( file_printer_args_t * self,
                            KDirectory * dir,
                            struct temp_registry_t * registry,
                            const char * output_base,
                            size_t buffer_size );

/* ---------------------------------------------------------------------------------------------------
    there are 2 modes for the flex-printer: file-per-read-id-mode / multi-writer-mode
        file_args   multi_writer
        ------------------------
        NULL        NULL            ... invalid
        ptr         NULL            ... file-per-read-id-mode
        NULL        ptr             ... multi-writer-mode
        ptr         ptr             ... invalid

    accession       ... used in both modes for filling into the flexible defline
    seq_defline     ... user supplied sequence-defline ( if NULL, pick internal default based on fasta and name-mode  )
    qual_defline    ... user supplied quality-defline  ( if NULL, pick internal default based on name-mode, ignored for fasta )
    name_mode       ... flag used to pick a default-defline
    fasta           ... flag used to pick a default-defline
 --------------------------------------------------------------------------------------------------- */
struct flex_printer_t * make_flex_printer( file_printer_args_t * file_args,     /* used in file-per-read-id-mode */
                        struct multi_writer_t * multi_writer,           /* if used: dir,registry,output_base,buffer_size are unused */
                        const char * accession,                         /* used in both modes */
                        const char * seq_defline,                       /* if NULL -> pick default based on fasta/name-mode */
                        const char * qual_defline,                      /* if NULL -> pick default based on fasta/name-mode */
                        bool use_name,                                  /* needed for picking a default */
                        bool use_read_id,                               /* needed for picking a default, split...true, whole...false */
                        bool fasta );

void release_flex_printer( struct flex_printer_t * self );

/* depending on the data:
    quality == NULL ... fasta / fastq
    read2 == NULL ... 1 spot / 2 spots
 */
rc_t join_result_flex_print( struct flex_printer_t * self, const flex_printer_data_t * data );

#ifdef __cplusplus
}
#endif

#endif
