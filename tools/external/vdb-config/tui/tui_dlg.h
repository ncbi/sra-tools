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

#ifndef _h_tui_dlg_
#define _h_tui_dlg_

#ifndef _h_tui_extern_
#include <tui/extern.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#ifndef _h_tui_
#include <tui/tui.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * KTUIPalette
 */
typedef uint32_t KTUIPa_entry;
enum
{
    ktuipa_dlg = 1,
    ktuipa_dlg_caption,
    ktuipa_dlg_focus,

    ktuipa_label,
    ktuipa_button,
    ktuipa_checkbox,
    ktuipa_input,
    ktuipa_input_hint,
    ktuipa_radiobox,
    ktuipa_list,
    ktuipa_progress,
    ktuipa_spinedit,
    ktuipa_grid,
    ktuipa_grid_col_hdr,
    ktuipa_grid_row_hdr,
    ktuipa_grid_cursor,

    ktuipa_menu,
    ktuipa_menu_sel,
    ktuipa_menu_hi,

    ktuipa_hl,  /* for the high-light */
    
    ktuipa_last
};


struct KTUIPalette;

TUI_EXTERN rc_t CC KTUIPaletteMake ( struct KTUIPalette ** self );
TUI_EXTERN rc_t CC KTUIPaletteAddRef ( const struct KTUIPalette * self );
TUI_EXTERN rc_t CC KTUIPaletteRelease ( const struct KTUIPalette * self );

TUI_EXTERN const tui_ac * CC KTUIPaletteGet ( const struct KTUIPalette * self, KTUIPa_entry what );
TUI_EXTERN rc_t CC KTUIPaletteSet ( struct KTUIPalette * self, KTUIPa_entry what, const tui_ac * ac );
TUI_EXTERN rc_t CC KTUIPaletteDefault ( struct KTUIPalette * self );
TUI_EXTERN rc_t CC KTUIPaletteCopy ( struct KTUIPalette * dst, const struct KTUIPalette * src );

LIB_EXPORT rc_t CC KTUIPaletteSet_fg ( struct KTUIPalette * self, KTUIPa_entry what, KTUI_color fg );
LIB_EXPORT rc_t CC KTUIPaletteSet_bg ( struct KTUIPalette * self, KTUIPa_entry what, KTUI_color bg );

/*--------------------------------------------------------------------------
 * The Menu of a Dialog
 */
struct KTUI_Menu;

TUI_EXTERN rc_t CC KTUI_Menu_Make ( struct KTUI_Menu ** self );
TUI_EXTERN rc_t CC KTUI_Menu_AddRef ( const struct KTUI_Menu * self );
TUI_EXTERN rc_t CC KTUI_Menu_Release ( struct KTUI_Menu * self );

TUI_EXTERN rc_t CC KTUI_Menu_Add ( struct KTUI_Menu * self, const char * path, uint32_t cmd, char shortcut );
TUI_EXTERN rc_t CC KTUI_Menu_Remove ( struct KTUI_Menu * self, uint32_t cmd );


/*--------------------------------------------------------------------------
 * KTUIDlg_event_type
 */
typedef uint32_t KTUIDlg_event_type;
enum
{
    ktuidlg_event_none = 0,
    ktuidlg_event_focus,
    ktuidlg_event_focus_lost,
    ktuidlg_event_select,
    ktuidlg_event_changed,
    ktuidlg_event_menu
};


/*--------------------------------------------------------------------------
 * KTUIDlg_event
 */
typedef struct tuidlg_event
{
    SLNode node;                        /* needed internally */
    KTUIDlg_event_type event_type;      /* what has happened */
    uint32_t widget_id;                 /* which widget did it happen to */
    uint64_t value_1;                   /* what was selected in case of ktuidlg_event_select */
    uint64_t value_2;                   /* what was selected in case of ktuidlg_event_select */
    void * ptr_0;                       /* depends on the widget type... */
} tuidlg_event;


/*--------------------------------------------------------------------------
 * KTUIDlg
 *  a handle to a TUI-Dialog
 */
struct KTUIDlg;

TUI_EXTERN rc_t CC KTUIDlgMake ( struct KTUI * tui, struct KTUIDlg ** self, struct KTUIDlg * parent,
                   struct KTUIPalette * palette, tui_rect * r );
TUI_EXTERN rc_t CC KTUIDlgAddRef ( const struct KTUIDlg * self );
TUI_EXTERN rc_t CC KTUIDlgRelease ( const struct KTUIDlg * self );

TUI_EXTERN struct KTUI * CC KTUIDlgGetTui ( struct KTUIDlg * self );
TUI_EXTERN struct KTUIPalette * CC KTUIDlgGetPalette ( struct KTUIDlg * self );
TUI_EXTERN struct KTUIDlg * CC KTUIDlgGetParent ( struct KTUIDlg * self );

