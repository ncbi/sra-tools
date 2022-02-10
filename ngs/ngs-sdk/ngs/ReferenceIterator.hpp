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

#ifndef _hpp_ngs_reference_iterator_
#define _hpp_ngs_reference_iterator_

#ifndef _hpp_ngs_reference_
#include <ngs/Reference.hpp>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * ReferenceIterator
     *  iterates across a list of References
     */
    class ReferenceIterator : public Reference
    {
    public:

        /* nextReference
         *  advance to first Reference on initial invocation
         *  advance to next Reference subsequently
         *  returns false if no more References are available.
         *  throws exception if more References should be available,
         *  but could not be accessed.
         */
        bool nextReference ()
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        ReferenceIterator ( ReferenceRef ref )
            NGS_NOTHROW ();

        ReferenceIterator & operator = ( const ReferenceIterator & obj )
            NGS_THROWS ( ErrorMsg );
        ReferenceIterator ( const ReferenceIterator & obj )
            NGS_THROWS ( ErrorMsg );

        ~ ReferenceIterator ()
            NGS_NOTHROW ();

    private:

        Reference & operator = ( const Reference & obj )
            NGS_THROWS ( ErrorMsg );
        ReferenceIterator & operator = ( ReferenceRef ref )
            NGS_NOTHROW ();
    };

} // namespace ngs


#ifndef _inl_ngs_reference_iterator_
#include <ngs/inl/ReferenceIterator.hpp>
#endif

#endif // _hpp_ngs_reference_iterator_
