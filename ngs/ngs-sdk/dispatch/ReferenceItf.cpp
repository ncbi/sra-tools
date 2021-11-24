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

#include <ngs/itf/ReferenceItf.hpp>
#include <ngs/itf/PileupItf.hpp>
#include <ngs/itf/AlignmentItf.hpp>
#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/ReferenceItf.h>

#include <ngs/Alignment.hpp>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Refcount_v1_tok;
    ItfTok NGS_Reference_v1_tok ( "NGS_Reference_v1", NGS_Refcount_v1_tok );


    /*----------------------------------------------------------------------
     * create NGS_ReferenceAlignFlags from Alignment bits
     */
    static uint32_t make_flags ( uint32_t categories, uint32_t filters )
    {
        static bool tested_bits;
        if ( ! tested_bits )
        {
            assert ( ( int ) Alignment :: primaryAlignment == ( int ) NGS_ReferenceAlignFlags_wants_primary );
            assert ( ( int ) Alignment :: secondaryAlignment == ( int ) NGS_ReferenceAlignFlags_wants_secondary );
            assert ( ( int ) Alignment :: passFailed << 2 == ( int ) NGS_ReferenceAlignFlags_pass_bad );
            assert ( ( int ) Alignment :: passDuplicates << 2 == ( int ) NGS_ReferenceAlignFlags_pass_dups );
            assert ( ( int ) Alignment :: minMapQuality << 2 == ( int ) NGS_ReferenceAlignFlags_min_map_qual );
            assert ( ( int ) Alignment :: maxMapQuality << 2 == ( int ) NGS_ReferenceAlignFlags_max_map_qual );
            assert ( ( int ) Alignment :: noWraparound << 2 == ( int ) NGS_ReferenceAlignFlags_no_wraparound );
            assert ( ( int ) Alignment :: startWithinSlice << 2 == ( int ) NGS_ReferenceAlignFlags_start_within_window );
            tested_bits = true;
        }
        return ( categories & 0x03 ) | ( filters << 2 );
    }


    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_Reference_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_Reference_v1_vt * out = static_cast < const NGS_Reference_v1_vt* >
            ( Cast ( vt, NGS_Reference_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_Reference_v1" );
        return out;
    }


    /*----------------------------------------------------------------------
     * ReferenceItf
     */

    StringItf * ReferenceItf :: getCommonName () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_cmn_name != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_cmn_name ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * ReferenceItf :: getCanonicalName () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_canon_name != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_canon_name ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    bool ReferenceItf :: getIsCircular () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> is_circular != 0 );
        bool ret  = ( * vt -> is_circular ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    bool ReferenceItf :: getIsLocal () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for v1.2
        if ( vt -> dad . minor_version < 4 )
            throw ErrorMsg ( "the Reference interface provided by this NGS engine is too old to support this message" );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> is_local != 0 );
        bool ret  = ( * vt -> is_local ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    uint64_t ReferenceItf :: getLength () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_length != 0 );
        uint64_t ret  = ( * vt -> get_length ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }

    StringItf * ReferenceItf :: getReferenceBases ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReferenceBases ( offset, -1 );
    }

    StringItf * ReferenceItf :: getReferenceBases ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_bases != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ref_bases ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    StringItf * ReferenceItf :: getReferenceChunk ( uint64_t offset ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getReferenceChunk ( offset, -1 );
    }

    StringItf * ReferenceItf :: getReferenceChunk ( uint64_t offset, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_ref_chunk != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_ref_chunk ) ( self, & err, offset, length );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }


    uint64_t ReferenceItf :: getAlignmentCount () const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getAlignmentCount ( Alignment :: all );
    }


    uint64_t ReferenceItf :: getAlignmentCount ( uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for v1.2
        if ( vt -> dad . minor_version < 2 )
            throw ErrorMsg ( "the Reference interface provided by this NGS engine is too old to support this message" );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_count != 0 );
        bool wants_primary      = ( categories & Alignment :: primaryAlignment ) != 0;
        bool wants_secondary    = ( categories & Alignment :: secondaryAlignment ) != 0;
        uint64_t ret  = ( * vt -> get_align_count ) ( self, & err, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return ret;
    }

    AlignmentItf * ReferenceItf :: getAlignment ( const char * alignmentId ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_alignment != 0 );
        NGS_Alignment_v1 * ret  = ( * vt -> get_alignment ) ( self, & err, alignmentId );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }

    AlignmentItf * ReferenceItf :: getAlignments ( uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_alignments != 0 );
        bool wants_primary      = ( categories & Alignment :: primaryAlignment ) != 0;
        bool wants_secondary    = ( categories & Alignment :: secondaryAlignment ) != 0;
        NGS_Alignment_v1 * ret  = ( * vt -> get_alignments ) ( self, & err, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }


    AlignmentItf * ReferenceItf :: getAlignmentSlice ( int64_t start, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getAlignmentSlice ( start, length, Alignment :: all );
    }

    AlignmentItf * ReferenceItf :: getAlignmentSlice ( int64_t start, uint64_t length, uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_slice != 0 );
        bool wants_primary      = ( categories & Alignment :: primaryAlignment ) != 0;
        bool wants_secondary    = ( categories & Alignment :: secondaryAlignment ) != 0;
        NGS_Alignment_v1 * ret  = ( * vt -> get_align_slice ) ( self, & err, start, length, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }

    AlignmentItf * ReferenceItf :: getFilteredAlignmentSlice ( int64_t start, uint64_t length, uint32_t categories, uint32_t filters, int32_t mappingQuality ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // test for conflicting filters
        const uint32_t conflictingMapQuality = Alignment :: minMapQuality | Alignment :: maxMapQuality;
        if ( ( filters & conflictingMapQuality ) == conflictingMapQuality )
            throw ErrorMsg ( "mapping quality can only be used as a minimum or maximum value, not both" );

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // test for v1.3
        if ( vt -> dad . minor_version < 3 )
            throw ErrorMsg ( "the Reference interface provided by this NGS engine is too old to support this message" );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_filtered_align_slice != 0 );
        uint32_t flags = make_flags ( categories, filters );
        NGS_Alignment_v1 * ret  = ( * vt -> get_filtered_align_slice ) ( self, & err, start, length, flags, mappingQuality );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }

    PileupItf * ReferenceItf :: getPileups ( uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_pileups != 0 );
        bool wants_primary      = ( categories & Alignment :: primaryAlignment ) != 0;
        bool wants_secondary    = ( categories & Alignment :: secondaryAlignment ) != 0;
        NGS_Pileup_v1 * ret  = ( * vt -> get_pileups ) ( self, & err, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return PileupItf :: Cast ( ret );
    }

    PileupItf * ReferenceItf :: getFilteredPileups ( uint32_t categories, uint32_t filters, int32_t mappingQuality ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // test for conflicting filters
        const uint32_t conflictingMapQuality = Alignment :: minMapQuality | Alignment :: maxMapQuality;
        if ( ( filters & conflictingMapQuality ) == conflictingMapQuality )
            throw ErrorMsg ( "mapping quality can only be used as a minimum or maximum value, not both" );

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for v1.1
        if ( vt -> dad . minor_version < 1 )
            throw ErrorMsg ( "the Reference interface provided by this NGS engine is too old to support this message" );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_filtered_pileups != 0 );
        uint32_t flags = make_flags ( categories, filters );
        NGS_Pileup_v1 * ret  = ( * vt -> get_filtered_pileups ) ( self, & err, flags, mappingQuality );

        // check for errors
        err . Check ();

        return PileupItf :: Cast ( ret );
    }

    PileupItf * ReferenceItf :: getPileupSlice ( int64_t start, uint64_t length ) const
        NGS_THROWS ( ErrorMsg )
    {
        return this -> getPileupSlice ( start, length, Alignment :: all );
    }

    PileupItf * ReferenceItf :: getPileupSlice ( int64_t start, uint64_t length, uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_pileup_slice != 0 );
        bool wants_primary      = ( categories & Alignment :: primaryAlignment ) != 0;
        bool wants_secondary    = ( categories & Alignment :: secondaryAlignment ) != 0;
        NGS_Pileup_v1 * ret  = ( * vt -> get_pileup_slice ) ( self, & err, start, length, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return PileupItf :: Cast ( ret );
    }

    PileupItf * ReferenceItf :: getFilteredPileupSlice ( int64_t start, uint64_t length, uint32_t categories, uint32_t filters, int32_t mappingQuality ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_Reference_v1 * self = Test ();

        // test for conflicting filters
        const uint32_t conflictingMapQuality = Alignment :: minMapQuality | Alignment :: maxMapQuality;
        if ( ( filters & conflictingMapQuality ) == conflictingMapQuality )
            throw ErrorMsg ( "mapping quality can only be used as a minimum or maximum value, not both" );

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // test for v1.1
        if ( vt -> dad . minor_version < 1 )
            throw ErrorMsg ( "the Reference interface provided by this NGS engine is too old to support this message" );

        // test for bad categories
        // this should not be possible in C++, but it is possible from other bindings
        if ( categories == 0 )
            categories = Alignment :: primaryAlignment;

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_filtered_pileup_slice != 0 );
        uint32_t flags = make_flags ( categories, filters );
        NGS_Pileup_v1 * ret  = ( * vt -> get_filtered_pileup_slice ) ( self, & err, start, length, flags, mappingQuality );

        // check for errors
        err . Check ();

        return PileupItf :: Cast ( ret );
    }

    bool ReferenceItf :: nextReference ()
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        NGS_Reference_v1 * self = Test ();

        // cast vtable to our level
        const NGS_Reference_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> next != 0 );
        bool ret  = ( * vt -> next ) ( self, & err );

        // check for errors
        err . Check ();

        return ret;
    }
}

