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

/*--------------------------------------------------------------------------
 * KTUI = Text User Interface ( different from GUI ... )
 *  platform specific code ... this one for linux
 */

#include <tui/extern.h>
#include <tui/tui.h>
#include <klib/refcount.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <sysalloc.h>


#ifndef __USE_UNIX98
#define __USE_UNIX98 1
#endif

#include "../tui-priv.h"

#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

#include <stdlib.h>
#include <errno.h>
#include <assert.h>

typedef uint32_t es_states;
enum
{
    es_normal,
    es_ESC,

    es_ESC_91,
    es_ESC_91_49,
    es_ESC_91_49_49,
    es_ESC_91_49_50,
    es_ESC_91_49_51,
    es_ESC_91_49_52,
    es_ESC_91_49_53,
    es_ESC_91_49_55,
    es_ESC_91_49_56,
    es_ESC_91_49_57,

    es_ESC_91_50,
    es_ESC_91_50_48,
    es_ESC_91_50_49,
    es_ESC_91_50_51,
    es_ESC_91_50_52,

    es_ESC_91_51,
    es_ESC_91_52,
    es_ESC_91_53,
    es_ESC_91_54,

    es_ESC_91_77_B,
    es_ESC_91_77_X,
    es_ESC_91_77_Y,

    es_ESC_79
};


/**********************************************************************************/

static int color_2_ansi( KTUI_color c )
{
    int res = 0;
    switch( c )
    {
        case KTUI_c_black           : res = 0;  break;
        case KTUI_c_gray            : res = 8;  break;

        case KTUI_c_dark_red        : res = 1;  break;    
        case KTUI_c_red             : res = 9;  break;

        case KTUI_c_green           : res = 10;  break;
        case KTUI_c_dark_green      : res = 2; break;

        case KTUI_c_brown           : res = 3;  break;
        case KTUI_c_yellow          : res = 11; break;

        case KTUI_c_dark_blue       : res = 4;  break;
        case KTUI_c_blue            : res = 12; break;

        case KTUI_c_dark_magenta    : res = 5;  break;
        case KTUI_c_magenta         : res = 13; break;

        case KTUI_c_dark_cyan       : res = 6;  break;
        case KTUI_c_cyan            : res = 14; break;

        case KTUI_c_light_gray      : res = 7;  break;
        case KTUI_c_white           : res = 15; break;
    }
    return res;
}


/**********************************************************************************/


static void set_tui_attrib( tui_ac * curr, KTUI_attrib attr )
{
    if ( curr->attr != attr )
    {
        printf( "\033[0m" ); /* reset attribute, that means also color is default now! */
        curr->fg = -1;       /* because of that we have to force the re-emission of color */
        curr->bg = -1;

        if ( attr & KTUI_a_bold )
            printf( "\033[1m" );
        if ( attr & KTUI_a_underline )
            printf( "\033[4m" );
        if ( attr & KTUI_a_blink )
            printf( "\033[5m" );
        if ( attr & KTUI_a_inverse )
            printf( "\033[7m" );

        curr->attr = attr;
    }
}


static void set_tui_fg_color( tui_ac * curr, KTUI_color color )
{
    if ( curr->fg != color )
    {
        printf( "\033[38;5;%dm", color_2_ansi( color ) );
        curr->fg = color;
    }
}


static void set_tui_bg_color( tui_ac * curr, KTUI_color color )
{
    if ( curr->bg != color )
    {
        printf( "\033[48;5;%dm", color_2_ansi( color ) );
        curr->bg = color;
    }
}


void CC tui_send_strip( int x, int y, int count, tui_ac * curr, tui_ac * v,
                        const char * s )
{
    set_tui_attrib( curr, v->attr );
    set_tui_fg_color( curr, v->fg );
    set_tui_bg_color( curr, v->bg );
    printf( "\033[%d;%dH%.*s",  y + 1, x + 1, count, s );
    fflush( stdout );
}


/**********************************************************************************/

typedef struct KTUI_pf
{
/*    struct termios stored_settings; */
    struct termios stored_settings;
    struct sigaction sa_saved;
    es_states es;
    unsigned int mouse_event, mouse_x;
} KTUI_pf;


static struct KTUI * sig_self = NULL;


static void get_lines_cols( int * cols, int * lines )
{
    struct winsize ws;
    ioctl( STDIN_FILENO, TIOCGWINSZ, &ws );
    if ( lines != NULL )
        *lines = ws.ws_row;
    if ( cols != NULL )
        *cols = ws.ws_col;
}


static void sigwinchHandler( int sig )
{
    if ( sig_self != NULL )
    {
        get_lines_cols( &sig_self->cols, &sig_self->lines );
        put_window_event( sig_self, sig_self->cols, sig_self->lines );
    }
}


