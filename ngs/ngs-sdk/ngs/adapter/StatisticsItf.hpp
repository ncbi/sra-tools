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

#ifndef _hpp_ngs_adapt_statisticsitf_
#define _hpp_ngs_adapt_statisticsitf_

#ifndef _hpp_ngs_adapt_refcount_
#include <ngs/adapter/Refcount.hpp>
#endif

#ifndef _h_ngs_itf_statisticsitf_
#include <ngs/itf/StatisticsItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * StatisticsItf
     */
    class StatisticsItf : public Refcount < StatisticsItf, NGS_Statistics_v1 >
    {
    public:

        virtual uint32_t getValueType ( const char * path ) const = 0;
        virtual StringItf * getAsString ( const char * path ) const = 0;
        virtual int64_t getAsI64 ( const char * path ) const = 0;
        virtual uint64_t getAsU64 ( const char * path ) const = 0;
        virtual double getAsDouble ( const char * path ) const = 0;
        virtual StringItf * nextPath ( const char * path ) const = 0;

    protected:

        StatisticsItf ();
        static NGS_Statistics_v1_vt ivt;

    private:

        static uint32_t CC get_type ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path );
        static NGS_String_v1 * CC as_string ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path );
        static int64_t CC as_I64 ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path );
        static uint64_t CC as_U64 ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path );
        static double CC as_F64 ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path );
        static NGS_String_v1 * CC next_path ( const NGS_Statistics_v1 * self, NGS_ErrBlock_v1 * err, const char * path );

    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_statisticsitf_
