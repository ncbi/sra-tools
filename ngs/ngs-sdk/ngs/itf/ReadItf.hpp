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

#ifndef _hpp_ngs_itf_readitf_
#define _hpp_ngs_itf_readitf_

#ifndef _hpp_ngs_itf_refcount_
#include <ngs/itf/Refcount.hpp>
#endif

struct NGS_Read_v1;

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * ReadItf
     */
    class   ReadItf : public Refcount < ReadItf, NGS_Read_v1 >
    {
    public:

        StringItf * getReadId () const
            NGS_THROWS ( ErrorMsg );
        uint32_t getNumFragments () const
            NGS_THROWS ( ErrorMsg );
        bool fragmentIsAligned ( uint32_t fragIdx ) const
            NGS_THROWS ( ErrorMsg );
        uint32_t getReadCategory () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadGroup () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadName () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadBases () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadBases ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadBases ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadQualities () const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadQualities ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringItf * getReadQualities ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );
        bool nextRead ()
            NGS_THROWS ( ErrorMsg );
    };

} // namespace ngs

#endif // _hpp_ngs_itf_readitf_
