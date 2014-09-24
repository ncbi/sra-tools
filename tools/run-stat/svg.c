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

#include <klib/log.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>
#include <kfs/directory.h>
#include <kfs/file.h>

#include <os-native.h>

#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "svg.h"

rc_t svg_path_init( p_svg_path path )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( path != NULL )
        rc = char_buffer_init( &path->buf, 250 );
    return rc;
}

void svg_path_destroy( p_svg_path path )
{
    if ( path != NULL )
        char_buffer_destroy( &path->buf );
}


rc_t svg_path_close( p_svg_path path )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( path != NULL )
        rc = char_buffer_printf( &path->buf, 2, "Z " );
    return rc;
}


static rc_t svg_path_2_to( p_svg_path path, const char c, int32_t x, int32_t y )
{
    if ( path != NULL )
        return char_buffer_printf( &path->buf, 25, "%c%d %d ", c, x, y );
    else
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
}


rc_t svg_path_move_to( p_svg_path path, bool absolute, int32_t x, int32_t y )
{
    return svg_path_2_to( path, absolute ? 'M' : 'm', x, y );
}


rc_t svg_path_line_to( p_svg_path path, bool absolute, int32_t x, int32_t y )
{
    return svg_path_2_to( path, absolute ? 'L' : 'l', x, y );
}


static rc_t svg_path_1_to( p_svg_path path, const char c, int32_t x )
{
    if ( path != NULL )
        return char_buffer_printf( &path->buf, 25, "%c%d ", c, x );
    else
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
}


rc_t svg_path_hline_to( p_svg_path path, bool absolute, int32_t x )
{
    return svg_path_1_to( path, absolute ? 'H' : 'h', x );
}


rc_t svg_path_vline_to( p_svg_path path, bool absolute, int32_t y )
{
    return svg_path_1_to( path, absolute ? 'V' : 'v', y );
}


rc_t svg_path_smooth_to( p_svg_path path, bool absolute, int32_t x, int32_t y )
{
    return svg_path_2_to( path, absolute ? 'T' : 't', x, y );
}


static rc_t svg_path_4_to( p_svg_path path, const char c, int32_t x1, int32_t y1,
                           int32_t x, int32_t y )
{
    if ( path != NULL )
        return char_buffer_printf( &path->buf, 50, "%c%d,%d %d,%d ", c, x1, y1, x, y );
    else
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
}

/* cubic bezier curve (x1,y1 ... control point / x,y ... endpoint ) */
rc_t svg_path_bez3_to( p_svg_path path, bool absolute,
                       int32_t x1, int32_t y1, int32_t x, int32_t y )
{
    return svg_path_4_to( path, absolute ? 'S' : 's', x1, y1, x, y );
}

/* quadratic bezier curve (x1,y1 ... control point / x,y ... endpoint ) */
rc_t svg_path_bez2_to( p_svg_path path, bool absolute,
                       int32_t x1, int32_t y1, int32_t x, int32_t y )
{
    return svg_path_4_to( path, absolute ? 'Q' : 'q', x1, y1, x, y );
}


/* quadratic bezier curve (x1,y1 x2,y2 ... control point / x,y ... endpoint ) */
rc_t svg_path_curve_to( p_svg_path path, bool absolute,
        int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x, int32_t y )
{
    if ( path != NULL )
    {
        char c = absolute ? 'C' : 'c';
        return char_buffer_printf( &path->buf, 75, "%c%d,%d %d,%d %d,%d ",
                                   c, x1, y1, x2, y2, x, y );
    }
    else
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
}


/* arc ( rx, ry ....... elliptical radius in x/y-direction
         rotx   ....... rotation around x-axis
         large-flag ... choose the large section of the arc
         sweep-flag ... draw in positive-angle-direction
         x,y .......... endpoint ) */
rc_t svg_path_arc( p_svg_path path, bool absolute,
                   int32_t rx, int32_t ry, int32_t rot_x, 
                   bool large_flag, bool sweep_flag,
                   int32_t x, int32_t y )
{
    if ( path != NULL )
    {
        char c = absolute ? 'A' : 'a';
        int32_t lf = large_flag ? 1 : 0;
        int32_t sf = sweep_flag ? 1 : 0;
        return char_buffer_printf( &path->buf, 75, "%c%d,%d %d %d,%d %d,%d ",
                                   c, rx, ry, rot_x, lf, sf, x, y );
    }
    else
        return RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
}

