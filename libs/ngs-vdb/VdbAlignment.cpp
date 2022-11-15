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

#include <ngs-vdb/VdbAlignment.hpp>

#define __mod__     "NGS_VDB"
#define __file__    "VdbAlignment"
#define __fext__    "cpp"
#include <kfc/ctx.h>

#include <kfc/except.h>

#include <ngs/itf/AlignmentItf.hpp>

#include <ncbi/ngs/NGS_Alignment.h>

#include "Error.hpp"

using namespace ncbi :: ngs :: vdb;

VdbAlignment :: VdbAlignment ( const :: ngs :: Alignment & dad ) throw ()
: Alignment ( dad )
{
}

VdbAlignment :: ~ VdbAlignment () throw ()
{
}

class VdbAlignmentItf : public ngs::AlignmentItf
{
public:
    inline NGS_Alignment * Self ()
    {
        return reinterpret_cast<NGS_Alignment*> ( AlignmentItf::Self() );
    }
};

bool
VdbAlignment :: IsFirst () const throw ()
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcArc, rcAccessing );
    bool ret = false;
    THROW_ON_FAIL ( ret = NGS_AlignmentIsFirst ( reinterpret_cast < VdbAlignmentItf * > ( self ) -> Self (), ctx ) );
    return ret;
}

