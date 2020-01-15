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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#pragma once

#include <ncbi/secure/ref.hpp>
#include <ncbi/secure/busy.hpp>
#include <ncbi/secure/except.hpp>
#include <ncbi/secure/string.hpp>

#include <vector>
#include <map>

/**
 * @file ncbi/json.hpp
 * @brief JSON Object Model - RFC 7159
 *
 * JavaScript Object Notation is a textual
 * approach to encoding general purpose values
 * and structures. A C++ object model is more
 * useful when native C++ constructs are also
 * included, such as numbers and arrays.
 *
 * The object model presented herein is therefor
 * very C++ centric, while at the same time attempts
 * to retain a flavor that is portable to other languages.
 */

namespace ncbi
{

    /*=====================================================*
     *                      FORWARDS                       *
     *=====================================================*/

    class JSONValue;
    class JSONArray;
    class JSONObject;
    struct JSONString;
    struct JSONNumber;


    /*=====================================================*
     *                      TYPEDEFS                       *
     *=====================================================*/

    /**
     * @typedef JSONValueRef
     * @brief a unique reference to a JSONValue
     */
    typedef XRef < JSONValue > JSONValueRef;

    /**
     * @typedef JSONArrayRef
     * @brief a unique reference to a JSONArray
     */
    typedef XRef < JSONArray > JSONArrayRef;

    /**
     * @typedef JSONObjectRef
     * @brief a unique reference to a JSONObject
     */
    typedef XRef < JSONObject > JSONObjectRef;


    /*=====================================================*
     *                        JSON                         *
     *=====================================================*/
    
    /**
     * @class JSON
     * @brief JavaScript Object Notation Management
     *
     * Globally accessible factory functions.
     */
    class JSON
    {
    public:

        /**
         * @struct Limits
         * @brief a structure to hold parsing limit constants
         */
        struct Limits
        {
            /**
             * @fn Limits
             * @brief set default limits
             */
            Limits ();
            
            unsigned int json_string_size;    //!< total size of JSON string
            unsigned int recursion_depth;     //!< parser stack depth
            unsigned int numeral_length;      //!< maximum number of characters in number
            unsigned int string_size;         //!< maximum number of bytes in string
            unsigned int string_length;       //!< maximum number of characters in string
            unsigned int array_elem_count;    //!< maximum number of elements in array
            unsigned int object_mbr_count;    //!< maximum number of members in object
        };
        
        /**
         * @fn parse
         * @overload parse JSON string using default Limits
         * @param json JSON text as described in RFC-7159
         * @exception MalformedJSON if text does not conform to standard
         * @exception JSONLimitViolation if source text exceeds established limits
         * @return JSONValueRef representing legal JSON source
         */
        static JSONValueRef parse ( const String & json );

        /**
         * @fn parse
         * @overload parse JSON string using provided Limits
         * @param json JSON text as described in RFC-7159
         * @exception MalformedJSON if text does not conform to standard
         * @exception JSONLimitViolation if source text exceeds established limits
         * @return JSONValueRef representing legal JSON source
         */
        static JSONValueRef parse ( const Limits & lim, const String & json );

        /**
         * @fn parseArray
         * @overload parse JSON string using default Limits
         * @param json JSON text as described in RFC-7159
         * @exception MalformedJSON if text does not conform to standard
         * @exception JSONLimitViolation if source text exceeds established limits
         * @return JSONArrayRef representing legal JSON source
         */
        static JSONArrayRef parseArray ( const String & json );

        /**
         * @fn parseObject
         * @overload parse JSON string using provided Limits
         * @param json JSON text as described in RFC-7159
         * @exception MalformedJSON if text does not conform to standard
         * @exception JSONLimitViolation if source text exceeds established limits
         * @return JSONArrayRef representing legal JSON source
         */
        static JSONArrayRef parseArray ( const Limits & lim, const String & json );

