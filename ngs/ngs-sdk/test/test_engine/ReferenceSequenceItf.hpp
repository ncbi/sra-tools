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

#ifndef _hpp_ngs_test_engine_referencesequenceitf_
#define _hpp_ngs_test_engine_referencesequenceitf_

#include <ngs/adapter/ErrorMsg.hpp>
#include <ngs/adapter/StringItf.hpp>
#include <ngs/adapter/ReferenceSequenceItf.hpp>

namespace ngs_test_engine
{

    /*----------------------------------------------------------------------
     * forwards
     */

    /*----------------------------------------------------------------------
     * ReferenceItf
     */
    class ReferenceSequenceItf : public ngs_adapt :: ReferenceSequenceItf
    {
    public:

		virtual ngs_adapt :: StringItf * getCanonicalName () const 
        { 
            static std::string name = "canonical name";
            return new ngs_adapt :: StringItf ( name.c_str(), name.size() ); 
        }

        virtual bool getIsCircular () const
        {   
            return false;
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

	public:
		ReferenceSequenceItf () 
        { 
        }
		
        ~ReferenceSequenceItf () 
        { 
        }
    };

} // namespace ngs_test_engine

#endif // _hpp_ngs_test_engine_referencesequenceitf_
