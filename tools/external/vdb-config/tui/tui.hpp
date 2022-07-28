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

#ifndef _hpp_tui_
#define _hpp_tui_

#ifndef _h_tui_
#include <tui/tui.h>
#endif

#ifndef _h_tui_dlg_
#include <tui/tui_dlg.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#include <string>
#include <cstring>

namespace tui {

typedef int tui_coord;
typedef int tui_id;
typedef long long int tui_long;

class Tui;  // forward decl because of beeing friend with Point, Rect and Ac 
class Dlg;
class Std_Dlg_Base;  
class Std_Dlg_Pick;
class Std_Dlg_File_Pick;
class Std_Dlg_Dir_Pick;
class Dlg_Runner;

class Tui_Point
{
    private :
        tui_point p_;     // open struct from interfaces/tui/tui.h

    public :
        Tui_Point( const tui_coord x, const tui_coord y ) { set( x, y ); }; // ctor from 2 coordinates
        Tui_Point( const Tui_Point &other ) { set( other.get_x(), other.get_y() ); };      // copy-ctor

        Tui_Point & operator= ( const Tui_Point & other )  // assignment operator
            { if ( this != &other ) set( other.get_x(), other.get_y() ); return *this; }

        const tui_coord get_x( void ) const { return p_.x; };
        const tui_coord get_y( void ) const { return p_.y; };
        void set_x( const tui_coord x ) { p_.x = x; };
        void set_y( const tui_coord y ) { p_.y = y; };
        void set( const tui_coord x, const tui_coord y ) { p_.x = x; p_.y = y; };

        friend class Tui;   // so the Tui-class can access the private struct of this class
};


class Tui_Rect
{
    private :
        tui_rect r_;     // open struct from interfaces/tui/tui.h

    public :
        Tui_Rect( const tui_coord x, const tui_coord y, const tui_coord w, const tui_coord h ) { set( x, y, w, h ); };
        Tui_Rect( void ) { set( 0, 0, 1, 1 ); };
        Tui_Rect( const Tui_Rect &other ) { set( other ); };      // copy-ctor

        Tui_Rect & operator= ( const Tui_Rect & other )  // assignment operator
            { if ( this != &other ) set( other ); return *this; }

        const tui_coord get_x( void ) const { return r_.top_left.x; };
        const tui_coord get_y( void ) const { return r_.top_left.y; };
        const tui_coord get_w( void ) const { return r_.w; };
        const tui_coord get_h( void ) const { return r_.h; };
        void get( tui_coord &x, tui_coord &y, tui_coord &w, tui_coord &h ) const
        { x = r_.top_left.x; y = r_.top_left.y; w = r_.w; h = r_.h; }

        void set_x( const tui_coord x ) { r_.top_left.x = x; };
        void set_y( const tui_coord y ) { r_.top_left.y = y; };
        void set_w( const tui_coord w ) { r_.w = w; };
        void set_h( const tui_coord h ) { r_.h = h; };
        void set( const tui_coord x, const tui_coord y, const tui_coord w, const tui_coord h )
            { r_.top_left.x = x; r_.top_left.y = y; r_.w = w; r_.h = h; };
        void set( Tui_Rect const &other )
            { r_.top_left.x = other.r_.top_left.x; r_.top_left.y = other.r_.top_left.y; r_.w = other.r_.w; r_.h = other.r_.h; };
        void change( const tui_coord dx, const tui_coord dy, const tui_coord dw, const tui_coord dh )
            { r_.top_left.x += dx; r_.top_left.y += dy, r_.w += dw, r_.h += dh; };

        friend class Tui;   // so the Tui-class can access the private struct of this class
        friend class Dlg;
        friend class Std_Dlg_Pick;
        friend class Std_Dlg_File_Pick;
        friend class Std_Dlg_Dir_Pick;
};

class Tui_Ac
{
    private :
        tui_ac ac_;     // open struct from interfaces/tui/tui.h

    public :
        Tui_Ac( const KTUI_attrib attr, const KTUI_color fg, const KTUI_color bg ) { set( attr, fg, bg ); };
        Tui_Ac( const Tui_Ac &other ) { set( other ); };      // copy-ctor

        Tui_Ac & operator= ( const Tui_Ac & other )  // assignment operator
            { if ( this != &other ) set( other ); return *this; }

        const KTUI_attrib get_attrib( void ) const { return ac_.attr; };
        const KTUI_color get_fg( void ) const { return ac_.fg; };
        const KTUI_color get_bg( void ) const { return ac_.bg; };
        void set_attrib( const KTUI_attrib attr ) { ac_.attr = attr; };
        void set_fg( const KTUI_color fg ) { ac_.fg = fg; };
        void set_bg( const KTUI_color bg ) { ac_.bg = bg; };
        void set( const KTUI_attrib attr, const KTUI_color fg, const KTUI_color bg ) { ac_.attr = attr; ac_.fg = fg; ac_.bg = bg; };
        void set( const Tui_Ac &other ) { ac_.attr = other.ac_.attr; ac_.fg = other.ac_.fg; ac_.bg = other.ac_.bg; };

        friend class Tui;   // so the Tui-class can access the private struct of this class
};


class Tui_Event
{
    private :
        tui_event ev_;     // open struct from interfaces/tui/tui.h

    public :
        Tui_Event( void ) { ev_.event_type = ktui_event_none; };

        bool get_from_tui( void );

        KTUI_event_type get_type( void ) const { return ev_.event_type; };

        KTUI_key get_key_type( void ) const { return ev_.data.kb_data.code; };
        int get_key( void ) const { return ev_.data.kb_data.key; };

        tui_coord get_mouse_x( void ) const { return ev_.data.mouse_data.x; };
        tui_coord get_mouse_y( void ) const { return ev_.data.mouse_data.y; };
        KTUI_mouse_button get_mouse_button( void ) const { return ev_.data.mouse_data.button; };

        tui_coord get_window_width( void ) const { return ev_.data.win_data.w; };
        tui_coord get_window_height( void ) const { return ev_.data.win_data.h; };

        bool empty( void ) const { return ( get_type() == ktui_event_none ); };

        friend class Tui;   // so the Tui-class can access the private struct of this class
        friend class Dlg;
};


class Tui
{
    private :
        struct KTUI * tui_;    // opaque struct from interfaces/tui/tui.h
        static Tui * instance;

        Tui( const int timeout = 10000 ) { KTUIMake ( NULL, &tui_, timeout ); };    // private constructor
        Tui( const Tui & );     // prevent copy construction
        Tui& operator=( const Tui& );   // prevent assignment
        ~Tui( void ) { KTUIRelease ( tui_ ); };

