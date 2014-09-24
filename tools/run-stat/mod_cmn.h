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

#ifndef _h_stat_mod_cmn_
#define _h_stat_mod_cmn_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/rc.h>
#include <klib/vector.h>
#include <klib/namelist.h>
#include <vdb/cursor.h>
#include "helper.h"
#include "key_value.h"

struct module;

/* the type of the callback-function to called before the cursor is open */
typedef rc_t ( CC * mod_pre_open ) ( struct module * self,
                                     const VCursor * cur );

/* the type of the callback-function to be performed on every row */
typedef rc_t ( CC * mod_row ) ( struct module * self,
                                const VCursor * cur );

/* the type of the callback-function to be performed on after all rows are read */
typedef rc_t ( CC * mod_post_rows ) ( struct module * self );


/* the type of the callback-function to query how many sub-reports
   a statistic-module can produce */
typedef rc_t ( CC * mod_query_report_count ) ( struct module * self,
                                               uint32_t * count );

/* the type of the callback-function to query the name of one
   of the sub-reports */
typedef rc_t ( CC * mod_query_report_name ) ( struct module * self,
                                              uint32_t n, char ** name );

/* the type of the callback-function to produce a sub-report */
typedef rc_t ( CC * mod_report ) ( struct module * self,
                                   p_report report,
                                   const uint32_t sub_select );

/* the type of the callback-function to produce a grafic-file */
typedef rc_t ( CC * mod_graph ) ( struct module * self,
                                  uint32_t n, const char * filename );


/* the type of the callback-function to be performed free the module-context */
typedef rc_t ( CC * mod_free ) ( struct module * self );


/********************************************************************
this structure represents a statistic-module
********************************************************************/
typedef struct module
{
    char *name;             /* every module puts its name in here */
    void * mod_ctx;         /* every module puts its module-context here */
    bool active;            /* if the module can be used (all necessary columns found)*/

    mod_pre_open f_pre_open; /* request the uses columns etc. */

    mod_row  f_row;         /* called for every row */

    mod_post_rows f_post_rows; /* called after all rows are read */

    mod_query_report_count f_count;
                            /* how many reports does the module produce */

    mod_query_report_name  f_name;
                            /* name of a report */

    mod_report f_report;    /* create a sub-report */

    mod_graph f_graph;      /* create a grafic for a sub-report */

    mod_free f_free;        /* called to free the context */
} module;
typedef module* p_module;


typedef struct mod_list
{
    Vector list;         /* the list of modules... */
} mod_list;
typedef mod_list* p_mod_list;


rc_t mod_list_init( p_mod_list * self, const KNamelist * names );

rc_t mod_list_destroy( p_mod_list self );

rc_t mod_list_pre_open( p_mod_list self, const VCursor * cur );

rc_t mod_list_row( p_mod_list self, const VCursor * cur );

rc_t mod_list_post_rows( p_mod_list self );

rc_t mod_list_count( p_mod_list self, uint32_t * count );

rc_t mod_list_name( p_mod_list self, const uint32_t idx,
                    char ** name );

rc_t mod_list_subreport_count( p_mod_list self, const uint32_t idx,
                               uint32_t * count );

rc_t mod_list_subreport_name( p_mod_list self, const uint32_t m_idx,
                              const uint32_t r_idx, char ** name );

rc_t mod_list_subreport( p_mod_list self,
                         const uint32_t m_idx,
                         const uint32_t r_idx,
                         p_char_buffer dst,
                         const uint32_t mode );

rc_t mod_list_graph( p_mod_list self, const uint32_t m_idx,
                        const uint32_t r_idx, const char * filename );

#endif
