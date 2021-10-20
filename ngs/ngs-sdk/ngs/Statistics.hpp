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
* ===========================================================================
*
*/

#ifndef _hpp_ngs_statistics_
#define _hpp_ngs_statistics_

#ifndef _hpp_ngs_error_msg_
#include <ngs/ErrorMsg.hpp>
#endif

#ifndef _hpp_ngs_stringref_
#include <ngs/StringRef.hpp>
#endif

#include <stdint.h>

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards and typedefs
     */
    typedef class StatisticsItf * StatisticsRef;


    /*======================================================================
     * Statistics
     *  represents a set of statistics as a collection of path/value pairs
     */
    class Statistics
    {
    public:

        enum ValueType
        {
            none,
            string,
            int64,
            uint64,
            real
        };

        /* getValueType
         */
        ValueType getValueType ( const String & path ) const
            NGS_NOTHROW ();

        /* getAsString
         */
        String getAsString ( const String & path ) const
            NGS_THROWS ( ErrorMsg );

        /* other int types ? */

        /* getAsI64
         *  returns a signed 64-bit integer
         */
        int64_t getAsI64 ( const String & path ) const
            NGS_THROWS ( ErrorMsg );

        /* getAsU64
         *  returns an unsigned 64-bit integer
         */
        uint64_t getAsU64 ( const String & path ) const
            NGS_THROWS ( ErrorMsg );

        /* getAsDouble
         *  returns a 64-bit floating point
         */
        double getAsDouble ( const String & path ) const
            NGS_THROWS ( ErrorMsg );

        /* nextPath
         *  advance to next path in container
         *
         * param path is NULL or empty to request first path, or a valid path string
         * returns an empty string if no more paths, or the next valid path string
         */
        String nextPath ( const String & path ) const
            NGS_NOTHROW ();

    public:

        // C++ support

        Statistics ( StatisticsRef ref )
            NGS_NOTHROW ();

        Statistics & operator = ( const Statistics & obj )
            NGS_THROWS ( ErrorMsg );
        Statistics ( const Statistics & obj )
            NGS_THROWS ( ErrorMsg );

        ~ Statistics ()
            NGS_NOTHROW ();

    private:
        Statistics & operator = ( StatisticsRef ref )
            NGS_NOTHROW ();

    protected:

        StatisticsRef self;
    };

} // namespace ngs


#ifndef _inl_ngs_statistics_
#include <ngs/inl/Statistics.hpp>
#endif

#endif // _hpp_ngs_statistics_