        /**
         * @fn parseObject
         * @overload parse JSON string using default Limits
         * @param json JSON text as described in RFC-7159
         * @exception MalformedJSON if text does not conform to standard
         * @exception NotJSONObject if text does not represent a JSON object
         * @exception JSONLimitViolation if source text exceeds established limits
         * @return JSONObjectRef representing legal JSON source
         */
        static JSONObjectRef parseObject ( const String & json );

        /**
         * @fn parseObject
         * @overload parse JSON string using provided Limits
         * @param json JSON text as described in RFC-7159
         * @exception MalformedJSON if text does not conform to standard
         * @exception NotJSONObject if text does not represent a JSON object
         * @exception JSONLimitViolation if source text exceeds established limits
         * @return JSONObjectRef representing legal JSON source
         */
        static JSONObjectRef parseObject ( const Limits & lim, const String & json );

        /**
         * @fn makeNull
         * @return a JSONValueRef representing JSON value keyword "null"
         */
        static JSONValueRef makeNull ();

        /**
         * @fn makeBoolean
         * @brief creates a value from C++ bool
         * @param val a Boolean initialization value
         * @return a JSONValueRef representing a JSON value keyword "true" or "false"
         */
        static JSONValueRef makeBoolean ( bool val );

        /**
         * @fn makeNumber
         * @brief creates a value from textual numeral
         * @param val a textual numeral initialization value
         * @return a JSONValueRef representing a numeric JSON value
         */
        static JSONValueRef makeNumber ( const String & val );

        /**
         * @fn makeInteger
         * @this is a specialization of a JSON number
         * @param val a long integer initialization value
         * @return a JSONValueRef representing an Integer JSON value
         * Use this method when starting with a C++ integer
         */
        static JSONValueRef makeInteger ( long long int val );

        /**
         * @fn makeDouble
         * @this is a specialization of a JSON number
         * @param val an IEEE-754 long double initialization value
         * @return a JSONValueRef representing a floating point JSON value
         * Use this method ONLY when starting with a C++ double
         * Its use is not recommended due to loss of information
         * when converting to textual numeral.
         */
        static JSONValueRef makeDouble ( long double val, unsigned int precision );

        /**
         * @fn makeString
         * @brief creates a value from std:: string
         * @param val a textual initialization value
         * @return a JSONValueRef representing a JSON string value
         */
        static JSONValueRef makeString ( const String & val );

        /**
         * @fn makeArray
         * @brief make an empty array
         * @return JSONArrayRef
         */
        static JSONArrayRef makeArray ();

        /**
         * @fn makeObject
         * @brief make an empty object
         * @return JSONObjectRef
         */
        static JSONObjectRef makeObject ();

    private:

        static void initLimits ();

        //!< internal parsing engine
        static JSONValueRef parse ( const Limits & lim, const String & json,
            String :: Iterator & curs, unsigned int depth );

        //!< type-specific value parsers
        static JSONValueRef parseNull ( String :: Iterator & curs );
        static JSONValueRef parseBoolean ( String :: Iterator & curs );
        static JSONValueRef parseNumber ( const Limits & lim,
            const String & json, String :: Iterator & curs );
        static JSONValueRef parseString ( const Limits & lim,
            const String & json, String :: Iterator & curs );

        //!< type-specific structure parsers
        static JSONArrayRef parseArray ( const Limits & lim, const String & json,
            String :: Iterator & curs, unsigned int depth );
        static JSONObjectRef parseObject ( const Limits & lim, const String & json,
            String :: Iterator & curs, unsigned int depth );

        //!< accept pre-parsed string input
        static JSONValueRef makeParsedNumber ( const String & val );
        static JSONValueRef makeParsedString ( const String & val );
    
        JSON ();    

        static Limits default_limits;
        
        // for testing
        static JSONValueRef test_parse ( const String & json, bool consume_all );
        friend class JSONFixture_WhiteBox;
    };


    /*=====================================================*
     *                      JSONValue                      *
     *=====================================================*/
    
