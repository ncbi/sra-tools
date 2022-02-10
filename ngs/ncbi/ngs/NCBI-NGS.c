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

#include "NCBI-NGS.h"
#include "NGS_ErrBlock.h"
#include "NGS_ReadCollection.h"
#include "NGS_ReferenceSequence.h"

#include <kfc/ctx.h>
#include <kfc/rsrc.h>
#include <kfc/except.h>
#include <kfc/xc.h>

/*--------------------------------------------------------------------------
 * NCBI NGS engine
 *  link against ncbi-vdb library
 */

LIB_EXPORT struct NGS_ReadCollection_v1 * NCBI_NGS_OpenReadCollection ( const char * spec, struct NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcDatabase, rcOpening );

    ON_FAIL ( NGS_ReadCollection * ret = NGS_ReadCollectionMake ( ctx, spec ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_ReadCollection_v1 * ) ret;
}

LIB_EXPORT struct NGS_ReferenceSequence_v1 * NCBI_NGS_OpenReferenceSequence ( const char * spec, struct NGS_ErrBlock_v1 * err )
{
    HYBRID_FUNC_ENTRY ( rcSRA, rcTable, rcOpening );

    ON_FAIL ( NGS_ReferenceSequence * ret = NGS_ReferenceSequenceMake ( ctx, spec ) )
    {
        NGS_ErrBlockThrow ( err, ctx );
    }

    CLEAR ();
    return ( struct NGS_ReferenceSequence_v1 * ) ret;
}
