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

#ifndef _h_sra_statistics_
#define _h_sra_statistics_

typedef struct SRA_Statistics SRA_Statistics;
#ifndef _h_ngs_statistics_
#define NGS_STATISTICS SRA_Statistics
#include "NGS_Statistics.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */

struct VTable;
struct VDatabase;
 
/*--------------------------------------------------------------------------
 * SRA_Statistics
 */

NGS_Statistics * SRA_StatisticsMake ( ctx_t ctx ); 

void SRA_StatisticsLoadTableStats ( NGS_Statistics * self, ctx_t ctx, const struct VTable* tbl, const char* prefix );

void SRA_StatisticsLoadBamHeader ( NGS_Statistics * self, ctx_t ctx, const struct VDatabase * db );

#ifdef __cplusplus
}
#endif

#endif /* _h_sra_statistics_ */
