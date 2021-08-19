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

#ifndef _hpp_ngs_read_
#define _hpp_ngs_read_

#ifndef _hpp_ngs_fragment_iterator_
#include <ngs/FragmentIterator.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * forwards and typedefs
     */
    typedef FragmentRef ReadRef;


    /*======================================================================
     * Read
     *  represents an NGS machine read
     *  having some number of biological Fragments
     */
    class Read : public FragmentIterator
    {
    public:

        /* getReadId
         */
        StringRef getReadId () const
            NGS_THROWS ( ErrorMsg );

        /* getNumFragments
         *  the number of biological Fragments contained in the read
         */
        uint32_t getNumFragments () const
            NGS_THROWS ( ErrorMsg );

        /* fragmentIsAligned
         *  tests a fragment for being aligned
         */
        bool fragmentIsAligned ( uint32_t fragIdx ) const
            NGS_THROWS ( ErrorMsg );

        /*------------------------------------------------------------------
         * read details
         */

        /* ReadCategory
         */
        enum ReadCategory
        {
            fullyAligned     = 1,
            partiallyAligned = 2,
            aligned          = fullyAligned | partiallyAligned,
            unaligned        = 4,
            all              = aligned | unaligned
        };

        /* getReadCategory
         */
        ReadCategory getReadCategory () const
            NGS_THROWS ( ErrorMsg );

        /* getReadGroup
         */
        String getReadGroup () const
            NGS_THROWS ( ErrorMsg );

        /* getReadName
         */
        StringRef getReadName () const
            NGS_THROWS ( ErrorMsg );


        /* getReadBases
         *  return sequence bases
         *  "offset" is zero-based
         */
        StringRef getReadBases () const
            NGS_THROWS ( ErrorMsg );
        StringRef getReadBases ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringRef getReadBases ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );


        /* getReadQualities
         *  return phred quality values
         *  using ASCII offset of 33
         *  "offset" is zero-based
         */
        StringRef getReadQualities () const
            NGS_THROWS ( ErrorMsg );
        StringRef getReadQualities ( uint64_t offset ) const
            NGS_THROWS ( ErrorMsg );
        StringRef getReadQualities ( uint64_t offset, uint64_t length ) const
            NGS_THROWS ( ErrorMsg );

    public:

        // C++ support

        Read ( ReadRef ref )
            NGS_NOTHROW ();

        Read & operator = ( const Read & obj )
            NGS_THROWS ( ErrorMsg );
        Read ( const Read & obj )
            NGS_THROWS ( ErrorMsg );

        ~ Read ()
            NGS_NOTHROW ();

    private:

        Read & operator = ( ReadRef ref )
            NGS_NOTHROW ();
    };

} // namespace ngs


// inlines
#ifndef _inl_ngs_read_
#include <ngs/inl/Read.hpp>
#endif

#endif // _hpp_ngs_read_
