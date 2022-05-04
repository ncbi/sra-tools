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
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/vector.h>
#include <klib/namelist.h>
#include <klib/out.h>

#include <tui/tui.h>
#include <tui/tui_dlg.h>
#include "tui_menu.h"
#include "tui_widget.h"

#include <sysalloc.h>
#include <stdlib.h>

/* a menu is a tree-structure of nodes,
   each node has a text and a integer element and a vector of child-nodes
*/

static const char tuimenu_classname [] = "KTUIMenu";

typedef struct KTUI_Menu_Node
{
    Vector children;
    const char * name;
    uint32_t id, selected, name_len, max_child_name_len;
    bool expanded;
    char shortcut;
} KTUI_Menu_Node;


typedef struct KTUI_Menu
{
    KRefcount refcount;
    KTUI_Menu_Node * root;
} KTUI_Menu;


typedef struct menu_ac
{
    const tui_ac * dflt;
    const tui_ac * select;
    const tui_ac * high;
} menu_ac;


static void destroy_node( struct KTUI_Menu_Node * node );

static void CC destroy_node_callback ( void *item, void *data )
{
    destroy_node( ( KTUI_Menu_Node * ) item );
}

static void destroy_node( struct KTUI_Menu_Node * node )
{
    VectorWhack ( &node->children, destroy_node_callback, NULL );
    free( ( void * ) node->name );
    free( node );
}

static void destroy_menu( struct KTUI_Menu * self )
{
    destroy_node( self->root );
    free( self );
}


static KTUI_Menu_Node * make_node ( KTUI_Menu_Node * parent, const char * name, uint32_t id, char shortcut )
{
    KTUI_Menu_Node * res = malloc( sizeof * res );
    if ( res != NULL )
    {
        size_t l;
        res->name = string_dup_measure ( name, &l );
        res->name_len = ( uint32_t )l;
        res->id = id;
        res->shortcut = shortcut;
        res->selected = 0;
        res->max_child_name_len = 0;
        VectorInit ( &res->children, 0, 12 );
        if ( parent != NULL )
        {
            VectorAppend ( &parent->children, NULL, res );
            if ( res->name_len > parent->max_child_name_len )
                parent->max_child_name_len = res->name_len;
        }
    }
    return res;
}


static int nlt_strcmp( const char* s1, const char* s2 )
{
    size_t n1 = string_size ( s1 );
    size_t n2 = string_size ( s2 );
    uint32_t maxchar = ( n1 < n2 ) ? ( uint32_t )n2 : ( uint32_t )n1;
    return string_cmp ( s1, n1, s2, n2, maxchar );
}


static KTUI_Menu_Node * find_node_by_name_flat ( KTUI_Menu_Node * root, const char * name )
{
    KTUI_Menu_Node * res = NULL;
    if ( root != NULL && name != NULL && name[ 0 ] != 0 )
    {
        uint32_t count = VectorLength( &root->children );
        if ( count > 0 )
        {
            uint32_t idx;
            for ( idx = VectorStart( &root->children ); idx < count && res == NULL; ++idx )
            {
                KTUI_Menu_Node * node = VectorGet ( &root->children, idx );
                if ( nlt_strcmp( node->name, name ) == 0 )
                    res = node;
            }
        }
    }
    return res;
}


#if 0
static KTUI_Menu_Node * find_node_by_name ( KTUI_Menu_Node * root, const char * name )
{
    KTUI_Menu_Node * res = NULL;
    if ( root != NULL && name != NULL && name[ 0 ] != 0 )
    {
        if ( nlt_strcmp( root->name, name ) == 0 )
            res = root;
        else
        {
            uint32_t count = VectorLength( &root->children );
            if ( count > 0 )
            {
                uint32_t idx;
                for ( idx = VectorStart( &root->children ); idx < count && res == NULL; ++idx )
                {
                    KTUI_Menu_Node * node = VectorGet ( &root->children, idx );
                    res = find_node_by_name ( node, name ); /* recursion */
                }
            }
        }
    }
    return res;
}
#endif