TUI_EXTERN rc_t CC KTUIDlgAbsoluteRect ( struct KTUIDlg * self, tui_rect * dst, tui_rect * src );
TUI_EXTERN rc_t CC KTUIDlgSetCaption ( struct KTUIDlg * self, const char * caption );

TUI_EXTERN void CC KTUIDlgSetData ( struct KTUIDlg * self, void * data );
TUI_EXTERN void * CC KTUIDlgGetData ( struct KTUIDlg * self );

TUI_EXTERN void CC KTUIDlgSetDone ( struct KTUIDlg * self, bool done );
TUI_EXTERN bool CC KTUIDlgGetDone ( struct KTUIDlg * self );

TUI_EXTERN void CC KTUIDlgSetChanged ( struct KTUIDlg * self );
TUI_EXTERN void CC KTUIDlgClearChanged ( struct KTUIDlg * self );
TUI_EXTERN bool CC KTUIDlgGetChanged ( struct KTUIDlg * self );

TUI_EXTERN rc_t CC KTUIDlgGetRect ( struct KTUIDlg * self, tui_rect * r );
TUI_EXTERN rc_t CC KTUIDlgSetRect ( struct KTUIDlg * self, const tui_rect * r, bool redraw );

TUI_EXTERN rc_t CC KTUIDlgGetActivePage ( struct KTUIDlg * self, uint32_t * page_id );
TUI_EXTERN rc_t CC KTUIDlgSetActivePage ( struct KTUIDlg * self, uint32_t page_id );

TUI_EXTERN bool CC KTUIDlgGetMenuActive ( const struct KTUIDlg * self );
TUI_EXTERN rc_t CC KTUIDlgSetMenuActive ( struct KTUIDlg * self, bool active );
TUI_EXTERN rc_t CC KTUIDlgSetMenu ( struct KTUIDlg * self, struct KTUI_Menu * menu );

TUI_EXTERN const char * CC KTUIDlgGetWidgetCaption ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetCaption ( struct KTUIDlg * self, uint32_t id, const char * caption );

TUI_EXTERN struct KTUIPalette * CC KTUIDlgNewWidgetPalette ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgReleaseWidgetPalette ( struct KTUIDlg * self, uint32_t id );

TUI_EXTERN rc_t CC KTUIDlgGetWidgetRect ( struct KTUIDlg * self, uint32_t id, tui_rect * r );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetRect ( struct KTUIDlg * self, uint32_t id, const tui_rect * r, bool redraw );

