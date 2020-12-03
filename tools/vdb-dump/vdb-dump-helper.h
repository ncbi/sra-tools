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

#ifndef _h_vdb_dump_helper_
#define _h_vdb_dump_helper_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <klib/out.h>
#include <klib/rc.h>
#include <klib/vector.h>
#include <klib/text.h>

#include <vfs/manager.h>
#include <vfs/path.h>
#include <vfs/resolver.h>

#include <vdb/manager.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <vdb/database.h>
#include <vdb/schema.h>

#include <sra/sraschema.h>

#include "vdb-dump-context.h"
#include "vdb-dump-coldefs.h"

#define DISP_RC(rc,err) (void)((rc == 0) ? 0 : LOGERR( klogInt, rc, err ))

#define DISP_RC2(rc,err,succ) \
    (void)((rc != 0)? 0 : (succ) ? LOGMSG( klogInfo, succ ) : LOGERR( klogInt, rc, err ))

rc_t ErrMsg( const char * fmt, ... );

rc_t vdh_show_manager_version( const VDBManager *my_manager );

rc_t vdh_parse_schema( const VDBManager *my_manager,
                       VSchema **new_schema,
                       Vector *schema_list,
                       bool with_sra_schema );

bool vdh_is_path_table( const VDBManager *my_manager, const char *path,
                        Vector *schema_list );
bool vdh_is_path_column( const VDBManager *my_manager, const char *path,
                         Vector *schema_list );
bool vdh_is_path_database( const VDBManager *my_manager, const char *path,
                           Vector *schema_list );

bool list_contains_value( const KNamelist * list, const String * value );

bool vdh_take_1st_table_from_db( dump_context *ctx, const KNamelist *tbl_names );

bool vdh_take_this_table_from_list( dump_context *ctx, const KNamelist *tbl_names,
                                    const char * table_to_find );

bool vdh_take_this_table_from_db( dump_context *ctx, const VDatabase *db,
                                  const char * table_to_find );

rc_t vdh_print_col_info( dump_context *ctx,
                         const p_col_def col_def,
                         const VSchema *my_schema );

rc_t resolve_remote_accession( const char * accession, char * dst, size_t dst_size );
rc_t resolve_accession( const char * accession, char * dst, size_t dst_size, bool remotely );
rc_t resolve_cache( const char * accession, char * dst, size_t dst_size );
rc_t check_cache_comleteness( const char * path, float * percent, uint64_t * bytes_in_cache );

int32_t index_of_match( const String * word, uint32_t num, ... );
void destroy_String_vector( Vector * v );
uint32_t copy_String_2_vector( Vector * v, const String * S );
uint32_t split_buffer( Vector * v, const String * S, const char * delim );

rc_t vdh_path_to_vpath( const char * path, VPath ** vpath );

#ifdef __cplusplus
}
#endif

#endif
