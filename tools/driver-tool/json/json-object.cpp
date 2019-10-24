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

    String JSONObject :: toString () const
    {
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "a string"
            );
    }
    
    // JSONValue interface implementations
    String JSONObject :: toJSON () const
    {
        SLocker lock ( busy );

        StringBuffer to_string;
        const UTF8 * sep = "";

        to_string += "{";
        
        for ( auto it = members . begin (); it != members . end (); ++ it )
        {
            String key =  it -> first;
            const JSONValue & value = * it -> second;
            
            to_string += sep;
            to_string += string_to_json ( key );
            to_string += ":";
            to_string += value . toJSON();
            
            sep = ",";
        }
        
        to_string += "}";
        
        return to_string . stealString ();
    }
    
    String JSONObject :: readableJSON ( unsigned int indent ) const
    {
        SLocker lock ( busy );

        StringBuffer margin;
        for ( unsigned int i = 0; i < indent; ++ i )
            margin += "    ";

        StringBuffer to_string;
        to_string += margin;
        to_string += "{";
        margin += "    ";

        const char * sep = "\n";

        // detect the apparent longest member length
        count_t longest_mbr_len = 0;
        for ( auto it = members . begin (); it != members . end (); ++ it )
        {
            const JSONValue & value = * it -> second;
            if ( ! value . isArray () && ! value . isObject () )
            {
                String key = it -> first;
                
                // count the length in characters
                count_t mbr_len = key . length ();

                // keep the longest                
                if ( mbr_len > longest_mbr_len )
                    longest_mbr_len = mbr_len;
            }
        }
        
        // build the string
        for ( auto it = members . begin (); it != members . end (); ++ it )
        {
            String key = it -> first;
            const JSONValue & value = * it -> second;
            
            to_string += sep;
            to_string += margin;
            to_string += string_to_json ( key );
            if ( value . isArray () )
            {
                to_string += " :\n";
                to_string += value . toArray () . readableJSON ( indent + 1 );
            }
            else if ( value . isObject () )
            {
                to_string += " :\n";
                to_string += value . toObject () . readableJSON ( indent + 1 );
            }
            else
            {
                for ( count_t l = key . length (); l < longest_mbr_len; ++ l )
                    to_string += ' ';
                
                to_string += " : ";
                to_string += value . toJSON ();
            }
            
            sep = ",\n";
        }
        
        to_string += "\n";
        to_string += margin . stealString () . subString ( 4 );
        to_string += "}";
        
        return to_string . stealString ();
    }
    
    JSONValueRef JSONObject :: clone () const
    {
        return cloneObject () . release ();
    }
    
    JSONObjectRef JSONObject :: cloneObject () const
    {
        JSONObjectRef copy ( new JSONObject () );
        
        * copy = * this;
        
        return copy;
    }

    void JSONObject :: invalidate ()
    {
        XLocker lock ( busy );
        for ( auto it = members . begin (); it != members . end (); ++ it )
            it -> second -> invalidate ();
    }

    // asks whether object is empty
    bool JSONObject :: isEmpty () const
    {
        SLocker lock ( busy );
        return members . empty ();
    }

    // does a member exist
    bool JSONObject :: exists ( const String & name ) const
    {
        SLocker lock ( busy );

        auto it = members . find ( name );

        if ( it == members . end () )
                return false;
        
        return true;
    }

    // return the number of members
    unsigned long int JSONObject :: count () const
    {
        SLocker lock ( busy );
        return members . size ();
    }
        
    // return names/keys
    std :: vector < String > JSONObject :: getNames () const
    {
        SLocker lock ( busy );

        std :: vector < String > names;
        
        for ( auto it = members . cbegin (); it != members . cend (); ++ it )
            names . push_back ( it -> first );
        
        return names;
    }
        
    // add a new ( name, value ) pair
    // "name" must be unique or an exception will be thrown
    void JSONObject :: addValue ( const String & name, const JSONValueRef & val )
    {
        XLocker lock ( busy );

        auto it = members . find ( name );
        
        // error if it exists
        if ( it != members . end () )
        {
            throw JSONUniqueConstraintViolation (
                XP ( XLOC )
                << "duplicate member name: '"
                << name
                << '\''
                );

        }

        // steal the object pointer
        members . emplace ( name, val . release () );
    }
        
    // set entry to a new value
    // throws exception if entry exists and is final
    void JSONObject :: setValue ( const String & name, const JSONValueRef & val )
    {
        XLocker lock ( busy );
        setValueInt ( name, val );
    }

    // get named value
    JSONValue & JSONObject :: getValue ( const String & name )
    {
        SLocker lock ( busy );

        auto it = members . find ( name );
        if ( it != members . end () )
            return * it -> second;

        throw JSONNoSuchName (
            XP ( XLOC )
            << "Member '"
            << name
            <<"' not found"
            );

    }
    
    const JSONValue & JSONObject :: getValue ( const String & name ) const
    {
        SLocker lock ( busy );

        auto it = members . find ( name );
        if ( it != members . cend () )
            return * it -> second;

        throw JSONNoSuchName (
            XP ( XLOC )
            << "Member '"
            << name
            <<"' not found"
            );

    }
        
    // remove a named value
    JSONValueRef JSONObject :: removeValue ( const String & name )
    {
        XLocker lock ( busy );

        JSONValueRef removed;

        auto it = members . find ( name );
        if ( it != members . end () )
        {
            removed = it -> second;
            members . erase ( it );
        }

        return removed;
    }

    // C++ assignment
    JSONObject & JSONObject :: operator = ( const JSONObject & obj )
    {
        XLocker lock1 ( busy );
        SLocker lock2 ( obj . busy );

        clear ();
        
        for ( auto it = obj . members . cbegin (); it != obj . members . cend (); ++ it )
        {
            String name = it -> first;
            JSONValueRef val = it -> second  -> clone ();
            setValueInt ( name, val );
        }
        
        return * this;
    }
    
    JSONObject :: JSONObject ( const JSONObject & obj )
    {
        SLocker lock ( obj . busy );
        for ( auto it = obj . members . cbegin (); it != obj . members . cend (); ++ it )
        {
            String name = it -> first;
            JSONValueRef val = it -> second  -> clone ();
            setValueInt ( name, val );
        }
    }

    JSONObject :: ~ JSONObject () noexcept
    {
        try
        {
            clear ();
        }
        catch ( ... )
        {
        }
    }
    
    // used to empty out the object before copy
    void JSONObject :: clear ()
    {
        if ( ! members . empty () )
        {
            for ( auto it = members . begin (); it != members . end (); )
            {
                delete it -> second;
                it = members . erase ( it );
            }
        }
    }

    void JSONObject :: setValueInt ( const String & name, const JSONValueRef & val )
    {
        auto it = members . find ( name );
        
        // if doesnt exist, add
        if ( it == members . end () )
            members . emplace ( name, val . release () );
        else
        {
            // overwrite value
            // TBD - need to look at thread safety
            delete it -> second;
            it -> second = val . release ();
        }
    }

    JSONObject :: JSONObject ()
    {
    }

}
