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

#ifndef _h_chart_
#define _h_chart_

#ifdef __cplusplus
extern "C" {
#endif

#include "svg.h"

typedef struct chart
{
    svg grafic;
    uint32_t xofs, yofs;
    uint32_t dx, dy;
    uint32_t entries;
} chart;
typedef chart* p_chart;

rc_t chart_init( p_chart chart,
                 uint32_t xofs, uint32_t yofs, uint32_t dx, uint32_t dy );

void chart_destroy( p_chart chart );

rc_t chart_write( p_chart chart, const char * filename );

rc_t chart_captions( p_chart chart,
                     const char * caption, 
                     const char * caption_x,
                     const char * caption_y );

long chart_round( double x );
uint32_t chart_calc_y( const uint32_t value, double factor );

rc_t chart_vruler( p_chart chart,
                   uint32_t from, uint32_t to,
                   double factor,
                   uint32_t spread,
                   const char * ext );

rc_t chart_vruler1( p_chart chart,
                   uint32_t max_y, uint32_t step_y, uint32_t max_value,
                   const char * ext );

rc_t chart_hruler( p_chart chart,
                   uint32_t *values, uint32_t n_values,
                   uint32_t xofs, uint32_t spread );

char * create_line_style( p_chart chart, const char * color,
                          const uint32_t width );

char * create_fill_style( p_chart chart, const char * line_color, 
                          const char * fill_color, const uint32_t width );

rc_t chart_line( p_chart chart,
                 const uint32_t * values,
                 uint32_t n_values,
                 double factor,
                 uint32_t spread,
                 const char * color,
                 const char * name  );

rc_t chart_bar2d( p_chart chart,
                  const uint32_t x,
                  const uint32_t h,
                  const uint32_t dx,
                  double factor,
                  const char * line_color,
                  const char * fill_color,
                  const char * name );

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
                      const char * median_style );

#endif
