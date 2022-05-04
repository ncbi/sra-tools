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

#ifndef _h_tui_priv_
#define _h_tui_priv_

#ifndef _h_tui_extern_
#include <tui/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/refcount.h>
#include <tui/tui.h>
#include <kfs/directory.h>
#include <vfs/manager.h>

#include "eventring.h"
#include "screen.h"

struct KTUI_pf;

typedef struct KTUI
{
    KRefcount refcount;
    const KTUIMgr * mgr;
    Event_Ring ev_ring;
    tui_screen screen;
    int lines, cols;
    uint32_t timeout;

    struct KTUI_pf * pf;
} KTUI;


void CC put_window_event( struct KTUI * self, int lines, int cols );
void CC put_kb_event( struct KTUI * self, int key, KTUI_key code );
void CC put_mouse_event( struct KTUI * self, unsigned int x, unsigned int y, 
                         KTUI_mouse_button button, KTUI_mouse_action action, uint32_t raw_event );

void CC read_from_stdin( struct KTUI * self, uint32_t timeout );
void CC tui_send_strip( int x, int y, int count, tui_ac * curr, tui_ac * v,
                        const char * s );

rc_t CC KTUI_Init_platform( KTUI * self );
rc_t CC KTUI_Destroy_platform ( struct KTUI_pf * pf );

rc_t native_to_internal( VFSManager * vfs_mgr, const char * native, char * buffer, uint32_t buffer_size, size_t * written );
rc_t internal_to_native( VFSManager * vfs_mgr, const char * internal, char * buffer, uint32_t buffer_size, size_t * written );
rc_t set_native_caption( struct KTUIDlg * dlg, VFSManager * vfs_mgr, uint32_t id, const char * internal_path );
rc_t fill_widget_with_dirs( struct KTUIDlg * dlg, KDirectory * dir, uint32_t id,
                            const char * path, const char * to_focus, bool clear );
rc_t fill_widget_with_files( struct KTUIDlg * dlg, KDirectory * dir, uint32_t id,
                             const char * path, const char * extension, bool clear );

#ifdef __cplusplus
}
#endif

#endif /* _h_tui_priv */
