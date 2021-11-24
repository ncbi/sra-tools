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

#ifndef _hpp_ngs_test_engine_pileupeventitf_
#define _hpp_ngs_test_engine_pileupeventitf_

#include <ngs/adapter/ErrorMsg.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/PileupEventItf.hpp>

namespace ngs_test_engine
{
    /*======================================================================
     * PileupEventItf
     *  represents a single cell of a sparse 2D matrix
     *  with Reference coordinates on one axis
     *  and stacked Alignments on the other axis
     */
    class PileupEventItf : public ngs_adapt::PileupEventItf
    {
    public:
    
        virtual int32_t getMappingQuality () const 
        {
            return 98;
        }

        virtual ngs_adapt::StringItf * getAlignmentId () const
        {
            static std::string alId = "pileupEventAlignId";
            return new ngs_adapt::StringItf ( alId.c_str(), alId.size() );
        }

        virtual int64_t getAlignmentPosition () const
        {
            return 5678;
        }

        virtual int64_t getFirstAlignmentPosition () const
        {
            return 90123;
        }

        virtual int64_t getLastAlignmentPosition () const
        {
            return 45678;
        }

        virtual uint32_t getEventType () const
        {
            return 1;
        }

        virtual char getAlignmentBase () const
        {
            return 'A';
        }

        virtual char getAlignmentQuality () const
        {
            return 'q';
        }

        virtual ngs_adapt::StringItf * getInsertionBases () const
        {
            static std::string bases = "AC";
            return new ngs_adapt::StringItf ( bases.c_str(), bases.size() );
        }

        virtual ngs_adapt::StringItf * getInsertionQualities () const
        {
            static std::string quals = "#$";
            return new ngs_adapt::StringItf ( quals.c_str(), quals.size() );
        }

        virtual uint32_t getEventRepeatCount () const
        {
            return 45;
        }


        virtual uint32_t getEventIndelType () const
        {
            return ( uint32_t ) ngs::PileupEvent::intron_minus;
        }

        virtual bool nextPileupEvent ()
        {
            switch ( iterateFor )
            {
            case -1:    throw ngs_adapt::ErrorMsg ( "invalid iterator access" );
            case 0:     return false;
            default:    --iterateFor; return true;
            }
        }

        virtual void resetPileupEvent ()
        {
        }

	public:
		PileupEventItf ( unsigned int p_iterateFor ) 
        : iterateFor( p_iterateFor )
        { 
            ++instanceCount;
        }
		
        ~PileupEventItf () 
        { 
            --instanceCount;
        }

        static   unsigned int instanceCount;

        unsigned int iterateFor;
    };

} // namespace ngs

#endif // _hpp_ngs_test_engine_pileupeventitf_
