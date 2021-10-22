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

#ifndef _hpp_ngs_reference_sequence_
#define _hpp_ngs_reference_sequence_

#ifndef _hpp_ngs_error_msg_
#include <ngs/ErrorMsg.hpp>
#endif

#ifndef _hpp_ngs_stringref_
#include <ngs/StringRef.hpp>
#endif

#include <stdint.h>

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards and typedefs
     */
    typedef class ReferenceSequenceItf * ReferenceSequenceRef;


    /*======================================================================
     * ReferenceSequence
     *  represents a reference sequence
     */
    class ReferenceSequence
    {
    public:

        /* getCanonicalName
         *  returns the accessioned name of reference, e.g. "NC_000001.11"
         */
        String getCanonicalName () const
            NGS_THROWS ( ErrorMsg );


        /* getIsCircular
         *  returns true if reference is circular
         */
        bool getIsCircular () const
            NGS_THROWS ( ErrorMsg );


        /* getLength
         *  returns the length of the reference sequence
         */
        uint64_t getLength () const
            NGS_THROWS ( ErrorMsg );


        /* getReferenceBases
         *  return sub-sequence bases for Reference
         *  "offset" is zero-based
         */
        String getReferenceBases ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        String getReferenceBases ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );

        /* getReferenceChunk
         *  return largest contiguous chunk available of
         *  sub-sequence bases for Reference
         *  "offset" is zero-based
         *
         * NB - actual returned sequence may be shorter
         *  than requested.
         */
        StringRef getReferenceChunk ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringRef getReferenceChunk ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        ReferenceSequence & operator = ( ReferenceSequenceRef ref )
            NGS_NOTHROW ();
        ReferenceSequence ( ReferenceSequenceRef ref )
            NGS_NOTHROW ();

        ReferenceSequence & operator = ( const ReferenceSequence & obj )
            NGS_THROWS ( ErrorMsg );
        ReferenceSequence ( const ReferenceSequence & obj )
            NGS_THROWS ( ErrorMsg );

        ~ ReferenceSequence ()
            NGS_NOTHROW ();

    protected:

        ReferenceSequenceRef self;
    };

} // namespace ngs


#ifndef _inl_ngs_reference_sequence_
#include <ngs/inl/ReferenceSequence.hpp>
#endif

#endif // _hpp_ngs_reference_sequence_
