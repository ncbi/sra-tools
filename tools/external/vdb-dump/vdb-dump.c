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

#include <vdb/manager.h>
#include <vdb/dependencies.h>
#include <vdb/vdb-priv.h>

#include <kdb/table.h>
#include <kdb/column.h>
#include <kdb/manager.h>
#include <kdb/namelist.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/args-conv.h>

#include <klib/container.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/debug.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/time.h>
#include <klib/num-gen.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <bitstr.h>

#include "vdb-dump-context.h"
#include "vdb-dump-coldefs.h"
#include "vdb-dump-tools.h"
#include "vdb-dump-helper.h"
#include "vdb-dump-row-context.h"
#include "vdb-dump-formats.h"
#include "vdb-dump-fastq.h"
#include "vdb-dump-redir.h"
#include "vdb_info.h"
#include "vdb-dump-view-spec.h"
#include "vdb-dump-inspect.h"

static const char * row_id_on_usage[]           = { "print row id",                                 NULL };
static const char * line_feed_usage[]           = { "line-feed's inbetween rows",                   NULL };
static const char * colname_off_usage[]         = { "do not print column-names",                    NULL };
static const char * in_hex_usage[]              = { "print numbers in hex",                         NULL };
static const char * table_usage[]               = { "table-name",                                   NULL };
static const char * rows_usage[]                = { "rows (default = all)",                         NULL };
static const char * columns_usage[]             = { "columns (default = all)",                      NULL };
static const char * schema_usage[]              = { "schema-name",                                  NULL };
static const char * schema_dump_usage[]         = { "dumps the schema",                             NULL };
static const char * table_enum_usage[]          = { "enumerates tables",                            NULL };
static const char * column_enum_usage[]         = { "enumerates columns in extended form",          NULL };
static const char * column_short_usage[]        = { "enumerates columns in short form",             NULL };
static const char * dna_bases_usage[]           = { "force dna-bases if column fits pattern",       NULL };
static const char * max_line_len_usage[]        = { "limits line length",                           NULL };
static const char * line_indent_usage[]         = { "indents the line",                             NULL };
static const char * filter_usage[]              = { "filters lines",                                NULL };
static const char * format_usage[]              = { "output format:",                               NULL };
static const char * id_range_usage[]            = { "prints id-range",                              NULL };
static const char * without_sra_usage[]         = { "without sra-type-translation",                 NULL };
static const char * excluded_columns_usage[]    = { "exclude these columns",                        NULL };
static const char * boolean_usage[]             = { "defines how boolean's are printed (1,T)",      NULL };
static const char * objver_usage[]              = { "request vdb-version",                          NULL };
static const char * objts_usage[]               = { "request object modification date",             NULL };
static const char * numelem_usage[]             = { "print only element-count",                     NULL };
static const char * numelemsum_usage[]          = { "sum element-count",                            NULL };
static const char * show_kdb_blobs_usage[]      = { "show physical blobs",                          NULL };
static const char * show_vdb_blobs_usage[]      = { "show VDB-blobs",                               NULL };
static const char * enum_phys_usage[]           = { "enumerate physical columns",                   NULL };
static const char * enum_readable_usage[]       = { "enumerate readable columns",                   NULL };
static const char * enum_static_usage[]         = { "enumerate static columns",                     NULL };
static const char * objtype_usage[]             = { "report type of object",                        NULL };
static const char * idx_enum_usage[]            = { "enumerate all available index",                NULL };
static const char * idx_range_usage[]           = { "enumerate values and row-ranges of one index", NULL };
static const char * cur_cache_usage[]           = { "size of cursor cache",                         NULL };
static const char * out_file_usage[]            = { "write output to this file",                    NULL };
static const char * out_path_usage[]            = { "write output to this directory",               NULL };
static const char * gzip_usage[]                = { "compress output using gzip",                   NULL };
static const char * bzip2_usage[]               = { "compress output using bzip2",                  NULL };
static const char * outbuf_size_usage[]         = { "size of output-buffer, 0...none",              NULL };
static const char * disable_mt_usage[]          = { "disable multithreading",                       NULL };
static const char * info_usage[]                = { "print info about run",                         NULL };
static const char * spotgroup_usage[]           = { "show spotgroups",                              NULL };
static const char * merge_ranges_usage[]        = { "merge and sort row-ranges",                    NULL };
static const char * spread_usage[]              = { "show spread of integer values",                NULL };
static const char * append_usage[]              = { "append to output-file, if output-file used",   NULL };
static const char * ngc_usage[]                 = { "path to ngc file",                             NULL };
static const char * view_usage[]                = { "view-name",                                    NULL };
static const char * inspect_usage[]             = { "inspect data usage inside object",             NULL };

/* from here on: not mentioned in help */
static const char * len_spread_usage[]          = { "show spread of READ/REF_LEN values",           NULL };
static const char * slice_usage[]               = { "find a slice of given depth",                  NULL };

/* OPTION_XXX and ALIAS_XXX in vdb-dump-contest.h */
OptDef DumpOptions[] =
{
    { OPTION_ROW_ID_ON,             ALIAS_ROW_ID_ON,          NULL, row_id_on_usage,         1, false,  false },
    { OPTION_LINE_FEED,             ALIAS_LINE_FEED,          NULL, line_feed_usage,         1, true,   false },
    { OPTION_COLNAME_OFF,           ALIAS_COLNAME_OFF,        NULL, colname_off_usage,       1, false,  false },
    { OPTION_IN_HEX,                ALIAS_IN_HEX,             NULL, in_hex_usage,            1, false,  false },
    { OPTION_TABLE,                 ALIAS_TABLE,              NULL, table_usage,             1, true,   false },
    { OPTION_ROWS,                  ALIAS_ROWS,               NULL, rows_usage,              1, true,   false },
    { OPTION_COLUMNS,               ALIAS_COLUMNS,            NULL, columns_usage,           1, true,   false },
    { OPTION_SCHEMA,                ALIAS_SCHEMA,             NULL, schema_usage,            5, true,   false },
    { OPTION_SCHEMA_DUMP,           ALIAS_SCHEMA_DUMP,        NULL, schema_dump_usage,       1, false,  false },
    { OPTION_TABLE_ENUM,            ALIAS_TABLE_ENUM,         NULL, table_enum_usage,        1, false,  false },
    { OPTION_COLUMN_ENUM,           ALIAS_COLUMN_ENUM,        NULL, column_enum_usage,       1, false,  false },
    { OPTION_COLUMN_SHORT,          ALIAS_COLUMN_SHORT,       NULL, column_short_usage,      1, false,  false },
    { OPTION_DNA_BASES,             ALIAS_DNA_BASES,          NULL, dna_bases_usage,         1, false,  false },
    { OPTION_MAX_LINE_LEN,          ALIAS_MAX_LINE_LEN,       NULL, max_line_len_usage,      1, true,   false },
    { OPTION_LINE_INDENT,           ALIAS_LINE_INDENT,        NULL, line_indent_usage,       1, true,   false },
    { OPTION_FILTER,                ALIAS_FILTER,             NULL, filter_usage,            1, true,   false },
    { OPTION_FORMAT,                ALIAS_FORMAT,             NULL, format_usage,            1, true,   false },
    { OPTION_ID_RANGE,              ALIAS_ID_RANGE,           NULL, id_range_usage,          1, false,  false },
    { OPTION_WITHOUT_SRA,           ALIAS_WITHOUT_SRA,        NULL, without_sra_usage,       1, false,  false },
    { OPTION_EXCLUDED_COLUMNS,      ALIAS_EXCLUDED_COLUMNS,   NULL, excluded_columns_usage,  1, true,   false },
    { OPTION_BOOLEAN,               ALIAS_BOOLEAN,            NULL, boolean_usage,           1, true,   false },
    { OPTION_NUMELEM,               ALIAS_NUMELEM,            NULL, numelem_usage,           1, false,  false },
    { OPTION_NUMELEMSUM,            ALIAS_NUMELEMSUM,         NULL, numelemsum_usage,        1, false,  false },
    { OPTION_SHOW_KDB_BLOBS,        NULL,                     NULL, show_kdb_blobs_usage,    1, false,  false },
    { OPTION_SHOW_VDB_BLOBS,        NULL,                     NULL, show_vdb_blobs_usage,    1, false,  false },
    { OPTION_ENUM_PHYS,             NULL,                     NULL, enum_phys_usage,         1, false,  false },
    { OPTION_ENUM_READABLE,         NULL,                     NULL, enum_readable_usage,     1, false,  false },
    { OPTION_ENUM_STATIC,           NULL,                     NULL, enum_static_usage,       1, false,  false },
    { OPTION_OBJVER,                ALIAS_OBJVER,             NULL, objver_usage,            1, false,  false },
    { OPTION_OBJTS,                 NULL,                     NULL, objts_usage,             1, false,  false },
    { OPTION_OBJTYPE,               ALIAS_OBJTYPE,            NULL, objtype_usage,           1, false,  false },
    { OPTION_IDX_ENUM,              NULL,                     NULL, idx_enum_usage,          1, false,  false },
    { OPTION_IDX_RANGE,             NULL,                     NULL, idx_range_usage,         1, true,   false },
    { OPTION_CUR_CACHE,             NULL,                     NULL, cur_cache_usage,         1, true,   false },
    { OPTION_OUT_FILE,              NULL,                     NULL, out_file_usage,          1, true,   false },
    { OPTION_OUT_PATH,              NULL,                     NULL, out_path_usage,          1, true,   false },
    { OPTION_GZIP,                  NULL,                     NULL, gzip_usage,              1, false,  false },
    { OPTION_BZIP2,                 NULL,                     NULL, bzip2_usage,             1, false,  false },
    { OPTION_OUT_BUF_SIZE,          NULL,                     NULL, outbuf_size_usage,       1, true,   false },
    { OPTION_NO_MULTITHREAD,        NULL,                     NULL, disable_mt_usage,        1, false,  false },
    { OPTION_INFO,                  NULL,                     NULL, info_usage,              1, false,  false },
    { OPTION_SPOTGROUPS,            NULL,                     NULL, spotgroup_usage,         1, false,  false },
    { OPTION_MERGE_RANGES,          NULL,                     NULL, merge_ranges_usage,      1, false,  false },
    { OPTION_SPREAD,                NULL,                     NULL, spread_usage,            1, false,  false },
    { OPTION_APPEND,                ALIAS_APPEND,             NULL, append_usage,            1, false,  false },
    { OPTION_LEN_SPREAD,            NULL,                     NULL, len_spread_usage,        1, false,  false },
    { OPTION_SLICE,                 NULL,                     NULL, slice_usage,             1, true,   false },
    { OPTION_CELL_DEBUG,            NULL,                     NULL, NULL,                    1, false,  false },
    { OPTION_CELL_V1,               NULL,                     NULL, NULL,                    1, false,  false },
    { OPTION_NGC,                   NULL,                     NULL, ngc_usage,               1, true,   false },
    { OPTION_VIEW,                  NULL,                     NULL, view_usage,              1, true,   false },
    { OPTION_INSPECT,               NULL,                     NULL, inspect_usage,           1, false,  false }
};

const char UsageDefaultName[] = "vdb-dump";


rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s <path> [<path> ...] [options]\n"
                    "\n", progname);
}


rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( args == NULL ) {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    } else {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }

    if ( 0 != rc ) {
        progname = fullpath = UsageDefaultName;
    }

    UsageSummary ( progname );
    KOutMsg ( "Options:\n" );
    HelpOptionLine ( ALIAS_ROW_ID_ON,           OPTION_ROW_ID_ON,       NULL,           row_id_on_usage );
    HelpOptionLine ( ALIAS_LINE_FEED,           OPTION_LINE_FEED,       "line_feed",    line_feed_usage );
    HelpOptionLine ( ALIAS_COLNAME_OFF,         OPTION_COLNAME_OFF,     NULL,           colname_off_usage );
    HelpOptionLine ( ALIAS_IN_HEX,              OPTION_IN_HEX,          NULL,           in_hex_usage );
    HelpOptionLine ( ALIAS_TABLE,               OPTION_TABLE,           "table",        table_usage );
    HelpOptionLine ( ALIAS_ROWS,                OPTION_ROWS,            "rows",         rows_usage );
    HelpOptionLine ( ALIAS_COLUMNS,             OPTION_COLUMNS,         "columns",      columns_usage );
    HelpOptionLine ( ALIAS_SCHEMA,              OPTION_SCHEMA,          "schema",       schema_usage );
    HelpOptionLine ( ALIAS_SCHEMA_DUMP,         OPTION_SCHEMA_DUMP,     NULL,           schema_dump_usage );
    HelpOptionLine ( ALIAS_TABLE_ENUM,          OPTION_TABLE_ENUM,      NULL,           table_enum_usage );
    HelpOptionLine ( ALIAS_COLUMN_ENUM,         OPTION_COLUMN_ENUM,     NULL,           column_enum_usage );
    HelpOptionLine ( ALIAS_COLUMN_SHORT,        OPTION_COLUMN_SHORT,    NULL,           column_short_usage );
    HelpOptionLine ( ALIAS_DNA_BASES,           OPTION_DNA_BASES,       NULL,           dna_bases_usage );
    HelpOptionLine ( ALIAS_MAX_LINE_LEN,        OPTION_MAX_LINE_LEN,    "max_length",   max_line_len_usage );
    HelpOptionLine ( ALIAS_LINE_INDENT,         OPTION_LINE_INDENT,     "indent_width", line_indent_usage );
    HelpOptionLine ( ALIAS_FORMAT,              OPTION_FORMAT,          "format",       format_usage );
    KOutMsg( "      csv ..... comma separated values on one line\n" );
    KOutMsg( "      xml ..... xml-style without complete xml-frame\n" );
    KOutMsg( "      json .... json-style\n" );
    KOutMsg( "      piped ... 1 line per cell: row-id, column-name: value\n" );
    KOutMsg( "      tab ..... 1 line per row: tab-separated values only\n" );
    KOutMsg( "      fastq ... FASTQ( 4 lines ) for each row\n" );
    KOutMsg( "      fastq1 .. FASTQ( 4 lines ) for each fragment\n" );
    KOutMsg( "      fasta ... FASTA( 2 lines ) for each fragment if possible\n" );
    KOutMsg( "      fasta1 .. one FASTA-record for the whole accession (REFSEQ)\n" );
    KOutMsg( "      fasta2 .. one FASTA-record for each REFERENCE in cSRA\n" );
    KOutMsg( "      qual .... QUAL( 2 lines ) for each row\n" );
    KOutMsg( "      qual1 ... QUAL( 2 lines ) for each fragment if possible\n\n" );
    HelpOptionLine ( ALIAS_ID_RANGE,            OPTION_ID_RANGE,        NULL,           id_range_usage );
    HelpOptionLine ( ALIAS_WITHOUT_SRA,         OPTION_WITHOUT_SRA,     NULL,           without_sra_usage );
    HelpOptionLine ( ALIAS_EXCLUDED_COLUMNS,    OPTION_EXCLUDED_COLUMNS,"columns",      excluded_columns_usage );
    HelpOptionLine ( ALIAS_BOOLEAN,             OPTION_BOOLEAN,         "1 or T",       boolean_usage );
    HelpOptionLine ( ALIAS_OBJVER,              OPTION_OBJVER,          NULL,           objver_usage );
    HelpOptionLine ( NULL,                      OPTION_OBJTS,           NULL,           objts_usage );
    HelpOptionLine ( ALIAS_OBJTYPE,             OPTION_OBJTYPE,         NULL,           objtype_usage );
    HelpOptionLine ( ALIAS_NUMELEM,             OPTION_NUMELEM,         NULL,           numelem_usage );
    HelpOptionLine ( ALIAS_NUMELEMSUM,          OPTION_NUMELEMSUM,      NULL,           numelemsum_usage );
    HelpOptionLine ( NULL,                      OPTION_SHOW_KDB_BLOBS,  NULL,           show_kdb_blobs_usage );
    HelpOptionLine ( NULL,                      OPTION_SHOW_VDB_BLOBS,  NULL,           show_vdb_blobs_usage );
    HelpOptionLine ( NULL,                      OPTION_ENUM_PHYS,       NULL,           enum_phys_usage );
    HelpOptionLine ( NULL,                      OPTION_ENUM_READABLE,   NULL,           enum_readable_usage );
    HelpOptionLine ( NULL,                      OPTION_IDX_ENUM,        NULL,           idx_enum_usage );
    HelpOptionLine ( NULL,                      OPTION_IDX_RANGE,       "idx-name",     idx_range_usage );
    HelpOptionLine ( NULL,                      OPTION_CUR_CACHE,       "size",         cur_cache_usage );
    HelpOptionLine ( NULL,                      OPTION_OUT_FILE,        "filename",     out_file_usage );
    HelpOptionLine ( NULL,                      OPTION_OUT_PATH,        "path",         out_path_usage );
    HelpOptionLine ( NULL,                      OPTION_GZIP,            NULL,           gzip_usage );
    HelpOptionLine ( NULL,                      OPTION_BZIP2,           NULL,           bzip2_usage );
    HelpOptionLine ( NULL,                      OPTION_OUT_BUF_SIZE,    "size",         outbuf_size_usage );
    HelpOptionLine ( NULL,                      OPTION_NO_MULTITHREAD,  NULL,           disable_mt_usage );
    HelpOptionLine ( NULL,                      OPTION_INFO,            NULL,           info_usage );
    HelpOptionLine ( NULL,                      OPTION_SPOTGROUPS,      NULL,           spotgroup_usage );
    HelpOptionLine ( NULL,                      OPTION_MERGE_RANGES,    NULL,           merge_ranges_usage );
    HelpOptionLine ( NULL,                      OPTION_SPREAD,          NULL,           spread_usage );
    HelpOptionLine ( ALIAS_APPEND,              OPTION_APPEND,          NULL,           append_usage );
    HelpOptionLine ( NULL,                      OPTION_NGC,             "path",         ngc_usage);
    HelpOptionLine ( NULL,                      OPTION_VIEW,            "view",         view_usage );
    HelpOptionLine ( NULL,                      OPTION_INSPECT,         NULL,           inspect_usage );

    HelpOptionsStandard ();

    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

/*************************************************************************************
    read_cell_data_and_dump:
    * called by "dump_rows()" via VectorForEach() for every column in a row
    * extracts the column-definition from the item, the row-context from the data-ptr
    * clears the column-text-buffer (part of the column-data-struct)
    * reads the cell-data from the cursor
    * eventually detects a unknown data-type
    * detects if this column has a dna-format ( special treatment for printing )
    * loops throuh the elements of a cell
        - calls "dump_element()" for every element (from vdb-dump-tools.c)

item    [IN] ... pointer to col-data ( definition and buffer )
data    [IN] ... pointer to row-context( cursor, dump_context, col_defs ... )
*************************************************************************************/
static void CC vdm_read_cell_data( void *item, void *data ) {
    dump_src src; /* defined in vdb-dump-tools.h */
    p_col_def col_def = ( p_col_def )item;
    p_row_context r_ctx = ( p_row_context )data; /* vdb-dump-row-context.h */

    if ( 0 != r_ctx -> rc ) return; /* important to stop if the last read was not successful */
    vds_clear( &( col_def -> content ) ); /* clear the destination-dump-string */
    if ( !col_def -> valid ) return;
    if ( col_def -> excluded ) return;

    /* to make vdt_dump_element() aware of the output-format ( df_json, df_xml, etc ) */
    src . output_format = r_ctx -> ctx -> format;

    /* read the data of a cursor-cell: buffer-addr, offset and element-count
       is stored in the dump_src-struct */
    r_ctx -> rc = VCursorCellData( r_ctx -> cursor, col_def -> idx, NULL, &( src . buf ),
                                 &( src . offset_in_bits ), &( src . number_of_elements ) );
    if ( 0 != r_ctx->rc ) {
        if ( r_ctx->view )
        {
            //TODO: make it work for views
            //if ( UIError( r_ctx -> rc, NULL, r_ctx -> table ) ) {
            //    UITableLOGError( r_ctx -> rc, r_ctx -> table, true );

            // do not complain about missing cells if we are in a view
            if ( r_ctx->rc == SILENT_RC ( rcVDB, rcColumn, rcReading, rcRow, rcNotFound ) ) {
                r_ctx -> rc = 0;
            } else {
                PLOGERR( klogInt,
                        (klogInt,
                        r_ctx -> rc,
                        "VCursorCellData( col:$(col_name) at row #$(row_nr) ) failed",
                        "col_name=%s,row_nr=%lu",
                        col_def -> name, r_ctx -> row_id ));
            }
        }
        if ( r_ctx->table ) {
            if ( UIError( r_ctx -> rc, NULL, r_ctx -> table ) ) {
                UITableLOGError( r_ctx -> rc, r_ctx -> table, true );
            } else {
                PLOGERR( klogInt,
                        (klogInt,
                        r_ctx -> rc,
                        "VCursorCellData( col:$(col_name) at row #$(row_nr) ) failed",
                        "col_name=%s,row_nr=%lu",
                        col_def -> name, r_ctx -> row_id ));
            }
        }

        /* remember the last error */
        r_ctx -> last_rc = r_ctx -> rc;
        /* be forgiving and continue if a cell cannot be read */
        r_ctx -> rc = 0;
    }

    /* check the type-domain */
    if ( ( col_def -> type_desc . domain < vtdBool ) ||
         ( col_def -> type_desc . domain > vtdUnicode ) ) {
        vds_append_str( &( col_def -> content ), "unknown data-type" );
    } else {
        src . print_comma = !( col_def -> type_desc . domain == vtdBool && r_ctx -> ctx -> c_boolean != 0 );

        /* special treatment to suppress spaces between values */
        src . translate_sra_values = ( r_ctx -> ctx -> format == df_sra_dump );

        /* transfer context-flags (hex-print, no sra-types) */
        src . in_hex = r_ctx -> ctx -> print_in_hex;
        src . without_sra_types = r_ctx -> ctx -> without_sra_types;

        /* how a boolean is displayed */
        src . c_boolean = r_ctx -> ctx -> c_boolean;

        /* hardcoded printing of dna-bases if the column-type fits */
        src . print_dna_bases = ( r_ctx -> ctx -> print_dna_bases &
                    ( col_def -> type_desc . intrinsic_dim == 2 ) &
                    ( col_def -> type_desc . intrinsic_bits == 1 ) );

        if ( r_ctx -> ctx -> print_num_elem ) {
            char temp[ 16 ];
            size_t num_writ;

            r_ctx -> rc = string_printf ( temp, sizeof temp, &num_writ, "%u", src . number_of_elements );
            if ( 0 == r_ctx -> rc ) {
                vds_append_str( &( col_def -> content ), temp );
            }
        } else if ( r_ctx -> ctx -> sum_num_elem ) {
            col_def -> elementsum += src . number_of_elements;
        } else {
            if ( r_ctx -> ctx -> cell_v1 ) {
                /* the version1 ( legacy ) of cell-formatting ( for comparison ) in vdb-dump-tools.c */
                r_ctx -> rc = vdt_format_cell_v1( &src, col_def, r_ctx -> ctx -> cell_debug );
            } else {
                /* the version2 of cell-formatting: faster, correct json in vdb-dump-tools.c */
                r_ctx -> rc = vdt_format_cell_v2( &src, col_def, r_ctx -> ctx -> cell_debug );
            }
        }
    }
}

static void CC vdm_print_elem_sum( void *item, void *data ) {
    char temp[ 16 ];
    size_t num_writ;
    p_col_def col_def = ( p_col_def )item;
    p_row_context r_ctx = ( p_row_context )data;

    if ( 0 != r_ctx -> rc ) return; /* important to stop if the last read was not successful */
    vds_clear( &( col_def -> content ) ); /* clear the destination-dump-string */

    r_ctx -> rc = string_printf ( temp, sizeof temp, &num_writ, "%,zu", col_def -> elementsum );
    if ( 0 == r_ctx -> rc ) {
        vds_append_str( &( col_def -> content ), temp );
    }
}

static void vdm_row_error( const char * fmt, rc_t rc, uint64_t row_id ) {
    PLOGERR( klogInt, ( klogInt, rc, fmt, "row_nr=%lu", row_id ) );
}

