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

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <klib/rc.h>
#include <kfs/file.h>
#include <kfs/subfile.h>
#include <kfs/arc.h>	/* definition of chunks */
#include "copycat-priv.h"
/* ======================================================================
 * KSubChunkedFile
 *  a chunked file inside an archive not general purpose yet
 */

/* -----
 * define the specific types to be used in the templatish/inheritancish
 * definition of vtables and their elements
 */
#define KFILE_IMPL struct KSubChunkFile
#include <kfs/impl.h>

static rc_t KSubChunkFileDestroy (KSubChunkFile *self);
static struct KSysFile *KSubChunkFileGetSysFile (const KSubChunkFile *self,
					    uint64_t *offset);
static rc_t KSubChunkFileRandomAccess (const KSubChunkFile *self);
static uint32_t KSubChunkFileType (const KSubChunkFile *self);
static rc_t KSubChunkFileSize (const KSubChunkFile *self, uint64_t *size);
static rc_t KSubChunkFileSetSize (KSubChunkFile *self, uint64_t size);
static rc_t KSubChunkFileRead (const KSubChunkFile *self, uint64_t pos,
			  void *buffer, size_t bsize, size_t *num_read);
static rc_t KSubChunkFileWrite (KSubChunkFile *self, uint64_t pos, const void *buffer,
			   size_t size, size_t *num_writ);


static const KFile_vt_v1 vtKSubChunkFile =
{
    /* version */
    1, 1,

    /* 1.0 */
    KSubChunkFileDestroy,
    KSubChunkFileGetSysFile,
    KSubChunkFileRandomAccess,
    KSubChunkFileSize,
    KSubChunkFileSetSize,
    KSubChunkFileRead,
    KSubChunkFileWrite,

    /* 1.1 */
    KSubChunkFileType
};

/*-----------------------------------------------------------------------
 * KSubChunkFile
 *  an archive file including tar and sra
 */
struct KSubChunkFile
{
    KFile		dad;
    uint64_t		size;
    const KFile *	original;
    uint32_t		num_chunks;
    const KTocChunk 	chunks [1];
};

/* ----------------------------------------------------------------------
 * KSubChunkFileMake
 *  create a new file object
 */

rc_t KFileMakeChunkRead (const struct KFile ** pself,
			 const struct KFile * original,
			 uint64_t size,
			 uint32_t num_chunks,
			 struct KTocChunk * chunks)
{
    rc_t	rc;
    KSubChunkFile *	self;

    /* -----
     */
    assert (pself != NULL);
    assert (original != NULL);

    *pself = NULL;
    rc = 0;
    /* -----
     * get space for the object
     */
    self = malloc (sizeof (KSubChunkFile) + ((num_chunks-1) * sizeof (KTocChunk)));
    if (self == NULL)	/* allocation failed */
    {
        /* fail */
        rc = RC (rcFS, rcFile, rcConstructing, rcMemory, rcExhausted);
    }
    else
    {
        rc = KFileInit (&self->dad,				/* initialize base class */
			(const KFile_vt*)&vtKSubChunkFile, 	/* VTable for KSubChunkFile */
                    "KSubChunkFile", "no-name",
                        true,false);				/* read allowed,write disallowed */
	if (rc == 0)
	{
	    KFileAddRef (original);
	    /* succeed */
	    self->size = size;
	    self->original = original;
	    self->num_chunks = num_chunks;
	    memcpy ((struct KTocChunk*)self->chunks, chunks, num_chunks * sizeof (KTocChunk));
	    *pself = &self->dad;
	    return 0;
	}
	/* fail */
	free (self);
    }
    return rc;
}

/* ----------------------------------------------------------------------
 * Destroy
 *
 */
