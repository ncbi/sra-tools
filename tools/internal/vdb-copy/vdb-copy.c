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

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_sra_sraschema_
#include <sra/sraschema.h>
#endif

#ifndef _h_kapp_main_
#include <kapp/main.h>
#endif

#ifndef _h_klib_progressbar_
#include <klib/progressbar.h>
#endif

#ifndef _h_vdb_database_
#include <vdb/database.h>
#endif

#ifndef _h_context_h
#include "context.h"
#endif

#ifndef _h_helper_h
#include "helper.h"
#endif

#ifndef _h_coldefs_h
#include "coldefs.h"
#endif

#ifndef _h_get_platform_h
#include "get_platform.h"
#endif

#ifndef _h_copy_meta_h
#include "copy_meta.h"
#endif

static const char * table_usage[] = { "table-name", NULL };
static const char * rows_usage[] = { "set of rows to be copied(default = all)", NULL };
#if ALLOW_COLUMN_SPEC
static const char * columns_usage[] = { "set of columns to be copied(default = all)", NULL };
#endif
static const char * schema_usage[] = { "schema-name", NULL };
static const char * without_accession_usage[] = { "without accession-test", NULL };
static const char * ignore_reject_usage[] = { "ignore SRA_FILTER_REJECT values", NULL };
static const char * ignore_redact_usage[] = { "ignore SRA_FILTER_REDACTED values", NULL };
#if ALLOW_EXTERNAL_CONFIG
static const char * kfg_path_usage[] = { "use this path to find the file vdb-copy.kfg", NULL };
#endif
static const char * show_matching_usage[] = { "show type-matching results", NULL };
static const char * show_progress_usage[] = { "show progress in percent while copying", NULL };
static const char * ignore_incomp_usage[] = { "ignore incompatible columns", NULL };
static const char * reindex_usage[] = { "reindex columns after copy", NULL };
static const char * show_redact_usage[] = { "show redaction-process", NULL };
static const char * excluded_columns_usage[] = { "exclude these columns from copy", NULL };
static const char * show_meta_usage[] = { "show metadata-copy-process", NULL };
static const char * md5mode_usage[] = { "MD5-mode def.: auto, '1'...forced ON, '0'...forced OFF)", NULL };
static const char * blcmode_usage[] = { "Blob-checksum def.: auto, '1'...CRC32, 'M'...MD5, '0'...OFF)", NULL };
static const char * force_usage[] = { "forces an existing target to be overwritten", NULL };
static const char * unlock_usage[] = { "forces a locked target to be unlocked", NULL };

OptDef MyOptions[] = {
    { OPTION_TABLE, ALIAS_TABLE, NULL, table_usage, 1, true, false },
    { OPTION_ROWS, ALIAS_ROWS, NULL, rows_usage, 1, true, false },
#if ALLOW_COLUMN_SPEC
    { OPTION_COLUMNS, ALIAS_COLUMNS, NULL, columns_usage, 1, true, false },
#endif
    { OPTION_SCHEMA, ALIAS_SCHEMA, NULL, schema_usage, 5, true, false },
    { OPTION_WITHOUT_ACCESSION, ALIAS_WITHOUT_ACCESSION, NULL, without_accession_usage, 1, false, false },
    { OPTION_IGNORE_REJECT, ALIAS_IGNORE_REJECT, NULL, ignore_reject_usage, 1, false, false },
    { OPTION_IGNORE_REDACT, ALIAS_IGNORE_REDACT, NULL, ignore_redact_usage, 1, false, false },
#if ALLOW_EXTERNAL_CONFIG
    { OPTION_KFG_PATH, ALIAS_KFG_PATH, NULL, kfg_path_usage, 1, true, false },
#endif
    { OPTION_SHOW_MATCHING, ALIAS_SHOW_MATCHING, NULL, show_matching_usage, 1, false, false },
    { OPTION_SHOW_PROGRESS, ALIAS_SHOW_PROGRESS, NULL, show_progress_usage, 1, false, false },
    { OPTION_IGNORE_INCOMP, ALIAS_IGNORE_INCOMP, NULL, ignore_incomp_usage, 1, false, false },
    { OPTION_REINDEX, ALIAS_REINDEX, NULL, reindex_usage, 1, false, false },
    { OPTION_SHOW_REDACT, ALIAS_SHOW_REDACT, NULL, show_redact_usage, 1, false, false },
    { OPTION_EXCLUDED_COLUMNS, ALIAS_EXCLUDED_COLUMNS, NULL, excluded_columns_usage, 1, true, false },
    { OPTION_SHOW_META, ALIAS_SHOW_META, NULL, show_meta_usage, 1, false, false },
    { OPTION_MD5_MODE, ALIAS_MD5_MODE, NULL, md5mode_usage, 1, true, false },
    { OPTION_BLOB_CHECKSUM, ALIAS_BLOB_CHECKSUM, NULL, blcmode_usage, 1, true, false },
    { OPTION_FORCE, ALIAS_FORCE, NULL, force_usage, 1, false, false },
    { OPTION_UNLOCK, ALIAS_UNLOCK, NULL, unlock_usage, 1, false, false }
};

const char UsageDefaultName[] = "vdb-copy";

rc_t CC UsageSummary ( const char * progname ) {
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s <src_path> <dst_path> [options]\n"
        "\n", progname );
}

rc_t CC Usage ( const Args * args ) {
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if ( NULL == args ) {
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    } else {
        rc = ArgsProgram ( args, &fullpath, &progname );
    }
    if ( 0 != rc) {
        progname = fullpath = UsageDefaultName;
    }
    UsageSummary( progname );

    KOutMsg ( "Options:\n" );

    HelpOptionLine ( ALIAS_TABLE, OPTION_TABLE, "table", table_usage );
    HelpOptionLine ( ALIAS_ROWS, OPTION_ROWS, "rows", rows_usage );
#if ALLOW_COLUMN_SPEC
    HelpOptionLine ( ALIAS_COLUMNS, OPTION_COLUMNS, "columns", columns_usage );
#endif
    HelpOptionLine ( ALIAS_SCHEMA, OPTION_SCHEMA, "schema", schema_usage );
    HelpOptionLine ( ALIAS_WITHOUT_ACCESSION, OPTION_WITHOUT_ACCESSION, NULL, without_accession_usage );
    HelpOptionLine ( ALIAS_IGNORE_REJECT, OPTION_IGNORE_REJECT, NULL, ignore_reject_usage );
    HelpOptionLine ( ALIAS_IGNORE_REDACT, OPTION_IGNORE_REDACT, NULL, ignore_redact_usage );
#if ALLOW_EXTERNAL_CONFIG
    HelpOptionLine ( ALIAS_KFG_PATH, OPTION_KFG_PATH, NULL, kfg_path_usage );
#endif
    HelpOptionLine ( ALIAS_SHOW_MATCHING, OPTION_SHOW_MATCHING, NULL, show_matching_usage );
    HelpOptionLine ( ALIAS_SHOW_PROGRESS, OPTION_SHOW_PROGRESS, NULL, show_progress_usage );
    HelpOptionLine ( ALIAS_IGNORE_INCOMP, OPTION_IGNORE_INCOMP, NULL, ignore_incomp_usage );
    HelpOptionLine ( ALIAS_REINDEX, OPTION_REINDEX, NULL, reindex_usage );
    HelpOptionLine ( ALIAS_SHOW_REDACT, OPTION_SHOW_REDACT, NULL, show_redact_usage );
    HelpOptionLine ( ALIAS_EXCLUDED_COLUMNS, OPTION_EXCLUDED_COLUMNS, NULL, excluded_columns_usage );
    HelpOptionLine ( ALIAS_SHOW_META, OPTION_SHOW_META, NULL, show_meta_usage );
    HelpOptionLine ( ALIAS_FORCE, OPTION_FORCE, NULL, force_usage );
    HelpOptionLine ( ALIAS_UNLOCK, OPTION_UNLOCK, NULL, unlock_usage );
    HelpOptionLine ( ALIAS_MD5_MODE, OPTION_MD5_MODE, NULL, md5mode_usage );
    HelpOptionLine ( ALIAS_BLOB_CHECKSUM, OPTION_BLOB_CHECKSUM, NULL, blcmode_usage );

    HelpOptionsStandard();

    HelpVersion( fullpath, KAppVersion() );

    return rc;
}

