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
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>
#include <vdb/dependencies.h>
#include <vdb/vdb-priv.h>

#include <kdb/table.h>
#include <kdb/column.h>
#include <kdb/manager.h>
#include <kdb/namelist.h>
#include <kdb/meta.h>

#include <kfs/directory.h>
#include <kns/manager.h>

#include <kapp/main.h>
#include <kapp/args.h>
#include <kapp/args-conv.h>

#include <klib/container.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/debug.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/rc.h>
#include <klib/namelist.h>
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
#include "vdb-dump-bin.h"
#include "vdb-dump-interact.h"
#include "vdb_info.h"

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
static const char * dna_bases_usage[]           = { "print dna-bases",                              NULL };
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
static const char * show_blobbing_usage[]       = { "show blobbing",                                NULL };
static const char * enum_phys_usage[]           = { "enumerate physical columns",                   NULL };
static const char * enum_readable_usage[]       = { "enumerate readable columns",                   NULL };
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
static const char * slice_usage[]               = { "find a slice of given depth",                  NULL };
static const char * interactive_usage[]         = { "interactive mode",                             NULL };

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
    { OPTION_SHOW_BLOBBING,         NULL,                     NULL, show_blobbing_usage,     1, false,  false },
    { OPTION_ENUM_PHYS,             NULL,                     NULL, enum_phys_usage,         1, false,  false },
    { OPTION_ENUM_READABLE,         NULL,                     NULL, enum_readable_usage,     1, false,  false },
    { OPTION_OBJVER,                ALIAS_OBJVER,             NULL, objver_usage,            1, false,  false },
    { OPTION_OBJTS,                 NULL,                     NULL, objts_usage,             1, false,  false },
    { OPTION_OBJTYPE,               ALIAS_OBJTYPE,            NULL, objtype_usage,           1, false,  false },
    { OPTION_IDX_ENUM,              NULL,                     NULL, idx_enum_usage,          1, false,  false },
    { OPTION_IDX_RANGE,             NULL,                     NULL, idx_range_usage,         1, true,   false },
    { OPTION_CUR_CACHE,             NULL,                     NULL, cur_cache_usage,         1, true,   false },
    { OPTION_OUT_FILE,              NULL,                     NULL, out_file_usage,          1, true,   false },
    { OPTION_OUT_PATH,              NULL,                     NULL, out_path_usage,          1, true,   false },
    { OPTION_PHASE,                 NULL,                     NULL, NULL,                   1, true,   false },
    { OPTION_GZIP,                  NULL,                     NULL, gzip_usage,              1, false,  false },
    { OPTION_BZIP2,                 NULL,                     NULL, bzip2_usage,             1, false,  false },
    { OPTION_OUT_BUF_SIZE,          NULL,                     NULL, outbuf_size_usage,       1, true,   false },
    { OPTION_NO_MULTITHREAD,        NULL,                     NULL, disable_mt_usage,        1, false,  false },
    { OPTION_INFO,                  NULL,                     NULL, info_usage,              1, false,  false },
    { OPTION_DIFF,                  NULL,                     NULL, NULL,                   1, false,  false },
    { OPTION_SPOTGROUPS,            NULL,                     NULL, spotgroup_usage,         1, false,  false },
    { OPTION_MERGE_RANGES,          NULL,                     NULL, merge_ranges_usage,      1, false,  false },
    { OPTION_SPREAD,                NULL,                     NULL, spread_usage,            1, false,  false },
    { OPTION_INTERACTIVE,           NULL,                     NULL, interactive_usage,       1, false,  false },    
    { OPTION_SLICE,                 NULL,                     NULL, slice_usage,             1, true,   false }
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

    if ( args == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram ( args, &fullpath, &progname );

    if ( rc )
        progname = fullpath = UsageDefaultName;

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
    HelpOptionLine ( ALIAS_DNA_BASES,           OPTION_DNA_BASES,       "dna_bases",    dna_bases_usage );
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
    HelpOptionLine ( ALIAS_EXCLUDED_COLUMNS,    OPTION_EXCLUDED_COLUMNS,NULL,           excluded_columns_usage );
    HelpOptionLine ( ALIAS_BOOLEAN,             OPTION_BOOLEAN,         NULL,           boolean_usage );
    HelpOptionLine ( ALIAS_OBJVER,              OPTION_OBJVER,          NULL,           objver_usage );
    HelpOptionLine ( NULL,                      OPTION_OBJTS,           NULL,           objts_usage );
    HelpOptionLine ( ALIAS_OBJTYPE,             OPTION_OBJTYPE,         NULL,           objtype_usage );
    HelpOptionLine ( ALIAS_NUMELEM,             OPTION_NUMELEM,         NULL,           numelem_usage );
    HelpOptionLine ( ALIAS_NUMELEMSUM,          OPTION_NUMELEMSUM,      NULL,           numelemsum_usage );
    HelpOptionLine ( NULL,                      OPTION_SHOW_BLOBBING,   NULL,           show_blobbing_usage );
    HelpOptionLine ( NULL,                      OPTION_ENUM_PHYS,       NULL,           enum_phys_usage );
    HelpOptionLine ( NULL,                      OPTION_ENUM_READABLE,   NULL,           enum_readable_usage );
    HelpOptionLine ( NULL,                      OPTION_IDX_ENUM,        NULL,           idx_enum_usage );    
    HelpOptionLine ( NULL,                      OPTION_IDX_RANGE,       NULL,           idx_range_usage );    
    HelpOptionLine ( NULL,                      OPTION_CUR_CACHE,       NULL,           cur_cache_usage );    
    HelpOptionLine ( NULL,                      OPTION_OUT_FILE,        NULL,           out_file_usage );
    HelpOptionLine ( NULL,                      OPTION_OUT_PATH,        NULL,           out_path_usage );
    HelpOptionLine ( NULL,                      OPTION_GZIP,            NULL,           gzip_usage );
    HelpOptionLine ( NULL,                      OPTION_BZIP2,           NULL,           bzip2_usage );
    HelpOptionLine ( NULL,                      OPTION_OUT_BUF_SIZE,    NULL,           outbuf_size_usage );
    HelpOptionLine ( NULL,                      OPTION_NO_MULTITHREAD,  NULL,           disable_mt_usage );
    HelpOptionLine ( NULL,                      OPTION_INFO,            NULL,           info_usage );
    HelpOptionLine ( NULL,                      OPTION_SPOTGROUPS,      NULL,           spotgroup_usage );
    HelpOptionLine ( NULL,                      OPTION_MERGE_RANGES,    NULL,           merge_ranges_usage );
    HelpOptionLine ( NULL,                      OPTION_SPREAD,          NULL,           spread_usage );
    
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
static void CC vdm_read_cell_data( void *item, void *data )
{
    dump_src src; /* defined in vdb-dump-tools.h */
    p_col_def my_col_def = (p_col_def)item;
    p_row_context r_ctx = (p_row_context)data;

    if ( r_ctx->rc != 0 ) return; /* important to stop if the last read was not successful */
    vds_clear( &(my_col_def->content) ); /* clear the destination-dump-string */
    if ( my_col_def->valid == false ) return;
    if ( my_col_def->excluded == true ) return;

    /* read the data of a cursor-cell: buffer-addr, offset and element-count
       is stored in the dump_src-struct */
    r_ctx->rc = VCursorCellData( r_ctx->cursor, my_col_def->idx, NULL, &src.buf,
                                 &src.offset_in_bits, &src.number_of_elements );
    if ( r_ctx->rc != 0 )
    {
        if( UIError(r_ctx->rc, NULL, r_ctx->table) ) {
            UITableLOGError(r_ctx->rc, r_ctx->table, true);
        } else {
            PLOGERR( klogInt,
                     (klogInt,
                     r_ctx->rc,
                     "VCursorCellData( col:$(col_name) at row #$(row_nr) ) failed",
                     "col_name=%s,row_nr=%lu",
                      my_col_def->name, r_ctx->row_id ));
            /* be forgiving and continue if a cell cannot be read */
        }
        r_ctx->rc = 0;
    }

    /* check the type-domain */
    if ( ( my_col_def->type_desc.domain < vtdBool )||
         ( my_col_def->type_desc.domain > vtdUnicode ) )
    {
        vds_append_str( &(my_col_def->content), "unknown data-type" );
    }
    else
    {
        bool print_comma = true;
        bool sra_dump_format;

        /* initialize the element-idx ( for dimension > 1 ) */
        src.element_idx = 0;

        /* transfer context-flags (hex-print, no sra-types) */
        src.in_hex = r_ctx->ctx->print_in_hex;
        src.without_sra_types = r_ctx->ctx->without_sra_types;

        /* special treatment to suppress spaces between values */
        sra_dump_format = ( r_ctx->ctx->format == df_sra_dump );

        /* hardcoded printing of dna-bases if the column-type fits */
        src.print_dna_bases = ( r_ctx->ctx->print_dna_bases &
                    ( my_col_def->type_desc.intrinsic_dim == 2 ) &
                    ( my_col_def->type_desc.intrinsic_bits == 1 ) );

        /* how a boolean is displayed */
        src.c_boolean = r_ctx->ctx->c_boolean;

        if ( my_col_def->type_desc.domain == vtdBool && src.c_boolean != 0 )
        {
            print_comma = false;
        }

        if ( r_ctx->ctx->print_num_elem )
        {
            char temp[ 16 ];
            size_t num_writ;

            r_ctx->rc = string_printf ( temp, sizeof temp, &num_writ, "%u", src.number_of_elements ); 
            if ( r_ctx->rc == 0 )
                vds_append_str( &(my_col_def->content), temp );
        }
        else if ( r_ctx->ctx->sum_num_elem )
        {
            my_col_def->elementsum += src.number_of_elements;
        }
        else
        {
            /* loop through the elements(dimension's) of a cell */
            while( ( src.element_idx < src.number_of_elements )&&( r_ctx->rc == 0 ) )
            {
                uint32_t eidx = src.element_idx;
                if ( ( eidx > 0 )&& ( src.print_dna_bases == false ) && print_comma )
                {
                    if ( sra_dump_format )
                        vds_append_str( &(my_col_def->content), "," );
                    else
                        vds_append_str( &(my_col_def->content), ", " );
                }

                /* dumps the basic data-types, implementation in vdb-dump-tools.c
                   >>> that means it appends the element-string to
                       my_col_def->content <<<
                   the formated output is only collected, to be printed later
                   dump_element is also responsible for incrementing
                   the src.element_idx by: 1...bool/int/uint/float
                                           n...string/unicode-string */
                r_ctx->rc = vdt_dump_element( &src, my_col_def, !sra_dump_format );

                /* insurance against endless loop */
                if ( eidx == src.element_idx )
                {
                    src.element_idx++;
                }
            }
        }
    }
}



static void CC vdm_print_elem_sum( void *item, void *data )
{
    char temp[ 16 ];
    size_t num_writ;
    p_col_def my_col_def = (p_col_def)item;
    p_row_context r_ctx = (p_row_context)data;

    if ( r_ctx->rc != 0 ) return; /* important to stop if the last read was not successful */
    vds_clear( &(my_col_def->content) ); /* clear the destination-dump-string */

    r_ctx->rc = string_printf ( temp, sizeof temp, &num_writ, "%u", my_col_def->elementsum ); 
    if ( r_ctx->rc == 0 )
        vds_append_str( &(my_col_def->content), temp );
}


static void vdm_row_error( const char * fmt, rc_t rc, uint64_t row_id )
{
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
static rc_t vdm_dump_rows( p_row_context r_ctx )
{
    /* the important row_id is a member of r_ctx ! */
    r_ctx->rc = vds_make( &(r_ctx->s_col), r_ctx->ctx->max_line_len, 512 );
    if ( r_ctx->rc != 0 )
        vdm_row_error( "dump_str_make( row#$(row_nr) ) failed", r_ctx->rc, r_ctx->row_id );
    else
    {
        const struct num_gen_iter * iter;

        r_ctx->rc = num_gen_iterator_make( r_ctx->ctx->rows, &iter );
        if ( r_ctx->rc != 0 )
            vdm_row_error( "num_gen_iterator_make( row#$(row_nr) ) failed", r_ctx->rc, r_ctx->row_id );
        else
        {
            while ( ( r_ctx->rc == 0 ) && num_gen_iterator_next( iter, &(r_ctx->row_id), &(r_ctx->rc) ) )
            {
                if ( r_ctx-> rc == 0 )
                    r_ctx-> rc = Quitting();
                if ( r_ctx->rc != 0 )
                    break;
                r_ctx->rc = VCursorSetRowId( r_ctx->cursor, r_ctx->row_id );
                if ( r_ctx->rc != 0 )
                {
                    vdm_row_error( "VCursorSetRowId( row#$(row_nr) ) failed", 
                                   r_ctx->rc, r_ctx->row_id );
                }
                else
                {
                    r_ctx->rc = VCursorOpenRow( r_ctx->cursor );
                    if ( r_ctx->rc != 0 )
                    {
                        vdm_row_error( "VCursorOpenRow( row#$(row_nr) ) failed", 
                                       r_ctx->rc, r_ctx->row_id );
                    }
                    else
                    {
                        /* first reset the string and valid-flag for every column */
                        vdcd_reset_content( r_ctx->col_defs );

                        /* read the data of every column and create a string for it */
                        VectorForEach( &(r_ctx->col_defs->cols),
                                       false, vdm_read_cell_data, r_ctx );

                        if ( r_ctx->rc == 0 )
                        {
                            /* prints the collected strings, in vdb-dump-formats.c */
                            if ( !r_ctx->ctx->sum_num_elem )
                            {
                                r_ctx->rc = vdfo_print_row( r_ctx );
                                if ( r_ctx->rc != 0 )
                                    vdm_row_error( "vdfo_print_row( row#$(row_nr) ) failed", 
                                           r_ctx->rc, r_ctx->row_id );
                            }
                        }
                        r_ctx->rc = VCursorCloseRow( r_ctx->cursor );
                        if ( r_ctx->rc != 0 )
                            vdm_row_error( "VCursorCloseRow( row#$(row_nr) ) failed", 
                                           r_ctx->rc, r_ctx->row_id );
                    }
                }
            }
        }
        num_gen_iterator_destroy( iter );

        if ( r_ctx->rc == 0 && r_ctx->ctx->sum_num_elem )
        {
            VectorForEach( &( r_ctx->col_defs->cols ), false, vdm_print_elem_sum, r_ctx );
            if ( r_ctx->rc == 0 )
            {
                r_ctx->rc = vdfo_print_row( r_ctx );
                DISP_RC( r_ctx->rc, "VTableOpenSchema() failed" );
            }
        }

        vds_free( &(r_ctx->s_col) );
    }
    return r_ctx->rc;
}


static uint32_t vdm_extract_or_parse_columns( const p_dump_context ctx,
                                          const VTable *my_table,
                                          p_col_defs my_col_defs )
{
    uint32_t count = 0;
    if ( ctx != NULL && my_col_defs != NULL )
    {
        bool cols_unknown = ( ( ctx->columns == NULL ) || ( string_cmp( ctx->columns, 1, "*", 1, 1 ) == 0 ) );
        if ( cols_unknown )
            /* the user does not know the column-names or wants all of them */
            count = vdcd_extract_from_table( my_col_defs, my_table );
        else
            /* the user knows the names of the wanted columns... */
            count = vdcd_parse_string( my_col_defs, ctx->columns, my_table );

        if ( ctx->excluded_columns != NULL )
            vdcd_exclude_these_columns( my_col_defs, ctx->excluded_columns );
    }

    return count;
}


static bool vdm_extract_or_parse_phys_columns( const p_dump_context ctx,
                                               const VTable *my_table,
                                               p_col_defs my_col_defs )
{
    bool res = false;
    if ( ctx != NULL && my_col_defs != NULL )
    {
        bool cols_unknown = ( ( ctx->columns == NULL ) || ( string_cmp( ctx->columns, 1, "*", 1, 1 ) == 0 ) );
        if ( cols_unknown )
            /* the user does not know the column-names or wants all of them */
            res = vdcd_extract_from_phys_table( my_col_defs, my_table );
        else
            /* the user knows the names of the wanted columns... */
            res = vdcd_parse_string( my_col_defs, ctx->columns, my_table );

        if ( ctx->excluded_columns != NULL )
            vdcd_exclude_these_columns( my_col_defs, ctx->excluded_columns );
    }

    return res;
}

/*************************************************************************************
    dump_tab_table:
    * called by "dump_db_table()" and "dump_tab()" as a fkt-pointer
    * opens a cursor to read
    * checks if the user did not specify columns, or wants all columns ( "*" )
        no columns specified ---> calls "col_defs_extract_from_table()"
        columns specified ---> calls "col_defs_parse_string()"
    * we end up with a list of column-definitions (name,type) in my_col_defs
    * calls "col_defs_add_to_cursor()" to add them to the cursor
    * opens the cursor
    * calls "dump_rows()" to execute the dump
    * destroys the my_col_defs - structure
    * releases the cursor

ctx       [IN] ... contains path, tablename, columns, row-range etc.
my_table  [IN] ... open table needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_opened_table( const p_dump_context ctx, const VTable *my_table )
{
    rc_t rc;

    if ( ctx->format == df_bin )
    {
        rc = vdi_dump_opened_table( ctx, my_table ); /* from vdb-dump-bin.c */
    }
    else
    {
        row_context r_ctx;

        rc = VTableCreateCachedCursorRead( my_table, &(r_ctx.cursor), ctx->cur_cache_size );
        DISP_RC( rc, "VTableCreateCursorRead() failed" );
        if ( rc == 0 )
        {
            r_ctx.table = my_table;
            if ( !vdcd_init( &(r_ctx.col_defs), ctx->max_line_len ) )
            {
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                DISP_RC( rc, "col_defs_init() failed" );
            }

            if ( rc == 0 )
            {
                uint32_t n = vdm_extract_or_parse_columns( ctx, my_table, r_ctx.col_defs );
                if ( n < 1 )
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                else
                {
                    n = vdcd_add_to_cursor( r_ctx.col_defs, r_ctx.cursor );
                    if ( n < 1 )
                        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                    else
                    {
                        const VSchema *my_schema;
                        rc = VTableOpenSchema( my_table, &my_schema );
                        DISP_RC( rc, "VTableOpenSchema() failed" );
                        if ( rc == 0 )
                        {
                            /* translate in special columns to numeric values to strings */
                            vdcd_ins_trans_fkt( r_ctx.col_defs, my_schema );
                            VSchemaRelease( my_schema );
                        }

                        rc = VCursorOpen( r_ctx.cursor );
                        DISP_RC( rc, "VCursorOpen() failed" );
                        if ( rc == 0 )
                        {
                            int64_t  first;
                            uint64_t count;
                            rc = VCursorIdRange( r_ctx.cursor, 0, &first, &count );
                            DISP_RC( rc, "VCursorIdRange() failed" );
                            if ( rc == 0 )
                            {
                                if ( ctx->rows == NULL )
                                {
                                    /* if the user did not specify a row-range, take all rows */
                                    rc = num_gen_make_from_range( &ctx->rows, first, count );
                                    DISP_RC( rc, "num_gen_make_from_range() failed" );
                                }
                                else
                                {
                                    /* if the user did specify a row-range, check the boundaries */
                                    if ( count > 0 )
                                    {
                                        /* trim only if the row-range is not zero, otherwise
                                           we will not get data if the user specified only static columns
                                           because they report a row-range of zero! */
                                        rc = num_gen_trim( ctx->rows, first, count );
                                        DISP_RC( rc, "num_gen_trim() failed" );
                                    }
                                }

                                if ( rc == 0 )
                                {
                                    if ( num_gen_empty( ctx->rows ) )
                                    {
                                        rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                                    }
                                    else
                                    {
                                        r_ctx.ctx = ctx;
                                        rc = vdm_dump_rows( &r_ctx ); /* <--- */
                                    }
                                }
                            }
                        }
                    }
                }
                vdcd_destroy( r_ctx.col_defs );
            }
            VCursorRelease( r_ctx.cursor );
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

ctx         [IN] ... contains path, tablename, columns, row-range etc.
my_database [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_walk_sections( const VDatabase * base_db, const VDatabase ** sub_db,
                               const VNamelist * sections, uint32_t count )
{
    rc_t rc = 0;
    const VDatabase * parent_db = base_db;
    if ( count == 0 )
    {
        rc = VDatabaseAddRef ( parent_db );
        DISP_RC( rc, "VDatabaseAddRef() failed" );
    }
    else
    {
        uint32_t idx;
        for ( idx = 0; rc == 0 && idx < count; ++idx )
        {
            const char * dbname;
            rc = VNameListGet ( sections, idx, &dbname );
            DISP_RC( rc, "VNameListGet() failed" );
            if ( rc == 0 )
            {
                const VDatabase * temp;
                rc = VDatabaseOpenDBRead ( parent_db, &temp, "%s", dbname );
                DISP_RC( rc, "VDatabaseOpenDBRead() failed" );
                if ( rc == 0 && idx > 0 )
                {
                    rc = VDatabaseRelease ( parent_db );
                    DISP_RC( rc, "VDatabaseRelease() failed" );
                }
                if ( rc == 0 )
                    parent_db = temp;
            }
        }
    }
    
    if ( rc == 0 ) *sub_db = parent_db;
    return rc;
}


static void vdm_clear_recorded_errors( void )
{
    rc_t rc;
    const char * filename;
    const char * funcname;
    uint32_t line_nr;
    while ( GetUnreadRCInfo ( &rc, &filename, &funcname, &line_nr ) )
    {
    }
}


static rc_t vdm_check_table_empty( const VTable * tab )
{
    bool empty;
    rc_t rc = VTableIsEmpty( tab, &empty );
    DISP_RC( rc, "VTableIsEmpty() failed" );
    if ( rc == 0 && empty )
    {
        vdm_clear_recorded_errors();
        KOutMsg( "the requested table is empty!\n" );
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcTable, rcEmpty );
    }
    return rc;
}

static rc_t vdm_open_table_by_path( const VDatabase * db, const char * path, const VTable ** tab )
{
    VNamelist * sections;
    rc_t rc = vds_path_to_sections( path, '.', &sections );
    DISP_RC( rc, "vds_path_to_sections() failed" );
    if ( rc == 0 )
    {
        uint32_t count;
        rc = VNameListCount ( sections, &count );
        DISP_RC( rc, "VNameListCount() failed" );
        if ( rc == 0 && count > 0 )
        {
            const VDatabase * sub_db;
            rc = vdm_walk_sections( db, &sub_db, sections, count - 1 );
            if ( rc == 0 )
            {
                const char * tabname;
                rc = VNameListGet ( sections, count - 1, &tabname );
                DISP_RC( rc, "VNameListGet() failed" );
                if ( rc == 0 )
                {
                    rc = VDatabaseOpenTableRead( sub_db, tab, "%s", tabname );
                    DISP_RC( rc, "VDatabaseOpenTableRead() failed" );
                    if ( rc == 0 )
                    {
                        rc = vdm_check_table_empty( *tab );
                        if ( rc != 0 )
                        {
                            VTableRelease( *tab );
                            tab = NULL;
                        }
                    }
                }
                VDatabaseRelease ( sub_db );
            }
        }
        VNamelistRelease ( sections );
    }
    return rc;
}

static rc_t vdm_dump_opened_database( const p_dump_context ctx,
                                      const VDatabase *my_database )
{
    const VTable *my_table;
    rc_t rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );
    if ( rc == 0 )
    {
        rc = vdm_dump_opened_table( ctx, my_table );
        VTableRelease( my_table );
    }
    return rc;
}

/* ********************************************************************** */

static rc_t vdm_show_tab_spread( const p_dump_context ctx,
                                 const VTable *my_table )
{
    const VCursor * cursor;
    rc_t rc = VTableCreateCachedCursorRead( my_table, &cursor, ctx->cur_cache_size );
    DISP_RC( rc, "VTableCreateCursorRead() failed" );
    if ( rc == 0 )
    {
        col_defs * cols;
        if ( !vdcd_init( &cols, ctx->max_line_len ) )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }
        if ( rc == 0 )
        {
            uint32_t n = vdm_extract_or_parse_columns( ctx, my_table, cols );
            if ( n < 1 )
                rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
            else
            {
                n = vdcd_add_to_cursor( cols, cursor );
                if ( n < 1 )
                    rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                else
                {
                    rc = VCursorOpen( cursor );
                    DISP_RC( rc, "VCursorOpen() failed" );
                    if ( rc == 0 )
                    {
                        int64_t  first;
                        uint64_t count;
                        rc = VCursorIdRange( cursor, 0, &first, &count );
                        DISP_RC( rc, "VCursorIdRange( spread ) failed" );
                        if ( rc == 0 )
                        {
                            if ( ctx->rows == NULL )
                            {
                                rc = num_gen_make_from_range( &ctx->rows, first, count );
                                DISP_RC( rc, "num_gen_make_from_range() failed" );
                            }
                            else
                            {
                                if ( count > 0 )
                                {
                                    rc = num_gen_trim( ctx->rows, first, count );
                                    DISP_RC( rc, "num_gen_trim() failed" );
                                }
                            }
                            
                            if ( rc == 0 )
                            {
                                if ( num_gen_empty( ctx->rows ) )
                                    rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
                                else
                                    rc = vdcd_collect_spread( ctx->rows, cols, cursor ); /* is in vdb-dump-coldefs.c */
                            }
                        }
                    }
                }
            }
            vdcd_destroy( cols );
        }
        VCursorRelease( cursor );
    }
    return rc;
}

static rc_t vdm_show_db_spread( const p_dump_context ctx,
                                const VDatabase *my_database )
{
    const VTable *my_table;
    rc_t rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );
    if ( rc == 0 )
    {
        rc = vdm_show_tab_spread( ctx, my_table );
        VTableRelease( my_table );
    }
    return rc;
}
/* ********************************************************************** */

