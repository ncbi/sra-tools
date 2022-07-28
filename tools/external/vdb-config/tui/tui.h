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

#ifndef _h_tui_
#define _h_tui_

#ifndef _h_tui_extern_
#include <tui/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifndef _h_klib_container_
#include <klib/container.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/*--------------------------------------------------------------------------
 * KTUIMgr
 *  the manager that creates a KTUI instance...
 */
typedef struct KTUIMgr KTUIMgr;



/*--------------------------------------------------------------------------
 * KTUI
 *  a handle to platform specific details of a text user interface
 */
struct KTUI;


/*--------------------------------------------------------------------------
 * KTUI_event_type
 */
typedef uint32_t KTUI_event_type;
enum
{
    ktui_event_none = 0,
    ktui_event_kb,
    ktui_event_mouse,
    ktui_event_window
};


typedef uint32_t KTUI_key;
enum
{
    ktui_down = 1,
    ktui_up,
    ktui_left,
    ktui_right,
    ktui_home,
    ktui_end,
    ktui_bksp,
    ktui_F1,
    ktui_F2,
    ktui_F3,
    ktui_F4,
    ktui_F5,
    ktui_F6,
    ktui_F7,
    ktui_F8,
    ktui_F9,
    ktui_F10,
    ktui_F11,
    ktui_F12,
    ktui_del,
    ktui_ins,
    ktui_pgdn,
    ktui_pgup,
    ktui_enter,
    ktui_tab,
    ktui_shift_tab,
    ktui_alpha,
	ktui_none
};

/*--------------------------------------------------------------------------
 * open sturcture for event
 */

typedef struct KTUI_kb_data
{
    KTUI_key code;
    uint32_t key;
} KTUI_kb_data;


typedef uint32_t KTUI_mouse_button;
enum
{
    ktui_mouse_button_none = 1,
    ktui_mouse_button_left,
    ktui_mouse_button_middle,
    ktui_mouse_button_right,
	ktui_mouse_button_up
};

typedef uint32_t KTUI_mouse_action;
enum
{
    ktui_mouse_action_none = 1,
    ktui_mouse_action_button,
	ktui_mouse_action_move,
	ktui_mouse_action_scroll
};

typedef struct KTUI_mouse_data
{
    uint32_t x, y, raw_event;
    KTUI_mouse_button button;
	KTUI_mouse_action action;
} KTUI_mouse_data;


typedef struct KTUI_win_data
{
    uint32_t w, h;
} KTUI_win_data;


union tui_event_data
{
    KTUI_kb_data     kb_data;
    KTUI_mouse_data  mouse_data;
    KTUI_win_data    win_data;
};

typedef struct tui_event
{
    SLNode node;
    KTUI_event_type event_type;
    union tui_event_data data;
} tui_event;


/*--------------------------------------------------------------------------
 * open sturcture for command
 */

typedef uint32_t KTUI_color;
enum
{
    KTUI_c_black = 0,
    KTUI_c_gray,
    KTUI_c_red,
    KTUI_c_dark_red,
    KTUI_c_green,
    KTUI_c_dark_green,
    KTUI_c_yellow,
    KTUI_c_brown,
    KTUI_c_blue,
    KTUI_c_dark_blue,
    KTUI_c_magenta,
    KTUI_c_dark_magenta,
    KTUI_c_cyan,
    KTUI_c_dark_cyan,
    KTUI_c_white,
    KTUI_c_light_gray
};


typedef uint32_t KTUI_attrib;
enum
{
    KTUI_a_none = 0,
    KTUI_a_bold = 1,
    KTUI_a_underline = 2,
    KTUI_a_blink = 4,
    KTUI_a_inverse = 8
};


typedef struct tui_point
{
    uint32_t x, y;
} tui_point;


typedef struct tui_rect
{
    tui_point top_left;
    uint32_t w, h;
} tui_rect;


/* attribute and color */
typedef struct tui_ac
{
    KTUI_attrib attr;
    KTUI_color fg;
    KTUI_color bg;
} tui_ac;


/* Make
 *  creates a platform specific TUI object
 */
TUI_EXTERN rc_t CC KTUIMake ( const KTUIMgr * mgr, struct KTUI ** self, uint32_t timeout );


/* AddRef
 * Release
 *  ignores NULL references
 */
TUI_EXTERN rc_t CC KTUIAddRef ( const struct KTUI * self );
TUI_EXTERN rc_t CC KTUIRelease ( const struct KTUI * self );


/* SetTimeout
 *  changes the timeout value...
 */
TUI_EXTERN rc_t CC KTUISetTimeout ( struct KTUI * self, uint32_t timeout );


/* Read
 *  reads one event ( keyboard, mouse or window event )
 */
TUI_EXTERN rc_t CC KTUIGet ( struct KTUI * self, tui_event * event );


/* Rect
 *  Print a string into the internal screen buffer with the given coordinates and attribute
 */
LIB_EXPORT rc_t CC KTUIPrint( struct KTUI * self, const tui_point * p, const tui_ac * ac, const char * s, uint32_t l );


/* Rect
 *  Fill a rectangle of the internal screen buffer with the given attribute and char
 *  if r == NULL it fills the whole screen buffer ( effectivly clear-screen )
 */
TUI_EXTERN rc_t CC KTUIRect ( struct KTUI * self, const tui_rect * r, const tui_ac * ac, const char c );


/* Flush
 *  flush the content of the internal screen buffer out to the console
 */
TUI_EXTERN rc_t CC KTUIFlush ( struct KTUI * self, bool forced );


/* GetExtent
 *  query how many rows/columns the terminal has
 */
TUI_EXTERN rc_t CC KTUIGetExtent ( struct KTUI * self, int * cols, int * lines );


/* ClrScr
 *  calls GetExtend, then clears the whole screen
 */
TUI_EXTERN rc_t CC KTUIClrScr( struct KTUI * self, KTUI_color bg );


TUI_EXTERN void CC set_ac( tui_ac * dst, KTUI_attrib attr, KTUI_color fg, KTUI_color bg );
TUI_EXTERN void CC copy_ac( tui_ac * dst, const tui_ac * src );
TUI_EXTERN void CC inverse_ac( tui_ac * dst, const tui_ac * src );

TUI_EXTERN void CC set_rect( tui_rect * dst, int x, int y, int w, int h );
TUI_EXTERN void CC copy_rect( tui_rect * dst, const tui_rect * src );

TUI_EXTERN bool CC is_alpha_key( tui_event * event, char c );
TUI_EXTERN bool CC is_key_code( tui_event * event, KTUI_key k );

#ifdef __cplusplus
}
#endif

#endif /* _h_tui_ */
