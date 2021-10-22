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

#ifndef _hpp_ngs_stringref_
#define _hpp_ngs_stringref_

#ifndef _hpp_ngs_error_msg_
#include <ngs/ErrorMsg.hpp>
#endif

#include <string>
#include <cstdlib>
#include <iostream>

namespace ngs
{
    /*----------------------------------------------------------------------
     * forwards and typedefs
     */
    class StringItf;
    typedef :: std :: string String;


    /*----------------------------------------------------------------------
     * StringRef
     *  a simple reference to textual data
     *  main purpose is to avoid copying
     *  provides a cast operator to create a language-specific String
     */
    class StringRef
    {
    public:

        /* data
         *  return character string
         *  NOT necessarily NUL-terminated
         */
        const char * data () const
            NGS_NOTHROW ();

        /* size
         *   return size of string in bytes
         */
        size_t size () const
            NGS_NOTHROW ();

        /* substr
         *  create a substring of the original
         *  almost certainly NOT NUL-terminated
         *  "offset" is zero-based
         */
        StringRef substr ( size_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringRef substr ( size_t offset, size_t size ) const
            NGS_THROWS ( ErrorMsg );

        /* toString
         *  create a normal C++ string
         *  copies data
         *  "offset" is zero-based
         */
        String toString () const
            NGS_THROWS ( ErrorMsg );
        String toString ( size_t offset ) const
            NGS_THROWS ( ErrorMsg );
        String toString ( size_t offset, size_t size ) const
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support
        StringRef ( StringItf * ref )
            NGS_NOTHROW ();

        StringRef ( const StringRef & obj )
            NGS_NOTHROW ();
        StringRef & operator = ( const StringRef & obj )
            NGS_NOTHROW ();

        ~ StringRef ()
            NGS_NOTHROW ();

    private:

        StringRef & operator = ( StringItf * ref )
            NGS_NOTHROW ();

        StringItf * self;
    };

    // support for C++ ostream
    :: std :: ostream & operator << ( :: std :: ostream & s, const StringRef & str );

} // namespace ngs


// inlines
#ifndef _inl_ngs_stringref_
#include <ngs/inl/StringRef.hpp>
#endif

#endif // _hpp_ngs_stringref_
