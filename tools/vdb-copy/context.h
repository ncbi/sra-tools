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

#ifndef _h_vdb_copy_includes_
#include "vdb-copy-includes.h"
#endif

#ifndef _h_config_values_
#include "config_values.h"
#endif

#ifndef _h_vdb_redactval_
#include "redactval.h"
#endif

#include <kapp/args.h>
#include "definitions.h"
#include "num-gen.h"

#define OPTION_TABLE             "table"
#define OPTION_ROWS              "rows"
#define OPTION_COLUMNS           "columns"
#define OPTION_SCHEMA            "schema"
#define OPTION_WITHOUT_ACCESSION "without_accession"
#define OPTION_IGNORE_REJECT     "ignore_reject"
#define OPTION_IGNORE_REDACT     "ignore_redact"

#if ALLOW_EXTERNAL_CONFIG
#define OPTION_KFG_PATH          "kfg_path"
#endif

#define OPTION_SHOW_MATCHING     "show_matching"
#define OPTION_SHOW_PROGRESS     "show_progress"
#define OPTION_IGNORE_INCOMP     "ignore_incompatible_columns"
#define OPTION_REINDEX           "reindex"
#define OPTION_SHOW_REDACT       "show_redact"
#define OPTION_EXCLUDED_COLUMNS  "exclude_columns"
#define OPTION_SHOW_META         "show_meta"
#define OPTION_MD5_MODE          "md5mode"
#define OPTION_FORCE             "force"
#define OPTION_UNLOCK            "unlock"
#define OPTION_BLOB_CHECKSUM     "blob_checksum"


#define ALIAS_TABLE             "T"
#define ALIAS_ROWS              "R"
#define ALIAS_COLUMNS           "C"
#define ALIAS_SCHEMA            "S"
#define ALIAS_WITHOUT_ACCESSION "a"
#define ALIAS_IGNORE_REJECT     "r"
#define ALIAS_IGNORE_REDACT     "e"

#if ALLOW_EXTERNAL_CONFIG
#define ALIAS_KFG_PATH          "k"
#endif

#define ALIAS_SHOW_MATCHING     "m"
#define ALIAS_SHOW_PROGRESS     "p"
#define ALIAS_IGNORE_INCOMP     "i"
#define ALIAS_REINDEX           "n"
#define ALIAS_SHOW_REDACT       "w"
#define ALIAS_EXCLUDED_COLUMNS  "x"
#define ALIAS_SHOW_META         "t"
#define ALIAS_MD5_MODE          "d"
#define ALIAS_FORCE             "f"
#define ALIAS_UNLOCK            "u"
#define ALIAS_BLOB_CHECKSUM     "b"


/* *******************************************************************
the dump context contains all informations needed to execute the dump
******************************************************************* */
typedef struct context
{
    /* read from commandline */
    char *src_path;
    char *dst_path;
    const char *kfg_path;    
    const KNamelist *src_schema_list;
    const char *table;
    const char *columns;
    const char *excluded_columns;
    num_gen *row_generator;
    bool usage_requested;
    bool dont_check_accession;
    uint64_t platform_id;
    bool ignore_reject;
    bool ignore_redact;
    bool show_matching;
    bool show_progress;
    bool ignore_incomp;
    bool reindex;
    bool show_redact;
    bool show_meta;
    uint8_t md5_mode;
    uint8_t blob_checksum;
    bool force_kcmInit;
    bool force_unlock;

    /* set by application */
    bool dont_remove_target;
    config_values config;
    redact_vals * rvals;
    /* for the destination table*/
    char * dst_schema_tabname;
    /* legacy related parameters */
    char * legacy_schema_file;
    char * legacy_dont_copy;
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
 * performs the range check to trim the internal number
 * generator to the given range
*/
rc_t context_range_check( p_context ctx, 
                          const int64_t first, const uint64_t count );


rc_t context_set_range( p_context ctx, 
                        const int64_t first, const uint64_t count );

/*
 * returns the number of schema's given on the commandline
*/
uint32_t context_schema_count( p_context ctx );


/*
 * reads all arguments and options, fills the context
 * with copies (if strings) of this data
*/
rc_t context_capture_arguments_and_options( const Args * args, p_context ctx );

#ifdef __cplusplus
}
#endif

#endif
