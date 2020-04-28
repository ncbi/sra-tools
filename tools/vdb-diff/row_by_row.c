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
#include "row_by_row.h"

#include <klib/log.h>
#include <klib/out.h>
#include <klib/num-gen.h>
#include <vdb/cursor.h>
#include <klib/progressbar.h>

#include "coldefs.h"
#include "cmn.h"

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>

rc_t Quitting( void );  /* because we cannot include <kapp/main.h> where it is defined! */

static rc_t rbr_diff_columns_iter( const col_defs * defs, const VCursor * cur_1, const VCursor * cur_2,
                                   const struct diff_ctx * dctx, const struct num_gen_iter * iter,
                                   unsigned long int *diffs )
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
	
		while ( rc == 0 && num_gen_iterator_next( iter, &row_id, &rc ) && *diffs < dctx->max_err )
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
                        bool col_equal;
                        rc = cmn_diff_column( pair, cur_1, cur_2, row_id,  &col_equal );
                        if ( !col_equal )
                        {
                            row_equal = false;
                            ( *diffs )++;
                        }
					}
				} /* for ( col_id ... ) */
				
				if ( !row_equal )
				{
					if ( rc == 0 )	rc = KOutMsg( "\n" );
					rows_different++;
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

		if ( progress != NULL )
			destroy_progressbar( progress );
			
	} /* if ( col_defs_count == 0 )*/
	
	return rc;
}


rc_t rbr_diff_columns( col_defs * defs, const VTable * tab_1, const VTable * tab_2,
                       const struct diff_ctx * dctx, unsigned long int *diffs )
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
                            struct num_gen * rows_to_diff = NULL;
                            rc = cmn_make_num_gen( cur_1, cur_2, 0, 0, dctx -> rows, &rows_to_diff );
                            if ( rc == 0 && rows_to_diff != NULL )
                            {
                                const struct num_gen_iter * iter = NULL;
                                rc = num_gen_iterator_make( rows_to_diff, &iter );
                                if ( rc != 0 )
                                {
                                    LOGERR ( klogInt, rc, "num_gen_iterator_make() failed" );
                                }
                                else if ( iter != NULL )
                                {
                                    /* *************************************************************** */
                                    rc = rbr_diff_columns_iter( defs, cur_1, cur_2, dctx, iter, diffs );
                                    /* *************************************************************** */
                                    num_gen_iterator_destroy( iter );
                                }
                                num_gen_destroy( rows_to_diff );
                            }
						}
					}
				}
			}
			VCursorRelease( cur_2 );
		}
		VCursorRelease( cur_1 );
	}
	return rc;
}