    public :
        static Tui * getInstance( void );
        static void clean_up( void );

        bool SetTimeout( const int timeout ) { return ( KTUISetTimeout ( tui_, timeout ) == 0 ); };

        bool Print ( const Tui_Point &p, const Tui_Ac &ac, const std::string &s, const int l = 0 )
            { return ( KTUIPrint( tui_, &( p.p_ ), &( ac.ac_ ), s.c_str(), ( l == 0 ) ? (int)s.length() : l ) == 0 ); };

        bool PaintRect( const Tui_Rect &r, const Tui_Ac &ac, const char c = ' ' )
            { return ( KTUIRect( tui_, &( r.r_ ), &( ac.ac_ ), c ) == 0 ); }

        bool Flush( bool forced = false ) { return ( KTUIFlush ( tui_, forced ) == 0 ); };

        bool GetExtent ( tui_coord * cols, tui_coord * lines ) const { return ( KTUIGetExtent ( tui_, cols, lines ) == 0 ); };
        bool GetExtent ( Tui_Rect &r )
        {
            tui_coord cols, lines;
            bool res = ( KTUIGetExtent ( tui_, &cols, &lines ) == 0 );
            if ( res ) r.set( 0, 0, cols, lines );
            return res;
        }

        bool ClrScr( const KTUI_color bg ) { return ( KTUIClrScr( tui_, bg ) == 0 ); };

        friend class Tui_Event;
        friend class Dlg;
        friend class Std_Dlg_Base;
};


class Tui_Dlg_Event
{
    private :
        tuidlg_event ev_;     // open struct from interfaces/tui/tui_dlg.h

    public :
        Tui_Dlg_Event( void ) { ev_.event_type = ktuidlg_event_none; };

        const KTUIDlg_event_type get_type( void ) const { return ev_.event_type; };
        const tui_id get_widget_id( void ) const { return ev_.widget_id; };
        const tui_long get_value_1( void ) const { return ev_.value_1; };
        const tui_long get_value_2( void ) const { return ev_.value_2; };
        const void * get_ptr_value( void ) const { return ev_.ptr_0; };
        const bool empty( void ) const { return ( ev_.event_type == ktuidlg_event_none ); };

        friend class Dlg_Runner;
};


class Tui_Menu
{
    private :
        struct KTUI_Menu * menu_;

    public :
        Tui_Menu( void ) { KTUI_Menu_Make ( &menu_ ); };
        ~Tui_Menu( void ) { KTUI_Menu_Release ( menu_ ); };

        bool Add( const char * path, tui_id id, char shortcut ) { return ( KTUI_Menu_Add ( menu_, path, id, shortcut ) == 0 ); }
        bool Remove( tui_id id ) { return ( KTUI_Menu_Remove ( menu_, id ) == 0 ); };

        struct KTUI_Menu * get( bool inc_ref_count )
        {
            if ( inc_ref_count )
            {
                if ( KTUI_Menu_AddRef ( menu_ ) == 0 ) return menu_; else return NULL;
            }
            else
                return menu_;
        };
};


class Grid
{
    private :
        TUIWGrid_data grid_data_;
        char buffer[ 1024 ];

        static void static_str_cb( TUIWGridStr what, uint64_t col, uint64_t row,
                                   uint32_t col_width, const char ** value, void * data, void * instance )
        {
            Grid *g = static_cast< Grid* >( instance );
            g->buffer[ 0 ] = 0;
            switch( what )
            {
                case kGrid_Col   : g->Col_Hdr_Request( col, col_width, data, g->buffer, sizeof g->buffer ); break; 
                case kGrid_Row   : g->Row_Hdr_Request( row, col_width, data, g->buffer, sizeof g->buffer ); break; 
                case kGrid_Cell  : g->Cell_Request( col, row, col_width, data, g->buffer, sizeof g->buffer ); break;
            }
            *value = g->buffer;
        };

        static void static_int_cb( TUIWGridInt what, uint64_t col, uint32_t widget_width,
                                   uint64_t * value, void * data, void * instance )
        {
            Grid *g = static_cast< Grid* >( instance );
            switch( what )
            {
                case kGrid_Get_Width    : *value = g->Get_Col_Width( col, widget_width, data ); break;
                case kGrid_Set_Width    : g->Set_Col_Width( col, widget_width, data, *value ); break;
                case kGrid_Get_ColCount : *value = g->Get_Col_Count( widget_width, data ); break;
                case kGrid_Get_RowCount : *value = g->Get_Row_Count( widget_width, data ); break;
                case kGrid_Prepare_Page : g->Prepare_Page( col, widget_width, data ); break;
                case kGrid_Next_Row     : g->Next_Row( col, data ); break;
                default : *value = 0;
            }
        };

    public :
        Grid( void * data );
        virtual ~Grid() {};

        void show_header( bool show ) { grid_data_.show_header = show; };
        void show_row_header( int width ) { grid_data_.row_hdr_width = width; grid_data_.show_row_header = ( width > 0 ); };
        void show_h_scroll( bool show ) { grid_data_.show_h_scroll = show; };
        void show_v_scroll( bool show ) { grid_data_.show_v_scroll = show; };

        TUIWGrid_data * get_ptr( void ) { return &grid_data_; };

        /* overwrite these in derived classes */
        virtual void Col_Hdr_Request( tui_long col, uint32_t col_width, void * data, char * buffer, size_t buffer_size )
            { string_printf ( buffer, buffer_size, NULL, "C %lu", col+1 ); };

        virtual void Row_Hdr_Request( tui_long row, uint32_t col_width, void * data, char * buffer, size_t buffer_size )
            { string_printf ( buffer, buffer_size, NULL, "R %lu", row+1 ); };

        virtual void Cell_Request( tui_long col, tui_long row, uint32_t col_width, void * data, char * buffer, size_t buffer_size )
            { string_printf ( buffer, buffer_size, NULL, "Z %lu-%lu", col + 1, row + 1 ); };

        virtual tui_long Get_Col_Width( tui_long col, uint32_t widget_width, void * data ) { return 12; };
        virtual void Set_Col_Width( tui_long col, uint32_t widget_width, void * data, tui_long value ) {};
        virtual tui_long Get_Col_Count( uint32_t widget_width, void * data ) { return 8; };
        virtual tui_long Get_Row_Count( uint32_t widget_width, void * data ) { return 32; };
        virtual void Prepare_Page( uint64_t row, uint32_t row_count, void * data ) { ; };
        virtual void Next_Row( uint64_t row, void * data ) { ; };
};


class Dlg
{
    private :
        struct KTUIDlg * dlg_;  // opaque struct from interfaces/tui/tui_dlg.h
        