static rc_t vdb_copy_redact_cell( const VCursor * src_cursor, VCursor * dst_cursor,
                                  const p_col_def col, uint64_t row_id,
                                  redact_buffer * rbuf,
                                  const bool show_redact ) {
    const void * src_buffer;
    uint32_t offset_in_bits;
    uint32_t n_elements;
    uint32_t elem_bits;

    /* we read the original cell-data to detect how big the data is before redacting */
    rc_t rc = VCursorCellData( src_cursor, col -> src_idx, &elem_bits,
                          &src_buffer, &offset_in_bits, &n_elements );
    if ( 0 != rc ) {
        PLOGERR( klogInt,
                 (klogInt,
                 rc,
                 "VCursorCellData( col:$(col_name) at row #$(row_nr) ) failed",
                 "col_name=%s,row_nr=%lu",
                  col->name, row_id ));
    } else {
        size_t new_size = ( ( elem_bits * n_elements ) + 8 ) >> 3;
        rc = redact_buf_resize( rbuf, new_size );
        DISP_RC( rc, "vdb_copy_redact_cell:redact_buf_resize() failed" );
        if ( 0 == rc ) {
            if ( col -> r_val != NULL ) {
                if ( show_redact ) {
                    char * c = ( char * )col -> r_val -> value;
                    KOutMsg( "redacting #%lu %s -> 0x%.02x\n", row_id, col -> dst_cast, *c );
                }
                redact_val_fill_buffer( col -> r_val, rbuf, new_size );
            } else {
                if ( show_redact ) {
                    KOutMsg( "redacting #%lu %s -> 0\n", row_id, col -> dst_cast );
                }
                memset( rbuf -> buffer, 0, new_size );
            }

            rc = VCursorWrite( dst_cursor, col -> dst_idx, elem_bits,
                               rbuf -> buffer, 0, n_elements );
            if ( 0 != rc ) {
                PLOGERR( klogInt,
                         ( klogInt,
                         rc,
                         "VCursorWrite( col:$(col_name) at row #$(row_nr) ) failed",
                         "col_name=%s,row_nr=%lu",
                          col -> name, row_id ) );
            }
        }
    }
    return rc;
}

static rc_t vdb_copy_cell( const VCursor * src_cursor, VCursor * dst_cursor,
                           const p_col_def col, uint64_t row_id ) {
    rc_t rc;

    const void *buffer;
    uint32_t offset_in_bits;
    uint32_t number_of_elements;
    uint32_t elem_bits;

    rc = VCursorCellData( src_cursor, col->src_idx, &elem_bits,
                          &buffer, &offset_in_bits, &number_of_elements );
    if ( 0 != rc ) {
        PLOGERR( klogInt,
                 ( klogInt,
                 rc,
                 "VCursorCellData( col:$(col_name) at row #$(row_nr) ) failed",
                 "col_name=%s,row_nr=%lu",
                  col -> name, row_id ) );
        return rc;
    }

    rc = VCursorWrite( dst_cursor, col->dst_idx, elem_bits,
                       buffer, offset_in_bits, number_of_elements );
    if ( 0 != rc ) {
        PLOGERR( klogInt,
                 (klogInt,
                 rc,
                 "VCursorWrite( col:$(col_name) at row #$(row_nr) ) failed",
                 "col_name=%s,row_nr=%lu",
                  col -> name, row_id ));
    }
    return rc;
}

static rc_t vdb_copy_row( const VCursor * src_cursor,
                          VCursor * dst_cursor,
                          col_defs * columns,
                          uint64_t row_id,
                          redact_buffer * rbuf,
                          const bool redact,
                          const bool show_redact ) {
    uint32_t len, idx = 0;
    rc_t rc = VCursorOpenRow( dst_cursor );
    if ( 0 != rc ) {
        PLOGERR( klogInt,
                 (klogInt,
                 rc,
                 "VCursorOpenRow(dst) row #$(row_nr) failed",
                 "row_nr=%lu",
                 row_id ));
        return rc;
    }

    len = VectorLength( &(columns->cols) );
    /* loop through the columns and copy them if they have to be copied */
    while ( idx < len && 0 == rc ) {
        p_col_def col = (p_col_def) VectorGet ( &(columns->cols), idx++ );
        if ( NULL != col ) {
            if ( col -> to_copy ) {
                if ( redact && col->redactable ) {
                    rc = vdb_copy_redact_cell( src_cursor, dst_cursor,
                                               col, row_id, rbuf, show_redact );
                } else {
                    rc = vdb_copy_cell( src_cursor, dst_cursor, col, row_id );
                }
            }
        }
    }
    if ( 0 == rc ) {
        rc = VCursorCommitRow( dst_cursor );
        if ( 0 != rc ) {
            PLOGERR( klogInt,
                     (klogInt,
                     rc,
                     "VCursorCommitRow(dst) row #$(row_nr) failed",
                     "row_nr=%lu",
                     row_id ));
        }

        rc = VCursorCloseRow( dst_cursor );
        if ( 0 != rc ) {
            PLOGERR( klogInt,
                     (klogInt,
                     rc,
                     "VCursorCloseRow(dst) row #$(row_nr) failed",
                     "row_nr=%lu",
                     row_id ));
        }
    }
    return rc;
}

static rc_t vdb_copy_read_row_flags( const p_context ctx,
                                     const VCursor *cursor,
                                     const uint32_t src_idx,
                                     bool *pass,
                                     bool *redact ) {
    uint64_t filter;
    /* read the filter-value from the filter-column */
    rc_t rc = helper_read_vdb_int_row_open( cursor, src_idx, &filter );
    if ( 0 != rc ) return rc;

    switch( filter ) {
    case SRA_READ_FILTER_REJECT   :
        if ( ctx -> ignore_reject == false ) { *pass = false; }
        break;

    case SRA_READ_FILTER_REDACTED :
        if ( ctx -> ignore_redact == false ) { *redact = true; }
        break;
    }
    return rc;
}