static bool find_node_by_id ( KTUI_Menu_Node * node, uint32_t id, KTUI_Menu_Node ** found, KTUI_Menu_Node ** parent )
{
    bool res = false;
    *found = NULL;
    *parent = NULL;
    if ( node != NULL )
    {
        if ( node->id == id )
            *found = node;
        else
        {
            uint32_t count = VectorLength( &node->children );
            if ( count > 0 )
            {
                uint32_t idx;
                for ( idx = VectorStart( &node->children ); idx < count && !res; ++idx )
                {
                    KTUI_Menu_Node * sub = VectorGet ( &node->children, idx );
                    res = ( sub->id == id );
                    if ( res )
                    {
                        *found = sub;
                        *parent = node;
                    }
                    else
                        res = find_node_by_id ( sub, id, found, parent ); /* recursion */
                }
            }
        }
    }
    return res;
}


LIB_EXPORT rc_t CC KTUI_Menu_Make ( struct KTUI_Menu ** self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        KTUI_Menu * m = malloc( sizeof * m );
        if ( m == NULL )
            rc = RC( rcApp, rcAttr, rcCreating, rcMemory, rcExhausted );
        else
        {
            KRefcountInit( &m->refcount, 1, "KTUI_Menu", "make", tuimenu_classname );
            m->root = make_node ( NULL, "root", 0, ' ' );
            m->root->expanded = true;
            ( * self ) = m;
        }
    }
    return rc;
}


LIB_EXPORT rc_t CC KTUI_Menu_AddRef ( const struct KTUI_Menu * self )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else
    {
        switch ( KRefcountAdd( &self->refcount, tuimenu_classname ) )
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


LIB_EXPORT rc_t CC KTUI_Menu_Release ( struct KTUI_Menu * self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        switch ( KRefcountDrop( &self->refcount, tuimenu_classname ) )
        {
        case krefOkay :
        case krefZero :
            break;
        case krefWhack : destroy_menu( ( struct KTUI_Menu * ) self ); break;
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


static rc_t append_segment( const char * path, VNamelist * segments, uint32_t start, uint32_t end )
{
    String S;
    uint32_t l = ( end-start ) + 1;
    StringInit( &S, &path[ start ], l, l );
    return VNamelistAppendString ( segments, &S );
}


static rc_t decompose_path( const char * path, VNamelist * segments )
{
    rc_t rc = 0;
    uint32_t len = string_measure ( path, NULL );
    if ( len > 1 && path[ 0 ] == '/' )
    {
        uint32_t i, start = 1, end = 1;
        for ( i = 1; i < len && rc == 0; ++ i )
        {
            if ( path[ i ] == '/' )
            {
                if ( end > start )
                {
                    rc = append_segment( path, segments, start, end );
                    start = end = i + 1;
                }
                else
                    rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcInvalid );
            }
            else
                end = i;
        }
        if ( rc == 0 && ( end > start ) )
            rc = append_segment( path, segments, start, end );
    }
    else
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcInvalid );
    return rc;
}


LIB_EXPORT rc_t CC KTUI_Menu_Add ( struct KTUI_Menu * self, const char * path, uint32_t id, char shortcut )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcCreating, rcSelf, rcNull );
    else if ( path == NULL || path[ 0 ] == 0 )
        rc = RC( rcApp, rcAttr, rcCreating, rcParam, rcNull );
    {
        /* take the path apart into a VNamelist with '/' as separator */
        VNamelist * segments;
        rc = VNamelistMake ( &segments, 5 );
        if ( rc == 0 )
        {
            rc = decompose_path( path, segments );
            if ( rc == 0 )
            {
                uint32_t count;
                rc = VNameListCount ( segments, &count );
                if ( rc == 0 && count > 0 )
                {
                    uint32_t i;
                    KTUI_Menu_Node * node = self->root;
                    for ( i = 0; i < count && node != NULL && rc == 0; ++i )
                    {
                        const char * segment;
                        rc = VNameListGet ( segments, i, &segment );
                        if ( rc == 0 && segment != NULL )
                        {
                            KTUI_Menu_Node * sub = find_node_by_name_flat ( node, segment );
                            if ( sub == NULL )
                            {
                                if ( i == ( count - 1 ) )
                                    sub = make_node ( node, segment, id, shortcut );
                                else
                                    sub = make_node ( node, segment, 0, 0 );
                            }
                            node = sub;
                        }
                    }
                }
            }
            VNamelistRelease ( segments );
        }
    }
    return rc;
}