/********************************************************************
helper function, needed by "VSchemaDump()"
********************************************************************/
static rc_t CC vdm_schema_dump_flush( void *dst, const void *buffer, size_t bsize )
{
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

ctx         [IN] ... contains path, tablename, columns, row-range etc.
my_database [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_tab_schema( const p_dump_context ctx,
                                 const VTable *my_table )
{
    const VSchema * my_schema;
    rc_t rc = VTableOpenSchema( my_table, &my_schema );
    DISP_RC( rc, "VTableOpenSchema() failed" );
    if ( rc == 0 )
    {
        if ( ctx->columns == NULL )
        {
            /* the user did not ask to inspect a specific object, we look for
               the Typespec of the table... */
            char buffer[ 4096 ];
            rc = VTableTypespec ( my_table, buffer, sizeof buffer );
            DISP_RC( rc, "VTableTypespec() failed" );
            if ( rc == 0 )
                rc = VSchemaDump( my_schema, sdmPrint, buffer,
                                  vdm_schema_dump_flush, stdout );
        
        }
        else
        {
            /* the user did ask to inspect a specific object */
            rc = VSchemaDump( my_schema, sdmPrint, ctx->columns,
                              vdm_schema_dump_flush, stdout );
        }
        DISP_RC( rc, "VSchemaDump() failed" );
        VSchemaRelease( my_schema );
    }
    return rc;
}

/*************************************************************************************
    dump_the_db_schema:
    * called by "dump_database()" as a function pointer
    * opens a table to read
    * calls "dump_the_tab_schema()" to dump the schema of this table
    * releases the table

ctx         [IN] ... contains path, tablename, columns, row-range etc.
my_database [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_dump_db_schema( const p_dump_context ctx,
                                const VDatabase *my_database )
{
    rc_t rc = 0;
    if ( ctx->table_defined )
    {
        /* the user has given a database as object, but asks to inspect a given table */
        const VTable *my_table;
        rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );
        if ( rc == 0 )
        {
            rc = vdm_dump_tab_schema( ctx, my_table );
            VTableRelease( my_table );
        }
    }
    else
    {
        /* the user has given a database as object, but did not ask for a specific table */
        const VSchema * my_schema;
        rc = VDatabaseOpenSchema( my_database, &my_schema );
        DISP_RC( rc, "VDatabaseOpenSchema() failed" );
        if ( rc == 0 )
        {
            if ( ctx->columns == NULL )
            {
                /* the used did not ask to inspect a specifiy object, we look for
                   the Typespec of the database... */
                char buffer[ 4096 ];
                rc = VDatabaseTypespec ( my_database, buffer, sizeof buffer );
                DISP_RC( rc, "VDatabaseTypespec() failed" );
                if ( rc == 0 )
                    rc = VSchemaDump( my_schema, sdmPrint, buffer,
                                      vdm_schema_dump_flush, stdout );
            }
            else
            {
                /* the user did ask to inspect a specific object */
                rc = VSchemaDump( my_schema, sdmPrint, ctx->columns,
                                  vdm_schema_dump_flush, stdout );
            }
            DISP_RC( rc, "VSchemaDump() failed" );
            VSchemaRelease( my_schema );
        }
    }
    return rc;
}

