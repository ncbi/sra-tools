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
#ifndef _h_tui_widget_
#define _h_tui_widget_

#include <tui/tui.h>
#include <tui/tui_dlg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_GRID_COLS 256
#define MAX_GRID_ROWS 128

rc_t draw_highlighted( struct KTUI * tui, uint32_t x, uint32_t y, uint32_t w,
                       tui_ac * ac, tui_ac * hl_ac, const char * caption );

rc_t DlgPaint( struct KTUI * tui, int x, int y, int w, int h, KTUI_color c );
rc_t DlgWrite( struct KTUI * tui, int x, int y, const tui_ac * ac, const char * s, uint32_t l );

rc_t DrawVScroll( struct KTUI * tui, tui_rect * r, uint64_t count, uint64_t value,
                  KTUI_color c_bar, KTUI_color c_sel );
rc_t DrawHScroll( struct KTUI * tui, tui_rect * r, uint64_t count, uint64_t value,
                  KTUI_color c_bar, KTUI_color c_sel );

rc_t draw_background( struct KTUI * tui, bool focused, tui_point * p,
                      int w, int h, KTUI_color bg );


/* ****************************************************************************************** */

typedef uint32_t KTUI_Widget_type;
enum
{
    KTUIW_label = 0,
    KTUIW_tabhdr,
    KTUIW_button,
    KTUIW_checkbox,
    KTUIW_input,
    KTUIW_radiobox,
    KTUIW_list,
    KTUIW_progress,
    KTUIW_spinedit,
    KTUIW_grid
};


struct KTUIWidget;
struct KTUIDlg;

typedef void ( * draw_cb ) ( struct KTUIWidget * w );
typedef void ( * init_cb ) ( struct KTUIWidget * w );
typedef bool ( * event_cb ) ( struct KTUIWidget * w, tui_event * event, bool hotkey );

typedef struct KTUIWidget
{
    struct KTUIDlg * dlg;   /* pointer to dialog they belong to */
    struct KTUI * tui;      /* pointer to the hw-specific layer */
    struct KTUIPalette * palette;   /* where the colors come from */
    uint32_t id;            /* a unique id... */
    uint32_t page_id;       /* the page of the parent the widget belongs to */
    KTUI_Widget_type wtype; /* what type of widget it is... */
    uint32_t vector_idx;    /* where in the vector it is inserted */
    tui_rect r;             /*  - a rect where it appears */

    KTUI_color bg_override; /* the color if the background is different from palette */
    bool bg_override_flag;

    KTUI_color fg_override; /* the color if the forground is different from palette */
    bool fg_override_flag;

    char * caption;         /*  - a caption */

    draw_cb on_draw;        /* know how to draw itself */
    event_cb on_event;      /* know how to handle events */
    uint64_t ints[ 8 ];     /* 8 internal 'state' integers */

    bool can_focus;         /* can it be focused */
    bool focused;           /* does it have focus... */
    bool visible;           /* is it visible */
    bool changed;           /* has the widget changed */

    /* different value types, according to the widget type one is used: */

    /* in case of a checkbox */
    bool bool_value;

    /* in case of a spin-edit */
    int64_t int64_value;
    int64_t int64_min;
    int64_t int64_max;

    /* in case of a percent-bar */
    int32_t percent;
    int32_t precision;

    /* for the input-string */
    char * txt;
    size_t txt_length;

    /* for Radio-Box / ListBox */
    VNamelist * strings;
    uint64_t selected_string;

    /* for Grid */
    TUIWGrid_data * grid_data;

} KTUIWidget;


rc_t TUI_MakeWidget ( KTUIWidget ** self, struct KTUIDlg * dlg, uint32_t id, KTUI_Widget_type wtype,
                      const tui_rect * r, draw_cb on_draw, event_cb on_event );

void TUI_DestroyWidget( struct KTUIWidget * self );

const tui_ac * GetWidgetPaletteEntry ( struct KTUIWidget * self, KTUIPa_entry what );
struct KTUIPalette * CopyWidgetPalette( struct KTUIWidget * self );
rc_t  ReleaseWidgetPalette( struct KTUIWidget * self );

rc_t RedrawWidget( KTUIWidget * w );

