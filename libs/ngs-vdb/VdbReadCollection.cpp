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

#include <ngs-vdb/VdbReadCollection.hpp>

#define __mod__     "NGS_VDB"
#define __file__    "VdbReadCollection"
#define __fext__    "cpp"
#include <kfc/ctx.h>

#include <kfc/except.h>

#include <ncbi/ngs/NGS_ReadCollection.h>
#include <ncbi/ngs/NGS_FragmentBlobIterator.h>

#include "Error.hpp"

using namespace ncbi :: ngs :: vdb;

VdbReadCollection :: VdbReadCollection ( const :: ngs :: ReadCollection & dad ) throw ()
: :: ngs :: ReadCollection ( dad )
{
}

VdbReadCollection &
VdbReadCollection :: operator = ( const VdbReadCollection & obj ) throw ()
{
    :: ngs :: ReadCollection :: operator = ( obj );
    return *this;
}

VdbReadCollection :: VdbReadCollection ( const VdbReadCollection & obj ) throw ()
: :: ngs :: ReadCollection ( obj )
{
}

VdbReadCollection :: ~ VdbReadCollection () throw ()
{
}

class VdbReadCollectionItf : public ngs::ReadCollectionItf
{
public:
    inline NGS_ReadCollection * Self ()
    {
        return reinterpret_cast<NGS_ReadCollection*> ( ReadCollectionItf::Self() );
    }
};

FragmentBlobIterator
VdbReadCollection :: getFragmentBlobs() const
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );

    THROW_ON_FAIL ( struct NGS_FragmentBlobIterator* iter = NGS_ReadCollectionGetFragmentBlobs ( reinterpret_cast<VdbReadCollectionItf*>(self) -> Self() , ctx ) );
    FragmentBlobIterator ret ( iter );
    THROW_ON_FAIL ( NGS_FragmentBlobIteratorRelease ( iter, ctx ) );
    return ret;
}