/*************************************************************************************
    enum_tables:
    * called by "dump_database()" as a function pointer
    * calls VDatabaseListTbl() to get a list of Tables
    * loops through this list to print the names

ctx         [IN] ... contains path, tablename, columns, row-range etc.
my_database [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_enum_sub_dbs_and_tabs( const VDatabase * db, uint32_t indent );

static rc_t vdm_report_tab_or_db( const VDatabase * db, const KNamelist * list,
                                  const char * prefix, uint32_t indent )
{
    uint32_t n;
    rc_t rc = KNamelistCount( list, &n );
    DISP_RC( rc, "KNamelistCount() failed" );
    if ( rc == 0 )
    {
        uint32_t i;
        for ( i = 0; i < n && rc == 0; ++i )
        {
            const char * entry;
            rc = KNamelistGet( list, i, &entry );
            DISP_RC( rc, "KNamelistGet() failed" );
            if ( rc == 0 )
            {
                rc = KOutMsg( "%*s : %s\n", indent + 3, prefix, entry );
                if ( rc == 0 && db != NULL )
                {
                    const VDatabase * sub_db;
                    rc = VDatabaseOpenDBRead ( db, &sub_db, entry );
                    DISP_RC( rc, "VDatabaseOpenDBRead() failed" );
                    if ( rc == 0 )
                    {
                        rc = vdm_enum_sub_dbs_and_tabs( sub_db, indent + 3 ); /* recursion here... */
                        VDatabaseRelease( sub_db );
                    }
                }
            }
        }
    }
    return rc;
}

