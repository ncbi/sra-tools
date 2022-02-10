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

#ifndef _h_sra_readgroupinfo_
#define _h_sra_readgroupinfo_

#ifndef _h_kfc_defs_
#include <kfc/defs.h>
#endif

#ifndef _h_klib_refcount_
#include <klib/refcount.h>
#endif

typedef struct SRA_ReadGroupInfo SRA_ReadGroupInfo;

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
 
struct VTable;
struct NGS_String;

/*--------------------------------------------------------------------------
 * SRA_ReadGroup
 */
 
struct SRA_ReadGroupStats
{ 
    const struct NGS_String* name;
    uint64_t min_row; 
    uint64_t max_row; 
    uint64_t row_count; 
    uint64_t base_count; 
    uint64_t bio_base_count; 
    
    /* BAM header info */
    const struct NGS_String* bam_LB; /* Library. */
    const struct NGS_String* bam_SM; /* Sample. */
};
 
struct SRA_ReadGroupInfo 
{
    KRefcount refcount;
    
    uint32_t count;
    struct SRA_ReadGroupStats groups[1]; /* actual size = count */
};

const SRA_ReadGroupInfo* SRA_ReadGroupInfoMake ( ctx_t ctx, const struct VTable* table );

const SRA_ReadGroupInfo* SRA_ReadGroupInfoDuplicate ( const SRA_ReadGroupInfo* self, ctx_t ctx );

void SRA_ReadGroupInfoRelease ( const SRA_ReadGroupInfo* self, ctx_t ctx );

uint32_t SRA_ReadGroupInfoFind ( const SRA_ReadGroupInfo* self, ctx_t ctx, char const* name, size_t name_size );

#ifdef __cplusplus
}
#endif

#endif /* _h_sra_readgroupinfo_ */
