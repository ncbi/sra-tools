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

#include "eventring.h"

#include <sysalloc.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>


void event_ring_init( Event_Ring * ring )
{
    SLListInit( &ring->events );
    SLListInit( &ring->stock );
}

static void CC event_ring_whack( SLNode *n, void *data )
{
    void * event = ( void * )n;
    if ( event != NULL ) free( event );
}


void event_ring_destroy( Event_Ring * ring )
{
    SLListWhack ( &ring->events, event_ring_whack, NULL );
    SLListWhack ( &ring->stock, event_ring_whack, NULL );
}


/* ****************************************************************************************** */


tui_event * event_ring_get_from_stock_or_make( Event_Ring * ring )
{
    tui_event * event = ( tui_event * ) SLListPopHead ( &ring->stock );
    if ( event == NULL )
        event = malloc( sizeof * event );
    return event;
}


void event_ring_put( Event_Ring * ring, tui_event * event )
{
    SLListPushTail ( &ring->events, ( SLNode * )event );
}


tui_event * event_ring_get( Event_Ring * ring )
{
    return ( tui_event * ) SLListPopHead ( &ring->events );
}


void event_ring_put_to_stock( Event_Ring * ring, tui_event * event )
{
    SLListPushTail ( &ring->stock, ( SLNode * )event );
}


void copy_event( tui_event * src, tui_event * dst )
{
    dst->event_type = src->event_type;
    switch( src->event_type )
    {
        case ktui_event_none	: break;
        case ktui_event_kb		: dst->data.kb_data = src->data.kb_data; break;
        case ktui_event_mouse	: dst->data.mouse_data = src->data.mouse_data; break;
        case ktui_event_window	: dst->data.win_data = src->data.win_data; break;
    }
}


/* ****************************************************************************************** */

tuidlg_event * dlg_event_ring_get_from_stock_or_make( Event_Ring * ring )
{
    tuidlg_event * event = ( tuidlg_event * ) SLListPopHead ( &ring->stock );
    if ( event == NULL )
        event = malloc( sizeof * event );
    return event;
}


void dlg_event_ring_put( Event_Ring * ring, tuidlg_event * event )
{
    SLListPushTail ( &ring->events, ( SLNode * )event );
}


tuidlg_event * dlg_event_ring_get( Event_Ring * ring )
{
    return ( tuidlg_event * ) SLListPopHead ( &ring->events );
}


void dlg_event_ring_put_to_stock( Event_Ring * ring, tuidlg_event * event )
{
    SLListPushTail ( &ring->stock, ( SLNode * )event );
}


void copy_dlg_event( tuidlg_event * src, tuidlg_event * dst )
{
    dst->event_type = src->event_type;
    dst->widget_id = src->widget_id;
    dst->value_1 = src->value_1;
    dst->value_2 = src->value_2;
    dst->ptr_0 = src->ptr_0;
}
