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

#include <os-native.h>
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "chart.h"


char * create_line_style( p_chart chart, const char * color, const uint32_t width )
{
    size_t name_size = string_size( color ) + 10;
    char * name = malloc( name_size );
    if ( name != NULL )
    {
        rc_t rc = string_printf( name, name_size, NULL, ".line_%s", color );
        if ( rc == 0 )
        {
            size_t value_size = name_size + 48;
            char * value = malloc( value_size );
            if ( value != NULL )
            {
                rc = string_printf( value, value_size, NULL,
                                    "fill:none; stroke: %s; stroke-width: %d;",
                                    color, width );
                if ( rc == 0 )
                    rc = svg_set_style( &chart->grafic, name, value );
                free( value );
            }
            else
                rc = -1;
        }
        if ( rc != 0 )
        {
            free( name );
            name = NULL;
        }
    }
    return name;
}


char * create_fill_style( p_chart chart, const char * line_color, 
                          const char * fill_color, const uint32_t width )
{
    size_t name_size = string_size( fill_color ) + 10;
    char * name = malloc( name_size );
    if ( name != NULL )
    {
        rc_t rc = string_printf( name, name_size, NULL, ".fill_%s", fill_color );
        if ( rc == 0 )
        {
            size_t value_size = name_size + string_size( line_color ) + 48;
            char * value = malloc( value_size );
            if ( value != NULL )
            {
                rc = string_printf( value, value_size, NULL,
                                    "fill: %s; stroke: %s; stroke-width: %d;",
                                    fill_color, line_color, width );
                if ( rc == 0 )
                    rc = svg_set_style( &chart->grafic, name, value );
                free( value );
            }
            else
                rc = -1;
        }
        if ( rc != 0 )
        {
            free( name );
            name = NULL;
        }
    }
    return name;
}


static void chart_styles( p_chart chart )
{
    /* populate the styles */
    svg_set_style( &chart->grafic,
                   ".thick", "stroke: black; stroke-width: 3;" );
    svg_set_style( &chart->grafic,
                   ".medium", "stroke: black; stroke-width: 2;" );
    svg_set_style( &chart->grafic,
                   ".thin", "stroke: black; stroke-width: 1;" );

    svg_set_style( &chart->grafic, ".big", "font-size: 24; text-anchor: middle;" );
    svg_set_style( &chart->grafic, ".small", "font-size: 12;" );
    svg_set_style( &chart->grafic, ".small_m", "font-size: 12; text-anchor: middle;" );
    svg_set_style( &chart->grafic, ".small_e", "font-size: 12; text-anchor: end;" );
}


static void chart_axis( p_chart chart )
{
    svg_line( &chart->grafic,
              chart->xofs, chart->yofs,
              chart->xofs, chart->yofs + chart->dy,
              "thick" );
    svg_line( &chart->grafic,
              chart->xofs, chart->yofs + chart->dy,
              chart->xofs + chart->dx, chart->yofs + chart->dy,
              "thick" );
}


rc_t chart_init( p_chart chart,
                 uint32_t xofs, uint32_t yofs, uint32_t dx, uint32_t dy )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL )
    {
        rc = svg_init( &chart->grafic );
        if ( rc == 0 )
        {
            chart->xofs = xofs;
            chart->yofs = yofs;
            chart->dx = dx;
            chart->dy = dy;
            chart->entries = 0;

            chart_styles( chart );
            chart_axis( chart );
        }
    }
    return rc;
}


void chart_destroy( p_chart chart )
{
    if ( chart != NULL )
        svg_destroy( &chart->grafic );
}


rc_t chart_write( p_chart chart, const char * filename )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL && filename != NULL )
    {
        rc = svg_write( &chart->grafic, filename );
    }
    return rc;
}


rc_t chart_captions( p_chart chart,
                     const char * caption, 
                     const char * caption_x,
                     const char * caption_y )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL )
    {
        /* the main caption centered, 24pt below the upper line (center-aligned)*/
        svg_text( &chart->grafic,
                  chart->xofs + ( chart->dx / 2 ), chart->yofs + 24,
                  caption, "big" );
        /* caption of the y-axis, 12pt below the upper line (end-aligned)*/
        svg_text( &chart->grafic,
                  chart->xofs -5, chart->yofs + 12,
                  caption_y, "small_e" );
        /* caption of the x-axis, 6pt above the x-axis (end-aligned) */
        svg_text( &chart->grafic,
                  chart->xofs + chart->dx, chart->yofs + chart->dy - 6,
                  caption_x, "small_e" );
    }
    return rc;
}


