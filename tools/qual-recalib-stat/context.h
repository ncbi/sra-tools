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

#include <kapp/args.h>
#include <klib/namelist.h>
#include <klib/out.h>
#include "namelist_tools.h"
#include "num-gen.h"

#define OPTION_ROWS              "rows"
#define OPTION_SCHEMA            "schema"
#define OPTION_SHOW_PROGRESS     "show_progress"
#define OPTION_OUTFILE           "output_file"
#define OPTION_OUTMODE           "mode"
#define OPTION_GCWINDOW          "gcwindow"
#define OPTION_EXCLUDE           "exclude"
#define OPTION_INFO              "info"
#define OPTION_IGNORE_MISMATCH   "ignore_mismatch"

#define ALIAS_ROWS              "R"
#define ALIAS_SCHEMA            "S"
#define ALIAS_SHOW_PROGRESS     "p"
#define ALIAS_OUTFILE           "o"
#define ALIAS_OUTMODE           "m"
#define ALIAS_GCWINDOW          "g"
#define ALIAS_EXCLUDE           "x"
#define ALIAS_INFO              "i"
#define ALIAS_IGNORE_MISMATCH   "n"

/* *******************************************************************
the context contains all informations needed to execute the run
******************************************************************* */
typedef struct context
{
    /* read from commandline */
    char *src_path;
    char *output_file_path;
    char *output_mode;
    char *exclude_file_path;
    const KNamelist *src_schema_list;
    num_gen *row_generator;
    bool usage_requested;
    bool show_progress;
    bool info;
    bool ignore_mismatch;
    uint32_t gc_window;
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
