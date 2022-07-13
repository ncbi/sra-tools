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

#include <ncbi/secure/busy.hpp>
#include <ncbi/secure/except.hpp>


/**
 * @file ncbi/secure/payload.hpp
 * @brief a payload abstraction
 */

namespace ncbi
{

    /*=====================================================*
     *                       Payload                       *
     *=====================================================*/
    
    /**
     * @class Payload
     * @brief contains binary data in a managed package
     */
    class Payload
    {
    public:

        /**
         * data
         * @overload non-const version
         * @return non-const pointer to buffer
         */
        unsigned char * data ();

        /**
         * data
         * @overload const version
         * @return const pointer to buffer
         */
        const unsigned char * data () const;

        /**
         * size
         * @return size of data in bytes
         */
        size_t size () const;

        /**
         * capacity
         * @return size of buffer in bytes
         */
        size_t capacity () const;

        /**
         * setSize
         * @brief record actual size
         * @param amt actual size in bytes
         */
        void setSize ( size_t amt );

        /**
         * increaseCapacity
         * @brief increase buffer size
         * @param amt amount of size increment
         */
        void increaseCapacity ( size_t amt = 256 );

        /**
         * wipe
         * @brief overwrites buffer space
         */
        void wipe ();

        /**
         * reinitialize
         * @brief deletes any buffer space and resets to initial state
         * @param wipe if true, overwrite buffer space before freeing
         */
        void reinitialize ( bool wipe = true );

        /**
         * operator =
         * @brief copy operator
         * @param payload source from which contents are STOLEN
         * @return C++ self-reference for use in idiomatic C++ expressions
         *
         * Source object will be empty afterward.
         */
        Payload & operator = ( const Payload & payload );

        /**
         * operator =
         * @brief move operator
         * @param payload source from which contents are STOLEN
         * @return C++ self-reference for use in idiomatic C++ expressions
         *
         * Source object will be empty afterward.
         */
        Payload & operator = ( const Payload && payload );

        /**
         * Payload
         * @overload copy constructor
         * @param payload source from which contents are STOLEN
         *
         * Source object will be empty afterward
         */
        Payload ( const Payload & payload );

        /**
         * Payload
         * @overload move constructor
         * @param payload source from which contents are STOLEN
         *
         * Source object will be empty afterward
         */
        Payload ( const Payload && payload );

        /**
         * Payload
         * @overload create zero size payload with initial buffer capacity
         * @param initial_capacity value in bytes of initial buffer size
         */
        explicit Payload ( size_t initial_capacity );

        /**
         * Payload
         * @overload default constructor
         */
        Payload () noexcept;

        /**
         * ~Payload
         * @brief delete buffer if present
         */
        ~ Payload () noexcept;

    private:

        BusyLock busy;
        mutable unsigned char * buff;
        mutable size_t sz, cap;
    };


    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/

}
