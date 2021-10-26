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

#ifndef _hpp_ngs_adapt_pileupitf_
#define _hpp_ngs_adapt_pileupitf_

#ifndef _hpp_ngs_adapt_pileup_eventitf_
#include <ngs/adapter/PileupEventItf.hpp>
#endif

#ifndef _h_ngs_itf_pileupitf_
#include <ngs/itf/PileupItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;
    class PileupEventItf;

    /*----------------------------------------------------------------------
     * PileupItf
     */
    class PileupItf : public PileupEventItf
    {
    public:

        virtual StringItf * getReferenceSpec () const = 0;
        virtual int64_t getReferencePosition () const = 0;
        virtual char getReferenceBase () const = 0;
        virtual uint32_t getPileupDepth () const = 0;
        virtual bool nextPileup () = 0;

        inline NGS_Pileup_v1 * Cast ()
        { return static_cast < NGS_Pileup_v1* > ( OpaqueRefcount :: offset_this () ); }

        inline const NGS_Pileup_v1 * Cast () const
        { return static_cast < const NGS_Pileup_v1* > ( OpaqueRefcount :: offset_this () ); }

        // assistance for C objects
        static inline PileupItf * Self ( NGS_Pileup_v1 * obj )
        { return static_cast < PileupItf* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }

        static inline const PileupItf * Self ( const NGS_Pileup_v1 * obj )
        { return static_cast < const PileupItf* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }
    protected:

        PileupItf ();
        static NGS_Pileup_v1_vt ivt;

    private:

        static NGS_String_v1 * CC get_ref_spec ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err );
        static int64_t CC get_ref_pos ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err );
        static char CC get_ref_base ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err );
        static uint32_t CC get_pileup_depth ( const NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC next ( NGS_Pileup_v1 * self, NGS_ErrBlock_v1 * err );

    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_pileupitf_
