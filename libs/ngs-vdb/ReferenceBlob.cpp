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

#include <ngs-vdb/ReferenceBlob.hpp>

#define __mod__     "NGS_VDB"
#define __file__    "ReferenceBlob"
#define __fext__    "cpp"
#include <kfc/ctx.h>

#include <kfc/except.h>

#include <ncbi/ngs/NGS_ReferenceBlob.h>
#include <ncbi/ngs/NGS_String.h>

#include "Error.hpp"

using namespace ncbi :: ngs :: vdb;

ReferenceBlob :: ReferenceBlob ( ReferenceBlobRef ref ) NGS_NOTHROW ()
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( self = NGS_ReferenceBlobDuplicate ( ref, ctx ) );
}

ReferenceBlob &
ReferenceBlob :: operator = ( const ReferenceBlob & obj )
{
    if ( &obj != this )
    {
        HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
        THROW_ON_FAIL ( NGS_ReferenceBlobRelease ( self, ctx ) );
        THROW_ON_FAIL ( self = NGS_ReferenceBlobDuplicate ( obj . self, ctx ) );
    }
    return *this;
}

ReferenceBlob :: ReferenceBlob ( const ReferenceBlob & obj )
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( self = NGS_ReferenceBlobDuplicate ( obj . self, ctx ) );
}

ReferenceBlob :: ~ ReferenceBlob () NGS_NOTHROW()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    ON_FAIL ( NGS_ReferenceBlobRelease ( self, ctx) )
    {
        CLEAR ();
    }
}

const char*
ReferenceBlob :: Data() const NGS_NOTHROW()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    const char * ret;
    THROW_ON_FAIL ( ret = ( const char * ) NGS_ReferenceBlobData ( self, ctx ) );
    return ret;
}

uint64_t
ReferenceBlob :: Size() const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    uint64_t ret;
    THROW_ON_FAIL ( ret = NGS_ReferenceBlobSize ( self, ctx ) );
    return ret;
}

uint64_t
ReferenceBlob :: UnpackedSize() const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    uint64_t ret;
    THROW_ON_FAIL ( ret = NGS_ReferenceBlobUnpackedSize ( self, ctx ) );
    return ret;
}

void
ReferenceBlob :: GetRowRange ( int64_t * p_first, uint64_t * p_count ) const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( NGS_ReferenceBlobRowRange ( self, ctx,  p_first, p_count ) );
}

void
ReferenceBlob :: ResolveOffset ( uint64_t p_inBlob, uint64_t * p_inReference, uint32_t * p_repeatCount, uint64_t * p_increment ) const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( NGS_ReferenceBlobResolveOffset ( self, ctx, p_inBlob, p_inReference, p_repeatCount, p_increment ) );
}
