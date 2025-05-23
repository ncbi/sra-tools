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

#include <kfs/extern.h>
/* #include <klib/container.h> */
/* #include <klib/vector.h> */
/* #include <klib/pbstree.h> */
/* #include <klib/text.h> */
/* #include <kfs/file.h> */
/* #include <assert.h> */
/* #include <limits.h> */
/* #include <stdio.h> */
/* #include <stdlib.h> */
/* #include <string.h> */

/* #include <klib/container.h> */
/* #include <klib/vector.h> */
/* #include <klib/pbstree.h> */
/* #include <klib/text.h> */
#include <klib/log.h>
#include <klib/rc.h>
#include <kfs/file.h>
#include <kfs/teefile.h>
#include <sysalloc.h>

#include <assert.h>
/* #include <limits.h> */
/* #include <stdio.h> */
#include <stdlib.h>
/* #include <string.h> */

/* ======================================================================
 * KTeeFile
 *  a file inside an archive
 */

/* -----
 * define the specific types to be used in the templatish/inheritancish
 * definition of vtables and their elements
 */
#define KFILE_IMPL struct KTeeFile
#include <kfs/impl.h>

static rc_t CC KTeeFileDestroy (KTeeFile *self);
static struct KSysFile *CC KTeeFileGetSysFile (const KTeeFile *self,
					    uint64_t *offset);
static rc_t CC KTeeFileRandomAccessRead (const KTeeFile *self);
static rc_t CC KTeeFileRandomAccessUpdate (const KTeeFile *self);
static uint32_t CC KTeeFileType (const KTeeFile *self);
static rc_t CC KTeeFileSize (const KTeeFile *self, uint64_t *size);
static rc_t CC KTeeFileSetSizeRead (KTeeFile *self, uint64_t size);
static rc_t CC KTeeFileSetSizeUpdate (KTeeFile *self, uint64_t size);
static rc_t CC KTeeFileRead (const KTeeFile *self, uint64_t pos,
			  void *buffer, size_t bsize, size_t *num_read);
static rc_t CC KTeeFileWriteRead (KTeeFile *self, uint64_t pos, const void *buffer,
			       size_t size, size_t *num_writ);
static rc_t CC KTeeFileWriteUpdate (KTeeFile *self, uint64_t pos, const void *buffer,
				 size_t size, size_t *num_writ);


static const KFile_vt_v1 vtKTeeFileRead =
{
    /* version */
    1, 1,

    /* 1.0 */
    KTeeFileDestroy,
    KTeeFileGetSysFile,
    KTeeFileRandomAccessRead,
    KTeeFileSize,
    KTeeFileSetSizeRead,
    KTeeFileRead,
    KTeeFileWriteRead,

    /* 1.1 */
    KTeeFileType
};
static const KFile_vt_v1 vtKTeeFileUpdate =
{
    /* version */
    1, 1,

    /* 1.0 */
    KTeeFileDestroy,
    KTeeFileGetSysFile,
    KTeeFileRandomAccessUpdate,
    KTeeFileSize,
    KTeeFileSetSizeUpdate,
    KTeeFileRead,
    KTeeFileWriteUpdate,

    /* 1.1 */
    KTeeFileType
};


/*-----------------------------------------------------------------------
 * KTeeFile
 *  an archive file including tar and sra
 */
struct KTeeFile
{
    KFile	dad;
    uint64_t	maxposition;
    KFile *	original;
    KFile *	copy;
};

static
rc_t KTeeFileSeek (const KTeeFile *cself, uint64_t pos)
{
    KTeeFile * self;
    rc_t rc = 0;
    size_t num_read;
    uint8_t buff [ 32 * 1024 ];

    self = (KTeeFile *)cself;
    /* seek to "pos" */
    while (self->maxposition < pos)
    {
        /* maximum to read in this buffer */
        size_t to_read = sizeof buff;
        if (self->maxposition + sizeof buff > pos )
            to_read = (size_t) (pos - self->maxposition);

        /* read bytes */
        rc = KFileRead (&self->dad, self->maxposition, buff, to_read, &num_read );
        if ( rc != 0 )
            break;

        /* detect EOF */
        if (num_read == 0)
        {
            break;
        }
    }

    return rc;
}


/* ----------------------------------------------------------------------
 * KTeeFileMake
 *  create a new file object
 */

