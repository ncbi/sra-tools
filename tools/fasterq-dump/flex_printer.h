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
#ifndef _h_flex_printer_
#define _h_flex_printer_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h> 
#include <stdint.h>

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_temp_registry_
#include "temp_registry.h"
#endif

#ifndef _h_copy_machine_
#include "copy_machine.h"
#endif

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
    accession       ... used in both modes for filling into the flexible defline
    seq_defline     ... user supplied sequence-defline ( if NULL -> error  )
    qual_defline    ... user supplied quality-defline  ( if NULL and FASTQ -> error )
    fasta           ... flag used to pick a default-defline
 --------------------------------------------------------------------------------------------------- */

/* for file-per-read-id-mode */
struct flex_printer_t * make_flex_printer_1( file_printer_args_t * file_args,
                        const char * accession,
                        const char * seq_defline,
                        const char * qual_defline,
                        bool fasta );

/* for multi-writer-mode */
struct flex_printer_t * make_flex_printer_2( struct multi_writer_t * multi_writer,
                        const char * accession,
                        const char * seq_defline,
                        const char * qual_defline,
                        bool fasta );

void release_flex_printer( struct flex_printer_t * self );

/* depending on the data:
    quality == NULL ... fasta / fastq
    read2 == NULL ... 1 spot / 2 spots
 */
rc_t flex_print( struct flex_printer_t * self, const flex_printer_data_t * data );


#ifdef __cplusplus
}
#endif

#endif
