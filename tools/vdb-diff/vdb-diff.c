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

#include <kapp/main.h>

#include <klib/rc.h>
#include <klib/out.h>
#include <klib/log.h>
#include <klib/num-gen.h>
#include <klib/progressbar.h>

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>

#include <sra/sraschema.h>

#include "coldefs.h"
#include "namelist_tools.h"

#include <stdlib.h>
#include <string.h>


#define OPTION_ROWS         "rows"
#define ALIAS_ROWS          "R"

#define OPTION_COLUMNS      "columns"
#define ALIAS_COLUMNS       "C"

#define OPTION_TABLE        "table"
#define ALIAS_TABLE         "T"

#define OPTION_PROGRESS     "progress"
#define ALIAS_PROGRESS      "p"

#define OPTION_MAXERR       "maxerr"
#define ALIAS_MAXERR        "e"

#define OPTION_INTERSECT    "intersect"
#define ALIAS_INTERSECT     "i"

#define OPTION_EXCLUDE      "exclude"
#define ALIAS_EXCLUDE       "x"

static const char * rows_usage[] = { "set of rows to be comparend (default = all)", NULL };
static const char * columns_usage[] = { "set of columns to be compared (default = all)", NULL };
static const char * table_usage[] = { "name of table (in case of database ) to be compared", NULL };
static const char * progress_usage[] = { "show progress in percent", NULL };
static const char * maxerr_usage[] = { "max errors im comparing (default = 1)", NULL };
static const char * intersect_usage[] = { "intersect column-set from both runs", NULL };
static const char * exclude_usage[] = { "exclude these columns from comapring", NULL };

OptDef MyOptions[] =
{
    { OPTION_ROWS, 			ALIAS_ROWS, 		NULL, 	rows_usage, 		1, 	true, 	false },
	{ OPTION_COLUMNS, 		ALIAS_COLUMNS, 		NULL, 	columns_usage, 		1, 	true, 	false },
	{ OPTION_TABLE, 		ALIAS_TABLE, 		NULL, 	table_usage, 		1, 	true, 	false },
	{ OPTION_PROGRESS, 		ALIAS_PROGRESS,		NULL, 	progress_usage,		1, 	false, 	false },
	{ OPTION_MAXERR, 		ALIAS_MAXERR,		NULL, 	maxerr_usage,		1, 	true, 	false },
	{ OPTION_INTERSECT,		ALIAS_INTERSECT,	NULL, 	intersect_usage,	1, 	false, 	false },
	{ OPTION_EXCLUDE,		ALIAS_EXCLUDE,		NULL, 	exclude_usage,		1, 	true, 	false }		
};


const char UsageDefaultName[] = "vdb-diff";


rc_t CC UsageSummary ( const char * progname )
{
    return KOutMsg (
        "\n"
        "Usage:\n"
        "  %s <src_path> <dst_path> [options]\n"
        "\n", progname );
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

    if ( rc != 0 )
        progname = fullpath = UsageDefaultName;

    UsageSummary ( progname );

    KOutMsg ( "Options:\n" );
    HelpOptionLine ( ALIAS_ROWS, 		OPTION_ROWS, 		"row-range",	rows_usage );
    HelpOptionLine ( ALIAS_COLUMNS, 	OPTION_COLUMNS, 	"column-set",	columns_usage );
	HelpOptionLine ( ALIAS_TABLE, 		OPTION_TABLE, 		"table-name",	table_usage );
	HelpOptionLine ( ALIAS_PROGRESS, 	OPTION_PROGRESS,	NULL,			progress_usage );
	HelpOptionLine ( ALIAS_MAXERR, 		OPTION_MAXERR,	    "max value",	maxerr_usage );
	HelpOptionLine ( ALIAS_INTERSECT, 	OPTION_INTERSECT,   NULL,			intersect_usage );
	HelpOptionLine ( ALIAS_EXCLUDE, 	OPTION_EXCLUDE,   	"column-set",	exclude_usage );
	
    HelpOptionsStandard ();
    HelpVersion ( fullpath, KAppVersion() );

    return rc;
}

struct diff_ctx
{
    const char * src1;
    const char * src2;
	const char * columns;
	const char * excluded;
	const char * table;
	
