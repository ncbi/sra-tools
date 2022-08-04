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
#include "config_values.h"

#include <sysalloc.h>
#include <string.h>
#include <stdlib.h>

static void config_values_destroy_str( char **s )
{
    if ( *s != NULL )
    {
        free( *s );
        *s = NULL;
    }
}


void config_values_init( p_config_values cv )
{
    memset( cv, 0, sizeof cv[0] );
}


void config_values_destroy( p_config_values cv )
{
    config_values_destroy_str( &cv->filter_col_name );
    config_values_destroy_str( &cv->meta_ignore_nodes );
    config_values_destroy_str( &cv->redactable_types );
    config_values_destroy_str( &cv->do_not_redact_columns );
}
