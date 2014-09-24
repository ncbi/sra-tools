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

#ifndef _h_columns_
#define _h_columns_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <vdb/cursor.h>

typedef struct col
{
    uint32_t idx;
    uint32_t elem_bits;
    uint32_t bit_offset;
    uint32_t row_len;
    const void * base;
} col;


rc_t add_column( const VCursor *cursor, col *column, const char *name );

rc_t add_columns( const VCursor *cursor,
                  col *columns, const char **names, uint32_t n_columns );

rc_t read_cell( const VCursor *my_cursor,
                int64_t row_id,
                col *column,
                const char * name );

rc_t read_cells( const VCursor *my_cursor, int64_t row_id,
                 col *columns, const char **names, uint32_t n_columns );

rc_t write_to_cursor( VCursor * cursor, uint32_t idx, uint32_t bsize,
                      void * src, uint32_t count, const char * name );

#ifdef __cplusplus
}
#endif

#endif
