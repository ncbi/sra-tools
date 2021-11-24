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

#ifndef _inl_ngs_statistics_
#define _inl_ngs_statistics_

#ifndef _hpp_ngs_statistics_
#include <ngs/Statistics.hpp>
#endif

#ifndef _hpp_ngs_itf_statisticsitf_
#include <ngs/itf/StatisticsItf.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * Statistics
     */

    inline
    Statistics :: ValueType Statistics :: getValueType ( const String & path ) const
        NGS_NOTHROW ()
    { return ( Statistics :: ValueType ) self -> getValueType ( path . c_str () ); }

    inline
    String Statistics :: getAsString ( const String & path ) const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getAsString ( path . c_str () ) ) . toString (); }

    inline
    int64_t Statistics :: getAsI64 ( const String & path ) const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAsI64 ( path . c_str () ); }

    inline
    uint64_t Statistics :: getAsU64 ( const String & path ) const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAsU64 ( path . c_str () ); }

    inline
    double Statistics :: getAsDouble ( const String & path ) const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAsDouble ( path . c_str () ); }

    inline
    String Statistics :: nextPath ( const String & path ) const
        NGS_NOTHROW ()
    { return StringRef ( self -> nextPath ( path . c_str () ) ) . toString (); }

} // namespace ngs

#endif // _inl_ngs_statistics_
