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

#ifndef _h_helper_
#define _h_helper_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_klib_rc_
#include <klib/rc.h>
#endif

#ifndef _h_klib_text_
#include <klib/text.h>
#endif

#ifndef _h_klib_namelist_
#include <klib/namelist.h>
#endif

#ifndef _h_kns_stream_
#include <kns/stream.h>
#endif

rc_t ErrMsg( const char * fmt, ... );

struct Args;

const char * get_str_option( const struct Args *args, const char *name, const char * dflt );
bool get_bool_option( const struct Args *args, const char *name );
size_t get_size_t_option( const struct Args * args, const char *name, size_t dflt );
uint32_t get_uint32_t_option( const struct Args * args, const char *name, uint32_t dflt );
uint64_t get_uint64_t_option( const struct Args * args, const char *name, uint64_t dflt );

typedef enum func_t { ft_resolve, ft_names, ft_search } func_t;
func_t get_func_t( const char * function );

rc_t foreach_arg( const struct Args * args,
                  rc_t ( * on_arg )( const char * arg, void * data ),
                  void * data );
rc_t args_to_namelist( const struct Args * args, VNamelist ** dst );
rc_t args_to_ptrs( const struct Args * args, const char *** ptr );

rc_t foreach_option( const struct Args * args, const char * option_name,
                     rc_t ( * on_option )( const char * arg, void * data ),
                     void * data );
rc_t options_to_namelist( const struct Args * args, const char * option_name, VNamelist ** dst );
rc_t options_to_ptrs( const struct Args * args, const char * option_name, const char *** ptr );

rc_t options_to_nums ( const struct Args * args,
                       const char * name, uint32_t ** ptr );

rc_t foreach_item( const VNamelist * src, 
                   rc_t ( * on_item )( const char * item, void * data ),
                   void * data );
                  
typedef enum pns_ver_t { vt_unknown, vt_1_0, vt_1_1, vt_3_0 } pns_ver_t;

pns_ver_t get_ver_t( const char * version, pns_ver_t default_version );
pns_ver_t get_ver_t_S( const String * version, pns_ver_t default_version );

typedef struct ver_maj_min
{
    uint8_t major;
    uint8_t minor;
} ver_maj_min;


void extract_versionS( ver_maj_min * ver, const String * version_string );
void extract_version( ver_maj_min * ver, const char * version_string );

pns_ver_t get_ver_t_S( const String * version, pns_ver_t default_version );


typedef rc_t ( * token_handler_t )( const String * token, uint32_t id, void * data );

rc_t foreach_token( const String * line,
                    token_handler_t token_handler,
                    char delim,
                    void * data );

#ifdef __cplusplus
}
#endif

#endif
