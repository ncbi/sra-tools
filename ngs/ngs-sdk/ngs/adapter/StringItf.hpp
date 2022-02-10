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

#ifndef _hpp_ngs_adapt_stringitf_
#define _hpp_ngs_adapt_stringitf_

#ifndef _hpp_ngs_adapt_refcount_
#include <ngs/adapter/Refcount.hpp>
#endif

#ifndef _h_ngs_itf_stringitf_
#include <ngs/itf/StringItf.h>
#endif

#include <string>

namespace ngs_adapt
{
    /*----------------------------------------------------------------------
     * StringItf
     */
    class StringItf : public Refcount < StringItf, NGS_String_v1 >
    {
    public:

        // interface
        virtual const char * data () const;
        virtual size_t size () const;
        virtual StringItf * substr ( size_t offset, size_t size ) const;

    public:

        // C++ support
        StringItf ( const char * data, size_t size );
        virtual ~ StringItf ();

    private:

        // ?? should these be left public ??
        StringItf ( const StringItf & obj );
        StringItf & operator = ( const StringItf & obj );

    protected:

        // support for C vtable
        static NGS_String_v1_vt ivt;

    private:

        // dispatch handlers
        static const char * CC data_dispatch ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err );
        static size_t CC size_dispatch ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_String_v1 * CC substr_dispatch ( const NGS_String_v1 * self, NGS_ErrBlock_v1 * err,
                                                    size_t offset, size_t size );

    protected:

        // member variables
        const char * str;
        size_t sz;
    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_stringitf_