static rc_t vdb_copy_row_loop( const p_context ctx,
                               const VCursor * src_cursor,
                               VCursor * dst_cursor,
                               col_defs * columns,
                               redact_vals * rvals ) {
    rc_t rc;
    const struct num_gen_iter * iter;
    int64_t row_id;
    uint64_t count;
    uint32_t percent;
    p_col_def filter_col_def = NULL;
    redact_buffer rbuf;
    struct progressbar * progress = NULL;

    if ( -1 != columns -> filter_idx ) {
        filter_col_def = col_defs_get( columns, columns -> filter_idx );
    }
    rc = num_gen_iterator_make( ctx -> row_generator, &iter );
    if ( 0 != rc ) return rc;

    rc = make_progressbar( &progress, 2 );
    DISP_RC( rc, "vdb_copy_row_loop:make_progressbar() failed" );
    if ( 0 != rc ) return rc;

    redact_buf_init( &rbuf );
    col_defs_find_redact_vals( columns, rvals );

    count = 0;
    while ( 0 == rc && num_gen_iterator_next( iter, &row_id, &rc ) ) {
        if ( 0 != rc ) {
            rc = Quitting();    /* to be able to cancel the loop by signal */
        }
        if ( 0 == rc ) {
            rc = VCursorSetRowId( src_cursor, row_id );
            if ( 0 != rc ) {
                PLOGERR( klogInt, (klogInt, rc,
                         "VCursorSetRowId(src) row #$(row_nr) failed",
                         "row_nr=%lu", row_id ));
            }
            if ( 0 == rc ) {
                rc = VCursorOpenRow( src_cursor );
                if ( 0 != rc ) {
                    PLOGERR( klogInt, (klogInt, rc,
                             "VCursorOpenRow(src) row #$(row_nr) failed",
                             "row_nr=%lu", row_id ));
                } else {
                    bool pass_flag = true;
                    bool redact_flag = false;

                    if ( NULL != filter_col_def ) {
                        vdb_copy_read_row_flags( ctx, src_cursor,
                                    filter_col_def -> src_idx, &pass_flag, &redact_flag );
                    }
                    if ( pass_flag ) {
                        rc = vdb_copy_row( src_cursor, dst_cursor,
                                           columns, row_id,
                                           &rbuf, redact_flag, ctx->show_redact );
                    }
                    if ( 0 == rc ) {
                        count++;
                        rc = VCursorCloseRow( src_cursor );
                        if ( 0 != rc ) {
                            PLOGERR( klogInt, ( klogInt, rc,
                                     "VCursorCloseRow(src) row #$(row_nr) failed",
                                     "row_nr=%lu", row_id ) );
                        }
                    }
                }

                if ( ctx -> show_progress ) {
                    if ( num_gen_iterator_percent( iter, 2, &percent ) == 0 ) {
                        update_progressbar( progress, percent );
                    }
                }
            }
        }
    }

    /* set rc to zero for num_gen_iterator_next() reached last id */
    if ( GetRCModule( rc ) == rcVDB &&
         GetRCTarget( rc ) == rcNoTarg &&
         GetRCContext( rc ) == rcReading &&
         GetRCObject( rc ) == rcId &&
         GetRCState( rc ) == rcInvalid ) {
        rc = 0;
    }

    if ( ctx -> show_progress ) {
        KOutMsg( "\n" );
    }
    destroy_progressbar( progress );

    PLOGMSG( klogInfo, ( klogInfo, "\n $(row_cnt) rows copied", "row_cnt=%lu", count ));

    if ( 0 == rc ) {
        rc = VCursorCommit( dst_cursor );
        if ( 0 != rc ) {
            LOGERR( klogInt, rc, "VCursorCommit( dst ) after processing all rows failed" );
        }
    }
    num_gen_iterator_destroy( iter );
    redact_buf_free( &rbuf );

    return rc;
}

static rc_t vdb_copy_make_dst_table( const p_context ctx,
                                     VDBManager * vdb_mgr,
                                     const VSchema * src_schema,
                                     VSchema **dst_schema,
                                     KCreateMode cmode,
                                     VTable **dst_table,
                                     bool is_legacy ) {
    rc_t rc = 0;
    /* different ways to make the schema for the dest-table */
    if ( is_legacy ) {
        /* load it from a file */
        if ( NULL == *dst_schema ) {
            rc = VDBManagerMakeSRASchema( vdb_mgr, dst_schema );
            if ( 0 != rc ) {
                PLOGERR( klogInt,
                         (klogInt,
                         rc, "VDBManagerMakeSRASchema(dst) failed", "" ));
            }
        }
        rc = VSchemaParseFile ( *dst_schema, "%s", ctx->legacy_schema_file );
        if ( 0 != rc ) {
            PLOGERR( klogInt,
                     (klogInt,
                     rc, "VSchemaParseFile() failed", "" ));
        }
    } else {
        /* in case of a non-legacy-table, do nothing,
           keep using the src-schema-object */
        *dst_schema = (VSchema *)src_schema;
        VSchemaAddRef( src_schema );
    }

    if ( 0 == rc ) {
        rc = VDBManagerCreateTable( vdb_mgr, dst_table,
                                    *dst_schema, ctx -> dst_schema_tabname,
                                    cmode, "%s", ctx -> dst_path );
        DISP_RC( rc, "vdb_copy_make_dst_table:VDBManagerCreateTable() failed" );
        if ( 0 == rc ) {
            KChecksum cs_mode = helper_assemble_ChecksumMode( ctx -> blob_checksum );
            rc = VTableColumnCreateParams ( *dst_table, cmode, cs_mode, 0 );
            DISP_RC( rc, "vdb_copy_make_dst_table:VTableColumnCreateParams() failed" );
        }
    }
    return rc;
}

static rc_t vdb_copy_open_dest_table( const p_context ctx,
                                      const VTable * src_table,
                                      VTable * dst_table,
                                      VCursor ** dst_cursor,
                                      col_defs * columns,
                                      bool is_legacy ) {
    /* copy the metadata */
    rc_t rc = copy_table_meta( src_table, dst_table,
                          ctx -> config.meta_ignore_nodes,
                          ctx -> show_meta, is_legacy );
    if ( 0 != rc ) return rc;

    /* mark all columns which are to be found writable as to_copy */
    rc = col_defs_mark_writable_columns( columns, dst_table, false );
    DISP_RC( rc, "vdb_copy_open_dest_table:col_defs_mark_writable_columns() failed" );
    if ( 0 != rc ) return rc;

    /* make a writable cursor */
    rc = VTableCreateCursorWrite( dst_table, dst_cursor, kcmInsert );
    DISP_RC( rc, "vdb_copy_open_dest_table:VTableCreateCursorWrite(dst) failed" );
    if ( 0 != rc ) return rc;

    /* add all marked ( as to copy ) columns to the writable cursor */
    rc = col_defs_add_to_wr_cursor( columns, *dst_cursor, false );
    DISP_RC( rc, "vdb_copy_open_dest_table:col_defs_add_to_wr_cursor(dst) failed" );
    if ( 0 != rc ) return rc;

    /* opens the dst cursor */
    rc = VCursorOpen( *dst_cursor );
    DISP_RC( rc, "vdb_copy_open_dest_table:VCursorOpen(dst) failed" );

    return rc;
}

