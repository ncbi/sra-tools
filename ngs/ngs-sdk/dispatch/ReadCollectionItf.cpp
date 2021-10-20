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

#include <ngs/itf/ReadCollectionItf.hpp>

#include <ngs/itf/StringItf.hpp>
#include <ngs/itf/ReadItf.hpp>
#include <ngs/itf/ReadGroupItf.hpp>
#include <ngs/itf/ReferenceItf.hpp>
#include <ngs/itf/AlignmentItf.hpp>
#include <ngs/itf/StatisticsItf.hpp>

#include <ngs/itf/ErrBlock.hpp>
#include <ngs/itf/VTable.hpp>

#include <ngs/itf/ReadCollectionItf.h>

#include <ngs/Alignment.hpp>
#include <ngs/Read.hpp>

namespace ngs
{
    /*----------------------------------------------------------------------
     * metadata
     */
    extern ItfTok NGS_Refcount_v1_tok;
    ItfTok NGS_ReadCollection_v1_tok ( "NGS_ReadCollection_v1", NGS_Refcount_v1_tok );

    /*----------------------------------------------------------------------
     * access vtable
     */
    static inline
    const NGS_ReadCollection_v1_vt * Access ( const NGS_VTable * vt )
    {
        const NGS_ReadCollection_v1_vt * out = static_cast < const NGS_ReadCollection_v1_vt* >
            ( Cast ( vt, NGS_ReadCollection_v1_tok ) );
        if ( out == 0 )
            throw ErrorMsg ( "object is not of type NGS_ReadCollection_v1" );
        return out;
    }

    /*----------------------------------------------------------------------
     * ReadCollectionItf
     */

