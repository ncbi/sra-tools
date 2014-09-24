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

#ifndef _h_kqsh_tok_
#define _h_kqsh_tok_

#ifndef _h_klib_token_
#include <klib/token.h>
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif



/*--------------------------------------------------------------------------
 * forwards
 */
struct KSymTable;


/*--------------------------------------------------------------------------
 * kqsh tokens
 */
enum kqsh_tokens
{
    /* alphabetical list of commands */
    kw_add = eNumSymtabIDs,
    kw_alias,
    kw_alter,
    kw_as,
    kw_at,
    kw_close,
    kw_column,
    kw_columns,
    kw_commands,
    kw_compact,
    kw_constants,
    kw_create,
    kw_cursor,
    kw_database,
    kw_databases,
    kw_delete,
    kw_drop,
    kw_execute,
    kw_exit,
    kw_for,
    kw_formats,
    kw_functions,
    kw_help,
    kw_include,
    kw_initialize,
    kw_insert,
    kw_kdb,
    kw_library,
    kw_load,
    kw_manager,
    kw_metadata,
    kw_objects,
    kw_on,
    kw_open,
    kw_or,
    kw_path,
    kw_quit,
    kw_rename,
    kw_replace,
    kw_row,
    kw_schema,
    kw_show,
    kw_sra,
    kw_table,
    kw_tables,
    kw_text,
    kw_types,
    kw_typesets,
    kw_update,
    kw_use,
    kw_using,
    kw_version,
    kw_vdb,
    kw_with,
    kw_write,

    rsrv_first,
    /* reserved names */
    rsrv_kmgr = rsrv_first,
    rsrv_vmgr,
    rsrv_sramgr,
    rsrv_srapath,

    obj_first,
    /* object types */
    obj_KDBManager = obj_first,
    obj_VDBManager,
    obj_SRAManager,
    obj_VSchema,
    obj_KTable,
    obj_VTable,
    obj_VCursor,

    /* accepted abbreviations */
    kw_col = kw_column,
    kw_cols = kw_columns,
    kw_db = kw_database,
    kw_dbs = kw_databases,
    kw_exec = kw_execute,
    kw_init = kw_initialize,
    kw_lib = kw_library,
    kw_mgr = kw_manager,
    kw_tbl = kw_table
};

/* next_token
 */
KToken *next_token ( struct KSymTable const *tbl, KTokenSource *src, KToken *t );
KToken *next_shallow_token ( struct KSymTable const *tbl, KTokenSource *src, KToken *t );

/* expected
 */
rc_t expected ( const KToken *self, KLogLevel lvl, const char *expected );


#ifdef __cplusplus
}
#endif

#endif /* _h_kqsh_tok_ */