    struct num_gen * rows;
	uint32_t max_err;
	bool show_progress;
	bool intersect;
};


static void init_diff_ctx( struct diff_ctx * dctx )
{
    dctx -> src1 = NULL;
    dctx -> src2 = NULL;
	dctx -> columns = NULL;
	dctx -> excluded = NULL;	
	dctx -> table = NULL;
	
    dctx -> rows = NULL;
	dctx -> show_progress = false;
	dctx -> intersect = false;
}


static void release_diff_ctx( struct diff_ctx * dctx )
{
	/* do not release the src1,src2,columns,table pointers! they are owned by the Args-obj. */
	if ( dctx -> rows != 0 )
	{
		num_gen_destroy( dctx -> rows );
	}
}


static const char * get_str_option( const Args *args, const char * option_name )
{
    const char * res = NULL;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsOptionCount() failed" );
	}
	else if ( count > 0 )
	{
		rc = ArgsOptionValue( args, option_name, 0, (const void **)&res );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "ArgsOptionValue( 0 ) failed" );
		}
	}
    return res;
}


static bool get_bool_option( const Args *args, const char * option_name, bool dflt )
{
    bool res = dflt;
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, option_name, &count );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsOptionCount() failed" );
	}
	else if ( count > 0 )
	{
		res = true;
	}
    return res;
}


static uint32_t get_uint32t_option( const Args *args, const char * option_name, uint32_t dflt )
{
    uint32_t res = dflt;
	const char * s = get_str_option( args, option_name );
	if ( s != NULL ) res = atoi( s );
    return res;
}


static rc_t gather_diff_ctx( struct diff_ctx * dctx, Args * args )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsParamCount() failed" );
	}
    else if ( count < 2 )
    {
        rc = RC( rcExe, rcNoTarg, rcReading, rcParam, rcIncorrect );
		LOGERR ( klogInt, rc, "This tool needs 2 arguments: source1 and source2" );
    }
    else
    {
        rc = ArgsParamValue( args, 0, (const void **)&dctx->src1 );
        if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "ArgsParamValue( 1 ) failed" );
		}
        else
        {
            rc = ArgsParamValue( args, 1, (const void **)&dctx->src2 );
            if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "ArgsParamValue( 2 ) failed" );
			}
        }
    }

    if ( rc == 0 )
    {
        const char * s = get_str_option( args, OPTION_ROWS );
        if ( s != NULL )
		{
            rc = num_gen_make_from_str( &dctx->rows, s );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "num_gen_make_from_str() failed" );
			}
		}
	}
	
	if ( rc == 0 )
	{
		dctx -> columns = get_str_option( args, OPTION_COLUMNS );
		dctx -> excluded = get_str_option( args, OPTION_EXCLUDE );		
		dctx -> show_progress = get_bool_option( args, OPTION_PROGRESS, false );
		dctx -> intersect = get_bool_option( args, OPTION_INTERSECT, false );
		dctx -> max_err = get_uint32t_option( args, OPTION_MAXERR, 1 );
    }

    return rc;
}


static rc_t report_diff_ctx( struct diff_ctx * dctx )
{
    rc_t rc = KOutMsg( "src[ 1 ] : %s\n", dctx -> src1 );
	if ( rc == 0 )
		rc = KOutMsg( "src[ 2 ] : %s\n", dctx -> src2 );

	if ( rc == 0 )
	{
		if ( dctx -> rows != NULL )
		{
			char buffer[ 4096 ];
			rc = num_gen_as_string( dctx -> rows, buffer, sizeof buffer, NULL, false );
			if ( rc == 0 )
				rc = KOutMsg( "- rows : %s\n", buffer );
		}
		else
			rc = KOutMsg( "- rows : all\n" );
	}
		
	if ( rc == 0 && dctx -> columns != NULL )
		rc = KOutMsg( "- columns : %s\n", dctx -> columns );
	if ( rc == 0 && dctx -> excluded != NULL )
		rc = KOutMsg( "- exclude : %s\n", dctx -> excluded );
	if ( rc == 0 && dctx -> table != NULL )
		rc = KOutMsg( "- table : %s\n", dctx -> table );
	if ( rc == 0 )
		rc = KOutMsg( "- progress : %s\n", dctx -> show_progress ? "show" : "hide" );
	if ( rc == 0 )
		rc = KOutMsg( "- intersect: %s\n", dctx -> intersect ? "yes" : "no" );
	if ( rc == 0 )
		rc = KOutMsg( "- max err : %u\n", dctx -> max_err );

	if ( rc == 0 )
		rc = KOutMsg( "\n" );
	return rc;
}