typedef struct svg_write_ctx
{
    KFile * f;
    uint64_t pos;
} svg_write_ctx;
typedef svg_write_ctx* p_svg_write_ctx;


static rc_t svg_write_txt( p_svg_write_ctx ctx, const char * s )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( ctx != NULL && s != NULL )
    {
        size_t num_writ;
        size_t len = string_size( s );
        rc = KFileWrite ( ctx->f, ctx->pos, s, len, &num_writ );
        if ( rc == 0 )
            ctx->pos += num_writ;
    }
    return rc;
}

typedef struct svg_style
{
    char * name;
    char * style;
} svg_style;
typedef svg_style* p_svg_style;


static p_svg_style svg_make_style( const char * name, const char * style )
{
    p_svg_style res = calloc( 1, sizeof( struct svg_style ) );
    if ( res != NULL )
    {
        res->name  = string_dup_measure( name, NULL );
        res->style = string_dup_measure( style, NULL );
    }
    return res;
}


static void CC svg_style_whack( void *item, void *data )
{
    p_svg_style style = item;
    if ( style != NULL )
    {
        if ( style->name  != NULL ) free( style->name );
        if ( style->style != NULL ) free( style->style );
        free( style );
    }
}

static const char * svg_style_fmt = "%s{ %s }\n";
static void CC svg_write_style( void *item, void * data )
{
    p_svg_style style = item;
    p_svg_write_ctx ctx = data;
    if ( style != NULL && ctx != NULL )
    {
        char temp[ 240 ];
        rc_t rc = string_printf( temp, sizeof( temp ), NULL, svg_style_fmt,
                    style->name, style->style );
        if ( rc == 0 )
            svg_write_txt( ctx, temp );
    }
}


static const char * svg_style_prefix  = "<style type=\"text/css\"><![CDATA[\n";
static const char * svg_style_postfix = "]]></style>\n\n";

static rc_t svg_write_styles( Vector * styles, p_svg_write_ctx ctx )
{
    rc_t rc = svg_write_txt( ctx, svg_style_prefix );
    if ( rc == 0 )
    {
        VectorForEach ( styles, false, svg_write_style, ctx );
        rc = svg_write_txt( ctx, svg_style_postfix );
    }
    return rc;
}


static rc_t svg_add_style( Vector * styles, p_svg_style style )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( styles != NULL && style != NULL )
    {
        rc = VectorAppend ( styles, NULL, style );
        if ( rc != 0 )
            svg_style_whack( style, NULL );
    }
    return rc;
}

struct svg_elem;

/* the type of the callback-function to be performed on every row */
typedef void ( CC * svg_elem_wr ) ( struct svg_elem * elem, p_svg_write_ctx ctx );

typedef struct svg_elem
{
    svg_elem_wr writer;
    uint32_t * values;
    uint32_t n_values;
    char * txt;
    char * style;
} svg_elem;
typedef svg_elem* p_svg_elem;


static p_svg_elem svg_make_elem( const char * style,
                                 svg_elem_wr writer,
                                 const uint32_t * values,
                                 uint32_t n_values )
{
    p_svg_elem res = calloc( 1, sizeof( struct svg_elem ) );
    if ( res != NULL )
    {
        if ( n_values > 0 )
        {
            size_t size = ( sizeof ( res->values[ 0 ] ) * n_values );
            res->values = malloc( size );
            if ( res->values != NULL )
            {
                memcpy( res->values, values, size );
                res->n_values = n_values;
            }
        }
        if ( style != NULL )
            res->style = string_dup_measure ( style, NULL );
        res->writer = writer;
    }
    return res;
}


static p_svg_elem svg_make_elemv( const char * style,
                                  svg_elem_wr writer,
                                  uint32_t n_values, ... )
{
    p_svg_elem res = calloc( 1, sizeof( struct svg_elem ) );
    va_list v;
    va_start( v, n_values );
    if ( res != NULL )
    {
        size_t size = ( sizeof ( res->values[ 0 ] ) * n_values );
        res->values = malloc( size );
        if ( res->values != NULL )
        {
            int32_t i;
            for ( i = 0; i < n_values; ++i )
                res->values[ i ] = va_arg( v, uint32_t );
            res->n_values = n_values;
        }
        res->style = string_dup_measure ( style, NULL );
        res->writer = writer;
    }
    va_end( v );
    return res;
}


