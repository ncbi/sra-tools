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

#include <klib/out.h>
#include <klib/text.h>
#include <sysalloc.h>
#include <stdlib.h>

#include "mod_cmn.h"
#include "mod_reads.h"


static rc_t module_init( p_module * new_module, const char * name )
{
    rc_t rc = rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( new_module != NULL && name != NULL )
    {
        rc = RC( rcExe, rcData, rcAllocating, rcMemory, rcExhausted );
        (*new_module) = calloc( 1, sizeof( module ) );
        if ( *new_module != NULL )
        {
            char * s_module;
            char * param;
            /* we have to split the name into mod-name and parameters */
            rc = helper_split_by_parenthesis( name, &s_module, &param );
            if ( rc == 0 )
            {
                (*new_module)->name = string_dup_measure ( s_module, NULL );
                /* here we hardcode the build-in modules */
                if ( helper_str_cmp( s_module, "bases" ) == 0 )
                {
                    rc = mod_reads_init( *new_module, param );
                }
                free( s_module );
                if ( param != NULL )
                    free( param );
            }
        }
    }
    return rc;
}


static void destroy_module( p_module a_mod )
{
    if ( a_mod != NULL )
    {
        /* free the name... */
        if ( a_mod->name != NULL )
            free( a_mod->name );
        /* call the function to free the context-pointer */
        if ( a_mod->f_free != NULL )
            a_mod->f_free( a_mod );
        free( a_mod );
    }
}


static void CC destroy_module_cb( void* node, void* data )
{
    destroy_module( ( p_module )node );
}


static rc_t modules_init_loop( p_mod_list self, const KNamelist * names )
{
    uint32_t count;
    rc_t rc = KNamelistCount ( names, &count );
    if ( rc == 0 )
    {
        uint32_t i;
        for ( i = 0; i < count && rc == 0; ++i )
        {
            const char * s;
            rc = KNamelistGet ( names, i, &s );
            if ( rc == 0 )
            {
                p_module a_module;
                rc = module_init( &a_module, s );
                if ( rc == 0 )
                {
                    rc = VectorAppend( &(self->list), NULL, a_module );
                    if ( rc != 0 )
                        destroy_module( a_module );
                }
            }
        }
    }
    return rc;
}


rc_t mod_list_init( p_mod_list * self, const KNamelist * names )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL )
    {
        rc = RC( rcExe, rcData, rcAllocating, rcMemory, rcExhausted );
        (*self) = calloc( 1, sizeof( mod_list ) );
        if ( *self != NULL )
        {
            VectorInit( &( (*self)->list ), 0, 5 );
            rc = modules_init_loop( *self, names );
            if ( rc != 0 )
            {
                VectorWhack( &( (*self)->list ), destroy_module_cb, NULL );
                free( *self );
                *self = NULL;
            }
        }
    }
    return rc;
}


rc_t mod_list_destroy( p_mod_list list )
{
    VectorWhack( &(list->list), destroy_module_cb, NULL );
    free( list );
    return 0;
}


rc_t mod_list_pre_open( p_mod_list self, 
                        const VCursor * cur )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && cur != NULL )
    {
        uint32_t len = VectorLength( &(self->list) );
        rc = 0;
        if ( len > 0 )
        {
            uint32_t m_idx;
            for ( m_idx = 0; m_idx < len && rc == 0; ++m_idx )
            {
                p_module a_module = VectorGet ( &(self->list), m_idx );
                if ( a_module != NULL && a_module->f_pre_open != NULL )
                {
                    rc = a_module->f_pre_open( a_module, cur );
                }
            }
        }
    }
    return rc;
}


rc_t mod_list_post_rows( p_mod_list self )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL )
    {
        uint32_t len = VectorLength( &(self->list) );
        rc = 0;
        if ( len > 0 )
        {
            uint32_t m_idx;
            for ( m_idx = 0; m_idx < len && rc == 0; ++m_idx )
            {
                p_module a_module = VectorGet ( &(self->list), m_idx );
                if ( a_module != NULL && a_module->f_post_rows != NULL )
                {
                    rc = a_module->f_post_rows( a_module );
                }
            }
        }
    }
    return rc;
}


