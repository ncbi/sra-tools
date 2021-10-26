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

#ifndef _hpp_ngs_adapt_fragmentitf_
#define _hpp_ngs_adapt_fragmentitf_

#ifndef _hpp_ngs_adapt_refcount_
#include <ngs/adapter/Refcount.hpp>
#endif

#ifndef _h_ngs_itf_fragmentitf_
#include <ngs/itf/FragmentItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * FragmentItf
     */
    class FragmentItf : public Refcount < FragmentItf, NGS_Fragment_v1 >
    {
    public:

        virtual StringItf * getFragmentId () const = 0;
        virtual StringItf * getFragmentBases ( uint64_t offset, uint64_t length ) const = 0;
        virtual StringItf * getFragmentQualities ( uint64_t offset, uint64_t length ) const = 0;
        virtual bool nextFragment () = 0;

    protected:

        // support for C vtable
        FragmentItf ( const NGS_VTable * vt );
        static NGS_Fragment_v1_vt ivt;

    private:

        static NGS_String_v1 * CC get_id ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_bases ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length );
        static NGS_String_v1 * CC get_quals ( const NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t offset, uint64_t length );
        static bool next ( NGS_Fragment_v1 * self, NGS_ErrBlock_v1 * err );

    };


} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_fragmentitf_