    /**
     * @class JSONValue
     * @brief an abstraction representing a value within a JSON entity
     * actual types are { null, Boolean, Integer, numeric-string, string, array, object }
     * predicate messages are defined to help determine the actual type
     */
    class JSONValue
    {
    public:

        /*=================================================*
         *                TYPE PREDICATES                  *
         *=================================================*/

        /**
         * @fn isNull
         * @return Boolean true if value is 'null'
         */
        virtual bool isNull () const;

        /**
         * @fn isBoolean
         * @return Boolean true if value is 'true' or 'false'
         */
        virtual bool isBoolean () const;

        /**
         * @fn isNumber
         * @return Boolean true if value is a number according to JSON spec
         */
        virtual bool isNumber () const;

        /**
         * @fn isInteger
         * @return Boolean true if value is specifically an Integer
         * an Integer is an integral number that can be represented
         * in a C++ long long int type.
         */
        virtual bool isInteger () const;

        /**
         * @fn isString
         * @return Boolean true if value is textual according to JSON spec
         */
        virtual bool isString () const;

        /**
         * @fn isArray
         * @return Boolean true if the value is an array
         * an array may contain other values
         */
        virtual bool isArray () const;

        /**
         * @fn isObject
         * @return Boolean true if the value is an object
         * an object may contain other values
         */
        virtual bool isObject () const;



        /*=================================================*
         *                 VALUE SETTERS                   *
         *=================================================*/

        /**
         * @fn setNull
         * @brief change type if necessary to null type with value 'null'
         * @return self reference as a convenience to aid in C++ expressions
         */
        virtual JSONValue & setNull ();

        /**
         * @fn setBoolean
         * @brief change type if necessary to Boolean and set value
         * @param val C++ bool true or false
         * @return self reference as a convenience to aid in C++ expressions
         */
        virtual JSONValue & setBoolean ( bool val );

        /**
         * @fn setNumber
         * @brief change type if necessary to Number and set value
         * @param val String numeric value representation
         * @return self reference as a convenience to aid in C++ expressions
         */
        virtual JSONValue & setNumber ( const String & val );

        /**
         * @fn setInteger
         * @brief change type if necessary to Integer and set value
         * @param val C++ long long int
         * @return self reference as a convenience to aid in C++ expressions
         */
        virtual JSONValue & setInteger ( long long int val );

        /**
         * @fn setDouble
         * @brief change type if necessary to Number and set textual value
         * @param val C++ IEEE-754 long double to be converted to text
         * @return self reference as a convenience to aid in C++ expressions
         * It is recommended to use setNumber() instead due to loss of precision
         */
        virtual JSONValue & setDouble ( long double val, unsigned int precision );

        /**
         * @fn setString
         * @brief change type if necessary to String and set value
         * @param val String value representation
         * @return self reference as a convenience to aid in C++ expressions
         */
        virtual JSONValue & setString ( const String & val );



        /*=================================================*
         *                 VALUE GETTERS                   *
         *=================================================*/

        /**
         * @fn toBoolean
         * @exception JSONIncompatibleType if value is not Boolean
         * @return C++ bool value of true or false
         */
        virtual bool toBoolean () const;

        /**
         * @fn toNumber
         * @exception JSONIncompatibleType if value is not numeric
         * @return C++ String representing numeral
         */
        virtual String toNumber () const;

        /**
         * @fn toInteger
         * @exception JSONIncompatibleType if value is not representable as long long int
         * @return C++ long long int
         */
        virtual long long int toInteger () const;

        /**
         * @fn toString
         * @return C++ String with textual representation of value
         * differs from toJSON() in that actual string values will be
         * returned as is, in full UTF-8 UNICODE, with no escapes or quotes.
         */
        virtual String toString () const;

        /**
         * @fn toJSON
         * @return C++ String with JSON representation of value
         * differs from toString() in that actual string values will
         * be quoted and escaped.
         */
        virtual String toJSON () const = 0;

