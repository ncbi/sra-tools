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

#ifndef _h_sra_sort_buff_writer_
#define _h_sra_sort_buff_writer_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct MapFile;
struct cSRATblPair;
struct ColumnWriter;


/*--------------------------------------------------------------------------
 * cSRATblPair
 *  interface to pairing of source and destination tables
 */

/* MakeBufferedColumnWriter
 *  make a wrapper that buffers all rows written
 *  then sorts on flush
 */
struct ColumnWriter *cSRATblPairMakeBufferedColumnWriter ( struct cSRATblPair *self,
    const ctx_t *ctx, struct ColumnWriter *writer );

/* MakeBufferedIdRemapColumnWriter
 *  make a wrapper that buffers all rows written
 *  maps all ids through index MapFile
 *  then sorts on flush
 */
struct ColumnWriter *cSRATblPairMakeBufferedIdRemapColumnWriter ( struct cSRATblPair *self,
    const ctx_t *ctx, struct ColumnWriter *writer, struct MapFile *idx, bool assign_ids );


#endif /* _h_sra_sort_buff_writer_ */