long chart_round( double x )
{
    if ( x >= 0 )
      return (long) ( x+0.5 );
    return (long) ( x-0.5 );
}


uint32_t chart_calc_y( const uint32_t value, double factor )
{
    return ( chart_round( factor * value ) );
}


/* the scale of the y-axis */
rc_t chart_vruler( p_chart chart,
                   uint32_t from, uint32_t to,
                   double factor,
                   uint32_t spread,
                   const char * ext )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL )
    {
        svg_path path;
        rc = svg_path_init( &path );
        if ( rc == 0 )
        {
            uint32_t idx;
            uint32_t x = chart->xofs;
            uint32_t y = chart->yofs + chart->dy;

            for ( idx = from; idx <= to && rc == 0; idx += spread )
            {
                char temp[ 32 ];
                uint32_t y1 = y - chart_calc_y( idx, factor );

                svg_path_move_to( &path, true, x, y1 );
                svg_path_hline_to( &path, false, -5 );

                if ( ext != NULL )
                    rc = string_printf( temp, sizeof( temp ), NULL, "%u%s", idx, ext );
                else
                    rc = string_printf( temp, sizeof( temp ), NULL, "%u", idx );
                if ( rc == 0 )
                    svg_text( &chart->grafic, x - 10, y1, temp, "small_e" );
            }
            svg_set_path( &chart->grafic, &path, "thin" );
            svg_path_destroy( &path );
        }
    }
    return rc;
}


/* the scale of the y-axis */
rc_t chart_vruler1( p_chart chart,
                   uint32_t max_y, uint32_t step_y, uint32_t max_value,
                   const char * ext )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL )
    {
        svg_path path;
        rc = svg_path_init( &path );
        if ( rc == 0 )
        {
            uint32_t idx;
            uint32_t x = chart->xofs;
            uint32_t y = chart->yofs + chart->dy;

            for ( idx = 0; idx <= max_y && rc == 0; idx += step_y )
            {
                char temp[ 32 ];
                uint64_t value = max_value;
                value *= idx;
                value /= max_y;

                svg_path_move_to( &path, true, x, y - idx );
                svg_path_hline_to( &path, false, -5 );

                if ( ext != NULL )
                    rc = string_printf( temp, sizeof( temp ), NULL, "%u%s", value, ext );
                else
                    rc = string_printf( temp, sizeof( temp ), NULL, "%u", value );
                if ( rc == 0 )
                    svg_text( &chart->grafic, x - 10, y - idx, temp, "small_e" );
            }
            svg_set_path( &chart->grafic, &path, "thin" );
            svg_path_destroy( &path );
        }
    }
    return rc;
}


/* the scale of the x-axis */
rc_t chart_hruler( p_chart chart,
                   uint32_t *values, uint32_t n_values,
                   uint32_t xofs, uint32_t spread )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL && values != NULL )
    {
        svg_path path;
        rc = svg_path_init( &path );
        if ( rc == 0 )
        {
            uint32_t idx;
            uint32_t y = chart->yofs + chart->dy;
            uint32_t x = chart->xofs + xofs;
            rc = 0;
            for ( idx = 0; idx < n_values && rc == 0; ++idx )
            {
                char temp[ 16 ];

                svg_path_move_to( &path, true, x, y );
                svg_path_vline_to( &path, false, 5 );

                rc = string_printf( temp, sizeof( temp ), NULL, "%u", values[ idx ] );
                if ( rc == 0 )
                {
                    if ( ( idx & 1 ) == 0 )
                        svg_text( &chart->grafic, x, y + 20, temp, "small_m" );
                    else
                        svg_text( &chart->grafic, x, y + 30, temp, "small_m" );
                }

                x += spread;
            }
            svg_set_path( &chart->grafic, &path, "thin" );
            svg_path_destroy( &path );
        }
    }
    return rc;
}


