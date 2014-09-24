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

#include <klib/out.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/namelist.h>
#include <sysalloc.h>
#include <stdlib.h>
#include <string.h>

#include "key_value.h"


rc_t report_init( p_report * self, uint32_t prealloc )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        *self = calloc( 1, sizeof **self );
        if ( *self != NULL )
            VectorInit ( &( (*self)->data ), 0, prealloc );
        else
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    else
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcSelf, rcNull );
    return rc;
}


static void CC report_data_whack( void *item, void *data )
{
    const VNamelist * values = item;
    if ( values != NULL )
        VNamelistRelease ( values );
}


rc_t report_clear( p_report self )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        VectorWhack ( &(self->data), report_data_whack, NULL );
        VNamelistRelease ( self->columns ); /* ignores NULL */
        if ( self->max_width != NULL )
        {
            free( self->max_width );
            self->max_width = NULL;
        }
    }
    else
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcSelf, rcNull );
    return rc;
}


rc_t report_destroy( p_report self )
{
    rc_t rc;
    if ( self != NULL )
    {
        rc = report_clear( self );
        free( self );
    }
    else
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcSelf, rcNull );
    return rc;
}


rc_t report_set_columns( p_report self, size_t count, ... )
{
    rc_t rc;
    if ( self != NULL )
    {
        VNamelistRelease ( self->columns ); /* ignores NULL */
        rc = VNamelistMake ( &self->columns, count );
        if ( rc == 0 )
        {
            self->col_count = count;
            if ( self->max_width != NULL )
                free( self->max_width );
            self->max_width = malloc( count * sizeof( *self->max_width ) );
            if ( self->max_width == NULL )
                rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );

            if ( rc == 0 )
            {
                size_t idx;
                va_list args;

                va_start ( args, count );
                for ( idx = 0; idx < count && rc == 0; ++idx )
                {
                    const char * s = va_arg( args, const char * );
                    if ( s != NULL )
                    {
                        rc = VNamelistAppend ( self->columns, s );
                        self->max_width[ idx ] = string_size( s );
                    }
                    else
                    {
                        rc = VNamelistAppend ( self->columns, "." );
                        self->max_width[ idx ] = 1;
                    }
                }
                va_end ( args );
            }
        }
    }
    else
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcSelf, rcNull );
    return rc;
}


rc_t report_new_data( p_report self )
{
    rc_t rc;
    if ( self != NULL )
    {
        if ( self->columns != NULL )
        {
            uint32_t count;
            rc = VNameListCount ( self->columns, &count );
            if ( rc == 0 && count > 0 )
            {
                VNamelist * new_row;
                rc = VNamelistMake ( &new_row, count );
                if ( rc == 0 )
                    rc = VectorAppend ( &self->data, NULL, new_row );
            }
        }
        else
            rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    }
    else
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcSelf, rcNull );
    return rc;
}


rc_t report_add_data( p_report self, const size_t estimated_len, const char * fmt, ... )
{
    rc_t rc = 0;
    if ( self != NULL )
    {
        uint32_t n_data = VectorLength( &self->data );
        if ( n_data == 0 )
        {
            rc = report_new_data( self );
            n_data++;
        }
        if ( rc == 0 )
        {
            VNamelist * last_row = VectorGet ( &self->data, n_data - 1 );
            if ( last_row != NULL )
            {
                uint32_t n_col;

                VNameListCount ( last_row, &n_col );
                if ( n_col < self->col_count )
                {
                    va_list args;
                    size_t len = estimated_len;
                    bool done = false;
                    char * s;

                    va_start ( args, fmt );
                    while( !done )
                    {
                        s = malloc( len );
                        done = ( s == NULL );
                        if ( !done )
                        {
                            size_t written;
                            rc = string_vprintf ( s, estimated_len, &written, fmt, args );
                            done = ( rc == 0 );
                            if ( done )
                            {
                                rc = VNamelistAppend ( last_row, s );
                                if ( self->max_width[ n_col ] < written )
                                    self->max_width[ n_col ] = written;
                            }
                            else
                            {
                                free( s );
                                len += len;
                            }
                        }
                        else
                            rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                    }
                    va_end ( args );
                }
            }
            else
                rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        }
    }
    else
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcSelf, rcNull );
    return rc;
}


static rc_t report_print_column_names( p_report self, p_char_buffer dst,
                                   const uint32_t n_col, const char delim )
{
    rc_t rc = 0;
    uint32_t col_idx;
    /* print the column-names */
    for ( col_idx = 0; col_idx < n_col && rc == 0; ++col_idx )
    {
        const char * s;
        rc = VNameListGet ( self->columns, col_idx, &s );
        if ( rc == 0 )
        {
            size_t len = string_size( s );
            if ( col_idx < n_col - 1 )
                rc = char_buffer_printf( dst, len + 5, "%s%c", s, delim );
            else
                rc = char_buffer_printf( dst, len + 5, "%s\n", s );
        }
    }
    return rc;
}

