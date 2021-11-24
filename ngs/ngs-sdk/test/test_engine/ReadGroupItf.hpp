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

#ifndef _hpp_ngs_test_engine_read_groupitf_
#define _hpp_ngs_test_engine_read_groupitf_

#include <ngs/adapter/ErrorMsg.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ReadGroupItf.hpp>

#include "StatisticsItf.hpp"

namespace ngs_test_engine
{

    /*----------------------------------------------------------------------
     * forwards
     */

    /*----------------------------------------------------------------------
     * ReadGroupItf
     */
    class ReadGroupItf : public ngs_adapt::ReadGroupItf
    {
    public:

		virtual ngs_adapt::StringItf * getName () const 
        { 
            static std::string name = "readgroup";
            return new ngs_adapt::StringItf ( name.c_str(), name.size() ); 
        }

        virtual ngs_adapt::StatisticsItf * getStatistics () const
        {
            return new ngs_test_engine::StatisticsItf();
        }

        virtual bool nextReadGroup () 
        {
            switch ( iterateFor )
            {
            case -1:    throw ngs_adapt::ErrorMsg ( "invalid iterator access" );
            case 0:     return false;
            default:    --iterateFor; return true;
            }
        }

	public:
		ReadGroupItf () 
        : iterateFor(-1)
        { 
            ++instanceCount;
        }
		ReadGroupItf ( unsigned int p_iterateFor ) 
        : iterateFor( ( int ) p_iterateFor )
        { 
            ++instanceCount;
        }
		
        ~ReadGroupItf () 
        { 
            --instanceCount;
        }

        static   unsigned int instanceCount;

        int iterateFor;
    };

} // namespace ngs_test_engine

#endif // _hpp_ngs_test_engine_read_groupitf_