rc_t RedrawWidgetAndPushEvent( KTUIWidget * w,
           KTUIDlg_event_type ev_type, uint64_t value_1, uint64_t value_2, void * ptr_0 );

rc_t GetWidgetRect ( struct KTUIWidget * self, tui_rect * r );
rc_t SetWidgetRect ( struct KTUIWidget * self, const tui_rect * r, bool redraw );

rc_t GetWidgetPageId ( struct KTUIWidget * self, uint32_t * id );
rc_t SetWidgetPageId ( struct KTUIWidget * self, uint32_t id );

rc_t SetWidgetCanFocus( struct KTUIWidget * self, bool can_focus );
rc_t GetWidgetAc( struct KTUIWidget * self, KTUIPa_entry pa_entry, tui_ac * ac );
rc_t GetWidgetHlAc( struct KTUIWidget * self, KTUIPa_entry pa_entry, tui_ac * ac );

const char * GetWidgetCaption ( struct KTUIWidget * self );
rc_t SetWidgetCaption ( struct KTUIWidget * self, const char * caption );

bool GetWidgetVisible ( struct KTUIWidget * self );
rc_t SetWidgetVisible ( struct KTUIWidget * self, bool visible, bool * redraw );

bool GetWidgetChanged ( struct KTUIWidget * self );
rc_t SetWidgetChanged ( struct KTUIWidget * self, bool changed );

bool GetWidgetBoolValue ( struct KTUIWidget * self );
rc_t SetWidgetBoolValue ( struct KTUIWidget * self, bool value );

rc_t SetWidgetBg ( struct KTUIWidget * self, KTUI_color value );
rc_t ReleaseWidgetBg ( struct KTUIWidget * self );

rc_t SetWidgetFg ( struct KTUIWidget * self, KTUI_color value );
rc_t ReleaseWidgetFg ( struct KTUIWidget * self );

int64_t GetWidgetInt64Value ( struct KTUIWidget * self );
rc_t SetWidgetInt64Value ( struct KTUIWidget * self, int64_t value );

int64_t GetWidgetInt64Min ( struct KTUIWidget * self );
rc_t SetWidgetInt64Min ( struct KTUIWidget * self, int64_t value );

int64_t GetWidgetInt64Max ( struct KTUIWidget * self );
rc_t SetWidgetInt64Max ( struct KTUIWidget * self, int64_t value );

int32_t GetWidgetPercent ( struct KTUIWidget * self );
rc_t SetWidgetPercent ( struct KTUIWidget * self, int32_t value );

int32_t GetWidgetPrecision ( struct KTUIWidget * self );
rc_t SetWidgetPrecision ( struct KTUIWidget * self, int32_t value );

int32_t CalcWidgetPercent( int64_t value, int64_t max, uint32_t precision );

const char * GetWidgetText ( struct KTUIWidget * self );
rc_t SetWidgetText ( struct KTUIWidget * self, const char * txt );

size_t GetWidgetTextLength ( struct KTUIWidget * self );
rc_t SetWidgetTextLength ( struct KTUIWidget * self, size_t new_length );
rc_t SetWidgetCarretPos ( struct KTUIWidget * self, size_t new_pos );
rc_t SetWidgetAlphaMode ( struct KTUIWidget * self, uint32_t alpha_mode );

rc_t AddWidgetString ( struct KTUIWidget * self, const char * txt );
rc_t AddWidgetStrings ( struct KTUIWidget * self, VNamelist * src );
const char * GetWidgetStringByIdx ( struct KTUIWidget * self, uint32_t idx );
rc_t RemoveWidgetStringByIdx ( struct KTUIWidget * self, uint32_t idx );
rc_t RemoveAllWidgetStrings ( struct KTUIWidget * self );
uint32_t GetWidgetStringCount ( struct KTUIWidget * self );
uint32_t HasWidgetString( struct KTUIWidget * self, const char * txt );
uint32_t GetWidgetSelectedString( struct KTUIWidget * self );
rc_t SetWidgetSelectedString ( struct KTUIWidget * self, uint32_t selection );

TUIWGrid_data * GetWidgetGridData( struct KTUIWidget * self );
rc_t SetWidgetGridData( struct KTUIWidget * self, TUIWGrid_data * grid_data, bool cached );

#ifdef __cplusplus
}
#endif

#endif /* _h_tui_widget_ */