static rc_t vdm_enum_tabs_of_db( const VDatabase * db, uint32_t indent )
{
    KNamelist *tbl_names;
    rc_t rc = VDatabaseListTbl( db, &tbl_names );
    if ( rc == 0 )
    {
        rc = vdm_report_tab_or_db( NULL, tbl_names, "tbl", indent );
        KNamelistRelease( tbl_names );
    }
    else
    {
        rc = 0;
    }
    return rc;
}

static rc_t vdm_enum_sub_dbs_of_db( const VDatabase * db, uint32_t indent )
{
    KNamelist *db_names;
    rc_t rc = VDatabaseListDB( db, &db_names );
    if ( rc == 0 )
    {
        rc = vdm_report_tab_or_db( db, db_names, "db ", indent );
        KNamelistRelease( db_names );
    }
    else
    {
        rc = 0;
    }
    return rc;
}

static rc_t vdm_enum_sub_dbs_and_tabs( const VDatabase * db, uint32_t indent )
{
    rc_t rc = vdm_enum_sub_dbs_of_db( db, indent );
    if ( rc == 0 )
        rc = vdm_enum_tabs_of_db( db, indent );
    return rc;
}

static rc_t vdm_enum_tables( const p_dump_context ctx, const VDatabase * db )
{
    rc_t rc = KOutMsg( "enumerating the tables of database '%s'\n", ctx->path );
    if ( rc == 0 )
        rc = vdm_enum_sub_dbs_and_tabs( db, 0 );
    return rc;
}


typedef struct col_info_context
{
    p_dump_context ctx;
    const VSchema *my_schema;
    const VTable *my_table;
    const KTable *my_ktable;
} col_info_context;
typedef struct col_info_context* p_col_info_context;

static rc_t vdm_print_column_datatypes( const p_col_def col_def,
                                        const p_col_info_context ci )
{
    KNamelist *names;
    uint32_t dflt_idx;

    rc_t rc = VTableColumnDatatypes( ci->my_table, col_def->name, &dflt_idx, &names );
    DISP_RC( rc, "VTableColumnDatatypes() failed" );
    if ( rc == 0 )
    {
        uint32_t n;
        rc = KNamelistCount( names, &n );
        DISP_RC( rc, "KNamelistCount() failed" );
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < n && rc == 0; ++i )
            {
                const char *type_name;
                rc = KNamelistGet( names, i, &type_name );
                DISP_RC( rc, "KNamelistGet() failed" );
                if ( rc == 0 )
                {
                    if ( dflt_idx == i )
                        rc = KOutMsg( "%20s.type[%d] = %s (dflt)\n", col_def->name, i, type_name );
                    else
                        rc = KOutMsg( "%20s.type[%d] = %s\n", col_def->name, i, type_name );
                }
            }
        }
        rc = KNamelistRelease( names );
        DISP_RC( rc, "KNamelistRelease() failed" );
    }
    if ( rc == 0 )
        rc = KOutMsg( "\n" );
    return rc;
}