        /**
         * @fn readableJSON
         * @return C++ String with human-formatted JSON representation of value
         * differs from toJSON() in that spacing, indentation and line endings are inserted.
         */
        virtual String readableJSON ( unsigned int indent = 0 ) const;



        /*=================================================*
         *                   TYPE CASTS                    *
         *=================================================*/

        /**
         * @fn toArray
         * @overload attempts to downcast from JSONValue to JSONArray
         * @exception JSONBadCast if value is not an array
         * @return JSONArray reference
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        virtual JSONArray & toArray ();

        /**
         * @fn toArray
         * @overload attempts to downcast from const JSONValue to const JSONArray
         * @exception JSONBadCast if value is not an array
         * @return JSONArray reference
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        virtual const JSONArray & toArray () const;

        /**
         * @fn toObject
         * @overload attempts to downcast from JSONValue to JSONObject
         * @exception JSONBadCast if value is not an object
         * @return JSONObject reference
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        virtual JSONObject & toObject ();

        /**
         * @fn toObject
         * @overload attempts to downcast from const JSONValue to const JSONObject
         * @exception JSONBadCast if value is not an object
         * @return JSONObject reference
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        virtual const JSONObject & toObject () const;

        /**
         * @fn clone
         * @return creates a deep copy of value
         */
        virtual JSONValueRef clone () const;

        /**
         * @fn invalidate
         * @brief overwrite potentially sensitive contents in memory
         */
        virtual void invalidate () = 0;


        /*=================================================*
         *           C++ OPERATOR OVERLOADS                *
         *=================================================*/

        /**
         * fn operator=
         * @overload C++ only null assignment operator
         * @param val is nullptr
         * @return non-const JSONValue reference
         */
        JSONValue & operator = ( std :: nullptr_t val )
        { return setNull (); }

        /**
         * fn operator=
         * @overload C++ only Boolean assignment operator
         * @param val C++ bool true or false
         * @return non-const JSONValue reference
         */
        JSONValue & operator = ( bool val )
        { return setBoolean ( val ); }

        /**
         * fn operator=
         * @overload C++ only Integer assignment operator
         * @param val C++ long long int
         * @return non-const JSONValue reference
         */
        JSONValue & operator = ( long long int val )
        { return setInteger ( val ); }

        /**
         * fn operator=
         * @overload C++ only String assignment operator
         * @param val C++ String
         * @return non-const JSONValue reference
         */
        JSONValue & operator = ( const String & val )
        { return setString ( val ); }

        /**
         * @fn operator()
         * @overload C++ only cast to bool operator
         * @return bool value
         */
        operator bool () const
        { return toBoolean (); }

        /**
         * @fn operator()
         * @overload C++ only cast to integer operator
         * @return long long int value
         */
        operator long long int () const
        { return toInteger (); }

        /**
         * @fn operator()
         * @overload C++ only cast to string operator
         * @return String value
         */
        operator String () const
        { return toString (); }

        /**
         * @fn operator[]
         * @overload C++ only array operator to access non-const JSONValue at idx
         * @param idx signed array index
         * @exception JSONBadCast if value is not an array
         * @exception JSONIndexOutOfBounds if idx < 0 or >= count
         * @return non-const JSONValue reference
         */
        JSONValue & operator [] ( long int idx );

        /**
         * @fn operator[]
         * @overload C++ only array operator to access const JSONValue at idx
         * @param idx signed array index
         * @exception JSONBadCast if value is not an array
         * @exception JSONIndexOutOfBounds if idx < 0 or >= count
         * @return const JSONValue reference
         */
        const JSONValue & operator [] ( long int idx ) const;
        
        /**
         * @fn operator[]
         * @overload C++ only array operator to access non-const named JSONValue
         * @param name object member name
         * @exception JSONBadCast if value is not an object
         * @exception JSONNoSuchName if name is not a member of object
         * @return non-const JSONValue reference
         */
        JSONValue & operator [] ( const String & name );

