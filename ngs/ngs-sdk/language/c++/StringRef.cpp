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

#include <ngs/StringRef.hpp>
#include <ngs/itf/StringItf.h>

namespace ngs
{
    /*----------------------------------------------------------------------
     * StringRef
     */

    String StringRef :: toString () const
        NGS_THROWS ( ErrorMsg )
    {
        const char * str = self -> data ();
        size_t sz = self -> size ();

        return String ( str, sz );
    }

    String StringRef :: toString ( size_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        const char * str = self -> data ();
        size_t sz = self -> size ();

        if ( offset > sz )
            offset = sz;

        return String ( str + offset, sz - offset );
    }

    String StringRef :: toString ( size_t offset, size_t size ) const
        NGS_THROWS ( ErrorMsg )
    {
        const char * str = self -> data ();
        size_t sz = self -> size ();

        if ( offset >= sz )
        {
            offset = sz;
            size = 0;
        }
        else if ( offset + size > sz )
        {
            size = sz - offset;
        }

        return String ( str + offset, size );
    }


    // C++ support
    StringRef :: StringRef ( StringItf * ref )
            NGS_NOTHROW ()
        : self ( ref )
    {
        assert ( self != 0 );
    }

    StringRef :: StringRef ( const StringRef & obj )
            NGS_NOTHROW ()
        : self ( obj . self -> Duplicate () )
    {
        assert ( self != 0 );
    }

    StringRef & StringRef :: operator = ( const StringRef & obj )
        NGS_NOTHROW ()
    {
        StringItf * new_self = obj . self -> Duplicate ();
        self -> Release ();
        self = new_self;

        return * this;
    }

    StringRef :: ~ StringRef ()
        NGS_NOTHROW ()
    {
        self -> Release ();
        self = 0;
    }

    :: std :: ostream & operator << ( :: std :: ostream & s, const StringRef & str )
    {
        return s . write ( str . data (), str . size () );
    }


} // namespace ngs
