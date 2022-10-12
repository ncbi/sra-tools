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

#ifndef _h_line_policy_
#define _h_line_policy_

#include <tui/tui.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef struct lp_context
{
    char * line;
    uint64_t len, max_len, visible;
    uint64_t * cur_pos;
    uint64_t * offset;
    uint64_t * ins_mode;
    uint64_t * alpha_mode;
    bool content_changed;
} lp_context;


bool lp_handle_event( lp_context * lp, tui_event * event );


typedef struct gen_context
{
    uint64_t count, visible;
    uint64_t * curr;
    uint64_t * offset;
} gen_context;


bool list_handle_event( gen_context * ctx, tui_event * event, uint32_t x_max );


typedef uint32_t ( CC * grid_ctx_get_width ) ( uint64_t col, void * data );
typedef void ( CC * grid_ctx_set_width ) ( uint64_t col, uint32_t value, void * data );

typedef struct grid_context
{
    void * data;
    grid_ctx_get_width on_get_width;
    grid_ctx_set_width on_set_width;

    gen_context row;
    gen_context col;
} grid_context;


bool grid_handle_event( grid_context * gp, tui_event * event );

bool grid_handle_set_col( grid_context * gp, uint64_t col );
bool grid_handle_set_row( grid_context * gp, uint64_t row );


#ifdef __cplusplus
}
#endif

#endif /* _h_line_policy_ */