/*************************************************************************************
    dump_rows:
    * is the main loop to dump all rows or all selected rows ( -R1-10 )
    * creates a dump-string ( parameterizes it with the wanted max. line-len )
    * starts the number-generator
    * as long as the number-generator has a number and the result-code is ok
      do for every row-id:
        - set the row-id into the cursor and open the cursor-row
        - loop throuh the columns
        - close the row
        - call print_row (vdb-dump-formats.c) which actually prints the row
    * the collection of the text's for the columns "read_cell_data_and_dump()"
      is separated from the actual printing "print_row()" !

r_ctx   [IN] ... row-context ( cursor, dump_context, col_defs ... )
*************************************************************************************/
static rc_t vdm_dump_rows( p_row_context r_ctx ) {
    /* the important row_id is a member of r_ctx ! */
    const struct num_gen_iter * iter;

    r_ctx -> rc = vds_make( &( r_ctx -> s_col ), r_ctx -> ctx->max_line_len, 512 ); /* vdb-dump-str.sh */
    DISP_RC( r_ctx -> rc, "vdm_dump_rows().vds_make() failed" );
    if ( 0 == r_ctx -> rc ) {
        r_ctx -> rc = num_gen_iterator_make( r_ctx -> ctx -> rows, &iter );
        DISP_RC( r_ctx -> rc, "vdm_dump_rows().num_gen_iterator_make() failed" );
        if ( 0 == r_ctx -> rc ) {
            uint64_t count;
            r_ctx -> rc = num_gen_iterator_count( iter, &count );
            DISP_RC( r_ctx -> rc, "vdm_dump_rows().num_gen_iterator_count() failed" );
            if ( 0 == r_ctx -> rc ) {
                uint64_t num = 0;
                while ( ( 0 == r_ctx -> rc ) &&
                        num_gen_iterator_next( iter, &( r_ctx -> row_id ), &( r_ctx -> rc ) ) ) {
                    if ( 0 == r_ctx -> rc ) {
                        r_ctx -> rc = Quitting();
                    }
                    if ( 0 != r_ctx -> rc ) break;
                    r_ctx -> rc = VCursorSetRowId( r_ctx -> cursor, r_ctx -> row_id );
                    if ( 0 != r_ctx -> rc ) {
                        vdm_row_error( "vdm_dump_rows().VCursorSetRowId( row#$(row_nr) ) failed",
                                    r_ctx -> rc, r_ctx -> row_id ); /* above */
                    } else {
                        r_ctx -> rc = VCursorOpenRow( r_ctx -> cursor );
                        if ( 0 != r_ctx -> rc ) {
                            vdm_row_error( "vdm_dump_rows().VCursorOpenRow( row#$(row_nr) ) failed",
                                        r_ctx -> rc, r_ctx -> row_id ); /* above */
                        } else {
                            /* first reset the string and valid-flag for every column */
                            vdcd_reset_content( r_ctx -> col_defs );
                            /* read the data of every column and create a string for it */
                            VectorForEach( &( r_ctx -> col_defs -> cols ), false, vdm_read_cell_data, r_ctx );
                            if ( 0 == r_ctx -> rc ) {
                                /* prints the collected strings, in vdb-dump-formats.c */
                                if ( !r_ctx -> ctx -> sum_num_elem ) {
                                    bool first = ( 0 == num );
                                    bool last  = ( num >= count - 1 );
                                    r_ctx -> rc = vdfo_print_row( r_ctx, first, last ); /* in vdb-dump-formats.c */
                                    if ( 0 != r_ctx -> rc ) {
                                        vdm_row_error( "vdm_dump_rows().vdfo_print_row( row#$(row_nr) ) failed",
                                            r_ctx -> rc, r_ctx -> row_id ); /* above */
                                    }
                                }
                            }
                            r_ctx -> rc = VCursorCloseRow( r_ctx -> cursor );
                            if ( 0 != r_ctx -> rc ) {
                                vdm_row_error( "vdm_dump_rows().VCursorCloseRow( row#$(row_nr) ) failed",
                                            r_ctx -> rc, r_ctx -> row_id ); /* above */
                            }
                        }
                    }
                    num += 1;
                } /* while( ... ) */
            }
        }
        num_gen_iterator_destroy( iter );
        /* in case the user selected element-sum on the commandline ( -U|--numelemsum )*/
        if ( 0 == r_ctx -> rc && r_ctx -> ctx -> sum_num_elem ) {
            VectorForEach( &( r_ctx -> col_defs -> cols ), false, vdm_print_elem_sum, r_ctx );
            if ( 0 == r_ctx -> rc ) {
                r_ctx -> rc = vdfo_print_row( r_ctx, true, true /* always the first and last (because the only ) row */ );
                DISP_RC( r_ctx -> rc, "vdm_dump_rows().vdfo_print_row() failed" );
            }
        }
        vds_free( &( r_ctx -> s_col ) ); /* vdb-dump-str.c */
    }
    return r_ctx -> rc;
}

static uint32_t vdm_extract_or_parse_columns( const p_dump_context ctx, const VTable *tbl,
                                              p_col_defs col_defs, uint32_t *invalid_columns ) {
    uint32_t count = 0;
    if ( NULL != invalid_columns ) {
        *invalid_columns = 0;
    }
    if ( NULL != ctx && NULL != col_defs ) {
        bool cols_unknown = ( ( NULL == ctx -> columns ) || 0 == ( string_cmp( ctx -> columns, 1, "*", 1, 1 ) ) );
        if ( cols_unknown ) {
            if ( ctx -> enum_static ) {
                /* the user wants to see only the static columns */
                count = vdcd_extract_static_columns( col_defs, tbl, ctx -> max_line_len, invalid_columns );
                if ( count > 0 ) {
                    /* if we found some static columns, let's restrict the row-count
                       if the user did not give a specific row-set to just show row #1 */
                    if ( NULL == ctx -> rows ) {
                        rc_t rc = num_gen_make_from_range( &ctx -> rows, 1, 1 );
                        DISP_RC( rc, "num_gen_make_from_range() failed" );
                    }
                }
            } else {
                /* the user does not know the column-names or wants all of them */
                count = vdcd_extract_from_table( col_defs, tbl, invalid_columns );
            }
        } else {
            /* the user knows the names of the wanted columns... */
            count = vdcd_parse_string( col_defs, ctx -> columns, tbl, invalid_columns );
        }
        vdcd_exclude_these_columns( col_defs, ctx -> excluded_columns );
    }
    return count;
}

static bool vdm_extract_or_parse_phys_columns( const p_dump_context ctx, const VTable *tbl,
                                               p_col_defs col_defs, uint32_t * invalid_columns ) {
    bool res = false;
    if ( NULL != invalid_columns ) {
            *invalid_columns = 0;
    }
    if ( NULL != ctx && NULL != col_defs ) {
        bool cols_unknown = ( ( NULL == ctx -> columns ) || ( 0 == string_cmp( ctx -> columns, 1, "*", 1, 1 ) ) );
        if ( cols_unknown ) {
            /* the user does not know the column-names or wants all of them */
            res = vdcd_extract_from_phys_table( col_defs, tbl );
        } else {
            /* the user knows the names of the wanted columns... */
            res = vdcd_parse_string( col_defs, ctx -> columns, tbl, invalid_columns );
        }
        vdcd_exclude_these_columns( col_defs, ctx -> excluded_columns );
    }
    return res;
}

static bool vdm_extract_or_parse_static_columns( const p_dump_context ctx, const VTable *tbl,
                                                 p_col_defs col_defs, uint32_t * invalid_columns ) {
    bool res = false;
    if ( NULL != invalid_columns ) {
        *invalid_columns = 0;
    }
    if ( NULL != ctx && NULL != col_defs ) {
            /* the user does not know the column-names or wants all of them */
        uint32_t valid_columns = vdcd_extract_static_columns( col_defs, tbl,
                                    ctx -> max_line_len, invalid_columns );
        res = ( valid_columns > 0 );
        vdcd_exclude_these_columns( col_defs, ctx -> excluded_columns );
    }
    return res;
}

static uint32_t vdm_extract_or_parse_columns_view( const p_dump_context ctx,
                                                   const VView *my_view,
                                                   p_col_defs my_col_defs,
                                                   uint32_t * invalid_columns ) {
    uint32_t count = 0;
    if ( NULL != ctx && NULL != my_col_defs ) {
        bool cols_unknown = ( ( NULL == ctx -> columns ) || ( string_cmp( ctx -> columns, 1, "*", 1, 1 ) == 0 ) );
        if ( cols_unknown ) {
            /* the user does not know the column-names or wants all of them */
            count = vdcd_extract_from_view( my_col_defs, my_view, invalid_columns );
        } else {
            /* the user knows the names of the wanted columns... */
            count = vdcd_parse_string_view( my_col_defs, ctx->columns, my_view );
        }
        if ( NULL != ctx -> excluded_columns ) {
            vdcd_exclude_these_columns( my_col_defs, ctx->excluded_columns );
        }
    }
    return count;
}

/*************************************************************************************
    dump_tab_table:
    * called by "dump_db_table()" and "dump_tab()" as a fkt-pointer
    * opens a cursor to read
    * checks if the user did not specify columns, or wants all columns ( "*" )
        no columns specified ---> calls "col_defs_extract_from_table()"
        columns specified ---> calls "col_defs_parse_string()"
    * we end up with a list of column-definitions (name,type) in col_defs
    * calls "col_defs_add_to_cursor()" to add them to the cursor
    * opens the cursor
    * calls "dump_rows()" to execute the dump
    * destroys the col_defs - structure
    * releases the cursor

ctx [IN] ... contains path, tablename, columns, row-range etc.
tbl [IN] ... open table needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_opened_table( const p_dump_context ctx, const VTable *tbl ) {
    row_context r_ctx;
    rc_t rc = VTableCreateCachedCursorRead( tbl, &( r_ctx . cursor ), ctx -> cur_cache_size );
    r_ctx . last_rc = 0;
    DISP_RC( rc, "VTableCreateCursorRead() failed" );
    if ( 0 == rc ) {
        r_ctx . table = tbl;
        r_ctx . view = NULL;
        if ( !vdcd_init( &( r_ctx . col_defs ), ctx -> max_line_len ) ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }
        if ( 0 == rc ) {
            uint32_t invalid_columns = 0;
            uint32_t n = vdm_extract_or_parse_columns( ctx, tbl, r_ctx . col_defs, &invalid_columns );
            if ( n < 1 ) {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            } else {
                n = vdcd_add_to_cursor( r_ctx . col_defs, r_ctx . cursor );
                if ( n < 1 ) {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                } else {
                    {
                        /* if this fails, we do not have name-translations for special cell-values
                        but we do not abort because of it ... */
                        const VSchema *schema;
                        rc_t rc2 = VTableOpenSchema( tbl, &schema );
                        DISP_RC( rc2, "VTableOpenSchema() failed" );
                        if ( 0 == rc2 ) {
                            /* translate in special columns to numeric values to strings */
                            vdcd_ins_trans_fkt( r_ctx . col_defs, schema );
                            vdh_vschema_release( rc, schema );
                        }
                    }
                    rc = VCursorOpen( r_ctx . cursor );
                    DISP_RC( rc, "VCursorOpen() failed" );
                    if ( 0 == rc ) {
                        int64_t  first;
                        uint64_t count;
                        rc = VCursorIdRange( r_ctx . cursor, 0, &first, &count );
                        DISP_RC( rc, "VCursorIdRange() failed" );
                        if ( 0 == rc ) {
                            if ( NULL == ctx -> rows ) {
                                /* if the user did not specify a row-range, take all rows */
                                rc = num_gen_make_from_range( &( ctx -> rows ), first, count );
                                DISP_RC( rc, "num_gen_make_from_range() failed" );
                            } else {
                                /* if the user did specify a row-range, check the boundaries */
                                if ( count > 0 ) {
                                    /* trim only if the row-range is not zero, otherwise
                                        we will not get data if the user specified only static columns
                                        because they report a row-range of zero! */
                                    rc = num_gen_trim( ctx -> rows, first, count );
                                    DISP_RC( rc, "num_gen_trim() failed" );
                                }
                            }
                            if ( 0 == rc ) {
                                if ( num_gen_empty( ctx -> rows ) ) {
                                    rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                                } else {
                                    r_ctx . ctx = ctx;
                                    rc = vdm_dump_rows( &r_ctx ); /* <--- */
                                }
                            }
                        }
                    }
                }
            }
            if ( 0 == rc && invalid_columns > 0 ) {
                rc = RC( rcExe, rcDatabase, rcResolving, rcColumn, rcInvalid );
            }
            vdcd_destroy( r_ctx . col_defs );
        }
        rc = vdh_vcursor_release( rc, r_ctx . cursor );
        if ( 0 == rc && 0 != r_ctx . last_rc ) {
            rc = r_ctx . last_rc;
        }
    }
    return rc;
}

