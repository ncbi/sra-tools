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

#include <tui/extern.h>
#include <tui/tui.h>
#include "../tui-priv.h"
#include <klib/rc.h>
#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <assert.h>


/**********************************************************************************/

static WORD tui_color_to_win_fg( KTUI_color c)
{
    switch ( c )
    {
        case KTUI_c_light_gray      : return FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED;
        case KTUI_c_gray            : return FOREGROUND_INTENSITY;
        case KTUI_c_white           : return FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED|FOREGROUND_INTENSITY;

        case KTUI_c_dark_red        : return FOREGROUND_RED;
        case KTUI_c_red             : return FOREGROUND_RED|FOREGROUND_INTENSITY;

        case KTUI_c_dark_green      : return FOREGROUND_GREEN;
        case KTUI_c_green           : return FOREGROUND_GREEN|FOREGROUND_INTENSITY;

        case KTUI_c_brown           : return FOREGROUND_RED|FOREGROUND_GREEN;
        case KTUI_c_yellow          : return FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY;

        case KTUI_c_dark_blue       : return FOREGROUND_BLUE;
        case KTUI_c_blue            : return FOREGROUND_BLUE|FOREGROUND_INTENSITY;

        case KTUI_c_dark_magenta    : return FOREGROUND_RED|FOREGROUND_BLUE;
        case KTUI_c_magenta         : return FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_INTENSITY;

        case KTUI_c_dark_cyan       : return FOREGROUND_GREEN|FOREGROUND_BLUE;
        case KTUI_c_cyan            : return FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY;


        case KTUI_c_black           :
        default                     : return 0;
    }
}


static WORD tui_color_to_win_bg( KTUI_color c )
{
    switch ( c )
    {
        case KTUI_c_light_gray      : return BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_RED;
        case KTUI_c_gray            : return BACKGROUND_INTENSITY;
        case KTUI_c_white           : return BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_RED|BACKGROUND_INTENSITY;

        case KTUI_c_dark_red        : return BACKGROUND_RED;
        case KTUI_c_red             : return BACKGROUND_RED|BACKGROUND_INTENSITY;

        case KTUI_c_dark_green      : return BACKGROUND_GREEN;
        case KTUI_c_green           : return BACKGROUND_GREEN|BACKGROUND_INTENSITY;

        case KTUI_c_brown           : return BACKGROUND_RED|BACKGROUND_GREEN;
        case KTUI_c_yellow          : return BACKGROUND_RED|BACKGROUND_GREEN|BACKGROUND_INTENSITY;

        case KTUI_c_dark_blue       : return BACKGROUND_BLUE;
        case KTUI_c_blue            : return BACKGROUND_BLUE|BACKGROUND_INTENSITY;

        case KTUI_c_dark_magenta    : return BACKGROUND_RED|BACKGROUND_BLUE;
        case KTUI_c_magenta         : return BACKGROUND_RED|BACKGROUND_BLUE|BACKGROUND_INTENSITY;

        case KTUI_c_dark_cyan       : return BACKGROUND_GREEN|BACKGROUND_BLUE;
        case KTUI_c_cyan            : return BACKGROUND_GREEN|BACKGROUND_BLUE|BACKGROUND_INTENSITY;


        case KTUI_c_black           :
        default                     : return 0;
    }
}


static void set_tui_attrib_and_colors( HANDLE h, tui_ac * curr,
                                       KTUI_attrib attr,
                                       KTUI_color color_fg,
                                       KTUI_color color_bg )
{
    if ( curr->attr != attr || curr->fg != color_fg || curr->bg != color_bg )
    {
        bool reverse = ( attr & KTUI_a_underline ) || ( attr & KTUI_a_inverse );

        if ( reverse )
            SetConsoleTextAttribute( h,  tui_color_to_win_fg( color_bg ) | 
                                         tui_color_to_win_bg( color_fg ) );
        else
            SetConsoleTextAttribute( h,  tui_color_to_win_fg( color_fg ) | 
                                         tui_color_to_win_bg( color_bg ) );

        curr->attr = attr;
        curr->fg = color_fg;
        curr->bg = color_bg;
    }
}


void CC tui_send_strip( int x, int y, int count, tui_ac * curr, tui_ac * v,
                        const char * s )
{
    HANDLE h = GetStdHandle( STD_OUTPUT_HANDLE );
    if ( h != INVALID_HANDLE_VALUE )
    {
        DWORD nBytesWritten;
        COORD curpos = { (SHORT)x, (SHORT)y };

        set_tui_attrib_and_colors( h, curr, v->attr, v->fg, v->bg );
        SetConsoleCursorPosition( h, curpos );
        WriteConsoleA( h, s, ( DWORD )count, &nBytesWritten, NULL );
    }
}


