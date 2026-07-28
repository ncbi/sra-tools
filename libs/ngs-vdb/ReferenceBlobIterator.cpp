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

#include <ngs-vdb/ReferenceBlobIterator.hpp>

#define __mod__     "NGS_VDB"
#define __file__    "ReferenceBlobIterator"
#define __fext__    "cpp"
#include <kfc/ctx.h>

#include <kfc/except.h>

#include <klib/log.h>

#include <ncbi/ngs/NGS_ReferenceBlobIterator.h>
#include <ncbi/ngs/NGS_ReferenceBlob.h>
#include <ncbi/ngs/NGS_ErrBlock.h>

#include "Error.hpp"

using namespace ncbi :: ngs :: vdb;

ReferenceBlobIterator :: ReferenceBlobIterator ( ReferenceBlobIteratorRef ref ) NGS_NOTHROW()
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( self = NGS_ReferenceBlobIteratorDuplicate ( ref, ctx) );
}

ReferenceBlobIterator &
ReferenceBlobIterator :: operator = ( const ReferenceBlobIterator & obj )
{
    if ( &obj != this)
    {
        HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
        THROW_ON_FAIL ( NGS_ReferenceBlobIteratorRelease ( self, ctx) );
        THROW_ON_FAIL ( self = NGS_ReferenceBlobIteratorDuplicate ( obj . self, ctx) );
    }
    return *this;
}

ReferenceBlobIterator :: ReferenceBlobIterator ( const ReferenceBlobIterator & obj )
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( self = NGS_ReferenceBlobIteratorDuplicate ( obj . self, ctx) );
}

ReferenceBlobIterator :: ~ ReferenceBlobIterator () NGS_NOTHROW()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    ON_FAIL ( NGS_ReferenceBlobIteratorRelease ( self, ctx) )
    {
        CLEAR ();
    }
}

bool
ReferenceBlobIterator :: hasMore() const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    bool ret = false;
    THROW_ON_FAIL ( ret = NGS_ReferenceBlobIteratorHasMore ( self, ctx ) );
    return ret;
}

ReferenceBlob
ReferenceBlobIterator :: nextBlob()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    NGS_ReferenceBlob* blob;
    THROW_ON_FAIL ( blob = NGS_ReferenceBlobIteratorNext ( self, ctx ) );
    if ( blob == 0 )
    {
        throw :: ngs :: ErrorMsg( "No more blobs" );
    }
    ReferenceBlob ret ( blob );
    THROW_ON_FAIL ( NGS_ReferenceBlobRelease ( blob, ctx ) )
    return ret;

}