static rc_t report_print_tabed_column_names( p_report self, p_char_buffer dst,
                                   const uint32_t n_col, const char delim )
{
    rc_t rc = 0;
    uint32_t col_idx;
    /* print the column-names */
    for ( col_idx = 0; col_idx < n_col && rc == 0; ++col_idx )
    {
        const char * s;
        rc = VNameListGet ( self->columns, col_idx, &s );
        if ( rc == 0 )
        {
            uint32_t w = self->max_width[ col_idx ];
            if ( col_idx < n_col - 1 )
                rc = char_buffer_printf( dst, w + 5, "%*s%c", w, s, delim );
            else
                rc = char_buffer_printf( dst, w + 5, "%*s\n", w, s );
        }
    }
    return rc;
}


static rc_t report_print_data_rows( p_report self, p_char_buffer dst,
                                const uint32_t n_data, const char delim )
{
    rc_t rc = 0;
    uint32_t d_idx;
    for ( d_idx = 0; d_idx < n_data && rc == 0; ++d_idx )
    {
        VNamelist * row = VectorGet ( &self->data, d_idx );
        if ( row != NULL )
        {
            uint32_t nc_data;
            rc = VNameListCount ( row, &nc_data );
            if ( rc == 0 )
            {
                uint32_t col_idx;
                for ( col_idx = 0; col_idx < nc_data && rc == 0; ++col_idx )
                {
                    const char * s;
                    rc = VNameListGet ( row, col_idx, &s );
                    if ( rc == 0 )
                    {
                        size_t len = string_size( s );
                        if ( col_idx < nc_data - 1 )
                            rc = char_buffer_printf( dst, len + 5, "%s%c", s, delim );
                        else
                            rc = char_buffer_printf( dst, len + 5, "%s\n", s );
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t report_print_tabed_data_rows( p_report self, p_char_buffer dst,
                                const uint32_t n_data, const char delim )
{
    rc_t rc = 0;
    uint32_t d_idx;
    for ( d_idx = 0; d_idx < n_data && rc == 0; ++d_idx )
    {
        VNamelist * row = VectorGet ( &self->data, d_idx );
        if ( row != NULL )
        {
            uint32_t nc_data;
            rc = VNameListCount ( row, &nc_data );
            if ( rc == 0 )
            {
                uint32_t col_idx;
                for ( col_idx = 0; col_idx < nc_data && rc == 0; ++col_idx )
                {
                    const char * s;
                    rc = VNameListGet ( row, col_idx, &s );
                    if ( rc == 0 )
                    {
                        uint32_t w = self->max_width[ col_idx ];
                        if ( col_idx < nc_data - 1 )
                            rc = char_buffer_printf( dst, w + 5, "%*s%c", w, s, delim );
                        else
                            rc = char_buffer_printf( dst, w + 5, "%*s\n", w, s );
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t report_print_column_names_row0( p_report self, p_char_buffer dst,
                                            const uint32_t n_col )
{
    rc_t rc = 0;
    uint32_t i, w = 0;
    VNamelist * row;

    for ( i = 0; i < n_col && rc == 0; ++i )
    {
        const char * s;
        rc = VNameListGet ( self->columns, i, &s );
        if ( rc == 0 )
        {
            size_t len = string_size( s );
        if ( len > w ) w = len;
        }
    }
    row = VectorGet ( &self->data, 0 );
    if ( row != NULL )
    {
        for ( i = 0; i < n_col && rc == 0; ++i )
        {
            const char * s_col;
            rc = VNameListGet ( self->columns, i, &s_col );
            if ( rc == 0 )
            {
                const char * s_value;
                rc = VNameListGet ( row, i, &s_value );
                if ( rc == 0 )
                {
                    size_t len = string_size( s_value ) + w + 5;
                    rc = char_buffer_printf( dst, len, "%*s : %s\n", w, s_col, s_value );
                }
            }
        }
    }
    return rc;
}

static rc_t report_print_txt( p_report self, p_char_buffer dst,
                            const uint32_t n_col, const uint32_t n_data )
{
    rc_t rc;
    uint32_t rows = VectorLength( &self->data );
    switch( rows )
    {
        case 0  : rc = report_print_tabed_column_names( self, dst, n_col, ' ' );
                  break;
        case 1  : rc = report_print_column_names_row0( self, dst, n_col );
                  break;
        default : rc = report_print_tabed_column_names( self, dst, n_col, ' ' );
                  if ( rc == 0 )
                    rc = report_print_tabed_data_rows( self, dst, n_data, ' ' );
    }
    return rc;
}


static rc_t report_print_csv( p_report self, p_char_buffer dst,
                            const uint32_t n_col, const uint32_t n_data )
{
    rc_t rc = report_print_column_names( self, dst, n_col, ',' );
    if ( rc == 0 )
        rc = report_print_data_rows( self, dst, n_data, ',' );
    return rc;
}



static rc_t report_print_xml( p_report self, p_char_buffer dst,
                            const uint32_t n_col, const uint32_t n_data )
{
    rc_t rc = 0;
    uint32_t d_idx;
    for ( d_idx = 0; d_idx < n_data && rc == 0; ++d_idx )
    {
        VNamelist * row = VectorGet ( &self->data, d_idx );
        if ( row != NULL )
        {
            /* open the row-xml node */
            rc = char_buffer_printf( dst, 20, "<row_%u>\n", d_idx );
            if ( rc == 0 )
            {
                uint32_t nc_data;
                rc = VNameListCount ( row, &nc_data );
                if ( rc == 0 )
                {
                    uint32_t col_idx;
                    for ( col_idx = 0; col_idx < nc_data && rc == 0; ++col_idx )
                    {
                        const char * s_col;
                        rc = VNameListGet ( self->columns, col_idx, &s_col );
                        if ( rc == 0 )
                        {
                            const char * s;
                            size_t col_len = string_size( s_col );
                            rc = VNameListGet ( row, col_idx, &s );
                            if ( rc == 0 )
                            {
                                size_t len = string_size( s );
                                /* print the value */
                                rc = char_buffer_printf( dst, col_len * 2 + len + 20, 
                                    " <%s>%s</%s>\n", s_col, s, s_col );
                            }
                        }
                    }
                }
                /* close the row-xml node */
                rc = char_buffer_printf( dst, 20, "</row_%u>\n\n", d_idx );
            }
        }
    }
    return rc;
}


static rc_t report_print_jso( p_report self, p_char_buffer dst,
                            const uint32_t n_col, const uint32_t n_data )
{
    rc_t rc = 0;
    uint32_t d_idx;
    for ( d_idx = 0; d_idx < n_data && rc == 0; ++d_idx )
    {
        VNamelist * row = VectorGet ( &self->data, d_idx );
        if ( row != NULL )
        {
            /* open the json node */
            rc = char_buffer_printf( dst, 20, "{\n", d_idx );
            if ( rc == 0 )
            {
                uint32_t nc_data;
                rc = VNameListCount ( row, &nc_data );
                if ( rc == 0 )
                {
                    uint32_t col_idx;
                    for ( col_idx = 0; col_idx < nc_data && rc == 0; ++col_idx )
                    {
                        const char * s_col;
                        rc = VNameListGet ( self->columns, col_idx, &s_col );
                        if ( rc == 0 )
                        {
                            const char * s;
                            size_t col_len = string_size( s_col );
                            rc = VNameListGet ( row, col_idx, &s );
                            if ( rc == 0 )
                            {
                                size_t len = string_size( s );
                                /* print the value */
                                if ( col_idx < nc_data -1 )
                                    rc = char_buffer_printf( dst, col_len + len + 10, 
                                        " \"%s\":%s,\n", s_col, s );
                                else
                                    rc = char_buffer_printf( dst, col_len + len + 10, 
                                        " \"%s\":%s\n", s_col, s );
                            }
                        }
                    }
                }
                /* close the json-node */
                if ( d_idx < n_data - 1 )
                    rc = char_buffer_printf( dst, 20, "},\n\n", d_idx );
                else
                    rc = char_buffer_printf( dst, 20, "}\n", d_idx );
            }
        }
    }
    return rc;
}


rc_t report_print( p_report self, p_char_buffer dst, uint32_t mode )
{
    rc_t rc = RC( rcExe, rcNoTarg, rcConstructing, rcSelf, rcNull );
    if ( self != NULL )
    {
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcNull );
        if ( dst != NULL )
        {
            uint32_t n_col;
            rc = VNameListCount ( self->columns, &n_col );
            if ( rc == 0 )
            {
                rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                if ( n_col > 0 )
                {
                    uint32_t n_data = VectorLength( &self->data );
                    if ( n_data >= 0 )
                    {
                        switch( mode )
                        {
                        case RT_TXT : rc = report_print_txt( self, dst, n_col, n_data ); break;
                        case RT_CSV : rc = report_print_csv( self, dst, n_col, n_data ); break;
                        case RT_XML : rc = report_print_xml( self, dst, n_col, n_data ); break;
                        case RT_JSO : rc = report_print_jso( self, dst, n_col, n_data ); break;
                        default : rc = RC( rcExe, rcNoTarg, rcConstructing, rcParam, rcInvalid );
                        }
                    }
                }
            }
        }
    }
    return rc;
}