/**********************************************************************************/


typedef struct KTUI_pf
{
    CONSOLE_SCREEN_BUFFER_INFO  sbinfo;
    CONSOLE_CURSOR_INFO         curinfo;
    DWORD                       bitsConsoleInputMode;
    int                         esc_count;

	int							prev_x, prev_y;
    KTUI_mouse_button			prev_button;
	KTUI_mouse_action			prev_action;
} KTUI_pf;


static void store_mouse_event( struct KTUI_pf * pf, int x, int y, KTUI_mouse_button b, KTUI_mouse_action a )
{
	pf -> prev_x = x;
	pf -> prev_y = y;
	pf -> prev_button = b;
	pf -> prev_action = a;
}

static bool different_mouse_event( struct KTUI_pf * pf, int x, int y, KTUI_mouse_button b, KTUI_mouse_action a )
{
	return ( pf -> prev_x != x ||
			 pf -> prev_y != y ||
			 pf -> prev_button != b ||
			 pf -> prev_action != a );
}


static KTUI_key g_VirtualCodeTable[ 256 ]; /* if max VK_* constant value is greater than 0xFF, then this code must be revised */


static void InitTableRange( int* table, size_t offset_first, size_t offset_last, int val )
{
    size_t i;
    assert( offset_first < 256 );
    assert( offset_last < 256 );

    for ( i = offset_first; i <= offset_last; ++i )
        table[ i ] = val;
}


static void InitVKTable( KTUI_key* table, size_t size )
{
    /*
        the table is supposed to be used in a "white-list approach",
        so it initializes every code that is supposed to be processed
    */
    size_t i;
    for ( i = 0; i < size; ++i )
        table[ i ] = ktui_none;

    /* special key virtual codes */
    table[VK_DOWN]   = ktui_down;
    table[VK_UP]     = ktui_up;
    table[VK_LEFT]   = ktui_left;
    table[VK_RIGHT]  = ktui_right;
    table[VK_HOME]   = ktui_home;
    table[VK_END]    = ktui_end;
    table[VK_BACK]   = ktui_bksp;
    table[VK_F1]     = ktui_F1;
    table[VK_F2]     = ktui_F2;
    table[VK_F3]     = ktui_F3;
    table[VK_F4]     = ktui_F4;
    table[VK_F5]     = ktui_F5;
    table[VK_F6]     = ktui_F6;
    table[VK_F7]     = ktui_F7;
    table[VK_F8]     = ktui_F8;
    table[VK_F9]     = ktui_F9;
    table[VK_F10]    = ktui_F10;
    table[VK_F11]    = ktui_F11;
    table[VK_F12]    = ktui_F12;
    table[VK_DELETE] = ktui_del;
    table[VK_INSERT] = ktui_ins;
    table[VK_NEXT]   = ktui_pgdn; 
    table[VK_PRIOR]  = ktui_pgup; 
    table[VK_RETURN] = ktui_enter;
    table[VK_TAB]    = ktui_tab;
    /*
        for the numpad windows reports the same ascii characters
        as for standart number keys, so treat them as ktui_alpha
    */
    table[VK_NUMPAD0]  = ktui_alpha;
    table[VK_NUMPAD1]  = ktui_alpha;
    table[VK_NUMPAD2]  = ktui_alpha;
    table[VK_NUMPAD3]  = ktui_alpha;
    table[VK_NUMPAD4]  = ktui_alpha;
    table[VK_NUMPAD5]  = ktui_alpha;
    table[VK_NUMPAD6]  = ktui_alpha;
    table[VK_NUMPAD7]  = ktui_alpha;
    table[VK_NUMPAD8]  = ktui_alpha;
    table[VK_NUMPAD9]  = ktui_alpha;
    table[VK_MULTIPLY] = ktui_alpha;
    table[VK_ADD]      = ktui_alpha;
    table[VK_SUBTRACT] = ktui_alpha;
    table[VK_DECIMAL]  = ktui_alpha;
    table[VK_DIVIDE]   = ktui_alpha;

    /* some other keys translated to ASCII */
    table[VK_SPACE]      = ktui_alpha;
    table[VK_OEM_1]      = ktui_alpha;
    table[VK_OEM_PLUS]   = ktui_alpha;
    table[VK_OEM_COMMA]  = ktui_alpha;
    table[VK_OEM_MINUS]  = ktui_alpha;
    table[VK_OEM_PERIOD] = ktui_alpha;
    table[VK_OEM_2]      = ktui_alpha;
    table[VK_OEM_3]      = ktui_alpha;
    table[VK_OEM_4]      = ktui_alpha;
    table[VK_OEM_5]      = ktui_alpha;
    table[VK_OEM_6]      = ktui_alpha;
    table[VK_OEM_7]      = ktui_alpha;
    table[VK_OEM_8]      = ktui_alpha;
    table[VK_OEM_102]    = ktui_alpha;
    table[VK_OEM_CLEAR]  = ktui_alpha;
    table[VK_ESCAPE]     = ktui_alpha;

    /* keys */
    InitTableRange( table, 0x30, 0x39, ktui_alpha );
    InitTableRange( table, 0x41, 0x5A, ktui_alpha );
}


