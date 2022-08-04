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
#ifndef _h_config_values_
#define _h_config_values_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct config_values
{
    char * filter_col_name;
    char * meta_ignore_nodes;
    char * redactable_types;
    char * do_not_redact_columns;
} config_values;
typedef config_values* p_config_values;

void config_values_init( p_config_values cv );
void config_values_destroy( p_config_values cv );

#ifdef __cplusplus
}
#endif

#endif
