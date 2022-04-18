/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author'm_s official duties as a United States Government employee and
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

#include <ngs-vdb/VdbReference.hpp>

#define __mod__     "NGS_VDB"
#define __file__    "VdbReference"
#define __fext__    "cpp"
#include <kfc/ctx.h>

#include <kfc/except.h>

#include <ncbi/ngs/NGS_Reference.h>
#include <ncbi/ngs/NGS_ReferenceBlobIterator.h>

#include "Error.hpp"

using namespace ncbi :: ngs :: vdb;

VdbReference :: VdbReference ( const :: ngs :: Reference & dad ) NGS_NOTHROW ()
: :: ngs :: Reference ( dad )
{
}

VdbReference &
VdbReference :: operator = ( const VdbReference & obj ) NGS_NOTHROW()
{
    :: ngs :: Reference :: operator = ( obj );
    return *this;
}

VdbReference :: VdbReference ( const VdbReference & obj ) NGS_NOTHROW()
: :: ngs :: Reference ( obj )
{
}

VdbReference :: ~ VdbReference () NGS_NOTHROW()
{
}

class VdbReferenceItf : public ngs::ReferenceItf
{
public:
    inline NGS_Reference * Self ()
    {
        return reinterpret_cast<NGS_Reference*> ( ReferenceItf::Self() );
    }
};

ReferenceBlobIterator
VdbReference :: getBlobs() const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );

    THROW_ON_FAIL ( struct NGS_ReferenceBlobIterator* iter = NGS_ReferenceGetBlobs ( reinterpret_cast<VdbReferenceItf*>(self) -> Self() , ctx, 0, (uint64_t)-1 ) );
    ReferenceBlobIterator ret ( iter );
    THROW_ON_FAIL ( NGS_ReferenceBlobIteratorRelease ( iter, ctx ) );
    return ret;
}

ReferenceBlobIterator
VdbReference :: getBlobs (uint64_t p_start, uint64_t p_count ) const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );

    THROW_ON_FAIL ( struct NGS_ReferenceBlobIterator* iter = NGS_ReferenceGetBlobs ( reinterpret_cast<VdbReferenceItf*>(self) -> Self() , ctx, p_start, p_count ) );
    ReferenceBlobIterator ret ( iter );
    THROW_ON_FAIL ( NGS_ReferenceBlobIteratorRelease ( iter, ctx ) )
    return ret;
}