static KTUI_key TranslateVKtoKTUI( WORD wVirtualKeyCode )
{
	KTUI_key res;
    if ( wVirtualKeyCode < _countof( g_VirtualCodeTable ) )
        res = g_VirtualCodeTable[ wVirtualKeyCode ];
    else
        res = ktui_none;
	return res;
}


static void save_current_console_settings( HANDLE hStdOut, HANDLE hStdIn, KTUI_pf * pWinSettings, KTUI * pCommonSettings )
{
    GetConsoleScreenBufferInfo( hStdOut, &pWinSettings->sbinfo );
    GetConsoleCursorInfo( hStdOut, &pWinSettings->curinfo );
    GetConsoleMode( hStdIn, &pWinSettings->bitsConsoleInputMode );

    pCommonSettings->lines = ( pWinSettings->sbinfo.srWindow.Bottom - pWinSettings->sbinfo.srWindow.Top );
    pCommonSettings->cols  = ( pWinSettings->sbinfo.srWindow.Right - pWinSettings->sbinfo.srWindow.Left );
}


static void set_tui_settings( HANDLE hStdOut, HANDLE hStdIn, KTUI_pf const * pWinSettings )
{
    DWORD bitsMode = pWinSettings->bitsConsoleInputMode; /* use mostly default windows settings */
    CONSOLE_CURSOR_INFO curinfo = { 1, FALSE };

    bitsMode &= ~ENABLE_ECHO_INPUT;      /* disable echo */
    bitsMode &= ~ENABLE_LINE_INPUT;      /* something like raw mode? TODO: ask Wolfgang */
    bitsMode &= ~ENABLE_PROCESSED_INPUT; /* capture Ctrl-C by application rather than system, shold be reset along with ENABLE_LINE_INPUT */

    bitsMode |= ENABLE_MOUSE_INPUT;      /* explicitly enabling mouse for the case when it was disabled in windows */
    /*bitsMode |= ENABLE_QUICK_EDIT_MODE;  /* explicitly enabling user to use the mouse for text selection and editing*/
    bitsMode |= ENABLE_WINDOW_INPUT;     /* process console screen buffer changes (?)*/

    SetConsoleMode( hStdIn, bitsMode );
    SetConsoleCursorInfo( hStdOut, &curinfo );   /* cursor off */
}


static void restore_console_settings( HANDLE hStdOut, HANDLE hStdIn, KTUI_pf const * pWinSettings )
{
    SetConsoleMode( hStdIn, pWinSettings->bitsConsoleInputMode );

    SetConsoleCursorPosition( hStdOut, pWinSettings->sbinfo.dwCursorPosition );
    SetConsoleCursorInfo( hStdOut, &pWinSettings->curinfo );
    SetConsoleTextAttribute( hStdOut, pWinSettings->sbinfo.wAttributes );
    SetConsoleMode( hStdIn, pWinSettings->bitsConsoleInputMode );
}


