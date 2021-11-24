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

#ifndef _hpp_ngs_read_iterator_
#define _hpp_ngs_read_iterator_

#ifndef _hpp_ngs_read_
#include <ngs/Read.hpp>
#endif

namespace ngs
{
    /*----------------------------------------------------------------------
     * ReadIterator
     *  iterates across a list of Reads
     */
    class ReadIterator : public Read
    {
    public:

        /* nextRead
         *  advance to first Read on initial invocation
         *  advance to next Read subsequently
         *  returns false if no more Reads are available.
         *  throws exception if more Reads should be available,
         *  but could not be accessed.
         */
        bool nextRead ()
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        ReadIterator ( ReadRef ref )
            NGS_NOTHROW ();

        ReadIterator & operator = ( const ReadIterator & obj )
            NGS_THROWS ( ErrorMsg );
        ReadIterator ( const ReadIterator & obj )
            NGS_THROWS ( ErrorMsg );

        ~ ReadIterator ()
            NGS_NOTHROW ();

    private:

        Read & operator = ( const Read & obj )
            NGS_THROWS ( ErrorMsg );
        ReadIterator & operator = ( ReadRef ref )
            NGS_NOTHROW ();
    };

} // namespace ngs


// inlines
#ifndef _inl_ngs_read_iterator_
#include <ngs/inl/ReadIterator.hpp>
#endif

#endif // _hpp_ngs_read_iterator_