static bool find_index_of_child( KTUI_Menu_Node * node, uint32_t child_id, uint32_t * idx )
{
    bool res = false;
    uint32_t count = VectorLength( &node->children );
    if ( count > 0 )
    {
        uint32_t i;
        for ( i = VectorStart( &node->children ); i < count && !res; ++i )
        {
            KTUI_Menu_Node * child = VectorGet ( &node->children, i );
            res = ( child->id == child_id );
        }
    }
    return res;
}


LIB_EXPORT rc_t CC KTUI_Menu_Remove_Id ( struct KTUI_Menu * self, uint32_t id )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcApp, rcAttr, rcRemoving, rcSelf, rcNull );
    else
    {
        KTUI_Menu_Node * node;
        KTUI_Menu_Node * parent;
        bool found = find_node_by_id ( self->root, id, &node, &parent );
        if ( !found )
            rc = RC( rcApp, rcAttr, rcRemoving, rcItem, rcNotFound );
        else
        {
            /* find out which index the node has in it's parents list */
            uint32_t idx;
            found = find_index_of_child( parent, id, &idx );
            if ( !found )
                rc = RC( rcApp, rcAttr, rcRemoving, rcItem, rcInvalid ); /* something is wrong ! */
            else
            {
                void * removed;
                rc = VectorRemove ( &parent->children, idx, &removed );
                if ( rc == 0 )
                    destroy_node( ( KTUI_Menu_Node * ) removed );
            }
        }
    }
    return rc;
}


void KTUI_Menu_Report_Node ( KTUI_Menu_Node * node, uint32_t depth )
{
    uint32_t count = VectorLength( &node->children );
    KOutMsg( "%*s [ %.03d ] %s ( %d children )\n", depth + 1, "-", node->id, node->name, count );
    if ( count > 0 )
    {
        uint32_t i;
        for ( i = VectorStart( &node->children ); i < count; ++i )
        {
            KTUI_Menu_Node * child = VectorGet ( &node->children, i );
            if ( child != NULL )
                KTUI_Menu_Report_Node ( child, depth + 1 ); /* recursion! */
        }
    }
}


void KTUI_Menu_Report ( struct KTUI_Menu * self )
{
    if ( self == NULL )
        KOutMsg( "KTUI_Menu_Report: self == NULL" );
    else if ( self->root == NULL )
        KOutMsg( "KTUI_Menu_Report: self->root == NULL" );
    else
        KTUI_Menu_Report_Node ( self->root, 0 );
}



static void draw_node_txt( struct KTUI * tui, tui_point * p, menu_ac * m_ac,
                           const char * txt, char shortcut, bool selected )
{
    size_t txt_size;
    uint32_t txt_len = string_measure ( txt, &txt_size );
    const tui_ac * ac = selected ? m_ac->select : m_ac->dflt;
    DlgWrite( tui, p->x + 1, p->y, ac, txt, txt_len );
    if ( shortcut != 0 )
    {
        char * sptr = string_chr ( txt, txt_size, shortcut );
        if ( sptr != NULL )
        {
            tui_ac ach;
            copy_ac( &ach, ac );
            ach.fg = m_ac->high->fg;
            DlgWrite( tui, p->x + 1 + ( uint32_t )( sptr - txt ), p->y, &ach, sptr, 1 );
        }
    }
}


static void draw_node_v( KTUI_Menu_Node * node, struct KTUI * tui, tui_point * p, menu_ac * m_ac )
{
    if ( node != NULL )
    {
        uint32_t count = VectorLength( &node->children );
        if ( count > 0 )
        {
            uint32_t i, w = node->max_child_name_len + 2;
            draw_background( tui, false, p, w, count, m_ac->dflt->bg );

            for ( i = VectorStart( &node->children ); i < count; ++i )
            {
                KTUI_Menu_Node * child = VectorGet ( &node->children, i );
                if ( child != NULL )
                {
                    bool selected = ( node->selected == i );
                    draw_node_txt( tui, p, m_ac, child->name, child->shortcut, selected );
                    if ( child->expanded && selected )
                    {
                        tui_point p_sub;
                        p_sub.x = p->x + w;
                        p_sub.y = p->y;
                        draw_node_v( child, tui, &p_sub, m_ac ); /* recursion here ! */
                    }
                    p->y++;
                }
            }
        }
    }
}


