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

#ifndef _h_vdb_dump_row_context_
#define _h_vdb_dump_row_context_

#include <vdb/cursor.h>
#include <klib/vector.h>

#include "vdb-dump-context.h"
#include "vdb-dump-coldefs.h"
#include "vdb-dump-str.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************************
    binds together everything to dump one row:
        - a pointer to the cursor to read the data-cells
        - a pointer to the column-definitions (Vector of column-definition's)
        - a pointer to the dump-context ( parameters and options for cmd-line )
        - a dump-string (structure not pointer!) to be reused to assemble output
        - a Vector containing p_col_data - pointers
        - a return-type to stop if reading data failed ( neccessary to stop after
          last row if no row-range is given at command-line )

    needed as a (one and only) parameter to VectorForEach
*************************************************************************************/
typedef struct row_context
{
    const VTable* table;
    const VCursor* cursor;
    p_col_defs col_defs;
    p_dump_context ctx;
    dump_str s_col;
    int64_t row_id;
    uint32_t col_nr;
    rc_t rc;
} row_context;
typedef row_context* p_row_context;


#ifdef __cplusplus
}
#endif

#endif
