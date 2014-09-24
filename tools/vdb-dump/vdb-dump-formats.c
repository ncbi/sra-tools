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

#include "vdb-dump-formats.h"
#include <sysalloc.h>

#include <klib/rc.h>
#include <klib/log.h>
#define DISP_RC(rc,err) if( rc != 0 ) LOGERR( klogInt, rc, err );

/*************************************************************************************
    default ( with line-length-limitation and pretty print )
*************************************************************************************/
static void CC vdfo_print_col_default( void *item, void *data )
{
    p_col_def my_col_def = (p_col_def)item;
    p_row_context r_ctx = (p_row_context)data;
    rc_t rc;

    if ( my_col_def->valid == false ) return;
    if ( my_col_def->excluded == true ) return;

    rc = vds_clear( &(r_ctx->s_col) );
    DISP_RC( rc, "dump_str_clear() failed" )
    if ( rc != 0 ) return;

    if ( r_ctx->ctx->print_column_names )
    {
        rc = vds_append_fmt( &(r_ctx->s_col),
                             r_ctx->col_defs->max_colname_chars,
                             "%*s: ",
                             r_ctx->col_defs->max_colname_chars,
                             my_col_def->name );
        DISP_RC( rc, "dump_str_append_fmt() failed" )
    }

    /* append the cell-content */
    rc = vds_append_str( &(r_ctx->s_col), my_col_def->content.buf );
    DISP_RC( rc, "dump_str_append_str() failed" )
    if ( rc == 0 )
    {

        /* indent the cell-line, if requested */
        if ( r_ctx->ctx->indented_line_len > 0 )
        {
            rc = vds_indent( &(r_ctx->s_col),
                             r_ctx->ctx->indented_line_len,
                             r_ctx->col_defs->max_colname_chars + 2 );
            DISP_RC( rc, "dump_str_indent() failed" )
        }

        /* print a truncate-hint at the end of the line if truncated */
        if ( vds_truncated( &(r_ctx->s_col) ) )
        {
            rc = vds_rinsert( &(r_ctx->s_col), " ..." );
            DISP_RC( rc, "dump_str_rinsert() failed" )
        }
    }

    /* FINALLY we print the content of a column... */
    KOutMsg( "%s\n", r_ctx->s_col.buf );
}

static rc_t vdfo_print_row_default( const p_row_context r_ctx )
{
    rc_t rc = 0;
    if ( r_ctx->ctx->print_row_id )
        rc = KOutMsg( "ROW-ID = %u\n", r_ctx->row_id );

    if ( rc == 0 )
        VectorForEach( &(r_ctx->col_defs->cols), false, vdfo_print_col_default, r_ctx );

    if ( rc == 0 && r_ctx->ctx->lf_after_row > 0 )
    {
        uint16_t i=0;
        while ( i++ < r_ctx->ctx->lf_after_row && rc == 0 )
            rc = KOutMsg( "\n" );
    }
    return 0;
}

/*************************************************************************************
    CSV
*************************************************************************************/
static void CC vdfo_print_col_csv( void *item, void *data )
{
    rc_t rc = 0;
    p_col_def my_col_def = (p_col_def)item;
    p_row_context r_ctx = (p_row_context)data;

    if ( my_col_def->valid == false ) return;
    if ( my_col_def->excluded == true ) return;

    if ( r_ctx->col_nr > 0 )
    {
        rc = vds_append_str( &(r_ctx->s_col), "," );
        DISP_RC( rc, "dump_str_append_str() failed" )
    }
    if ( rc == 0 )
    {
        rc = vds_2_csv( &(my_col_def->content) );
        DISP_RC( rc, "dump_str_2_csv() failed" )
        if ( rc == 0 )
        {
            rc = vds_append_str( &(r_ctx->s_col), my_col_def->content.buf );
            DISP_RC( rc, "dump_str_append_str() failed" )
            r_ctx->col_nr++;
        }
    }
}

static rc_t vdfo_print_row_csv( const p_row_context r_ctx )
{
    rc_t rc = vds_clear( &(r_ctx->s_col) );
    DISP_RC( rc, "dump_str_clear() failed" )
    if ( rc == 0 )
    {
        r_ctx->col_nr = 0;
        VectorForEach( &(r_ctx->col_defs->cols), false, vdfo_print_col_csv, r_ctx );
        rc = KOutMsg( "%s\n", r_ctx->s_col.buf );
    }
    return rc;
}

/*************************************************************************************
    XML
*************************************************************************************/
static void CC vdfo_print_col_xml( void *item, void *data )
{
    p_col_def my_col_def = (p_col_def)item;
    if ( my_col_def->valid == false ) return;
    if ( my_col_def->excluded == true ) return;

    KOutMsg( " <%s>\n", my_col_def->name );
    KOutMsg( "%s", my_col_def->content.buf );
    KOutMsg( " </%s>\n", my_col_def->name );
}

static rc_t vdfo_print_row_xml( const p_row_context r_ctx )
{
    rc_t rc = vds_clear( &(r_ctx->s_col) );
    DISP_RC( rc, "dump_str_clear() failed" )
    if ( rc == 0 )
    {
        rc = KOutMsg( "<row>\n" );
        if ( rc  == 0 )
        {
            VectorForEach( &(r_ctx->col_defs->cols), false, vdfo_print_col_xml, r_ctx );
            rc = KOutMsg( "</row>\n");
        }
    }
    return rc;
}