        /**
         * @fn operator[]
         * @overload C++ only array operator to access const JSONValue at idx
         * @param name object member name
         * @exception JSONBadCast if value is not an object
         * @exception JSONNoSuchName if name is not a member of object
         * @return const JSONValue reference
         */
        const JSONValue & operator [] ( const String & name ) const;

        
        /**
         * @fn ~JSONValue
         * @brief disposes of dynmically allocated content in derived classes
         */
        virtual ~JSONValue () noexcept;

    protected:
        
        JSONValue () {}
    };
        

    /*=====================================================*
     *                      JSONArray                      *
     *=====================================================*/
    
    /**
     * @class JSONArray
     * @brief an array of zero or more JSONValues
     * extends JSONValue to define some behavior
     * and to provide access via container interface.
     */
    class JSONArray : public JSONValue
    {
    public:

        /*=================================================*
         *                   OVERRIDES                     *
         *=================================================*/

        // type predicate
        virtual bool isArray () const override
        { return true; }

        // getters
        virtual String toString () const override;
        virtual String toJSON () const override;
        virtual String readableJSON ( unsigned int indent = 0 ) const override;

        // casts
        virtual JSONArray & toArray () override
        { return * this; }
        virtual const JSONArray & toArray () const override
        { return * this; }

        // clone and invalidate
        virtual JSONValueRef clone () const override;
        virtual void invalidate () override;


        /*=================================================*
         *             CONTAINER INTERFACE                 *
         *=================================================*/

        /**
         * @fn isEmpty
         * @return Boolean true if array has no elements
         */
        bool isEmpty () const;

        /**
         * @fn count
         * @return Natural number with the number of contained elements
         */
        unsigned long int count () const;

        /**
         * @fn exists
         * @brief answers whether the indicated element exists
         * @param idx signed array index
         * @return Boolean true if element exists
         */
        bool exists ( long int idx ) const;

        /**
         * @fn appendValue
         * @brief add a new element to end of array
         * @param elem a JSONValueRef to value
         * @exception JSONNullValue if elem == nullptr
         */
        void appendValue ( const JSONValueRef & elem );

        /**
         * @fn setValue
         * @brief set entry to a new value
         * @param idx signed array index
         * @param elem a JSONValueRef to store within array
         * @exception JSONIndexOutOfBounds if idx < 0
         * @exception JSONNullValue if elem == nullptr
         * if idx >= count (), the array will be extended
         * and missing elements with index < idx will be
         * filled with null values.
         */
        void setValue ( long int idx, const JSONValueRef & elem );

        /**
         * @fn getValue
         * @overload returns non-const JSONValue at idx
         * @param idx signed array index
         * @exception JSONIndexOutOfBounds if idx < 0
         * @return non-const JSONValue reference
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        JSONValue & getValue ( long int idx );

        /**
         * @fn getValue
         * @overload returns const JSONValue at idx
         * @param idx signed array index
         * @exception JSONIndexOutOfBounds if idx < 0 or >= count
         * @return const JSONValue reference
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        const JSONValue & getValue ( long int idx ) const;

        /**
         * @fn removeValue
         * @brief remove and return an entry if valid
         * @param idx signed array index
         * @return JSONValueRef with removed value
         * replaces valid internal entries with null element
         * deletes trailing null elements making them undefined
         */
        JSONValueRef removeValue ( long int idx );

        /**
         * @fn deleteValue
         * @brief remove and return an entry if valid
         * @param idx signed array index
         * replaces valid internal entries with null element
         * deletes trailing null elements making them undefined
         */
        void deleteValue ( long int idx )
        { JSONValueRef removed = removeValue ( idx ); }

        /**
         * @fn cloneArray
         * @return creates a deep copy of array
         * avoids need to typecast a JSONValueRef
         */
        JSONArrayRef cloneArray () const;


        /*=================================================*
         *           C++ OPERATOR OVERLOADS                *
         *=================================================*/

