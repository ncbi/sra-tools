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

#ifndef _h_svg_
#define _h_svg_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <klib/vector.h>
#include <helper.h>

typedef struct svg_path
{
    char_buffer buf;
} svg_path;
typedef svg_path* p_svg_path;

rc_t svg_path_init( p_svg_path path );

void svg_path_destroy( p_svg_path path );

rc_t svg_path_close( p_svg_path path );
rc_t svg_path_move_to( p_svg_path path, bool absolute, int32_t x, int32_t y );
rc_t svg_path_line_to( p_svg_path path, bool absolute, int32_t x, int32_t y );
rc_t svg_path_hline_to( p_svg_path path, bool absolute, int32_t x );
rc_t svg_path_vline_to( p_svg_path path, bool absolute, int32_t y );
rc_t svg_path_smooth_to( p_svg_path path, bool absolute, int32_t x, int32_t y );

/* cubic bezier curve (x1,y1 ... control point / x,y ... endpoint )*/
rc_t svg_path_bez3_to( p_svg_path path, bool absolute,
                       int32_t x1, int32_t y1, int32_t x, int32_t y );

/* quadratic bezier curve (x1,y1 ... control point / x,y ... endpoint )*/
rc_t svg_path_bez2_to( p_svg_path path, bool absolute,
                       int32_t x1, int32_t y1, int32_t x, int32_t y );

/* quadratic bezier curve (x1,y1 x2,y2 ... control point / x,y ... endpoint )*/
rc_t svg_path_curve_to( p_svg_path path, bool absolute,
        int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x, int32_t y );

/* arc ( rx, ry ....... elliptical radius in x/y-direction
         rotx   ....... rotation around x-axis
         large-flag ... choose the large section of the arc
         sweep-flag ... draw in positive-angle-direction
         x,y .......... endpoint ) */
rc_t svg_path_arc( p_svg_path path, bool absolute,
                   int32_t rx, int32_t ry, int32_t rot_x, 
                   bool large_flag, bool sweep_flag,
                   int32_t x, int32_t y );


typedef struct svg
{
    Vector styles;
    Vector elements;
} svg;
typedef svg* p_svg;

rc_t svg_init( p_svg grafic );

void svg_destroy( p_svg grafic );

rc_t svg_set_style( p_svg grafic, const char * name, const char * style );

rc_t svg_circle( p_svg grafic, uint32_t x, uint32_t y, uint32_t r,
                 const char * style );

rc_t svg_rect( p_svg grafic, uint32_t x, uint32_t y, uint32_t dx, uint32_t dy,
               const char * style );

rc_t svg_ellipse( p_svg grafic, uint32_t x, uint32_t y, uint32_t rx, uint32_t ry,
               const char * style );

rc_t svg_line( p_svg grafic, uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2,
               const char * style );

rc_t svg_polyline( p_svg grafic, const char * style,
                   const uint32_t * values, const uint32_t n_values );

rc_t svg_text( p_svg grafic, uint32_t x, uint32_t y, const char * txt,
               const char * style );

rc_t svg_set_path( p_svg grafic, p_svg_path path, const char * style );

rc_t svg_write( p_svg grafic, const char * filename );

#ifdef __cplusplus
}
#endif

#endif