static void draw_node_h( KTUI_Menu_Node * node, struct KTUI * tui,
                         const tui_rect * r, menu_ac * m_ac )
{
    if ( node != NULL )
    {
        uint32_t count = VectorLength( &node->children );
        if ( count > 0 )
        {
            uint32_t i, x;
            for ( x = r->top_left.x, i = VectorStart( &node->children ); i < count; ++i )
            {
                KTUI_Menu_Node * child = VectorGet ( &node->children, i );
                if ( child != NULL )
                {
                    tui_point p;
                    bool selected = ( node->selected == i );
                    p.x = x + 1;
                    p.y = r->top_left.y;
                    draw_node_txt( tui, &p, m_ac, child->name, child->shortcut, selected && !child->expanded );
                    if ( selected && child->expanded )
                    {
                        p.x = x + 1;
                        p.y = r->top_left.y + 1;
                        draw_node_v( child, tui, &p, m_ac );
                    }
                    x += ( child->name_len + 2 );
                }
            }
        }
    }
}


static rc_t get_abs_dlg_rect( struct KTUIDlg * dlg, tui_rect * r )
{
    tui_rect rr;
    rc_t rc = KTUIDlgGetRect ( dlg, &rr );
    if ( rc == 0 )
        rc = KTUIDlgAbsoluteRect ( dlg, r, &rr );
    return rc;
}


void KTUI_Menu_Draw( struct KTUI_Menu * self, struct KTUIDlg * dlg )
{
    tui_rect r;
    rc_t rc = get_abs_dlg_rect( dlg, &r );
    if ( rc == 0 )
    {
        const struct KTUIPalette * pa = KTUIDlgGetPalette ( dlg );
        if ( pa != NULL )
        {
            menu_ac m_ac;
            m_ac.dflt   = KTUIPaletteGet ( pa, ktuipa_menu );
            m_ac.select = KTUIPaletteGet ( pa, ktuipa_menu_sel );
            m_ac.high   = KTUIPaletteGet ( pa, ktuipa_menu_hi );
            if ( m_ac.dflt != NULL && m_ac.select != NULL && m_ac.high != NULL )
            {
                struct KTUI * tui = KTUIDlgGetTui ( dlg );
                if ( tui != NULL )
                {
                    rc_t rc = draw_background( tui, false, ( tui_point * )&r.top_left, r.w, 1, m_ac.dflt->bg );
                    if ( rc == 0 )
                        draw_node_h( self->root, tui, &r, &m_ac );
                }
            }
        }
    }
}


static KTUI_Menu_Node * get_sub_node( KTUI_Menu_Node * node, uint32_t idx )
{
    if ( node != NULL )
        return VectorGet ( &node->children, idx );
    else
        return NULL;
}


static bool shift_node_selection( KTUI_Menu_Node * node, int32_t by )
{
    bool res = false;
    uint32_t count = VectorLength( &node->children );
    if ( count > 0 )
    {
        int32_t selected = node->selected;
        selected += by;
        while ( selected < 0 )
            selected += count;
        while ( ( uint32_t )selected >= count )
            selected -= count;
        res = ( node->selected != selected );
        if ( res ) node->selected = selected;
    }
    return res;
}


static bool menu_select_h( struct KTUI_Menu * self, int32_t by )
{
    bool res = false;
    KTUI_Menu_Node * node = self->root;
    if ( node != NULL )
        res = shift_node_selection( node, by );
    return res;
}

static KTUI_Menu_Node * menu_get_expanded_node( struct KTUI_Menu * self )
{
    KTUI_Menu_Node * node = self->root;
    if ( node != NULL )
        node = get_sub_node( node, node->selected );
    while ( node != NULL && node->expanded )
        node = get_sub_node( node, node->selected );
    return node;
}


static bool selected_node_expanded( KTUI_Menu_Node * node )
{
    bool res = false;
    KTUI_Menu_Node * sub_node = get_sub_node( node, node->selected );
    if ( sub_node != NULL )
        res = sub_node->expanded;
    return res;
}


