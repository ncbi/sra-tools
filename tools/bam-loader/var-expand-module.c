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

#include "var-expand-module.h"

rc_t var_expand_init( var_expand_data ** data )
{
    rc_t rc = 0;
    if ( data == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcConstructing, rcParam, rcNull );
    }
    else
    {
        var_expand_data * tmp = malloc( sizeof( *tmp ) );
        if ( tmp == NULL )
        {
            rc = RC ( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        }
        else
        {
            tmp->something = 0;
            *data = tmp;
        }
    }
    return rc;

}


rc_t var_expand_handle( var_expand_data * data, void * some_data )
{
    rc_t rc = 0;
    if ( data == NULL || some_data == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    return rc;
}


rc_t var_expand_finish( var_expand_data * data )
{
    rc_t rc = 0;
    if ( data == NULL )
    {
        rc = RC ( rcApp, rcNoTarg, rcAccessing, rcParam, rcNull );
    }
    else
    {
        free( ( void * ) data );
    }
    return rc;
}