        /**
         * @fn operator[]
         * @overload C++ only array operator to access non-const JSONValue at idx
         * @param idx signed array index
         * @exception JSONBadCast if value is not an array
         * @exception JSONIndexOutOfBounds if idx < 0 or >= count
         * @return non-const JSONValue reference
         */
        JSONValue & operator [] ( long int idx )
        { return getValue ( idx ); }

        /**
         * @fn operator[]
         * @overload C++ only array operator to access const JSONValue at idx
         * @param idx signed array index
         * @exception JSONBadCast if value is not an array
         * @exception JSONIndexOutOfBounds if idx < 0 or >= count
         * @return const JSONValue reference
         */
        const JSONValue & operator [] ( long int idx ) const
        { return getValue ( idx ); }

        /**
         * @fn ~JSONArray
         * @brief deletes any contents and destroys internal structures
         */        
        virtual ~JSONArray () noexcept override;
        
    private:
        
        // used to empty out the array before copy
        void clear ();
        void appendValueInt ( const JSONValueRef & elem );
        JSONValueRef removeValueInt ( long int idx );

        JSONArray & operator = ( const JSONArray & array );
        JSONArray & operator = ( const JSONArray && array );
        JSONArray ( const JSONArray & a );
        JSONArray ( const JSONArray && a );
        JSONArray ();

        std :: vector < JSONValue * > array;
        BusyLock busy;
        
        friend class JSON;
    };
    

    /*=====================================================*
     *                     JSONObject                      *
     *=====================================================*/
    
    /**
     * @class JSONObject
     * @brief a map of zero or more ( name, value ) pairs
     * extends JSONValue to define some behavior
     * and to provide access via container interface.
     */
    class JSONObject : public JSONValue
    {
    public:
        
        /*=================================================*
         *                   OVERRIDES                     *
         *=================================================*/

        // type predicate
        virtual bool isObject () const override
        { return true; }

        // getters
        virtual String toString () const override;
        virtual String toJSON () const override;
        virtual String readableJSON ( unsigned int indent = 0 ) const override;

        // casts
        virtual JSONObject & toObject () override
        { return * this; }
        virtual const JSONObject & toObject () const override
        { return * this; }

        // clone and invalidate
        virtual JSONValueRef clone () const override;
        virtual void invalidate () override;


        /*=================================================*
         *             CONTAINER INTERFACE                 *
         *=================================================*/

        /**
         * @fn isEmpty
         * @return Boolean true if object has no ( name, value ) pairs
         */
        bool isEmpty () const;

        /**
         * @fn count
         * @return Natural number with the number of member pairs
         */
        unsigned long int count () const;

        /**
         * @fn exists
         * @brief answers whether the indicated mapping exists
         * @param name String with the member name
         * @return Boolean true if pair exists
         */
        bool exists ( const String & name ) const;
        
        /**
         * @fn getNames
         * @return std::vector<String> of member names
         */
        std :: vector < String > getNames () const;

        /**
         * @fn addValue
         * @brief add a new ( name, value ) pair
         * @param name String with unique member name
         * @param val a JSONValueRef
         * @exception JSONUniqueConstraintViolation if name exists
         * @exception JSONNullValue if val == nullptr
         */
        void addValue ( const String & name, const JSONValueRef & val );
        
        /**
         * @fn setValue
         * @brief set value of an existing pair or add a new pair
         * @param name String with unique member name
         * @param val a JSONValueRef
         * @exception JSONPermViolation if member exists and is final
         * @exception JSONNullValue if val == nullptr
         */
        void setValue ( const String & name, const JSONValueRef & val );

        /**
         * @fn getValue
         * @overload non-const accessor
         * @param name String with the member name
         * @exception JSONNoSuchName if name is not a member of object
         * @return JSONValue reference to existing value
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        JSONValue & getValue ( const String & name );

        /**
         * @fn getValue
         * @overload const accessor
         * @param name String with the member name
         * @exception JSONNoSuchName if name is not a member of object
         * @return const JSONValue reference to existing value
         * a C++ reference is returned rather than a pointer in order
         * to facilitate certain idiomatic C++ expressions, in addition
         * to the fact that success can never return nullptr.
         */
        const JSONValue & getValue ( const String & name ) const;