static void set_kb_raw_mode( struct termios * stored_settings )
{
    struct termios new_settings;

    /* ioctl( STDIN_FILENO, TCGETA, stored_settings ); */
    tcgetattr( STDIN_FILENO, stored_settings );    
    memmove ( &new_settings, stored_settings, sizeof new_settings );

    new_settings.c_lflag &= ( ~ICANON );    /* exit canonical mode, enter raw mode */
    new_settings.c_lflag &= ( ~ECHO );      /* don't echo the character */
    new_settings.c_lflag &= ( ~IEXTEN );    /* don't enable extended input character processing */
    new_settings.c_lflag &= ( ~ISIG );      /* don't automatically handle control-C */
    new_settings.c_cc[ VTIME ] = 1;         /* timeout (tenths of a second) */
    new_settings.c_cc[ VMIN ] = 0;          /* minimum number of characters */
    
    /* apply the new settings */
    /* ioctl( STDIN_FILENO, TCSETA, &new_settings ); */
    tcsetattr( STDIN_FILENO, TCSANOW, &new_settings );
}


static void restore_kb_mode( const struct termios * stored_settings )
{
    /* applies the terminal settings supplied as the argument */
    /* ioctl( STDIN_FILENO, TCSETA, stored_settings ); */
    tcsetattr( STDIN_FILENO, TCSANOW, stored_settings );
}


rc_t CC KTUI_Init_platform( KTUI * self )
{
    rc_t rc = 0;
    struct KTUI_pf * pf = malloc( sizeof * pf );
    if ( pf == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcMemory, rcExhausted );
    else
    {
        struct sigaction sa_new;

        set_kb_raw_mode( &pf->stored_settings );
        sa_new.sa_flags = 0;
        sa_new.sa_handler = sigwinchHandler;
        sigaction( SIGWINCH, &sa_new, &pf->sa_saved );
        get_lines_cols( &self->cols, &self->lines );
        pf->es = es_normal;

        printf( "\033[2J" );        /* clrscr */
        printf( "\033[?25l" );      /* cursor off */

        /* printf( "\033[?9h" );     mouse tracking on ( SET_X10_MOUSE ): mouse-down events, does not work with putty */
		
		printf( "\033[?1000h" );     /* mouse tracking on ( SET_VT200_MOUSE ): mouse-up/down events, does work with putty */
		printf( "\033[?1002h" );     /* mouse tracking on ( SET_BTN_EVENT_MOUSE ): mouse-up/down/move events, does work with putty */
		/*printf( "\033[?1003h" );   mouse tracking on ( SET_ANY_EVENT_MOUSE ): ??? */
		
        fflush( stdout );

        sig_self = self;
        self->pf = pf;
    }
    return rc;
}


rc_t CC KTUI_Destroy_platform ( struct KTUI_pf * pf )
{
    if ( pf != NULL )
    {
        restore_kb_mode( &pf->stored_settings );
        sigaction( SIGWINCH, &pf->sa_saved, NULL );
        free( ( void * ) pf );
    }

    sig_self = NULL;
    printf( "\033[0;39;49m" );  /* reset colors */
    printf( "\033[H" );         /* home */
    printf( "\033[2J" );        /* clrscr */
    printf( "\033[?25h" );      /* cursor off */

    /*printf( "\033[?9l" );       mouse tracking X10-style off */
	printf( "\033[?1000l" );       /* mouse tracking off */	
	printf( "\033[?1002l" );       /* mouse tracking off */		
	/*printf( "\033[?1003l" );    mouse tracking off */	
    fflush( stdout );

    return 0;
}


static void put_kb_event_u( struct KTUI * self, int key, KTUI_key code )
{
    if ( self->pf != NULL ) self->pf->es = es_normal;
    put_kb_event( self, key, code );
}


static void put_kb_alpha( struct KTUI * self, int key )
{
    put_kb_event_u( self, key, ktui_alpha );
}

static void put_kb_alpha_3( struct KTUI * self, int key1, int key2, int key3 )
{
    put_kb_alpha( self, key1 );
    put_kb_alpha( self, key2 );
    put_kb_alpha( self, key3 );
}

static void put_kb_alpha_4( struct KTUI * self, int key1, int key2, int key3, int key4 )
{
    put_kb_alpha( self, key1 );
    put_kb_alpha( self, key2 );
    put_kb_alpha( self, key3 );
    put_kb_alpha( self, key4 );
}

static void put_kb_alpha_5( struct KTUI * self, int key1, int key2, int key3, int key4, int key5 )
{
    put_kb_alpha( self, key1 );
    put_kb_alpha( self, key2 );
    put_kb_alpha( self, key3 );
    put_kb_alpha( self, key4 );
    put_kb_alpha( self, key5 );
}