/*************************************************************************************
    vdm_dump_opened_view:
    * opens a cursor to read from a view

ctx [IN] ... contains path, tablename, columns, row-range etc.
view [IN] ... open view needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_opened_view( const p_dump_context ctx, const VView *view ) {
    row_context r_ctx;
    rc_t rc = VViewCreateCursor( view, &( r_ctx . cursor ) );
    r_ctx . last_rc = 0;
    DISP_RC( rc, "VViewCreateCursor() failed" );
    if ( 0 == rc ) {
        r_ctx . table = NULL; // TODO: make dependency reporting work again
        r_ctx . view = view;
        if ( !vdcd_init( &( r_ctx . col_defs ), ctx -> max_line_len ) ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }
        if ( 0 == rc ) {
            uint32_t invalid_columns = 0;
            uint32_t n = vdm_extract_or_parse_columns_view( ctx, view, r_ctx . col_defs, &invalid_columns );
            if ( n < 1 ) {
                KOutMsg( "the requested view is empty!\n" );
            } else {
                n = vdcd_add_to_cursor( r_ctx . col_defs, r_ctx . cursor );
                if ( n < 1 ) {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                } else {
                    {
                        /* if this fails, we do not have name-translations for special cell-values
                        but we do not abort because of it ... */
                        const VSchema *schema;
                        rc_t rc2 = VViewOpenSchema( view, &schema );
                        DISP_RC( rc2, "VViewOpenSchema() failed" );
                        if ( 0 == rc2 ) {
                            /* translate in special columns to numeric values to strings */
                            vdcd_ins_trans_fkt( r_ctx . col_defs, schema );
                            vdh_vschema_release( rc, schema );
                        }
                    }
                    rc = VCursorOpen( r_ctx . cursor );
                    DISP_RC( rc, "VCursorOpen() failed" );
                    if ( 0 == rc ) {
                        int64_t  first;
                        uint64_t count;
                        rc = VCursorIdRange( r_ctx . cursor, 0, &first, &count );
                        DISP_RC( rc, "VCursorIdRange() failed" );
                        if ( 0 == rc ) {
                            if ( NULL == ctx -> rows ) {
                                /* if the user did not specify a row-range, take all rows */
                                rc = num_gen_make_from_range( &( ctx -> rows ), first, count );
                                DISP_RC( rc, "num_gen_make_from_range() failed" );
                            } else {
                                /* if the user did specify a row-range, check the boundaries */
                                if ( count > 0 ) {
                                    /* trim only if the row-range is not zero, otherwise
                                        we will not get data if the user specified only static columns
                                        because they report a row-range of zero! */
                                    rc = num_gen_trim( ctx -> rows, first, count );
                                    DISP_RC( rc, "num_gen_trim() failed" );
                                }
                            }
                            if ( 0 == rc ) {
                                if ( num_gen_empty( ctx -> rows ) ) {
                                    rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                                } else {
                                    r_ctx . ctx = ctx;
                                    rc = vdm_dump_rows( &r_ctx ); /* <--- */
                                }
                            }
                        }
                    }
                }
            }
            if ( 0 == rc && invalid_columns > 0 ) {
                rc = RC( rcExe, rcDatabase, rcResolving, rcColumn, rcInvalid );
            }
            vdcd_destroy( r_ctx . col_defs );
        }
        rc = vdh_vcursor_release( rc, r_ctx . cursor );
        if ( 0 == rc && 0 != r_ctx . last_rc ) {
            rc = r_ctx . last_rc;
        }
    }
    return rc;
}

/*************************************************************************************
    dump_db_table:
    * called by "dump_database()" as a fkt-pointer
    * opens a table to read
    * calls "dump_tab_table()" to do the dump
    * releases the table

ctx  [IN] ... contains path, tablename, columns, row-range etc.
db   [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_opened_database( const p_dump_context ctx,
                                      const VDatabase *db ) {
    uint32_t t;
    rc_t rc = VDatabaseMemberType( db, ctx -> table, &t );
    if ( rc == 0 ) {
        switch ( t ) {
        case dbmTable: {
                const VTable *tbl;
                rc_t rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
                if ( 0 == rc ) {
                    rc = vdm_dump_opened_table( ctx, tbl );
                    rc = vdh_vtable_release( rc, tbl );
                }
            }
            break;
        case dbmViewAlias: {
                const VView * view;
                rc = VDatabaseOpenView ( db, & view, ctx -> table );
                if ( 0 == rc ) {
                    rc = vdm_dump_opened_view( ctx, view );
                    rc = vdh_view_release( rc, view );
                }
            }
            break;
        default:
            rc = RC( rcExe, rcDatabase, rcOpening, rcTable, rcInvalid );
            break;
        }
    }
    return rc;
}

/* ********************************************************************** */

static rc_t vdm_show_tab_spread( const p_dump_context ctx,
                                 const VTable *tbl ) {
    const VCursor * cursor;
    rc_t rc = VTableCreateCachedCursorRead( tbl, &cursor, ctx -> cur_cache_size );
    DISP_RC( rc, "VTableCreateCursorRead() failed" );
    if ( 0 == rc ) {
        col_defs * cols;
        if ( !vdcd_init( &cols, ctx -> max_line_len ) ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }
        if ( 0 == rc ) {
            uint32_t invalid_columns = 0;
            uint32_t n = vdm_extract_or_parse_columns( ctx, tbl, cols, &invalid_columns );
            if ( n < 1 ) {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            } else {
                n = vdcd_add_to_cursor( cols, cursor );
                if ( n < 1 ) {
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                } else {
                    rc = VCursorOpen( cursor );
                    DISP_RC( rc, "VCursorOpen() failed" );
                    if ( 0 == rc ) {
                        int64_t  first;
                        uint64_t count;
                        rc = VCursorIdRange( cursor, 0, &first, &count );
                        DISP_RC( rc, "VCursorIdRange( spread ) failed" );
                        if ( 0 == rc ) {
                            if ( NULL == ctx -> rows ) {
                                rc = num_gen_make_from_range( &( ctx -> rows ), first, count );
                                DISP_RC( rc, "num_gen_make_from_range() failed" );
                            } else {
                                if ( count > 0 ) {
                                    rc = num_gen_trim( ctx -> rows, first, count );
                                    DISP_RC( rc, "num_gen_trim() failed" );
                                }
                            }
                            if ( 0 == rc ) {
                                if ( num_gen_empty( ctx -> rows ) ) {
                                    rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                                } else {
                                    rc = vdcd_collect_spread( ctx -> rows, cols, cursor ); /* is in vdb-dump-coldefs.c */
                                }
                            }
                        }
                    }
                }
            }
            vdcd_destroy( cols );
            if ( 0 == rc && invalid_columns > 0 ) {
                rc = RC( rcExe, rcDatabase, rcResolving, rcColumn, rcInvalid );
            }
        }
        rc = vdh_vcursor_release( rc, cursor );
    }
    return rc;
}

static rc_t vdm_show_db_spread( const p_dump_context ctx, const VDatabase *db ) {
    const VTable *tbl;
    rc_t rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
    if ( 0 == rc ) {
        rc = vdm_show_tab_spread( ctx, tbl ); /* above */
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}
/* ********************************************************************** */

/********************************************************************
helper function, needed by "VSchemaDump()"
********************************************************************/
static rc_t CC vdm_schema_dump_flush( void *dst, const void *buffer, size_t bsize ) {
    FILE *f = dst;
    fwrite( buffer, 1, bsize, f );
    return 0;
}

/*************************************************************************************
    dump_the_tab_schema:
    * called by "dump_the_db_schema()" and "dump_table()" as a function pointer
    * opens the schema of a table
    * calls "VSchemaDump()" to dump this schema
    * releases the schema

ctx   [IN] ... contains path, tablename, columns, row-range etc.
tbl   [IN] ... open table needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_tab_schema( const p_dump_context ctx,
                                 const VTable *tbl ) {
    const VSchema * schema;
    rc_t rc = VTableOpenSchema( tbl, &schema );
    DISP_RC( rc, "VTableOpenSchema() failed" );
    if ( 0 == rc ) {
        if ( ctx -> columns == NULL ) {
            /* the user did not ask to inspect a specific object, we look for
               the Typespec of the table... */
            char buffer[ 4096 ];
            rc = VTableTypespec ( tbl, buffer, sizeof buffer );
            DISP_RC( rc, "VTableTypespec() failed" );
            if ( 0 == rc ) {
                rc = VSchemaDump( schema, sdmPrint, buffer,
                                  vdm_schema_dump_flush, stdout );
            }
        } else {
            /* the user did ask to inspect a specific object */
            rc = VSchemaDump( schema, sdmPrint, ctx -> columns,
                              vdm_schema_dump_flush, stdout );
        }
        DISP_RC( rc, "VSchemaDump() failed" );
        rc = vdh_vschema_release( rc, schema );
    }
    return rc;
}

/*************************************************************************************
    dump_the_db_schema:
    * called by "dump_database()" as a function pointer
    * opens a table to read
    * calls "dump_the_tab_schema()" to dump the schema of this table
    * releases the table

ctx   [IN] ... contains path, tablename, columns, row-range etc.
db    [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_db_schema( const p_dump_context ctx, const VDatabase *db ) {
    rc_t rc = 0;
    if ( ctx -> table_defined ) {
        /* the user has given a database as object, but asks to inspect a given table */
        const VTable *tbl;
        rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
        if ( 0 == rc ) {
            rc = vdm_dump_tab_schema( ctx, tbl ); /* above */
            rc = vdh_vtable_release( rc, tbl );
        }
    } else {
        /* the user has given a database as object, but did not ask for a specific table */
        const VSchema * schema;
        rc = VDatabaseOpenSchema( db, &schema );
        DISP_RC( rc, "VDatabaseOpenSchema() failed" );
        if ( 0 == rc ) {
            if ( NULL == ctx -> columns ) {
                /* the used did not ask to inspect a specifiy object, we look for
                   the Typespec of the database... */
                char buffer[ 4096 ];
                rc = VDatabaseTypespec ( db, buffer, sizeof buffer );
                DISP_RC( rc, "VDatabaseTypespec() failed" );
                if ( 0 == rc ) {
                    rc = VSchemaDump( schema, sdmPrint, buffer,
                                      vdm_schema_dump_flush, stdout );
                }
            } else {
                /* the user did ask to inspect a specific object */
                rc = VSchemaDump( schema, sdmPrint, ctx -> columns,
                                  vdm_schema_dump_flush, stdout );
            }
            DISP_RC( rc, "VSchemaDump() failed" );
            rc = vdh_vschema_release( rc, schema );
        }
    }
    return rc;
}

/*************************************************************************************
    enum_tables:
    * called by "dump_database()" as a function pointer
    * calls VDatabaseListTbl() to get a list of Tables
    * loops through this list to print the names

ctx  [IN] ... contains path, tablename, columns, row-range etc.
db   [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_enum_sub_dbs_and_tabs( const VDatabase * db, uint32_t indent );

static rc_t vdm_report_tab_or_db( const VDatabase * db, const KNamelist * list,
                                  const char * prefix, uint32_t indent ) {
    uint32_t n;
    rc_t rc = KNamelistCount( list, &n );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( 0 == rc ) {
        uint32_t i;
        for ( i = 0; i < n && 0 == rc; ++i ) {
            const char * entry;
            rc = KNamelistGet( list, i, &entry );
            DISP_RC( rc, "KNamelistGet() failed" );
            if ( 0 == rc ) {
                rc = KOutMsg( "%*s : %s\n", indent + 3, prefix, entry );
                if ( 0 == rc && NULL != db ) {
                    const VDatabase * sub_db;
                    rc = VDatabaseOpenDBRead ( db, &sub_db, entry );
                    DISP_RC( rc, "VDatabaseOpenDBRead() failed" );
                    if ( 0 == rc ) {
                        rc = vdm_enum_sub_dbs_and_tabs( sub_db, indent + 3 ); /* recursion here... */
                        rc = vdh_vdatabase_release( rc, sub_db );
                    }
                }
            }
        }
    }
    return rc;
}

static rc_t vdm_enum_tabs_of_db( const VDatabase * db, uint32_t indent ) {
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( db, &tbl_names );
    if ( 0 == rc ) {
        rc = vdm_report_tab_or_db( NULL, tbl_names, "tbl", indent );
        rc = vdh_knamelist_release( rc, tbl_names );
    } else {
        rc = 0;
    }
    return rc;
}

static rc_t vdm_enum_sub_dbs_of_db( const VDatabase * db, uint32_t indent ) {
    KNamelist *db_names;
    rc_t rc = VDatabaseListDB( db, &db_names );
    if ( 0 == rc ) {
        rc = vdm_report_tab_or_db( db, db_names, "db ", indent );
        rc = vdh_knamelist_release( rc, db_names );
    } else {
        rc = 0;
    }
    return rc;
}