/* This is from MSDN: http://msdn.microsoft.com/en-us/library/windows/desktop/ms682022%28v=vs.85%29.aspx */
void cls( HANDLE hConsole )
{
   COORD coordScreen = { 0, 0 };    // home for the cursor 
   DWORD cCharsWritten;
   CONSOLE_SCREEN_BUFFER_INFO csbi; 
   DWORD dwConSize;

/* Get the number of character cells in the current buffer. */

   if( !GetConsoleScreenBufferInfo( hConsole, &csbi ))
   {
      return;
   }

   dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

   /* Fill the entire screen with blanks. */

   if( !FillConsoleOutputCharacter( hConsole,        /* Handle to console screen buffer */
                                    (TCHAR) ' ',     /* Character to write to the buffer */
                                    dwConSize,       /* Number of cells to write */
                                    coordScreen,     /* Coordinates of first cell */
                                    &cCharsWritten ))/* Receive number of characters written */
   {
      return;
   }

   /* Get the current text attribute. */

   if( !GetConsoleScreenBufferInfo( hConsole, &csbi ))
   {
      return;
   }

   /* Set the buffer's attributes accordingly. */

   if( !FillConsoleOutputAttribute( hConsole,         /* Handle to console screen buffer */
                                    csbi.wAttributes, /* Character attributes to use */
                                    dwConSize,        /* Number of cells to set attribute */
                                    coordScreen,      /* Coordinates of first cell */
                                    &cCharsWritten )) /* Receive number of characters written */
   {
      return;
   }

   /* Put the cursor at its home coordinates. */

   SetConsoleCursorPosition( hConsole, coordScreen );
}


rc_t CC KTUI_Init_platform( KTUI * self )
{
    HANDLE hStdOut = INVALID_HANDLE_VALUE;
    HANDLE hStdIn  = INVALID_HANDLE_VALUE;
    rc_t rc = 0;
    struct KTUI_pf * pf = malloc( sizeof(*pf) );
    if ( pf == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcMemory, rcExhausted );
    else
    {
        memset( pf, 0, sizeof( *pf ) );
        hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );
        hStdIn  = GetStdHandle( STD_INPUT_HANDLE );
        if ( hStdOut != INVALID_HANDLE_VALUE && hStdIn != INVALID_HANDLE_VALUE )
        {
            save_current_console_settings( hStdOut, hStdIn, pf, self );
            set_tui_settings( hStdOut, hStdIn, pf );
        }

        self->pf = pf;
        InitVKTable( g_VirtualCodeTable, _countof( g_VirtualCodeTable ) );
		store_mouse_event( pf, 0, 0, ktui_mouse_button_none, ktui_mouse_action_none );
    }
    return rc;
}


rc_t CC KTUI_Destroy_platform ( struct KTUI_pf * pf )
{
    HANDLE hStdOut = INVALID_HANDLE_VALUE;
    HANDLE hStdIn  = INVALID_HANDLE_VALUE;

    if ( pf != NULL )
    {
        hStdOut = GetStdHandle( STD_OUTPUT_HANDLE );
        hStdIn  = GetStdHandle( STD_INPUT_HANDLE );
        if ( hStdOut != INVALID_HANDLE_VALUE && hStdIn != INVALID_HANDLE_VALUE )
        {
            restore_console_settings( hStdOut, hStdIn, pf );
            cls( hStdOut );
        }
        free( ( void * ) pf );
    }

    return 0;
}


static BOOL KeyEventProc( KEY_EVENT_RECORD const* pEvent, struct KTUI * self )
{
    BOOL ret = FALSE;
    if ( pEvent->bKeyDown )
    {
        struct KTUI_pf * pf = ( self->pf );
        int key = 0;

        KTUI_key code = TranslateVKtoKTUI( pEvent->wVirtualKeyCode );

        if ( code == ktui_alpha )
        {
            key = ( int )( pEvent->uChar.AsciiChar );

            /* we artificially let the user press ESC twice to make it do the same as a posix console */
            if ( key == 27 )
            {
                if ( pf->esc_count == 0 )
                {
                    code = ktui_none;
                    pf->esc_count = 1;
                }
                else
                    pf->esc_count = 0;
            }
            else
                pf->esc_count = 0;
        }
        else
            pf->esc_count = 0;

        if ( code == ktui_tab && ( ( pEvent->dwControlKeyState & SHIFT_PRESSED ) == SHIFT_PRESSED ) )
            code = ktui_shift_tab;
        
        if ( code != ktui_none )
        {
            size_t i;
            for ( i = 0; i < pEvent->wRepeatCount; ++i )
            {
                put_kb_event( self, key, code );
                ret = TRUE;
            }
        }
    }
    return ret;
}


static KTUI_mouse_button get_button( DWORD btn_flags )
{
	KTUI_mouse_button res = ktui_mouse_button_none;
	switch( btn_flags )
	{
        case FROM_LEFT_1ST_BUTTON_PRESSED : res = ktui_mouse_button_left; break;
        case FROM_LEFT_2ND_BUTTON_PRESSED : res = ktui_mouse_button_middle; break;
        case RIGHTMOST_BUTTON_PRESSED     : res = ktui_mouse_button_right; break;
		case 0 : res = ktui_mouse_button_up; break;
	}
	return res;
}

