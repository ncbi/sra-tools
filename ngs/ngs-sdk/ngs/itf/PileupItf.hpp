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

#ifndef _hpp_ngs_itf_pileupitf_
#define _hpp_ngs_itf_pileupitf_

#ifndef _hpp_ngs_itf_refcount_
#include <ngs/itf/Refcount.hpp>
#endif

struct NGS_Pileup_v1;

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * PileupItf
     */
    class   PileupItf : public Refcount < PileupItf, NGS_Pileup_v1 >
    {
    public:

        StringItf * getReferenceSpec () const
            NGS_THROWS ( ErrorMsg );
        int64_t getReferencePosition () const
            NGS_THROWS ( ErrorMsg );
        char getReferenceBase () const
            NGS_THROWS ( ErrorMsg );
        uint32_t getPileupDepth () const
            NGS_THROWS ( ErrorMsg );
        bool nextPileup ()
            NGS_THROWS ( ErrorMsg );

    };

} // namespace ngs

#endif // _hpp_ngs_itf_pileupitf_