static bool menu_select_v( struct KTUI_Menu * self, int32_t by )
{
    bool res = false;

    KTUI_Menu_Node * node = self->root;
    if ( node != NULL )
        node = get_sub_node( node, node->selected );
    while ( node != NULL && selected_node_expanded( node ) )
        node = get_sub_node( node, node->selected );

    if ( node != NULL )
    {
        if ( !node->expanded && ( VectorLength( &node->children ) > 0 ) )
        {
            node->expanded = true;
            res = true;
        }
        else
        {
            if ( node->selected == 0 && by < 0 )
            {
                node->expanded = false;
                res = true;
            }
            else
                res = shift_node_selection( node, by );
        }
    }
    return res;
}


static bool menu_select( struct KTUI_Menu * self, struct KTUIDlg * dlg )
{
    bool res = false;
    KTUI_Menu_Node * node = menu_get_expanded_node( self );
    if ( node != NULL )
    {
        uint32_t count = VectorLength( &node->children );
        if ( count == 0 )
            KTUIDlgPushEvent( dlg, ktuidlg_event_menu, node->id, 0, 0, NULL );
        else
        {
            node->expanded = !node->expanded;
            res = true;
        }
    }
    return res;
}


static KTUI_Menu_Node * x_coordinate_to_top_node( KTUI_Menu_Node * node, uint32_t x, uint32_t * idx )
{
    KTUI_Menu_Node * res = NULL;
    if ( node != NULL )
    {
        uint32_t count = VectorLength( &node->children );
        if ( count > 0 )
        {
            uint32_t i, x1;
            for ( x1 = 0, i = VectorStart( &node->children ); i < count && res == NULL; ++i )
            {
                KTUI_Menu_Node * child = VectorGet ( &node->children, i );
                if ( child != NULL )
                {
                    uint32_t w = child->name_len + 2;
                    if ( x >= x1 && x <= x1 + w )
                    {
                        res = child;
                        *idx = i;
                    }
                    else
                        x1 += w;
                }
            }
        }
    }
    return res;
}


static bool on_mouse( struct KTUI_Menu * self, struct KTUIDlg * dlg, tui_rect * r, uint32_t x, uint32_t y )
{
    bool res = false;
    if ( y >= r->top_left.y )
    {
        /* the mouse was not left of the windows */
        uint32_t idx;
        KTUI_Menu_Node * top_node = x_coordinate_to_top_node( self->root, x - r->top_left.x, &idx );
        if ( top_node != NULL )
        {
            int32_t h_idx = y;
            h_idx -= r->top_left.y;
            if ( h_idx == 0 )
            {
                /* the mouse is in the top menu-line */
                res = ( self->root->selected != idx || !top_node->expanded );
                if ( res )
                {
                    top_node->expanded = true;
                    self->root->selected = idx;
                }
            }
            else
            {
                /* the mouse is not in the top menu-line */
                res = ( self->root->selected == idx && top_node->expanded && ( h_idx > 0 ) );
                if ( res )
                {
                    /* the mouse is below the the top menu-line and the menu is expanded and selected */
                    uint32_t count = VectorLength( &top_node->children );
                    h_idx--;
                    res = ( ( uint32_t )h_idx < count );
                    if ( res )
                    {
                        KTUI_Menu_Node * sub_node = get_sub_node( top_node, h_idx );
                        top_node->selected = h_idx;
                        if ( sub_node != NULL )
                        {
                            count = VectorLength( &sub_node->children );
                            if ( count == 0 )
                                KTUIDlgPushEvent( dlg, ktuidlg_event_menu, sub_node->id, 0, 0, NULL );
                            else
                            {

                            }
                        }
                    }
                }
            }
        }
    }
    return res;
}


