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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#include <ncbi/secure/payload.hpp>
#include "memset-priv.hpp"

#include <cstdint>
#include <cstring>
#include <cassert>
#include <string>

namespace ncbi
{
    // Payload
    unsigned char * Payload :: data ()
    {
        SLocker lock ( busy );
        return buff;
    }

    const unsigned char * Payload :: data () const
    {
        SLocker lock ( busy );
        return buff;
    }

    size_t Payload :: size () const
    {
        SLocker lock ( busy );
        return sz;
    }

    size_t Payload :: capacity () const
    {
        SLocker lock ( busy );
        return cap;
    }

    void Payload :: setSize ( size_t amt )
    {
        XLocker lock ( busy );

        // if setting size to zero and buffer is null, ignore
        if ( amt != 0 || buff != nullptr )
        {
            // actual allocated capacity is cap + 1
            if ( amt > cap || buff == nullptr )
            {
                throw SizeViolation (
                    XP ( XLOC )
                    << "payload size of "
                    << amt
                    << " bytes exceeds capacity of "
                    << cap
                    );
            }

            sz = amt;
        }
    }
    
    void Payload :: increaseCapacity ( size_t amt )
    {
        XLocker lock ( busy );

        // allocate a new buffer with additional capacity + 1
        unsigned char * new_buff = new unsigned char [ cap + amt + 1 ];

        // zero out new memory
        memset ( & new_buff [ cap ], 0, amt + 1 );

        // copy from old
        memmove ( new_buff, buff, sz );

        // douse the old one
        delete [] buff;

        // update contents
        cap += amt;
        buff = new_buff;
    }

    void Payload :: wipe ()
    {
        XLocker lock ( busy );

        if ( buff != nullptr )
        {
            // would prefer to use memset_s, but
            // as of the date of writing there is not
            // universal support for it on all build hosts
            memset_while_respecting_language_semantics
                ( buff, cap, 0, cap, ( const char * ) buff );
        }
    }

    void Payload :: reinitialize ( bool _wipe )
    {
        XLocker lock ( busy );

        if ( buff != nullptr )
        {
            if ( _wipe )
                wipe ();

            // douse the buffer
            delete [] buff;
            buff = nullptr;
        }

        sz = cap = 0;
    }

    Payload & Payload :: operator = ( const Payload & payload )
    {
        XLocker lock1 ( busy );
        XLocker lock2 ( payload . busy );

        delete [] buff;

        buff = payload . buff;
        sz = payload . sz;
        cap = payload . cap;
        
        payload . buff = nullptr;
        payload . sz = payload . cap = 0;

        return * this;
    }

    Payload & Payload :: operator = ( const Payload && payload )
    {
        XLocker lock1 ( busy );
        XLocker lock2 ( payload . busy );

        delete [] buff;

        buff = payload . buff;
        sz = payload . sz;
        cap = payload . cap;
        
        payload . buff = nullptr;
        payload . sz = payload . cap = 0;

        return * this;
    }

    Payload :: Payload ( const Payload & payload )
        : buff ( nullptr )
        , sz ( payload . sz )
        , cap ( payload . cap )
    {
        XLocker lock ( payload . busy );
        buff = payload . buff;
        payload . buff = nullptr;
        payload . sz = payload . cap = 0;
    }

    Payload :: Payload ( const Payload && payload )
        : buff ( nullptr )
        , sz ( payload . sz )
        , cap ( payload . cap )
    {
        XLocker lock ( payload . busy );
        buff = payload . buff;
        payload . buff = nullptr;
        payload . sz = payload . cap = 0;
    }

    Payload :: Payload () noexcept
        : buff ( nullptr )
        , sz ( 0 )
        , cap ( 0 )
    {
    }

    Payload :: Payload ( size_t initial_capacity )
        : buff ( nullptr )
        , sz ( 0 )
        , cap ( initial_capacity )
    {
        buff = new unsigned char [ cap + 1 ];
    }

    Payload :: ~ Payload () noexcept
    {
        wipe ();
    }
    
}