        /**
         * @fn removeValue
         * @brief remove and return named value
         * @param name std:: string with member name
         * @return JSONValueRef with removed value
         * returns reference to nullptr if member is not found
         */
        JSONValueRef removeValue ( const String & name );

        /**
         * @fn deleteValue
         * @brief remove and delete named value
         * @param name std:: string with member name
         * ignored if member is not found
         */
        void deleteValue ( const String & name )
        { JSONValueRef removed = removeValue ( name ); }

        /**
         * @fn cloneObject
         * @return creates a deep copy of object
         * avoids need to typecast a JSONValueRef
         */
        JSONObjectRef cloneObject () const;



        /*=================================================*
         *           C++ OPERATOR OVERLOADS                *
         *=================================================*/

        /**
         * @fn operator[]
         * @overload C++ only array operator to access non-const named JSONValue
         * @param name object member name
         * @exception JSONBadCast if value is not an object
         * @exception JSONNoSuchName if name is not a member of object
         * @return non-const JSONValue reference
         */
        JSONValue & operator [] ( const String & name )
        { return getValue ( name ); }

        /**
         * @fn operator[]
         * @overload C++ only array operator to access const JSONValue at idx
         * @param name object member name
         * @exception JSONBadCast if value is not an object
         * @exception JSONNoSuchName if name is not a member of object
         * @return const JSONValue reference
         */
        const JSONValue & operator [] ( const String & name ) const
        { return getValue ( name ); }

        /**
         * @fn ~JSONObject
         * @brief deletes any contents and destroys internal structures
         */        
        virtual ~JSONObject () noexcept override;

        
    private:

        void clear ();
        void setValueInt ( const String & name, const JSONValueRef & val );

        JSONObject & operator = ( const JSONObject & obj );
        JSONObject & operator = ( const JSONObject && obj );
        JSONObject ( const JSONObject & obj );
        JSONObject ( const JSONObject && obj );
        JSONObject ();

        std :: map < String, JSONValue * > members;
        BusyLock busy;
        
        friend class JSON;
    };


    /*=====================================================*
     *                   C++ INLINES                       *
     *=====================================================*/

    inline
    JSONValue & JSONValue :: operator [] ( long int idx )
    {
        return toArray () . getValue ( idx );
    }

    inline
    const JSONValue & JSONValue :: operator [] ( long int idx ) const
    {
        return toArray () . getValue ( idx );
    }

    inline
    JSONValue & JSONValue :: operator [] ( const String & name )
    {
        return toObject () . getValue ( name );
    }

    inline
    const JSONValue & JSONValue :: operator [] ( const String & name ) const
    {
        return toObject () . getValue ( name );
    }


    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/

    DECLARE_SEC_MSG_EXCEPTION ( MalformedJSON, InvalidArgument );
    DECLARE_SEC_MSG_EXCEPTION ( NotJSONObject, InvalidArgument );
    DECLARE_SEC_MSG_EXCEPTION ( JSONLimitViolation, BoundsException );
    DECLARE_SEC_MSG_EXCEPTION ( JSONIncompatibleType, IncompatibleTypeException );
    DECLARE_SEC_MSG_EXCEPTION ( JSONBadCast, BadCastException );
    DECLARE_SEC_MSG_EXCEPTION ( JSONNullValue, NullArgumentException );
    DECLARE_SEC_MSG_EXCEPTION ( JSONIndexOutOfBounds, BoundsException );
    DECLARE_SEC_MSG_EXCEPTION ( JSONUniqueConstraintViolation, UniqueConstraintViolation );
    DECLARE_SEC_MSG_EXCEPTION ( JSONPermViolation, PermissionViolation );
    DECLARE_SEC_MSG_EXCEPTION ( JSONNoSuchName, NotFoundException );
    DECLARE_SEC_MSG_EXCEPTION ( JSONInternalError, InternalError );

}
