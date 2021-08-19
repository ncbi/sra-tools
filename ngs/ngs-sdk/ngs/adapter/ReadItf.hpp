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

#ifndef _hpp_ngs_adapt_readitf_
#define _hpp_ngs_adapt_readitf_

#ifndef _hpp_ngs_adapt_fragmentitf_
#include <ngs/adapter/FragmentItf.hpp>
#endif

#ifndef _h_ngs_itf_readitf_
#include <ngs/itf/ReadItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class StringItf;

    /*----------------------------------------------------------------------
     * ReadItf
     */
    class ReadItf : public FragmentItf
    {
    public:

        virtual StringItf * getReadId () const = 0;
        virtual uint32_t getNumFragments () const = 0;
        virtual uint32_t getReadCategory () const = 0;
        virtual StringItf * getReadGroup () const = 0;
        virtual StringItf * getReadName () const = 0;
        virtual StringItf * getReadBases ( uint64_t offset, uint64_t length ) const = 0;
        virtual StringItf * getReadQualities ( uint64_t offset, uint64_t length ) const = 0;
        virtual bool nextRead () = 0;

        inline NGS_Read_v1 * Cast ()
        { return static_cast < NGS_Read_v1* > ( OpaqueRefcount :: offset_this () ); }

        inline const NGS_Read_v1 * Cast () const
        { return static_cast < const NGS_Read_v1* > ( OpaqueRefcount :: offset_this () ); }

        // assistance for C objects
        static inline ReadItf * Self ( NGS_Read_v1 * obj )
        { return static_cast < ReadItf* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }

        static inline const ReadItf * Self ( const NGS_Read_v1 * obj )
        { return static_cast < const ReadItf* > ( OpaqueRefcount :: offset_cobj ( obj ) ); }

    protected:

        ReadItf ();
        static NGS_Read_v1_vt ivt;

    private:

        static NGS_String_v1 * CC get_id ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err );
        static uint32_t CC get_num_frags ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err );
        static uint32_t CC get_category ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_read_group ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_name ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC get_bases ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length );
        static NGS_String_v1 * CC get_quals ( const NGS_Read_v1 * self, NGS_ErrBlock_v1 * err, uint64_t offset, uint64_t length );
        static bool CC next ( NGS_Read_v1 * self, NGS_ErrBlock_v1 * err );

    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_readitf_
