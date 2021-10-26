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

#ifndef _h_csra1_readcollection_
#define _h_csra1_readcollection_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_Cursor;
struct CSRA1_ReadCollection;

struct CSRA1_ReadCollection * CSRA1_ReadCollectionDuplicate ( struct CSRA1_ReadCollection * self, ctx_t ctx );
void CSRA1_ReadCollectionRelease ( struct CSRA1_ReadCollection * self, ctx_t ctx );
struct NGS_ReadCollection * CSRA1_ReadCollectionToNGS_ReadCollection ( struct CSRA1_ReadCollection * self, ctx_t ctx );

const struct NGS_Cursor* CSRA1_ReadCollectionMakeAlignmentCursor ( struct CSRA1_ReadCollection * self, 
                                                                   ctx_t ctx, 
                                                                   bool primary, 
                                                                   bool exclusive );


#ifdef __cplusplus
}
#endif

#endif /* _h_csra1_readcollection_ */
