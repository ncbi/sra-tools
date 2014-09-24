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

#ifndef _h_sra_sort_caps_
#define _h_sra_sort_caps_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct MemBank;
struct KConfig;
struct KDBManager;
struct VDBManager;
struct Tool;


/*--------------------------------------------------------------------------
 * Caps
 *  a very watered-down version of vdb-3 capabilities
 */
typedef struct Caps Caps;
struct Caps
{
    struct MemBank *mem;
    struct KConfig const *cfg;
    struct KDBManager *kdb;
    struct VDBManager *vdb;
    struct Tool const *tool;
};


/* Init
 *  initialize a local block from another
 */
void CapsInit ( Caps *caps, const ctx_t *ctx );


/* Whack
 *  release references
 */
void CapsWhack ( Caps *self, const ctx_t *ctx );

#endif
