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

#ifndef _hpp_ngs_test_engine_referenceitf_
#define _hpp_ngs_test_engine_referenceitf_

#include <ngs/adapter/ErrorMsg.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ReferenceItf.hpp>

#include "AlignmentItf.hpp"
#include "PileupItf.hpp"

namespace ngs_test_engine
{

    /*----------------------------------------------------------------------
     * forwards
     */

    /*----------------------------------------------------------------------
     * ReferenceItf
     */
    class ReferenceItf : public ngs_adapt :: ReferenceItf
    {
    public:

		virtual ngs_adapt :: StringItf * getCommonName () const
        {
            static std::string name = "common name";
            return new ngs_adapt :: StringItf ( name.c_str(), name.size() );
        }

		virtual ngs_adapt :: StringItf * getCanonicalName () const
        {
            static std::string name = "canonical name";
            return new ngs_adapt :: StringItf ( name.c_str(), name.size() );
        }

        virtual bool getIsCircular () const
        {
            return false;
        }

        virtual bool getIsLocal () const
        {
            return true;
        }

        virtual uint64_t getLength () const
        {
            return 101;
        }

        virtual ngs_adapt :: StringItf * getReferenceBases ( uint64_t offset, uint64_t length ) const
        {
            static std::string bases= "CAGT";
            return new ngs_adapt :: StringItf ( bases.c_str(), bases.size() );
        }

        virtual ngs_adapt :: StringItf * getReferenceChunk ( uint64_t offset, uint64_t length ) const
        {
            static std::string bases= "AG";
            return new ngs_adapt :: StringItf ( bases.c_str(), bases.size() );
        }

        virtual uint64_t getAlignmentCount ( bool wants_primary, bool wants_secondary ) const
        {
            return ( 13 * wants_primary ) + ( 6 * wants_secondary );
        }

        virtual ngs_adapt :: AlignmentItf * getAlignment ( const char * alignmentId ) const
        {
            return new ngs_test_engine::AlignmentItf ( alignmentId );
        }

        virtual ngs_adapt :: AlignmentItf * getAlignments ( bool wants_primary, bool wants_secondary ) const
        {
            return new ngs_test_engine::AlignmentItf ( 5 );
        }

        virtual ngs_adapt :: AlignmentItf * getAlignmentSlice ( int64_t start, uint64_t length, bool wants_primary, bool wants_secondary ) const
        {
            return new ngs_test_engine::AlignmentItf ( ( unsigned int ) length );
        }

        virtual ngs_adapt :: AlignmentItf * getFilteredAlignmentSlice ( int64_t start, uint64_t length, uint32_t flags, int32_t map_qual ) const
        {
            return new ngs_test_engine::AlignmentItf ( ( unsigned int ) length );
        }

        virtual ngs_adapt :: PileupItf * getPileups ( bool wants_primary, bool wants_secondary ) const
        {
            return new ngs_test_engine::PileupItf ( 3 );
        }

        virtual ngs_adapt :: PileupItf * getFilteredPileups ( uint32_t flags, int32_t map_qual ) const
        {
            return new ngs_test_engine::PileupItf ( 3 );
        }

        virtual ngs_adapt :: PileupItf * getPileupSlice ( int64_t start, uint64_t length, bool wants_primary, bool wants_secondary ) const
        {
            return new ngs_test_engine::PileupItf ( ( unsigned int ) length );
        }

        virtual ngs_adapt :: PileupItf * getFilteredPileupSlice ( int64_t start, uint64_t length, uint32_t flags, int32_t map_qual ) const
        {
            return new ngs_test_engine::PileupItf ( ( unsigned int ) length );
        }

        virtual bool nextReference ()
        {
            switch ( iterateFor )
            {
            case -1:    throw ngs_adapt::ErrorMsg ( "invalid iterator access" );
            case 0:     return false;
            default:    --iterateFor; return true;
            }
        }

	public:
		ReferenceItf ()
        : iterateFor(-1)
        {
            ++instanceCount;
        }
		ReferenceItf ( unsigned int p_iterateFor )
        : iterateFor( ( int ) p_iterateFor )
        {
            ++instanceCount;
        }

        ~ReferenceItf ()
        {
            --instanceCount;
        }

        static   unsigned int instanceCount;

        int iterateFor;
    };

} // namespace ngs_test_engine

#endif // _hpp_ngs_test_engine_referenceitf_
