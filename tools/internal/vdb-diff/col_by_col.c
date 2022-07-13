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
#include "col_by_col.h"

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

static rc_t cbc_diff_column_iter( const col_pair * pair, const VCursor * cur_1, const VCursor * cur_2,
                                  const struct diff_ctx * dctx, const struct num_gen_iter * iter,
                                  unsigned long int * diffs )
{
    rc_t rc = 0;
    struct progressbar * progress = NULL;
    int64_t row_id;
    uint64_t rows_checked = 0;
    uint64_t rows_different = 0;
    
    if ( dctx -> show_progress )
        make_progressbar( &progress, 2 );

    while ( ( rc == 0 ) && ( num_gen_iterator_next( iter, &row_id, &rc ) ) && ( *diffs < dctx -> max_err ) )
    {
        if ( rc == 0 ) rc = Quitting();    /* to be able to cancel the loop by signal */
        if ( rc == 0 )
        {
            bool col_equal = true;

            if ( pair != NULL )
                rc = cmn_diff_column( pair, cur_1, cur_2, row_id,  &col_equal );

            if ( !col_equal )
            {
                if ( rc == 0 )	rc = KOutMsg( "\n" );
                rows_different++;
                ( *diffs )++;
            }
            rows_checked++;

            if ( progress != NULL )
            {
                uint32_t progress_value;
                if ( num_gen_iterator_percent( iter, 2, &progress_value ) == 0 )
                    update_progressbar( progress, progress_value );
            }

        } /* if (!Quitting) */
    } /* while ( num_gen_iterator_next() ) */

    if ( rc == 0 )
        rc = KOutMsg( "\n%,lu rows checked, %,lu rows differ\n", rows_checked, rows_different );

    if ( progress != NULL ) destroy_progressbar( progress );
	
	return rc;
}

static rc_t cbc_diff_column( col_pair * pair, const VCursor * cur_1, const VCursor * cur_2,
                             const struct diff_ctx * dctx, unsigned long int *diffs )
{
    rc_t rc = VCursorAddColumn( cur_1, &( pair -> idx[ 0 ] ), "%s", pair -> name );
    if ( rc != 0 )
    {
        LOGERR ( klogInt, rc, "VCursorAddColumn( acc #1 ) failed" );
    }
    else
    {
        rc = VCursorAddColumn( cur_2, &( pair -> idx[ 1 ] ), "%s", pair -> name );
        if ( rc != 0 )
        {
            LOGERR ( klogInt, rc, "VCursorAddColumn( acc #1 ) failed" );
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
                    rc = cmn_make_num_gen( cur_1, cur_2, pair->idx[0], pair->idx[1], dctx -> rows, &rows_to_diff );
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
                            rc = cbc_diff_column_iter( pair, cur_1, cur_2, dctx, iter, diffs );
                            /* *************************************************************** */
                            num_gen_iterator_destroy( iter );
                        }
                        num_gen_destroy( rows_to_diff );
                    }
                }
            }
        }
    }
    return rc;
}

rc_t cbc_diff_columns( const col_defs * defs, const VTable * tab_1, const VTable * tab_2,
                       const struct diff_ctx * dctx, const char * tablename, unsigned long int *diffs )
{
    rc_t rc = 0;
    uint32_t i;
    uint32_t count = VectorLength( &( defs -> cols ) );
    for ( i = 0; i < count && rc == 0 && ( *diffs < dctx -> max_err ); ++i )
    {
        col_pair * pair = VectorGet( &( defs -> cols ), i );
        if ( pair != NULL )
        {
            rc = KOutMsg( "comparing column '%s.%s'\n", tablename, pair -> name );
            if ( rc == 0 )
            {
                const VCursor * cur_1;
                rc = VTableCreateCursorRead( tab_1, &cur_1 );
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
                        /* *************************************************************** */
                        rc = cbc_diff_column( pair, cur_1, cur_2, dctx, diffs );
                        /* *************************************************************** */
                        VCursorRelease( cur_2 );
                    }
                    VCursorRelease( cur_1 );
                }
            }
        }
    }
    return rc;
}