static void CC svg_elem_whack( void *item, void *data )
{
    p_svg_elem elem = item;
    if ( elem )
    {
        if ( elem->values != NULL ) free( elem->values );
        if ( elem->style != NULL ) free( elem->style );
        if ( elem->txt != NULL ) free( elem->txt );
        free( elem );
    }
}


static rc_t svg_add_elem( Vector * elements, p_svg_elem elem )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( elements != NULL && elem != NULL )
    {
        rc = VectorAppend ( elements, NULL, elem );
        if ( rc != 0 )
            svg_elem_whack( elem, NULL );
    }
    return rc;
}


static const char * svg_circle_fmt_c =
    "<circle cx=\"%d\" cy=\"%d\" r=\"%d\" class=\"%s\"/>\n\n";
static const char * svg_circle_fmt =
    "<circle cx=\"%d\" cy=\"%d\" r=\"%d\"/>\n\n";


static void svg_elem_wr_circle( p_svg_elem elem, p_svg_write_ctx ctx )
{
    if ( elem != NULL && ctx != NULL && 
         elem->values != NULL && elem->n_values > 2 )
    {
        char temp[ 120 ];
        rc_t rc;
        if ( elem->style != NULL )
            rc = string_printf( temp, sizeof( temp ), NULL, svg_circle_fmt_c,
                    elem->values[0], elem->values[1], elem->values[2], elem->style );
        else
            rc = string_printf( temp, sizeof( temp ), NULL, svg_circle_fmt,
                    elem->values[0], elem->values[1], elem->values[2] );
        if ( rc == 0 )
            svg_write_txt( ctx, temp );
    }
}


static const char * svg_rect_fmt_c =
    "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" class=\"%s\"/>\n\n";
static const char * svg_rect_fmt =
    "<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\"/>\n\n";

static void svg_elem_wr_rect( p_svg_elem elem, p_svg_write_ctx ctx )
{
    if ( elem != NULL && ctx != NULL &&
         elem->values != NULL && elem->n_values > 3 )
    {
        char temp[ 120 ];
        rc_t rc;
        if ( elem->style != NULL )
            rc = string_printf( temp, sizeof( temp ), NULL, svg_rect_fmt_c,
                    elem->values[0], elem->values[1], elem->values[2],
                    elem->values[3], elem->style );
        else
            rc = string_printf( temp, sizeof( temp ), NULL, svg_rect_fmt,
                    elem->values[0], elem->values[1], elem->values[2],
                    elem->values[3] );
        if ( rc == 0 )
            svg_write_txt( ctx, temp );
    }
}


static const char * svg_ell_fmt_c =
    "<ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\" class=\"%s\"/>\n\n";
static const char * svg_ell_fmt =
    "<ellipse cx=\"%d\" cy=\"%d\" rx=\"%d\" ry=\"%d\"/>\n\n";


static void svg_elem_wr_ellipse( p_svg_elem elem, p_svg_write_ctx ctx )
{
    if ( elem != NULL && ctx != NULL &&
         elem->values != NULL && elem->n_values > 3 )
    {
        char temp[ 120 ];
        rc_t rc;
        if ( elem->style != NULL )
            rc = string_printf( temp, sizeof( temp ), NULL, svg_ell_fmt_c,
                    elem->values[0], elem->values[1], elem->values[2],
                    elem->values[3], elem->style );
            rc = string_printf( temp, sizeof( temp ), NULL, svg_ell_fmt,
                    elem->values[0], elem->values[1], elem->values[2],
                    elem->values[3] );
        if ( rc == 0 )
            svg_write_txt( ctx, temp );
    }
}


static const char * svg_line_fmt_c =
    "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\" class=\"%s\"/>\n\n";
static const char * svg_line_fmt =
    "<line x1=\"%d\" y1=\"%d\" x2=\"%d\" y2=\"%d\"/>\n\n";

static void svg_elem_wr_line( p_svg_elem elem, p_svg_write_ctx ctx )
{
    if ( elem != NULL && ctx != NULL &&
         elem->values != NULL && elem->n_values > 3 )
    {
        char temp[ 120 ];
        rc_t rc;
        if ( elem->style != NULL )
            rc = string_printf( temp, sizeof( temp ), NULL, svg_line_fmt_c,
                    elem->values[0], elem->values[1], elem->values[2],
                    elem->values[3], elem->style );
        else
            rc = string_printf( temp, sizeof( temp ), NULL, svg_line_fmt,
                    elem->values[0], elem->values[1], elem->values[2],
                    elem->values[3] );
        if ( rc == 0 )
            svg_write_txt( ctx, temp );
    }
}


