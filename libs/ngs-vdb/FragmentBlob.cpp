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

#include <ngs-vdb/FragmentBlob.hpp>

#define __mod__     "NGS_VDB"
#define __file__    "FragmentBlob"
#define __fext__    "cpp"
#include <kfc/ctx.h>

#include <kfc/except.h>

#include <ncbi/ngs/NGS_FragmentBlob.h>
#include <ncbi/ngs/NGS_Id.h>
#include <ncbi/ngs/NGS_ErrBlock.h>
#include <ncbi/ngs/NGS_String.h>
#include <ncbi/ngs/NGS_Id.h>

#include "Error.hpp"

using namespace std;
using namespace ncbi :: ngs :: vdb;

FragmentBlob :: FragmentBlob ( FragmentBlobRef ref ) NGS_NOTHROW ()
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( self = NGS_FragmentBlobDuplicate ( ref, ctx) );
}

FragmentBlob &
FragmentBlob :: operator = ( const FragmentBlob & obj )
{
    if ( &obj != this )
    {
        HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
        THROW_ON_FAIL ( NGS_FragmentBlobRelease ( self, ctx) )
        THROW_ON_FAIL ( self = NGS_FragmentBlobDuplicate ( obj . self, ctx) );
    }
    return *this;
}

FragmentBlob :: FragmentBlob ( const FragmentBlob & obj )
: self ( 0 )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    self = NGS_FragmentBlobDuplicate ( obj . self, ctx);
}

FragmentBlob :: ~ FragmentBlob () NGS_NOTHROW()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    ON_FAIL ( NGS_FragmentBlobRelease ( self, ctx) )
    {
        CLEAR ();
    }
}

const char*
FragmentBlob :: Data() const NGS_NOTHROW()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    const char* ret;
    THROW_ON_FAIL ( ret = ( const char * ) NGS_FragmentBlobData ( self, ctx ) );
    return ret;
}

uint64_t
FragmentBlob :: Size() const NGS_NOTHROW()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    uint64_t ret;
    THROW_ON_FAIL ( ret = NGS_FragmentBlobSize ( self, ctx ) )
    return ret;
}

void
FragmentBlob :: GetFragmentInfo ( uint64_t p_offset, string * p_fragId, uint64_t * p_startInBlob, uint64_t * p_lengthInBases, bool * p_biological ) const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    // outputs
    string      fragId;
    uint64_t    startInBlob;
    uint64_t    lengthInBases;
    bool        biological;

    int64_t rowId;
    int32_t fragNum;
    THROW_ON_FAIL ( NGS_FragmentBlobInfoByOffset ( self, ctx, p_offset, & rowId, & startInBlob, & lengthInBases, & fragNum ) );
    if ( fragNum >= 0 )
    {
        THROW_ON_FAIL ( const NGS_String * run = NGS_FragmentBlobRun ( self, ctx ) );
        THROW_ON_FAIL ( const NGS_String* readId = NGS_IdMakeFragment ( ctx, run, false, rowId, fragNum ) );
        THROW_ON_FAIL ( fragId = string ( NGS_StringData ( readId, ctx ), NGS_StringSize ( readId, ctx ) ) );
        THROW_ON_FAIL ( NGS_StringRelease ( readId, ctx ) );
        biological = true;
    }
    else
    {
        biological = false;
    }

    if ( p_fragId != 0 )
    {
        * p_fragId = fragId;
    }
    if ( p_startInBlob != 0 )
    {
        * p_startInBlob = startInBlob;
    }
    if ( p_lengthInBases != 0 )
    {
        * p_lengthInBases = lengthInBases;
    }
    if ( p_biological != 0 )
    {
        * p_biological = biological;
    }
}

void
FragmentBlob :: GetRowRange ( int64_t * first, uint64_t * count ) const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    THROW_ON_FAIL ( NGS_FragmentBlobRowRange ( self, ctx, first, count ) );
}
