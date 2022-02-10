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

#ifndef _h_sra_readgroup_
#define _h_sra_readgroup_

typedef struct SRA_ReadGroup SRA_ReadGroup;
#ifndef _h_ngs_readgroup_
#define NGS_READGROUP SRA_ReadGroup
#include "NGS_ReadGroup.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct NGS_Cursor;
struct SRA_ReadGroupInfo;
struct NGS_String;

/*--------------------------------------------------------------------------
 * SRA_ReadGroup
 */

/* Make
    run_name : will be used for unnamed group if name == NULL
    group_name : NULL OK, will locate unnamed group and spec as the group's name
 */
struct NGS_ReadGroup * SRA_ReadGroupMake ( ctx_t ctx, 
                                                 const struct NGS_Cursor * curs, 
                                                 const struct SRA_ReadGroupInfo* group_info, 
                                                 const struct NGS_String * run_name,
                                                 const char * group_name, size_t group_name_size ); 

/* IteratorMake
    run_name : will be used for unnamed group
 */
struct NGS_ReadGroup * SRA_ReadGroupIteratorMake ( ctx_t ctx, 
                                                         const struct NGS_Cursor * curs, 
                                                         const struct SRA_ReadGroupInfo* group_info, 
                                                         const struct NGS_String * run_name ); 

#ifdef __cplusplus
}
#endif

#endif /* _h_sra_readgroup_ */
