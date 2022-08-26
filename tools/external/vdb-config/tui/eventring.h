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

#ifndef _h_eventring_
#define _h_eventring_

#ifndef _h_tui_extern_
#include <tui/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <tui/tui.h>
#include <tui/tui_dlg.h>

#define EVENT_RING_SIZE 128

typedef struct Event_Ring
{
    SLList events;
    SLList stock;
} Event_Ring;

void event_ring_init( Event_Ring * ring );
void event_ring_destroy( Event_Ring * ring );

tui_event * event_ring_get_from_stock_or_make( Event_Ring * ring );
void event_ring_put( Event_Ring * ring, tui_event * event );
tui_event * event_ring_get( Event_Ring * ring );
void event_ring_put_to_stock( Event_Ring * ring, tui_event * event );
void copy_event( tui_event * src, tui_event * dst );


tuidlg_event * dlg_event_ring_get_from_stock_or_make( Event_Ring * ring );
void dlg_event_ring_put( Event_Ring * ring, tuidlg_event * event );
tuidlg_event * dlg_event_ring_get( Event_Ring * ring );
void dlg_event_ring_put_to_stock( Event_Ring * ring, tuidlg_event * event );
void copy_dlg_event( tuidlg_event * src, tuidlg_event * dst );


#ifdef __cplusplus
}
#endif

#endif /* _h_eventring */
