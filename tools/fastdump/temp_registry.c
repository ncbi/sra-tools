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
#include "temp_registry.h"
#include "concatenator.h"
#include <klib/vector.h>
#include <klib/out.h>
#include <klib/namelist.h>
#include <kproc/lock.h>

typedef struct temp_registry
{
    struct KFastDumpCleanupTask * cleanup_task;
    KLock * lock;
    size_t buf_size;
    Vector lists;
} temp_registry;


static void CC destroy_list( void * item, void * data )
{
    if ( item != NULL )
        VNamelistRelease ( item );
}

void destroy_temp_registry( temp_registry * self )
{
    if ( self != NULL )
    {
        VectorWhack ( &self -> lists, destroy_list, NULL );
        KLockRelease ( self -> lock );
        free( ( void * ) self );
    }
}

rc_t make_temp_registry( temp_registry ** registry, struct KFastDumpCleanupTask * cleanup_task )
{
    KLock * lock;
    rc_t rc = KLockMake ( &lock );
    if ( rc == 0 )
    {
        temp_registry * p = calloc( 1, sizeof * p );
        if ( p == 0 )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "make_temp_registry().calloc( %d ) -> %R", ( sizeof * p ), rc );
            KLockRelease ( lock );
        }
        else
        {
            VectorInit ( &p -> lists, 0, 4 );
            p -> lock = lock;
            p -> cleanup_task = cleanup_task;
            *registry = p;
        }
    }
    return rc;
}

rc_t register_temp_file( temp_registry * self, uint32_t read_id, const char * filename )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else if ( filename == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            VNamelist * l = VectorGet ( &self -> lists, read_id );
            if ( l == NULL )
            {
                rc = VNamelistMake ( &l, 12 );
                if ( rc == 0 )
                {
                    rc = VectorSet ( &self -> lists, read_id, l );
                    if ( rc != 0 )
                        VNamelistRelease ( l );
                }
            }
            if ( rc == 0 && l != NULL )
            {
                rc = VNamelistAppend ( l, filename );
                if ( rc == 0 )
                    rc = Add_to_Cleanup_Task ( self -> cleanup_task, filename );
            }
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

static uint32_t count_files( temp_registry * self, uint32_t * first )
{
    uint32_t count = VectorLength( &self -> lists );
    if ( count < 2 )
        *first = VectorStart( &self -> lists );
    else
    {
        uint32_t valid = 0;
        uint32_t idx = VectorStart( &self -> lists );
        while ( idx < count )
        {
            VNamelist * l = VectorGet ( &self -> lists, idx );
            if ( l != NULL )
            {
                if ( valid == 0 )
                    *first = idx;
                valid++;
            }
            idx += 1;
        }
        count = valid;
    }
    return count;
}

rc_t temp_registry_merge( temp_registry * self,
                          KDirectory * dir,
                          const char * output_filename,
                          size_t buf_size,
                          bool show_progress,
                          bool print_to_stdout,
                          bool force,
                          compress_t compress )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else if ( output_filename == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        uint32_t first;
        uint32_t count = count_files( self, &first );
        if ( count == 1 )
        {
            VNamelist * l = VectorGet ( &self -> lists, first );
            VNamelistReorder ( l, false );
            rc = execute_concat( dir,
                output_filename,
                l,
                buf_size,
                show_progress,
                print_to_stdout,
                force,
                compress );
        }
        else if ( count > 1 )
        {
            uint32_t count = VectorLength( &self -> lists );
            uint32_t idx = 0;
            while ( rc == 0 && idx < count )
            {
                VNamelist * l = VectorGet ( &self -> lists, idx );
                if ( l != NULL )
                {
                    SBuffer s_filename;
                    VNamelistReorder ( l, false );
                    rc = make_and_print_to_SBuffer( &s_filename, 4096, "%s.%u", output_filename, idx + 1 );    
                    if ( rc == 0 )
                    {
                        rc = execute_concat( dir,
                            s_filename . S . addr,
                            l,
                            buf_size,
                            show_progress,
                            print_to_stdout,
                            force,
                            compress );
                        release_SBuffer( &s_filename );
                    }
                }
                idx += 1;
            }
        }
    }
    return rc;
}