rc_t chart_line( p_chart chart,
                 const uint32_t * values,
                 uint32_t n_values,
                 double factor,
                 uint32_t spread,
                 const char * color,
                 const char * name  )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL && values != NULL && color != NULL && name != NULL )
    {
        /* create a style for every line with the requested color */
        char * style_name = create_line_style( chart, color, 1 );
        if ( style_name != NULL )
        {
            /* translate the given values (factor) into an array of 
               xy-coordinates, render that as polyline */
            uint32_t y = chart->yofs + chart->dy;
            uint32_t * points = malloc( sizeof( values[ 0 ] ) * n_values * 2 );
            if ( points != NULL )
            {
                uint32_t idx, x = chart->xofs;
                for ( idx = 0; idx < n_values; ++idx )
                {
                    points[ idx * 2 ] = x;
                    points[ idx * 2 + 1 ] = y - chart_calc_y( values[ idx ], factor ); 
                    x += spread;
                }
                rc = svg_polyline( &chart->grafic, &style_name[ 1 ],
                                   points, n_values * 2 );
                free( points );
            }

            if ( rc == 0 )
            {
                /* print a short line with the right color, follow by the name */
                uint32_t x = chart->xofs + ( chart->entries * 100 );
                y += 50;
                svg_line( &chart->grafic, x, y, x + 20, y, &style_name[ 1 ] );
                svg_text( &chart->grafic, x +25, y, name, "small" );
                chart->entries++;
            }
            free( style_name );
        }
    }
    return rc;
}


rc_t chart_bar2d( p_chart chart,
                  const uint32_t x,
                  const uint32_t h,
                  const uint32_t dx,
                  double factor,
                  const char * line_color,
                  const char * fill_color,
                  const char * name )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL && line_color != NULL && fill_color!= NULL && name != NULL )
    {
        /* create a style for every line with the requested color */
        char * style_name = create_fill_style( chart, line_color, fill_color, 1 );
        if ( style_name != NULL )
        {
            uint32_t y  = chart->yofs + chart->dy;
            uint32_t dy = chart_calc_y( h, factor );
            uint32_t x1 = x + chart->xofs;
            rc = svg_rect( &chart->grafic, x1, y - dy, dx, dy,
                           &(style_name[ 1 ]) );
            free( style_name );
        }
    }
    return rc;
}


rc_t chart_box_whisker( p_chart chart,
                      const uint32_t x,
                      const uint32_t dx,
                      const uint32_t p_low,
                      const uint32_t p_high,
                      const uint32_t q_low,
                      const uint32_t q_high,
                      const uint32_t median,
                      double factor,
                      const char * box_style,
                      const char * median_style )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
    if ( chart != NULL && box_style != NULL && median_style != NULL )
    {
        uint32_t y = chart->yofs + chart->dy;
        uint32_t y1 = y - chart_calc_y( p_low, factor );
        uint32_t y2 = y - chart_calc_y( p_high, factor );
        uint32_t x1 = x + chart->xofs + dx / 2;

        /* draw the "whisker"*/
        rc = svg_line( &chart->grafic, x1, y1, x1, y2, "thin" );
        if ( rc == 0 )
            rc = svg_line( &chart->grafic, x1 - ( dx / 4 ), y1, 
                           x1 + ( dx / 4 ), y1, "thin" );
        if ( rc == 0 )
            rc = svg_line( &chart->grafic, x1 - ( dx / 4 ), y2, 
                           x1 + ( dx / 4 ), y2, "thin" );
        
        /* draw the "box" over the "whisker" */
        if ( rc == 0 )
        {
            x1 = x + chart->xofs;
            y1 = y - chart_calc_y( q_low, factor );
            y2 = y - chart_calc_y( q_high, factor );
            rc = svg_rect( &chart->grafic, x1, y2, dx, y1-y2, box_style );
        }

        /* draw the "median" over the "box" */
        if ( rc == 0 )
        {
            y1 = y - chart_calc_y( median, factor );
            rc = svg_line( &chart->grafic, x1, y1, x1 + dx, y1, median_style );
        }
    }
    return rc;
}