/* -------------------------------------------------------------------------------------

mouse_event	7 6 5 4 3 2 1 0
						B B
						0 0 ... left mouse button
						0 1 ... middle mouse button
						1 0 ... right mouse button
						1 1 ... button up ( don't know which one )
                      S ....... status of Shift key						0x04
                    M ......... status of Meta key ( Alt )				0x08
                  C ........... status of Ctrl key						0x10 (16)
			  0 0 ............. unknown									0x00
              0 1 ............. button event ( down, up )				0x20
              1 0 ............. move event ( + button )					0x40
              1 1 ............. scroll event ( + button )				0x60
			  
------------------------------------------------------------------------------------- */
static void put_mouse_event_u( struct KTUI * self, unsigned int y )
{
    if ( self->pf != NULL )
    {
		unsigned int ev = self->pf->mouse_event;
		KTUI_mouse_button b = ktui_mouse_button_none;
		KTUI_mouse_action a = ktui_mouse_action_none;

		switch( ev & 0x03 )
		{
			case 0x00 : b = ktui_mouse_button_left; break;
			case 0x01 : b = ktui_mouse_button_middle; break;
			case 0x02 : b = ktui_mouse_button_right; break;
			case 0x03 : b = ktui_mouse_button_up; break;
		}
		
		switch ( ev & 0x60 )
		{
			case 0x20 : a = ktui_mouse_action_button; break;
			case 0x40 : a = ktui_mouse_action_move; break;
			case 0x60 : a = ktui_mouse_action_scroll; break;
		}

		put_mouse_event( self, self->pf->mouse_x, y, b, a, ev );
        self->pf->es = es_normal;
    }
}