static rc_t diff_columns_iter( col_defs * defs, const VCursor * cur_1, const VCursor * cur_2,
							   struct diff_ctx * dctx, const struct num_gen_iter * iter )
{
	uint32_t column_count;
	rc_t rc = col_defs_count( defs, &column_count );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "col_defs_count() failed" );
	}
	else
	{
		struct progressbar * progress = NULL;
		int64_t row_id;
		uint64_t rows_checked = 0;
		uint64_t rows_different = 0;
		
		if ( dctx -> show_progress )
			make_progressbar( &progress, 2 );
	
		while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) )
		{
			if ( rc == 0 ) rc = Quitting();    /* to be able to cancel the loop by signal */
			if ( rc == 0 )
			{
				bool row_equal = true;
				
				uint32_t col_id;
				for ( col_id = 0; col_id < column_count && rc == 0; ++col_id )
				{
					col_pair * pair = VectorGet( &( defs -> cols ), col_id );
					if ( pair != NULL )
					{
						uint32_t elem_bits_1, boff_1, row_len_1;
						const void * base_1;
						rc = VCursorCellDataDirect ( cur_1, row_id, pair->pair[ 0 ].idx, 
													 &elem_bits_1, &base_1, &boff_1, &row_len_1 );
						if ( rc != 0 )
						{
							PLOGERR( klogInt, ( klogInt, rc, 
									 "VCursorCellDataDirect( #1 [$(col)].$(row) ) failed",
									 "col=%s,row=%ld", pair->name, row_id ) );
						}
						else
						{
							uint32_t elem_bits_2, boff_2, row_len_2;
							const void * base_2;
							rc = VCursorCellDataDirect ( cur_2, row_id, pair->pair[ 1 ].idx, 
														 &elem_bits_2, &base_2, &boff_2, &row_len_2 );
							if ( rc != 0 )
							{
								PLOGERR( klogInt, ( klogInt, rc, 
										 "VCursorCellDataDirect( #2 [$(col)].$(row) ) failed",
										 "col=%s,row=%ld", pair->name, row_id ) );
							}
							else
							{
								bool bin_diff = true;
								
								if ( elem_bits_1 != elem_bits_2 )
								{
									bin_diff = row_equal = false;
									rc = KOutMsg( "%s[ %ld ].elem_bits %u != %u\n", pair->name, row_id, elem_bits_1, elem_bits_2 );
								}

								if ( row_len_1 != row_len_2 )
								{
									bin_diff = row_equal = false;
									if ( rc == 0 )
									{
										rc = KOutMsg( "%s[ %ld ].row_len %u != %u\n", pair->name, row_id, row_len_1, row_len_2 );
									}
								}

								if ( boff_1 != 0 || boff_2 != 0 )
								{
									bin_diff = row_equal = false;
									if ( rc == 0 )
									{
										rc = KOutMsg( "%s[ %ld ].bit_offset: %u, %u\n", pair->name, row_id, boff_1, boff_2 );
									}
								}
								
								if ( bin_diff )
								{
									size_t num_bits = ( row_len_1 * elem_bits_1 );
									if ( num_bits & 0x07 )
									{
										if ( rc == 0 )
										{
											rc = KOutMsg( "%s[ %ld ].bits_total %% 8 = %u\n", pair->name, row_id, ( num_bits % 8 ) );
										}
									}
									else
									{
										size_t num_bytes = ( num_bits >> 3 );
										int cmp = memcmp ( base_1, base_2, num_bytes );
										if ( cmp != 0 )
										{
											if ( rc == 0 )
											{
												rc = KOutMsg( "%s[ %ld ] differ\n", pair->name, row_id );
											}
											row_equal = false;
										}
									}
								}
							}
						}
					}
				} /* for ( col_id ... ) */
				
				if ( !row_equal )
				{
					if ( rc == 0 )	rc = KOutMsg( "\n" );
					rows_different ++;
					if ( rows_different >= dctx -> max_err )
						rc = RC( rcExe, rcNoTarg, rcComparing, rcRow, rcInconsistent );		
				}
				rows_checked ++;
				
				if ( progress != NULL )
				{
					uint32_t progress_value;
					if ( num_gen_iterator_percent( iter, 2, &progress_value ) == 0 )
						update_progressbar( progress, progress_value );
				}
				
			} /* if (!Quitting) */
		} /* while ( num_gen_iterator_next() ) */

		if ( rc == 0 )
			rc = KOutMsg( "\n%,lu rows checked ( %d columns each ), %,lu rows differ\n",
				rows_checked, column_count, rows_different );

		if ( rows_different > 0 )
			rc = RC( rcExe, rcNoTarg, rcComparing, rcRow, rcInconsistent );
		
		if ( progress != NULL )
			destroy_progressbar( progress );
			
	} /* if ( col_defs_count == 0 )*/
	
	return rc;
}