static rc_t vdm_enum_sub_dbs_and_tabs( const VDatabase * db, uint32_t indent ) {
    rc_t rc = vdm_enum_sub_dbs_of_db( db, indent );
    if ( 0 == rc ) {
        rc = vdm_enum_tabs_of_db( db, indent );
    }
    return rc;
}

static rc_t vdm_enum_tables( const p_dump_context ctx, const VDatabase * db ) {
    rc_t rc = KOutMsg( "enumerating the tables of database '%s'\n", ctx -> path );
    if ( 0 == rc ) {
        rc = vdm_enum_sub_dbs_and_tabs( db, 0 );
    }
    return rc;
}

typedef struct col_info_context
{
    p_dump_context ctx;
    const VSchema *schema;
    const VTable *tbl;
    const KTable *ktbl;
} col_info_context;
typedef struct col_info_context* p_col_info_context;

static rc_t vdm_print_column_datatypes( const p_col_def col_def,
                                        const p_col_info_context ci ) {
    KNamelist *names;
    uint32_t dflt_idx;
    rc_t rc = VTableColumnDatatypes( ci -> tbl, col_def -> name, &dflt_idx, &names );
    DISP_RC( rc, "VTableColumnDatatypes() failed" );
    if ( 0 == rc ) {
        uint32_t n;
        rc = KNamelistCount( names, &n );
        DISP_RC( rc, "KNamelistCount() failed" );
        if ( 0  == rc ) {
            uint32_t i;
            for ( i = 0; i < n && 0 == rc; ++i ) {
                const char *type_name;
                rc = KNamelistGet( names, i, &type_name );
                DISP_RC( rc, "KNamelistGet() failed" );
                if ( 0 == rc ) {
                    if ( dflt_idx == i ) {
                        rc = KOutMsg( "%20s.type[%d] = %s (dflt)\n", col_def -> name, i, type_name );
                    } else {
                        rc = KOutMsg( "%20s.type[%d] = %s\n", col_def -> name, i, type_name );
                    }
                }
            }
        }
        rc = vdh_knamelist_release( rc, names );
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "\n" );
    }
    return rc;
}

static rc_t vdm_show_kdb_blobs( const p_col_def col_def,
                                const p_col_info_context ci ) {
    const KColumn * kcol;
    rc_t rc = KTableOpenColumnRead( ci -> ktbl, &kcol, "%s", col_def -> name );
    if ( 0 != rc ) {
        return KOutMsg( "'%s' is not a physical column (%R)\n", col_def -> name, rc );
    } else {
        rc = KOutMsg( "\nCOLUMN '%s':\n", col_def -> name );
    }
    if ( 0 == rc ) {
        int64_t first;
        uint64_t count;
        rc = KColumnIdRange( kcol, &first, &count );
        DISP_RC( rc, "KColumnIdRange() failed" );
        if ( 0 == rc ) {
            int64_t last = first + count - 1;
            rc = KOutMsg( "range: %,ld ... %,ld\n", first, last );
            if ( 0 == rc ) {
                int64_t id = first;
                uint64_t sum = 0;
                while ( id < last && 0 == rc ) {
                    const KColumnBlob *blob;
                    rc = KColumnOpenBlobRead( kcol, &blob, id );
                    DISP_RC( rc, "KColumnOpenBlobRead() failed" );
                    if ( 0 == rc ) {
                        int64_t first_id_in_blob;
                        uint32_t ids_in_blob;
                        rc = KColumnBlobIdRange( blob, &first_id_in_blob, &ids_in_blob );
                        DISP_RC( rc, "KColumnBlobIdRange() failed" );
                        if ( 0 == rc ) {
                            int64_t last_id_in_blob = first_id_in_blob + ids_in_blob - 1;
                            char buffer[ 8 ];
                            size_t num_read, remaining;
                            rc = KColumnBlobRead ( blob, 0, &buffer, 0, &num_read, &remaining );
                            DISP_RC( rc, "KColumnBlobRead() failed" );
                            if ( 0 == rc ) {
                                size_t blob_size = remaining + num_read;
                                rc = KOutMsg( "blob[ %,ld ... %,ld] size = %,zu\n",
                                    first_id_in_blob, last_id_in_blob, blob_size );
                                sum += blob_size;
                            }
                            id = last_id_in_blob + 1;
                        }
                        rc = vdh_kcolumnblob_release( rc, blob );
                    }
                }
                if ( 0 == rc ) {
                    rc = KOutMsg( "total bytes = %,zu\n", sum );
                }
            }
        }
        rc = vdh_kcolumn_release( rc, kcol );
    }
    return rc;
}

static rc_t vdm_show_vdb_blobs( const p_col_def col_def,
                                const p_col_info_context ci ) {
    const VCursor * curs;
    rc_t rc = VTableCreateCachedCursorRead( ci -> tbl, &curs, ci -> ctx -> cur_cache_size );
    DISP_RC( rc, "VTableCreateCachedCursorRead() failed" );
    if ( 0 == rc ) {
        uint32_t idx;
        rc = VCursorAddColumn( curs, &idx, "%s", col_def -> name );
        DISP_RC( rc, "VCursorAddColumn() failed in vdb_show_blobs()" );
        if ( 0 == rc ) {
            rc = VCursorOpen( curs );
            DISP_RC( rc, "VCursorOpen() failed" );
            if ( 0 == rc ) {
                int64_t column_first;
                uint64_t column_count;
                rc = VCursorIdRange( curs, idx, &column_first, &column_count );
                DISP_RC( rc, "VCursorIdRange() failed" );
                if ( 0 == rc && column_count > 0 ) {
                    int64_t row_id = column_first;
                    int64_t blob_nr = 0;
                    bool done = false;
                    while ( 0 == rc && !done ) {
                        rc = VCursorSetRowId ( curs, row_id );
                        DISP_RC( rc, "VCursorSetRowId() failed" );
                        if ( 0 == rc ) {
                            rc = VCursorOpenRow( curs );
                            DISP_RC( rc, "VCursorOpenRow() failed" );
                            if ( 0 == rc ) {
                                const VBlob * blob;
                                rc = VCursorGetBlob ( curs, &blob, idx );
                                DISP_RC( rc, "VCursorGetBlob() failed" );
                                if ( 0 == rc ) {
                                    int64_t blob_first;
                                    uint64_t blob_count;
                                    rc = VBlobIdRange ( blob, &blob_first, &blob_count );
                                    DISP_RC( rc, "VBlobIdRange() failed" );
                                    if ( 0 == rc ) {
                                        size_t blob_bytes;
                                        rc = VBlobSize ( blob, &blob_bytes );
                                        DISP_RC( rc, "VBlobSize() failed" );
                                        if ( 0 == rc ) {
                                            rc = KOutMsg( "%s.%d\t%d\t%u\t%u\n",
                                                col_def -> name, blob_nr++,
                                                blob_first, blob_count, blob_bytes );
                                            row_id += blob_count;
                                            done = ( 0 == blob_count ||
                                                     row_id >= ( column_first + (int64_t)column_count ) );
                                        }
                                    }
                                }
                                {
                                    rc_t rc2 = VCursorCloseRow ( curs );
                                    DISP_RC( rc2, "VCursorCloseRow() failed" );
                                    rc = ( 0 == rc ) ? rc2 : rc;
                                }
                            }
                        }
                    }
                }
            }
        }
        rc = vdh_vcursor_release( rc, curs );
    }
    return rc;
}

/*************************************************************************************
    print_column_info:
    * get's called from "enum_tab_columns()" via VectorForEach
    * prints: table-name, col-idx, bits, dimension, domain and name
      for every column

item  [IN] ... pointer to a column-definition-struct ( from "vdb-dump-coldefs.h" )
data  [IN] ... pointer to dump-context ( in this case to have a line-idx )
*************************************************************************************/
static rc_t vdm_print_column_info( const p_col_def col_def, p_col_info_context ci_ctx ) {
    rc_t rc = 0;
    if ( ci_ctx -> ctx -> show_kdb_blobs ) {
        rc = vdm_show_kdb_blobs( col_def, ci_ctx );
    } else if ( ci_ctx -> ctx -> show_vdb_blobs ) {
        rc = vdm_show_vdb_blobs( col_def, ci_ctx );
    } else {
        /* print_col_info is in vdb-dump-helper.c */
        rc = vdh_print_col_info( ci_ctx -> ctx , col_def, ci_ctx -> schema );
        /* to test VTableColumnDatatypes() */
        if ( 0 == rc  && ci_ctx -> ctx -> column_enum_requested ) {
            rc = vdm_print_column_datatypes( col_def, ci_ctx );
        }
    }
    return rc;
}

static rc_t vdm_enum_phys_columns( const VTable *tbl ) {
    rc_t rc = KOutMsg( "physical columns:\n" );
    if ( 0 == rc ) {
        KNamelist *phys;
        rc = VTableListPhysColumns( tbl, &phys );
        DISP_RC( rc, "VTableListPhysColumns() failed" );
        if ( 0 == rc ) {
            uint32_t count;
            rc = KNamelistCount( phys, &count );
            DISP_RC( rc, "KNamelistCount( physical columns ) failed" );
            if ( 0 == rc ) {
                if ( count > 0 ) {
                    uint32_t idx;
                    for ( idx = 0; idx < count && 0 == rc; ++idx ) {
                        const char * name;
                        rc = KNamelistGet( phys, idx, &name );
                        DISP_RC( rc, "KNamelistGet( physical columns ) failed" );
                        if ( 0 == rc ) {
                            rc = KOutMsg( "[%.02d] = %s\n", idx, name );
                        }
                    }
                } else {
                    rc = KOutMsg( "... list is empty!\n" );
                }
            }
            rc = vdh_knamelist_release( rc, phys );
        }
    }
    return rc;
}

static rc_t vdm_enum_readable_columns( const VTable *tbl ) {
    rc_t rc = KOutMsg( "readable columns:\n" );
    if ( 0 == rc ) {
        KNamelist *readable;
        rc = VTableListReadableColumns( tbl, &readable );
        DISP_RC( rc, "VTableListReadableColumns() failed" );
        if ( 0 == rc ) {
            uint32_t count;
            rc = KNamelistCount( readable, &count );
            DISP_RC( rc, "KNamelistCount( readable columns ) failed" );
            if ( 0 == rc ) {
                if ( count > 0 ) {
                    uint32_t idx;
                    for ( idx = 0; idx < count && 0 == rc; ++idx ) {
                        const char * name;
                        rc = KNamelistGet( readable, idx, &name );
                        DISP_RC( rc, "KNamelistGet( readable columns ) failed" );
                        if ( 0 == rc ) {
                            rc = KOutMsg( "[%.02d] = %s\n", idx, name );
                        }
                    }
                } else {
                    rc = KOutMsg( "... list is empty!\n" );
                }
            }
            rc = vdh_knamelist_release( rc, readable );
        }
    }
    return rc;
}

