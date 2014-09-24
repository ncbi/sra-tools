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

#include "ref_walker_0.h"

rc_t CC Quitting( void );

static rc_t walk_placements( walk_data * data, walk_funcs * funcs )
{
    rc_t rc;
    do
    {
        rc = ReferenceIteratorNextPlacement ( data->ref_iter, &data->rec );
        if ( GetRCState( rc ) != rcDone )
        {
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "ReferenceIteratorNextPlacement() failed" );
            }
            else
            {
                data->state = ReferenceIteratorState ( data->ref_iter, &data->seq_pos );
                data->xrec = ( tool_rec * ) PlacementRecordCast ( data->rec, placementRecordExtension1 );
                if ( funcs->on_placement != NULL )
                    rc = funcs->on_placement( data );
            }
        }
    } while ( rc == 0 );
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    return rc;
}


static rc_t walk_spot_group( walk_data * data, walk_funcs * funcs )
{
    rc_t rc;
    do
    {
        rc = ReferenceIteratorNextSpotGroup ( data->ref_iter, &data->spotgroup, &data->spotgroup_len );
        if ( GetRCState( rc ) != rcDone )
        {
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "ReferenceIteratorNextPos() failed" );
            }
            else
            {
                if ( funcs->on_enter_spotgroup != NULL )
                    rc = funcs->on_enter_spotgroup( data );
                if ( rc == 0 )
                    rc = walk_placements( data, funcs );
                if ( rc == 0 && funcs->on_exit_spotgroup != NULL )
                    rc = funcs->on_exit_spotgroup( data );
            }
        }
    } while ( rc == 0 );
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    return rc;
}


static rc_t walk_ref_pos( walk_data * data, walk_funcs * funcs )
{
    rc_t rc;
    do
    {
        rc = ReferenceIteratorNextPos ( data->ref_iter, !data->options->no_skip );
        if ( GetRCState( rc ) != rcDone )
        {
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "ReferenceIteratorNextPos() failed" );
            }
            else
            {
                rc = ReferenceIteratorPosition ( data->ref_iter, &data->ref_pos, &data->depth, &data->ref_base );
                if ( rc != 0 )
                {
                    LOGERR( klogInt, rc, "ReferenceIteratorPosition() failed" );
                }
                else if ( data->depth > 0 )
                {
                    bool skip = false;

                    if ( data->options->skiplist != NULL )
                        skip = skiplist_is_skip_position( data->options->skiplist, data->ref_pos + 1 );

                    if ( !skip )
                    {
                        if ( funcs->on_enter_ref_pos != NULL )
                            rc = funcs->on_enter_ref_pos( data );
                        if ( rc == 0 )
                            rc = walk_spot_group( data, funcs );
                        if ( rc == 0 && funcs->on_exit_ref_pos != NULL )
                            rc = funcs->on_exit_ref_pos( data );
                    }
                }
            }
            if ( rc == 0 ) { rc = Quitting(); }
        }
    } while ( rc == 0 );
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    return rc;
}


static rc_t walk_ref_window( walk_data * data, walk_funcs * funcs )
{
    rc_t rc;
    do
    {
        rc = ReferenceIteratorNextWindow ( data->ref_iter, &data->ref_window_start, &data->ref_window_len );
        if ( GetRCState( rc ) != rcDone )
        {
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "ReferenceIteratorNextWindow() failed" );
            }
            else
            {
                if ( funcs->on_enter_ref_window != NULL )
                    rc = funcs->on_enter_ref_window( data );
                if ( rc == 0 )
                    rc = walk_ref_pos( data, funcs );
                if ( rc == 0 && funcs->on_exit_ref_window != NULL )
                    rc = funcs->on_exit_ref_window( data );
            }
        }
    } while ( rc == 0 );
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    return rc;
}


rc_t walk_0( walk_data * data, walk_funcs * funcs )
{
    rc_t rc;

    data->ref_start = 0;
    data->ref_len = 0;
    data->ref_name = NULL;
    data->ref_obj = NULL;
    data->ref_window_start = 0;
    data->ref_window_len = 0;
    data->ref_pos = 0;
    data->depth = 0;
    data->ref_base = 0;
    data->spotgroup = NULL;
    data->spotgroup_len = 0;
    data->rec = NULL;
    data->xrec = NULL;
    data->state = 0;
    data->seq_pos = 0;

    do
    {
        rc = ReferenceIteratorNextReference( data->ref_iter, &data->ref_start, &data->ref_len, &data->ref_obj );
        if ( GetRCState( rc ) != rcDone )
        {
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "ReferenceIteratorNextReference() failed" );
            }
            else if ( data->ref_obj != NULL )
            {
                if ( data->options->use_seq_name )
                    rc = ReferenceObj_Name( data->ref_obj, &data->ref_name );
                else
                    rc = ReferenceObj_SeqId( data->ref_obj, &data->ref_name );

                if ( data->options->skiplist != NULL )
                    skiplist_enter_ref( data->options->skiplist, data->ref_name );

                if ( rc != 0 )
                {
                    if ( data->options->use_seq_name )
                    {
                        LOGERR( klogInt, rc, "ReferenceObj_Name() failed" );
                    }
                    else
                    {
                        LOGERR( klogInt, rc, "ReferenceObj_SeqId() failed" );
                    }
                }
                else
                {
                    if ( funcs->on_enter_ref != NULL )
                        rc = funcs->on_enter_ref( data );
                    if ( rc == 0 )
                        rc = walk_ref_window( data, funcs );
                    if ( rc == 0 && funcs->on_exit_ref != NULL )
                        rc = funcs->on_exit_ref( data );
                }
            }
        }
    } while ( rc == 0 );
    if ( GetRCState( rc ) == rcDone ) { rc = 0; }
    if ( GetRCState( rc ) == rcCanceled ) { rc = 0; }
    return rc;
}