static rc_t diff_columns_cursor( col_defs * defs, const VCursor * cur_1, const VCursor * cur_2, struct diff_ctx * dctx )
{
    int64_t  first_1;
    uint64_t count_1;
    rc_t rc = VCursorIdRange( cur_1, 0, &first_1, &count_1 );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "VCursorIdRange( acc #1 ) failed" );
	}
	else
	{
		int64_t  first_2;
		uint64_t count_2;
		rc = VCursorIdRange( cur_2, 0, &first_2, &count_2 );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "VCursorIdRange( acc #2 ) failed" );
		}
		else
		{
			struct num_gen * rows_to_diff = NULL;
			if ( dctx -> rows != NULL )
			{
				num_gen_copy( dctx -> rows, &rows_to_diff );
			}

			if ( rows_to_diff == NULL )
			{
				/* no row-range given create the number generator from the discovered range, if it is the same */
				if ( first_1 != first_2 || count_1 != count_2 )
				{
					rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
					PLOGERR( klogInt, ( klogInt, rc, "row-ranges differ: $(first1).$(count1) != $(first2).$(count2)",
							 "first1=%ld,count1=%lu,first2=%ld,count2=%lu",
							 first_1, count_1, first_2, count_2 ) );
				}
				else
				{
					rc = num_gen_make_from_range( &rows_to_diff, first_1, count_1 );
					if ( rc != 0 )
					{
						LOGERR ( klogInt, rc, "num_gen_make_from_range() failed" );
					}
				}
			}
			else
			{
				/* row-range given, clip the rows be the 2 ranges ( even if they are not the same ) */
				rc = num_gen_trim( rows_to_diff, first_1, count_1 );
				if ( rc != 0 )
				{
					LOGERR ( klogInt, rc, "num_gen_trim( acc #1 ) failed" );
				}
				else if ( first_1 != first_2 || count_1 != count_2 )
				{
					rc = num_gen_trim( rows_to_diff, first_2, count_2 );
					if ( rc != 0 )
					{
						LOGERR ( klogInt, rc, "num_gen_trim( acc #2 ) failed" );
					}
				}
			}
			
			if ( rc == 0 )
			{
				const struct num_gen_iter * iter;
				rc = num_gen_iterator_make( rows_to_diff, &iter );
				if ( rc != 0 )
				{
					LOGERR ( klogInt, rc, "num_gen_iterator_make() failed" );
				}
				else
				{
					/* *************************************************************** */
					rc = diff_columns_iter( defs, cur_1, cur_2, dctx, iter );
					/* *************************************************************** */
					num_gen_iterator_destroy( iter );
				}
			}
			
			if ( rows_to_diff != NULL )
				num_gen_destroy( rows_to_diff );
		}
	}
	return rc;
}