/*************************************************************************************
    enum_tab_columns:
    * called by "enum_db_columns()" and "dump_table()" as fkt-pointer
    * initializes a column-definitions-structure
    * calls col_defs_extract_from_table() to enumerate all columns in the table
    ( both functions are in "vdb-dump-coldefs.h" )
    * loops through the columns and prints it's information
    * releases the column-definitions-structure

ctx   [IN] ... contains path, tablename, columns, row-range etc.
tbl   [IN] ... open table needed for vdb-calls
*************************************************************************************/
static rc_t vdm_enum_tab_columns( const p_dump_context ctx, const VTable *tbl ) {
    rc_t rc = 0;
    if ( ctx -> enum_phys ) {
        rc = vdm_enum_phys_columns( tbl );
    } else if ( ctx -> enum_readable ) {
        rc = vdm_enum_readable_columns( tbl );
    } else {
        p_col_defs col_defs;
        if ( !vdcd_init( &col_defs, ctx -> max_line_len ) ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }
        if ( 0 == rc ) {
            col_info_context ci_ctx;
            bool extracted;
            uint32_t invalid_columns = 0;
            ci_ctx . ctx = ctx;
            ci_ctx . tbl = tbl;
            if ( ctx -> show_kdb_blobs ) {
                extracted = vdm_extract_or_parse_phys_columns( ctx, tbl, col_defs, &invalid_columns );
                rc = VTableOpenKTableRead( tbl, &( ci_ctx . ktbl ) );
                DISP_RC( rc, "VTableOpenKTableRead() failed" );
            } else if ( ctx -> enum_static ) {
                extracted = vdm_extract_or_parse_static_columns( ctx, tbl, col_defs, &invalid_columns );
                rc = VTableOpenKTableRead( tbl, &( ci_ctx . ktbl ) );
                DISP_RC( rc, "VTableOpenKTableRead() failed" );
            } else {
                extracted = vdm_extract_or_parse_columns( ctx, tbl, col_defs, &invalid_columns );
                ci_ctx . ktbl = NULL;
            }
            if ( extracted && 0 == rc ) {
                rc = VTableOpenSchema( tbl, &( ci_ctx . schema ) );
                DISP_RC( rc, "VTableOpenSchema() failed" );
                if ( 0 == rc ) {
                    uint32_t idx, count;
                    ctx -> generic_idx = 1;
                    count = VectorLength( &( col_defs -> cols ) );
                    for ( idx = 0; idx < count && 0 == rc; ++idx ) {
                        col_def *col = ( col_def * )VectorGet( &( col_defs -> cols ), idx );
                        if ( col != 0 ) {
                            rc = vdm_print_column_info( col, &ci_ctx );
                        }
                    }
                    rc = vdh_vschema_release( rc, ci_ctx . schema );
                }
                rc = vdh_ktable_release( rc, ci_ctx . ktbl );
            } else {
                rc = KOutMsg( "error in col_defs_extract_from_table\n" );
            }
            vdcd_destroy( col_defs );
            if ( 0 == rc && invalid_columns > 0 ) {
                rc = RC( rcExe, rcDatabase, rcResolving, rcColumn, rcInvalid );
            }
        }
    }
    return rc;
}

/*************************************************************************************
    enum_db_columns:
    * called by "dump_database()" as fkt-pointer
    * opens the table
    * calls enum_tab_columns()
    * releases table

ctx   [IN] ... contains path, tablename, columns, row-range etc.
db    [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_enum_db_columns( const p_dump_context ctx, const VDatabase *db ) {
    const VTable *tbl;
    rc_t rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
    if ( 0 == rc ) {
        rc = vdm_enum_tab_columns( ctx, tbl );
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}

static rc_t vdm_print_tab_id_range( const p_dump_context ctx, const VTable *tbl ) {
    const VCursor *curs;
    rc_t rc = VTableCreateCursorRead( tbl, &curs );
    DISP_RC( rc, "VTableCreateCursorRead() failed" );
    if ( 0 == rc ) {
        col_defs *col_defs;
        if ( !vdcd_init( &col_defs, ctx -> max_line_len ) ) {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }
        if ( 0 == rc ) {
            uint32_t invalid_columns = 0;
            if ( vdm_extract_or_parse_columns( ctx, tbl, col_defs, &invalid_columns ) ) {
                if ( vdcd_add_to_cursor( col_defs, curs ) ) {
                    rc = VCursorOpen( curs );
                    DISP_RC( rc, "VCursorOpen() failed" );
                    if ( 0 == rc ) {
                        int64_t  first;
                        uint64_t count;
                        uint32_t idx = 0;

                        /* calling with idx = 0 means: let the cursor find out the min/max values of
                           all open columns...
                           vdcd_get_first_none_static_column_idx( col_defs, curs, &idx );
                        */
                        rc = VCursorIdRange( curs, idx, &first, &count );
                        DISP_RC( rc, "VCursorIdRange() failed" );
                        if ( 0 == rc ) {
                            rc = KOutMsg( "id-range: first-row = %,ld, row-count = %,ld\n", first, count );
                        }
                    }
                }
            }
            vdcd_destroy( col_defs );
            if ( 0 == rc && invalid_columns > 0 ) {
                rc = RC( rcExe, rcDatabase, rcResolving, rcColumn, rcInvalid );
            }
        }
        rc = vdh_vcursor_release( rc, curs );
    }
    return rc;
}

/*************************************************************************************
    print_db_id_range:
    * called by "dump_database()" as fkt-pointer
    * opens the table
    * calls print_tab_id_range()
    * releases table

ctx   [IN] ... contains path, tablename, columns, row-range etc.
db    [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_print_db_id_range( const p_dump_context ctx, const VDatabase *db ) {
    const VTable *tbl;
    rc_t rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
    if ( 0 == rc ) {
        rc = vdm_print_tab_id_range( ctx, tbl ); /* above */
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}

/* ************************************************************************************ */

static rc_t vdm_enum_index( const KTable * ktbl, uint32_t idx_nr, const char * idx_name ) {
    rc_t rc = KOutMsg( "idx #%u: %s", idx_nr + 1, idx_name );
    if ( 0 == rc ) {
        const KIndex * kidx;
        rc = KTableOpenIndexRead ( ktbl, &kidx, "%s", idx_name );
        if ( 0 != rc ) {
            rc = KOutMsg( " (cannot open)" );
        } else {
            uint32_t idx_version;
            rc = KIndexVersion ( kidx, &idx_version );
            if ( rc != 0 ) {
                rc = KOutMsg( " V?.?.?" );
            } else {
                rc = KOutMsg( " V%V", idx_version );
            }
            if ( 0 == rc ) {
                KIdxType idx_type;
                rc = KIndexType ( kidx, &idx_type );
                if ( 0 != rc ) {
                    rc = KOutMsg( " type = ?" );
                } else {
                    switch ( idx_type &~ kitProj ) {
                        case kitText : rc = KOutMsg( " type = Text" ); break;
                        case kitU64  : rc = KOutMsg( " type = U64" ); break;
                        default      : rc = KOutMsg( " type = unknown" ); break;
                    }
                    if ( 0 == rc && ( ( idx_type & kitProj ) == kitProj ) ) {
                        rc = KOutMsg( " reverse" );
                    }
                }
            }
            if ( 0 == rc ) {
                bool locked = KIndexLocked ( kidx );
                if ( locked ) {
                    rc = KOutMsg( " locked" );
                }
            }
            rc = vdh_kindex_release( rc, kidx );
        }
    }
    if ( 0 == rc ) {
        rc = KOutMsg( "\n" );
    }
    return rc;
}

static rc_t vdm_enum_tab_index( const p_dump_context ctx, const VTable *tbl ) {
    const KTable * ktbl;
    rc_t rc = VTableOpenKTableRead( tbl, &ktbl );
    DISP_RC( rc, "VTableOpenKTableRead() failed" );
    if ( 0 == rc ) {
        KNamelist *idx_names;
        rc = KTableListIdx ( ktbl, &idx_names );
        if ( 0 == rc ) {
            uint32_t count;
            rc = KNamelistCount( idx_names, &count );
            if ( 0 == rc ) {
                uint32_t i;
                for ( i = 0; i < count && 0 == rc; ++i ) {
                    const char * idx_name = NULL;
                    rc = KNamelistGet( idx_names, i, &idx_name );
                    if ( 0 == rc && idx_name != NULL ) {
                        rc = vdm_enum_index( ktbl, i, idx_name );
                    }
                }
            }
            rc = vdh_knamelist_release( rc, idx_names );
        } else {
            rc = KOutMsg( "no index available\n" );
        }
        rc = vdh_ktable_release( rc, ktbl );
    }
    return rc;
}

static rc_t vdm_enum_db_index( const p_dump_context ctx, const VDatabase *db ) {
    const VTable *tbl;
    rc_t rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
    if ( 0 == rc ) {
        rc = vdm_enum_tab_index( ctx, tbl ); /* above */
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}

/* ************************************************************************************ */

static rc_t vdm_range_tab_index( const p_dump_context ctx, const VTable *tbl ) {
    const KTable * ktbl;
    rc_t rc = VTableOpenKTableRead( tbl, &ktbl );
    if ( 0 != rc ) {
        ErrMsg( "VTableOpenKTableRead() -> %R", rc );
    } else {
        const KIndex * kindex;
        rc = KTableOpenIndexRead ( ktbl, &kindex, "%s", ctx->idx_range );
        if ( 0 != rc ) {
            ErrMsg( "KTableOpenIndexRead() -> %R", rc );
        } else {
            int64_t start;
            uint64_t count;
            rc_t rc2 = 0;
            for ( start = 1; 0 == rc2 && 0 == rc; start += count ) {
                size_t key_size;
                char key [ 4096 ];
                rc2 = KIndexProjectText ( kindex, start, &start, &count, key, sizeof key, &key_size );
                if ( 0 == rc2 ) {
                    rc = KOutMsg( "%.*s : %lu ... %lu\n", ( int )key_size, key, start, start + count - 1 );
                }
            }
            rc = vdh_kindex_release( rc, kindex );
        }
        rc = vdh_ktable_release( rc, ktbl );
    }
    return rc;
}

static rc_t vdm_range_db_index( const p_dump_context ctx, const VDatabase *db ) {
    const VTable *tbl;
    rc_t rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
    if ( 0 == rc ) {
        rc = vdm_range_tab_index( ctx, tbl ); /* above */
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}

/* ************************************************************************************ */

static rc_t vdm_show_tab_spotgroups( const p_dump_context ctx, const VTable *tbl ) {
    const KMetadata * meta = NULL;
    rc_t rc = VTableOpenMetadataRead( tbl, &meta );
    DISP_RC( rc, "VTableOpenMetadataRead()" );
    if ( 0 == rc ) {
        const KMDataNode * spot_groups_node;
        rc = KMetadataOpenNodeRead( meta, &spot_groups_node, "STATS/SPOT_GROUP" );
        DISP_RC( rc, "KMetadataOpenNodeRead( STATS/SPOT_GROUP ) failed");
        if ( 0 == rc ) {
            KNamelist * spot_groups;
            rc = KMDataNodeListChildren( spot_groups_node, &spot_groups );
            DISP_RC( rc, "KMDataNodeListChildren() failed" );
            if ( 0 == rc ) {
                uint32_t count;
                rc = KNamelistCount( spot_groups, &count );
                DISP_RC( rc, "KNamelistCount() failed" );
                if ( 0 == rc && count > 0 ) {
                    uint32_t i;
                    for ( i = 0; i < count && 0 == rc; ++i ) {
                        const char * name = NULL;
                        rc = KNamelistGet( spot_groups, i, &name );
                        DISP_RC( rc, "KNamelistGet() failed" );
                        if ( 0 == rc && NULL != name ) {
                            const KMDataNode * spot_count_node;
                            rc = KMDataNodeOpenNodeRead( spot_groups_node, &spot_count_node, "%s/SPOT_COUNT", name );
                            DISP_RC( rc, "KMDataNodeOpenNodeRead() failed" );
                            if ( 0 == rc ) {
                                uint64_t spot_count = 0;
                                rc = KMDataNodeReadAsU64( spot_count_node, &spot_count );
                                if ( 0 != rc ) {
                                    ErrMsg( "KMDataNodeReadAsU64() -> %R", rc );
                                    vdh_clear_recorded_errors();
                                } else {
                                    if ( spot_count > 0 ) {
                                        const KMDataNode * spot_group_node;
                                        rc = KMDataNodeOpenNodeRead( spot_groups_node, &spot_group_node, name );
                                        DISP_RC( rc, "KMDataNodeOpenNodeRead() failed" );
                                        if ( 0 == rc ) {
                                            char name_attr[ 2048 ];
                                            size_t num_writ;
                                            rc = KMDataNodeReadAttr( spot_group_node, "name", name_attr, sizeof name_attr, &num_writ );
                                            DISP_RC( rc, "KMDataNodeReadAttr() failed" );
                                            if ( 0 == rc ) {
                                                rc = KOutMsg( "%s\t%,lu\n", rc == 0 ? name_attr : name, spot_count );
                                                rc = vdh_datanode_release( rc, spot_group_node );
                                            }
                                        }

                                    }
                                }
                                rc = vdh_datanode_release( rc, spot_count_node );
                            }
                        }
                    }
                }
                rc = vdh_knamelist_release( rc, spot_groups );
            }
            rc = vdh_datanode_release( rc, spot_groups_node );
        }
        rc = vdh_kmeta_release( rc, meta );
    }
    return rc;
}

static rc_t vdm_show_db_spotgroups( const p_dump_context ctx, const VDatabase *db ) {
    const VTable *tbl;
    rc_t rc = vdh_open_table_by_path( db, ctx -> table, &tbl ); /* in vdb-dump-helper.c */
    if ( 0 == rc ) {
        rc = vdm_show_tab_spotgroups( ctx, tbl );
        rc = vdh_vtable_release( rc, tbl );
    }
    return rc;
}

typedef rc_t (*db_tab_t)( const p_dump_context ctx, const VTable *a_tab );

/*************************************************************************************
    dump_tab_fkt:
    * called by "dump_table()"
    * if the user has provided the name of a schema-file:
        - make a schema from manager
        - parse the schema-file
        - use this schema to open the database
    * if we do not have a schema-file, use NULL ( that means the internal tab-schema )
    * open the table for read
    * call the function-pointer, with context and table
    * release table and schema

ctx    [IN] ... contains source-path, tablename, columns and row-range as ascii-text
db_fkt [IN] ... function to be called if directory, manager and database are open
*************************************************************************************/
static rc_t vdm_dump_tab_fkt( const p_dump_context ctx,
                              const VDBManager *mgr,
                              const db_tab_t tab_fkt ) {
    VSchema *schema = NULL;
    rc_t rc = vdh_parse_schema( mgr, &schema, &( ctx -> schema_list ) );
    if ( rc == 0 ) {
        VPath * path = NULL;
        rc = vdh_path_to_vpath( ctx -> path, &path );
        if ( 0 == rc ) {
            const VTable *tbl;
            rc = VDBManagerOpenTableReadVPath( mgr, &tbl, schema, path );
            if ( 0 != rc ) {
                ErrMsg( "VDBManagerOpenTableReadVPath( '%R' ) -> %R", ctx->path, rc );
            } else {
                rc = vdh_check_table_empty( tbl );
                if ( 0 == rc ) {
                    rc = tab_fkt( ctx, tbl ); /* fkt-pointer is called */
                }
                rc = vdh_vtable_release( rc, tbl );
            }
            rc = vdh_vschema_release( rc, schema );
            rc = vdh_vpath_release( rc, path );
        }
    }
    return rc;
}

static bool enum_col_request( const p_dump_context ctx ) {
    return ( ctx -> column_enum_requested || ctx -> column_enum_short ||
             ctx -> show_kdb_blobs || ctx -> show_vdb_blobs ||
             ctx -> enum_phys || ctx -> enum_readable );
}

static const char * SEQUENCE_TAB = "SEQUENCE";
static bool is_sequence( const char * tbl ) {
    return ( 0 == string_cmp ( tbl, string_size( tbl ),
            SEQUENCE_TAB, string_size( SEQUENCE_TAB ), 0xFFFF ) );
}

/***************************************************************************
    dump_table:
    * called by "dump_main()" to handle a table
    * calls the dump_tab_fkt with 3 different fkt-pointers as argument
      depending on what was selected at the commandline

ctx  [IN] ... contains path, tablename, columns, row-range etc.
mgr  [IN] ... open manager needed for vdb-calls
***************************************************************************/
static rc_t vdm_dump_table( const p_dump_context ctx, const VDBManager *mgr ) {
    bool table_valid = ( NULL == ctx -> table ) || ( is_sequence( ctx -> table ) );
    if ( !table_valid ) {
        rc_t rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        ErrMsg( "Table '%s' not found-> %R", ctx -> table, rc );
        return rc;
    } else {
        if ( ctx -> schema_dump_requested ) {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_dump_tab_schema );
        } else if ( ctx -> table_enum_requested ) {
            KOutMsg( "cannot enum tables of a table-object\n" );
            vdh_clear_recorded_errors();
            return 0;
        } else if ( enum_col_request( ctx ) ) {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_enum_tab_columns );
        } else if ( ctx -> id_range_requested ) {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_print_tab_id_range );
        } else if ( ctx -> idx_enum_requested ) {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_enum_tab_index );
        } else if ( ctx -> idx_range_requested ) {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_range_tab_index );
        } else if ( ctx -> show_spotgroups ) {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_show_tab_spotgroups );
        } else if ( ctx -> show_spread ) {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_show_tab_spread );
        } else {
            return vdm_dump_tab_fkt( ctx, mgr, vdm_dump_opened_table );
        }
    }
    return 0;
}

