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
#include <klib/refcount.h>
#include <klib/rc.h>

#include <tui/tui.h>
#include <tui/tui_dlg.h>

#include <sysalloc.h>
#include <stdlib.h>

typedef struct KTUIPalette
{
    KRefcount refcount;
    tui_ac entry[ ktuipa_last ];
} KTUIPalette;

static const char tuipa_classname [] = "KTUIPalette";

/* the hardcoded default values... */
#define INVALID_ATTR KTUI_a_none
#define INVALID_FG KTUI_c_white
#define INVALID_BG KTUI_c_gray

#define DFL_DLG_ATTR KTUI_a_none
#define DFL_DLG_FG KTUI_c_white
#define DFL_DLG_BG KTUI_c_light_gray

#define DFL_DLG_CAPTION_ATTR KTUI_a_none
#define DFL_DLG_CAPTION_FG KTUI_c_white
#define DFL_DLG_CAPTION_BG KTUI_c_gray

#define DFL_DLG_FOCUS_ATTR KTUI_a_none
#define DFL_DLG_FOCUS_FG KTUI_c_white
#define DFL_DLG_FOCUS_BG KTUI_c_red

#define DFL_LABEL_ATTR KTUI_a_none
#define DFL_LABEL_FG KTUI_c_white
#define DFL_LABEL_BG KTUI_c_gray

#define DFL_BUTTON_ATTR KTUI_a_none
#define DFL_BUTTON_FG KTUI_c_white
#define DFL_BUTTON_BG KTUI_c_gray

#define DFL_CHECKBOX_ATTR KTUI_a_none
#define DFL_CHECKBOX_FG KTUI_c_white
#define DFL_CHECKBOX_BG KTUI_c_gray

#define DFL_INPUT_ATTR KTUI_a_none
#define DFL_INPUT_FG KTUI_c_black
#define DFL_INPUT_BG KTUI_c_white

#define DFL_INPUT_HINT_ATTR KTUI_a_none
#define DFL_INPUT_HINT_FG KTUI_c_black
#define DFL_INPUT_HINT_BG KTUI_c_red

#define DFL_RADIOBOX_ATTR KTUI_a_none
#define DFL_RADIOBOX_FG KTUI_c_white
#define DFL_RADIOBOX_BG KTUI_c_gray

#define DFL_LIST_ATTR KTUI_a_none
#define DFL_LIST_FG KTUI_c_white
#define DFL_LIST_BG KTUI_c_gray

#define DFL_PROGRESS_ATTR KTUI_a_none
#define DFL_PROGRESS_FG KTUI_c_white
#define DFL_PROGRESS_BG KTUI_c_gray

#define DFL_SPINEDIT_ATTR KTUI_a_none
#define DFL_SPINEDIT_FG KTUI_c_white
#define DFL_SPINEDIT_BG KTUI_c_gray

#define DFL_GRID_ATTR KTUI_a_none
#define DFL_GRID_FG KTUI_c_black
#define DFL_GRID_BG KTUI_c_gray

#define DFL_GRID_COL_HDR_ATTR KTUI_a_none
#define DFL_GRID_COL_HDR_FG KTUI_c_black
#define DFL_GRID_COL_HDR_BG KTUI_c_white

#define DFL_GRID_ROW_HDR_ATTR KTUI_a_none
#define DFL_GRID_ROW_HDR_FG KTUI_c_black
#define DFL_GRID_ROW_HDR_BG KTUI_c_white

#define DFL_GRID_CURSOR_ATTR KTUI_a_none
#define DFL_GRID_CURSOR_FG KTUI_c_yellow
#define DFL_GRID_CURSOR_BG KTUI_c_black

#define DFL_MENU_ATTR KTUI_a_none
#define DFL_MENU_FG KTUI_c_gray
#define DFL_MENU_BG KTUI_c_dark_blue

#define DFL_MENU_SEL_ATTR KTUI_a_bold
#define DFL_MENU_SEL_FG KTUI_c_white
#define DFL_MENU_SEL_BG KTUI_c_dark_blue

#define DFL_MENU_HI_ATTR KTUI_a_none
#define DFL_MENU_HI_FG KTUI_c_cyan
#define DFL_MENU_HI_BG KTUI_c_dark_blue

#define DFL_HL_ATTR KTUI_a_bold | KTUI_a_underline
#define DFL_HL_FG KTUI_c_red
#define DFL_HL_BG KTUI_c_white

