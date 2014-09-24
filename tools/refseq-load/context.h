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

#ifndef _h_context_
#define _h_context_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_refseq_load_includes_
#include "refseq-load-includes.h"
#endif

#ifndef _h_definitions_
#include "definitions.h"
#endif

#include <kapp/args.h>

#define OPTION_DST_PATH          "dst-path"
#define OPTION_FORCE             "Force"
#define OPTION_SRC               "src"
#define OPTION_SCHEMA            "schema"
#define OPTION_CHUNK_SIZE        "chunk-size"
#define OPTION_CIRCULAR          "circular"
#define OPTION_QUIET             "quiet"
#define OPTION_IFILE             "input-is-file"

#define ALIAS_DST_PATH          "d"
#define ALIAS_FORCE             "F"
#define ALIAS_SRC               "f"
#define ALIAS_SCHEMA            "s"
#define ALIAS_CHUNK_SIZE        "b"
#define ALIAS_CIRCULAR          "c"
#define ALIAS_IFILE             "i"

typedef struct context
{
    const char* argv0;
    char *dst_path;
    char *src;
    char *schema;
    uint32_t chunk_size;
    bool usage_requested;
    bool circular;
    bool quiet;
    bool force_target;
    bool input_is_file;
} context;
typedef context* p_context;


/*
 * generates a new context, initializes values
*/
rc_t context_init( context **ctx );


/*
 * destroys a context, frees all pointers the context owns
*/
rc_t context_destroy( p_context ctx );


/*
 * reads all arguments and options, fills the context
 * with copies (if strings) of this data
*/
rc_t context_capture_arguments_and_options( const Args * args, p_context ctx );

#ifdef __cplusplus
}
#endif

#endif