static rc_t vdm_show_row_blobbing( const p_col_def col_def,
                                   const p_col_info_context ci )
{
    const KColumn * kcol;
    rc_t rc = KTableOpenColumnRead( ci->my_ktable, &kcol, "%s", col_def->name );
    if ( rc != 0 )
        return KOutMsg( "'%s' is not a physical column\n", col_def->name );
    else
        rc = KOutMsg( "\nCOLUMN '%s':\n", col_def->name );

    if ( rc == 0 )
    {
        int64_t first;
        uint64_t count;
        rc = KColumnIdRange( kcol, &first, &count );
        DISP_RC( rc, "KColumnIdRange() failed" );
        if ( rc == 0 )
        {
            int64_t last = first + count - 1;
            rc = KOutMsg( "range: %,ld ... %,ld\n", first, last );
            if ( rc == 0 )
            {
                int64_t id = first;
                while ( id < last && rc == 0 )
                {
                    const KColumnBlob *blob;
                    rc = KColumnOpenBlobRead( kcol, &blob, id );
                    DISP_RC( rc, "KColumnOpenBlobRead() failed" );
                    if ( rc == 0 )
                    {
                        int64_t first_id_in_blob;
                        uint32_t ids_in_blob;
                        rc = KColumnBlobIdRange( blob, &first_id_in_blob, &ids_in_blob );
                        DISP_RC( rc, "KColumnBlobIdRange() failed" );
                        if ( rc == 0 )
                        {
                            int64_t last_id_in_blob = first_id_in_blob + ids_in_blob - 1;
                            char buffer[ 8 ];
                            size_t num_read, remaining;
                            rc = KColumnBlobRead ( blob, 0, &buffer, 0, &num_read, &remaining );
                            DISP_RC( rc, "KColumnBlobRead() failed" );
                            if ( rc == 0 )
                            {
                                rc = KOutMsg( "blob[ %,ld ... %,ld] size = %,zu\n", first_id_in_blob, last_id_in_blob, remaining + num_read );
                            }
                            id = last_id_in_blob + 1;
                        }
                        KColumnBlobRelease( blob );
                    }
                }
            }
        }
        KColumnRelease( kcol );
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
static rc_t vdm_print_column_info( const p_col_def col_def, p_col_info_context ci_ctx )
{
    rc_t rc = 0;

    if ( ci_ctx->ctx->show_blobbing )
        rc = vdm_show_row_blobbing( col_def, ci_ctx );
    else
    {
        /* print_col_info is in vdb-dump-helper.c */
        rc = vdh_print_col_info( ci_ctx->ctx , col_def, ci_ctx->my_schema );

        /* to test VTableColumnDatatypes() */
        if ( rc == 0  && ci_ctx->ctx->column_enum_requested )
            rc = vdm_print_column_datatypes( col_def, ci_ctx );
    }
    return rc;
}


static rc_t vdm_enum_phys_columns( const VTable *my_table )
{
    rc_t rc = KOutMsg( "physical columns:\n" );
    if ( rc == 0 )
    {
        KNamelist *phys;
        rc = VTableListPhysColumns( my_table, &phys );
        DISP_RC( rc, "VTableListPhysColumns() failed" );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = KNamelistCount( phys, &count );
            DISP_RC( rc, "KNamelistCount( physical columns ) failed" );
            if ( rc == 0 )
            {
                if ( count > 0 )
                {
                    uint32_t idx;
                    for ( idx = 0; idx < count && rc == 0; ++idx )
                    {
                        const char * name;
                        rc = KNamelistGet( phys, idx, &name );
                        DISP_RC( rc, "KNamelistGet( physical columns ) failed" );
                        if ( rc == 0 )
                            rc = KOutMsg( "[%.02d] = %s\n", idx, name );
                    }
                }
                else
                {
                    rc = KOutMsg( "... list is empty!\n" );
                }
            }
            KNamelistRelease( phys );
        }
    }
    return rc;
}


static rc_t vdm_enum_readable_columns( const VTable *my_table )
{
    rc_t rc = KOutMsg( "readable columns:\n" );
    if ( rc == 0 )
    {
        KNamelist *readable;
        rc = VTableListReadableColumns( my_table, &readable );
        DISP_RC( rc, "VTableListReadableColumns() failed" );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = KNamelistCount( readable, &count );
            DISP_RC( rc, "KNamelistCount( readable columns ) failed" );
            if ( rc == 0 )
            {
                if ( count > 0 )
                {
                    uint32_t idx;
                    for ( idx = 0; idx < count && rc == 0; ++idx )
                    {
                        const char * name;
                        rc = KNamelistGet( readable, idx, &name );
                        DISP_RC( rc, "KNamelistGet( readable columns ) failed" );
                        if ( rc == 0 )
                            rc = KOutMsg( "[%.02d] = %s\n", idx, name );
                    }
                }
                else
                {
                    rc = KOutMsg( "... list is empty!\n" );
                }
            }
            KNamelistRelease( readable );
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

ctx         [IN] ... contains path, tablename, columns, row-range etc.
my_database [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_enum_tab_columns( const p_dump_context ctx, const VTable *my_table )
{
    rc_t rc = 0;
    if ( ctx->enum_phys )
    {
        rc = vdm_enum_phys_columns( my_table );
    }
    else if ( ctx->enum_readable )
    {
        rc = vdm_enum_readable_columns( my_table );
    }
    else
    {
        col_defs *my_col_defs;
        if ( !vdcd_init( &my_col_defs, ctx->max_line_len ) )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }

        if ( rc == 0 )
        {
            col_info_context ci_ctx;
            bool extracted;

            ci_ctx.ctx = ctx;
            ci_ctx.my_table = my_table;
            if ( ctx->show_blobbing )
            {
                extracted = vdm_extract_or_parse_phys_columns( ctx, my_table, my_col_defs );
                rc = VTableOpenKTableRead( my_table, &ci_ctx.my_ktable );
                DISP_RC( rc, "VTableOpenKTableRead() failed" );
            }
            else
            {
                extracted = vdm_extract_or_parse_columns( ctx, my_table, my_col_defs );
                ci_ctx.my_ktable = NULL;
            }

            if ( extracted && rc == 0 )
            {
                rc = VTableOpenSchema( my_table, &(ci_ctx.my_schema) );
                DISP_RC( rc, "VTableOpenSchema() failed" );
                if ( rc == 0 )
                {
                    uint32_t idx, count;

                    ctx->generic_idx = 1;
                    count = VectorLength( &(my_col_defs->cols) );
                    for ( idx = 0; idx < count && rc == 0; ++idx )
                    {
                        col_def *col = ( col_def * )VectorGet( &(my_col_defs->cols), idx );
                        if ( col != 0 )
                        {
                            rc = vdm_print_column_info( col, &ci_ctx );
                        }
                    }
                    VSchemaRelease( ci_ctx.my_schema );
                }
                if ( ci_ctx.my_ktable != NULL )
                    KTableRelease( ci_ctx.my_ktable );
            }
            else
            {
                rc = KOutMsg( "error in col_defs_extract_from_table\n" );
            }
            vdcd_destroy( my_col_defs );
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

ctx         [IN] ... contains path, tablename, columns, row-range etc.
my_database [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_enum_db_columns( const p_dump_context ctx, const VDatabase *my_database )
{
    const VTable *my_table;
    rc_t rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );    
    if ( rc == 0 )
    {
        rc = vdm_enum_tab_columns( ctx, my_table );
        VTableRelease( my_table );
    }
    return rc;
}


static rc_t vdm_print_tab_id_range( const p_dump_context ctx, const VTable *my_table )
{
    const VCursor *my_cursor;
    rc_t rc = VTableCreateCursorRead( my_table, &my_cursor );
    DISP_RC( rc, "VTableCreateCursorRead() failed" );
    if ( rc == 0 )
    {
        col_defs *my_col_defs;

        if ( !vdcd_init( &my_col_defs, ctx->max_line_len ) )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            DISP_RC( rc, "col_defs_init() failed" );
        }

        if ( rc == 0 )
        {
            if ( vdm_extract_or_parse_columns( ctx, my_table, my_col_defs ) )
            {
                if ( vdcd_add_to_cursor( my_col_defs, my_cursor ) )
                {
                    rc = VCursorOpen( my_cursor );
                    DISP_RC( rc, "VCursorOpen() failed" );
                    if ( rc == 0 )
                    {
                        int64_t  first;
                        uint64_t count;

                        uint32_t idx = 0;
                        
                        /* calling with idx = 0 means: let the cursor find out the min/max values of
                           all open columns...
                           
                        vdcd_get_first_none_static_column_idx( my_col_defs, my_cursor, &idx );
                        */
                        
                        rc = VCursorIdRange( my_cursor, idx, &first, &count );
                        DISP_RC( rc, "VCursorIdRange() failed" );
                        if ( rc == 0 )
                            rc = KOutMsg( "id-range: first-row = %,ld, row-count = %,ld\n", first, count );
                    }
                }
            }
            vdcd_destroy( my_col_defs );
        }
        VCursorRelease( my_cursor );
    }
    return rc;
}

/*************************************************************************************
    print_db_id_range:
    * called by "dump_database()" as fkt-pointer
    * opens the table
    * calls print_tab_id_range()
    * releases table

ctx         [IN] ... contains path, tablename, columns, row-range etc.
my_database [IN] ... open database needed for vdb-calls
*************************************************************************************/
static rc_t vdm_print_db_id_range( const p_dump_context ctx, const VDatabase *my_database )
{
    const VTable *my_table;
    rc_t rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );
    if ( rc == 0 )
    {
        rc = vdm_print_tab_id_range( ctx, my_table );
        VTableRelease( my_table );
    }
    return rc;
}



/* ************************************************************************************ */

static rc_t vdm_enum_index( const KTable * my_ktable, uint32_t idx_nr, const char * idx_name )
{
    rc_t rc = KOutMsg( "idx #%u: %s", idx_nr + 1, idx_name );
    if ( rc == 0 )
    {
        const KIndex * my_idx;
        rc = KTableOpenIndexRead ( my_ktable, &my_idx, "%s", idx_name );
        if ( rc != 0 )
            rc = KOutMsg( " (cannot open)" );
        else
        {
            uint32_t idx_version;
            rc = KIndexVersion ( my_idx, &idx_version );
            if ( rc != 0 )
                rc = KOutMsg( " V?.?.?" );
            else
                rc = KOutMsg( " V%V", idx_version );

            if ( rc == 0 )
            {
                KIdxType idx_type;
                rc = KIndexType ( my_idx, &idx_type );
                if ( rc != 0 )
                    rc = KOutMsg( " type = ?" );
                else
                {
                    switch ( idx_type &~ kitProj )
                    {
                        case kitText : rc = KOutMsg( " type = Text" ); break;
                        case kitU64  : rc = KOutMsg( " type = U64" ); break;
                        default       : rc = KOutMsg( " type = unknown" ); break;
                    }
                    if ( rc == 0 && ( ( idx_type & kitProj ) == kitProj ) )
                        rc = KOutMsg( " reverse" );
                }
            }
            
            if ( rc == 0 )
            {
                bool locked = KIndexLocked ( my_idx );
                if ( locked )
                    rc = KOutMsg( " locked" );
            }
            KIndexRelease( my_idx );
        }
    }
    if ( rc == 0 )
        rc = KOutMsg( "\n" );
    return rc;
}


static rc_t vdm_enum_tab_index( const p_dump_context ctx, const VTable *my_table )
{
    const KTable * my_ktable;
    rc_t rc = VTableOpenKTableRead( my_table, &my_ktable );
    DISP_RC( rc, "VTableOpenKTableRead() failed" );
    if ( rc == 0 )
    {
        KNamelist *idx_names;
        rc = KTableListIdx ( my_ktable, &idx_names );
        if ( rc == 0 )
        {
            uint32_t count;
            rc = KNamelistCount( idx_names, &count );
            if ( rc == 0 )
            {
                uint32_t i;
                for ( i = 0; i < count && rc == 0; ++i )
                {
                    const char * idx_name = NULL;
                    rc = KNamelistGet( idx_names, i, &idx_name );
                    if ( rc == 0 && idx_name != NULL )
                        rc = vdm_enum_index( my_ktable, i, idx_name );
                }
            }
            KNamelistRelease( idx_names );
        }
        else
            rc = KOutMsg( "no index available\n" );
        KTableRelease( my_ktable );
    }
    return rc;
}

static rc_t vdm_enum_db_index( const p_dump_context ctx, const VDatabase *my_database )
{
    const VTable *my_table;
    rc_t rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );
    if ( rc == 0 )
    {
        rc = vdm_enum_tab_index( ctx, my_table );
        VTableRelease( my_table );
    }
    return rc;
}


/* ************************************************************************************ */

static rc_t vdm_range_tab_index( const p_dump_context ctx, const VTable *my_table )
{
    const KTable * my_ktable;
    rc_t rc = VTableOpenKTableRead( my_table, &my_ktable );
    if ( rc != 0 )
        ErrMsg( "VTableOpenKTableRead() -> %R", rc );
    else
    {
        const KIndex * my_kindex;
        rc = KTableOpenIndexRead ( my_ktable, &my_kindex, "%s", ctx->idx_range );
        if ( rc != 0 )
            ErrMsg( "KTableOpenIndexRead() -> %R", rc );
        else
        {
            int64_t start;
            uint64_t count;
            rc_t rc2 = 0;
            for ( start = 1; rc2 == 0 && rc == 0; start += count )
            {
                size_t key_size;
                char key [ 4096 ];
                rc2 = KIndexProjectText ( my_kindex, start, &start, &count,
                                         key, sizeof key, &key_size );
                if ( rc2 == 0 )
                    rc = KOutMsg( "%.*s : %lu ... %lu\n", ( int )key_size, key, start, start + count - 1 );
            }
            KIndexRelease( my_kindex );
        }
        KTableRelease( my_ktable );
    }
    return rc;
}


static rc_t vdm_range_db_index( const p_dump_context ctx, const VDatabase *my_database )
{
    const VTable *my_table;
    rc_t rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );
    if ( rc == 0 )
    {
        rc = vdm_range_tab_index( ctx, my_table );
        VTableRelease( my_table );
    }
    return rc;
}


/* ************************************************************************************ */

static rc_t vdm_show_tab_spotgroups( const p_dump_context ctx, const VTable *my_table )
{
    const KMetadata * meta = NULL;
    rc_t rc = VTableOpenMetadataRead( my_table, &meta );
    if ( rc != 0 )
        ErrMsg( "VTableOpenMetadataRead() -> %R", rc );
    else
    {
        const KMDataNode * spot_groups_node;
        rc = KMetadataOpenNodeRead( meta, &spot_groups_node, "STATS/SPOT_GROUP" );
        if ( rc != 0 )
            ErrMsg( "KMetadataOpenNodeRead( STATS/SPOT_GROUP ) -> %R", rc );
        else
        {
            KNamelist * spot_groups;
            rc = KMDataNodeListChildren( spot_groups_node, &spot_groups );
            if ( rc != 0 )
                ErrMsg( "KMDataNodeListChildren() -> %R", rc );
            else
            {
                uint32_t count;
                rc = KNamelistCount( spot_groups, &count );
                if ( rc != 0 )
                    ErrMsg( "KNamelistCount() -> %R", rc );
                else if ( count > 0 )
                {
                    uint32_t i;
                    for ( i = 0; i < count && rc == 0; ++i )
                    {
                        const char * name = NULL;
                        rc = KNamelistGet( spot_groups, i, &name );
                        if ( rc != 0 )
                            ErrMsg( "KNamelistCount( %d) -> %R", i, rc );
                        else if ( name != NULL )
                        {
                            const KMDataNode * spot_count_node;
                            rc = KMDataNodeOpenNodeRead( spot_groups_node, &spot_count_node, "%s/SPOT_COUNT", name );
                            if ( rc != 0 )
                                ErrMsg( "KMDataNodeOpenNodeRead() -> %R", rc );
                            else
                            {
                                uint64_t spot_count = 0;
                                rc = KMDataNodeReadAsU64( spot_count_node, &spot_count );
                                if ( rc != 0 )
                                {
                                    ErrMsg( "KMDataNodeReadAsU64() -> %R", rc );
                                    vdm_clear_recorded_errors();
                                }
                                else
                                {
                                    if ( spot_count > 0 )
                                    {
                                        const KMDataNode * spot_group_node;
                                        rc = KMDataNodeOpenNodeRead( spot_groups_node, &spot_group_node, name );
                                        if ( rc != 0 )
                                            ErrMsg( "KMDataNodeOpenNodeRead( '%s' ) -> %R", name, rc );
                                        else
                                        {
                                            char name_attr[ 2048 ];
                                            size_t num_writ;
                                            rc = KMDataNodeReadAttr( spot_group_node, "name", name_attr, sizeof name_attr, &num_writ );
                                            rc = KOutMsg( "%s\t%,lu\n", rc == 0 ? name_attr : name, spot_count );
                                            KMDataNodeRelease( spot_group_node );                
                                        }

                                    }
                                }
                                KMDataNodeRelease( spot_count_node );
                            }
                        }
                    }
                }
                KNamelistRelease( spot_groups );
            }
            KMDataNodeRelease( spot_groups_node );
        }
        KMetadataRelease ( meta );
    }
    return rc;
}

static rc_t vdm_show_db_spotgroups( const p_dump_context ctx, const VDatabase *my_database )
{
    const VTable *my_table;
    rc_t rc = vdm_open_table_by_path( my_database, ctx->table, &my_table );
    if ( rc == 0 )
    {
        rc = vdm_show_tab_spotgroups( ctx, my_table );
        VTableRelease( my_table );
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
                              const VDBManager *my_manager,
                              const db_tab_t tab_fkt )
{
    const VTable *my_table;
    VSchema *my_schema = NULL;
    rc_t rc;

    vdh_parse_schema( my_manager, &my_schema, &(ctx->schema_list), true /*ctx->force_sra_schema*/ );

    rc = VDBManagerOpenTableRead( my_manager, &my_table, my_schema, "%s", ctx->path );
    if ( rc != 0 )
        ErrMsg( "VDBManagerOpenTableRead( '%R' ) -> %R", ctx->path, rc );
    else
    {
        rc = vdm_check_table_empty( my_table );
        if ( rc == 0 )
            rc = tab_fkt( ctx, my_table ); /* fkt-pointer is called */
        VTableRelease( my_table );
    }

    if ( my_schema != NULL )
    {
        rc = VSchemaRelease( my_schema );
        if ( rc != 0 )
            ErrMsg( "VSchemaRelease() -> %R", rc );
    }
    return rc;
}


static bool enum_col_request( const p_dump_context ctx )
{
    return ( ctx->column_enum_requested || ctx->column_enum_short || 
             ctx->show_blobbing || ctx->enum_phys || ctx->enum_readable );
}

/***************************************************************************
    dump_table:
    * called by "dump_main()" to handle a table
    * calls the dump_tab_fkt with 3 different fkt-pointers as argument
      depending on what was selected at the commandline

ctx        [IN] ... contains path, tablename, columns, row-range etc.
my_manager [IN] ... open manager needed for vdb-calls
***************************************************************************/
static rc_t vdm_dump_table( const p_dump_context ctx, const VDBManager *my_manager )
{
    rc_t rc;

    /* take ctx->path as table ( if ctx->table is empty ) */
    if ( ctx->table == NULL )
    {
        ctx->table = string_dup_measure ( ctx->path, NULL );
    }
    if ( ctx->schema_dump_requested )
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_dump_tab_schema );
    }
    else if ( ctx->table_enum_requested )
    {
        KOutMsg( "cannot enum tables of a table-object\n" );
        vdm_clear_recorded_errors();
        rc = 0;
    }
    else if ( enum_col_request( ctx ) )
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_enum_tab_columns );
    }
    else if ( ctx->id_range_requested )
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_print_tab_id_range );
    }
    else if ( ctx->idx_enum_requested )
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_enum_tab_index );
    }
    else if ( ctx->idx_range_requested )
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_range_tab_index );
    }
    else if ( ctx->show_spotgroups )
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_show_tab_spotgroups );
    }
    else if ( ctx->show_spread )
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_show_tab_spread );
    }
    else
    {
        rc = vdm_dump_tab_fkt( ctx, my_manager, vdm_dump_opened_table );
    }
    return rc;
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
static rc_t vdm_dump_db_fkt( const p_dump_context ctx,
                             const VDBManager * mgr,
                             const db_fkt_t db_fkt )
{
    const VDatabase *db;
    VSchema *schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &(ctx->schema_list), true /* ctx->force_sra_schema */ );

    rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", ctx->path );
    if ( rc != 0 )
        ErrMsg( "VDBManagerOpenDBRead( '%s' ) -> %R", ctx->path, rc );
    else
    {
        KNamelist *tbl_names;
        rc = VDatabaseListTbl( db, &tbl_names );
        if ( rc != 0 )
            ErrMsg( "VDatabaseListTbl( '%s' ) -> %R", ctx->path, rc );
        else
        {
            if ( ctx->table == NULL )
            {
                /* the user DID NOT not specify a table: by default assume the SEQUENCE-table */
                bool table_found = vdh_take_this_table_from_list( ctx, tbl_names, "SEQUENCE" );
                /* if there is no SEQUENCE-table, just pick the first table available... */
                if ( !table_found )
                    vdh_take_1st_table_from_db( ctx, tbl_names );
            }
            else
            {
                /* the user DID specify a table: check if the database has a table with this name,
                   if not try with a sub-string */
                String value;
                StringInitCString( &value, ctx->table );
                if ( !list_contains_value( tbl_names, &value ) )
                    vdh_take_this_table_from_list( ctx, tbl_names, ctx->table );
            }
            
            if ( ctx->table != NULL || ctx->table_enum_requested )
            {
                rc = db_fkt( ctx, db ); /* fkt-pointer is called */
            }
            else
            {
                LOGMSG( klogInfo, "opened as vdb-database, but no table found" );
                ctx->usage_requested = true;
            }
            rc = KNamelistRelease( tbl_names );
            if ( rc != 0 )
                ErrMsg( "KNamelistRelease() -> %R", rc );
        }
        rc = VDatabaseRelease( db );
        if ( rc != 0 )
            ErrMsg( "VDatabaseRelease() -> %R", rc );
    }

    if ( schema != NULL )
    {
        rc = VSchemaRelease( schema );
        if ( rc != 0 )
            ErrMsg( "VSchemaRelease() -> %R", rc );
    }
    
    return rc;
}


