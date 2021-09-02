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

#ifndef _h_vdb_dump_context_
#define _h_vdb_dump_context_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <kapp/args.h>
#include <klib/num-gen.h>
#include <klib/out.h>

#define OPTION_ROWS         "rows"
#define ALIAS_ROWS          "R"

#define OPTION_COLUMNS      "columns"
#define ALIAS_COLUMNS       "C"

#define OPTION_TABLE        "table"
#define ALIAS_TABLE         "T"

#define OPTION_PROGRESS     "progress"
#define ALIAS_PROGRESS      "p"

#define OPTION_MAXERR       "maxerr"
#define ALIAS_MAXERR        "e"

#define OPTION_INTERSECT    "intersect"
#define ALIAS_INTERSECT     "i"

#define OPTION_EXCLUDE      "exclude"
#define ALIAS_EXCLUDE       "x"

#define OPTION_COLUMNWISE   "col-by-col"
#define ALIAS_COLUMNWISE    "c"

struct diff_ctx
{
    const char * src1;
    const char * src2;
	const char * columns;
	const char * excluded;
	const char * table;
	
    struct num_gen * rows;
	uint32_t max_err;
	bool show_progress;
	bool intersect;
    bool columnwise;
};

void init_diff_ctx( struct diff_ctx * dctx );
void release_diff_ctx( struct diff_ctx * dctx );
rc_t gather_diff_ctx( struct diff_ctx * dctx, Args * args );
rc_t report_diff_ctx( struct diff_ctx * dctx );

#ifdef __cplusplus
}
#endif

#endif