TUI_EXTERN rc_t CC KTUIDlgGetWidgetPageId ( struct KTUIDlg * self, uint32_t id, uint32_t * page_id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetPageId ( struct KTUIDlg * self, uint32_t id, uint32_t page_id );

TUI_EXTERN rc_t CC KTUIDlgSetWidgetCanFocus ( struct KTUIDlg * self, uint32_t id, bool can_focus );

TUI_EXTERN bool CC KTUIDlgGetWidgetVisible ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetVisible ( struct KTUIDlg * self, uint32_t id, bool visible );

TUI_EXTERN bool CC KTUIDlgGetWidgetChanged ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetChanged ( struct KTUIDlg * self, uint32_t id, bool changed );

TUI_EXTERN bool CC KTUIDlgGetWidgetBoolValue ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetBoolValue ( struct KTUIDlg * self, uint32_t id, bool value );

TUI_EXTERN rc_t CC KTUIDlgSetWidgetBg ( struct KTUIDlg * self, uint32_t id, KTUI_color value );
TUI_EXTERN rc_t CC KTUIDlgReleaseWidgetBg ( struct KTUIDlg * self, uint32_t id );

TUI_EXTERN rc_t CC KTUIDlgSetWidgetFg ( struct KTUIDlg * self, uint32_t id, KTUI_color value );
TUI_EXTERN rc_t CC KTUIDlgReleaseWidgetFg ( struct KTUIDlg * self, uint32_t id );

TUI_EXTERN int64_t CC KTUIDlgGetWidgetInt64Value ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetInt64Value ( struct KTUIDlg * self, uint32_t id, int64_t value );

TUI_EXTERN int64_t CC KTUIDlgGetWidgetInt64Min ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetInt64Min ( struct KTUIDlg * self, uint32_t id, int64_t value );

TUI_EXTERN int64_t CC KTUIDlgGetWidgetInt64Max ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetInt64Max ( struct KTUIDlg * self, uint32_t id, int64_t value );

TUI_EXTERN int32_t CC KTUIDlgGetWidgetPercent ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetPercent ( struct KTUIDlg * self, uint32_t id, int32_t value );
TUI_EXTERN int32_t CC KTUIDlgGetWidgetPrecision ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetPrecision( struct KTUIDlg * self, uint32_t id, int32_t value );
TUI_EXTERN int32_t CC KTUIDlgCalcPercent ( int64_t value, int64_t max, uint32_t precision );

TUI_EXTERN const char * CC KTUIDlgGetWidgetText( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetText ( struct KTUIDlg * self, uint32_t id, const char * value );
TUI_EXTERN size_t CC KTUIDlgGetWidgetTextLength( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetTextLength ( struct KTUIDlg * self, uint32_t id, size_t value );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetCarretPos ( struct KTUIDlg * self, uint32_t id, size_t value );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetAlphaMode ( struct KTUIDlg * self, uint32_t id, uint32_t value );

TUI_EXTERN rc_t CC KTUIDlgAddWidgetString ( struct KTUIDlg * self, uint32_t id, const char * txt );
TUI_EXTERN rc_t CC KTUIDlgAddWidgetStrings ( struct KTUIDlg * self, uint32_t id, VNamelist * src );
TUI_EXTERN const char * CC KTUIDlgGetWidgetStringByIdx ( struct KTUIDlg * self, uint32_t id, uint32_t idx );
TUI_EXTERN rc_t CC KTUIDlgRemoveWidgetStringByIdx ( struct KTUIDlg * self, uint32_t id, uint32_t idx );
TUI_EXTERN rc_t CC KTUIDlgRemoveAllWidgetStrings ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN uint32_t CC KTUIDlgGetWidgetStringCount ( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN uint32_t CC KTUIDlgHasWidgetString( struct KTUIDlg * self, uint32_t id, const char * txt );
TUI_EXTERN uint32_t CC KTUIDlgGetWidgetSelectedString( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetSelectedString ( struct KTUIDlg * self, uint32_t id, uint32_t selection );

TUI_EXTERN rc_t CC KTUIDlgAddLabel( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption );
TUI_EXTERN rc_t CC KTUIDlgAddTabHdr( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption );
TUI_EXTERN rc_t CC KTUIDlgAddLabel2( struct KTUIDlg * self, uint32_t id, uint32_t x, uint32_t y, uint32_t w,
                                     const char * caption );

TUI_EXTERN rc_t CC KTUIDlgAddBtn ( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption );
TUI_EXTERN rc_t CC KTUIDlgAddBtn2 ( struct KTUIDlg * self, uint32_t id, uint32_t x, uint32_t y, uint32_t w,
                                    const char * caption );

TUI_EXTERN rc_t CC KTUIDlgAddCheckBox ( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * caption );

TUI_EXTERN rc_t CC KTUIDlgAddInput ( struct KTUIDlg * self, uint32_t id, const tui_rect * r, const char * txt, size_t length );

TUI_EXTERN rc_t CC KTUIDlgAddRadioBox ( struct KTUIDlg * self, uint32_t id, const tui_rect * r );

TUI_EXTERN rc_t CC KTUIDlgAddList ( struct KTUIDlg * self, uint32_t id, const tui_rect * r );

TUI_EXTERN rc_t CC KTUIDlgSetHScroll ( struct KTUIDlg * self, uint32_t id, bool enabled );


TUI_EXTERN rc_t CC KTUIDlgAddProgress ( struct KTUIDlg * self, uint32_t id,
                                        const tui_rect * r, int32_t percent, int32_t precision );

TUI_EXTERN rc_t CC KTUIDlgAddSpinEdit ( struct KTUIDlg * self, uint32_t id,
                                        const tui_rect * r, int64_t value, int64_t min, int64_t max );

TUI_EXTERN bool CC KTUIDlgHasWidget ( struct KTUIDlg * self, uint32_t id );

TUI_EXTERN void KTUIDlgEnableCursorNavigation( struct KTUIDlg * self, bool enabled );

/* ****************************************************************************************** */

typedef uint32_t TUIWGridStr;
enum
{
    kGrid_Col = 1,
    kGrid_Row,
    kGrid_Cell
};

typedef void ( * TUIWGridCallbackStr  ) ( TUIWGridStr what,
                                           uint64_t col, uint64_t row, uint32_t col_width,
                                           const char ** value, void * data, void * instance );

typedef uint32_t TUIWGridInt;
enum
{
    kGrid_Get_Width = 1,
    kGrid_Set_Width,
    kGrid_Get_ColCount,
    kGrid_Get_RowCount,
    kGrid_Prepare_Page,
    kGrid_Next_Row
};

typedef void ( * TUIWGridCallbackInt  ) ( TUIWGridInt what,
                                           uint64_t col, uint32_t widget_width,
                                           uint64_t * value, void * data, void * instance );

typedef struct TUIWGrid_data
{
    TUIWGridCallbackStr  cb_str;
    TUIWGridCallbackInt  cb_int;
    void * data;
    void * instance;
    void * int_string_cache;
    uint64_t col, row, col_offset, row_offset;
    bool show_header, show_row_header, show_h_scroll, show_v_scroll, row_offset_changed;
    uint32_t default_col_width, row_hdr_width;
} TUIWGrid_data;


TUI_EXTERN rc_t CC KTUIDlgAddGrid ( struct KTUIDlg * self, uint32_t id,
                                    const tui_rect * r, TUIWGrid_data * grid_data, bool cached );

TUI_EXTERN TUIWGrid_data * CC KTUIDlgGetWidgetGridData( struct KTUIDlg * self, uint32_t id );
TUI_EXTERN rc_t CC KTUIDlgSetWidgetGridData( struct KTUIDlg * self, uint32_t id, TUIWGrid_data * grid_data, bool cached );


TUI_EXTERN rc_t CC KTUIDlgGetGridCol( struct KTUIDlg * self, uint32_t id, uint64_t *col );
TUI_EXTERN rc_t CC KTUIDlgSetGridCol( struct KTUIDlg * self, uint32_t id, uint64_t col );
TUI_EXTERN rc_t CC KTUIDlgGetGridRow( struct KTUIDlg * self, uint32_t id, uint64_t *row );
TUI_EXTERN rc_t CC KTUIDlgSetGridRow( struct KTUIDlg * self, uint32_t id, uint64_t row );

/* ****************************************************************************************** */


TUI_EXTERN rc_t CC KTUIDlgRemove ( struct KTUIDlg * self, uint32_t id );

TUI_EXTERN rc_t CC KTUIDlgDraw( struct KTUIDlg * self, bool forced );

TUI_EXTERN rc_t CC KTUIDlgDrawWidget( struct KTUIDlg * self, uint32_t id );

TUI_EXTERN rc_t CC KTUIDlgDrawCaption( struct KTUIDlg * self );


TUI_EXTERN rc_t CC KTUIDlgSetFocus( struct KTUIDlg * self, uint32_t id );

TUI_EXTERN rc_t CC KTUIDlgMoveFocus( struct KTUIDlg * self, bool forward );

TUI_EXTERN bool CC KTUIDlgFocusValid( struct KTUIDlg * self );

TUI_EXTERN uint32_t CC KTUIDlgGetFocusId( struct KTUIDlg * self );

TUI_EXTERN void KTUIDlgPushEvent( struct KTUIDlg * self,
                       KTUIDlg_event_type event_type, uint32_t widget_id,
                       uint64_t value_1, uint64_t value_2, void * ptr_0 );

TUI_EXTERN rc_t CC KTUIDlgHandleEvent( struct KTUIDlg * self, tui_event * event );

TUI_EXTERN rc_t CC KTUIDlgGet ( struct KTUIDlg * self, tuidlg_event * event );


/* ****************************************************************************************** */

TUI_EXTERN rc_t CC TUI_ShowMessage ( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                       const char * txt, uint32_t x, uint32_t y, uint32_t w, KTUI_color bg1, KTUI_color bg2 );

TUI_EXTERN rc_t CC TUI_YesNoDlg ( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                    const char * question, uint32_t x, uint32_t y, uint32_t w, bool * yes, KTUI_color bg1, KTUI_color bg2 );

TUI_EXTERN rc_t CC TUI_ShowFile( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                   const char * filename, tui_rect * r, KTUI_color bg1, KTUI_color bg2 );

TUI_EXTERN rc_t CC TUI_EditBuffer( struct KTUI * tui, struct KTUIDlg * parent, const char * caption,
                     char * buffer, size_t buflen, uint32_t x, uint32_t y, uint32_t w, bool * selected,
                     KTUI_color bg1, KTUI_color bg2 );

TUI_EXTERN rc_t CC TUI_PickFromList( struct KTUI * tui, struct KTUIDlg * parent,
        const char * caption, const VNamelist * list,
        uint32_t * selection, tui_rect * r, bool * selected, KTUI_color bg1, KTUI_color bg2 );

TUI_EXTERN rc_t CC FileDlg ( struct KTUI * tui, struct KTUIDlg * parent,
        char * buffer, uint32_t buffer_size, const char * extension, bool * done, tui_rect * r,
        uint32_t dir_h, KTUI_color bg1, KTUI_color bg2 );

TUI_EXTERN rc_t CC DirDlg ( struct KTUI * tui, struct KTUIDlg * parent,
        char * buffer, uint32_t buffer_size, bool * done, tui_rect * r,
        KTUI_color bg1, KTUI_color bg2, bool allow_dir_create );

#ifdef __cplusplus
}
#endif

#endif /* _h_tui_dlg_ */
