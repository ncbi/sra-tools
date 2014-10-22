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
#include "inputfiles.h"

#include <sra/sraschema.h> /* VDBManagerMakeSRASchema */
#include <vdb/schema.h> /* VSchemaRelease */

#include <kdb/manager.h>
#include <kfs/file.h>
#include <vfs/manager.h>
#include <vfs/path.h>

#include <sysalloc.h>
#include <stdlib.h>


static void free_input_database( input_database * id )
{
    if ( id->path != NULL ) free( id->path );
    if ( id->db != NULL ) VDatabaseRelease( id->db );
    if ( id->reflist != NULL ) ReferenceList_Release( id->reflist );
    if ( id->prim_ctx != NULL ) free( id->prim_ctx );
    if ( id->sec_ctx != NULL ) free( id->sec_ctx );
    if ( id->ev_ctx != NULL ) free( id->ev_ctx );
    free( id );
}


static void free_input_table( input_table * it )
{
    if ( it->path != NULL ) free( it->path );
    if ( it->tab != NULL ) VTableRelease( it->tab );
    free( it );
}


static rc_t append_database( input_files *self, const VDatabase * db,
                             const char * path, uint32_t reflist_options, bool show_err )
{
    const ReferenceList *reflist;
    rc_t rc = ReferenceList_MakeDatabase( &reflist, db, reflist_options, 0, NULL, 0 );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot create reflist for '$(t)'", "t=%s", path ) );
    }
    else
    {
        input_database * id = calloc( sizeof * id, 1 );
        if ( id == NULL )
        {
            rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            (void)LOGERR( klogErr, rc, "cannot create input-database structure" );
        }
        else
        {
            uint32_t idx;

            id->db = db;
            id->reflist = reflist;
            id->path = string_dup( path, string_size( path ) );

            rc = VectorAppend( &self->dbs, &idx, id );
            if ( rc != 0 )
            {
                (void)PLOGERR( klogErr, ( klogErr, rc, "cannot append db '$(t)' to inputfiles", "t=%s", path ) );
                free_input_database( id );
            }
            else
            {
                ++(self->database_count);
                id->db_idx = idx;
            }
        }
    }
    return rc;
}


static rc_t append_table( input_files *self, const VTable *tab, const char * path )
{
    rc_t rc = 0;
    input_table * it = calloc( sizeof * it, 1 );
    if ( it == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        (void)LOGERR( klogErr, rc, "cannot create input-table structure" );
    }
    else
    {
        it->path = string_dup( path, string_size( path ) );
        it->tab = tab;

        rc = VectorAppend( &self->tabs, NULL, it );
        if ( rc != 0 )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot append tab '$(t)' to inputfiles", "t=%s", path ) );
            free_input_table( it );
        }
        else
        {
            ++(self->table_count);
        }
    }
    return rc;
}


static int cmp_pchar( const char * a, const char * b )
{
    int res = 0;
    if ( ( a != NULL )&&( b != NULL ) )
    {
        size_t len_a = string_size( a );
        size_t len_b = string_size( b );
        res = string_cmp ( a, len_a, b, len_b, ( len_a < len_b ) ? len_b : len_a );
    }
    return res;
}


static rc_t contains_ref_and_alignments( const VDatabase * db, const char * path, bool * res, bool show_err )
{
    KNamelist *tables;
    rc_t rc = VDatabaseListTbl ( db, &tables );
    if ( rc != 0 )
    {
        if ( show_err )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "VDatabaseListTbl() failed on '$(t)'", "t=%s", path ) );
        }
    }
    else
    {
        uint32_t count;
        rc = KNamelistCount ( tables, &count );
        if ( rc != 0 )
        {
            if ( show_err )
            {
                (void)PLOGERR( klogErr, ( klogErr, rc, "KNamelistCount on Table-Names-List failed on '$(t)'", "t=%s", path ) );
            }
        }
        else
        {
            uint32_t idx;
            bool has_ref = false;
            bool has_alignment = false;
            for ( idx = 0; rc == 0 && idx < count; ++idx )
            {
                const char * tabname;
                rc = KNamelistGet ( tables, idx, &tabname );
                if ( rc != 0 )
                {
                    if ( show_err )
                    {
                        (void)PLOGERR( klogErr, ( klogErr, rc, "KNamelistGet on Table-Names-List failed on '$(t)'", "t=%s", path ) );
                    }
                }
                else
                {
                    if ( cmp_pchar( tabname, "REFERENCE" ) == 0 )
                        has_ref = true;
                    if ( cmp_pchar( tabname, "PRIMARY_ALIGNMENT" ) == 0 )
                        has_alignment = true;
                }
            }
            *res = ( has_ref && has_alignment );
        }
        KNamelistRelease( tables );
    }
    return rc;
}


