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

#ifndef _h_vdb_dump_context_
#define _h_vdb_dump_context_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <kapp/args.h>
#include <klib/vector.h>
#include <klib/num-gen.h>
#include "vdb-dump-redir.h"

#define OPTION_ROW_ID_ON         "row_id_on"
#define OPTION_LINE_FEED         "line_feed"
#define OPTION_COLNAME_OFF       "colname_off"
#define OPTION_IN_HEX            "in_hex"
#define OPTION_TABLE             "table"
#define OPTION_ROWS              "rows"
#define OPTION_COLUMNS           "columns"
#define OPTION_SCHEMA            "schema"
#define OPTION_SCHEMA_DUMP       "schema_dump"
#define OPTION_TABLE_ENUM        "table_enum"
#define OPTION_COLUMN_ENUM       "column_enum"
#define OPTION_COLUMN_SHORT      "column_enum_short"
#define OPTION_DNA_BASES         "dna_bases"
#define OPTION_MAX_LINE_LEN      "max_length"
#define OPTION_LINE_INDENT       "indent_width"
#define OPTION_FILTER            "filter"
#define OPTION_FORMAT            "format"
#define OPTION_ID_RANGE          "id_range"
#define OPTION_WITHOUT_SRA       "without_sra"
#define OPTION_EXCLUDED_COLUMNS  "exclude"
#define OPTION_BOOLEAN           "boolean"
#define OPTION_OBJVER            "obj_version"
#define OPTION_OBJTS             "obj_timestamp"
#define OPTION_OBJTYPE           "obj_type"
#define OPTION_NUMELEM           "numelem"
#define OPTION_NUMELEMSUM        "numelemsum"
#define OPTION_SHOW_BLOBBING     "blobbing"
#define OPTION_ENUM_PHYS         "phys"
#define OPTION_ENUM_READABLE     "readable"
#define OPTION_ENUM_STATIC       "static"
#define OPTION_IDX_ENUM          "idx-report"
#define OPTION_IDX_RANGE         "idx-range"
#define OPTION_CUR_CACHE         "cur-cache"
#define OPTION_OUT_FILE          "output-file"
#define OPTION_OUT_PATH          "output-path"
#define OPTION_PHASE             "phase"
#define OPTION_GZIP              "gzip"
#define OPTION_BZIP2             "bzip2"
#define OPTION_OUT_BUF_SIZE      "output-buffer-size"
#define OPTION_NO_MULTITHREAD    "disable-multithreading"
#define OPTION_INFO              "info"
#define OPTION_DIFF              "diff"
#define OPTION_SPOTGROUPS        "spotgroups"
/*#define OPTION_SRASCHEMA         "sraschema"*/
#define OPTION_MERGE_RANGES      "merge-ranges"
#define OPTION_SPREAD            "spread"
#define OPTION_APPEND            "append"

#define OPTION_SLICE             "slice"
#define OPTION_INTERACTIVE       "interactive"
#define OPTION_LEN_SPREAD        "len-spread"

#define ALIAS_ROW_ID_ON         "I"
#define ALIAS_LINE_FEED         "l"
#define ALIAS_COLNAME_OFF       "N"
#define ALIAS_IN_HEX            "X"
#define ALIAS_TABLE             "T"
#define ALIAS_ROWS              "R"
#define ALIAS_COLUMNS           "C"
#define ALIAS_SCHEMA            "S"
#define ALIAS_SCHEMA_DUMP       "A"
#define ALIAS_TABLE_ENUM        "E"
#define ALIAS_COLUMN_ENUM       "O"
#define ALIAS_COLUMN_SHORT      "o"
#define ALIAS_DNA_BASES         "D"
#define ALIAS_MAX_LINE_LEN      "M"
#define ALIAS_LINE_INDENT       "i"
#define ALIAS_FILTER            "F"
#define ALIAS_FORMAT            "f"
#define ALIAS_ID_RANGE          "r"
#define ALIAS_WITHOUT_SRA       "n"
#define ALIAS_EXCLUDED_COLUMNS  "x"
#define ALIAS_BOOLEAN           "b"
#define ALIAS_OBJVER            "j"
#define ALIAS_OBJTYPE           "y"
#define ALIAS_NUMELEM           "u"
#define ALIAS_NUMELEMSUM        "U"
#define ALIAS_APPEND            "a"

#define USE_PATHTYPE_TO_DETECT_DB_OR_TAB 1
#define CURSOR_CACHE_SIZE 256*1024*1024
#define DEF_OPTION_OUT_BUF_SIZE 1024*1024

typedef enum dump_format_t
{
    df_default,
    df_csv,
    df_xml,
    df_json,
    df_piped,
    df_sra_dump,
    df_tab,
    df_fastq,
    df_fastq1,
    df_fasta,
    df_fasta1,
    df_fasta2,
    df_qual,
    df_qual1,
    df_bin,
    df_sql
} dump_format_t;

/********************************************************************
the dump context contains all informations needed to execute the dump
********************************************************************/
typedef struct dump_context
{
    const char *path;
    Vector schema_list;
    const char *table;
    const char *columns;
    const char *excluded_columns;
    const char *filter;
    const char *idx_range;
    const char *row_range;
    const char *output_file;
    const char *output_path;
    struct num_gen * rows;
    bool print_row_id;
    uint16_t lf_after_row;
    uint16_t max_line_len;
    uint16_t indented_line_len;
    uint16_t phase;
    uint32_t generic_idx;
    uint32_t slice_depth;
    size_t cur_cache_size;
    size_t output_buffer_size;
    dump_format_t format;
    out_redir_mode_t compress_mode;
    char c_boolean;

    bool print_column_names;
    bool print_in_hex;
    bool print_dna_bases;
    bool help_requested;
    bool usage_requested;
    bool schema_dump_requested;
    bool table_enum_requested;
    bool version_requested;
    bool column_enum_requested;
    bool column_enum_short;
    bool id_range_requested;
    bool without_sra_types;
    bool objver_requested;
    bool objts_requested;
    bool objtype_requested;
    bool print_num_elem;
    bool sum_num_elem;
    bool show_blobbing;
    bool enum_phys;
    bool enum_readable;
    bool enum_static;
    bool idx_enum_requested;
    bool idx_range_requested;
    bool disable_multithreading;
    bool print_info;
    bool table_defined;
    bool diff;
    bool show_spotgroups;
    bool merge_ranges;
    bool show_spread;
    bool interactive;
    bool len_spread;
    bool append;
} dump_context;
typedef dump_context* p_dump_context;


rc_t vdco_init( dump_context **ctx );
rc_t vdco_destroy( p_dump_context ctx );

size_t vdco_schema_count( p_dump_context ctx );

void vdco_show_usage( p_dump_context ctx );
void vdco_show_help( p_dump_context ctx );

rc_t vdco_set_table( p_dump_context ctx, const char *src );
rc_t vdco_set_table_String( p_dump_context ctx, const String * src );

rc_t vdco_capture_arguments_and_options( const Args * args, dump_context *ctx );

#ifdef __cplusplus
}
#endif

#endif
