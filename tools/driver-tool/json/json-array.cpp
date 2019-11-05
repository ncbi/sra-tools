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

// this will allow expressions like
// array [ N ] = 10;
// where N is > the current number of elements.
#define ALLOW_MUTABLE_GET_VALUE_TO_INSERT_NULLS 1

namespace ncbi
{

    String JSONArray :: toString () const
    {
        throw JSONIncompatibleType (
            XP ( XLOC )
            << "this value cannot be converted to "
            << "a string"
            );
    }
    
    String JSONArray :: toJSON () const
    {
        SLocker lock ( busy );

        StringBuffer to_string;
        const char * sep = "";

        to_string += "[";

        size_t size = array . size ();
        for ( size_t i = 0; i < size; ++ i )
        {
            const JSONValue & value = * array [ i ];
            
            to_string += sep;
            to_string += value . toJSON();
            
            sep = ",";
        }
        
        to_string += "]";
        
        return to_string . stealString ();
    }
    
    String JSONArray :: readableJSON ( unsigned int indent ) const
    {
        SLocker lock ( busy );

        StringBuffer margin;
        for ( unsigned int i = 0; i < indent; ++ i )
            margin += "    ";

        StringBuffer to_string;
        to_string += margin;
        to_string += '[';
        margin += "    ";

        const char * sep = "\n";

        size_t size = array . size ();
        for ( size_t i = 0; i < size; ++ i )
        {
            const JSONValue & value = * array [ i ];
            
            to_string += sep;
            if ( value . isArray () )
                to_string += value . toArray () . readableJSON ( indent + 1 );

            else if ( value . isObject () )
                to_string += value . toObject () . readableJSON ( indent + 1 );

            else
            {
                to_string += margin;
                to_string += value . toJSON ();
            }
            
            sep = ",\n";
        }
        
        to_string += "\n";
        to_string += margin . stealString () . subString ( 4 );
        to_string += "]";
        
        return to_string . stealString ();
    }
    
    JSONValueRef JSONArray :: clone () const
    {
        return cloneArray () . release ();
    }
    
    JSONArrayRef JSONArray :: cloneArray () const
    {
        JSONArrayRef copy ( new JSONArray () );

        * copy = * this;

        return copy;
    }
    
    void JSONArray :: invalidate ()
    {
        XLocker lock ( busy );

        size_t i, count = array . size ();
        for ( i = 0; i < count; ++ i )
        {
            array [ i ] -> invalidate ();
        }
    }

    // asks whether array is empty
    bool JSONArray :: isEmpty () const
    {
        SLocker lock ( busy );
        return array . empty ();
    }

    // return the number of elements
    unsigned long int JSONArray :: count () const
    {
        SLocker lock ( busy );
        return ( unsigned long int ) array . size ();
    }

    // does an element exist
    bool JSONArray :: exists ( long int idx ) const
    {
        SLocker lock ( busy );

        if ( idx < 0 || ( size_t ) idx >= array . size () )
            return false;
        
        // TBD - determine whether null objects are considered to exist...
        return ! array [ idx ] -> isNull ();
    }

    // add a new element to end of array
    void JSONArray :: appendValue ( const JSONValueRef & elem )
    {
        XLocker lock ( busy );
        appendValueInt ( elem );
    }

    // set entry to a new value
    // will fill any undefined intermediate elements with null values
    // throws exception on negative index
    void JSONArray :: setValue ( long int idx, const JSONValueRef & elem )
    {
        XLocker lock ( busy );

        if ( idx < 0 )
        {
            throw JSONIndexOutOfBounds (
                XP ( XLOC )
                << "illegal index value - "
                << idx
                );
        }

        if ( elem == nullptr )
        {
            // basically trying to remove what's there
            JSONValueRef gone = removeValueInt ( idx );
            ( void ) gone;
        }
        else
        {
            // fill whatever is in-between
            while ( ( size_t ) idx > array . size () )
            {
                JSONValueRef nully = JSON :: makeNull ();
                appendValueInt ( nully );
            }

            // append...
            if ( ( size_t ) idx == array . size () )
                array . push_back ( elem . release () );

            // or replace
            else
            {
                delete array [ idx ];
                array [ idx ] = elem . release ();
            }
        }
        
    }