    public :
        Dlg( void )
        {
            Tui * instance = Tui::getInstance();
            KTUIDlgMake ( instance->tui_, &dlg_, /*parent*/ NULL, /*palette*/ NULL, /*rect*/ NULL );
        };

        Dlg( Dlg &parent )
        {
            KTUIDlgMake ( KTUIDlgGetTui ( parent . dlg_ ), &dlg_, parent . dlg_, /*palette*/ NULL, /*rect*/ NULL );
        };
        
        Dlg( Dlg &parent, Tui_Rect &r )
        {
            KTUIDlgMake ( KTUIDlgGetTui ( parent . dlg_ ), &dlg_, parent . dlg_, /*palette*/ NULL, & r.r_ );
        };
        
        ~Dlg( void ) { KTUIDlgRelease ( dlg_ ); };

        bool SetCaption( const char * s ) { return ( KTUIDlgSetCaption ( dlg_, s ) == 0 ); };
        bool SetCaption( std::string &s ) { return ( KTUIDlgSetCaption ( dlg_, s.c_str() ) == 0 ); };
        bool SetCaptionF( const char * fmt, ... );

        Tui_Rect center( uint32_t x_margin, uint32_t y_margin );
        void center( Tui_Rect &r );

        void SetData( void * data ) { KTUIDlgSetData ( dlg_, data ); };
        void * GetData( void ) { return KTUIDlgGetData ( dlg_ ); };

        void SetDone( bool done ) { KTUIDlgSetDone ( dlg_, done ); };
        bool IsDone( void ) { return KTUIDlgGetDone ( dlg_ ); };

        void SetChanged( void ) { KTUIDlgSetChanged ( dlg_ ); };
        void ClearChanged( void ) { KTUIDlgClearChanged ( dlg_ ); };
        bool IsChanged( void ) { return KTUIDlgGetChanged ( dlg_ ); };

        bool GetRect( Tui_Rect &r ) { return ( KTUIDlgGetRect ( dlg_, &( r.r_ ) ) == 0 ); };
        Tui_Rect GetRect( void ) { Tui_Rect r; GetRect( r ); return r; }        
        bool SetRect( Tui_Rect const &r, bool redraw ) { return ( KTUIDlgSetRect ( dlg_, &( r.r_ ), redraw ) == 0 ); };
        virtual bool Resize( Tui_Rect const &r );

        virtual void onPageChanged( uint32_t old_page, uint32_t new_page ) {};
        
        uint32_t GetActivePage( void ) { uint32_t id; KTUIDlgGetActivePage ( dlg_, &id ); return id; };
        bool SetActivePage( uint32_t id );
        
        bool IsMenuActive( void ) { return KTUIDlgGetMenuActive ( dlg_ ); };
        bool SetMenuActive( bool active ) { return ( KTUIDlgSetMenuActive ( dlg_, active ) == 0 ); };
        bool SetMenu( Tui_Menu &menu ) { return ( KTUIDlgSetMenu ( dlg_, menu.get( true ) ) == 0 ); };
        bool ToggleMenu( void ) { return SetMenuActive( !IsMenuActive() ); };

        bool SetWidgetCaption( tui_id id, const char * caption ) { return ( KTUIDlgSetWidgetCaption ( dlg_, id, caption ) == 0 ); };
        bool SetWidgetCaption( tui_id id, std::string caption ) { return ( KTUIDlgSetWidgetCaption ( dlg_, id, caption.c_str() ) == 0 ); };
        bool SetWidgetCaptionF( tui_id id, const char * fmt, ... );

        bool GetWidgetRect( tui_id id, Tui_Rect &r ) { return ( KTUIDlgGetWidgetRect ( dlg_, id, &( r.r_ ) ) == 0 ); };
        bool SetWidgetRect( tui_id id, Tui_Rect const &r, bool redraw ) { return ( KTUIDlgSetWidgetRect ( dlg_, id, &( r.r_ ), redraw ) == 0 ); };

        bool GetWidgetPageId( tui_id id, uint32_t * pg_id ) { return ( KTUIDlgGetWidgetPageId ( dlg_, id, pg_id ) == 0 ); };
        bool SetWidgetPageId( tui_id id, uint32_t pg_id ) { return ( KTUIDlgSetWidgetPageId ( dlg_, id, pg_id ) == 0 ); };
        
        bool SetWidgetCanFocus( tui_id id, bool can_focus ) { return ( KTUIDlgSetWidgetCanFocus ( dlg_, id, can_focus ) == 0 ); };

        bool IsWidgetVisisble( tui_id id ) { return KTUIDlgGetWidgetVisible ( dlg_, id ); };
        bool SetWidgetVisible( tui_id id, bool visible ) { return ( KTUIDlgSetWidgetVisible ( dlg_, id, visible ) == 0 ); };

        bool HasWidgetChanged( tui_id id ) { return KTUIDlgGetWidgetChanged ( dlg_, id ); };
        bool SetWidgetChanged( tui_id id, bool changed ) { return ( KTUIDlgSetWidgetChanged ( dlg_, id, changed ) == 0 ); };

        bool GetWidgetBoolValue( tui_id id ) { return KTUIDlgGetWidgetBoolValue ( dlg_, id ); };
        bool SetWidgetBoolValue( tui_id id, bool value ) { return ( KTUIDlgSetWidgetBoolValue ( dlg_, id, value ) == 0 ); };
        bool ToggleWidgetBoolValue( tui_id id ) { return SetWidgetBoolValue( id, !GetWidgetBoolValue( id ) ); };
        
        bool SetWidgetBackground( tui_id id, KTUI_color value ) { return ( KTUIDlgSetWidgetBg ( dlg_, id, value ) == 0 ); };
        bool ReleaseWidgetBackground( tui_id id ) { return ( KTUIDlgReleaseWidgetBg ( dlg_, id ) == 0 ); };

		bool SetWidgetForeground( tui_id id, KTUI_color value ) { return ( KTUIDlgSetWidgetFg ( dlg_, id, value ) == 0 ); };
        bool ReleaseWidgetForeground( tui_id id ) { return ( KTUIDlgReleaseWidgetFg ( dlg_, id ) == 0 ); };

