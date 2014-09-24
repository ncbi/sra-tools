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

#ifndef _h_run_stat_context_
#define _h_run_stat_context_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <kapp/args.h>
#include <klib/vector.h>
#include <klib/namelist.h>
#include "definitions.h"
#include "num-gen.h"

#define OPTION_TABLE             "table"
#define OPTION_ROWS              "rows"
#define OPTION_SCHEMA            "schema"
#define OPTION_WITHOUT_ACCESSION "without_accession"
#define OPTION_PROGRESS          "progress"
#define OPTION_MODULE            "module"
#define OPTION_GRAFIC            "grafic"
#define OPTION_REPORT            "report"
#define OPTION_OUTPUT            "output"
#define OPTION_PREFIX            "prefix"


#define ALIAS_TABLE             "T"
#define ALIAS_ROWS              "R"
#define ALIAS_SCHEMA            "S"
#define ALIAS_WITHOUT_ACCESSION "a"
#define ALIAS_PROGRESS          "p"
#define ALIAS_MODULE            "m"
#define ALIAS_GRAFIC            "g"
#define ALIAS_RREPORT           "r"
#define ALIAS_OUTPUT            "o"
#define ALIAS_PREFIX            "b"

/********************************************************************
the dump context contains all informations needed to execute the dump
********************************************************************/
typedef struct stat_ctx
{
    const char *path;
    const KNamelist *schema_list;
    const KNamelist *module_list;
    const char *table;
    p_ng row_generator;

    bool usage_requested;
    bool dont_check_accession;
    bool show_progress;
    bool produce_grafic;
    uint32_t report_type;

    const char *output_path;
    const char *name_prefix;
} stat_ctx;
typedef stat_ctx* p_stat_ctx;


rc_t ctx_init( stat_ctx **ctx );
rc_t ctx_destroy( p_stat_ctx ctx );

uint32_t context_schema_count( p_stat_ctx ctx );

rc_t ctx_set_table( p_stat_ctx ctx, const char *src );

rc_t ctx_capture_arguments_and_options( const Args * args, p_stat_ctx ctx );

#endif