static rc_t diff_columns( col_defs * defs, const VTable * tab_1, const VTable * tab_2, struct diff_ctx * dctx )
{
	const VCursor * cur_1;
	rc_t rc = VTableCreateCursorRead( tab_1, &cur_1 );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "VTableCreateCursorRead( acc #1 ) failed" );
	}
	else
	{
		const VCursor * cur_2;
		rc = VTableCreateCursorRead( tab_2, &cur_2 );	
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "VTableCreateCursorRead( acc #2 ) failed" );
		}
		else
		{
			rc = col_defs_add_to_cursor( defs, cur_1, 0 );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "failed to add all requested columns to cursor of 1st accession" );
			}
			else
			{
				rc = col_defs_add_to_cursor( defs, cur_2, 1 );
				if ( rc != 0 )
				{
					LOGERR ( klogInt, rc, "failed to add all requested columns to cursor of 2nd accession" );
				}
				else
				{
					rc = VCursorOpen( cur_1 );
					if ( rc != 0 )
					{
						LOGERR ( klogInt, rc, "VCursorOpen( acc #1 ) failed" );
					}
					else
					{
						rc = VCursorOpen( cur_2 );
						if ( rc != 0 )
						{
							LOGERR ( klogInt, rc, "VCursorOpen( acc #2 ) failed" );
						}
						else
						{
							/* ************************************************** */
							rc = diff_columns_cursor( defs, cur_1, cur_2, dctx );
							/* ************************************************** */
						}
					}
				}
			}
			VCursorRelease( cur_2 );
		}
		VCursorRelease( cur_1 );
	}
	return 0;
}


static rc_t compare_2_namelists( const KNamelist * cols_1, const KNamelist * cols_2, const KNamelist ** cols_to_diff, bool intersect )
{
	uint32_t count_1;
	rc_t rc = KNamelistCount( cols_1, &count_1 );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "KNamelistCount() failed" );
	}
	else
	{
		uint32_t count_2;
		rc = KNamelistCount( cols_2, &count_2 );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "KNamelistCount() failed" );
		}
		else if ( count_1 != count_2 )
		{
			if ( intersect )
			{
				rc = nlt_build_intersect( cols_1, cols_2, cols_to_diff );
				if ( rc != 0 )
				{
					LOGERR ( klogInt, rc, "nlt_build_intersect() failed" );	
				}
			}
			else
			{
				rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
				PLOGERR( klogInt, ( klogInt, rc,
						 "the accessions have not the same number of columns! ( $(count1) != $(count2) )\n",
						 "count1=%u,count2=%u", count_1, count_2 ) );
				*cols_to_diff = NULL;
			}
		}
		else
		{
			uint32_t found_in_both;
			bool equal = nlt_compare_namelists( cols_1, cols_2, &found_in_both );
			if ( equal )
			{
				rc = nlt_copy_namelist( cols_1, cols_to_diff );
				if ( rc != 0 )
				{
					LOGERR ( klogInt, rc, "nlt_copy_namelist() failed" );
				}
			}
			else
			{
				if ( intersect )
				{
					rc = nlt_build_intersect( cols_1, cols_2, cols_to_diff );
					if ( rc != 0 )
					{
						LOGERR ( klogInt, rc, "nlt_build_intersect() failed" );	
					}
				}
				else
				{
					rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
					KOutMsg( "the 2 accessions have not the same set of columns! ( %u found in both )\n", found_in_both );
				}
			}
		}
	}
	return rc;
}


static rc_t extract_columns_from_2_namelists( const KNamelist * cols_1, const KNamelist * cols_2, const char * sub_set,
											  const KNamelist ** cols_to_diff )
{
	rc_t rc = nlt_make_namelist_from_string( cols_to_diff, sub_set );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "cannot parse set of requested columns" );
	}
	else
	{
		if ( nlt_namelist_is_sub_set_in_full_set( *cols_to_diff, cols_1 ) )
		{
			if ( nlt_namelist_is_sub_set_in_full_set( *cols_to_diff, cols_2 ) )
			{
				rc = 0;
			}
			else
			{
				rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
				LOGERR ( klogInt, rc, "accession #2 is missing some of the requested columns" );
			}
		}
		else
		{
			rc = RC( rcExe, rcNoTarg, rcResolving, rcParam, rcInvalid );
			LOGERR ( klogInt, rc, "accession #1 is missing some of the requested columns" );
		}
	}
	return rc;
}


