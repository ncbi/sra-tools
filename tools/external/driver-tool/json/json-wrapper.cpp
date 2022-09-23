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

namespace ncbi
{
    // JSONWrapper - an implementation of JSONValue
    
    bool JSONWrapper :: isNull () const
    {
        SLocker lock ( busy );
        return type == jvt_null;
    }
    
    bool JSONWrapper :: isBoolean () const
    {
        SLocker lock ( busy );
        return type == jvt_bool;
    }
    
    bool JSONWrapper :: isInteger () const
    {
        SLocker lock ( busy );
        return type == jvt_int;
    }
    
    bool JSONWrapper :: isNumber () const
    {
        SLocker lock ( busy );
        return type == jvt_num || type == jvt_int;
    }
    
    bool JSONWrapper :: isString () const
    {
        SLocker lock ( busy );
        return type == jvt_str;
    }
    
    JSONValue & JSONWrapper :: setNull ()
    {
        XLocker lock ( busy );
        delete value;
        value = nullptr;
        type = jvt_null;

        return * this;
    }
    
    JSONValue & JSONWrapper :: setBoolean ( bool val )
    {
        XLocker lock ( busy );
        JSONBoolean * b = new JSONBoolean ( val );
        delete value;
        value = b;
        type = jvt_bool;
        
        return * this;
    }
    
    JSONValue & JSONWrapper :: setInteger ( long long int val )
    {
        XLocker lock ( busy );
        JSONInteger * i = new JSONInteger ( val );
        delete value;
        value = i;
        type = jvt_int;
        
        return * this;
    }
    
    JSONValue & JSONWrapper :: setDouble ( long double val, unsigned int precision )
    {
        return setNumber ( double_to_string ( val, precision ) );
    }
    
    JSONValue & JSONWrapper :: setNumber ( const String & val )
    {
        XLocker lock ( busy );
        JSONNumber * n = new JSONNumber ( val );
        delete value;
        value = n;
        type = jvt_num;
        
        return * this;
    }
    
    JSONValue & JSONWrapper :: setString ( const String & val )
    {
        XLocker lock ( busy );
        JSONString * s = new JSONString ( val );
        delete value;
        value = s;
        type = jvt_str;
        
        return * this;
    }

    bool JSONWrapper :: toBoolean () const
    {
        SLocker lock ( busy );
        if ( type == jvt_bool )
        {
            assert ( value != nullptr );
            return ( ( const JSONBoolean * ) value ) -> toBoolean ();
        }

        return JSONValue :: toBoolean ();
    }
    
    long long JSONWrapper :: toInteger () const
    {
        SLocker lock ( busy );
        if ( type == jvt_int )
        {
            assert ( value != nullptr );
            return ( ( const JSONInteger * ) value ) -> toInteger ();
        }

        return JSONValue :: toInteger ();
    }
    
    String JSONWrapper :: toNumber () const
    {
        SLocker lock ( busy );
        switch ( type )
        {
        case jvt_int:
        case jvt_num:
            assert ( value != nullptr );
            return value -> toString ();
        default:
            break;
        }

        return JSONValue :: toNumber ();
    }
    
    String JSONWrapper :: toString () const
    {
        SLocker lock ( busy );

        if ( value == nullptr )
            return "null";

        return value -> toString ();
    }
    
    String JSONWrapper :: toJSON () const
    {
        SLocker lock ( busy );

        if ( value == nullptr )
            return "null";
        
        return value -> toJSON ();
    }
    
    JSONValueRef JSONWrapper :: clone () const
    {
        SLocker lock ( busy );

        if ( value == nullptr )
            return JSONValueRef ( new JSONWrapper ( jvt_null ) );

        return JSONValueRef ( new JSONWrapper ( type, value -> clone () ) );
    }

    void JSONWrapper :: invalidate ()
    {
        XLocker lock ( busy );
        if ( value != nullptr )
            value -> invalidate ();
    }

    JSONWrapper & JSONWrapper :: operator = ( const JSONWrapper & val )
    {
        JSONPrimitive * copy = val . value ? val . value -> clone () : nullptr;
        delete value;
        value = copy;
        type = val . type;

        return * this;
    }

    JSONWrapper :: JSONWrapper ( const JSONWrapper & val )
        : value ( nullptr )
        , type ( jvt_null )
    {
        SLocker lock ( val . busy );
        if ( val . value != nullptr )
        {
            value = val . value -> clone ();
            type = val . type;
        }
    }

    JSONWrapper :: JSONWrapper ( JSONValType _type )
        : value ( nullptr )
        , type ( _type )
    {
        assert ( type == jvt_null );
    }

    JSONWrapper :: JSONWrapper ( JSONValType _type, JSONPrimitive * val )
        : value ( val )
        , type ( _type )
    {
    }

    JSONWrapper :: ~ JSONWrapper () noexcept
    {
        delete value;
        value = nullptr;
        type = jvt_null;
    }
}