        tui_long GetWidgetInt64Value( tui_id id ) { return KTUIDlgGetWidgetInt64Value ( dlg_, id ); };
        bool SetWidgetInt64Value( tui_id id, tui_long value ) { return ( KTUIDlgSetWidgetInt64Value ( dlg_, id, value ) == 0 ); };

        tui_long GetWidgetInt64Min( tui_id id ) { return KTUIDlgGetWidgetInt64Min ( dlg_, id ); };
        bool SetWidgetInt64Min( tui_id id, tui_long value ) { return ( KTUIDlgSetWidgetInt64Min ( dlg_, id, value ) == 0 ); };

        tui_long GetWidgetInt64Max( tui_id id ) { return KTUIDlgGetWidgetInt64Max ( dlg_, id ); };
        bool SetWidgetInt64Max( tui_id id, tui_long value ) { return ( KTUIDlgSetWidgetInt64Max ( dlg_, id, value ) == 0 ); };

        int GetWidgetPercent( tui_id id ) { return KTUIDlgGetWidgetPercent ( dlg_, id ); };
        bool SetWidgetPercent( tui_id id, int value ) { return ( KTUIDlgSetWidgetPercent ( dlg_, id, value ) == 0 ); };

        int GetWidgetPrecision( tui_id id ) { return KTUIDlgGetWidgetPrecision ( dlg_, id ); };
        bool SetWidgetPrecision( tui_id id, int value ) { return ( KTUIDlgSetWidgetPrecision( dlg_, id, value ) == 0 ); };
        int CalcPercent( tui_long value, tui_long max, int precision ) { return KTUIDlgCalcPercent ( value, max, precision ); };

        const char * GetWidgetText( tui_id id ) { return KTUIDlgGetWidgetText( dlg_, id ); };
        std::string GetWidgetString( tui_id id ) { return std::string( KTUIDlgGetWidgetText( dlg_, id ) ); };
        bool SetWidgetText( tui_id id, const char * value ) { return ( KTUIDlgSetWidgetText ( dlg_, id, value ) == 0 ); };
        bool SetWidgetText( tui_id id, std::string &s ) { return ( KTUIDlgSetWidgetText ( dlg_, id, s.c_str() ) == 0 ); };
        bool SetWidgetTextF( tui_id id, const char * fmt, ... );
        size_t GetWidgetTextLength( tui_id id ) { return KTUIDlgGetWidgetTextLength( dlg_, id ); };
        bool SetWidgetTextLength( tui_id id, size_t value ) { return ( KTUIDlgSetWidgetTextLength ( dlg_, id, value ) == 0 ); };

        bool SetWidgetCarretPos( tui_id id, size_t value ) { return ( KTUIDlgSetWidgetCarretPos ( dlg_, id, value ) == 0 ); };
        bool SetWidgetAlphaMode( tui_id id, uint32_t value ) { return ( KTUIDlgSetWidgetAlphaMode ( dlg_, id, value ) == 0 ); };
        
        bool AddWidgetString( tui_id id, const char * txt ) { return ( KTUIDlgAddWidgetString ( dlg_, id, txt ) == 0 ); };
        bool AddWidgetString( tui_id id, std::string &s ) { return ( KTUIDlgAddWidgetString ( dlg_, id, s.c_str() ) == 0 ); };
        bool AddWidgetStringN( tui_id id, int n, ... );
        bool AddWidgetStringF( tui_id id, const char * fmt, ... );
        bool AddWidgetStrings( tui_id id, VNamelist * src ) { return ( KTUIDlgAddWidgetStrings ( dlg_, id, src ) == 0 ); };
        const char * GetWidgetStringByIdx( tui_id id, tui_id idx ) { return KTUIDlgGetWidgetStringByIdx ( dlg_, id, idx ); };
        bool RemoveWidgetStringByIdx( tui_id id, tui_id idx ) { return ( KTUIDlgRemoveWidgetStringByIdx ( dlg_, id, idx ) == 0 ); };
        bool RemoveAllWidgetStrings( tui_id id ) { return ( KTUIDlgRemoveAllWidgetStrings ( dlg_, id ) == 0 ); };
        tui_id GetWidgetStringCount( tui_id id ) { return KTUIDlgGetWidgetStringCount ( dlg_, id ); };
        tui_id HasWidgetString( tui_id id, const char * txt ) { return KTUIDlgHasWidgetString( dlg_, id, txt ); };
        tui_id HasWidgetString( tui_id id, std::string &txt ) { return KTUIDlgHasWidgetString( dlg_, id, txt.c_str() ); };
        tui_id GetWidgetSelectedString( tui_id id ) { return KTUIDlgGetWidgetSelectedString( dlg_, id ); };
        bool SetWidgetSelectedString( tui_id id, tui_id idx ) { return ( KTUIDlgSetWidgetSelectedString ( dlg_, id, idx ) == 0 ); };

        bool AddLabel( tui_id id, Tui_Rect const &r, const char * s ) { return ( KTUIDlgAddLabel( dlg_, id, &( r.r_ ), s ) == 0 ); };
        bool AddTabHdr( tui_id id, Tui_Rect const &r, const char * s ) { return ( KTUIDlgAddTabHdr( dlg_, id, &( r.r_ ), s ) == 0 ); };        
        bool AddButton( tui_id id, Tui_Rect const &r, const char * s ) { return ( KTUIDlgAddBtn ( dlg_, id, &( r.r_ ), s ) == 0 ); };

        bool AddCheckBox( tui_id id, Tui_Rect const &r, const char * s, bool enabled )
        {
            bool res = ( KTUIDlgAddCheckBox ( dlg_, id, &( r.r_ ), s ) == 0 );
            if ( res ) res = SetWidgetBoolValue( id, enabled );
            return res;
        };

        bool AddInput( tui_id id, Tui_Rect const &r, const char * s, size_t length ) { return ( KTUIDlgAddInput ( dlg_, id, &( r.r_ ), s, length ) == 0 ); };
        bool AddRadioBox( tui_id id, Tui_Rect const &r ) { return ( KTUIDlgAddRadioBox ( dlg_, id, &( r.r_ ) ) == 0 ); };
        bool AddList( tui_id id, Tui_Rect const &r ) { return ( KTUIDlgAddList ( dlg_, id, &( r.r_ ) ) == 0 ); };
        bool SetHScroll( tui_id id, bool enabled ) { return ( KTUIDlgSetHScroll ( dlg_, id, enabled ) == 0 ); };
        bool AddProgress( tui_id id, Tui_Rect const &r, int percent, int precision ) { return ( KTUIDlgAddProgress ( dlg_, id, &( r.r_ ), percent, precision ) == 0 ); };
        bool AddSpinEdit( tui_id id, Tui_Rect const &r, tui_long value, tui_long min, tui_long max ) { return ( KTUIDlgAddSpinEdit ( dlg_, id, &( r.r_ ), value, min, max ) == 0 ); };
        bool AddGrid( tui_id id, Tui_Rect const &r, Grid &grid, bool cached ) { return( KTUIDlgAddGrid ( dlg_, id, &( r.r_ ), grid.get_ptr(), cached ) == 0 ); };

