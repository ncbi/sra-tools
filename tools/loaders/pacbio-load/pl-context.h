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

#ifndef _h_pl_context_
#define _h_pl_context_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/out.h>
#include <klib/rc.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/namelist.h>

#include <kapp/args.h>


#define OPTION_SCHEMA       "schema"
#define OPTION_FORCE        "force"
#define OPTION_TABS         "tabs"
#define OPTION_WITH_PROGRESS  "with_progressbar"
#define OPTION_OUTPUT       "output"

#define ALIAS_SCHEMA        "S"
#define ALIAS_FORCE         "f"
#define ALIAS_TABS          "t"
#define ALIAS_WITH_PROGRESS "p"
#define ALIAS_OUTPUT        "o"

#define DFLT_SCHEMA         "sra/pacbio.vschema"
#define PACBIO_SCHEMA_DB    "NCBI:SRA:PacBio:smrt:db"


/* *******************************************************************
the parameter-context contains all informations needed to load
******************************************************************* */
typedef struct context
{
    char *dst_path;     /* the vdb-database-path to create */
    char *schema_name;  /* name of a schema-file to use", if different from std */
    char *tabs;         /* load only these tabs... */
    VNamelist * src_paths;  /* list of source-paths */
    bool force;         /* if true", overwrite eventually existing output-db */
    bool with_progress; /* if true", use the pl_progressbar */
} context;


rc_t ctx_init( const Args * args, context *ctx );
void ctx_free( context *ctx );

rc_t ctx_show( context * ctx );
bool ctx_ld_sequence( context * ctx );
bool ctx_ld_consensus( context * ctx );
bool ctx_ld_passes( context * ctx );
bool ctx_ld_metrics( context * ctx );

rc_t CC Usage ( const Args * args );

#ifdef __cplusplus
}
#endif

#endif
