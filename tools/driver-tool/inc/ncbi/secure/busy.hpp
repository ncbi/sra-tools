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

#include <atomic>

/**
 * @file ncbi/secure/busy.hpp
 * @brief simple busy-lock for mutable objects
 */

namespace ncbi
{

    /*=====================================================*
     *                      BusyLock                       *
     *=====================================================*/
    
    /**
     * @class BusyLock
     * @brief an atomic element to maintain busy-state
     */
    class BusyLock
    {
    public:

        BusyLock ();
        ~ BusyLock ();

    private:

        BusyLock & acquireShared () const;
        BusyLock & acquireExclusive () const;
        void release ();

        // this will probably be changed for r/w locking
        mutable std :: atomic_flag busy;

        friend class SLocker;
        friend class XLocker;
    };


    /*=====================================================*
     *                       SLocker                       *
     *=====================================================*/
    
    /**
     * @class SLocker
     * @brief acquires and releases shared lock
     */
    class SLocker
    {
    public:

        SLocker ( const BusyLock & lock )
            : busy ( lock . acquireShared () ) {}
        ~ SLocker () { busy . release (); }

    private:

        BusyLock & busy;
    };


    /*=====================================================*
     *                       XLocker                       *
     *=====================================================*/
    
    /**
     * @class XLocker
     * @brief acquires and releases exclusive lock
     */
    class XLocker
    {
    public:

        XLocker ( const BusyLock & lock )
            : busy ( lock . acquireExclusive () ) {}
        ~ XLocker () { busy . release (); }

    private:

        BusyLock & busy;
    };


    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/

    DECLARE_SEC_MSG_EXCEPTION ( ObjectBusyException, InternalError );
}