static const char * svg_point_fmt ="%d,%d ";

static char * svg_make_points_str( const uint32_t * values,
                                   const uint32_t n_values )
{
    size_t size = ( n_values * 10 );
    char * res = malloc( size );
    if ( res )
    {
        rc_t rc = 0;
        size_t ofs = 0;
        uint32_t i, n = ( n_values >> 1 );
        for ( i = 0; i < n && rc == 0; ++i )
        {
            size_t written;
            rc = string_printf( &res[ofs], size-ofs, &written, svg_point_fmt,
                                values[ i*2 ], values[ i*2 + 1 ] );
            if ( rc == 0 )
                ofs+=written;
        }
    }
    return res;
}

static const char * svg_pline_fmt_c =
    "<polyline points=\"%s\" class=\"%s\"/>\n\n";
static const char * svg_pline_fmt =
    "<polyline points=\"%s\"/>\n\n";

static void svg_elem_wr_polyline( p_svg_elem elem, p_svg_write_ctx ctx )
{
    if ( elem != NULL && ctx != NULL &&
         elem->values != NULL && elem->n_values > 1 )
    {
        char * point_str = svg_make_points_str( elem->values, elem->n_values );
        if ( point_str != NULL )
        {
            size_t size = string_size( point_str ) + string_size( elem->style ) + 40;
            char * temp = malloc( size );
            if ( temp != NULL )
            {
                rc_t rc;
                if ( elem->style != NULL )
                    rc = string_printf( temp, size, NULL, svg_pline_fmt_c,
                                        point_str, elem->style );
                else
                    rc = string_printf( temp, size, NULL, svg_pline_fmt,
                                        point_str );
                if ( rc == 0 )
                    svg_write_txt( ctx, temp );
                free( temp );
            }
            free( point_str );
        }
    }
}


static const char * svg_text_fmt_c =
    "<text x=\"%d\" y=\"%d\" class=\"%s\">%s</text>\n\n";
static const char * svg_text_fmt =
    "<text x=\"%d\" y=\"%d\">%s</text>\n\n";

static void svg_elem_wr_text( p_svg_elem elem, p_svg_write_ctx ctx )
{
    if ( elem != NULL && ctx != NULL &&
         elem->values != NULL && elem->n_values > 1 )
    {
        size_t size = string_size( elem->txt ) + string_size( elem->style ) + 50;
        char * temp = malloc( size );
        if ( temp != NULL )
        {
            rc_t rc;
            if ( elem->style != NULL )
                rc = string_printf( temp, size, NULL, svg_text_fmt_c,
                        elem->values[0], elem->values[1], elem->style, elem->txt );
            else
                rc = string_printf( temp, size, NULL, svg_text_fmt,
                        elem->values[0], elem->values[1], elem->txt );

            if ( rc == 0 )
                svg_write_txt( ctx, temp );
            free( temp );
        }
    }
}


static const char * svg_path_fmt_c =
    "<path d=\"%s\" class=\"%s\"/>\n\n";
static const char * svg_path_fmt =
    "<path d=\"%s\"/>\n\n";

static void svg_elem_wr_path( p_svg_elem elem, p_svg_write_ctx ctx )
{
    if ( elem != NULL && ctx != NULL &&  elem->txt != NULL )
    {
        size_t size = string_size( elem->txt ) + string_size( elem->style ) + 50;
        char * temp = malloc( size );
        if ( temp != NULL )
        {
            rc_t rc;
            if ( elem->style != NULL )
                rc = string_printf( temp, size, NULL, svg_path_fmt_c,
                        elem->txt, elem->style );
            else
                rc = string_printf( temp, size, NULL, svg_path_fmt,
                        elem->txt );
            if ( rc == 0 )
                svg_write_txt( ctx, temp );
            free( temp );
        }
    }
}


rc_t svg_init( p_svg grafic )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( grafic != NULL )
    {
        VectorInit ( &grafic->elements, 0, 20 );
        VectorInit ( &grafic->styles, 0, 5 );
        rc = 0;
    }
    return rc;
}


void svg_destroy( p_svg grafic )
{
    if ( grafic != NULL )
    {
        VectorWhack ( &grafic->styles, svg_style_whack, NULL );
        VectorWhack ( &grafic->elements, svg_elem_whack, NULL );
    }
}


