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

#ifndef _hpp_ngs_read_group_iterator_
#define _hpp_ngs_read_group_iterator_

#ifndef _hpp_ngs_read_group_
#include <ngs/ReadGroup.hpp>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * ReadGroupIterator
     *  iterates across a list of ReadGroups
     */
    class ReadGroupIterator : public ReadGroup
    {
    public:

        /* nextReadGroup
         *  advance to first ReadGroup on initial invocation
         *  advance to next ReadGroup subsequently
         *  returns false if no more ReadGroups are available.
         *  throws exception if more ReadGroups should be available,
         *  but could not be accessed.
         */
        bool nextReadGroup ()
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        ReadGroupIterator ( ReadGroupRef ref )
            NGS_NOTHROW ();

        ReadGroupIterator & operator = ( const ReadGroupIterator & obj )
            NGS_THROWS ( ErrorMsg );
        ReadGroupIterator ( const ReadGroupIterator & obj )
            NGS_THROWS ( ErrorMsg );

        ~ ReadGroupIterator ()
            NGS_NOTHROW ();

    private:

        ReadGroup & operator = ( const ReadGroup & obj )
            NGS_THROWS ( ErrorMsg );
        ReadGroupIterator & operator = ( ReadGroupRef ref )
            NGS_NOTHROW ();
    };

} // namespace ngs


// inlines
#ifndef _inl_ngs_read_group_iterator_
#include <ngs/inl/ReadGroupIterator.hpp>
#endif

#endif // _hpp_ngs_read_group_iterator_
