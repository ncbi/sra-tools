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

#ifndef _hpp_ngs_test_engine_statisticsitf_
#define _hpp_ngs_test_engine_statisticsitf_

#include <ngs/adapter/ErrorMsg.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/StatisticsItf.hpp>

namespace ngs_test_engine
{

    /*----------------------------------------------------------------------
     * forwards
     */

    /*----------------------------------------------------------------------
     * StatisticsItf
     */
    class StatisticsItf : public ngs_adapt::StatisticsItf
    {
    public:

        virtual uint32_t getValueType ( const char * path ) const 
        { 
            return 3; 
        }

        virtual ngs_adapt::StringItf * getAsString ( const char * path ) const 
        { 
            static std::string val = "string";
            return new ngs_adapt::StringItf( val.c_str(), val.size() );         
        }

        virtual int64_t getAsI64 ( const char * path ) const
        { 
            return -14; 
        }
        virtual uint64_t getAsU64 ( const char * path ) const 
        { 
            return 144; 
        }
        virtual double getAsDouble ( const char * path ) const
        { 
            return 3.14; 
        }
        virtual ngs_adapt::StringItf * nextPath ( const char * path ) const
        { 
            static std::string val = "nextpath";
            return new ngs_adapt::StringItf( val.c_str(), val.size() );         
        }

	public:
		StatisticsItf () 
        { 
        }
		
        ~StatisticsItf () 
        { 
            --instanceCount;
        }

        static   unsigned int instanceCount;
    };

} // namespace ngs_test_engine

#endif // _hpp_ngs_test_engine_statisticsitf_