        void Populate_common( uint32_t id, KTUI_color bg, KTUI_color fg, uint32_t page_id );
        void PopulateLabel( Tui_Rect const &r, bool resize, uint32_t id, const char * txt,
                            KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );
        void PopulateTabHdr( Tui_Rect const &r, bool resize, uint32_t id, const char * txt,
                            KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );
        void PopulateButton( Tui_Rect const &r, bool resize, uint32_t id, const char * txt,
                            KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );
        void PopulateCheckbox( Tui_Rect const &r, bool resize, uint32_t id, const char * txt,
                            bool checked, KTUI_color bg, KTUI_color fg, uint32_t page_id );
        void PopulateInput( Tui_Rect const &r, bool resize, uint32_t id, const char * txt,
            size_t length, KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );
        void PopulateSpinEdit( Tui_Rect const &r, bool resize, uint32_t id, tui_long value, tui_long min,
            tui_long max, KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );
        void PopulateGrid( Tui_Rect const &r, bool resize, uint32_t id, Grid &grid_model,
                            KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );
        void PopulateList( Tui_Rect const &r, bool resize, uint32_t id,
                            KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );
        void PopulateRadioBox( Tui_Rect const &r, bool resize, uint32_t id,
                            KTUI_color bg, KTUI_color fg, uint32_t page_id = 0 );

        bool HasWidget( tui_id id ) { return KTUIDlgHasWidget ( dlg_, id ); };

        tui_long GetGridCol( tui_id id ) { uint64_t col = 0; KTUIDlgGetGridCol( dlg_, id, &col ); return col; };
        bool SetGridCol( tui_id id, tui_long col ) { return ( KTUIDlgSetGridCol( dlg_, id, col ) == 0 ); };
        tui_long GetGridRow( tui_id id ) { uint64_t row = 0; KTUIDlgGetGridRow( dlg_, id, &row ); return row; };
        bool SetGridRow( tui_id id, tui_long row ) { return ( KTUIDlgSetGridRow( dlg_, id, row ) == 0 ); };

        bool RemoveWidget( tui_id id ) { return ( KTUIDlgRemove ( dlg_, id ) == 0 ); };
        bool Draw( bool forced = false ) { return ( KTUIDlgDraw( dlg_, forced ) == 0 ); };
        bool DrawWidget( tui_id id ) { return ( KTUIDlgDrawWidget( dlg_, id ) == 0 ); };
        bool DrawCaption( void ) { return ( KTUIDlgDrawCaption( dlg_ ) == 0 ); };

        bool SetFocus( tui_id id ) { return ( KTUIDlgSetFocus( dlg_, id ) == 0 ); };
        bool MoveFocus( bool forward ) { return ( KTUIDlgMoveFocus( dlg_, forward ) == 0 ); };
        bool FocusValid( void ) { return KTUIDlgFocusValid( dlg_ ); };
        tui_id GetFocusId( void ) { return KTUIDlgGetFocusId( dlg_ ); };

        bool HandleEvent( Tui_Event &ev ) { return ( KTUIDlgHandleEvent( dlg_, &( ev.ev_ ) ) == 0 ); };
        bool GetDlgEvent( tuidlg_event * event ) { return ( KTUIDlgGet ( dlg_, event ) == 0 ); };

        void SetHighLightColor( KTUI_color value );
        void SetHighLightAttr( KTUI_attrib value );
        void EnableCursorNavigation( bool enabled ) { KTUIDlgEnableCursorNavigation( dlg_, enabled ); }
        
        KTUI_color GetDlgBg( void )
        {
            KTUI_color c = KTUI_c_gray;
            KTUIPalette * p = KTUIDlgGetPalette ( dlg_ );
            if ( p != NULL )
            {
                const tui_ac * ac = KTUIPaletteGet ( p, ktuipa_dlg );
                if ( ac != NULL ) c = ac -> bg;
            }
            return c;
        }
        
        void SetDlgBg( KTUI_color value )
        {
            KTUIPalette * p = KTUIDlgGetPalette ( dlg_ );
            if ( p != NULL )
            {
                KTUIPaletteSet_bg ( p, ktuipa_dlg, value );
                Draw();
            }
        }
        
        friend class Std_Dlg_Base;
};

class Dlg_Runner
{
    private :
        Dlg &dlg_;
        void * data_;

        bool handle_dlg_event_loop( void );
        bool handle_tui_event( Tui_Event &ev );
        bool handle_dlg_event( Tui_Event &ev );


    public :
        Dlg_Runner( Dlg &dlg, void * data ) : dlg_( dlg ), data_( data ) { };
        virtual ~Dlg_Runner() {};

        void run( void );

        virtual bool on_kb_alpha( Dlg &dlg, void * data, int code )               { return false; };
        virtual bool on_kb_special_key( Dlg &dlg, void * data, KTUI_key key )     { return false; };
        virtual bool on_mouse( Dlg &dlg, void * data, tui_coord x, tui_coord y, KTUI_mouse_button button ) { return false; };
        virtual bool on_win( Dlg &dlg, void * data, tui_coord w, tui_coord h )    { return dlg_.Resize( tui::Tui_Rect( 0, 0, w, h ) ); };
        virtual bool on_focus( Dlg &dlg, void * data, Tui_Dlg_Event &dev )        { return false; };
        virtual bool on_focus_lost( Dlg &dlg, void * data, Tui_Dlg_Event &dev )   { return false; };
        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )       { return false; };
        virtual bool on_changed( Dlg &dlg, void * data, Tui_Dlg_Event &dev )      { return false; };
        virtual bool on_menu( Dlg &dlg, void * data, Tui_Dlg_Event &dev )         { return false; };
};


class Std_Dlg_Base
{
    protected :
        Dlg * parent_;
        KTUI_color bg1_, bg2_;
        bool allow_dir_create_;
        Tui_Rect R_;
        char buffer[ 1024 ];
        char caption[ 256 ];

