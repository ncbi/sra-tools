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

#ifndef _h_sra_sort_row_set_priv_
#define _h_sra_sort_row_set_priv_

#ifndef _h_sra_sort_row_set_
#include "row-set.h"
#endif


/*--------------------------------------------------------------------------
 * RowSetIterator
 *  the basic 4 types
 */
RowSetIterator * MapFileMakeMappingRowSetIterator ( struct MapFile const *self, const ctx_t *ctx, bool large );
RowSetIterator * MapFileMakeSortingRowSetIterator ( struct MapFile const *self, const ctx_t *ctx, bool large );
RowSetIterator * TablePairMakeMappingRowSetIterator ( struct TablePair *self, const ctx_t *ctx, bool large );
RowSetIterator * TablePairMakeSimpleRowSetIterator ( struct TablePair *self, const ctx_t *ctx );

#endif /* _h_sra_sort_row_set_priv_ */