static rc_t perform_table_diff( const VTable * tab_1, const VTable * tab_2, struct diff_ctx * dctx )
{
	col_defs * defs;
	rc_t rc = col_defs_init( &defs );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "col_defs_init() failed" );
	}
	else
	{
		KNamelist * cols_1;
		rc_t rc = VTableListReadableColumns( tab_1, &cols_1 );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "VTableListReadableColumns( acc #1 ) failed" );
		}
		else
		{
			KNamelist * cols_2;
			rc = VTableListReadableColumns( tab_2, &cols_2 );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "VTableListReadableColumns( acc #2 ) failed" );
			}
			else
			{
				const KNamelist * cols_to_diff;
				
				if ( dctx -> columns == NULL )
					rc = compare_2_namelists( cols_1, cols_2, &cols_to_diff, dctx -> intersect );
				else
					rc = extract_columns_from_2_namelists( cols_1, cols_2, dctx -> columns, &cols_to_diff );

				if ( rc == 0 && dctx -> excluded != NULL )
				{
					const KNamelist * temp;
					rc = nlt_remove_strings_from_namelist( cols_to_diff, &temp, dctx -> excluded );
					KNamelistRelease( cols_to_diff );
					cols_to_diff = temp;
				}
				
				if ( rc == 0 )
				{
					rc = col_defs_fill( defs, cols_to_diff );
					if ( rc == 0 )
					{
						/* ******************************************* */
						rc = diff_columns( defs, tab_1, tab_2, dctx );
						/* ******************************************* */
					}
					KNamelistRelease( cols_to_diff );
				}
				
				KNamelistRelease( cols_2 );
			}
			KNamelistRelease( cols_1 );
		}
		col_defs_destroy( defs );
	}
	return 0;
}


static rc_t perform_database_diff_on_this_table( const VDatabase * db_1, const VDatabase * db_2,
												 struct diff_ctx * dctx, const char * table_name )
{
	/* we want to compare only the table wich name was given at the commandline */
	const VTable * tab_1;
	rc_t rc = VDatabaseOpenTableRead ( db_1, &tab_1, "%s", table_name );
	if ( rc != 0 )
	{
		PLOGERR( klogInt, ( klogInt, rc, 
				 "VDatabaseOpenTableRead( [$(acc)].$(tab) ) failed",
				 "acc=%s,tab=%s", dctx -> src1, table_name ) );
	}
	else
	{
		const VTable * tab_2;
		rc = VDatabaseOpenTableRead ( db_2, &tab_2, "%s", table_name );
		if ( rc != 0 )
		{
			PLOGERR( klogInt, ( klogInt, rc, 
					 "VDatabaseOpenTableRead( [$(acc)].$(tab) ) failed",
					 "acc=%s,tab=%s", dctx -> src2, table_name ) );
		}
		else
		{
			/* ******************************************* */
			rc = perform_table_diff( tab_1, tab_2, dctx );
			/* ******************************************* */
			VTableRelease( tab_2 );
		}
		VTableRelease( tab_1 );
	}
	return rc;
}


static rc_t perform_database_diff( const VDatabase * db_1, const VDatabase * db_2, struct diff_ctx * dctx )
{
	rc_t rc = 0;
	if ( dctx -> table != NULL )
	{
		/* we want to compare only the table wich name was given at the commandline */
		/* ************************************************************************** */
		rc = perform_database_diff_on_this_table( db_1, db_2, dctx, dctx -> table );
		/* ************************************************************************** */
	}
	else
	{
		/* we want to compare all tables of the 2 accession, if they have the same set of tables */
		KNamelist * table_set_1;
		rc = VDatabaseListTbl ( db_1, &table_set_1 );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "VDatabaseListTbl( acc #1 ) failed" );
		}
		else
		{
			KNamelist * table_set_2;
			rc = VDatabaseListTbl ( db_2, &table_set_2 );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "VDatabaseListTbl( acc #2 ) failed" );
			}
			else
			{
				if ( nlt_compare_namelists( table_set_1, table_set_2, NULL ) )
				{
					/* the set of tables match, walk the first set, and perform a diff on each table */
					uint32_t count;
					rc = KNamelistCount( table_set_1, &count );
					if ( rc != 0  )
					{
						LOGERR ( klogInt, rc, "KNamelistCount() failed" );
					}
					else
					{
						uint32_t idx;
						for ( idx = 0; idx < count && rc == 0; ++idx )
						{
							const char * table_name;
							rc = KNamelistGet( table_set_1, idx, &table_name );
							if ( rc != 0 )
							{
								LOGERR ( klogInt, rc, "KNamelistGet() failed" );
							}
							else
							{
								rc = KOutMsg( "\ncomparing table: %s\n", table_name );
								if ( rc == 0 )
								{
									/* ********************************************************************* */
									rc = perform_database_diff_on_this_table( db_1, db_2, dctx, table_name );
									/* ********************************************************************* */									
								}
							}
						}
					}
				}
				else
				{
					LOGERR ( klogInt, rc, "the 2 databases have not the same set of tables" );
				}
				KNamelistRelease( table_set_2 );
			}
			KNamelistRelease( table_set_1 );
		}
	}
	return rc;
}


