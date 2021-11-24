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

#ifndef _hpp_ngs_itf_collection_
#define _hpp_ngs_itf_collection_

#ifndef _hpp_ngs_itf_collection_
#include <ngs/ReadCollection.hpp>
#endif

#ifndef _hpp_ngs_itf_read_collectionitf_
#include <ngs/itf/ReadCollectionItf.hpp>
#endif

namespace ngs
{

    /*----------------------------------------------------------------------
     * ReadCollection
     */

	inline
    String ReadCollection :: getName () const
        NGS_THROWS ( ErrorMsg )
    { return StringRef ( self -> getName () ) . toString (); }

	inline
    ReadGroupIterator ReadCollection :: getReadGroups () const
        NGS_THROWS ( ErrorMsg )
    { return ReadGroupIterator ( self -> getReadGroups () ); }

	inline
    bool ReadCollection :: hasReadGroup ( const String & spec ) const
        NGS_NOTHROW ()
    { return self -> hasReadGroup ( spec . c_str () ); }

	inline
    ReadGroup ReadCollection :: getReadGroup ( const String & spec ) const
        NGS_THROWS ( ErrorMsg )
    { return ReadGroup ( self -> getReadGroup ( spec . c_str () ) ); }

	inline
    ReferenceIterator ReadCollection :: getReferences () const
        NGS_THROWS ( ErrorMsg )
    { return ReferenceIterator ( self -> getReferences () ); }

	inline
    bool ReadCollection :: hasReference ( const String & spec ) const
        NGS_NOTHROW ()
    { return self -> hasReference ( spec . c_str () ); }

	inline
    Reference ReadCollection :: getReference ( const String & spec ) const
        NGS_THROWS ( ErrorMsg )
    { return Reference ( self -> getReference ( spec . c_str () ) ); }

	inline
    Alignment ReadCollection :: getAlignment ( const String & alignmentId ) const
        NGS_THROWS ( ErrorMsg )
    { return Alignment ( ( AlignmentRef ) self -> getAlignment ( alignmentId . c_str () ) ); }

	inline
    AlignmentIterator ReadCollection :: getAlignments ( Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return AlignmentIterator ( ( AlignmentRef ) self -> getAlignments ( ( uint32_t ) categories ) ); }

	inline
    uint64_t ReadCollection :: getAlignmentCount () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAlignmentCount ( ( uint32_t ) Alignment :: all ); }

	inline
    uint64_t ReadCollection :: getAlignmentCount ( Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return self -> getAlignmentCount ( ( uint32_t ) categories ); }

	inline
    AlignmentIterator ReadCollection :: getAlignmentRange ( uint64_t first, uint64_t count ) const
        NGS_THROWS ( ErrorMsg )
    { return AlignmentIterator ( ( AlignmentRef ) self -> getAlignmentRange ( first, count, ( uint32_t ) Alignment :: all ) ); }

	inline
    AlignmentIterator ReadCollection :: getAlignmentRange ( uint64_t first, uint64_t count, Alignment :: AlignmentCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return AlignmentIterator ( ( AlignmentRef ) self -> getAlignmentRange ( first, count, ( uint32_t ) categories ) ); }

	inline
    Read ReadCollection :: getRead ( const String & readId ) const
        NGS_THROWS ( ErrorMsg )
    { return Read ( ( ReadRef ) self -> getRead ( readId . c_str () ) ); }

	inline
    ReadIterator ReadCollection :: getReads ( Read :: ReadCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return ReadIterator ( ( ReadRef ) self -> getReads ( ( uint32_t ) categories ) ); }

	inline
    uint64_t ReadCollection :: getReadCount () const
        NGS_THROWS ( ErrorMsg )
    { return self -> getReadCount ( ( uint32_t ) Read :: all ); }

	inline
    uint64_t ReadCollection :: getReadCount ( Read :: ReadCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return self -> getReadCount ( ( uint32_t ) categories ); }

	inline
    ReadIterator ReadCollection :: getReadRange ( uint64_t first, uint64_t count ) const
        NGS_THROWS ( ErrorMsg )
    { return ReadIterator ( ( ReadRef ) self -> getReadRange ( first, count ) ); }

	inline
    ReadIterator ReadCollection :: getReadRange ( uint64_t first, uint64_t count, Read :: ReadCategory categories ) const
        NGS_THROWS ( ErrorMsg )
    { return ReadIterator ( ( ReadRef ) self -> getReadRange ( first, count, ( uint32_t ) categories ) ); }

} // namespace ngs

#endif // _hpp_ngs_itf_collection_