static bool menu_all_shortcut( KTUI_Menu_Node * node, KTUI_Menu_Node * parent, uint32_t node_id, struct KTUIDlg * dlg, char key )
{
    bool res = false;
    if ( node != NULL )
    {
        uint32_t idx, count = VectorLength( &node->children );
        res = ( node->shortcut == key );
        if ( res )
        {
            /* we have found a node with the requested shortcut */
            if ( count == 0 )
            {
                /* the node has no sub-nodes: we have found what we were looking for! */
                KTUIDlgPushEvent( dlg, ktuidlg_event_menu, node->id, 0, 0, NULL );
            }
            else
            {
                /* the node has sub-nodes: we have to dig deeper */
                if ( !node->expanded )
                {
                    /* the node is not expanded: let's expand it, do not examime it's sub-nodes */
                    node->expanded = true;
                }
                else
                {
                    if ( parent != NULL && parent->selected == node_id )
                    {
                        bool sub_res = false;
                        /* the node is expanded: lets dig deeper into it's subnodes */
                        for ( idx = 0; idx < count && !sub_res; ++idx )
                            sub_res = menu_all_shortcut( VectorGet ( &node->children, idx ), node, idx, dlg, key ); /* recursion */
                    }
                }
                /* in any case we have an expanded subnode now, select it too now */
                if ( parent != NULL )
                    parent->selected = node_id;
            }
        }
        else
        {
            /* the node does not match the requested shortcut, let's inspect it's subnodes */
            for ( idx = 0; idx < count && !res; ++idx )
                res = menu_all_shortcut( VectorGet ( &node->children, idx ), node, idx, dlg, key ); /* recursion */
        }
    }
    return res;
}


/* try to find the key in the expanded path */
static bool menu_path_shortcut( KTUI_Menu_Node * node, struct KTUIDlg * dlg, char key )
{
    bool res = false;
    if ( node != NULL )
    {
        uint32_t count = VectorLength( &node->children );
        res = ( node->shortcut == key && count == 0 );
        if ( res )
            KTUIDlgPushEvent( dlg, ktuidlg_event_menu, node->id, 0, 0, NULL );
        else
            res = menu_path_shortcut( VectorGet ( &node->children, node->selected ), dlg, key ); /* recursion here */
    }
    return res;
}


/* try to find the key in the child-vector of root ( does expand/un-expand top-level drop-down menus ) */
static bool menu_root_shortcut( KTUI_Menu_Node * root, struct KTUIDlg * dlg, char key )
{
    bool res = false;
    if ( root != NULL )
    {
        uint32_t idx, count = VectorLength( &root->children );
        for ( idx = 0; idx < count && !res; ++idx )
        {
            KTUI_Menu_Node * node = VectorGet ( &root->children, idx );
            res = ( node != NULL && node->shortcut == key );
            if ( res )
            {
                uint32_t c_count = VectorLength( &node->children );
                if ( c_count == 0 )
                    KTUIDlgPushEvent( dlg, ktuidlg_event_menu, node->id, 0, 0, NULL );
                else
                {
                    if ( root->selected == idx )
                        node->expanded = !node->expanded;
                    else
                    {
                        node->expanded = true;
                        root->selected = idx;
                    }
                }
            }
        }
    }
    return res;
}


/*
static bool is_the_selected_node_expanded( KTUI_Menu_Node * node )
{
    bool res = false;
    if ( node != NULL && node->expanded )
    {
        KTUI_Menu_Node * sub = VectorGet ( &node->children, node->selected );
        if ( sub != NULL )
            res = sub->expanded;
    }
    return res;
}
*/

static bool menu_shortcut( struct KTUI_Menu * self, struct KTUIDlg * dlg, tui_event * event )
{
    char key = event->data.kb_data.key;
    bool res = menu_path_shortcut( self->root, dlg, key );
    if ( !res )
        res = menu_root_shortcut( self->root, dlg, key );
    if ( !res )
        res = menu_all_shortcut( self->root, NULL, 0, dlg, key );
    return res;
}


bool KTUI_Menu_Event( struct KTUI_Menu * self, struct KTUIDlg * dlg, tui_event * event )
{
    bool res = false;
    if ( event->event_type == ktui_event_kb )
    {
        switch( event->data.kb_data.code )
        {
            case ktui_left  : res = menu_select_h( self, -1 ); break;
            case ktui_right : res = menu_select_h( self, +1 ); break;
            case ktui_up    : res = menu_select_v( self, -1 ); break;
            case ktui_down  : res = menu_select_v( self, +1 ); break;
            case ktui_enter : res = menu_select( self, dlg ); break;
            case ktui_alpha : res = menu_shortcut( self, dlg, event ); break;
        }
    }
    else if ( event->event_type == ktui_event_mouse )
    {
        tui_rect r;
        rc_t rc = get_abs_dlg_rect( dlg, &r );
        if ( rc == 0 )
            res = on_mouse( self, dlg, &r, event->data.mouse_data.x, event->data.mouse_data.y );
    }
    return res;
}