typedef rc_t (*db_fkt_t)( const p_dump_context ctx, const VDatabase *a_db );

/*************************************************************************************
    dump_db_fkt:
    * called by "dump_database()" to handle a database
    * if the user has provided the name of a schema-file:
        - make a schema from manager
        - parse the schema-file
        - use this schema to open the database
    * if we do not have a schema-file, use NULL ( that means the internal db-schema )
    * open the database for read
    * if the user has not given a table-name to dump:
        - take the first table found in the database
    * if we now have a table-name
        - call the function-pointer, with context and database
    * if we do not have a table-name
        - request display of the usage-message
    * release database and schema

ctx    [IN] ... contains source-path, tablename, columns and row-range as ascii-text
db_fkt [IN] ... function to be called if directory, manager and database are open
*************************************************************************************/
static rc_t vdm_dump_db_fkt( const p_dump_context ctx, const VDBManager * mgr,
                             const db_fkt_t db_fkt ) {
    VSchema *schema = NULL;
    rc_t rc = vdh_parse_schema( mgr, &schema, &( ctx -> schema_list ) );
    if ( 0 == rc ) {
        VPath * path = NULL;
        rc = vdh_path_to_vpath( ctx -> path, &path );
        if ( 0 == rc ) {
            const VDatabase *db;
            rc = VDBManagerOpenDBReadVPath( mgr, &db, schema, path );
            if ( 0 != rc ) {
                ErrMsg( "VDBManagerOpenDBReadVPath( '%s' ) -> %R", ctx -> path, rc );
            } else {
                KNamelist *tbl_names;
                rc = VDatabaseListTbl( db, &tbl_names );
                if ( 0 != rc ) {
                    ErrMsg( "VDatabaseListTbl( '%s' ) -> %R", ctx -> path, rc );
                } else {
                    if ( NULL == ctx -> table )
                    {
                        /* the user DID NOT not specify a table: by default assume the SEQUENCE-table */
                        bool table_found = vdh_take_this_table_from_list( ctx, tbl_names, "SEQUENCE" );
                        /* if there is no SEQUENCE-table, just pick the first table available... */
                        if ( !table_found ) {
                            vdh_take_1st_table_from_db( ctx, tbl_names );
                        }
                    } else {
                        /* the user DID specify a table: check if the database has a table with this name,
                        if not try with a sub-string */
                        String value;
                        StringInitCString( &value, ctx -> table );
                        if ( !vdh_list_contains_value( tbl_names, &value ) ) { /* vdb-dump-helper.c */
                            vdh_take_this_table_from_list( ctx, tbl_names, ctx -> table );
                        }
                    }
                    if ( NULL != ctx -> table || ctx -> table_enum_requested ) {
                        rc = db_fkt( ctx, db ); /* fkt-pointer is called */
                    } else {
                        LOGMSG( klogInfo, "opened as vdb-database, but no table found" );
                        ctx -> usage_requested = true;
                    }
                    rc = vdh_knamelist_release( rc, tbl_names );
                }
                rc = vdh_vdatabase_release( rc, db );
            }
            rc = vdh_vpath_release( rc, path );
        }
        rc = vdh_vschema_release( rc, schema );
    }
    return rc;
}

/***************************************************************************
    dump_database:
    * called by "dump_main()"
    * calls the dump_db_fkt with 4 different fkt-pointers as argument
      depending on what was selected at the commandline

ctx   [IN] ... contains path, tablename, columns, row-range etc.
mgr   [IN] ... open manager needed for vdb-calls
***************************************************************************/
static rc_t vdm_dump_database( const p_dump_context ctx, const VDBManager *mgr ) {
    if ( ctx -> schema_dump_requested ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_dump_db_schema );
    } else if ( ctx -> table_enum_requested ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_enum_tables );
    } else if ( enum_col_request( ctx ) ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_enum_db_columns );
    } else if ( ctx -> id_range_requested ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_print_db_id_range );
    } else if ( ctx -> idx_enum_requested ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_enum_db_index );
    } else if ( ctx -> idx_range_requested ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_range_db_index );
    } else if ( ctx -> show_spotgroups ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_show_db_spotgroups );
    } else if ( ctx -> show_spread ) {
        return vdm_dump_db_fkt( ctx, mgr, vdm_show_db_spread );
    } else {
        return vdm_dump_db_fkt( ctx, mgr, vdm_dump_opened_database );
    }
    return 0;
}

// returns a VCursor* in ctx->cursor; may be NULL if the view has no data
static rc_t vdb_dump_view_make_cursor( const p_dump_context ctx, const VDBManager *mgr,
                                       row_context * r_ctx, uint32_t * invalid_columns ) {
    view_spec * spec;
    char error [1024];
    rc_t rc = view_spec_parse ( ctx -> view, & spec, error, sizeof ( error ) );
    if ( 0 != rc ) {
        ErrMsg( "view_spec_parse( '%s' ) -> %R (%s)", ctx -> path, rc, error );
    } else {
        const VDatabase * db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", ctx -> path );
        if ( 0 != rc ) {
            ErrMsg( "VDBManagerOpenDBRead( '%s' ) -> %R", ctx -> path, rc );
        } else {
            VSchema * schema = NULL;
            rc = VDatabaseOpenSchema ( db, ( const VSchema ** ) & schema );
            if ( 0 != rc ) {
                ErrMsg( "VDatabaseOpenSchema( '%s' ) -> %R", ctx->path, rc );
            } else {
                rc = vdh_parse_schema_add_on ( mgr, schema, &( ctx -> schema_list ) );
                if ( 0 != rc ) {
                    ErrMsg( "vdh_parse_schema_add_on( '%s' ) -> %R", ctx->path, rc );
                } else {
                    rc = view_spec_open ( spec, db, schema, & r_ctx -> view );
                    if ( 0 != rc ) {
                        ErrMsg( "view_spec_make_cursor( '%s' ) -> %R", ctx->path, rc );
                    } else if ( ! vdcd_init( &( r_ctx -> col_defs ), ctx -> max_line_len ) ) {
                        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                        DISP_RC( rc, "col_defs_init() failed" );
                    } else {
                        uint32_t n = vdm_extract_or_parse_columns_view( ctx, r_ctx -> view,
                                                                        r_ctx -> col_defs, invalid_columns );
                        if ( n < 1 ) {
                            KOutMsg( "the requested view is empty!\n" );
                            r_ctx -> cursor = NULL;
                        }
                        else
                        if ( 0 == rc ) {
                            rc = VViewCreateCursor ( r_ctx -> view, & r_ctx -> cursor );
                            if ( 0 == rc ) {
                                n = vdcd_add_to_cursor( r_ctx -> col_defs, r_ctx -> cursor );
                                if ( n < 1 ) {
                                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                                    DISP_RC( rc, "vdcd_add_to_cursor() failed" );
                                }
                            }
                        }
                    }
                }
                rc = vdh_vschema_release( rc, schema );
            }
            rc = vdh_vdatabase_release( rc, db );
        }
        view_spec_free ( spec );
    }
    return rc;
}

/***************************************************************************
    dump_unbound_view:
    * called by "dump_main()" to handle a view
    * similar to dump_table but dumps a view
    * before dumping, binds the view's arguments to existing tables according to the view-spec string given in ctx.view

ctx        [IN] ... contains path, view instantiation string, columns, row-range etc.
my_manager [IN] ... open manager needed for vdb-calls
***************************************************************************/
static rc_t vdm_dump_unbound_view( const p_dump_context ctx, const VDBManager *mgr ) {
    rc_t rc;
    if ( ! vdh_is_path_database( mgr, ctx -> path, NULL ) ) {
        ErrMsg( "Option --view can only be used with a database object" );
        rc = RC( rcVDB, rcTable, rcConstructing, rcFormat, rcIncorrect );
    } else {
        row_context r_ctx;
        uint32_t invalid_columns = 0;
        rc = vdb_dump_view_make_cursor ( ctx, mgr, & r_ctx, & invalid_columns );
        if ( rc == 0 && r_ctx.cursor != NULL ) {
            const VSchema * schema;
            rc = VViewOpenSchema( r_ctx . view, & schema );
            DISP_RC( rc, "VViewOpenSchema() failed" );
            if ( rc == 0 ) {
                /* translate in special columns to numeric values to strings */
                vdcd_ins_trans_fkt( r_ctx . col_defs, schema );
                rc = vdh_vschema_release( rc, schema );
            }
            rc = VCursorOpen( r_ctx.cursor );
            DISP_RC( rc, "VCursorOpen() failed" );
            if ( 0 == rc ) {
                int64_t  first;
                uint64_t count;
                rc = VCursorIdRange( r_ctx . cursor, 0, &first, &count );
                DISP_RC( rc, "VCursorIdRange() failed" );
                if ( 0 == rc ) {
                    if ( NULL == ctx -> rows ) {
                        /* if the user did not specify a row-range, take all rows */
                        rc = num_gen_make_from_range( &( ctx -> rows ), first, count );
                        DISP_RC( rc, "num_gen_make_from_range() failed" );
                    } else {
                        /* if the user did specify a row-range, check the boundaries */
                        if ( count > 0 ) {
                            /* trim only if the row-range is not zero, otherwise
                                we will not get data if the user specified only static columns
                                because they report a row-range of zero! */
                            rc = num_gen_trim( ctx -> rows, first, count );
                            DISP_RC( rc, "num_gen_trim() failed" );
                        }
                    }
                    if ( 0 == rc ) {
                        if ( num_gen_empty( ctx -> rows ) ) {
                            rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                        } else {
                            r_ctx.ctx = ctx;
                            rc = vdm_dump_rows( &r_ctx ); /* <--- */
                        }
                    }
                }
                rc = vdh_vcursor_release( rc, r_ctx . cursor );
            }
            rc = vdh_view_release( rc, r_ctx . view );
            vdcd_destroy( r_ctx.col_defs );
            if ( 0 == rc && invalid_columns > 0 ) {
                rc = RC( rcExe, rcDatabase, rcResolving, rcColumn, rcInvalid );
            }
        }
    }
    return rc;
}

