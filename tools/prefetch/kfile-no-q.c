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

#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/arc.h>
#include <kfs/sra.h>

#include <assert.h>
#include <string.h>

static
bool CC qual_col_filter ( const KDirectory * dir, const char * path, void * data )
{
    if ( strstr(path, "col/QUALITY") == NULL )
    {
        return true;
    }
    
    return false;
}

static
rc_t CC sort_none (const KDirectory * d, struct Vector * v)
{
    return 0;
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