/*************************************************************************************
    JSON
*************************************************************************************/
static void CC vdfo_print_col_json( void *item, void *data )
{
    rc_t rc = 0;
    p_col_def my_col_def = (p_col_def)item;

    if ( my_col_def->valid == false ) return;
    if ( my_col_def->excluded == true ) return;

    if ( ( my_col_def->type_desc.domain == vtdAscii )||
         ( my_col_def->type_desc.domain == vtdUnicode ) )
    {
        rc = vds_escape( &(my_col_def->content), '"', '\\' );
        DISP_RC( rc, "dump_str_escape() failed" )
        if ( rc == 0 )
        {
            rc = vds_enclose_string( &(my_col_def->content), '"', '"' );
            DISP_RC( rc, "dump_str_enclose_string() failed" )
        }
    }
    else
    {
        if ( my_col_def->type_desc.intrinsic_dim > 1 )
        {
            rc = vds_enclose_string( &(my_col_def->content), '[', ']' );
            DISP_RC( rc, "dump_str_enclose_string() failed" )
        }
    }

    if ( rc == 0 )
        KOutMsg( ",\n\"%s\":%s", my_col_def->name, my_col_def->content.buf );
}

static rc_t vdfo_print_row_json( const p_row_context r_ctx )
{
    rc_t rc = vds_clear( &(r_ctx->s_col) );
    DISP_RC( rc, "dump_str_clear() failed" )
    if ( rc == 0 )
    {
        rc = KOutMsg( "{\n" );
        if ( rc == 0 )
        {
            rc = KOutMsg( "\"row_id\": %lu", r_ctx->row_id );
            if ( rc == 0 )
            {
                VectorForEach( &(r_ctx->col_defs->cols), false, vdfo_print_col_json, r_ctx );
                rc = KOutMsg( "\n},\n\n" );
            }
        }
    }
    return rc;
}


/*************************************************************************************
    PIPED
*************************************************************************************/
static void CC vdfo_print_col_piped( void *item, void *data )
{
    rc_t rc = 0;
    p_col_def my_col_def = (p_col_def)item;
    p_row_context r_ctx = (p_row_context)data;

    if ( my_col_def->valid == false ) return;
    if ( my_col_def->excluded == true ) return;

    /* first we print the row_id and the column-name for every column! */
    KOutMsg( "%lu, %s: ", r_ctx->row_id, my_col_def->name );

    if ( ( my_col_def->type_desc.domain == vtdAscii )||
         ( my_col_def->type_desc.domain == vtdUnicode ) )
    {
        rc = vds_escape( &(my_col_def->content), '"', '\\' );
        DISP_RC( rc, "dump_str_escape() failed" )
        if ( rc == 0 )
        {
            rc = vds_enclose_string( &(my_col_def->content), '"', '"' );
            DISP_RC( rc, "dump_str_enclose_string() failed" )
        }
    }
    else
    {
        if ( my_col_def->type_desc.intrinsic_dim > 1 )
        {
            rc = vds_enclose_string( &(my_col_def->content), '[', ']' );
            DISP_RC( rc, "dump_str_enclose_string() failed" )
        }
    }

    if ( rc == 0 )
        KOutMsg( "%s\n", my_col_def->content.buf );
}


/*************************************************************************************
    TAB-delimited
*************************************************************************************/
static void CC vdfo_print_col_tab( void *item, void *data )
{
    rc_t rc = 0;
    p_col_def my_col_def = (p_col_def)item;
    p_row_context r_ctx = (p_row_context)data;

    if ( my_col_def->valid == false ) return;
    if ( my_col_def->excluded == true ) return;

    if ( r_ctx->col_nr > 0 )
    {
        rc = vds_append_str( &(r_ctx->s_col), "\t" );
        DISP_RC( rc, "dump_str_append_str() failed" )
    }
    if ( rc == 0 )
    {
        rc = vds_append_str( &(r_ctx->s_col), my_col_def->content.buf );
        DISP_RC( rc, "dump_str_append_str() failed" )
        r_ctx->col_nr++;
    }
}


static rc_t vdfo_print_row_piped( const p_row_context r_ctx )
{
    rc_t rc = vds_clear( &(r_ctx->s_col) );
    DISP_RC( rc, "dump_str_clear() failed" )
    if ( rc == 0 )
    {
        VectorForEach( &(r_ctx->col_defs->cols), false, vdfo_print_col_piped, r_ctx );
        rc = KOutMsg( "\n" );
    }
    return rc;
}

static rc_t vdfo_print_row_tab( const p_row_context r_ctx )
{
    rc_t rc = vds_clear( &(r_ctx->s_col) );
    DISP_RC( rc, "dump_str_clear() failed" )
    if ( rc == 0 )
    {
        r_ctx->col_nr = 0;
        VectorForEach( &(r_ctx->col_defs->cols), false, vdfo_print_col_tab, r_ctx );
        rc = KOutMsg( "%s\n", r_ctx->s_col.buf );
    }
    return rc;
}

/*************************************************************************************
    print-format-switch
*************************************************************************************/
rc_t vdfo_print_row( const p_row_context r_ctx )
{
    rc_t rc = 0;
    switch( r_ctx->ctx->format )
    {
    case df_default : rc = vdfo_print_row_default( r_ctx ); break;
    case df_csv     : rc = vdfo_print_row_csv( r_ctx ); break;
    case df_xml     : rc = vdfo_print_row_xml( r_ctx ); break;
    case df_json    : rc = vdfo_print_row_json( r_ctx ); break;
    case df_piped   : rc = vdfo_print_row_piped( r_ctx ); break;
    case df_tab     : rc = vdfo_print_row_tab( r_ctx ); break;
    default         : rc = vdfo_print_row_default( r_ctx ); break;
    }
    return rc;
}
