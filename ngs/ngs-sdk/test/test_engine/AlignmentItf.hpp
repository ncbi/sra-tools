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

#ifndef _hpp_ngs_test_engine_alignmentitf_
#define _hpp_ngs_test_engine_alignmentitf_

#include <ngs/adapter/ErrorMsg.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/AlignmentItf.hpp>

namespace ngs_test_engine
{

    /*----------------------------------------------------------------------
     * forwards
     */

    /*----------------------------------------------------------------------
     * AlignmentItf
     */
    class AlignmentItf : public ngs_adapt :: AlignmentItf
    {
    public:

        virtual ngs_adapt::StringItf * getFragmentId () const 
        {
            static std::string id = "alignFragId";
            return new ngs_adapt::StringItf( id.c_str(), id.size() ); 
        }

        virtual ngs_adapt::StringItf * getFragmentBases ( uint64_t offset, uint64_t length ) const 
        {
            static std::string bases = "AGCT";
            static std::string substr;
            substr = bases.substr( ( size_t ) offset, length == (uint64_t)-1 ? std::string::npos : ( size_t ) length );
            return new ngs_adapt::StringItf( substr.c_str(), substr.size() ); 
        }

        virtual ngs_adapt::StringItf * getFragmentQualities ( uint64_t offset, uint64_t length ) const 
        {
            static std::string quals = "bbd^";
            static std::string substr;
            substr = quals.substr( ( size_t ) offset, length == (uint64_t)-1 ? std::string::npos : ( size_t ) length );
            return new ngs_adapt::StringItf( substr.c_str(), substr.size() ); 
        }

        virtual ngs_adapt::StringItf * getAlignmentId () const 
        {
            return new ngs_adapt::StringItf( id.c_str(), id.size() ); 
        }

        virtual ngs_adapt::StringItf * getReferenceSpec () const
        {
            static std::string spec = "referenceSpec";
            return new ngs_adapt::StringItf( spec.c_str(), spec.size() ); 
        }

        virtual int32_t getMappingQuality () const 
        { 
            return 90; 
        }

        virtual ngs_adapt::StringItf * getReferenceBases () const 
        {
            static std::string bases = "CTAG";
            return new ngs_adapt::StringItf( bases.c_str(), bases.size() ); 
        }

        virtual ngs_adapt::StringItf * getReadGroup () const
        {
            static std::string rg = "alignReadGroup";
            return new ngs_adapt::StringItf( rg.c_str(), rg.size() ); 
        }

        virtual ngs_adapt::StringItf * getReadId () const
        {
            static std::string id = "alignReadId";
            return new ngs_adapt::StringItf( id.c_str(), id.size() ); 
        }

        virtual ngs_adapt::StringItf * getClippedFragmentBases () const
        {
            static std::string bases = "TA";
            return new ngs_adapt::StringItf( bases.c_str(), bases.size() ); 
        }

        virtual ngs_adapt::StringItf * getClippedFragmentQualities () const
        {
            static std::string quals = "bd";
            return new ngs_adapt::StringItf( quals.c_str(), quals.size() ); 
        }

        virtual ngs_adapt::StringItf * getAlignedFragmentBases () const
        {
            static std::string bases = "AC";
            return new ngs_adapt::StringItf( bases.c_str(), bases.size() ); 
        }

        virtual bool isPrimary () const 
        { 
            return false; 
        }

        virtual int64_t getAlignmentPosition () const 
        { 
            return 123; 
        }

        virtual uint64_t getReferencePositionProjectionRange ( int64_t ref_pos ) const 
        { 
            return ((uint64_t)123 << 32) | 15;
        }

        virtual uint64_t getAlignmentLength () const 
        { 
            return 321; 
        }

        virtual bool getIsReversedOrientation () const 
        { 
            return true; 
        }

        virtual int32_t getSoftClip ( uint32_t edge ) const 
        { 
            return edge + 1; 
        }

        virtual uint64_t getTemplateLength () const 
        { 
            return 17; 
        }

        virtual ngs_adapt::StringItf * getShortCigar ( bool clipped ) const 
        { 
            static std::string cigar = "MD";
            return new ngs_adapt::StringItf( cigar.c_str(), cigar.size() ); 
        }

        virtual ngs_adapt::StringItf * getLongCigar ( bool clipped ) const 
        { 
            static std::string cigar = "MDNA";
            return new ngs_adapt::StringItf( cigar.c_str(), cigar.size() ); 
        }

        virtual char getRNAOrientation () const 
        { 
            return '+'; 
        }

        virtual bool hasMate () const 
        { 
            return true; 
        }

        virtual ngs_adapt::StringItf * getMateAlignmentId () const 
        { 
            static std::string id = "mateId";
            return new ngs_adapt::StringItf( id.c_str(), id.size() ); 
        }

        virtual ngs_adapt::AlignmentItf * getMateAlignment () const 
        { 
            return new ngs_test_engine::AlignmentItf(); 
        }

        virtual ngs_adapt::StringItf * getMateReferenceSpec () const 
        {
            static std::string spec = "mateReferenceSpec";
            return new ngs_adapt::StringItf( spec.c_str(), spec.size() ); 
        }

        virtual bool getMateIsReversedOrientation () const 
        { 
            return true; 
        }

        virtual bool nextAlignment () 
        { 
            switch ( iterateFor )
            {
            case -1:    throw ngs_adapt::ErrorMsg ( "invalid iterator access" );
            case 0:     return false;
            default:    --iterateFor; return true;
            }
        }

	public:
		AlignmentItf () 
        :   iterateFor(-1)
        { 
            ++instanceCount;
        }
		AlignmentItf ( const char* p_id ) 
        :   id(p_id), 
            iterateFor(-1)
        { 
            ++instanceCount;
        }
		AlignmentItf ( unsigned int p_iterateFor ) 
        :   iterateFor( ( int ) p_iterateFor )
        { 
            ++instanceCount;
        }
		
        ~AlignmentItf () 
        { 
            --instanceCount;
        }

        static   unsigned int instanceCount;

        std::string id;
        int iterateFor;
    };

} // namespace ngs_test_engine

#endif // _hpp_ngs_test_engine_alignmentitf_