static
rc_t KSubChunkFileDestroy (KSubChunkFile *self)
{
    assert (self != NULL);
    KFileRelease (self->original);
    free (self);
    return 0;
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
struct KSysFile *KSubChunkFileGetSysFile (const KSubChunkFile *self, uint64_t *offset)
{
    /* parameters must be non-NULL */
    assert (self != NULL);
    assert (offset != NULL);

    /* not implmenting at this time */
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
rc_t KSubChunkFileRandomAccess (const KSubChunkFile *self)
{
    assert (self != NULL);
    return KFileRandomAccess (self->original);
}

/* ----------------------------------------------------------------------
 * Type
 *  returns a KFileDesc
 *  not intended to be a content type,
 *  but rather an implementation class
 */
static
uint32_t KSubChunkFileType (const KSubChunkFile *self)
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
rc_t KSubChunkFileSize (const KSubChunkFile *self, uint64_t *size)
{
    assert (self != NULL);
    assert (size != NULL);

    *size = self->size;

    return 0;;
}

/* ----------------------------------------------------------------------
 * SetSize
 *  sets size in bytes of file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t KSubChunkFileSetSize (KSubChunkFile *self, uint64_t size)
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
rc_t	KSubChunkFileRead	(const KSubChunkFile *self,
				 uint64_t pos,
				 void *buffer,
				 size_t bsize,
				 size_t *num_read)
{
    size_t	count;		/* how many to read/write in an action */
    uint8_t *	pbuff;		/* access the buffer as an array of bytes */
    uint64_t	end;		/* this will be set to the end offset */
    uint32_t	num_chunks;
    const KTocChunk * pchunk;
    rc_t	rc;		/* general purpose return from calls and pass along */

    assert (self != NULL);
    assert (buffer != NULL);
    assert (num_read != NULL);
    assert (bsize != 0);

    /* -----
     * assume no read/write will happen or rather start with having read none;
     * this write could be superfluous but we need to prepare *num_read for += operations
     */
    *num_read = 0;

    pbuff = buffer;
    end = pos + bsize;	
    num_chunks = self->num_chunks;
    pchunk = self->chunks;

    /* -----
     * step through the chunks
     */
    for (rc = 0; (num_chunks) && (pos < end); --num_chunks, ++pchunk)
    {
	uint64_t 	cend;		/* end offset of this chunk */

	/* -----
	 * determine the end of this chunk
	 */
	cend = pchunk->logical_position + pchunk->size;

	/* -----
	 * if this chunk is entirely before the current position
	 * we are looking for
	 * skip to the next (if any) chunk
	 */
	if (pos > cend)
	    continue;

	/* -----
	 * handle any needed zero fill section before the next chunk
	 */
	if (pos < pchunk->logical_position)
	{
	    /* -----
	     * try to fake-read as many bytes of zero as possible
	     * so start assuming you need enough zeros to reach the next chunk
	     * but cut it back to the remaining requested if that was too many
	     */
	    count = pchunk->logical_position - pos;
	    if (count > bsize)
		count = bsize;

	    /* fake read the zeros */
	    memset (pbuff, 0, count);

	    /* update tracking variables */
	    pbuff += count;
	    pos += count;
	    *num_read += count;
	}

	/* -----
	 * handle a chunk section
	 *
	 * if we are here, then we still have bytes to get and
	 * pos >= pchunk_logical_position
	 *
	 * Get the most we can from this chunk.
	 * If there are enough bytes in this chunk to finish the read: do so.
	 * Else read through the end of the chunk
	 */
	count = (end <= cend) ? end - pos : cend - pos;

	/* -----
	 * a little tricky is we call by value the wanted count and the function
	 * called will over write that with the actual read count
	 */
	rc = KFileRead (self->original, 
			pchunk->source_position + (pchunk->logical_position - pos), 
			pbuff, count, &count);

	*num_read += count;
	if (rc != 0)
	{
	    /* failure so abort */
	    break;
	}
	pbuff += count;
	pos += count;
	*num_read += count;
    }
    /* -----
     * If eveything so far is okay but we have more bytes to read
     * yet no more chunks; then fill to the end with zeroes
     */
    if ((rc == 0) && (pos < end))
    {
	count = end - pos;
	memset (pbuff, 0, count);
	*num_read += count;
    }
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
rc_t	KSubChunkFileWrite (KSubChunkFile *self, uint64_t pos,
		       const void *buffer, size_t bsize,
		       size_t *num_writ)
{
    assert (self != NULL);
    assert (buffer != NULL);
    assert (num_writ != NULL);

    *num_writ = 0;
    return RC (rcFS, rcFile, rcWriting, rcSelf, rcUnsupported);
}

/* end of file subfile.c */

