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

static
rc_t Error ( view_spec * p_self, const char * p_message )
{
    size_t num_writ;
    string_printf( p_self -> error, sizeof ( p_self -> error ), & num_writ, p_message );
    return RC( rcVDB, rcTable, rcConstructing, rcFormat, rcIncorrect );
}

rc_t
view_spec_parse ( const char * p_spec, view_spec ** p_self )
{
    rc_t rc = 0;
    if ( p_self == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    else
    {
        view_spec * self = ( view_spec * ) malloc ( sizeof ( * self ) );
        if ( self == NULL )
        {
            * p_self = NULL;
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            StringInit ( & self -> view_name, NULL, 0, 0 );
            VectorInit( & self -> args, 0, 4 );
            self -> error [0] = 0;
            * p_self = self;

            if ( p_spec == NULL )
            {
                rc = Error ( self, "empty view specification" );
            }
            else
            { /* parse p_spec */
                /* ident '<' ident { ',' ident } >' */
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
                    rc = Error ( self, "missing view name" );
                }
                else
                {
                    self -> view_name = tok . str;

                    if ( KTokenizerNext ( kDefaultTokenizer, & src, & tok ) -> id != eLeftAngle )
                    {
                        rc = Error ( self, "missing '<' after the view name" );
                    }
                    else if ( KTokenizerNext ( kDefaultTokenizer, & src, & tok ) -> id != eIdent )
                    {
                        rc = Error ( self, "missing view parameter(s)" );
                    }
                    else
                    {
                        VectorAppend ( & self -> args, NULL, string_dup ( tok . str . addr, tok . str . size ) );

                        while ( KTokenizerNext ( kDefaultTokenizer, & src, & tok ) -> id == eComma )
                        {
                            if ( KTokenizerNext ( kDefaultTokenizer, & src, & tok ) -> id != eIdent )
                            {
                                return Error ( self, "missing view parameter(s) after ','" );
                            }
                            VectorAppend ( & self -> args, NULL, string_dup ( tok . str . addr, tok . str . size ) );
                        }
                        if ( tok . id != eRightAngle )
                        {
                            rc = Error ( self, "expected ',' or '>' after a view parameter" );
                        }
                        else if ( KTokenizerNext ( kDefaultTokenizer, & src, & tok ) -> id != eEndOfInput )
                        {
                            rc = Error ( self, "extra characters after '>'" );
                        }
                    }
                }
            }
        }
    }
    return rc;
}

static
void CC free_arg ( void* item, void * data )
{
    free ( item );
}

void view_spec_free ( view_spec * p_self )
{
    if ( p_self != NULL )
    {
        VectorWhack( & p_self -> args, free_arg, NULL );
    }
}
