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

namespace ncbi
{

    // These are all false because it's a base class and can't know what it is
    bool JSONValue :: isNull () const
    {
        return false;
    }
    
    bool JSONValue :: isBoolean () const
    {
        return false;
    }
    
    bool JSONValue :: isInteger () const
    {
        return false;
    }
    
    bool JSONValue :: isNumber () const
    {
        return false;
    }
    
    bool JSONValue :: isString () const
    {
        return false;
    }
    
    bool JSONValue :: isArray () const
    {
        return false;
    }

    bool JSONValue :: isObject () const
    {
        return false;
    }

    // the sets all throw exceptions
    JSONValue & JSONValue :: setNull ()
    {
        // this is an internal error
        // same for other setters
        throw InternalError ( XP ( XLOC ) << "INTERNAL ERROR" ); // test hit
    }
    
    JSONValue & JSONValue :: setBoolean ( bool val )
    {
        throw InternalError ( XP ( XLOC ) << "INTERNAL ERROR" ); // test hit
    }
    
    JSONValue & JSONValue :: setInteger ( long long int val )
    {
        throw InternalError ( XP ( XLOC ) << "INTERNAL ERROR" ); // test hit
    }
    
    JSONValue & JSONValue :: setDouble ( long double val, unsigned int precision )
    {
        throw InternalError ( XP ( XLOC ) << "INTERNAL ERROR" ); // test hit
    }
    
    JSONValue & JSONValue :: setNumber ( const String & val )
    {
        throw InternalError ( XP ( XLOC ) << "INTERNAL ERROR" ); // test hit
    }
    
    JSONValue & JSONValue :: setString ( const String & val )
    {
        throw InternalError ( XP ( XLOC ) << "INTERNAL ERROR" ); // test hit
    }

    // these are accessors
    bool JSONValue :: toBoolean () const
    {
        // this is a logic-error
        // same for other casts
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "bool"
            );
    }
    
    long long JSONValue :: toInteger () const
    {
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "an integer"
            );
    }
    
    String JSONValue :: toNumber () const
    {
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "a number"
            );
    }
    
    String JSONValue :: toString () const
    {
       // default behavior is to return JSON text
        return toJSON ();
    }
    
    String JSONValue :: readableJSON ( unsigned int indent ) const
    {
       // default behavior is to return JSON text
        return toJSON ();
    }
    
    JSONArray & JSONValue :: toArray ()
    {
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "an array"
            );
    }
    
    const JSONArray & JSONValue :: toArray () const
    {
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "an array"
            );
    }
    
    JSONObject & JSONValue :: toObject ()
    {
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "an object"
            );
    }
    
    const JSONObject & JSONValue :: toObject () const
    {
        throw JSONIncompatibleType (
            XP ( XLOC ) << "this value cannot be converted to "
            << "an object"
            );
    }
    
    JSONValueRef JSONValue :: clone () const
    {
        // this is an internal
        throw InternalError ( XP ( XLOC ) << "this value cannot be cloned" );
    }
    
    JSONValue :: ~ JSONValue () noexcept
    {
    }
    
}
