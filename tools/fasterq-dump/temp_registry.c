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
#include "progress_thread.h"

#include <klib/vector.h>
#include <klib/out.h>
#include <klib/namelist.h>
#include <kproc/lock.h>
#include <kfs/filetools.h>

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
                    rc = Add_File_to_Cleanup_Task ( self -> cleanup_task, filename );
            }
            KLockUnlock ( self -> lock );
        }
    }
    return rc;
}

/* -------------------------------------------------------------------- */
typedef struct on_list_ctx
{
    KDirectory * dir;
    uint64_t res;
} on_list_ctx;

static void CC on_list( void *item, void *data )
{
    if ( item != NULL )
    {
        on_list_ctx * olc = data;
        olc -> res += total_size_of_files_in_list( olc -> dir, item );
    }
}

static uint64_t total_size( KDirectory * dir, Vector * v )
{
    on_list_ctx olc = { dir, 0 };
    VectorForEach ( v, false, on_list, &olc );
    return olc . res;
}

/* -------------------------------------------------------------------- */
typedef struct on_count_ctx
{
    uint32_t idx;
    uint32_t valid;
    uint32_t first;
} on_count_ctx;

static void CC on_count( void *item, void *data )
{
    on_count_ctx * occ = data;
    if ( item != NULL )
    {
        if ( occ -> valid == 0 )
            occ -> first = occ -> idx;
        occ -> valid++;
    }
    occ -> idx ++;
}

static uint32_t count_valid_entries( Vector * v, uint32_t * first )
{
    on_count_ctx occ = { 0, 0, 0 };
    VectorForEach ( v, false, on_count, &occ );
    *first = occ . first;
    return occ . valid;
}

/* -------------------------------------------------------------------- */
typedef struct cmn_merge
{
    KDirectory * dir;
    const char * output_filename;
    size_t buf_size;
    struct bg_progress * progress;
    bool force;
    bool append;
    compress_t compress;
} cmn_merge;

typedef struct merge_data
{
    cmn_merge * cmn;
    VNamelist * files;
    uint32_t idx;
} merge_data;

static rc_t CC merge_thread_func( const KThread *self, void *data )
{
    merge_data * md = data;
    SBuffer s_filename;
    rc_t rc = split_filename_insert_idx( &s_filename, 4096,
                            md -> cmn -> output_filename, md -> idx ); /* helper.c */
    if ( rc == 0 )
    {
        VNamelistReorder ( md -> files, false );        
        rc = execute_concat( md -> cmn -> dir,
            s_filename . S . addr,
            md -> files,
            md -> cmn -> buf_size,
            md -> cmn -> progress,
            md -> cmn -> force,
            md -> cmn -> append,
            md -> cmn -> compress ); /* concatenator.c */
        release_SBuffer( &s_filename ); /* helper.c */
    }
    free( ( void * ) md );
    return rc;
}

typedef struct on_merge_ctx
{
    cmn_merge * cmn;
    uint32_t idx;
    Vector threads;
} on_merge_ctx;

static void CC on_merge( void *item, void *data )
{
    on_merge_ctx * omc = data;
    if ( item != NULL )
    {
        merge_data * md = calloc( 1, sizeof * md );
        if ( md != NULL )
        {
            rc_t rc;
            KThread * thread;
            
            md -> cmn = omc -> cmn;
            md -> files = item;
            md -> idx = omc -> idx;
            
            rc = helper_make_thread( &thread, merge_thread_func, md, THREAD_DFLT_STACK_SIZE );
            if ( rc != 0 )
                ErrMsg( "temp_registry.c helper_make_thread( on_merge #%d ) -> %R", omc -> idx, rc );
            else
            {
                rc = VectorAppend( &omc -> threads, NULL, thread );
                if ( rc != 0 )
                    ErrMsg( "temp_registry.c VectorAppend( merge-thread #%d ) -> %R", omc -> idx, rc );
            }
        }
    }
    omc -> idx ++;
}