/* detect the row-range of the source-table
   check if the requested row-range is within this range
   otherwise correct the requested row-range
*/
static rc_t vdb_copy_check_range( const p_context ctx,
                                  const VCursor * a_cursor ) {
    int64_t  first;
    uint64_t count;
    rc_t rc = VCursorIdRange( a_cursor, 0, &first, &count );
    DISP_RC( rc, "vdb_copy_check_range:VCursorIdRange() failed" );
    if ( 0 == rc ) {
        rc = context_range_check( ctx, first, count );
        DISP_RC( rc, "vdb_copy_check_range:context_range_check() failed" );
    }
    return rc;
}

/* detect the row-range of the source-table
   set's the range of the number-generator to the discovered values
*/
static rc_t vdb_copy_set_range( const p_context ctx,
                                const VCursor * a_cursor ) {
    int64_t  first;
    uint64_t count;
    rc_t rc = VCursorIdRange( a_cursor, 0, &first, &count );
    DISP_RC( rc, "vdb_copy_set_range:VCursorIdRange() failed" );
    if ( 0 == rc ) {
        rc = context_set_range( ctx, first, count );
        DISP_RC( rc, "vdb_copy_set_range:context_set_range() failed" );
    }
    return rc;
}

static rc_t vdb_copy_prepare_legacy_tab( const p_context ctx,
                                         const char * src_schema_tabname ) {
    KConfig * config_mgr;
    char * src_platform;

    rc_t rc = get_table_platform( ctx -> src_path, &src_platform, '_' );
    DISP_RC( rc, "vdb_copy_prepare_legacy_tab:get_table_platform() failed" );
    if ( 0 != rc ) return rc;

    rc = helper_make_config_mgr( &config_mgr, ctx -> kfg_path );
    DISP_RC( rc, "vdb_copy_prepare_legacy_tab:helper_make_config_mgr() failed" );
    if ( 0 != rc ) return rc;

    PLOGMSG( klogInfo, ( klogInfo, "legacy-platform: $(platform)", "platform=%s",
                                   src_platform ));

    rc = helper_get_legacy_write_schema_from_config( config_mgr,
                src_platform,
                src_schema_tabname,
                &( ctx -> legacy_schema_file ),
                &( ctx -> dst_schema_tabname ),
                &( ctx -> legacy_dont_copy ) );
    DISP_RC( rc, "vdb_copy_prepare_legacy_tab:helper_get_legacy_write_schema_from_config() failed" );
    {
        rc_t rc1 = KConfigRelease( config_mgr );
        DISP_RC( rc1, "vdb_copy_prepare_legacy_tab:KConfigRelease() failed" );
    }
    if ( 0 != rc ) return rc;

    if ( ctx -> show_matching ) {
        PLOGMSG( klogInfo, ( klogInfo, "-file: $(file)", "file=%s", ctx -> legacy_schema_file ));
        PLOGMSG( klogInfo, ( klogInfo, "-tab : $(tab)",  "tab=%s",  ctx -> dst_schema_tabname ));
        PLOGMSG( klogInfo, ( klogInfo, "-dont: $(dont)", "dont=%s", ctx -> legacy_dont_copy ));
    }
    return rc;
}

static rc_t vdb_copy_find_out_what_columns_to_use( const VTable * src_table,
                                                   const char * tablename,
                                                   col_defs * columns,
                                                   const char * requested,
                                                   const char * excluded ) {
    bool cols_requested = ( ( requested != NULL ) &&
                            ( nlt_strcmp( requested, "*" ) != 0 ) );
    /* no matter if specific columns are requested, we first discover all of them
       to later mark the columns which are requested */
    rc_t rc = col_defs_extract_from_table( columns, src_table );
    DISP_RC( rc, "vdb_copy_find_out_what_columns_to_use:col_defs_extract_from_table() failed" );
    if ( 0 == rc ) {
        if ( cols_requested ) {
            /* the user requested a specific subset of columns... */
            rc = col_defs_mark_requested_columns( columns, requested );
        } else {
            /* no specific subset of columns is requested --> copy what we can... */
            rc = col_defs_mark_requested_columns( columns, NULL );
        }
        DISP_RC( rc, "vdb_copy_find_out_what_columns_to_use:col_defs_mark_requested_columns() failed" );
        if ( 0 == rc && excluded != NULL ) {
            rc = col_defs_exclude_these_columns( columns, tablename, excluded );
            DISP_RC( rc, "vdb_copy_find_out_what_columns_to_use:col_defs_unmark_writable_columns() failed" );
        }
    }
    return rc;
}

/* find the filter-column and redactable columns */
static void vdb_copy_find_filter_and_redact_columns( const VSchema * src_schema,
                                                     col_defs * columns,
                                                     config_values * config,
                                                     matcher * type_matcher ) {
    /* it is ok to not find a filter-column: no error in this case */
    col_defs_detect_filter_col( columns, config->filter_col_name );

    /* it is ok to not find redactable types: no error in this case */
    col_defs_detect_redactable_cols_by_type( columns,
             src_schema, type_matcher, config->redactable_types );

    /* it is ok to not find columns excluded from redacting: no error in this case */
    col_defs_unmark_do_not_redact_columns( columns,
                    config->do_not_redact_columns );
}

static rc_t vdb_copy_detect_legacy( const p_context ctx,
                                    VDBManager * vdb_mgr,
                                    const VTable * src_table,
                                    bool * is_legacy ) {
    char * src_schema_tabname;
    /* detects the name of the schema-table used by the source-table */
    rc_t rc = helper_get_schema_tab_name( src_table, &src_schema_tabname );
    DISP_RC( rc, "vdb_copy_prepare_columns:helper_get_schema_tab_name() failed" );
    if ( 0 == rc ) {
        rc = helper_is_tablename_legacy( vdb_mgr, src_schema_tabname, is_legacy );
        if ( 0 == rc ) {
            if ( *is_legacy ) {
                PLOGMSG( klogInfo, ( klogInfo, "used legacy schema: $(schema)", "schema=%s",
                                     src_schema_tabname ));
                rc = vdb_copy_prepare_legacy_tab( ctx, src_schema_tabname );
            } else {
                PLOGMSG( klogInfo, ( klogInfo, "used schema: $(schema)", "schema=%s",
                                     src_schema_tabname ));
                ctx -> dst_schema_tabname = string_dup_measure ( src_schema_tabname, NULL );
            }
        }
        free( src_schema_tabname );
    }
    return rc;
}

/* we have the src-columns collected (incl. there types):
   no columns have been added to a cursor (rd and wr),
   now we can find the columns which have to be copied
   including the right type-casts for every one of these columns */