/***************************************************************************
    perform the actual diffing
***************************************************************************/
static rc_t perform_diff( struct diff_ctx * dctx )
{
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
	if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "KDirectoryNativeDir() failed" );
	}
	else
    {
		const VDBManager * vdb_mgr;
		rc = VDBManagerMakeRead ( &vdb_mgr, dir );
		if ( rc != 0 )
		{
			LOGERR ( klogInt, rc, "VDBManagerMakeRead() failed" );
		}
		else
		{
			VSchema * vdb_schema = NULL;
			rc = VDBManagerMakeSRASchema( vdb_mgr, &vdb_schema );
			if ( rc != 0 )
			{
				LOGERR ( klogInt, rc, "VDBManagerMakeSRASchema() failed" );
			}
			else
			{
				const VDatabase * db_1;
				/* try to open it as a database */
				rc = VDBManagerOpenDBRead ( vdb_mgr, &db_1, vdb_schema, "%s", dctx -> src1 );
				if ( rc == 0 )
				{
					const VDatabase * db_2;
					rc = VDBManagerOpenDBRead ( vdb_mgr, &db_2, vdb_schema, "%s", dctx -> src2 );
					if ( rc == 0 )
					{
						/* ******************************************** */
						rc = perform_database_diff( db_1, db_2, dctx );
						/* ******************************************** */
						VDatabaseRelease( db_2 );
					}
					else
					{
						LOGERR ( klogInt, rc, "accession #1 opened as database, but accession #2 cannot be opened as database" );
					}
					VDatabaseRelease( db_1 );
				}
				else
				{
					const VTable * tab_1;
					rc = VDBManagerOpenTableRead( vdb_mgr, &tab_1, vdb_schema, "%s", dctx -> src1 );
					if ( rc == 0 )
					{
						const VTable * tab_2;
						rc = VDBManagerOpenTableRead( vdb_mgr, &tab_2, vdb_schema, "%s", dctx -> src2 );
						if ( rc == 0 )
						{
							/* ******************************************** */
							rc = perform_table_diff( tab_1, tab_2, dctx );
							/* ******************************************** */
							VTableRelease( tab_2 );
						}
						else
						{
							LOGERR ( klogInt, rc, "accession #1 opened as table, but accession #2 cannot be opened as table" );
						}
						VTableRelease( tab_1 );
					}
					else
					{
						LOGERR ( klogInt, rc, "accession #1 cannot be opened as table or database" );
					}
				}
				VSchemaRelease( vdb_schema );
			}
			VDBManagerRelease( vdb_mgr );
		}
		KDirectoryRelease( dir );
	}
	return rc;
}


/***************************************************************************
    Main:
***************************************************************************/
rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc = ArgsMakeAndHandle ( &args, argc, argv, 1,
                                  MyOptions, sizeof MyOptions / sizeof ( OptDef ) );
    if ( rc != 0 )
	{
		LOGERR ( klogInt, rc, "ArgsMakeAndHandle failed()" );
	}
    else
    {
        struct diff_ctx dctx;

        KLogHandlerSetStdErr();
        init_diff_ctx( &dctx );
        rc = gather_diff_ctx( &dctx, args );
        if ( rc == 0 )
        {
            rc = report_diff_ctx( &dctx );
			if ( rc == 0 )
			{
				/* ***************************** */
				rc = perform_diff( &dctx );
				/* ***************************** */				
			}
        }
        release_diff_ctx( &dctx );

        ArgsWhack ( args );
    }
    return rc;
}