static void statemachine( struct KTUI * self, unsigned int x )
{
    if ( self->pf != NULL ) switch( self->pf->es )
    {
        case es_normal : switch( x )
                        {
                            case 10 : put_kb_event_u( self, x, ktui_enter ); break;
                            case 8  : put_kb_event_u( self, x, ktui_bksp ); break;
                            case 9  : put_kb_event_u( self, x, ktui_tab ); break;
                            case 127 : put_kb_event_u( self, x, ktui_bksp ); break;
                            case 27 : self->pf->es = es_ESC; break;
                            default : put_kb_alpha( self, x ); break;
                        } break;

        case es_ESC : switch( x )
                        {
                            case 91 : self->pf->es = es_ESC_91; break;
                            case 79 : self->pf->es = es_ESC_79; break;
                            default : put_kb_alpha( self, x ); break;
                        } break;

        case es_ESC_91 : switch( x )
                        {
                            case 65 : put_kb_event_u( self, x, ktui_up ); break;
                            case 66 : put_kb_event_u( self, x, ktui_down ); break;
                            case 67 : put_kb_event_u( self, x, ktui_right ); break;
                            case 68 : put_kb_event_u( self, x, ktui_left ); break;
                            case 69 : put_kb_alpha( self, '5' ); break;
                            case 70 : put_kb_event_u( self, x, ktui_end ); break;
                            case 72 : put_kb_event_u( self, x, ktui_home ); break;

                            case 49 : self->pf->es = es_ESC_91_49; break;
                            case 50 : self->pf->es = es_ESC_91_50; break;
                            case 51 : self->pf->es = es_ESC_91_51; break;
                            case 52 : self->pf->es = es_ESC_91_52; break;
                            case 53 : self->pf->es = es_ESC_91_53; break;
                            case 54 : self->pf->es = es_ESC_91_54; break;

                            case 77 : self->pf->es = es_ESC_91_77_B; break;   /* mouse reporting */

                            case 90 : put_kb_event_u( self, x, ktui_shift_tab ); break; /* shift tab */

                            default : put_kb_alpha_3( self, 27, 91, x ); break;
                        } break;

        case es_ESC_91_49 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_home ); break;
                            case 49  : self->pf->es = es_ESC_91_49_49; break;
                            case 50  : self->pf->es = es_ESC_91_49_50; break;
                            case 51  : self->pf->es = es_ESC_91_49_51; break;
                            case 52  : self->pf->es = es_ESC_91_49_52; break;
                            case 53  : self->pf->es = es_ESC_91_49_53; break;
                            case 55  : self->pf->es = es_ESC_91_49_55; break;
                            case 56  : self->pf->es = es_ESC_91_49_56; break;
                            case 57  : self->pf->es = es_ESC_91_49_57; break;
                            default : put_kb_alpha_4( self, 27, 91, 49, x ); break;
                        } break;

        case es_ESC_91_49_49 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F1 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 49, x ); break;
                        } break;

        case es_ESC_91_49_50 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F2 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 50, x ); break;
                        } break;

        case es_ESC_91_49_51 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F3 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 51, x ); break;
                        } break;

        case es_ESC_91_49_52 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F4 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 52, x ); break;
                        } break;

        case es_ESC_91_49_53 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F5 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 53, x ); break;
                        } break;

        case es_ESC_91_49_55 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F6 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 55, x ); break;
                        } break;

        case es_ESC_91_49_56 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F7 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 56, x ); break;
                        } break;

        case es_ESC_91_49_57 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F8 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 49, 57, x ); break;
                        } break;

        case es_ESC_91_50 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_ins ); break;
                            case 48 : self->pf->es = es_ESC_91_50_48; break;
                            case 49 : self->pf->es = es_ESC_91_50_49; break;
                            case 51 : self->pf->es = es_ESC_91_50_51; break;
                            case 52 : self->pf->es = es_ESC_91_50_52; break;
                            default : put_kb_alpha_4( self, 27, 91, 50, x ); break;
                        } break;

        case es_ESC_91_50_48 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F9 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 50, 48, x ); break;
                        } break;

        case es_ESC_91_50_49 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F10 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 50, 49, x ); break;
                        } break;

        case es_ESC_91_50_51 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F11 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 50, 51, x ); break;
                        } break;

        case es_ESC_91_50_52 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_F12 ); break;
                            default : put_kb_alpha_5( self, 27, 91, 50, 52, x ); break;
                        } break;

        case es_ESC_91_51 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_del ); break;
                            default : put_kb_alpha_4( self, 27, 91, 51, x ); break;
                        } break;

        case es_ESC_91_52 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_end ); break;
                            default : put_kb_alpha_4( self, 27, 91, 52, x ); break;
                        } break;

        case es_ESC_91_53 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_pgup ); break;
                            default : put_kb_alpha_4( self, 27, 91, 53, x ); break;
                        } break;

        case es_ESC_91_54 : switch( x )
                        {
                            case 126 : put_kb_event_u( self, x, ktui_pgdn ); break;
                            default : put_kb_alpha_4( self, 27, 91, 54, x ); break;
                        } break;

        case es_ESC_79 : switch( x )
                        {
                            case 80 : put_kb_event_u( self, x, ktui_F1 ); break;
                            case 81 : put_kb_event_u( self, x, ktui_F2 ); break;
                            case 82 : put_kb_event_u( self, x, ktui_F3 ); break;
                            case 83 : put_kb_event_u( self, x, ktui_F4 ); break;
                            case 77 : put_kb_event_u( self, x, ktui_enter ); break;
                            case 65 : put_kb_event_u( self, x, ktui_up ); break;
                            case 66 : put_kb_event_u( self, x, ktui_down ); break;
                            case 67 : put_kb_event_u( self, x, ktui_right ); break;
                            case 68 : put_kb_event_u( self, x, ktui_left ); break;

                            case 70 : put_kb_event_u( self, x, ktui_end ); break;
                            case 72 : put_kb_event_u( self, x, ktui_home ); break;

                            case 119 : put_kb_alpha( self, '7' ); break;
                            case 120 : put_kb_alpha( self, '8' ); break;
                            case 121 : put_kb_alpha( self, '9' ); break;
                            case 116 : put_kb_alpha( self, '4' ); break;
                            case 117 : put_kb_alpha( self, '5' ); break;
                            case 118 : put_kb_alpha( self, '6' ); break;
                            case 113 : put_kb_alpha( self, '1' ); break;
                            case 114 : put_kb_alpha( self, '2' ); break;
                            case 115 : put_kb_alpha( self, '3' ); break;
                            case 112 : put_kb_alpha( self, '0' ); break;
                            case 110 : put_kb_alpha( self, '.' ); break;

                            default : put_kb_alpha_3( self, 27, 79, x ); break;
                        } break;

        case es_ESC_91_77_B :   self->pf->mouse_event = x;
                                self->pf->es = es_ESC_91_77_X;
                                break;

        case es_ESC_91_77_X :   self->pf->mouse_x = x - 33;
                                self->pf->es = es_ESC_91_77_Y;
                                break;

        case es_ESC_91_77_Y :  put_mouse_event_u( self, x - 33 ); break;

        default : self->pf->es = es_normal; break;
    }

}

void CC read_from_stdin( struct KTUI * self, uint32_t timeout )
{
    fd_set rfds;
    struct timeval tv;
    int select_res;

    FD_ZERO( &rfds );
    FD_SET ( STDIN_FILENO, &rfds );
    tv.tv_sec  = 0;
    tv.tv_usec = timeout;

    select_res = select( 1, &rfds, NULL, NULL, &tv );
    if ( select_res < 0 )
    {
        /* error ... */
    }
    else if ( select_res == 0 )
    {
        /* no input during timeout... */
    }
    else
    {
        unsigned char in_buffer[ 32 ];
        int i, n = read( STDIN_FILENO, in_buffer, sizeof in_buffer );
        for ( i = 0; i < n; ++i )
        {
            unsigned int x = ( unsigned int ) in_buffer[ i ];
            statemachine( self, x );
        }
    }
}