rc_t mod_list_count( p_mod_list self, uint32_t * count )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && count != NULL )
    {
        *count = VectorLength( &(self->list) );
        rc = 0;
    }
    return rc;
}


static p_module mod_list_get( p_mod_list self, const uint32_t idx )
{
    p_module res = NULL;
    if ( self != NULL )
    {
        if ( idx < VectorLength( &(self->list) ) )
            res = VectorGet ( &(self->list), idx );
    }
    return res;
}


rc_t mod_list_name( p_mod_list self, const uint32_t idx,
                    char ** name )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && name != NULL )
    {
        p_module a_module = mod_list_get( self, idx );
        if ( a_module == NULL )
            rc = RC( rcExe, rcData, rcAllocating, rcParam, rcInvalid );
        else
        {
            (*name) = string_dup_measure ( a_module->name, NULL );
            rc = 0;
        }
    }
    return rc;
}


rc_t mod_list_subreport_count( p_mod_list self, const uint32_t idx,
                               uint32_t * count )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && count != NULL )
    {
        p_module a_module = mod_list_get( self, idx );
        rc = RC( rcExe, rcData, rcAllocating, rcParam, rcInvalid );
        if ( a_module != NULL && a_module->f_count != NULL )
            rc = a_module->f_count( a_module, count );
    }
    return rc;
}


rc_t mod_list_subreport_name( p_mod_list self, const uint32_t m_idx,
                              const uint32_t r_idx, char ** name )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && name != NULL )
    {
        p_module a_module = mod_list_get( self, m_idx );
        rc = RC( rcExe, rcData, rcAllocating, rcParam, rcInvalid );
        if ( a_module != NULL && a_module->f_name != NULL )
            rc = a_module->f_name( a_module, r_idx, name );
    }
    return rc;
}


rc_t mod_list_subreport( p_mod_list self,
                         const uint32_t m_idx,
                         const uint32_t r_idx,
                         p_char_buffer dst,
                         const uint32_t mode )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && dst != NULL )
    {
        p_module a_module = mod_list_get( self, m_idx );
        rc = RC( rcExe, rcData, rcAllocating, rcParam, rcInvalid );
        if ( a_module != NULL && a_module->f_report != NULL )
        {
            p_report report;
            rc = report_init( &report, 50 );
            if ( rc == 0 )
            {
                rc = a_module->f_report( a_module, report, r_idx );
                if ( rc == 0 )
                    rc = report_print( report, dst, mode );
                report_destroy( report );
            }
        }
    }
    return rc;
}


rc_t mod_list_graph( p_mod_list self, const uint32_t m_idx,
                     const uint32_t r_idx, const char * filename )
{
    rc_t rc = RC( rcExe, rcData, rcAllocating, rcParam, rcNull );
    if ( self != NULL && filename != NULL )
    {
        p_module a_module = mod_list_get( self, m_idx );
        rc = RC( rcExe, rcData, rcAllocating, rcParam, rcInvalid );
        if ( a_module != NULL && a_module->f_graph != NULL )
            rc = a_module->f_graph( a_module, r_idx, filename );
    }
    return rc;
}


rc_t mod_list_row( p_mod_list self, const VCursor * cur )
{
    rc_t rc = 0;
    uint32_t len = VectorLength( &(self->list) );
    if ( len > 0 )
    {
        uint32_t m_idx;
        for ( m_idx = 0; m_idx < len && rc == 0; ++m_idx )
        {
            p_module a_module = VectorGet ( &(self->list), m_idx );
            if ( a_module != NULL && a_module->f_row != NULL )
            {
                rc = a_module->f_row( a_module, cur );
            }
        }
    }
    return rc;
}
