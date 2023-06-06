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

#include "vdb-dump-view-spec.h"

#include <klib/rc.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/token.h>

#include <vdb/cursor.h>
#include <vdb/view.h>
#include <vdb/table.h>
#include <vdb/database.h>
#include <vdb/manager.h>

static
rc_t Error ( char * p_error, size_t p_error_size, const char * p_message )
{
    size_t num_writ;
    string_printf( p_error, p_error_size, & num_writ, p_message );
    return RC( rcVDB, rcTable, rcConstructing, rcFormat, rcIncorrect );
}

static rc_t view_args_parse ( view_spec * p_owner, KTokenSource * p_src, char * p_error, size_t p_error_size, bool p_optional );

static
rc_t
view_arg_parse( view_spec * p_owner, KTokenSource * p_src, char * p_error, size_t p_error_size )
{
    /* view_arg = ident [ view_args ] */

    rc_t rc = 0;
    KToken tok;
    if ( KTokenizerNext ( kDefaultTokenizer, p_src, & tok ) -> id != eIdent )
    {
        rc = Error ( p_error, p_error_size, "missing view parameter(s)" );
    }
    else
    {
        char * name = string_dup ( tok . str . addr, tok . str . size );
        view_spec * self = ( view_spec * ) malloc ( sizeof ( * self ) );
        if ( self == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            VectorInit( & self -> args, 0, 4 );
            rc = view_args_parse ( self, p_src, p_error, p_error_size, true );
            if ( rc == 0 )
            {
                self -> name = name;
                VectorAppend ( & p_owner -> args, NULL, self );
            }
            else
            {
                free ( name );
                free ( self );
            }
        }
    }
    return rc;
}

static
rc_t
view_args_parse ( view_spec * p_owner, KTokenSource * p_src, char * p_error, size_t p_error_size, bool p_optional )
{
    /* view_args = '<' view_arg { ',' view_arg } >' */

    rc_t rc = 0;
    KToken tok;
    if ( KTokenizerNext ( kDefaultTokenizer, p_src, & tok ) -> id != eLeftAngle )
    {
        KTokenSourceReturn ( p_src, & tok );
        if ( p_optional )
        {
            rc = 0;
        }
        else
        {
            rc = Error ( p_error, p_error_size, "missing '<' after the view name" );
        }
    }
    else
    {
        do
        {
            rc = view_arg_parse ( p_owner, p_src, p_error, p_error_size );
        }
        while ( rc == 0 && KTokenizerNext ( kDefaultTokenizer, p_src, & tok ) -> id == eComma );

        if ( rc == 0 )
        {
            switch ( tok . id )
            {
            case eRightAngle:
                break;
            case eDblRightAngle:
                {   /* split ">>"" and return '>' */
                    KTokenText tt;
                    KToken t;
                    KTokenTextInitCString ( &tt , ">", "" );
                    t . txt = & tt;
                    t . sym = NULL;
                    t . str . addr = tok . str . addr + 1;
                    t . str . size = 1;
                    t . id = eRightAngle;
                    KTokenSourceReturn ( p_src, & t );
                }
                break;
            default:
                rc = Error ( p_error, p_error_size, "expected ',' or '>' after a view parameter" );
                break;
            }
        }
    }

    return rc;
}