static rc_t vdb_copy_match_columns( const p_context ctx,
                                    VDBManager * vdb_mgr,
                                    col_defs * columns,
                                    matcher * type_matcher ) {
    matcher_input mi;
    rc_t rc = KDirectoryNativeDir( &( mi.dir ) );
    DISP_RC( rc, "vdb_copy_match_columns:KDirectoryNativeDir() failed" );
    if ( 0 == rc ) {
        rc = helper_make_config_mgr( &(mi.cfg), ctx->kfg_path );
        DISP_RC( rc, "vdb_copy_match_columns:helper_make_config_mgr() failed" );
        if ( 0 == rc ) {
            mi . manager = vdb_mgr;
            mi . add_schemas = ctx -> src_schema_list;

            rc = col_defs_as_string( columns, (char**)&mi.columns, true );
            if ( 0 == rc ) {
                mi . src_path         = ctx -> src_path;
                mi . dst_path         = ctx -> dst_path;
                mi . legacy_schema    = ctx -> legacy_schema_file;
                mi . dst_tabname      = ctx -> dst_schema_tabname;
                mi . excluded_columns = ctx -> legacy_dont_copy;
                mi . force_kcmInit    = ctx -> force_kcmInit;
                mi . force_unlock     = ctx -> force_unlock;

                rc = matcher_execute( type_matcher, &mi );
                if ( 0 != rc ) {
                    if ( GetRCState( rc ) == rcExists ) {
                        LOGMSG( klogInfo, "vdb-copy cannot create the target-table, because it already exists!" );
                        LOGMSG( klogInfo, "try to use the force-option (-f)" );
                        ctx -> dont_remove_target = true;
                    } else {
                        DISP_RC( rc, "vdb_copy_match_columns:matcher_execute() failed" );
                    }
                } else {
                    if ( ctx -> show_matching ) {
                        matcher_report( type_matcher, true );
                    }
                    rc = col_defs_apply_casts( columns, type_matcher );
                    DISP_RC( rc, "vdb_copy_match_columns:col_defs_apply_casts() failed" );
                }
                free( ( void * )mi.columns );
            }
            {
                rc_t rc1 = KConfigRelease( mi.cfg );
                DISP_RC( rc1, "vdb_copy_match_columns:KConfigRelease() failed" );
            }
        }
        {
            rc_t rc1 = KDirectoryRelease( mi.dir );
            DISP_RC( rc1, "vdb_copy_match_columns:KDirectoryRelease() failed" );
        }
    }
    return rc;
}

/* this is the common entry-point for the copy-operation in both cases
   "given path is database" OR "given path is table" */
static rc_t vdb_copy_open_source_table( const p_context ctx,
                                        VDBManager * vdb_mgr,
                                        const VSchema * src_schema,
                                        VSchema **dst_schema,
                                        const VTable * src_table,
                                        const VCursor * src_cursor,
                                        KCreateMode cmode,
                                        VTable **dst_table,
                                        col_defs * columns,
                                        bool * is_legacy,
                                        matcher * type_matcher ) {
    /* everything what has to be done before adding the columns
       to the source-cursor and opening the source-cursor */
    rc_t rc = vdb_copy_detect_legacy( ctx, vdb_mgr, src_table, is_legacy );
    if ( 0 != rc ) return rc;

    /* type-match between src <---> dst columns, find best typecast */
    rc = vdb_copy_match_columns( ctx, vdb_mgr, columns, type_matcher );
    if ( 0 != rc ) return rc;

    /* in case of legacy table make new schema - parse matched schema
       in case on non-legacy use the src-schema as dst-schema */
    rc = vdb_copy_make_dst_table( ctx, vdb_mgr, src_schema, dst_schema, cmode,
                                  dst_table, *is_legacy );
    if ( 0 != rc ) return rc;

    /* the prepared and eventually shortened src-column-list is used */
    rc = col_defs_add_to_rd_cursor( columns, src_cursor, false );
    if ( 0 != rc ) return rc;

    rc = VCursorOpen( src_cursor );
    DISP_RC( rc, "vdb_copy_open_source_table:VCursorOpen() failed" );
    if ( 0 != rc ) return rc;

    /* range check requires an open cursor */
    return vdb_copy_check_range( ctx, src_cursor );
}

static rc_t vdb_copy_table2( const p_context ctx,
                             VDBManager * vdb_mgr,
                             const VTable * src_table,
                             const VCursor * src_cursor,
                             const VSchema * src_schema,
                             col_defs * columns,
                             matcher * type_matcher ) {
    VSchema * dst_schema = NULL;
    VTable * dst_table;
    bool is_legacy;

    KCreateMode cmode = helper_assemble_CreateMode( src_table,
                              ctx->force_kcmInit, ctx->md5_mode );
    rc_t rc = vdb_copy_open_source_table( ctx, vdb_mgr, src_schema, &dst_schema,
                                     src_table, src_cursor, cmode, &dst_table, columns,
                                     &is_legacy, type_matcher );
    if ( 0 == rc ) {
        VCursor * dst_cursor;
        rc = vdb_copy_open_dest_table( ctx, src_table, dst_table, &dst_cursor, columns,
                                       is_legacy );
        if ( 0 == rc ) {
            /* this function does not fail, because it is ok to not find
               filter-column, redactable types and excluded columns */
            vdb_copy_find_filter_and_redact_columns( src_schema,
                                   columns, &(ctx->config), type_matcher );

            rc = vdb_copy_row_loop( ctx, src_cursor, dst_cursor,
                                    columns, ctx->rvals );

            {
                rc_t rc1 = VCursorRelease( dst_cursor );
                DISP_RC( rc1, "vdb_copy_table2:VCursorRelease() failed" );
            }
            if ( 0 == rc && ctx -> reindex ) {
                /* releasing the cursor is necessary for reindex */
                rc = VTableReindex( dst_table );
                DISP_RC( rc, "vdb_copy_table2:VTableReindex() failed" );
            }
        }
        {
            rc_t rc1 = VSchemaRelease( dst_schema );
            DISP_RC( rc1, "vdb_copy_table2:VSchemaRelease() failed" );
            rc1 = VTableRelease( dst_table );
            DISP_RC( rc1, "vdb_copy_table2:VTableRelease( dst ) failed" );
        }
    }
    return rc;
}

static rc_t perform_vdb_copy_on_table( const p_context ctx,
                            VDBManager * vdb_mgr,
                            const VTable * src_table,
                            const char * tablename ) {
    const VSchema * src_schema;
    rc_t rc = VTableOpenSchema ( src_table, &src_schema );
    DISP_RC( rc, "vdb_copy_main:VTableOpenSchema( src_schema ) failed" );
    if ( 0 == rc ) {
        col_defs * columns;
        rc = col_defs_init( &columns );
        DISP_RC( rc, "vdb_copy_table:col_defs_init() failed" );
        if ( 0 == rc ) {
            rc = vdb_copy_find_out_what_columns_to_use( src_table, tablename, columns,
                                                        ctx->columns, ctx->excluded_columns );
            if ( 0 == rc ) {
                const VCursor * src_cursor;
                rc = VTableCreateCursorRead( src_table, &src_cursor );
                DISP_RC( rc, "vdb_copy_table:VTableCreateCursorRead() failed" );
                if ( 0 == rc ) {
                    matcher * type_matcher;
                    rc = matcher_init( &type_matcher );
                    DISP_RC( rc, "vdb_copy_table:matcher_init() failed" );
                    if ( 0 == rc ) {
                        /*****************************************************/
                        rc = vdb_copy_table2( ctx, vdb_mgr, src_table, src_cursor,
                                              src_schema, columns, type_matcher );
                        /*****************************************************/
                        matcher_destroy( type_matcher );
                    }
                }
                {
                    rc_t rc1 = VCursorRelease( src_cursor );
                    DISP_RC( rc1, "vdb_copy_table:VCursorRelease() failed" );
                }
            }
            col_defs_destroy( columns );
        }
        {
            rc_t rc1 = VSchemaRelease( src_schema );
            DISP_RC( rc1, "vdb_copy_table:VSchemaRelease() failed" );
        }
    }
    return rc;
}

