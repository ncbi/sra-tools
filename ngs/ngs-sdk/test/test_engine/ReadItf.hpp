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

#ifndef _hpp_ngs_test_engine_readitf_
#define _hpp_ngs_test_engine_readitf_

#include <ngs/adapter/ErrorMsg.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ReadItf.hpp>

namespace ngs_test_engine
{

    /*----------------------------------------------------------------------
     * forwards
     */

    /*----------------------------------------------------------------------
     * ReadItf
     */
    class ReadItf : public ngs_adapt::ReadItf
    {
    public:

        virtual ngs_adapt::StringItf * getFragmentId () const 
        {
            static std::string fragId = "readFragId";
            return new ngs_adapt::StringItf( fragId.c_str(), fragId.size() ); 
        }

        virtual ngs_adapt::StringItf * getFragmentBases ( uint64_t offset, uint64_t length ) const 
        {
            static std::string fragBases = "AGCT";
            static std::string substr;
            substr = fragBases.substr( ( size_t ) offset, length == (uint64_t)-1 ? std::string::npos : ( size_t ) length );
            return new ngs_adapt::StringItf( substr.c_str(), substr.size() ); 
        }

        virtual ngs_adapt::StringItf * getFragmentQualities ( uint64_t offset, uint64_t length ) const 
        {
            static std::string fragQuals = "bbd^";
            static std::string substr;
            substr = fragQuals.substr( ( size_t ) offset, length == (uint64_t)-1 ? std::string::npos : ( size_t ) length );
            return new ngs_adapt::StringItf( substr.c_str(), substr.size() ); 
        }

        virtual bool nextFragment () 
        {
            switch ( fragmentsIterateFor )
            {
            case -1:    throw ngs_adapt::ErrorMsg ( "invalid iterator access" );
            case 0:     return false;
            default:    --fragmentsIterateFor; return true;
            }
        }

        virtual ngs_adapt::StringItf * getReadId () const 
        {
            static std::string readId = "readId";
            return new ngs_adapt::StringItf( readId.c_str(), readId.size() ); 
        }

        virtual uint32_t getNumFragments () const 
        { 
            return 2; 
        }

        virtual uint32_t getReadCategory () const 
        { 
            return ngs::Read::partiallyAligned; 
        }

        virtual ngs_adapt::StringItf * getReadGroup () const 
        {
            static std::string rg = "readGroup";
            return new ngs_adapt::StringItf( rg.c_str(), rg.size() ); 
        }

        virtual ngs_adapt::StringItf * getReadName () const
        {
            static std::string name = "readName";
            return new ngs_adapt::StringItf( name.c_str(), name.size() ); 
        }

        virtual ngs_adapt::StringItf * getReadBases ( uint64_t offset, uint64_t length ) const
        {
            static std::string readBases = "TGCA";
            static std::string substr;
            substr = readBases.substr( ( size_t ) offset, length == (uint64_t)-1 ? std::string::npos : ( size_t ) length );
            return new ngs_adapt::StringItf( substr.c_str(), substr.size() ); 
        }

        virtual ngs_adapt::StringItf * getReadQualities ( uint64_t offset, uint64_t length ) const
        {
            static std::string readQuals = "qrst";
            static std::string substr;
            substr = readQuals.substr( ( size_t ) offset, length == (uint64_t)-1 ? std::string::npos : ( size_t ) length );
            return new ngs_adapt::StringItf( substr.c_str(), substr.size() ); 
        }

        virtual bool nextRead () 
        {
            switch ( iterateFor )
            {
            case -1:    throw ngs_adapt::ErrorMsg ( "invalid iterator access" );
            case 0:     return false;
            default:    --iterateFor; return true;
            }
        }

	public:
		ReadItf () 
        :   iterateFor(-1),
            fragmentsIterateFor(2)
        { 
            ++instanceCount;
        }
		ReadItf ( const char* p_id ) 
        :   id(p_id), 
            iterateFor(-1),
            fragmentsIterateFor(2)
        { 
            ++instanceCount;
        }
		ReadItf ( unsigned int p_iterateFor ) 
        :   iterateFor( ( int ) p_iterateFor ),
            fragmentsIterateFor(2)
        { 
            ++instanceCount;
        }
		
        ~ReadItf () 
        { 
            --instanceCount;
        }

        static   unsigned int instanceCount;

        std::string id;
        int iterateFor;
        int fragmentsIterateFor;
    };

} // namespace ngs_test_engine

#endif // _hpp_ngs_test_engine_alignmentitf_