        void prepare( struct KTUI ** tui, struct KTUIDlg ** parent_dlg )
        {
            *tui = NULL;
            *parent_dlg = NULL;
            if ( parent_ != NULL )
                *parent_dlg = parent_->dlg_;
            else
            {
                Tui * instance = Tui::getInstance();
                *tui = instance->tui_;
            }
        }

        void post_process( void )
        {
            if ( parent_ != NULL )
                parent_->Draw();
        }

    public :
        Std_Dlg_Base( void ) : parent_( NULL ), bg1_( KTUI_c_brown ), bg2_( KTUI_c_dark_green ),
                               allow_dir_create_( false ), R_( 0, 0, 35, 1 )
        {
			buffer[ 0 ] = 0;
			caption[ 0 ] = 0;
		};

        void set_parent( Dlg * parent ) { parent_ = parent; };
        void set_location( Tui_Rect const &r ) { R_.set( r ); };
        void set_colors( const KTUI_color c1, const KTUI_color c2 ) { bg1_ = c1; bg2_ = c2; };
        void set_color1( const KTUI_color c ) { bg1_ = c; };
        void set_color2( const KTUI_color c ) { bg2_ = c; };
        void allow_dir_create() { allow_dir_create_ = true; };

        void set_caption( const char * txt ) { if ( txt != NULL ) string_printf ( caption, sizeof caption, NULL, "%s", txt ); };
        void set_caption2( const std::string &txt ) { set_caption( txt.c_str() ); };

        void set_text( const char * txt ) { if ( txt != NULL ) string_printf ( buffer, sizeof buffer, NULL, "%s", txt ); };
        void set_text2( std::string &txt ) { set_text( txt.c_str() ); };
        const char * get_text( void ) { return buffer; };
        std::string get_text2( void ) { return buffer; };

        Tui_Rect center( void )
        {
            tui_coord w = R_.get_w();
            tui_coord cols, lines;
            Tui::getInstance()->GetExtent( &cols, &lines );
            return Tui_Rect( ( cols / 2 ) - ( w / 2 ), ( lines / 2 ) - 3, w, 1 );
        };
};


class Std_Dlg_Info_Line : public Std_Dlg_Base
{
    private :

        bool execute_int( void )
        {
            struct KTUI * tui = NULL;
            struct KTUIDlg * parent_dlg = NULL;
            prepare( &tui, &parent_dlg ); /* get either tui-ptr or parent_dlg-ptr for calling actual dlg-function */

            rc_t rc = TUI_ShowMessage ( tui, parent_dlg, caption, buffer, 
                                        R_.get_x(), R_.get_y(), R_.get_w(), bg1_, bg2_ );

            post_process(); /* draw the parent */
            return ( rc == 0 );
        }

    public :
        bool execute( void ) { return execute_int(); };
};


class Std_Dlg_Input : public Std_Dlg_Base
{
    private :

        bool execute_int( void )
        {
            struct KTUI * tui = NULL;
            struct KTUIDlg * parent_dlg = NULL;
            prepare( &tui, &parent_dlg ); /* get either tui-ptr or parent_dlg-ptr for calling actual dlg-function */

            bool selected;

            rc_t rc = TUI_EditBuffer( tui, parent_dlg, caption, buffer, sizeof buffer,
                                      R_.get_x(), R_.get_y(), R_.get_w(),
                                      &selected, bg1_, bg2_ );

            post_process(); /* draw the parent */
            return ( rc == 0 && selected );
        }

    public :
        bool execute( void ) { return execute_int(); };
};


class Std_Dlg_Question : public Std_Dlg_Base
{
    private :

        bool execute_int( void )
        {
            struct KTUI * tui = NULL;
            struct KTUIDlg * parent_dlg = NULL;
            prepare( &tui, &parent_dlg ); /* get either tui-ptr or parent_dlg-ptr for calling actual dlg-function */

            bool yes;

            rc_t rc = TUI_YesNoDlg ( tui, parent_dlg, caption, buffer,
                                     R_.get_x(), R_.get_y(), R_.get_w(), &yes, bg1_, bg2_ );

            post_process(); /* draw the parent */
            return ( rc == 0 && yes );
        }

    public :

        bool execute( void ) { return execute_int(); };
};


class Std_Dlg_Pick : public Std_Dlg_Base
{
    private :
        VNamelist * list_;

        bool execute_int( unsigned int * selection )
        {
            struct KTUI * tui = NULL;
            struct KTUIDlg * parent_dlg = NULL;
            prepare( &tui, &parent_dlg ); /* get either tui-ptr or parent_dlg-ptr for calling actual dlg-function */

            bool selected;
            if ( R_.get_h() < 6 ) R_.set_h( 6 );

            rc_t rc = TUI_PickFromList( tui, parent_dlg, caption, list_, selection,
                                        &( R_.r_ ), &selected, bg1_, bg2_ );

            post_process(); /* draw the parent */
            return ( rc == 0 && selected );
        }

    public :
        Std_Dlg_Pick( void ) { VNamelistMake ( &list_, 100 ); };
        virtual ~Std_Dlg_Pick( void ) { VNamelistRelease( list_ ); };

        void add_entry( const char * txt ) { VNamelistAppend ( list_, txt ); };
        bool execute( unsigned int * selection ) { return execute_int( selection ); };
};


class Std_Dlg_File_Pick : public Std_Dlg_Base
{
    private :
        char ext[ 32 ];
        uint32_t dir_h;

        bool execute_int( void )
        {
            struct KTUI * tui = NULL;
            struct KTUIDlg * parent_dlg = NULL;
            prepare( &tui, &parent_dlg ); /* get either tui-ptr or parent_dlg-ptr for calling actual dlg-function */

            bool done;
            if ( R_.get_h() < 12 ) R_.set_h( 12 );
            if ( dir_h == 0 ) dir_h = ( R_.get_h() - 6 ) / 2 ;

            rc_t rc = FileDlg ( tui, parent_dlg, buffer, sizeof buffer, ext, &done,
                                &( R_.r_ ), dir_h, bg1_, bg2_ );

            post_process(); /* draw the parent */
            return ( rc == 0 && done );
        }

    public :
        Std_Dlg_File_Pick( void ) { ext[ 0 ] = 0; dir_h = 0; };

        void set_ext( const char * txt ) { if ( txt != NULL ) string_printf ( ext, sizeof ext, NULL, "%s", txt ); };
        void set_ext2( const std::string &txt ) { set_ext( txt.c_str() ); };
        void set_dir_h( uint32_t value ) { dir_h = value; };

        bool execute( void ) { return execute_int(); };
};