static rc_t vdm_print_objver( const p_dump_context ctx, const VDBManager *mgr ) {
    ver_t version;
    rc_t rc = VDBManagerGetObjVersion ( mgr, &version, ctx -> path );
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerGetObjVersion( '%s' ) -> %R", ctx -> path, rc );
    } else {
        rc = KOutMsg( "%V\n", version );
    }
    return rc;
}

static rc_t vdm_print_objts ( const p_dump_context ctx, const VDBManager *mgr ) {
    KTime_t timestamp;
    rc_t rc = VDBManagerGetObjModDate ( mgr, &timestamp, ctx -> path );
    if ( 0 != rc ) {
        ErrMsg( "VDBManagerGetObjModDate( '%s' ) -> %R", ctx -> path, rc  );
    } else {
        KTime kt;
        KTimeGlobal ( & kt, timestamp );
        rc = KOutMsg ( "%04u-%02u-%02uT"
                       "%02u:%02u:%02uZ"
                       "\n",
                       kt . year,
                       kt . month,
                       kt . day,
                       kt . hour,
                       kt . minute,
                       kt . second
            );
    }
    return rc;
}

static rc_t vdm_print_objtype( const VDBManager *mgr, const char * acc_or_path ) {
    int path_type = ( VDBManagerPathType ( mgr, "%s", acc_or_path ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type ) {
    case kptDatabase      :   return KOutMsg( "Database\n" ); break;
    case kptTable         :   return KOutMsg( "Table\n" ); break;
    case kptPrereleaseTbl :   return KOutMsg( "Prerelease Table\n" ); break;
    case kptColumn        :   return KOutMsg( "Column\n" ); break;
    case kptIndex         :   return KOutMsg( "Index\n" ); break;
    case kptNotFound      :   return KOutMsg( "not found\n" ); break;
    case kptBadPath       :   return KOutMsg( "bad path\n" ); break;
    case kptFile          :   return KOutMsg( "File\n" ); break;
    case kptDir           :   return KOutMsg( "Dir\n" ); break;
    case kptCharDev       :   return KOutMsg( "CharDev\n" ); break;
    case kptBlockDev      :   return KOutMsg( "BlockDev\n" ); break;
    case kptFIFO          :   return KOutMsg( "FIFO\n" ); break;
    case kptZombieFile    :   return KOutMsg( "ZombieFile\n" ); break;
    case kptDataset       :   return KOutMsg( "Dataset\n" ); break;
    case kptDatatype      :   return KOutMsg( "Datatype\n" ); break;
    default               :   return KOutMsg( "value = '%i'\n", path_type ); break;
    }
    return 0;
}

static rc_t vdb_main_one_obj_by_pathtype( const p_dump_context ctx,
                                          const VDBManager *mgr ) {
    rc_t rc;
    int path_type = ( VDBManagerPathType ( mgr, "%s", ctx -> path ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type ) {
    case kptDatabase    :   rc = vdm_dump_database( ctx, mgr ); break;
    case kptPrereleaseTbl:
    case kptTable       :   rc = vdm_dump_table( ctx, mgr ); break;
    default             :   rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
                            PLOGERR( klogInt, ( klogInt, rc,
                                "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                                "p=%s", ctx->path ) );
                            break;
    }
    return rc;
}

static rc_t vdb_main_one_obj_by_probing( const p_dump_context ctx,
                                         const VDBManager *mgr ) {
    rc_t rc;
    if ( vdh_is_path_database( mgr, ctx -> path, &( ctx -> schema_list ) ) ) {
        rc = vdm_dump_database( ctx, mgr );
    } else if ( vdh_is_path_table( mgr, ctx -> path, &( ctx -> schema_list ) ) ) {
        rc = vdm_dump_table( ctx, mgr );
    } else {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        PLOGERR( klogInt, ( klogInt, rc,
                 "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                 "p=%s", ctx -> path ) );
    }
    return rc;
}

static rc_t vdm_main_one_obj( const p_dump_context ctx,
                              const VDBManager *mgr,
                              const char * acc_or_path ) {
    rc_t rc = 0;
    ctx -> path = string_dup_measure ( acc_or_path, NULL );
    if ( ctx -> objver_requested ) {
        rc = vdm_print_objver( ctx, mgr );
    } else if ( ctx -> objts_requested ) {
        rc = vdm_print_objts ( ctx, mgr );
    } else if ( ctx -> objtype_requested ) {
        vdm_print_objtype( mgr, acc_or_path );
    } else if ( ctx -> view_defined ) {
        rc = vdm_dump_unbound_view ( ctx, mgr ); // V<t1> where V is an ubound view, needs binding to source(s)
    } else {
        if ( USE_PATHTYPE_TO_DETECT_DB_OR_TAB ) { /* in vdb-dump-context.h */
            rc = vdb_main_one_obj_by_pathtype( ctx, mgr );
        } else {
            rc = vdb_main_one_obj_by_probing( ctx, mgr );
        }
    }
    free( (char*)ctx -> path );
    ctx -> path = NULL;
    return rc;
}

/***************************************************************************
    dump_main:
    * called by "main()"
    * make the "native directory"
    * make a vdb-manager for read
      all subsequent dump-functions will use this manager...
    * show the manager-version, if it was requested from the command-line
    * check if the given path is database-path ( by trying to open it )
      if it is one: continue wit dump_database()
    * check if the given path is table-path ( by trying to open it )
      if it is one: continue wit dump_table()
    * release manager and directory

ctx        [IN] ... contains path, tablename, columns, row-range etc.
***************************************************************************/
static rc_t vdm_main( const p_dump_context ctx, Args * args ) {
    KDirectory *dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    DISP_RC( rc, "KDirectoryNativeDir() failed" );
    if ( 0 == rc ) {
        const VDBManager *mgr;
        rc = VDBManagerMakeRead( &mgr, dir );
        DISP_RC( rc, "VDBManagerMakeRead() failed" );
        if( 0 == rc ) {
            if ( ctx -> disable_multithreading ) {
                rc = VDBManagerDisablePagemapThread( mgr );
                DISP_RC( rc, "VDBManagerDisablePagemapThread() failed" );
                rc = 0; /* not a typo, we continue even if we cannot disable threads! */
            }
            /* show manager is independend form db or tab */
            if ( ctx -> version_requested ) {
                rc = vdh_show_manager_version( mgr );
            } else {
                uint32_t count;
                rc = ArgsParamCount( args, &count );
                DISP_RC( rc, "ArgsParamCount() failed" );
                if ( 0 == rc ) {
                    if ( count > 0 ) {
                        uint32_t idx;
                        for ( idx = 0; idx < count && 0 == rc; ++idx ) {
                            const char *value = NULL;
                            rc = ArgsParamValue( args, idx, (const void **)&value );
                            DISP_RC( rc, "ArgsParamValue() failed" );
                            if ( 0 == rc ) {
                                if ( ctx -> print_info ) {
                                    rc = vdb_info( &( ctx -> schema_list ), ctx -> format, mgr,
                                                   value, ctx -> rows );   /* in vdb_info.c */
                                } else if ( ctx -> inspect ) {
                                    rc = vdi_inspect( dir, mgr, value, ctx -> sum_num_elem, ctx -> format );
                                } else if ( ctx -> len_spread ) {
                                    rc = vdf_len_spread( ctx, mgr, value ); /* in vdb-dump-fastq.c */
                                } else {
                                    switch( ctx -> format ) {
                                    case df_fastq  : ;
                                    case df_fastq1 : ;
                                    case df_fasta  : ;
                                    case df_fasta1 : ;
                                    case df_fasta2 : ;
                                    case df_qual   : ;
                                    case df_qual1  : vdf_main( ctx, mgr, value ); break; /* in vdb-dump-fastq.c */
                                    default : rc = vdm_main_one_obj( ctx, mgr, value ); break;
                                    }
                                }
                            }
                        }
                    } else {
                        UsageSummary ( UsageDefaultName );
                        rc = SILENT_RC( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
                    }
                }
            }
            rc = vdh_vmanager_release( rc, mgr );
        }
        rc = vdh_kdirectory_release( rc, dir );
    }
    return rc;
}

/***************************************************************************
    Main:
    * create the dump-context
    * parse the commandline for arguments and options
    * react to help/usage - requests ( no dump in this case )
      these functions are in vdb-dump-context.c
    * call dump_main() to execute the dump
    * destroy the dump-context
***************************************************************************/
static
rc_t CC write_to_FILE ( void *f, const char *buffer, size_t bytes, size_t *num_writ )
{
    * num_writ = fwrite ( buffer, 1, bytes, f );
    if ( * num_writ != bytes )
    {
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    }
    return 0;
}

/* uncomment the next line to compile vdb-dump with sqlite-support */
/* #define WITH_VDB_SHELL */
/*
 * in Makefile:
 *  - add 'vdb_shell' to VDB_DUMP_SRC
 *  - add -sngs to VDB_DUMP_LIB
 *  - add -svdb-sqlite to VDB_DUMP_LIB ( we need libxml2-dev to be installed for this )
 *  - add -lreadline to VDB_DUMP_LIB ( we need libreadline-dev to be installed for this )
 *  compile vdb_shell.c with '-DHAVE_READLINE'
 */

#ifdef WITH_VDB_SHELL
    int main_vdb_shell_org( int argc, char **argv );    /* to be found in vdb_shell.c! */
#endif

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    rc_t rc;
    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

#ifdef WITH_VDB_SHELL
    if ( argc > 1 && ( 0 == string_cmp ( argv[ 1 ], 7, "--shell", 7, 7 ) ) )
    {
        return main_vdb_shell_org( argc, argv );
    }
#endif

    rc = KOutHandlerSet( write_to_FILE, stdout );
    DISP_RC( rc, "KOutHandlerSet() failed" );
    if ( 0 == rc )
    {
        rc = ArgsMakeAndHandle( &args, argc, argv,
            1, DumpOptions, sizeof DumpOptions / sizeof DumpOptions [ 0 ] );
        DISP_RC( rc, "ArgsMakeAndHandle() failed" );
        if ( 0 == rc )
        {
            dump_context *ctx;  /* vdb-dump-context.h */

            rc = vdco_init( &ctx );
            if ( 0 == rc )
            {
                rc = vdco_capture_arguments_and_options( args, ctx ); /* vdb-dump-context.c */
                if ( 0 == rc )
                {
                    out_redir redir; /* vdb-dump-redir.h */

                    KLogHandlerSetStdErr();
                    rc = init_out_redir( &redir,
                                     ctx -> compress_mode,
                                     ctx -> output_file,
                                     ctx -> interactive ? 0 : ctx -> output_buffer_size,
                                     ctx -> append ); /* vdb-dump-redir.c */

                    if ( 0 == rc )
                    {
                        rc = vdm_main( ctx, args );         /* <=== code is above */
                        {
                            rc_t rc2 = release_out_redir( &redir );        /* vdb-dump-redir.c */
                            rc = ( 0 == rc ) ? rc2 : rc;
                        }
                    }
                }
                vdco_destroy( ctx );
            }
            {
                rc_t rc2 = ArgsWhack( args );
                DISP_RC( rc2, "ArgsWhack() failed" );
                rc = ( 0 == rc ) ? rc2 : rc;
            }
        }
    }
    return VDB_TERMINATE( rc );
}