    StringItf * ReadCollectionItf :: getName () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_name != 0 );
        NGS_String_v1 * ret  = ( * vt -> get_name ) ( self, & err );

        // check for errors
        err . Check ();

        return StringItf :: Cast ( ret );
    }

    ReadGroupItf * ReadCollectionItf :: getReadGroups () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_read_groups != 0 );
        NGS_ReadGroup_v1 * ret  = ( * vt -> get_read_groups ) ( self, & err );

        // check for errors
        err . Check ();

        return ReadGroupItf :: Cast ( ret );
    }

    bool ReadCollectionItf :: hasReadGroup ( const char * spec ) const
        NGS_NOTHROW ()
    {
        try
        {
            // the object is really from C
            const NGS_ReadCollection_v1 * self = Test ();

            // cast vtable to our level
            const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

            // test for v1.1
            if ( vt -> dad . minor_version < 1 )
            {
                ReadGroupItf * itf = getReadGroup ( spec );
                if ( itf == 0 )
                    return false;

                itf -> Release ();
                return true;
            }

            // call through C vtable
            assert ( vt -> has_read_group != 0 );
            return ( * vt -> has_read_group ) ( self, spec );
        }
        catch ( ... )
        {
        }

        return false;
    }

    ReadGroupItf * ReadCollectionItf :: getReadGroup ( const char * spec ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_read_group != 0 );
        NGS_ReadGroup_v1 * ret  = ( * vt -> get_read_group ) ( self, & err, spec );

        // check for errors
        err . Check ();

        return ReadGroupItf :: Cast ( ret );
    }

    ReferenceItf * ReadCollectionItf :: getReferences () const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_references != 0 );
        NGS_Reference_v1 * ret  = ( * vt -> get_references ) ( self, & err );

        // check for errors
        err . Check ();

        return ReferenceItf :: Cast ( ret );
    }

    bool ReadCollectionItf :: hasReference ( const char * spec ) const
        NGS_NOTHROW ()
    {
        try
        {
            // the object is really from C
            const NGS_ReadCollection_v1 * self = Test ();

            // cast vtable to our level
            const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

            // test for v1.1
            if ( vt -> dad . minor_version < 1 )
            {
                ReferenceItf * itf = getReference ( spec );
                if ( itf == 0 )
                    return false;

                itf -> Release ();
                return true;
            }

            // call through C vtable
            assert ( vt -> has_reference != 0 );
            return ( * vt -> has_reference ) ( self, spec );
        }
        catch ( ... )
        {
        }

        return false;
    }

    ReferenceItf * ReadCollectionItf :: getReference ( const char * spec ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_reference != 0 );
        NGS_Reference_v1 * ret  = ( * vt -> get_reference ) ( self, & err, spec );

        // check for errors
        err . Check ();

        return ReferenceItf :: Cast ( ret );
    }

    AlignmentItf * ReadCollectionItf :: getAlignment ( const char * alignmentId ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_alignment != 0 );
        NGS_Alignment_v1 * ret  = ( * vt -> get_alignment ) ( self, & err, alignmentId );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }

    AlignmentItf * ReadCollectionItf :: getAlignments ( uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_alignments != 0 );
        bool wants_primary = ( categories & Alignment :: primaryAlignment );
        bool wants_secondary
            = ( categories & Alignment :: secondaryAlignment ) != 0;
        NGS_Alignment_v1 * ret  = ( * vt -> get_alignments ) ( self, & err, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }

    uint64_t ReadCollectionItf :: getAlignmentCount ( uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_count != 0 );
        bool wants_primary = ( categories & Alignment :: primaryAlignment );
        bool wants_secondary
            = ( categories & Alignment :: secondaryAlignment ) != 0;
        uint64_t ret  = ( * vt -> get_align_count ) ( self, & err, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return ret;
    }

    AlignmentItf * ReadCollectionItf :: getAlignmentRange ( uint64_t first, uint64_t count, uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_align_range != 0 );
        bool wants_primary = ( categories & Alignment :: primaryAlignment );
        bool wants_secondary
            = ( categories & Alignment :: secondaryAlignment ) != 0;
        NGS_Alignment_v1 * ret  = ( * vt -> get_align_range ) ( self, & err, first, count, wants_primary, wants_secondary );

        // check for errors
        err . Check ();

        return AlignmentItf :: Cast ( ret );
    }

    ReadItf * ReadCollectionItf :: getRead ( const char * readId ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_read != 0 );
        NGS_Read_v1 * ret  = ( * vt -> get_read ) ( self, & err, readId );

        // check for errors
        err . Check ();

        return ReadItf :: Cast ( ret );
    }

    ReadItf * ReadCollectionItf :: getReads ( uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_reads != 0 );
        bool wants_full         = ( categories & Read :: fullyAligned ) != 0;
        bool wants_partial      = ( categories & Read :: partiallyAligned ) != 0;
        bool wants_unaligned    = ( categories & Read :: unaligned ) != 0;
        NGS_Read_v1 * ret  = ( * vt -> get_reads ) ( self, & err, wants_full, wants_partial, wants_unaligned );

        // check for errors
        err . Check ();

        return ReadItf :: Cast ( ret );
    }

    uint64_t ReadCollectionItf :: getReadCount ( uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_read_count != 0 );
        bool wants_full         = ( categories & Read :: fullyAligned ) != 0;
        bool wants_partial      = ( categories & Read :: partiallyAligned ) != 0;
        bool wants_unaligned    = ( categories & Read :: unaligned ) != 0;
        uint64_t ret  = ( * vt -> get_read_count ) ( self, & err, wants_full, wants_partial, wants_unaligned );

        // check for errors
        err . Check ();

        return ret;
    }

    ReadItf * ReadCollectionItf :: getReadRange ( uint64_t first, uint64_t count ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_reads != 0 );
        NGS_Read_v1 * ret  = ( * vt -> get_read_range ) ( self, & err, first, count, true, true, true );

        // check for errors
        err . Check ();

        return ReadItf :: Cast ( ret );
    }

    ReadItf * ReadCollectionItf :: getReadRange ( uint64_t first, uint64_t count, uint32_t categories ) const
        NGS_THROWS ( ErrorMsg )
    {
        // the object is really from C
        const NGS_ReadCollection_v1 * self = Test ();

        // cast vtable to our level
        const NGS_ReadCollection_v1_vt * vt = Access ( self -> vt );

        // call through C vtable
        ErrBlock err;
        assert ( vt -> get_reads != 0 );
        bool wants_full         = ( categories & Read :: fullyAligned ) != 0;
        bool wants_partial      = ( categories & Read :: partiallyAligned ) != 0;
        bool wants_unaligned    = ( categories & Read :: unaligned ) != 0;
        NGS_Read_v1 * ret  = ( * vt -> get_read_range ) ( self, & err, first, count, wants_full, wants_partial, wants_unaligned );

        // check for errors
        err . Check ();

        return ReadItf :: Cast ( ret );
    }


} // namespace ngs