class Std_Dlg_Dir_Pick : public Std_Dlg_Base
{
    private :
        bool execute_int( void )
        {
            struct KTUI * tui = NULL;
            struct KTUIDlg * parent_dlg = NULL;
            prepare( &tui, &parent_dlg ); /* get either tui-ptr or parent_dlg-ptr for calling actual dlg-function */

            bool done;
            if ( R_.get_h() < 12 ) R_.set_h( 12 );

            rc_t rc = DirDlg ( tui, parent_dlg, buffer, sizeof buffer, &done,
                               &( R_.r_ ), bg1_, bg2_, allow_dir_create_ );

            post_process(); /* draw the parent */
            return ( rc == 0 && done );
        }

    public :
        Std_Dlg_Dir_Pick( void ) { };
        bool execute( void ) { return execute_int(); };
};

class widget_colors
{
    public :
        KTUI_color bg, fg;
        
        widget_colors( KTUI_color a_bg, KTUI_color a_fg ) : bg( a_bg ), fg( a_fg ) {}
};

class msg_colors
{
    public :
        KTUI_color bg;
        widget_colors hdr, label, button;
        
        msg_colors( KTUI_color a_bg, const widget_colors &a_hdr,
                    const widget_colors &a_label, const widget_colors &a_button )
            : bg( a_bg ), hdr( a_hdr ), label( a_label ), button( a_button ) {}
};

class dflt_msg_colors : public msg_colors
{
    public :
        dflt_msg_colors( void ) : msg_colors( KTUI_c_light_gray, 
                                              widget_colors( KTUI_c_gray, KTUI_c_white ),
                                              widget_colors( KTUI_c_gray, KTUI_c_white ),
                                              widget_colors( KTUI_c_cyan, KTUI_c_black ) ) {}
};

class input_colors
{
    public :
        KTUI_color bg;    
        widget_colors label, input, button;

        input_colors( KTUI_color a_bg, const widget_colors &a_label,
                      const widget_colors &a_input, const widget_colors &a_button )
            : bg( a_bg ), label( a_label ), input( a_input ), button( a_button ) {}
};

class dflt_input_colors : public input_colors
{
    public :
        dflt_input_colors( void ) : input_colors( KTUI_c_light_gray,
                                                  widget_colors( KTUI_c_gray, KTUI_c_white ),
                                                  widget_colors( KTUI_c_white, KTUI_c_black ),
                                                  widget_colors( KTUI_c_cyan, KTUI_c_black ) ) {}
};

/* ==== message sub-dialog =================================================================== */
class msg_ctrl : public Dlg_Runner
{
    private :
        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        { dlg.SetDone( dev.get_widget_id() == 101 ); return true; }

    class msg_view : public Dlg
    {
        public :
            msg_view( Dlg &parent, Tui_Rect &r, const std::string &msg, const msg_colors &colors )
                : Dlg( parent, r )
            {
                SetDlgBg( colors.bg );
                Tui_Rect r1( 1, 1, r.get_w() -2, 1 );
                PopulateLabel( r1, false, 100, msg.c_str(), colors.label.bg, colors.label.fg );
                Tui_Rect r2( 1, 3, 12, 1 );
                PopulateButton( r2, false, 101, "&ok", colors.button.bg, colors.button.fg );
            }
            
            msg_view( Dlg &parent, Tui_Rect &r, const std::string &hdr, const std::string &msg, 
                      const msg_colors &colors )
                : Dlg( parent, r )
            {
                SetDlgBg( colors.bg );
                
                Tui_Rect r1( 1, 1, r.get_w() -3, 1 );
                PopulateLabel( r1, false, 99, hdr.c_str(), colors.hdr.bg, colors.hdr.fg );
                
                Tui_Rect r2( 1, 3, r.get_w() -3, 1 );
                PopulateLabel( r2, false, 100, msg.c_str(), colors.label.bg, colors.label.fg );
                
                Tui_Rect r3( 1, 5, 12, 1 );
                PopulateButton( r3, false, 101, "&ok", colors.button.bg, colors.button.fg );
            }
            
    };
        
    public :
        msg_ctrl( Dlg &dlg ) : Dlg_Runner( dlg, NULL ) { dlg.SetFocus( 101 ); }

        static bool show_msg( Dlg & parent, const std::string &msg, Tui_Rect r,
                              msg_colors colors = dflt_msg_colors() )
        {
            KTUI_color c = parent.GetDlgBg();
            parent.center( r );
            msg_view view( parent, r, msg, colors );
            msg_ctrl ctrl( view );
            ctrl.run();
            parent.SetDlgBg( c );
            return true;
        }
        
        static bool show_msg( Dlg & parent, const std::string &msg, tui_coord w = 80,
                              msg_colors colors = dflt_msg_colors() )
        {
            return show_msg( parent, msg, Tui_Rect( 0, 0, w, 5 ), colors );
        }
        
        static bool show_msg2( Dlg & parent, const std::string &hdr, const std::string &msg,
                               Tui_Rect r, msg_colors colors = dflt_msg_colors() )
        {
            KTUI_color c = parent.GetDlgBg();
            parent.center( r );
            msg_view view( parent, r, hdr, msg, colors );
            msg_ctrl ctrl( view );
            ctrl.run();
            parent.SetDlgBg( c );
            return true;
        }
        
        static bool show_msg2( Dlg & parent, const std::string &hdr, const std::string &msg,
                               tui_coord w = 80, msg_colors colors = dflt_msg_colors() )
        {
            return show_msg2( parent, hdr, msg, Tui_Rect( 0, 0, w, 7 ), colors );
        }

        static bool show_err( Dlg & parent, const std::string &hdr, const std::string &msg,
                              tui_coord w = 80, msg_colors colors = dflt_msg_colors() )
        {
            colors . hdr . fg = KTUI_c_red;
            return show_msg2( parent, hdr, msg, Tui_Rect( 0, 0, w, 7 ), colors );
        }
        
};

/* ==== question sub-dialog =================================================================== */
class question_ctrl : public Dlg_Runner
{
    private :
        bool answer;