/***************************************************************************
    dump_database:
    * called by "dump_main()"
    * calls the dump_db_fkt with 4 different fkt-pointers as argument
      depending on what was selected at the commandline

ctx        [IN] ... contains path, tablename, columns, row-range etc.
my_manager [IN] ... open manager needed for vdb-calls
***************************************************************************/
static rc_t vdm_dump_database( const p_dump_context ctx, const VDBManager *my_manager )
{
    rc_t rc;

    if ( ctx->schema_dump_requested )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_dump_db_schema );
    }
    else if ( ctx->table_enum_requested )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_enum_tables );
    }
    else if ( enum_col_request( ctx ) )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_enum_db_columns );
    }
    else if ( ctx->id_range_requested )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_print_db_id_range );
    }
    else if ( ctx->idx_enum_requested )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_enum_db_index );
    }
    else if ( ctx->idx_range_requested )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_range_db_index );
    }
    else if ( ctx->show_spotgroups )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_show_db_spotgroups );
    }
    else if ( ctx->show_spread )
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_show_db_spread );
    }
    else
    {
        rc = vdm_dump_db_fkt( ctx, my_manager, vdm_dump_opened_database );
    }

    return rc;
}


static rc_t vdm_print_objver( const p_dump_context ctx, const VDBManager *mgr )
{
    ver_t version;
    rc_t rc = VDBManagerGetObjVersion ( mgr, &version, ctx->path );
    if ( rc != 0 )
        ErrMsg( "VDBManagerGetObjVersion( '%s' ) -> %R", ctx->path, rc );
    else
        rc = KOutMsg( "%V\n", version );
    return rc;
}