static KTUI_mouse_action get_action( DWORD ev_flags, KTUI_mouse_button button )
{
	KTUI_mouse_action res = ktui_mouse_action_none;

	if ( ev_flags == 0 || ( ( ev_flags & DOUBLE_CLICK ) == DOUBLE_CLICK ) )
	{
		res = ktui_mouse_action_button;
	}
	else if ( ( ev_flags & MOUSE_MOVED ) == MOUSE_MOVED )
	{
		/* to make the behavior the same as on posix:
		   if not mouse-buttons is pressed, do not report a move action... */
		if ( button != ktui_mouse_button_up )
			res = ktui_mouse_action_move;
	}
	else if ( ( ev_flags & MOUSE_WHEELED ) == MOUSE_WHEELED )
	{
		res = ktui_mouse_action_scroll;
	}
	return res;
}

static void MouseEventProc( MOUSE_EVENT_RECORD const* pEvent, struct KTUI* self )
{
    KTUI_mouse_button button = get_button( pEvent->dwButtonState );
	KTUI_mouse_action action = get_action( pEvent->dwEventFlags, button );

	if ( button != ktui_mouse_button_none && action != ktui_mouse_action_none )
	{
		int x = pEvent->dwMousePosition.X;
		int y = pEvent->dwMousePosition.Y;
		if ( different_mouse_event( self->pf, x, y, button, action ) )
		{
			put_mouse_event( self, x, y, button, action,
						    ( uint32_t )( pEvent->dwEventFlags & 0xFFFFFFFF ) );
			store_mouse_event( self->pf, x, y, button, action );
		}
	}
}


static void WindowBufferSizeEventProc( WINDOW_BUFFER_SIZE_RECORD const* pEvent, struct KTUI* self )
{
    put_window_event( self, pEvent->dwSize.Y, pEvent->dwSize.X );
}


#define INPUT_EVENT_BUF_SIZE 10

static BOOL ReadAndProcessEvents( struct KTUI * self, HANDLE h )
{
    DWORD nEventsRead;
    INPUT_RECORD arrInputEvents[ INPUT_EVENT_BUF_SIZE ];
    PINPUT_RECORD pInputEvents = arrInputEvents;
	BOOL res = ReadConsoleInput( h, pInputEvents, INPUT_EVENT_BUF_SIZE, &nEventsRead );
    if ( res )
	{
		DWORD i;
		for ( i = 0; i < nEventsRead; ++i )
		{
			switch ( pInputEvents[ i ].EventType )
			{
				case KEY_EVENT					: KeyEventProc( &pInputEvents[ i ].Event.KeyEvent, self ); break;
				case MOUSE_EVENT				: MouseEventProc( &pInputEvents[ i ].Event.MouseEvent, self ); break;
				case WINDOW_BUFFER_SIZE_EVENT	: WindowBufferSizeEventProc( &pInputEvents[ i ].Event.WindowBufferSizeEvent, self ); break;
			}
		}
    }
    return res;
}


void CC read_from_stdin( struct KTUI * self, uint32_t timeout )
{
    BOOL resEventProc;
    DWORD dwStartedWaitingTime, dwEffectiveTimeout, dwTimeElapsed, dwWaitRes;
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD const dwMinimumTimeout = 10;

    h = GetStdHandle( STD_INPUT_HANDLE );
    if ( h == INVALID_HANDLE_VALUE )
        return;

    /* blocking waiting with the timeout */
    dwEffectiveTimeout = min( ( DWORD )( timeout / 1000 ), dwMinimumTimeout );
    dwStartedWaitingTime = GetTickCount();
    dwTimeElapsed = 0;
    for ( ; ; )
    {
        dwWaitRes = WaitForSingleObject( h, dwEffectiveTimeout - dwTimeElapsed );
        if ( dwWaitRes == WAIT_OBJECT_0 )
        {
            resEventProc = ReadAndProcessEvents( self, h );
            if ( resEventProc )
                break;
            /*continue waiting for the remaining time */
            dwTimeElapsed = GetTickCount() - dwStartedWaitingTime;
            if ( dwTimeElapsed >= dwEffectiveTimeout )
                break;
        }
        else /* timeout, error */
        {
            break;
        }
    }
}
