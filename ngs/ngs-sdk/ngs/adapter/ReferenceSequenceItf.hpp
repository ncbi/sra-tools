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

#ifndef _hpp_ngs_adapt_referencesequenceitf_
#define _hpp_ngs_adapt_referencesequenceitf_

#ifndef _hpp_ngs_adapt_refcount_
#include <ngs/adapter/Refcount.hpp>
#endif

#ifndef _h_ngs_itf_referenceitf_
#include <ngs/itf/ReferenceSequenceItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * ReferenceSequence
     */
    class ReferenceSequenceItf : public Refcount < ReferenceSequenceItf, NGS_ReferenceSequence_v1 >
    {
    public:

        virtual StringItf * getCanonicalName () const = 0;
        virtual bool getIsCircular () const = 0;
        virtual uint64_t getLength () const = 0;
        virtual StringItf * getReferenceBases ( uint64_t offset, uint64_t length ) const = 0;
        virtual StringItf * getReferenceChunk ( uint64_t offset, uint64_t length ) const = 0;

    protected:

        ReferenceSequenceItf ();
        static NGS_ReferenceSequence_v1_vt ivt;

    private:

        static NGS_String_v1 * CC get_canon_name ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC is_circular ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err );
        static uint64_t CC get_length ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_ref_bases ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length );
        static NGS_String_v1 * CC get_ref_chunk ( const NGS_ReferenceSequence_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length );
    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_referencesequenceitf_