static rc_t vdm_print_objts ( const p_dump_context ctx, const VDBManager *mgr )
{
    KTime_t timestamp;
    rc_t rc = VDBManagerGetObjModDate ( mgr, &timestamp, ctx->path );
    if ( rc != 0 )
        ErrMsg( "VDBManagerGetObjModDate( '%s' ) -> %R", ctx->path, rc  );
    if ( rc == 0 )
    {
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

static void vdm_print_objtype( const VDBManager *mgr, const char * acc_or_path )
{
    int path_type = ( VDBManagerPathType ( mgr, "%s", acc_or_path ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type )
    {
    case kptDatabase      :   KOutMsg( "Database\n" ); break;
    case kptTable         :   KOutMsg( "Table\n" ); break;
    case kptPrereleaseTbl :   KOutMsg( "Prerelease Table\n" ); break;
    case kptColumn        :   KOutMsg( "Column\n" ); break;
    case kptIndex         :   KOutMsg( "Index\n" ); break;
    case kptNotFound      :   KOutMsg( "not found\n" ); break;
    case kptBadPath       :   KOutMsg( "bad path\n" ); break;
    case kptFile          :   KOutMsg( "File\n" ); break;
    case kptDir           :   KOutMsg( "Dir\n" ); break;
    case kptCharDev       :   KOutMsg( "CharDev\n" ); break;
    case kptBlockDev      :   KOutMsg( "BlockDev\n" ); break;
    case kptFIFO          :   KOutMsg( "FIFO\n" ); break;
    case kptZombieFile    :   KOutMsg( "ZombieFile\n" ); break;
    case kptDataset       :   KOutMsg( "Dataset\n" ); break;
    case kptDatatype      :   KOutMsg( "Datatype\n" ); break;
    default               :   KOutMsg( "value = '%i'\n", path_type ); break;
    }
}


static rc_t vdb_main_one_obj_by_pathtype( const p_dump_context ctx,
                                          const VDBManager *mgr )
{
    rc_t rc;
    int path_type = ( VDBManagerPathType ( mgr, "%s", ctx->path ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type )
    {
    case kptDatabase    :   rc = vdm_dump_database( ctx, mgr ); break;

    case kptPrereleaseTbl:
    case kptTable       :   rc = vdm_dump_table( ctx, mgr ); break;

    default             :   rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
                            PLOGERR( klogInt, ( klogInt, rc,
                                "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                                "p=%s", ctx->path ) );
                            if ( vdco_schema_count( ctx ) == 0 )
                            {
                                LOGERR( klogInt, rc, "Maybe it is a legacy table. If so, specify a schema with the -S option" );
                            }
                            break;
    }
    return rc;
}


static rc_t vdb_main_one_obj_by_probing( const p_dump_context ctx,
                                         const VDBManager *mgr )
{
    rc_t rc;
    if ( vdh_is_path_database( mgr, ctx->path, &(ctx->schema_list) ) )
    {
        rc = vdm_dump_database( ctx, mgr );
    }
    else if ( vdh_is_path_table( mgr, ctx->path, &(ctx->schema_list) ) )
    {
        rc = vdm_dump_table( ctx, mgr );
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        PLOGERR( klogInt, ( klogInt, rc,
                 "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                 "p=%s", ctx->path ) );
        if ( vdco_schema_count( ctx ) == 0 )
        {
            LOGERR( klogInt, rc, "Maybe it is a legacy table. If so, specify a schema with the -S option" );
        }
    }
    return rc;
}


static rc_t vdm_main_one_obj( const p_dump_context ctx,
                        const VDBManager *mgr,
                        const char * acc_or_path )
{
    rc_t rc = 0;

    ctx->path = string_dup_measure ( acc_or_path, NULL );

    if ( ctx->objver_requested )
    {
        rc = vdm_print_objver( ctx, mgr );
    }
    else if ( ctx->objts_requested )
    {
        rc = vdm_print_objts ( ctx, mgr );
    }
    else if ( ctx->objtype_requested )
    {
        vdm_print_objtype( mgr, acc_or_path );
    }
    else
    {
        if ( USE_PATHTYPE_TO_DETECT_DB_OR_TAB ) /* in vdb-dump-context.h */
        {
            rc = vdb_main_one_obj_by_pathtype( ctx, mgr );
        }
        else
        {
            rc = vdb_main_one_obj_by_probing( ctx, mgr );
        }
    }

    free( (char*)ctx->path );
    ctx->path = NULL;

    return rc;
}


/***************************************************************************
    dump_main:
    * called by "KMain()"
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
static rc_t vdm_main( const p_dump_context ctx, Args * args )
{
    KDirectory *dir;
    rc_t rc1, rc = KDirectoryNativeDir( &dir );
    if ( rc != 0 )
        ErrMsg( "KDirectoryNativeDir() -> %R", rc );
    else
    {
        const VDBManager *mgr;
        rc = VDBManagerMakeRead( &mgr, dir );
        if ( rc != 0 )
            ErrMsg( "VDBManagerMakeRead() -> %R", rc );
        else
        {
            if ( ctx->disable_multithreading )
            {
                rc = VDBManagerDisablePagemapThread( mgr );
                if ( rc != 0 )
                {
                    ErrMsg( "VDBManagerDisablePagemapThread() -> %R", rc );
                    rc = 0;
                }
            }
            
            /* show manager is independend form db or tab */
            if ( ctx->version_requested )
                rc = vdh_show_manager_version( mgr );
            else
            {
                uint32_t count;
                rc = ArgsParamCount( args, &count );
                if ( rc != 0 )
                    ErrMsg( "ArgsParamCount() -> %R", rc );
                else
                {
                    if ( count > 0 )
                    {
                        uint32_t idx;
                        for ( idx = 0; idx < count && rc == 0; ++idx )
                        {
                            const char *value = NULL;
                            rc = ArgsParamValue( args, idx, (const void **)&value );
                            if ( rc != 0 )
                                ErrMsg( "ArgsParamValue() -> %R", rc );
                            else
                            {
                                if ( ctx->print_info )
                                    rc = vdb_info( &(ctx->schema_list), ctx->format, mgr,
                                                   value, ctx->rows );   /* in vdb_info.c */
                                else switch( ctx->format )
                                {
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
                    else
                    {
                        UsageSummary ( UsageDefaultName );
                        rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
                    }
                }
            }
            rc1 = VDBManagerRelease( mgr );
            if ( rc1 != 0 )
                ErrMsg( "VDBManagerRelease() -> %R", rc );
        }
        rc1 = KDirectoryRelease( dir );
        if ( rc != 0 )
            ErrMsg( "KDirectoryRelease() -> %R", rc );
    }
    return rc;
}


static rc_t diff_files( Args * args )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
        ErrMsg( "ArgsParamCount() -> %R", rc );
    else
    {
        if ( count != 2 )
            KOutMsg( "this function needs exactly 2 files to diff\n" );
        else
        {
            const char * f1;
            rc = ArgsParamValue( args, 0, (const void **)&f1 );
            if ( rc != 0 )
                ErrMsg( "ArgsParamValue( 0 ) -> %R", rc );
            else
            {
                const char * f2;
                rc = ArgsParamValue( args, 1, (const void **)&f2 );
                if ( rc != 0 )
                    ErrMsg( "ArgsParamValue( 1 ) -> %R", rc );
                else
                    rc = vds_diff( f1, f2 ); /* in vdb-dump-str.c */
            }
        }
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
        return RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = KOutHandlerSet( write_to_FILE, stdout );
    if ( rc != 0 )
        ErrMsg( "KOutHandlerSet() -> %R", rc );
    else
    {
        rc = ArgsMakeAndHandle( &args, argc, argv,
            1, DumpOptions, sizeof DumpOptions / sizeof DumpOptions [ 0 ] );
        if ( rc != 0 )
            ErrMsg( "ArgsMakeAndHandle() -> %R", rc );
        else
        {
            dump_context *ctx;

            rc = vdco_init( &ctx );
            if ( rc == 0 )
            {
                rc = vdco_capture_arguments_and_options( args, ctx );
                if ( rc == 0 )
                {
                    out_redir redir; /* vdb-dump-redir.h */
                    
                    KLogHandlerSetStdErr();
                    rc = init_out_redir( &redir,
                                     ctx->compress_mode,
                                     ctx->output_file,
                                     ctx->interactive ? 0 : ctx->output_buffer_size ); /* vdb-dump-redir.c */
                    
                    if ( rc == 0 )
                    {
                        if ( ctx->phase > 0 )
                            rc = vdi_bin_phase( ctx, args ); /* vdb-dump-bin.c */
                        else if ( ctx->diff )
                            rc = diff_files( args ); /* above calls into vdb-dump-str.c */
                        else if ( ctx->interactive )
                            rc = vdi_main( ctx, args ); /* vdb-dump-interact.c */
                        else if ( ctx->slice_depth > 0 )
                            rc = find_slice( ctx, args ); /* vdb-dump-str.c */
                        else
                            rc = vdm_main( ctx, args );
                    
                        release_out_redir( &redir ); /* vdb-dump-redir.c */
                    }
                }
                vdco_destroy( ctx );
            }
            ArgsWhack( args );
        }
    }
    return rc;
}