static rc_t vdb_copy_cur_2_cur( const p_context ctx,
                                const VCursor * src_cursor,
                                VCursor * dst_cursor,
                                const VSchema * schema,
                                col_defs * columns,
                                matcher * type_matcher,
                                const char * tab_name ) {
    rc_t rc = col_defs_apply_casts( columns, type_matcher );
    DISP_RC( rc, "vdb_copy_cur_2_cur:col_defs_apply_casts() failed" );
    if ( 0 == rc ) {
        rc = col_defs_add_to_wr_cursor( columns, dst_cursor, false );
        DISP_RC( rc, "vdb_copy_cur_2_cur:col_defs_add_to_wr_cursor(dst) failed" );
        if ( 0 == rc ) {
            rc = VCursorOpen( dst_cursor );
            DISP_RC( rc, "vdb_copy_cur_2_cur:VCursorOpen(dst) failed" );
            if ( 0 == rc ) {
                rc = col_defs_add_to_rd_cursor( columns, src_cursor, false );
                DISP_RC( rc, "vdb_copy_cur_2_cur:col_defs_add_to_rd_cursor() failed" );
                if ( 0 == rc ) {
                    rc = VCursorOpen( src_cursor );
                    DISP_RC( rc, "vdb_copy_cur_2_cur:VCursorOpen(src) failed" );
                    if ( 0 == rc ) {
                        /* set the row-range in ctx to cover the whole table */
                        rc = vdb_copy_set_range( ctx, src_cursor );
                        DISP_RC( rc, "vdb_copy_cur_2_cur:vdb_copy_check_range(src) failed" );
                        if ( 0 == rc ) {
                            /* it is ok to not find a filter-column: no error in this case */
                            col_defs_detect_filter_col( columns,
                                                        ctx->config.filter_col_name );

                            /* it is ok to not find columns excluded from redacting: no error in this case */
                            col_defs_unmark_do_not_redact_columns( columns,
                                            ctx->config.do_not_redact_columns );

                            if ( ctx -> show_progress ) {
                                KOutMsg( "copy of >%s<\n", tab_name );
                            }
                            vdb_copy_find_filter_and_redact_columns( schema,
                                                   columns, &(ctx->config), type_matcher );

                            /**************************************************/
                            rc = vdb_copy_row_loop( ctx, src_cursor, dst_cursor,
                                                    columns, ctx->rvals );
                            /**************************************************/
                        }
                    }
                }
            }
        }
    }
    return rc;
}

static rc_t vdb_copy_tab_2_tab( const p_context ctx,
                                const VTable * src_tab,
                                VTable * dst_tab,
                                const char * tab_name ) {
    const VSchema * schema;
    rc_t rc = VTableOpenSchema ( src_tab, &schema );
    DISP_RC( rc, "vdb_copy_tab_2_tab:VTableOpenSchema( src_schema ) failed" );
    if ( 0 == rc ) {
        col_defs * columns;
        rc = col_defs_init( &columns );
        DISP_RC( rc, "vdb_copy_tab_2_tab:col_defs_init() failed" );
        if ( 0 == rc ) {
            rc = vdb_copy_find_out_what_columns_to_use( src_tab, tab_name, columns,
                                                        NULL, ctx->excluded_columns );
            if ( 0 == rc ) {
                matcher * type_matcher;
                rc = matcher_init( &type_matcher );
                if ( 0 == rc ) {
                    char * column_names;
                    rc = col_defs_as_string( columns, (char**)&column_names, true );
                    if ( 0 == rc ) {
                        rc = matcher_db_execute( type_matcher, src_tab, dst_tab, schema,
                                                 column_names, ctx->kfg_path );
                        if ( 0 == rc ) {
                            const VCursor * src_cursor;
                            if ( ctx->show_matching ) {
                                matcher_report( type_matcher, false );
                            }
                            rc = VTableCreateCursorRead( src_tab, &src_cursor );
                            DISP_RC( rc, "vdb_copy_tab_2_tab:VTableCreateCursorRead(src) failed" );
                            if ( 0 == rc ) {
                                rc = col_defs_mark_writable_columns( columns, dst_tab, false );
                                DISP_RC( rc, "vdb_copy_tab_2_tab:col_defs_mark_writable_columns() failed" );
                                if ( 0 == rc ) {
                                    VCursor * dst_cursor;
                                    rc = VTableCreateCursorWrite( dst_tab, &dst_cursor, kcmInsert );
                                    DISP_RC( rc, "vdb_copy_tab_2_tab:VTableCreateCursorWrite(dst) failed" );
                                    if ( 0 == rc ) {
                                        /*****************************************************/
                                        rc = vdb_copy_cur_2_cur( ctx, src_cursor, dst_cursor,
                                                                 schema, columns, type_matcher,
                                                                 tab_name );
                                        /*****************************************************/
                                    }
                                    {
                                        rc_t rc1 = VCursorRelease( dst_cursor );
                                        DISP_RC( rc1, "vdb_copy_tab_2_tab:VCursorRelease(dst) failed" );
                                    }
                                }
                                {
                                    rc_t rc1 = VCursorRelease( src_cursor );
                                    DISP_RC( rc1, "vdb_copy_tab_2_tab:VCursorRelease(src) failed" );
                                }
                            }
                        }
                        free( column_names );
                    }
                    matcher_destroy( type_matcher );
                }
            }
            col_defs_destroy( columns );
        }
        {
            rc_t rc1 = VSchemaRelease( schema );
            DISP_RC( rc1, "vdb_copy_tab_2_tab:VSchemaRelease() failed" );
        }
    }
    return rc;
}

static rc_t vdb_copy_db_tab( const p_context ctx,
                             const VDatabase * src_db,
                             VDatabase * dst_db,
                             const char *tab_name ) {
    const VTable * src_tab;
    rc_t rc = VDatabaseOpenTableRead( src_db, &src_tab, "%s", tab_name );
    DISP_RC( rc, "vdb_copy_db_tab:VDatabaseOpenTableRead(src) failed" );
    if ( 0 == rc ) {
        VTable * dst_tab;
        KCreateMode cmode = helper_assemble_CreateMode( src_tab,
                            ctx->force_kcmInit, ctx->md5_mode );

        rc = VDatabaseCreateTable ( dst_db, &dst_tab, tab_name,
                                    cmode, "%s", tab_name );
        DISP_RC( rc, "vdb_copy_db_tab:VDatabaseCreateTable(dst) failed" );
        if ( 0 == rc ) {
            KChecksum cs_mode = helper_assemble_ChecksumMode( ctx->blob_checksum );
            rc = VTableColumnCreateParams ( dst_tab, cmode, cs_mode, 0 );
            DISP_RC( rc, "vdb_copy_db_tab:VTableColumnCreateParams failed" );
            if ( 0 == rc ) {
                rc = copy_table_meta( src_tab, dst_tab,
                                      ctx->config.meta_ignore_nodes,
                                      ctx->show_meta, false );
                DISP_RC( rc, "vdb_copy_db_tab:copy_table_meta failed" );
                if ( 0 == rc ) {
                    /********************************************************/
                    rc = vdb_copy_tab_2_tab( ctx, src_tab, dst_tab, tab_name );
                    /********************************************************/
                }
            }
            {
                rc_t rc1 = VTableRelease( dst_tab );
                DISP_RC( rc1, "vdb_copy_db_tab:VTableRelease( dst ) failed" );
            }
        }
        {
            rc_t rc1 = VTableRelease( src_tab );
            DISP_RC( rc1, "vdb_copy_db_tab:VTableRelease( src ) failed" );
        }
    }
    return rc;
}

