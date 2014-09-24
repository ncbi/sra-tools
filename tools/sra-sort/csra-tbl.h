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

#ifndef _h_sra_sort_csra_tbl_
#define _h_sra_sort_csra_tbl_

#ifndef _h_sra_sort_tbl_pair_
#include "tbl-pair.h"
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct cSRAPair;
struct RowSetIterator;


/*--------------------------------------------------------------------------
 * cSRATblPair
 *  interface to pairing of source and destination tables
 */
typedef struct cSRATblPair cSRATblPair;
struct cSRATblPair
{
    TablePair dad;

    /* database */
    struct cSRAPair *csra;

    /* special rowset iterator */
    struct RowSetIterator *rsi;

    /* if an alignment table, which one? */
    uint32_t align_idx;
};


/* MakeRef
 *  special cased for REFERENCE table
 */
TablePair *cSRATblPairMakeRef ( struct DbPair *self, const ctx_t *ctx,
    struct VTable const *src, struct VTable *dst, const char *name, bool reorder );


/* MakeAlign
 *  special cased for *_ALIGNMENT tables
 */
TablePair *cSRATblPairMakeAlign ( struct DbPair *self, const ctx_t *ctx,
    struct VTable const *src, struct VTable *dst, const char *name, bool reorder );


/* MakeSeq
 *  special cased for SEQUENCE table
 */
TablePair *cSRATblPairMakeSeq ( struct DbPair *self, const ctx_t *ctx,
    struct VTable const *src, struct VTable *dst, const char *name, bool reorder );


#endif /* _h_sra_sort_csra_tbl_ */