    // get value at index
    // throws exception on negative index or when element is undefined
    JSONValue & JSONArray :: getValue ( long int idx )
    {
        {
            SLocker lock ( busy );
            if ( idx >= 0 && ( size_t ) idx < array . size () )
                return * array [ idx ];
        }

#if ALLOW_MUTABLE_GET_VALUE_TO_INSERT_NULLS
        if ( idx >= 0 )
        {
            setValue ( idx, JSON :: makeNull () );
            SLocker lock ( busy );
            if ( ( size_t ) idx < array . size () )
                return * array [ idx ];
        }
#endif
        throw JSONIndexOutOfBounds (
            XP ( XLOC )
            << "illegal index value - "
            << idx
            );
    }
    
    const JSONValue & JSONArray :: getValue ( long int idx ) const
    {
        SLocker lock ( busy );
        if ( idx < 0 || ( size_t ) idx >= array . size () )
        {
            throw JSONIndexOutOfBounds (
                XP ( XLOC )
                << "illegal index value - "
                << idx
                );
        }

        return * array [ idx ];
    }

    // remove and delete an entry if valid
    // replaces valid internal entries with null element
    // deletes trailing null elements making them undefined
    JSONValueRef JSONArray :: removeValue ( long int idx )
    {
        XLocker lock ( busy );
        return removeValueInt ( idx );
    }

    // C++ assignment
    JSONArray & JSONArray :: operator = ( const JSONArray & a )
    {
        XLocker lock1 ( busy );
        SLocker lock2 ( a . busy );

        // forget everything we have right now
        clear ();

        // run through all of the new ones
        size_t i, count = a . array . size ();
        for ( i = 0; i < count; ++ i )
        {
            // clone them
            JSONValueRef elem = a . array [ i ] -> clone ();

            // append them
            appendValueInt ( elem );
        }

        return * this;
    }
    
    JSONArray :: JSONArray ( const JSONArray & a )
    {
        SLocker lock ( a . busy );

        size_t i, count = a . array . size ();
        for ( i = 0; i < count; ++ i )
        {
            JSONValueRef elem = a . array [ i ] -> clone ();
            appendValueInt ( elem );
        }
    }
        
    JSONArray :: ~ JSONArray () noexcept
    {
        try
        {
            clear ();
        }
        catch ( ... )
        {
        }
    }

    // used to empty out the array before copy
    void JSONArray :: clear ()
    {
        while ( ! array . empty () )
        {
            delete array . back ();
            array . pop_back ();
        }
    }

    // add a new element to end of array
    void JSONArray :: appendValueInt ( const JSONValueRef & elem )
    {
        if ( elem == nullptr )
            array . push_back ( JSON :: makeNull () . release () );
        else
            array . push_back ( elem . release () );
    }

    JSONValueRef JSONArray :: removeValueInt ( long int idx )
    {
        JSONValueRef removed;

       // test for illegal index
        if ( idx < 0 || ( size_t ) idx >= array . size () )
            return removed;

        // if the element is already null
        if ( array [ idx ] -> isNull () )
            return removed;

        // delete existing element
        removed = array [ idx ];

        // if it was not the last element in the array
        // just replace it with a null value
        if ( ( size_t ) idx + 1 < array . size () )
            array [ idx ] = JSON :: makeNull () . release ();
        else
        {
            // otherwise, forget the last element in the array
            array . pop_back ();

            // and test from the back toward front for null elements
            // up until the first non-null (resize the array)
            while ( ! array . empty () )
            {
                // any non-null element breaks loop
                if ( ! array . back () -> isNull () )
                    break;

                // delete the null
                //delete array . back ();
                array . pop_back ();
            }
        }

        return removed;
    }

    JSONArray :: JSONArray ()
    {
    }
    
}