rc_t
view_spec_parse ( const char * p_spec, view_spec ** p_self, char * p_error, size_t p_error_size )
{
    rc_t rc = 0;
    if ( p_self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    else if ( p_error == NULL || p_error_size == 0 )
    {
        * p_self = NULL;
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    else
    {
        view_spec * self = ( view_spec * ) malloc ( sizeof ( * self ) );
        p_error [0] = 0;
        if ( self == NULL )
        {
            * p_self = NULL;
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            self -> name = NULL;
            VectorInit( & self -> args, 0, 4 );

            if ( p_spec == NULL )
            {
                rc = Error ( p_error, p_error_size, "empty view specification" );
            }
            else
            {   /* parse p_spec */
                /* ident view-args  */
                String input;
                String empty;
                KTokenText txt;
                KTokenSource src;
                KToken tok;
                StringInitCString ( & input, p_spec );
                StringInit ( & empty, NULL, 0, 0 );
                KTokenTextInit ( & txt, & input, & empty );
                KTokenSourceInit ( & src, & txt );
                if ( KTokenizerNext ( kDefaultTokenizer, & src, & tok ) -> id != eIdent )
                {
                    rc = Error ( p_error, p_error_size, "missing view name" );
                }
                else
                {
                    self -> name = string_dup ( tok . str . addr, tok . str . size );
                    if ( self -> name == NULL )
                    {
                        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                    }
                    else
                    {
                        rc = view_args_parse ( self, & src, p_error, p_error_size, false );
                        if ( rc == 0 && KTokenizerNext ( kDefaultTokenizer, & src, & tok ) -> id != eEndOfInput )
                        {
                            rc = Error ( p_error, p_error_size, "extra characters after '>'" );
                        }
                    }
                }
            }
            if ( rc != 0 )
            {
                view_spec_free ( self );
                * p_self = NULL;
            }
            else
            {
                * p_self = self;
            }
        }
    }

    return rc;
}

static
void CC free_arg ( void* item, void * data )
{
    view_spec_free ( ( view_spec * ) item );
}

void view_spec_free ( view_spec * p_self )
{
    if ( p_self != NULL )
    {
        VectorWhack( & p_self -> args, free_arg, NULL );
        free ( p_self -> name );
        free ( p_self );
    }
}

static
rc_t
InstantiateView ( const VDatabase * p_db, const view_spec * p_self, const struct VSchema * p_schema, const struct VView ** p_view )
{
    /* open view and bind parameters */

    const VDBManager * mgr;
    rc_t rc = VDatabaseOpenManagerRead ( p_db, & mgr );
    if ( rc == 0 )
    {
        const struct VView * view;
        rc = VDBManagerOpenView ( mgr, & view, p_schema, p_self -> name );
        if ( rc == 0 )
        {
            uint32_t count = VectorLength ( & p_self -> args );
            if ( count != VViewParameterCount ( view ) )
            {
                rc = RC( rcVDB, rcCursor, rcConstructing, rcParam, rcIncorrect );
            }
            else
            {
                uint32_t start = VectorStart ( & p_self -> args );
                uint32_t i;
                for ( i = 0; i < count; ++i )
                {
                    uint32_t idx = start + i;
                    const String * paramName;
                    bool is_table;
                    const view_spec * arg = (const view_spec*) VectorGet ( & p_self -> args, idx );
                    rc = VViewGetParameter ( view, idx, & paramName, & is_table );
                    if ( rc == 0 )
                    {
                        if ( is_table )
                        {
                            if ( VectorLength ( & arg -> args ) != 0 )
                            {
                                rc = RC( rcVDB, rcCursor, rcConstructing, rcParam, rcExcessive );
                            }
                            else
                            {
                                const VTable * tbl;
                                rc = VDatabaseOpenTableRead ( p_db, & tbl, "%s", arg -> name );
                                if ( rc == 0 )
                                {
                                    rc = VViewBindParameterTable ( view, paramName, tbl );
                                    VTableRelease ( tbl );
                                }
                            }
                        }
                        else /* view */
                        {
                            if ( VectorLength ( & arg -> args ) == 0 )
                            {
                                rc = RC( rcVDB, rcCursor, rcConstructing, rcParam, rcInsufficient );
                            }
                            else
                            {
                                const VView * v;
                                rc = InstantiateView ( p_db, arg, p_schema, & v );
                                if ( rc == 0 )
                                {
                                    rc = VViewBindParameterView ( view, paramName, v );
                                    VViewRelease ( v );
                                }
                            }
                        }
                    }
                    if ( rc != 0 )
                    {
                        break;
                    }
                }
            }
            if ( rc == 0 )
            {
                * p_view = view;
            }
            else
            {
                VViewRelease ( view );
            }
        }
        VDBManagerRelease ( mgr );
    }
    return rc;
}

rc_t
view_spec_open ( view_spec *             p_self,
                 const VDatabase *       p_db,
                 const struct VSchema *  p_schema,
                 const VView **          p_view )
{
    if ( p_self == NULL )
    {
        return RC( rcVDB, rcCursor, rcConstructing, rcSelf, rcNull );
    }
    else if ( p_db == NULL || p_schema == NULL || p_view == NULL )
    {
        return RC( rcVDB, rcCursor, rcConstructing, rcParam, rcNull );
    }
    else
    {
        return InstantiateView ( p_db, p_self, p_schema, p_view );
    }
}