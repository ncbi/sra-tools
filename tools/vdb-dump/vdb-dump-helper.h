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

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_vfs_path_
#include <vfs/path.h>
#endif

#ifndef _h_vdb_manager_
#include <vdb/manager.h>
#endif

#ifndef _h_vdb_table_
#include <vdb/table.h>
#endif

#ifndef _h_vdb_database_
#include <vdb/database.h>
#endif

#ifndef _h_vdb_schema_
#include <vdb/schema.h>
#endif


#include "vdb-dump-context.h"
#include "vdb-dump-coldefs.h"

#define DISP_RC(rc,err) \
    (void)((0 == rc) ? 0 : LOGERR( klogInt, rc, err ))

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

bool vdh_list_contains_value( const KNamelist * list, const String * value );

bool vdh_take_1st_table_from_db( dump_context *ctx, const KNamelist *tbl_names );

bool vdh_take_this_table_from_list( dump_context *ctx, const KNamelist *tbl_names,
                                    const char * table_to_find );

bool vdh_take_this_table_from_db( dump_context *ctx, const VDatabase *db,
                                  const char * table_to_find );

rc_t vdh_print_col_info( dump_context *ctx,
                         const p_col_def col_def,
                         const VSchema *my_schema );

rc_t vdh_resolve_remote_accession( const char * accession, char * dst, size_t dst_size );
rc_t vdh_resolve_accession( const char * accession, char * dst, size_t dst_size, bool remotely );
rc_t vdh_resolve_cache( const char * accession, char * dst, size_t dst_size );
rc_t vdh_check_cache_comleteness( const char * path, float * percent, uint64_t * bytes_in_cache );

rc_t vdh_path_to_vpath( const char * path, VPath ** vpath );

void vdh_clear_recorded_errors( void );

rc_t vdh_check_table_empty( const VTable * tab );

rc_t vdh_open_table_by_path( const VDatabase * db, const char * inner_db_path, const VTable ** tab );

#ifdef __cplusplus
}
#endif

#endif