/* -------------------------------------------------------------------- */
rc_t temp_registry_merge( temp_registry * self,
                          KDirectory * dir,
                          const char * output_filename,
                          size_t buf_size,
                          bool show_progress,
                          bool force,
                          compress_t compress,
                          bool append )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else if ( output_filename == NULL || dir == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        struct bg_progress * progress = NULL;
        
        if ( show_progress )
        {
            rc = KOutMsg( "concat :" );
            if ( rc == 0 )
            {
                uint64_t total = total_size( dir, &self -> lists );
                rc = bg_progress_make( &progress, total, 0, 0 ); /* progress_thread.c */
            }
        }
        
        if ( rc == 0 )
        {
            uint32_t first;
            uint32_t count = count_valid_entries( &self -> lists, &first ); /* above */
            if ( count == 1 )
            {
                /* we have only ONE set of files... */
                VNamelist * l = VectorGet ( &self -> lists, first );
                VNamelistReorder ( l, false );
                rc = execute_concat( dir,
                    output_filename,
                    l,
                    buf_size,
                    progress,
                    force, append,
                    compress ); /* concatenator.c */
            }
            else if ( count > 1 )
            {
                /* we have MULTIPLE sets of files... */
                cmn_merge cmn = { dir, output_filename, buf_size, progress, force, append, compress };
                on_merge_ctx omc = { &cmn, 0 };
                VectorInit( &omc . threads, 0, count );
                VectorForEach ( &self -> lists, false, on_merge, &omc );
                join_and_release_threads( &omc . threads ); /* helper.c */
            }
            
            bg_progress_release( progress ); /* progress_thread.c ( ignores NULL )*/
        }
    }
    return rc;
}

/* -------------------------------------------------------------------------------- */

static rc_t temp_on_line_to_stdout( const String * line, void * data )
{
    return KOutMsg( "%S\n", line );
}

typedef struct print_to_stdout_ctx
{
    KDirectory * dir;
    size_t buf_size;
} print_to_stdout_ctx;

static void CC on_print_to_stdout( void * item, void * data )
{
    const VNamelist * l = ( const VNamelist * )item;
    if ( l != NULL )
    {
        const print_to_stdout_ctx * c = ( const print_to_stdout_ctx * )data;

        uint32_t count;
        rc_t rc = VNameListCount ( l, &count );
        if ( rc != 0 )
            ErrMsg( "on_print_to_stdout().VNameListCoun() -> %R", rc );
        else
        {
            uint32_t idx;
            VNamelistReorder ( ( VNamelist * )l, false );
            for ( idx = 0; rc == 0 && idx < count; ++idx )
            {
                const char * filename;
                rc = VNameListGet( l, idx, &filename );
                if ( rc != 0 )
                    ErrMsg( "on_print_to_stdout().VNameListGet( #%d ) -> %R", idx, rc );
                else
                {
                    const struct KFile * src;
                    rc = make_buffered_for_read( c -> dir, &src, filename, c -> buf_size ); /* helper.c */
                    if ( rc == 0 )
                    {
                        /* libs/kfs/from_to_namelist.c, libs/interfaces/filetools.h */
                        rc = ProcessFileLineByLine( src, temp_on_line_to_stdout, NULL );
                        if ( rc != 0 )
                            ErrMsg( "on_print_to_stdout().ProcessFileLineByLine( '%s' ) -> %R", filename, rc );
                        KFileRelease( src );
                    }

                    {
                        rc_t rc1 = KDirectoryRemove( c -> dir, true, "%s", filename );
                        if ( rc1 != 0 )
                            ErrMsg( "on_print_to_stdout.KDirectoryRemove( '%s' ) -> %R", filename, rc1 );
                    }
                }
            }
        }
    }
}

rc_t temp_registry_to_stdout( temp_registry * self,
                              KDirectory * dir,
                              size_t buf_size )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else if ( dir == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcNull );
    else
    {
        print_to_stdout_ctx c;
        c . dir = dir;
        c . buf_size = buf_size;
        
        VectorForEach ( &( self -> lists ), false, on_print_to_stdout, &c );
    }
    return rc;
}
