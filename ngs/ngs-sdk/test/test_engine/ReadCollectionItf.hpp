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

#ifndef _hpp_ngs_test_engine_read_collectionitf_
#define _hpp_ngs_test_engine_read_collectionitf_

#include <ngs/adapter/ReadCollectionItf.hpp>
#include <ngs/adapter/StringItf.hpp>

#include "ReadGroupItf.hpp"
#include "ReferenceItf.hpp"
#include "AlignmentItf.hpp"
#include "ReadItf.hpp"

namespace ngs_test_engine
{

    /*----------------------------------------------------------------------
     * forwards
     */

    /*----------------------------------------------------------------------
     * ReadCollectionItf
     */
    class ReadCollectionItf : public ngs_adapt::ReadCollectionItf
    {
    public:

		virtual ngs_adapt::StringItf * getName () const 
        { 
            return new ngs_adapt::StringItf ( name.c_str(), name.size() ); 
        }

        virtual ngs_adapt::ReadGroupItf *	getReadGroups () const 
        { 
            return new ngs_test_engine::ReadGroupItf ( 2 );
        }

        virtual bool hasReadGroup ( const char * spec ) const 
        { 
            return true;
        }

        virtual ngs_adapt::ReadGroupItf *	getReadGroup ( const char * spec ) const 
        { 
            return new ngs_test_engine::ReadGroupItf (); 
        }

        virtual ngs_adapt::ReferenceItf *	getReferences () const 
        { 
            return new ngs_test_engine::ReferenceItf ( 3 ); 
        }

        virtual bool hasReference ( const char * spec ) const 
        { 
            return true;
        }

        virtual ngs_adapt::ReferenceItf *	getReference ( const char * spec ) const 
        { 
            return new ngs_test_engine::ReferenceItf (); 
        }

        virtual ngs_adapt::AlignmentItf *	getAlignment ( const char * alignmentId ) const 
        { 
            return new ngs_test_engine::AlignmentItf ( alignmentId ); 
        }

        virtual ngs_adapt::AlignmentItf *	getAlignments ( bool wants_primary, bool wants_secondary ) const 
        { 
            return new ngs_test_engine::AlignmentItf ( 4 ); 
        }

        virtual uint64_t getAlignmentCount ( bool wants_primary, bool wants_secondary ) const 
        { 
            return 13; 
        }

        virtual ngs_adapt::AlignmentItf * getAlignmentRange ( uint64_t first, uint64_t count, bool wants_primary, bool wants_secondary ) const 
        {
            return new ngs_test_engine::AlignmentItf ( 2 ); 
        }

        virtual ngs_adapt::ReadItf * getRead ( const char * readId ) const 
        {
            return new ngs_test_engine::ReadItf ( readId ); 
        }

        virtual ngs_adapt::ReadItf * getReads ( bool wants_full, bool wants_partial, bool wants_unaligned ) const 
        {
            return new ngs_test_engine::ReadItf ( 6 ); 
        }

        virtual uint64_t getReadCount ( bool wants_full, bool wants_partial, bool wants_unaligned ) const 
        { 
            return 26; 
        }

        virtual ngs_adapt::ReadItf * getReadRange ( uint64_t first, uint64_t count, bool wants_full, bool wants_partial, bool wants_unaligned ) const 
        {
            return new ngs_test_engine::ReadItf ( ( unsigned int ) count ); 
        }

	public:
		ReadCollectionItf ( const char* accession ) 
            : name ( accession )
        { 
            ++instanceCount;
        }
		
        ~ReadCollectionItf () 
        { 
            --instanceCount;
        }

        static   unsigned int instanceCount;

    private:
        std::string name;
    };

} // namespace ngs_test_engine

#endif // _hpp_ngs_test_engine_read_collectionitf_