static rc_t vdb_copy_db_sub_tables( const p_context ctx,
                                    const VDatabase * src_db,
                                    VDatabase * dst_db ) {
    KNamelist *names;
    rc_t rc = VDatabaseListTbl( src_db, &names );
    if ( 0 == rc ) {
        uint32_t idx, count;
        rc = KNamelistCount( names, &count );
        DISP_RC( rc, "vdb_copy_db_sub_tables:KNamelistCount failed" );
        for ( idx = 0; idx < count && 0 == rc; ++idx )
        {
            const char *a_name;
            rc = KNamelistGet( names, idx, &a_name );
            DISP_RC( rc, "vdb_copy_db_sub_tables:KNamelistGet() failed" );
            if ( 0 == rc ) {
                /**************************************************/
                rc = vdb_copy_db_tab( ctx, src_db, dst_db, a_name );
                /**************************************************/
            }
        }
        {
            rc_t rc1 = KNamelistRelease( names );
            DISP_RC( rc1, "vdb_copy_db_sub_tables:KNamelistRelease() failed" );

        }
    } else {
        /* VDatabaseListTbl() did not return any tables, this is OK */
        rc = 0;
    }
    return rc;
}

/* forward decl. to make recursive copy of sub-databases possible */
static rc_t vdb_copy_db_2_db( const p_context ctx,
                              const VDatabase * src_db,
                              VDatabase * dst_db );

static rc_t vdb_copy_sub_dbs( const p_context ctx,
                              const VDatabase * src_db,
                              VDatabase * dst_db ) {
    KNamelist *names;
    rc_t rc = VDatabaseListDB( src_db, &names );
    if ( 0 == rc ) {
        uint32_t idx, count;
        rc = KNamelistCount( names, &count );
        DISP_RC( rc, "vdb_copy_sub_dbs:KNamelistCount failed" );
        if ( 0 == rc ) {
            for ( idx = 0; idx < count && 0 == rc; ++idx ) {
                const char *a_name;
                rc = KNamelistGet( names, idx, &a_name );
                DISP_RC( rc, "vdb_copy_sub_dbs:KNamelistGet() failed" );
                if ( 0 == rc ) {
                    const VDatabase * src_sub_db;
                    /* try to open the sub-database */
                    rc = VDatabaseOpenDBRead ( src_db, &src_sub_db, "%s", a_name );
                    DISP_RC( rc, "vdb_copy_sub_dbs:VDatabaseOpenDBRead() failed" );
                    if ( 0 == rc ) {
                        char typespec[ TYPESPEC_BUF_LEN ];
                        rc = VDatabaseTypespec ( src_sub_db, typespec, sizeof typespec );
                        DISP_RC( rc, "vdb_copy_sub_dbs:VDatabaseTypespec( src ) failed" );
                        if ( 0 == rc ) {
                            VDatabase * dst_sub_db;
                            KCreateMode cmode = kcmInit | kcmParents;
                            if ( ctx->md5_mode == MD5_MODE_ON ) {
                                cmode |= kcmMD5;
                            }
                            rc = VDatabaseCreateDB ( dst_db, &dst_sub_db,
                                                     typespec, cmode, "%s", a_name );
                            DISP_RC( rc, "vdb_copy_sub_dbs:VDBManagerCreateDB( dst ) failed" );
                            if ( 0 == rc ) {
                                KChecksum cs_mode = helper_assemble_ChecksumMode( ctx->blob_checksum );
                                rc = VDatabaseColumnCreateParams ( dst_db, cmode, cs_mode, 0 );
                                DISP_RC( rc, "vdb_copy_sub_dbs:VDatabaseColumnCreateParams failed" );
                                if ( 0 == rc ) {
                                    /**************************************************/
                                    rc = vdb_copy_db_2_db( ctx, src_sub_db, dst_sub_db );
                                    /**************************************************/
                                }
                                {
                                    rc_t rc1 = VDatabaseRelease( dst_sub_db );
                                    DISP_RC( rc1, "vdb_copy_sub_dbs:VDatabaseRelease( dst ) failed" );
                                }
                            }
                        }
                        {
                            rc_t rc1 = VDatabaseRelease( src_sub_db );
                            DISP_RC( rc1, "vdb_copy_sub_dbs:VDatabaseRelease( dst ) failed" );
                        }
                    }
                }
            }
        }
        {
            rc_t rc1 = KNamelistRelease( names );
            DISP_RC( rc1, "vdb_copy_sub_dbs:KNamelistRelease() failed" );
        }
    } else {
        /* VDatabaseListDB() did not return any sub-databases - this is OK */
        rc = 0;
    }

    return rc;
}

static rc_t vdb_copy_db_2_db( const p_context ctx,
                              const VDatabase * src_db,
                              VDatabase * dst_db ) {
    rc_t rc = copy_database_meta ( src_db, dst_db, NULL, ctx->show_meta );
    if ( 0 == rc ) {
        if ( NULL == ctx -> table ) {
            /* the user did not specify a particular table: copy all of them */
            /*************************************************/
            rc = vdb_copy_db_sub_tables( ctx, src_db, dst_db );
            /*************************************************/
            if ( 0 == rc ) {
                /* if the database has sub-databases: copy them too */
                rc = vdb_copy_sub_dbs( ctx, src_db, dst_db );
            }
        } else {
            /* copy only this table... */
            rc = vdb_copy_db_tab( ctx, src_db, dst_db, ctx->table );
        }
    }
    return rc;
}

static rc_t perform_vdb_copy_on_database( const p_context ctx,
                               VDBManager * vdb_mgr,
                               const VDatabase * src_db ) {
    char typespec[ TYPESPEC_BUF_LEN ];
    rc_t rc = VDatabaseTypespec ( src_db, typespec, sizeof typespec );
    DISP_RC( rc, "vdb_copy_database:VDatabaseTypespec( src ) failed" );
    if ( 0 == rc ) {
        const VSchema * schema;
        rc = VDatabaseOpenSchema ( src_db, &schema );
        DISP_RC( rc, "vdb_copy_database:VDatabaseOpenSchema( src ) failed" );
        if ( 0 == rc ) {
            VDatabase * dst_db;
            KCreateMode cmode = kcmInit | kcmParents;
            if ( ctx->md5_mode == MD5_MODE_ON ) {
                cmode |= kcmMD5;
            }
            rc = VDBManagerCreateDB ( vdb_mgr, &dst_db, schema,
                                      typespec, cmode, "%s", ctx->dst_path );
            DISP_RC( rc, "vdb_copy_database:VDBManagerCreateDB( dst ) failed" );
            if ( 0 == rc ) {
                KChecksum cs_mode = helper_assemble_ChecksumMode( ctx->blob_checksum );
                rc = VDatabaseColumnCreateParams ( dst_db, cmode, cs_mode, 0 );
                DISP_RC( rc, "vdb_copy_sub_dbs:VDatabaseColumnCreateParams failed" );
                if ( 0 == rc )
                {
                    /*******************************************/
                    rc = vdb_copy_db_2_db( ctx, src_db, dst_db );
                    /*******************************************/
                }
                {
                    rc_t rc1 = VDatabaseRelease( dst_db );
                    DISP_RC( rc1, "vdb_copy_database:VDatabaseRelease( dst ) failed" );
                }
            }
            {
                rc_t rc1 = VSchemaRelease( schema );
                DISP_RC( rc1, "vdb_copy_database:VSchemaRelease() failed" );
            }
        }
    }
    return rc;
}

