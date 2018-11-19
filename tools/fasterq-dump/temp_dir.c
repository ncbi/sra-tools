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

#include "temp_dir.h"
#include "helper.h"

#include <klib/out.h>
#include <klib/printf.h>
#include <kproc/procmgr.h>

#include <os-native.h>
#include <sysalloc.h>

#define HOSTNAMELEN 64
#define DFLT_HOST_NAME "host"
#define DFLT_PATH_LEN 4096

typedef struct temp_dir
{
    char hostname[ HOSTNAMELEN ];
    char path[ DFLT_PATH_LEN ];
    uint32_t pid;
} temp_dir;

void destroy_temp_dir( struct temp_dir * self )
{
    if ( self != NULL )
    {
        free( (void *)self );
    }
}

static rc_t get_pid_and_hostname( temp_dir * self )
{
    struct KProcMgr * proc_mgr;
    rc_t rc = KProcMgrMakeSingleton ( &proc_mgr );
    if ( rc != 0 )
        ErrMsg( "cannot access process-manager" );
    else
    {
        rc = KProcMgrGetPID ( proc_mgr, & self -> pid );
        if ( rc == 0 )
        {
            rc = KProcMgrGetHostName ( proc_mgr, self -> hostname, sizeof self -> hostname );
            if ( rc != 0 )
            {
                size_t num_writ;
                rc = string_printf( self -> hostname, sizeof self -> hostname, &num_writ, DFLT_HOST_NAME );
            }
        }
        KProcMgrRelease ( proc_mgr );
    }
    return rc;
}

static rc_t generate_dflt_path( temp_dir * self )
{
    size_t num_writ;
    return string_printf( self -> path, sizeof self -> path, &num_writ,
                         "fasterq.tmp.%s.%u/", self -> hostname, self -> pid );
}

static rc_t generate_sub_path( temp_dir * self, const char * requested )
{
    rc_t rc;
    size_t num_writ;
    bool es = ends_in_slash( requested );
    if ( es )
        rc = string_printf( self -> path, sizeof self -> path, &num_writ,
                            "%sfasterq.tmp.%s.%u/", requested, self -> hostname, self -> pid );
    else
        rc = string_printf( self -> path, sizeof self -> path, &num_writ,
                            "%s/fasterq.tmp.%s.%u/", requested, self -> hostname, self -> pid );
    return rc;
}

rc_t make_temp_dir( struct temp_dir ** obj, const char * requested, KDirectory * dir )
{
    rc_t rc = 0;
    if ( obj == NULL || dir == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c make_temp_dir() -> %R", rc );
    }
    else
    {
        temp_dir * o = calloc( 1, sizeof * o );
        if ( o == NULL )
        {
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
            ErrMsg( "temp_dir.c make_temp_dir().calloc( %d ) -> %R", ( sizeof * o ), rc );
        }
        else
        {
            rc = get_pid_and_hostname( o );
            if ( rc == 0 )
            {
                if ( requested == NULL )
                    rc = generate_dflt_path( o );
                else
                    rc = generate_sub_path( o, requested );
            }
            
            if ( rc == 0 )
            {
                if ( !dir_exists( dir, "%s", o -> path ) ) /* helper.c */
                {
                    KCreateMode create_mode = kcmCreate | kcmParents;
                    rc = KDirectoryCreateDir ( dir, 0775, create_mode, "%s", o -> path );
                    if ( rc != 0 )
                        ErrMsg( "temp_dir.c make_temp_dir().KDirectoryCreateDir( '%s' ) -> %R", o -> path, rc );
                }
            }

            if ( rc == 0 )
                *obj = o;
            else
                destroy_temp_dir( o );
        }
        
    }
    return rc;
}

const char * get_temp_dir( struct temp_dir * self )
{
    if ( self != NULL )
        return self -> path;
    return "unknown";
}

rc_t generate_lookup_filename( const struct temp_dir * self, char * dst, size_t dst_size )
{
    rc_t rc;
    if ( self == NULL || dst == NULL || dst_size == 0 )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c generate_lookup_filename() -> %R", rc );
    }
    else
    {
        size_t num_writ;
        rc = string_printf( dst, dst_size, &num_writ,
                "%s%s.%u.lookup",
                self -> path, self -> hostname, self -> pid );
        if ( rc != 0 )
            ErrMsg( "temp_dir.c generate_lookup_filename().printf() -> %R", rc );
                
    }
    return rc;
}

rc_t generate_bg_sub_filename( const struct temp_dir * self, char * dst, size_t dst_size, uint32_t product_id )
{
    rc_t rc;
    if ( self == NULL || dst == NULL || dst_size == 0 )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c generate_bg_sub_filename() -> %R", rc );
    }
    else
    {
        size_t num_writ;        
        rc = string_printf( dst, dst_size, &num_writ,
                "%sbg_sub_%s_%u_%u.dat",
                self -> path, self -> hostname, self -> pid, product_id );
        if ( rc != 0 )
            ErrMsg( "temp_dir.c generate_bg_sub_filename().printf() -> %R", rc );
    }
    return rc;
}

rc_t generate_bg_merge_filename( const struct temp_dir * self, char * dst, size_t dst_size, uint32_t product_id )
{
    rc_t rc;
    if ( self == NULL || dst == NULL || dst_size == 0 )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c generate_bg_sub_filename() -> %R", rc );
    }
    else
    {
        size_t num_writ;        
        rc = string_printf( dst, dst_size, &num_writ,
                "%sbg_merge_%s_%u_%u.dat",
                self -> path, self -> hostname, self -> pid, product_id );
        if ( rc != 0 )
            ErrMsg( "temp_dir.c generate_bg_sub_filename().printf() -> %R", rc );
    }
    return rc;
}


rc_t make_joined_filename( const struct temp_dir * self, char * dst, size_t dst_size,
                           const char * accession, uint32_t id )
{
    rc_t rc;
    if ( self == NULL || dst == NULL || dst_size == 0 || accession == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c make_joined_filename() -> %R", rc );
    }
    else
    {
        size_t num_writ;
        rc = string_printf( dst, dst_size, &num_writ, "%s%s.%s.%u.%u",
                                 self -> path,
                                 accession,
                                 self -> hostname,
                                 self -> pid,
                                 id );
        if ( rc != 0 )
            ErrMsg( "temp_dir.c make_joined_filename().string_printf() -> %R", rc );
    }
    return rc;
}

rc_t remove_temp_dir( const struct temp_dir * self, KDirectory * dir )
{
    rc_t rc;
    if ( self == NULL || dir == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
        ErrMsg( "temp_dir.c remove_temp_dir() -> %R", rc );
    }
    else
    {
        bool tmp_exists = dir_exists( dir, "%s", self -> path ); /* helper.c */
        if ( tmp_exists )
        {
            rc = KDirectoryClearDir ( dir, true, "%s", self -> path );
            if ( rc != 0 )
                ErrMsg( "temp_dir.c remove_temp_dir.KDirectoryClearDir( '%s' ) -> %R", self -> path, rc );
            else
            {
                tmp_exists = dir_exists( dir, "%s", self -> path ); /* helper.c */
                if ( tmp_exists )
                {
                    rc = KDirectoryRemove ( dir, true, "%s", self -> path );
                    if ( rc != 0 )
                        ErrMsg( "temp_dir.c remove_temp_dir.KDirectoryRemove( '%s' ) -> %R", self -> path, rc );
                }
            }
        }
    }
    return rc;
}