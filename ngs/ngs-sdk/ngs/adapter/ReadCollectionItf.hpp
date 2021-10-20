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

#ifndef _hpp_ngs_adapt_read_collectionitf_
#define _hpp_ngs_adapt_read_collectionitf_

#ifndef _hpp_ngs_adapt_refcount_
#include <ngs/adapter/Refcount.hpp>
#endif

#ifndef _h_ngs_itf_read_collectionitf_
#include <ngs/itf/ReadCollectionItf.h>
#endif

namespace ngs_adapt
{

    /*----------------------------------------------------------------------
     * forwards
     */
    class ReadItf;
    class StringItf;
    class ReadGroupItf;
    class ReferenceItf;
    class AlignmentItf;

    /*----------------------------------------------------------------------
     * ReadCollectionItf
     */
    class ReadCollectionItf : public Refcount < ReadCollectionItf, NGS_ReadCollection_v1 >
    {
    public:
        
        virtual StringItf * getName () const = 0;
        virtual ReadGroupItf * getReadGroups () const = 0;
        virtual bool hasReadGroup ( const char * spec ) const = 0;
        virtual ReadGroupItf * getReadGroup ( const char * spec ) const = 0;
        virtual ReferenceItf * getReferences () const = 0;
        virtual bool hasReference ( const char * spec ) const = 0;
        virtual ReferenceItf * getReference ( const char * spec ) const = 0;
        virtual AlignmentItf * getAlignment ( const char * alignmentId ) const = 0;
        virtual AlignmentItf * getAlignments ( bool wants_primary, bool wants_secondary ) const = 0;
        virtual uint64_t getAlignmentCount ( bool wants_primary, bool wants_secondary ) const = 0;
        virtual AlignmentItf * getAlignmentRange ( uint64_t first, uint64_t count, bool wants_primary, bool wants_secondary ) const = 0;
        virtual ReadItf * getRead ( const char * readId ) const = 0;
        virtual ReadItf * getReads ( bool wants_full, bool wants_partial, bool wants_unaligned ) const = 0;
        virtual uint64_t getReadCount ( bool wants_full, bool wants_partial, bool wants_unaligned ) const = 0;
        virtual ReadItf * getReadRange ( uint64_t first, uint64_t count, bool wants_full, bool wants_partial, bool wants_unaligned ) const = 0;

    protected:

        ReadCollectionItf ();
        static NGS_ReadCollection_v1_vt ivt;

    private:

        static NGS_String_v1 * CC get_name ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err );
        static NGS_ReadGroup_v1 * CC get_read_groups ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC has_read_group ( const NGS_ReadCollection_v1 * self, const char * spec );
        static NGS_ReadGroup_v1 * CC get_read_group ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            const char * spec );
        static NGS_Reference_v1 * CC get_references ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err );
        static bool CC has_reference ( const NGS_ReadCollection_v1 * self, const char * spec );
        static NGS_Reference_v1 * CC get_reference ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            const char * spec );
        static NGS_Alignment_v1 * CC get_alignment ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            const char * alignmentId );
        static NGS_Alignment_v1 * CC get_alignments ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary );
        static uint64_t CC get_align_count ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            bool wants_primary, bool wants_secondary );
        static NGS_Alignment_v1 * CC get_align_range ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t first, uint64_t count, bool wants_primary, bool wants_secondary );
        static NGS_Read_v1 * CC get_read ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            const char * readId );
        static NGS_Read_v1 * CC get_reads ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            bool wants_full, bool wants_partial, bool wants_unaligned );
        static uint64_t CC get_read_count ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            bool wants_full, bool wants_partial, bool wants_unaligned );
        static NGS_Read_v1 * CC get_read_range ( const NGS_ReadCollection_v1 * self, NGS_ErrBlock_v1 * err,
            uint64_t first, uint64_t count, bool wants_full, bool wants_partial, bool wants_unaligned );

    };

} // namespace ngs_adapt

#endif // _hpp_ngs_adapt_read_collectionitf_
