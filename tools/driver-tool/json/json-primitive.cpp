/*==============================================================================
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

#include "json-priv.hpp"

#include <assert.h>
#include <string.h>
#include <errno.h>

namespace ncbi
{

    String string_to_json ( const String & str )
    {
        StringBuffer quoted;
        quoted += '"';

        String :: Iterator pos = str . makeIterator ();
        String :: Iterator it = pos;
        
        for ( ; it . isValid () ; ++ it )
        {
            UTF32 ch = * it;
            if ( ch < 128 )
            {
                const ASCII * esc = nullptr;
                switch ( ch )
                {
                case '"':
                    esc = "\\\"";
                    break;
                case '\\':
                    esc = "\\\\";
                    break;
                case '\b':
                    esc = "\\b";
                    break;
                case '\f':
                    esc = "\\f";
                    break;
                case '\n':
                    esc = "\\n";
                    break;
                case '\r':
                    esc = "\\r";
                    break;
                case '\t':
                    esc = "\\t";
                    break;
                default:
                {
                    if ( ! isprint ( ch ) )
                    {
                        // unicode escape hex sequence
                        ASCII buff [ 32 ];
                        size_t len = snprintf( buff, sizeof buff, "\\u%04x",
                            ( unsigned int ) ( unsigned char ) ch );
                        assert ( len == 6 );
                        
                        if ( it != pos )
                        {
                            // put all of the stuff we've seen so far                            
                            quoted += str . subString ( pos . charIndex (),
                                it . charIndex () - pos . charIndex () );
                        }

                        // follow it by the escaped sequence
                        quoted += buff;

                        // skip over the non-printable character
                        pos = it;
                        ++ pos;
                    }
                }}

                // special character
                if ( esc != nullptr )
                {
                    quoted += str . subString ( pos . charIndex (),
                        it . charIndex () - pos . charIndex () );
                    quoted += esc;
                    
                    pos = it;
                    ++ pos;
                }
            }

        }
        
        if ( pos . isValid () )
            quoted += str . subString ( pos . charIndex () );
       
        quoted +=  "\"";

        return quoted . stealString ();
    }

    String JSONPrimitive :: toJSON () const
    {
        return toString ();
    }
    
    String JSONBoolean :: toString () const
    {
        return String ( value ? "true" : "false" );
    }

    String JSONInteger :: toString () const
    {
        ASCII buffer [ 128 ];
        int status = snprintf ( buffer, sizeof buffer, "%lld", value );
        if ( status < 0 )
        {
            status = errno;
            throw InternalError (
                XP ( XLOC )
                << "failed to convert long long integer to string"
                << xcause
                << syserr ( status )
                );
        }
        if ( ( size_t ) status >= sizeof buffer )
        {
            throw InternalError (
                XP ( XLOC )
                << "failed to convert long long integer to string"
                << xcause
                << "buffer insufficient"
                );
        }
        return String ( buffer, status );
    }

    String JSONString :: toJSON () const
    {
        return string_to_json ( value );
    }
}
