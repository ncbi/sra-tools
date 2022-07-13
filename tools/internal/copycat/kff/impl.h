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

#ifndef _h_kfs_impl_
#define _h_kfs_impl_

#include <klib/rc.h>
#include <atomic.h>

#include "kff/fileformat.h"
#include "kff/fileformat-priv.h"

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
struct KDirectory;

typedef union KFileFormat_vt KFileFormat_vt;


/*--------------------------------------------------------------------------
 * KFileFormat
 *  a virtual file
 */
struct KFileFormat
{
    const KFileFormat_vt *vt;
    atomic32_t refcount;
    KFFTables * tables;
};

#ifndef KFILEFORMAT_IMPL
#define KFILEFORMAT_IMPL KFileFormat
#endif

typedef struct KFileFormat_vt_v1 KFileFormat_vt_v1;
struct KFileFormat_vt_v1
{
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    rc_t (*destroy) (KFILEFORMAT_IMPL * self);
    rc_t (*gettypebuff) (const KFILEFORMAT_IMPL *self, const void * buff, size_t buff_len,
			 KFileFormatType * type, KFileFormatClass * class,
			 char * description, size_t descriptionmax,
			 size_t * descriptionlength);
    rc_t (*gettypepath) (const KFILEFORMAT_IMPL *self, const struct KDirectory * dir, const char * path,
			 KFileFormatType * type, KFileFormatClass * class,
			 char * description, size_t descriptionmax,
			 size_t * descriptionlength);

    /* end minor version == 0 */
    /* start minor version == 1 */
    /* end minor version == 1 */
    /* end version == 1.x */
};

union KFileFormat_vt
{
    KFileFormat_vt_v1 v1;
};

/* Init
 *  initialize a newly allocated file object
 */
rc_t KFileFormatInit (KFileFormat *self, const KFileFormat_vt *vt,
		      const char * typeAndClass, size_t len);

/* Destroy
 *  destroy file
 */
rc_t KFileFormatDestroy ( KFileFormat *self );

/* GetSysFile
 *  returns an underlying system file object
 *  and starting offset to contiguous region
 *  suitable for memory mapping, or NULL if
 *  no such file is available.
 */
struct KSysFile *KFileFormatGetSysFile ( const KFileFormat *self, uint64_t *offset );




#ifdef __cplusplus
}
#endif

#endif /* _h_kfs_impl_ */
