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

#include <assert.h>
#include <atomic.h>
#include <stdlib.h>


/* #include <os-native.h> */

/* #include <klib/rc.h> */
/* #include <kapp/main.h> */
/* #include <kcont/queue.h> */

#include "copycat-priv.h"

/* ==========
 * Buffer is a generic buffer to be used to pass data between co-routines or 
 * threads.
 */
struct Buffer
{
    /* standard SRA tool kit type reference counter: zero when free and 
     * in the queue of the BufferMgr */
    atomic32_t	refcount;
    /* keep track of manager - by adding a reference when allocated it keeps
     * the manager alive long enough to free all buffers */
    BufferMgr *	mgr;
    /* how large is the payload for the buffer */
    size_t	payload_size;
    /* how large is the content of the payload? */
    size_t	payload_content;
    /* the payload of the packet is the rest of the packet (and not normally 
     * a single byte) */
    char	payload [1];
};

/* ----------
 * Constructor/initializer for a buffer: called only from a BufferMgr
 * Return: rc_t
 *  buff          pointer to a pointer to hold a reference to the new Buffer
 *  payload_size  how large to make the data portion of the Buffer
 *  mgr           a reference back to the manager that will control this buffer
 */
rc_t BufferMake (Buffer ** buff, size_t payload_size, BufferMgr * mgr)
{
    Buffer * self = NULL;
    rc_t rc = 0;

    assert (mgr != NULL);

    self = malloc (payload_size + sizeof (Buffer) - 1);
    if (self == NULL)
    {
	rc = RC (rcExe, rcBuffer, rcAllocating, rcMemory, rcExhausted);
    }
    else
    {
	/* non-standard for SRA Toolkit; refcount is 0 while not allocated */
	atomic32_set (&self->refcount, 0);
	self->mgr = mgr;
	self->payload_size = payload_size;
	self->payload_content = 0;
    }
    *buff = self;
    return rc;
}

rc_t BufferAddRef (const Buffer * self)
{
    if (self != NULL)
	atomic32_inc (&((Buffer*)self)->refcount);
    return 0;
}

rc_t BufferRelease (const Buffer * cself)
{
    Buffer * self = (Buffer *)cself;
    rc_t rc = 0;

    if (self != NULL)
    {
	if (atomic32_dec_and_test (&self->refcount))
	{
	    if (self->mgr == NULL)
		free (self);
	    else
	    {
		/* refcount reaching 0 means to put back in the free_q for the BufferMgr */
		rc = BufferMgrPutBuffer (self->mgr, self, NULL);

		/* if failed return ownership to last releaser */
		if (rc != 0)
		    atomic32_set (&self->refcount, 1);
	    }
	}
    }
    return rc;
}

size_t BufferPayloadGetSize (const Buffer * self)
{
    assert (self != NULL);
    return self->payload_size;
}
size_t BufferContentGetSize (const Buffer * self)
{
    assert (self != NULL);
    return self->payload_content;
}
rc_t BufferContentSetSize (Buffer * self, size_t z)
{
    rc_t rc = 0;
    assert (self != NULL);
    assert (z <= self->payload_size);
    self->payload_content = z;
    return rc;
}
/* is it too redundant to have a read and write version? */
const void * BufferPayload (const Buffer * self)
{
    assert (self != NULL);
    return (const void*)self->payload;
}
void * BufferPayloadWrite (Buffer * self)
{
    assert (self != NULL);
    return (void*)self->payload;
}