static rc_t svg_write_lines( p_svg_write_ctx ctx, const char ** lines )
{
    rc_t rc = 0;
    const char **s = lines;
    while ( *s != NULL && rc == 0 )
    {
        rc = svg_write_txt( ctx, *s );
        s++;
    }
    return rc;
}


static const char * svg_prefix[] =
{ 
    "<?xml version=\"1.0\" standalone=\"no\"?>\n\n",
    "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n",
    "\"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n\n",
    "<svg width=\"100%\" height=\"100%\" version=\"1.1\"\n",
    "xmlns=\"http://www.w3.org/2000/svg\">\n\n",
    NULL
};


static void CC svg_write_elem( void *item, void * data )
{
    p_svg_elem elem = item;
    p_svg_write_ctx ctx = data;
    if ( elem != NULL && ctx != NULL )
    {
        if ( elem->writer != NULL )
            elem->writer( elem, ctx );
    }
}

rc_t svg_write( p_svg grafic, const char * filename )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( grafic != NULL && filename != NULL )
    {
        KDirectory * dir;
        rc = KDirectoryNativeDir ( &dir );
        if ( rc == 0 )
        {
            svg_write_ctx ctx;
            ctx.pos = 0;
            rc = KDirectoryCreateFile ( dir, &ctx.f, true, 0664, kcmInit, "%s", filename );
            if ( rc == 0 )
            {
                rc = svg_write_lines( &ctx, svg_prefix );
                if ( rc == 0 )
                {
                    rc = svg_write_styles( &grafic->styles, &ctx );
                    if ( rc == 0 )
                    {
                        VectorForEach ( &grafic->elements, false, svg_write_elem, &ctx );
                        rc = svg_write_txt( &ctx, "</svg>\n" );
                    }
                }
                KFileRelease( ctx.f );
            }
            KDirectoryRelease( dir );
        }
    }
    return rc;
}


rc_t svg_set_style( p_svg grafic, const char * name, const char * style )
{
    return svg_add_style( &grafic->styles,
                          svg_make_style( name, style ) );
}


rc_t svg_circle( p_svg grafic, uint32_t x, uint32_t y, uint32_t r,
                 const char * style )
{
    return svg_add_elem( &grafic->elements,
                     svg_make_elemv( style, svg_elem_wr_circle,
                                     3, x, y, r ) );
}


rc_t svg_rect( p_svg grafic, uint32_t x, uint32_t y, uint32_t dx, uint32_t dy,
               const char * style )
{
    return svg_add_elem( &grafic->elements,
                    svg_make_elemv( style, svg_elem_wr_rect,
                                    4, x, y, dx, dy ) );
}


rc_t svg_ellipse( p_svg grafic, uint32_t x, uint32_t y, uint32_t rx, uint32_t ry,
               const char * style )
{
    return svg_add_elem( &grafic->elements,
                    svg_make_elemv( style, svg_elem_wr_ellipse,
                                    4, x, y, rx, ry ) );
}


rc_t svg_line( p_svg grafic, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
               const char * style )
{
    return svg_add_elem( &grafic->elements,
                    svg_make_elemv( style, svg_elem_wr_line,
                                    4, x1, y1, x2, y2 ) );
}


rc_t svg_polyline( p_svg grafic, const char * style,
                   const uint32_t * values, const uint32_t n_values )
{
    return svg_add_elem( &grafic->elements,
                    svg_make_elem( style, svg_elem_wr_polyline,
                                   values, n_values ) );
}


rc_t svg_text( p_svg grafic, uint32_t x, uint32_t y, const char * txt,
               const char * style )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    p_svg_elem elem = svg_make_elemv( style, svg_elem_wr_text, 2, x, y );
    if ( elem != NULL )
    {
        elem->txt = string_dup_measure ( txt, NULL );
        rc = svg_add_elem( &grafic->elements, elem );
    }
    return rc;
}


rc_t svg_set_path( p_svg grafic, p_svg_path path, const char * style )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    p_svg_elem elem = svg_make_elem( style, svg_elem_wr_path, NULL, 0 );
    if ( elem != NULL )
    {
        elem->txt = string_dup_measure ( path->buf.ptr, NULL );
        rc = svg_add_elem( &grafic->elements, elem );
    }
    return rc;

}