static void write_default_values ( struct KTUIPalette * self )
{
    set_ac( &self->entry[ ktuipa_dlg ], DFL_DLG_ATTR, DFL_DLG_FG, DFL_DLG_BG );
    set_ac( &self->entry[ ktuipa_dlg_caption ], DFL_DLG_CAPTION_ATTR, DFL_DLG_CAPTION_FG, DFL_DLG_CAPTION_BG );
    set_ac( &self->entry[ ktuipa_dlg_focus ], DFL_DLG_FOCUS_ATTR, DFL_DLG_FOCUS_FG, DFL_DLG_FOCUS_BG );

    set_ac( &self->entry[ ktuipa_label ], DFL_LABEL_ATTR, DFL_LABEL_FG, DFL_LABEL_BG );
    set_ac( &self->entry[ ktuipa_button ], DFL_BUTTON_ATTR, DFL_BUTTON_FG, DFL_BUTTON_BG );
    set_ac( &self->entry[ ktuipa_checkbox ], DFL_CHECKBOX_ATTR, DFL_CHECKBOX_FG, DFL_CHECKBOX_BG );
    set_ac( &self->entry[ ktuipa_input ], DFL_INPUT_ATTR, DFL_INPUT_FG, DFL_INPUT_BG );
    set_ac( &self->entry[ ktuipa_input_hint ], DFL_INPUT_HINT_ATTR, DFL_INPUT_HINT_FG, DFL_INPUT_HINT_BG );
    set_ac( &self->entry[ ktuipa_radiobox ], DFL_RADIOBOX_ATTR, DFL_RADIOBOX_FG, DFL_RADIOBOX_BG );
    set_ac( &self->entry[ ktuipa_list ], DFL_LIST_ATTR, DFL_LIST_FG, DFL_LIST_BG );
    set_ac( &self->entry[ ktuipa_progress ], DFL_PROGRESS_ATTR, DFL_PROGRESS_FG, DFL_PROGRESS_BG );
    set_ac( &self->entry[ ktuipa_spinedit ], DFL_SPINEDIT_ATTR, DFL_SPINEDIT_FG, DFL_SPINEDIT_BG );
    set_ac( &self->entry[ ktuipa_grid ], DFL_GRID_ATTR, DFL_GRID_FG, DFL_GRID_BG );

    set_ac( &self->entry[ ktuipa_grid_col_hdr ], DFL_GRID_COL_HDR_ATTR, DFL_GRID_COL_HDR_FG, DFL_GRID_COL_HDR_BG );
    set_ac( &self->entry[ ktuipa_grid_row_hdr ], DFL_GRID_ROW_HDR_ATTR, DFL_GRID_ROW_HDR_FG, DFL_GRID_ROW_HDR_BG );
    set_ac( &self->entry[ ktuipa_grid_cursor ], DFL_GRID_CURSOR_ATTR, DFL_GRID_CURSOR_FG, DFL_GRID_CURSOR_BG );

    set_ac( &self->entry[ ktuipa_menu ], DFL_MENU_ATTR, DFL_MENU_FG, DFL_MENU_BG );
    set_ac( &self->entry[ ktuipa_menu_sel ], DFL_MENU_SEL_ATTR, DFL_MENU_SEL_FG, DFL_MENU_SEL_BG );
    set_ac( &self->entry[ ktuipa_menu_hi ], DFL_MENU_HI_ATTR, DFL_MENU_HI_FG, DFL_MENU_HI_BG );

    set_ac( &self->entry[ ktuipa_hl ], DFL_HL_ATTR, DFL_HL_FG, DFL_HL_BG );
}


LIB_EXPORT rc_t CC KTUIPaletteMake ( struct KTUIPalette ** self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUIPalette * pa = malloc( sizeof * pa );
        if ( pa == NULL )
            rc = RC( rcApp, rcAttr, rcCreating, rcMemory, rcExhausted );
        else
        {
            KRefcountInit( &pa->refcount, 1, "KTUIPalette", "make", tuipa_classname );
            write_default_values ( pa );
            ( * self ) = pa;
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIPaletteAddRef ( const struct KTUIPalette * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        switch ( KRefcountAdd( &self->refcount, tuipa_classname ) )
        {
        case krefOkay :
            break;
        case krefZero :
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcIncorrect );
        case krefLimit :
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcExhausted );
        case krefNegative :
            rc =  RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcInvalid );
        default :
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcUnknown );
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUIPaletteRelease ( const struct KTUIPalette * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        switch ( KRefcountDrop( &self->refcount, tuipa_classname ) )
        {
        case krefOkay :
        case krefZero :
            break;
        case krefWhack :
            free( ( void * ) self );
            break;
        case krefNegative:
            return RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcInvalid );
        default:
            rc = RC( rcNS, rcMgr, rcAttaching, rcRefcount, rcUnknown );
            break;            
        }
    }
    return rc;
}


static tui_ac invalid_entry = { INVALID_ATTR, INVALID_FG, INVALID_BG };


LIB_EXPORT const tui_ac * CC KTUIPaletteGet ( const struct KTUIPalette * self, KTUIPa_entry what )
{
    if ( self != NULL && what >= ktuipa_dlg && what < ktuipa_last )
        return &self->entry[ what ];
    else 
        return &invalid_entry;
}


LIB_EXPORT rc_t CC KTUIPaletteSet ( struct KTUIPalette * self, KTUIPa_entry what, const tui_ac * ac )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( ac == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else if ( what < ktuipa_dlg || what >= ktuipa_last )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcInvalid );
    else
        copy_ac( &self->entry[ what ], ac );
    return rc;
}


LIB_EXPORT rc_t CC KTUIPaletteSet_fg ( struct KTUIPalette * self, KTUIPa_entry what, KTUI_color fg )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( what < ktuipa_dlg || what >= ktuipa_last )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcInvalid );
    else
        self->entry[ what ].fg = fg;
    return rc;
}


LIB_EXPORT rc_t CC KTUIPaletteSet_bg ( struct KTUIPalette * self, KTUIPa_entry what, KTUI_color bg )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( what < ktuipa_dlg || what >= ktuipa_last )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcInvalid );
    else
        self->entry[ what ].bg = bg;
    return rc;
}


LIB_EXPORT rc_t CC KTUIPaletteDefault ( struct KTUIPalette * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
        write_default_values ( self );
    return rc;
}


LIB_EXPORT rc_t CC KTUIPaletteCopy ( struct KTUIPalette * dst, const struct KTUIPalette * src )
{
    rc_t rc = 0;
    if ( dst == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( src == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    else
    {
        uint32_t i;
        for ( i = ktuipa_dlg; i < ktuipa_last; ++ i )
            copy_ac( &dst->entry[ i ], &src->entry[ i ] );
    }
    return rc;
}
