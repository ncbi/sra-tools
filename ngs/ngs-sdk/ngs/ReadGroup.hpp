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

#ifndef _hpp_ngs_read_group_
#define _hpp_ngs_read_group_

#ifndef _hpp_ngs_read_iterator_
#include <ngs/ReadIterator.hpp>
#endif

#ifndef _hpp_ngs_statistics_
#include <ngs/Statistics.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards and typedefs
     */
    typedef class ReadGroupItf * ReadGroupRef;


    /*======================================================================
     * ReadGroup
     *  represents an NGS-capable object with a group of Reads
     */
    class ReadGroup
    {
    public:

        /* getName
         *  returns the simple name of the read group
         */
        String getName () const
            NGS_THROWS ( ErrorMsg );


        /*------------------------------------------------------------------
         * STATISTICS
         */

		Statistics getStatistics () const
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        ReadGroup & operator = ( ReadGroupRef ref )
            NGS_NOTHROW ();
        ReadGroup ( ReadGroupRef ref )
            NGS_NOTHROW ();

        ReadGroup & operator = ( const ReadGroup & obj )
            NGS_NOTHROW ();
        ReadGroup ( const ReadGroup & obj )
            NGS_NOTHROW ();

        ~ ReadGroup ()
            NGS_NOTHROW ();

    protected:

        ReadGroupRef self;
    };

} // namespace ngs


// inlines
#ifndef _inl_ngs_read_group_
#include <ngs/inl/ReadGroup.hpp>
#endif

#endif // _hpp_ngs_read_group_