static
rc_t KTeeFileMake (KTeeFile ** self,
		   KFile * original,
		   KFile * copy,
		   const KFile_vt * vt,
		   bool read_enabled,
		   bool write_enabled)
{
    rc_t	rc;
    KTeeFile *	pF;

    /* -----
     * we can not accept any of the three pointer parameters as NULL
     */
    assert (self != NULL);
    assert (original != NULL);
    assert (copy != NULL);

    /* -----
     * the enables should be true or false
     */
    assert ((read_enabled == true)||(read_enabled == false));
    assert ((write_enabled == true)||(write_enabled == false));

    /* -----
     * get space for the object
     */
    pF = malloc (sizeof (KTeeFile));
    if (pF == NULL)	/* allocation failed */
    {
	/* fail */
	rc = RC (rcFS, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    else
    {
	rc = KFileInit (&pF->dad,			/* initialize base class */
			vt,			 	/* VTable for KTeeFile */
            "KTeeFile", "no-name",
			read_enabled,			/* read allowed */
			write_enabled);			/* write disallowed */
	if (rc == 0)
	{
/* take over the existing KFile Reference for original and copy*/
	    /* succeed */
	    pF->original = original;
	    pF->copy = copy;
	    pF->maxposition = 0;
	    *self = pF;
	    return 0;
	}
	/* fail */
	free (pF);
    }
    return rc;
}

LIB_EXPORT rc_t CC KFileMakeTeeRead (const KFile ** self, const KFile * original, KFile * copy)
{
    return KTeeFileMake ((KTeeFile **)self, (KFile*)original, copy,
			 (const KFile_vt*)&vtKTeeFileRead, true, false);
}

LIB_EXPORT rc_t CC KFileMakeTeeUpdate (KFile ** self, KFile * original, KFile * copy)
{
    return KTeeFileMake ((KTeeFile **)self, original, copy,
			 (const KFile_vt*)&vtKTeeFileUpdate, true, true);
}

/* ----------------------------------------------------------------------
 * Destroy
 *
 */
static
rc_t CC KTeeFileDestroy (KTeeFile *self)
{
    rc_t rc;
    uint64_t last_max;

    assert (self != NULL);

    do
    {
        last_max = self->maxposition;

        /* keep seeking ahead by a Gigabyte until we read no more */
	rc = KTeeFileSeek (self, last_max + 1024*1024*1024);
	if (rc != 0)
	    return rc;

    } while (last_max < self->maxposition);

    rc = KFileRelease (self->original);
    if ( rc == 0 )
    {
        KFileRelease (self->copy);
        free (self);
    }
    return rc;
}

/* ----------------------------------------------------------------------
 * GetSysFile
 *  returns an underlying system file object
 *  and starting offset to contiguous region
 *  suitable for memory mapping, or NULL if
 *  no such file is available.
 *
 * We cant allow memory mapping a tee file as the read?writes ar needed
 * to trigger the writes to the copy KFile
 */

static
struct KSysFile *CC KTeeFileGetSysFile (const KTeeFile *self, uint64_t *offset)
{
    /* parameters must be non-NULL */
    assert (self != NULL);
    assert (offset != NULL);

    return NULL;
}

/* ----------------------------------------------------------------------
 * RandomAccess
 *
 *  returns 0 if random access, error code otherwise
 *
 * Update needs to be able to seek both original and copy while read
 * only needs to be able to seek the original.
 */
static
rc_t CC KTeeFileRandomAccessUpdate (const KTeeFile *self)
{
    rc_t rc;
    assert (self != NULL);
    rc = KFileRandomAccess (self->original);
    if (rc == 0)
	rc = KFileRandomAccess (self->copy);
    return rc;
}
static
rc_t CC KTeeFileRandomAccessRead (const KTeeFile *self)
{
    rc_t rc;
    assert (self != NULL);
    rc = KFileRandomAccess (self->original);
    return rc;
}

/* ----------------------------------------------------------------------
 * Type
 *  returns a KFileDesc
 *  not intended to be a content type,
 *  but rather an implementation class
 */
static
uint32_t CC KTeeFileType (const KTeeFile *self)
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
rc_t CC KTeeFileSize (const KTeeFile *self, uint64_t *size)
{
    rc_t	rc;
    uint64_t	fsize;

    assert (self != NULL);
    assert (size != NULL);

    rc = KFileSize (self->original, &fsize);

    if (rc == 0)
    {
	/* success */
	*size = fsize;
    }
    /* pass along RC value */
    return rc;
}

/* ----------------------------------------------------------------------
 * SetSize
 *  sets size in bytes of file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t CC KTeeFileSetSizeUpdate (KTeeFile *self, uint64_t size)
{
    rc_t rc;

    rc = KFileSetSize (self->original, size);
    if (rc == 0)
	rc = KFileSetSize (self->copy, size);
    return rc;
}
static
rc_t CC KTeeFileSetSizeRead (KTeeFile *self, uint64_t size)
{
    return RC (rcFS, rcFile, rcUpdating, rcSelf, rcUnsupported);
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
rc_t CC KTeeFileRead	(const KTeeFile *cself,
			 uint64_t pos,
			 void *buffer,
			 size_t bsize,
			 size_t *num_read)
{
    KTeeFile * 	self;
    uint64_t	maxposition;
    size_t	read;
    size_t	written;
    size_t	sofar;
    rc_t	rc;


    /* -----
     * self and buffer were validated as not NULL before calling here
     *
     * So get the KTTOCNode type: chunked files and contiguous files 
     * are read differently.
     */
    assert (cself != NULL);
    assert (buffer != NULL);
    assert (num_read != NULL);
    assert (bsize != 0);

    rc = 0;
    read = 0;
    self = (KTeeFile*)cself;
    maxposition = self->maxposition;
    if (pos > maxposition)
	rc = KTeeFileSeek (self, pos);
    if (rc == 0)
    {
	rc = KFileRead (self->original, pos, buffer, bsize, &read);
	if (rc == 0)
	{
	    if (pos + read > maxposition)
	    {
		for ( sofar = (size_t)( maxposition - pos );
			  sofar < read;
		      sofar += written)
		{
		    rc = KFileWrite (self->copy, pos + sofar, (uint8_t*)buffer + sofar,
				     read - sofar, &written);
		    if (rc != 0)
			break;
		    if (written == 0)
		    {
			LOGERR (klogErr, rc, "Failure to write to copy in KTeeFileRead");
			rc = RC (rcFS, rcFile, rcReading, rcFile, rcIncomplete);
		    break;
		    }
		}
		maxposition = pos + sofar;
		if (maxposition > self->maxposition)
		    self->maxposition = maxposition;
	    }
	}
    }
    *num_read = read;
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
 * Unsupported as we now treat archives as READ ONLY
 */
static
rc_t CC KTeeFileWriteUpdate (KTeeFile *self, uint64_t pos,
		       const void *buffer, size_t bsize,
		       size_t *num_writ)
{
    uint64_t	max_position;
    size_t	writ;
    size_t	written;
    size_t	sofar;
    rc_t	rc;

    assert (self != NULL);
    assert (buffer != NULL);
    assert (num_writ != NULL);
    assert (bsize != 0);

    writ = 0;
    rc = 0;
    if (pos > self->maxposition)
	rc = KTeeFileSeek (self, pos);
    if (rc == 0)
    {
	rc = KFileWrite (self->original, pos, buffer, bsize, &writ);
	if (rc == 0)
	{
	    for ( sofar = written = 0; sofar < writ; sofar += written)
	    {
		rc = KFileWrite (self->copy, pos + sofar, (uint8_t*)buffer + sofar,
			     writ - sofar, &written);
		if (rc != 0)
		    break;
		if (written == 0)
		{
		    rc = RC (rcFS, rcFile, rcReading, rcFile, rcIncomplete);
		    LOGERR (klogErr, rc, "Failure to write to copy in KTeeFileWrite");
		    break;
		}
	    }
	    max_position = pos + sofar;
	    if (max_position > self->maxposition)
		self->maxposition = max_position;
	}
    }
    *num_writ = writ;
    return rc;
}
static
rc_t CC KTeeFileWriteRead (KTeeFile *self, uint64_t pos,
			   const void *buffer, size_t bsize,
			   size_t *num_writ)
{
    assert (self != NULL);
    assert (buffer != NULL);
    assert (num_writ != NULL);
    assert (bsize != 0);

    *num_writ = 0;
    return RC (rcFS, rcFile, rcWriting, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * 
 */


/* end of file teefile.c */