        class question_view : public Dlg
        {
            public :
                question_view( Dlg &parent, Tui_Rect r, const std::string &msg, const msg_colors &colors ) 
                    : Dlg( parent, r )
                {
                    SetDlgBg( colors.bg );
                    Tui_Rect r1( 1, 1, r.get_w() -2, 1 );
                    PopulateLabel( r1, false, 100, msg.c_str(), colors.label.bg, colors.label.fg );
                    Tui_Rect r2( 1, 3, 12, 1 );
                    PopulateButton( r2, false, 101, "&yes", colors.button.bg, colors.button.fg );
                    Tui_Rect r3( 14, 3, 12, 1 );
                    PopulateButton( r3, false, 102, "&no", colors.button.bg, colors.button.fg );
                }
        };
        
        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            switch ( dev.get_widget_id() )
            {
                case 101 : answer = true; dlg.SetDone( true ); break;
                case 102 : answer = false; dlg.SetDone( true ); break;
            }
            return true;
        }

    public :
        question_ctrl( Dlg &dlg ) : Dlg_Runner( dlg, NULL ), answer( false ) { dlg.SetFocus( 101 ); }

        static bool question( Dlg &parent, const std::string &msg, Tui_Rect r,
                              msg_colors colors = dflt_msg_colors() )
        {
            KTUI_color c = parent.GetDlgBg();
            parent.center( r );
            question_view view( parent, r, msg, colors );
            question_ctrl ctrl( view );
            ctrl.run();
            parent.SetDlgBg( c );
            return ctrl . answer;
        }
        
        static bool question( Dlg &parent, const std::string &msg, tui_coord w = 80,
                              msg_colors colors = dflt_msg_colors() )
        {
            return question( parent, msg, Tui_Rect( 0, 0, w, 5 ), colors );
        }
};

/* ==== input sub-dialog =================================================================== */
class input_ctrl : public Dlg_Runner
{
    private :
        std::string &txt;
        bool ok;

        class input_view : public Dlg
        {
            public :
                input_view( Dlg &parent, Tui_Rect r,
                            const std::string &caption, const std::string &txt,
                            uint32_t txt_len, input_colors &colors ) : Dlg( parent, r )
                {
                    SetDlgBg( colors.bg );
                    Tui_Rect r1( 1, 1, r.get_w() - 2, 1 );
                    PopulateLabel( r1, false, 100, caption.c_str(), colors.label.bg, colors.label.fg );
                    Tui_Rect r2( 1, 2, r.get_w() - 2, 1 );
                    PopulateInput( r2, false, 101, txt.c_str(), txt_len, colors.input.bg, colors.input.fg );
                    Tui_Rect r3( 1, 4, 12, 1 );
                    PopulateButton( r3, false, 102, "&ok", colors.button.bg, colors.button.fg );
                    Tui_Rect r4( 14, 4, 12, 1 );
                    PopulateButton( r4, false, 103, "&cancel", colors.button.bg, colors.button.fg );
                }
        };

        virtual bool on_changed( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            tui_id id = dev.get_widget_id();
            if ( id == 101 ) txt = dlg.GetWidgetText( id );
            return true;
        }

        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            switch ( dev.get_widget_id() )
            {
                case 102 : ok = true; dlg.SetDone( true ); break;
                case 103 : ok = false; dlg.SetDone( true ); break;
            }
            return true;
        }

    public :
        input_ctrl( Dlg &dlg, std::string &a_txt ) : Dlg_Runner( dlg, NULL ), txt( a_txt ), ok( false )
        { dlg.SetFocus( 101 ); }

        static bool input( Dlg &parent, const std::string &caption, std::string &txt, uint32_t txt_len,
                           Tui_Rect r, input_colors colors = dflt_input_colors() )
        {
            KTUI_color c = parent.GetDlgBg();
            parent.center( r );
            input_view view( parent, r, caption, txt, txt_len, colors );
            input_ctrl ctrl( view, txt );
            ctrl.run();
            parent.SetDlgBg( c );
            return ctrl . ok;
        }

        static bool input( Dlg &parent, const std::string &caption, std::string &txt, uint32_t txt_len,
                           tui_coord w = 80, input_colors colors = dflt_input_colors() )
        {
            return input( parent, caption, txt, txt_len, Tui_Rect( 0, 0, w, 6 ), colors );
        }

};

/* ==== pick path sub-dialog =================================================================== */
class pick_path_ctrl : public Dlg_Runner
{
    private :
        std::string path;
        bool ok;

        class pick_path_view : public Dlg
        {
            public :
                pick_path_view( Dlg &parent, Tui_Rect r, const std::string &caption,
                                const std::string &path, input_colors &colors ) : Dlg( parent, r )
                {
                    SetDlgBg( colors.bg );
                    
                    Tui_Rect r1( 0, 0, r.get_w(), 1 );
                    PopulateLabel( r1, false, 100, caption.c_str(), colors.label.bg, colors.label.fg );

                    Tui_Rect r2( 1, 2, r.get_w() - 2, 1 );
                    PopulateLabel( r2, false, 101, path.c_str(), colors.label.bg, colors.label.fg );
                    
                    Tui_Rect r3( 1, 4, r.get_w() - 2, r.get_h() - 7 );
                    PopulateList( r3, false, 102, colors.label.bg, colors.label.fg );
                    
                    Tui_Rect r4( 1, r.get_h() - 2, 12, 1 );
                    PopulateButton( r4, false, 103, "&ok", colors.button.bg, colors.button.fg );
                    Tui_Rect r5( 14, r.get_h() - 2, 12, 1 );
                    PopulateButton( r5, false, 104, "&cancel", colors.button.bg, colors.button.fg );
                   
                }
        };

        virtual bool on_select( Dlg &dlg, void * data, Tui_Dlg_Event &dev )
        {
            switch ( dev.get_widget_id() )
            {
                case 103 : ok = true; dlg.SetDone( true ); break;
                case 104 : ok = false; dlg.SetDone( true ); break;
            }
            return true;
        }

    public :
        pick_path_ctrl( Dlg &dlg, const std::string &a_path ) : Dlg_Runner( dlg, NULL ),
                path( a_path ), ok( false ) { dlg.SetFocus( 102 ); }
        
        static bool pick( Dlg &parent, const std::string &caption, std::string &path,
                          Tui_Rect r, input_colors colors = dflt_input_colors() )
        {
            KTUI_color c = parent.GetDlgBg();
            pick_path_view view( parent, r, caption, path, colors );
            pick_path_ctrl ctrl( view, caption );
            ctrl.run();
            parent.SetDlgBg( c );
            if ( ctrl . ok ) path = ctrl.path;
            return ctrl . ok;
        }

        static bool pick( Dlg &parent, const std::string &caption, std::string &path,
                          input_colors colors = dflt_input_colors() )
        {
            Tui_Rect r;
            parent.GetRect( r );
            return pick( parent, caption, path, r, colors );
        }
};

} // namespace tui

#endif // _hpp_tui_
