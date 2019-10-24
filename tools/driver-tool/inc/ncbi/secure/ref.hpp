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

#include <ncbi/secure/except.hpp>

#include <memory>

/**
 * @file ncbi/secure/ref.hpp
 * @brief C++ reference-objects
 *
 * this type of structure is commonly known as a "smart-pointer"
 * and C++11 has introduced a number of versions while deprecating
 * the "auto_ptr".
 */

namespace ncbi
{

    /*=====================================================*
     *                        XRef                         *
     *=====================================================*/
    
    /**
     * @class XRef
     * @brief the hot-potato reference
     *
     * ensures that only a single reference holds the object pointer,
     * while allowing for that pointer to be passed freely between
     * references.
     *
     * the point of such a reference is exactly for tracking an allocation
     * through a message chain, and ensuring that an exception at any
     * point will cause the object to be collected, while at the same
     * time allowing for efficient transfer of the pointer.
     */
    template < class T >
    class XRef
    {
    public:

        T * release () const noexcept
        { return p . release (); }

        void reset ( T * ptr ) const noexcept
        { p . reset ( ptr ); }

        bool operator ! () const noexcept
        { return ! p; }

        T * operator -> () const noexcept
        { return p . get (); }

        T & operator * () const noexcept
        { return * p . get (); }

        bool operator == ( const XRef < T > & r ) const noexcept
        { return p == r . p; }

        bool operator == ( std :: nullptr_t ) const noexcept
        { return p == nullptr; }

        bool operator == ( T * ptr ) const noexcept
        { return p == ptr; }

        bool operator != ( const XRef < T > & r ) const noexcept
        { return p != r . p; }

        bool operator != ( std :: nullptr_t ) const noexcept
        { return p != nullptr; }

        bool operator != ( T * ptr ) const noexcept
        { return p != ptr; }

        XRef & operator = ( T * ptr ) noexcept
        {
            p . reset ( ptr );
            return * this;
        }

        XRef & operator = ( const XRef < T > & r ) noexcept
        {
            p . reset ( r . p . release () );
            return * this;
        }

        XRef ( const XRef & r ) noexcept
            : p ( r . p . release () )
        {
        }

        XRef ( T * ptr ) noexcept
            : p ( ptr )
        {
        }

        XRef () noexcept {}
        ~ XRef () noexcept {}

    private:

        mutable std :: unique_ptr < T > p;
    };


    /*=====================================================*
     *                        SRef                         *
     *=====================================================*/
    
    /**
     * @class SRef
     * @brief a shared, counted reference
     *
     * for our purposes, this allows for co-equal shared ownership
     * that is useful for immutable objects like JWKs.
     */
    template < class T >
    class SRef
    {
    public:

        T * release () const noexcept
        { return p . release (); }

        void reset ( T * ptr ) const noexcept
        { p . reset ( ptr ); }

        bool unique () const noexcept
        { return p . unique (); }

        long int use_count () const noexcept
        { return p . use_count (); }

        bool operator ! () const noexcept
        { return ! p; }

        T * operator -> () const noexcept
        { return p . get (); }

        T & operator * () const noexcept
        { return * p . get (); }

        bool operator == ( const SRef < T > & r ) const noexcept
        { return p == r . p; }

        bool operator == ( std :: nullptr_t ) const noexcept
        { return p == nullptr; }

        bool operator == ( T * ptr ) const noexcept
        { return p == ptr; }

        bool operator != ( const SRef < T > & r ) const noexcept
        { return p != r . p; }

        bool operator != ( std :: nullptr_t ) const noexcept
        { return p != nullptr; }

        bool operator != ( T * ptr ) const noexcept
        { return p != ptr; }

        SRef & operator = ( T * ptr ) noexcept
        {
            p . reset ( ptr );
            return * this;
        }

        SRef & operator = ( const SRef < T > & r ) noexcept
        {
            p = r . p;
            return * this;
        }

        SRef ( const SRef & r ) noexcept
            : p ( r . p )
        {
        }

        SRef ( T * ptr ) noexcept
            : p ( ptr )
        {
        }

        SRef () noexcept {}
        ~ SRef () noexcept {}

    private:

        mutable std :: shared_ptr < T > p;
    };


    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/

}