static rc_t open_database( input_files *self, const VDBManager *mgr,
                           const char * path, uint32_t reflist_options, bool show_err )
{
    const VDatabase * db;
    rc_t rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", path );
    if ( rc != 0 )
    {
        if ( show_err )
        {
            (void)PLOGERR( klogErr, ( klogErr, rc, "cannot open db '$(t)'", "t=%s", path ) );
        }
    }
    else
    {
        bool has_alignments = false;
        rc = contains_ref_and_alignments( db, path, &has_alignments, show_err );
        if ( rc == 0 )
        {
            if ( has_alignments )
            {
                rc = append_database( self, db, path, reflist_options, show_err );
            }
            else
            {
                const VTable *tab;
                rc = VDatabaseOpenTableRead ( db, &tab, "SEQUENCE" );
                if ( rc != 0 )
                {
                    if ( show_err )
                    {
                        (void)PLOGERR( klogErr, ( klogErr, rc, "cannot open table SEQUENCE for '$(t)'", "t=%s", path ) );
                    }
                }
                else
                {
                    rc = append_table( self, tab, path );
                }
            }
        }
    }
    return rc;
}


static rc_t open_table( input_files *self, const VDBManager *mgr, const char * path, bool show_error )
{
    const VTable * tab;    

    rc_t rc = VDBManagerOpenTableRead( mgr, &tab, NULL, "%s", path );
    if ( rc != 0 )
    {
        struct VSchema* schema = NULL;
        rc = VDBManagerMakeSRASchema( mgr, &schema );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot MakeSRASchema" );
        }
        else
        {
            rc = VDBManagerOpenTableRead( mgr, &tab, schema, "%s", path );
            if ( rc != 0 )
            {
                if ( show_error )
                {
                    (void)PLOGERR( klogErr, ( klogErr, rc, "cannot open tab '$(t)'", "t=%s", path ) );
                }
            }
            VSchemaRelease( schema );
        }
    }

    if ( rc == 0 )
        rc = append_table( self, tab, path );

    return rc;
}


static rc_t is_unknown( input_files *self, const char * path )
{
    rc_t rc = VNamelistAppend( self->not_found, path );
    if ( rc == 0 )
        ++( self->not_found_count );
    return rc;
}


static rc_t split_input_files( input_files *self, const VDBManager *mgr,
                               const VNamelist * src, uint32_t reflist_options )
{
    uint32_t src_count;
    rc_t rc = VNameListCount( src, &src_count );
    if ( rc != 0 )
    {
        (void)LOGERR( klogErr, rc, "VNameListCount failed" );
    }
    else
    {
        uint32_t src_idx;
        for ( src_idx = 0; src_idx < src_count && rc == 0 ; ++src_idx )
        {
            const char * path;
            rc = VNameListGet( src, src_idx, &path );
            if ( rc == 0 && path != NULL )
            {
                int path_type = VDBManagerPathType ( mgr, "%s", path );
                if ( rc == 0 )
                {
                    switch( path_type )
                    {
                        case kptDatabase : rc = open_database( self, mgr, path, reflist_options, true ); break;

                        case kptPrereleaseTbl :
                        case kptTable    : rc = open_table( self, mgr, path, true ); break;

                        default          : rc = is_unknown( self, path ); break;
                    }
                }
            }
        }
    }
    return rc;
}


rc_t discover_input_files( input_files **self, const VDBManager *mgr,
                           const VNamelist * src, uint32_t reflist_options )
{
    rc_t rc = 0;

    input_files * ipf = calloc( sizeof * ipf, 1 );
    *self = NULL;
    if ( ipf == NULL )
    {
        rc = RC( rcApp, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        (void)LOGERR( klogErr, rc, "cannot create inputfiles structure" );
    }
    else
    {
        rc = VNamelistMake( &( ipf->not_found ), 5 );
        if ( rc != 0 )
            (void)LOGERR( klogErr, rc, "cannot create inputfiles.not_found structure" );
        else
        {
            VectorInit( &( ipf->dbs ), 0, 5 );
            VectorInit( &( ipf->tabs ), 0, 5 );
            rc = split_input_files( ipf, mgr, src, reflist_options );
        }
        if ( rc != 0 )
            release_input_files( ipf );
        else
            *self = ipf;
    }
    return rc;
}


static void CC db_whack( void * item, void * data )
{
    input_database * id = ( input_database * )item;
    free_input_database( id );
}


static void CC tab_whack( void * item, void * data )
{
    input_table * it = ( input_table * )item;
    free( it->path );
    VTableRelease( it->tab );
    free( it );
}


void release_input_files( input_files *self )
{
    VectorWhack( &self->dbs, db_whack, NULL );
    VectorWhack( &self->tabs, tab_whack, NULL );
    VNamelistRelease( self->not_found );
    free( self );
}
