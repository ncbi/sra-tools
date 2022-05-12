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

#include <ngs-vdb/FragmentBlobIterator.hpp>

#define __mod__     "NGS_VDB"
#define __file__    "FragmentBlobIterator"
#define __fext__    "cpp"
#include <kfc/ctx.h>

#include <kfc/except.h>

#include <ncbi/ngs/NGS_FragmentBlobIterator.h>
#include <ncbi/ngs/NGS_FragmentBlob.h>

#include "Error.hpp"

using namespace ncbi :: ngs :: vdb;

FragmentBlobIterator :: FragmentBlobIterator ( FragmentBlobIteratorRef ref ) NGS_NOTHROW()
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( self = NGS_FragmentBlobIteratorDuplicate ( ref, ctx) );
}

FragmentBlobIterator &
FragmentBlobIterator :: operator = ( const FragmentBlobIterator & obj )
{
    if ( &obj != this )
    {
        HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
        THROW_ON_FAIL ( NGS_FragmentBlobIteratorRelease ( self, ctx) );
        THROW_ON_FAIL ( self = NGS_FragmentBlobIteratorDuplicate ( obj . self, ctx) );
    }
    return *this;
}

FragmentBlobIterator :: FragmentBlobIterator ( const FragmentBlobIterator & obj )
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( self = NGS_FragmentBlobIteratorDuplicate ( obj . self, ctx) )
}

FragmentBlobIterator :: ~ FragmentBlobIterator () NGS_NOTHROW()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    ON_FAIL ( NGS_FragmentBlobIteratorRelease ( self, ctx) )
    {
        CLEAR ();
    }
}

bool
FragmentBlobIterator :: hasMore() const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    bool ret;
    THROW_ON_FAIL ( ret = NGS_FragmentBlobIteratorHasMore ( self, ctx ) );
    return ret;
}

FragmentBlob
FragmentBlobIterator :: nextBlob()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    NGS_FragmentBlob* blob;
    THROW_ON_FAIL ( blob = NGS_FragmentBlobIteratorNext ( self, ctx ) );
    if ( blob == 0 )
    {
        throw :: ngs :: ErrorMsg( "No more blobs" );
    }
    FragmentBlob ret ( blob );
    THROW_ON_FAIL ( NGS_FragmentBlobRelease ( blob, ctx ) );
    return ret;
}
