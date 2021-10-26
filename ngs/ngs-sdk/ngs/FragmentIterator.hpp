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

#ifndef _hpp_ngs_fragment_iterator_
#define _hpp_ngs_fragment_iterator_

#ifndef _hpp_ngs_fragment_
#include <ngs/Fragment.hpp>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * FragmentIterator
     *  iterates across a list of Fragments
     */
    class FragmentIterator : public Fragment
    {
    public:

        /* nextFragment
         *  advance to next Fragment
         *  returns false if no more Fragments are available.
         *  throws exception if more Fragments should be available,
         *  but could not be accessed.
         */
        bool nextFragment ()
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        FragmentIterator ( FragmentRef ref )
            NGS_NOTHROW ();

        FragmentIterator & operator = ( const FragmentIterator & obj )
            NGS_THROWS ( ErrorMsg );
        FragmentIterator ( const FragmentIterator & obj )
            NGS_THROWS ( ErrorMsg );

        ~ FragmentIterator ()
            NGS_NOTHROW ();

    private:

        Fragment & operator = ( const Fragment & obj )
            NGS_THROWS ( ErrorMsg );
        FragmentIterator & operator = ( FragmentRef ref )
            NGS_NOTHROW ();
    };

} // namespace ngs


// inlines
#ifndef _inl_ngs_fragment_iterator_
#include <ngs/inl/FragmentIterator.hpp>
#endif

#endif // _hpp_ngs_fragment_iterator_
