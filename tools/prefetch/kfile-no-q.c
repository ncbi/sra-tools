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

#include "kfile-no-q.h"

#include <kapp/main.h>

#include <klib/log.h>
#include <klib/status.h>

#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/arc.h>
#include <kfs/sra.h>

#include <assert.h>
#include <string.h>

#define READ_CACHE_BLOCK_SIZE (1024*1024)

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define STS_FIN 3

typedef rc_t ( * FileProcessFn ) ( const KDirectory_v1 * dir, const char * name );

typedef struct {
    FileProcessFn processFn;
    
    bool elimQuals;
} VisitParams;

static
bool qual_col_filter ( const KDirectory * self, const char * path, void * data )
{
    if ( strstr(path, "col/QUALITY") == NULL )
    {
        return true;
    }
    
    return false;
}

static
rc_t sort_none (const KDirectory * self, struct Vector * v)
{
    return 0;
}

static
rc_t visit_cb ( const KDirectory * self, uint32_t type, const char * name, void * data )
{
    rc_t rc = 0;
    VisitParams * params = data;
    
    assert(params);
    
    if (params->processFn == NULL)
        return rc;
    
    if (type == kptFile)
    {
        if (params->elimQuals)
        {
            char path[PATH_MAX];
            rc = KDirectoryResolvePath ( self, true, path, sizeof path, name );
        
            if (rc == 0 && ( strstr(path, "/col/QUALITY/") == NULL ) )
            {
                rc = params->processFn(self, name);
            }
        }
        else
        {
            rc = params->processFn(self, name);
        }
    }
    
    return rc;
}

static
rc_t file_read ( const KDirectory * dir, const char * name )
{
    rc_t rc;
    const KFile * kfile;
    char buffer[READ_CACHE_BLOCK_SIZE];
    uint64_t pos = 0;
    uint64_t prevPos = 0;
    size_t num_read;
    
    
    rc = KDirectoryOpenFileRead ( dir, &kfile, name );
    if (rc != 0)
        return rc;
    
    do {
        bool print = pos - prevPos > 200000000;
        rc = Quitting();
        
        if (rc == 0) {
            if (print) {
                STSMSG(STS_FIN,
                       ("Reading %lu bytes from pos. %lu", READ_CACHE_BLOCK_SIZE, pos));
            }
            rc = KFileRead(kfile,
                           pos, buffer, READ_CACHE_BLOCK_SIZE, &num_read);
            if (rc != 0) {
                PLOGERR(klogInt, (klogInt, rc, "KFileRead failed for file $(name)",
                                  "name=%S", name));
            }
            else {
                pos += num_read;
            }
            
            if (print) {
                prevPos = pos;
            }
        }
    } while (rc == 0 && num_read > 0);
    
    KFileRelease(kfile);

    return rc;
}

rc_t CC KSraFileNoQuals( const struct KFile * self,
                         const struct KFile ** kfile )
{
    rc_t rc;
    struct KDirectory * kdir_native, * kdir_virtual;
    const struct KFile * new_kfile;
    
    assert(self != NULL);
    assert(kfile != NULL);
    
    rc = KDirectoryNativeDir(&kdir_native);
    if (rc != 0)
    {
        return rc;
    }
    
    rc = KDirectoryOpenArcDirRead_silent_preopened ( kdir_native, (const struct KDirectory **)&kdir_virtual, false, "/virtual", tocKFile,
                                                      ( void* ) self, KArcParseSRA, NULL, NULL );
    if (rc == 0)
    {
        rc = KDirectoryOpenTocFileRead (kdir_virtual, &new_kfile, sraAlign4Byte, qual_col_filter, NULL, sort_none );
        KDirectoryRelease (kdir_virtual);
        
        if (rc == 0)
        {
            *kfile = new_kfile;
        }
    }
    
    KDirectoryRelease (kdir_native);
    
    return 0;
}

rc_t CC KSraReadCacheFile( const struct KFile * self, bool elimQuals )
{
    rc_t rc;
    struct KDirectory * kdir_native, * kdir_virtual;

    
    assert(self != NULL);
    
    rc = KDirectoryNativeDir(&kdir_native);
    if (rc != 0)
    {
        return rc;
    }
    
    rc = KDirectoryOpenArcDirRead_silent_preopened ( kdir_native, (const struct KDirectory **)&kdir_virtual, false, "/virtual", tocKFile,
                                                    ( void* ) self, KArcParseSRA, NULL, NULL );
    if (rc == 0)
    {
        VisitParams params;
        params.elimQuals = elimQuals;
        params.processFn = &file_read;
        
        rc = KDirectoryVisit(kdir_virtual, true, visit_cb, &params, ".");
        KDirectoryRelease (kdir_virtual);
    }
    
    KDirectoryRelease (kdir_native);
    
    return 0;
}
