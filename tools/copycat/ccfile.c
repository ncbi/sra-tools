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
 */

#include <klib/log.h>
#include <klib/debug.h>
#include <klib/rc.h>
#include <kfs/file.h>
#include <kfs/countfile.h>
#include <sysalloc.h>

#include <assert.h>
#include <stdlib.h>

/* ======================================================================
 * CCFile
 */
/* -----
 * define the specific types to be used in the templatish/inheritancish
 * definition of vtables and their elements
 */
typedef struct CCFile CCFile;
#define KFILE_IMPL struct CCFile
#include <kfs/impl.h>


/*-----------------------------------------------------------------------
 * CCFile
 */
struct CCFile
{
    KFile	dad;
    KFile *	original;
    rc_t *      prc;
};


/* ----------------------------------------------------------------------
 * Destroy
 *
 */
static
rc_t CC CCFileDestroy (CCFile *self)
{
    rc_t rc = KFileRelease (self->original);
    free (self);
    return rc;
}

/* ----------------------------------------------------------------------
 * GetSysFile
 *  returns an underlying system file object
 *  and starting offset to contiguous region
 *  suitable for memory mapping, or NULL if
 *  no such file is available.
 *
 * bytes could not be counted if memory mapped so this is disallowed
 */

static
struct KSysFile *CC CCFileGetSysFile (const CCFile *self, uint64_t *offset)
{
    return KFileGetSysFile (self->original, offset);
}

/* ----------------------------------------------------------------------
 * RandomAccess
 *
 *  returns 0 if random access, error code otherwise
 */
static
rc_t CC CCFileRandomAccess (const CCFile *self)
{
    assert (self != NULL);
    assert (self->original != NULL);
    return KFileRandomAccess (self->original);
}

/* ----------------------------------------------------------------------
 * Type
 *  returns a KFileDesc
 *  not intended to be a content type,
 *  but rather an implementation class
 */
static
uint32_t CC CCFileType (const CCFile *self)
{
    return KFileType (self->original);
}

/* ----------------------------------------------------------------------
 * Size
 *  returns size in bytes of file
 *
 *  "size" [ OUT ] - return parameter for file size
 */
static
rc_t CC CCFileSize (const CCFile *self, uint64_t *size)
{
    return KFileSize (self->original, size);
}

/* ----------------------------------------------------------------------
 * SetSize
 *  sets size in bytes of file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t CC CCFileSetSize (CCFile *self, uint64_t size)
{
    return KFileSetSize (self->original, size);
}

/* ----------------------------------------------------------------------
 * Read
 *  read file from known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ OUT ] and "bsize" [ IN ] - return buffer for read
 *
 *  "num_read" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of bytes actually read
 */
static
rc_t CC CCFileRead	(const CCFile *self,
                         uint64_t pos,
                         void *buffer,
                         size_t bsize,
                         size_t *num_read)
{
    rc_t	rc;
    
    rc = KFileRead (self->original, pos, buffer, bsize, num_read);
    if (*self->prc == 0)
        *((CCFile*)self)->prc = rc;
    return rc;
}

/* ----------------------------------------------------------------------
 * Write
 *  write file at known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ IN ] and "size" [ IN ] - data to be written
 *
 *  "num_writ" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of bytes actually written
 *
 */
static
rc_t CC CCFileWrite (CCFile *self, uint64_t pos,
                     const void *buffer, size_t bsize,
                     size_t *num_writ)
{
    rc_t rc;

    rc = KFileWrite (self->original, pos, buffer, bsize, num_writ);
    if (*self->prc == 0)
        *self->prc = rc;
    return rc;
}

static const KFile_vt_v1 vtCCFile =
{
    /* version */
    1, 1,

    /* 1.0 */
    CCFileDestroy,
    CCFileGetSysFile,
    CCFileRandomAccess,
    CCFileSize,
    CCFileSetSize,
    CCFileRead,
    CCFileWrite,

    /* 1.1 */
    CCFileType
};

/* ----------------------------------------------------------------------
 * CCFileMake
 *  create a new file object
 */

static
rc_t CCFileMake (CCFile ** pself,
                 KFile * original,
                 rc_t * prc)
{
    CCFile * self;
    rc_t rc;
    /* needs to be better */
    assert (pself);
    assert (original);
    assert (prc);

    self = malloc (sizeof (CCFile));
    if (self == NULL)	/* allocation failed */
    {
	/* fail */
	rc = RC (rcFS, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    else
    {
	rc = KFileInit (&self->dad,			/* initialize base class */
			(const KFile_vt*)&vtCCFile,/* VTable for CCFile */
            "CCFile", "no-name",
			original->read_enabled,
			original->write_enabled);
	if (rc == 0)
	{
            rc = KFileAddRef (original);
            if (rc == 0)
            {
                self->original = original;
                self->prc = prc;
                *pself = self;
                return *prc = 0;
            }
	}
	/* fail */
	free (self);
    }
    *pself = NULL;
    *prc = rc;
    return rc;
}

LIB_EXPORT rc_t CC CCFileMakeRead (const KFile ** self, const KFile * original,
                                   rc_t * prc)
{
    return CCFileMake ((CCFile **)self, (KFile*)original, prc);
}
LIB_EXPORT rc_t CC CCFileMakeUpdate (KFile ** self, KFile * original,
                                     rc_t * prc)
{
    return CCFileMake ((CCFile **)self, (KFile*)original, prc);
}
LIB_EXPORT rc_t CC CCFileMakeWrite (KFile ** self, KFile * original,
                                    rc_t * prc)
{
    return CCFileMake ((CCFile **)self, (KFile*)original, prc);
}

/* end of file countfile.c */

