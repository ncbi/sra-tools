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

#ifndef _h_cgi_request_
#define _h_cgi_request_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_helper_
#include "helper.h"
#endif

/* ------------------------------------------------------------------------ */


typedef struct request_params
{
    const char * names_url;     /* NULL ... default url  */
    const char * search_url;    /* NULL ... default url  */
    const char * names_ver;     /* NULL ... default vers */
    const char * search_ver;    /* NULL ... default vers */
    const char * proto;
    uint32_t * projects;
    const char ** params;       /* NULL ... none */
    const char ** terms;        /* mandatory... NULL-terminated list of terms */
    size_t buffer_size;
    uint32_t timeout_ms;
} request_params;


/* ------------------------------------------------------------------------ */


typedef rc_t ( * on_string_t )( const String * line, void * data );

typedef struct reply_obj        /*  names       search  */
{
    const String * prj_id;       /*  -           X       */
    const String * obj_type;     /*  X           X       */
    const String * id;           /*  X           X       */
    const String * date;         /*  X           -       */
    const String * checksum;     /*  X           -       */
    const String * ticket;       /*  X           -       */
    const String * name;         /*  -           X       */
    const String * path;         /*  X           X       */
    const String * message;      /*  X           X       */
    size_t size;                 /*  X           X       */    
    ver_maj_min version;         /*  X           X       */
    uint32_t code;               /*  X           -       */    
} reply_obj;

struct reply_obj_list;

void release_reply_obj_list( struct reply_obj_list * list );

typedef rc_t ( * on_reply_obj_t )( const reply_obj * obj, void * data );

rc_t foreach_reply_obj( struct reply_obj_list * list,
                        on_reply_obj_t on_obj,
                        void * data );


/* ------------------------------------------------------------------------ */


rc_t raw_names_request( const request_params * request,
                        on_string_t on_line,
                        uint32_t * rslt_code,
                        void * data );

rc_t parsed_names_request( const request_params * request,
                           on_reply_obj_t on_obj,
                           uint32_t * rslt_code,
                           void * data );

rc_t names_request_to_list( const request_params * request,
                            uint32_t * rslt_code,
                            struct reply_obj_list ** list );

/* ------------------------------------------------------------------------ */


rc_t raw_search_request( const request_params * request,
                        on_string_t on_line,
                        uint32_t * rslt_code,
                        void * data );

rc_t parsed_search_request( const request_params * request,
                            on_reply_obj_t on_obj,
                            uint32_t * rslt_code,
                            void * data );

rc_t search_request_to_list( const request_params * request,
                            uint32_t * rslt_code,
                            struct reply_obj_list ** list,
                            bool resolve_path );

#ifdef __cplusplus
}
#endif

#endif