/***************************************************************************
    vdb_copy_perform:
    * called by "vdb_copy_main()"
    * makes the "default-schema"
    * tests if the given source path is a database/table/none-of-that

ctx        [IN] ... contains path, tablename, columns, row-range etc.
vdb_mgr    [IN] ... contains the vdb-manger to create src/dst-objects
***************************************************************************/
static rc_t perform_vdb_copy_tab_or_db( const p_context ctx, VDBManager * vdb_mgr ) {
    VSchema * dflt_schema;
    rc_t rc = helper_parse_schema( vdb_mgr, &dflt_schema, ctx->src_schema_list );
    DISP_RC( rc, "vdb_copy_main:helper_parse_schema(dflt) failed" );
    if ( 0 == rc ) {
        const VDatabase * src_db;
        /* try to open it as a database */
        rc = VDBManagerOpenDBRead ( vdb_mgr, &src_db, dflt_schema, "%s", ctx -> src_path );
        if ( 0 == rc ) {
            /* if it succeeds it is a database, continue to copy it */
            if ( 1 == DB_COPY_ENABLED ) {
                /*********************************************/
                rc = perform_vdb_copy_on_database( ctx, vdb_mgr, src_db );
                /*********************************************/
                DISP_RC( rc, "vdb_copy_perform:vdb_copy_database() failed" );
            } else {
                LOGMSG( klogInfo, "a copy of a database is not implemented yet..." );
            }
            {
                rc_t rc1 = VDatabaseRelease( src_db );
                DISP_RC( rc1, "vdb_copy_perform:VDatabaseRelease() failed" );
            }
        } else {
            const VTable * src_table;
            /* try to open it as a table */
            rc = VDBManagerOpenTableRead( vdb_mgr, &src_table,
                                          dflt_schema, "%s", ctx -> src_path );
            /* if it succeeds it is a table, continue to copy it */
            if ( 0 == rc ) {
                /*********************************************/
                rc = perform_vdb_copy_on_table( ctx, vdb_mgr, src_table, NULL );
                /*********************************************/
                DISP_RC( rc, "vdb_copy_perform:vdb_copy_table() failed" );
                {
                    rc_t rc1 = VTableRelease( src_table );
                    DISP_RC( rc1, "vdb_copy_perform:VTableRelease() failed" );
                }
            } else {
                rc = RC( rcVDB, rcNoTarg, rcCopying, rcItem, rcNotFound );
                PLOGERR( klogInt, ( klogInt, rc,
                         "\nthe path '$(path)' cannot be opened as vdb-database or vdb-table",
                         "path=%s", ctx->src_path ));
            }
        }
        {
            rc_t rc1 = VSchemaRelease( dflt_schema );
            DISP_RC( rc1, "vdb_copy_perform:VSchemaRelease() failed" );
        }
    }
    return rc;
}

/***************************************************************************
    vdb_copy_main:
    * called by "main()"
    * make the "native directory"
    * make a vdb-manager for write
      all subsequent copy-functions will use this manager...
    * check if the given path is database-path ( by trying to open it )
      if it is one: continue wit copy_database()
    * check if the given path is table-path ( by trying to open it )
      if it is one: continue wit copy_table()
    * release manager and directory

ctx        [IN] ... contains path, tablename, columns, row-range etc.
***************************************************************************/
static rc_t perform_vdb_copy( const p_context ctx ) {
    KDirectory * directory;
    rc_t rc = KDirectoryNativeDir( &directory );
    DISP_RC( rc, "vdb_copy_main:KDirectoryNativeDir() failed" );
    if ( 0 == rc ) {
        KConfig * config_mgr;

#if TOOLS_USE_SRAPATH != 0
        if ( !( ctx -> dont_check_accession ) ) {
            ctx->dont_check_accession = helper_is_this_a_filesystem_path( ctx -> src_path );
        }
        if ( !( ctx -> dont_check_accession ) ) {
            rc_t rc1 = helper_resolve_accession( directory, (char**)&( ctx -> src_path ) );
            DISP_RC( rc1, "vdb_copy_main:helper_check_accession() failed" );
        }
#endif

        rc = helper_make_config_mgr( &config_mgr, ctx -> kfg_path );
        DISP_RC( rc, "vdb_copy_main:helper_make_config_mgr() failed" );
        if ( 0 == rc ) {
            VDBManager * vdb_mgr;

            helper_read_config_values( config_mgr, &( ctx -> config ) );
            helper_read_redact_values( config_mgr, ctx -> rvals );
            KConfigRelease( config_mgr );

            rc = VDBManagerMakeUpdate ( &vdb_mgr, directory );
            DISP_RC( rc, "vdb_copy_main:VDBManagerMakeRead() failed" );
            if ( 0 == rc ) {
                /************************************/
                rc = perform_vdb_copy_tab_or_db( ctx, vdb_mgr );
                /************************************/
                {
                    rc_t rc1 = VDBManagerRelease( vdb_mgr );
                    DISP_RC( rc1, "vdb_copy_main:VDBManagerRelease() failed" );
                }
            }
        }
        /* after that we can remove the output-path if necessary */
        if ( 0 != rc && !ctx->dont_remove_target ) {
            rc_t rc1 = helper_remove_path( directory, ctx->dst_path );
            DISP_RC( rc1, "vdb_copy_main:helper_remove_path() failed" );
        }

        /* now we can release the directory */
        {
            rc_t rc1 = KDirectoryRelease( directory );
            DISP_RC( rc1, "vdb_copy_main:KDirectoryRelease() failed" );
        }
    }
    return rc;
}

/***************************************************************************
    Main:
    * create the copy-context
    * parse the commandline for arguments and options
    * react to help/usage - requests ( no dump in this case )
      these functions are in vdb-copy-context.c
    * call copy_main() to execute the copy-operation
    * destroy the copy-context
***************************************************************************/
MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                             MyOptions, sizeof MyOptions / sizeof ( OptDef ) );
    DISP_RC( rc, "Main:ArgsMakeAndHandle() failed" );
    if ( 0 == rc ) {
        context *ctx;
        KLogHandlerSetStdErr();
        rc = context_init( &ctx );
        DISP_RC( rc, "Main:copy_context_init() failed" );
        if ( 0 == rc ) {
            rc = context_capture_arguments_and_options( args, ctx );
            DISP_RC( rc, "Main:context_capture_arguments_and_options() failed" );
            if ( 0 == rc ) {
                if ( ctx -> usage_requested ) {
                    MiniUsage( args );
                    rc = RC(rcApp, rcArgv, rcParsing, rcParam, rcInsufficient);
                } else {
                    /****************************/
                    rc = perform_vdb_copy( ctx );
                    /****************************/
                }
            }
            context_destroy ( ctx );
        }
        ArgsWhack ( args );
    }
    return VDB_TERMINATE( rc );
}
