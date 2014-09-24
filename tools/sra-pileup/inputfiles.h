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

#ifndef _h_inputfiles_
#define _h_inputfiles_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/vector.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/namelist.h>

#include <vdb/manager.h>
#include <vdb/vdb-priv.h>
#include <vdb/database.h>
#include <vdb/table.h>

#include <align/reference.h>
#include <align/manager.h>

/*
    the VNamelist of input-files/accessions/uri's is tested one at a time
    if it is: a database, a table or cannot be found
    in case of a database : a vdb-db-handle is opened
    in case of a table    : a vdb-tab-handle is opened
    in case of not found  : name is put into the not-found list
*/


typedef struct input_database
{
    uint32_t db_idx;
    char * path;
    const VDatabase * db;
    const ReferenceList *reflist;
    void * prim_ctx;
    void * sec_ctx;
    void * ev_ctx;
} input_database;


typedef struct input_table
{
    char * path;
    const VTable * tab;
} input_table;



typedef struct input_files
{
    uint32_t database_count;
    uint32_t table_count;
    uint32_t not_found_count;

    Vector dbs;
    Vector tabs;
    VNamelist * not_found;
} input_files;



rc_t discover_input_files( input_files **self, const VDBManager *mgr,
                           const VNamelist * src, uint32_t reflist_options );

void release_input_files( input_files *self );

#endif
