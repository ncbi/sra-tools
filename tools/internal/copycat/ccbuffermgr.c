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
#include <stdlib.h>
#include <assert.h>

#include <atomic.h>
/* #include <os-native.h> */

/* #include <klib/rc.h> */
/* #include <kapp/main.h> */
#include <kcont/queue.h>
#include <kapp/log.h>

#include "copycat-priv.h"

/* ==========
 * BufferMgr
 */
struct BufferMgr
{
    atomic32_t	refcount;
    uint32_t	timeout;
    KQueue *	free_q;
};


/* Standard SRA type constructor:
 * construction of the manager also creates the buffers per specification in the parameters
 *  buffmgr    where to put a reference to the new BufferMgr
 *  buffcount  how many Buffers to make as well
 *  buffsize   how large a payload is desired for the Buffers
 *  timeout    number of milliseconds of wait time for the operations for this BufferMgr
 */
rc_t BufferMgrMake (BufferMgr ** buffmgr, uint32_t buffcount, size_t buffsize, uint32_t timeout)
{
    rc_t rc = 0;
    BufferMgr * self;

    *buffmgr = NULL;
    self = malloc ( sizeof * self );

    if ( self == NULL)
    {
	rc = RC (rcExe, rcBuffer, rcAllocating, rcMemory, rcExhausted);
	LOGERR (klogErr, rc, "BufferMgrMake: error allocating for buffer manager");
    }
    else
    {
	atomic32_set (&self->refcount, 1);
	self->timeout = timeout; /* default timeout */
	rc = KQueueMake (&self->free_q, buffcount);

	if (rc != 0)
	    LOGERR (klogErr, rc, "BufferMgrMake: error making KQueue");
	else
	{
	    uint32_t ix;
	    union
	    {
		Buffer * b;
		void * v;
	    } bp;
	    timeout_t tm;

	    for (ix = 0; ix < buffcount; ++ix)
	    {
		bp.v = NULL;
		rc = TimeoutInit (&tm, timeout);

		if (rc == 0)
		{
		    rc = BufferMake (&bp.b, buffsize, self);
		    if (rc == 0)
		    {
			rc = KQueuePush (self->free_q, bp.v, &tm);
			if (rc != 0)
			    free (bp.v);
		    }
		    if (rc != 0)
			break;
		}
	    }
	    if (ix == buffcount)
	    {
		atomic32_set (&self->refcount, 1);
		*buffmgr = self;
		return 0;
	    }
	    else
	    {
		/* failure so undo all */
		rc_t rc_sub = 0;

		while (rc_sub)
		{
		    rc_sub = TimeoutInit (&tm, timeout);
		    if (rc_sub == 0)
		    {
			rc_sub = KQueuePop (self->free_q, &bp.v, &tm);
			if (rc_sub == 0)
			    if (bp.v != NULL)
				free (bp.v);
		    }
		}
	    }	
	    KQueueRelease (self->free_q);
	}
	free (self);
    }
    return rc;
}

rc_t BufferMgrAddRef (const BufferMgr * self)
{
    if (self != NULL)
	atomic32_inc (&((BufferMgr*)self)->refcount);
    return 0;
}

rc_t BufferMgrRelease (BufferMgr *self)
{
    rc_t rc = 0;
    void * bp;
    timeout_t tm;
    if ( self != NULL )
    {
        if ( atomic32_dec_and_test (&self->refcount))
        {
	    /* release all allocated buffers here */
	    while (rc)
	    {
		rc = TimeoutInit (&tm, self->timeout);
		if (rc == 0)
		{
		    rc = KQueuePop (self->free_q, &bp, &tm);
		    if (rc == 0)
			if (bp != NULL)
			    free (bp);
		}
	    }
	    if (rc == 0)
		free (self);
        }
    }
    return rc;
}
rc_t BufferMgrPutBuffer (BufferMgr * self, Buffer * buff, timeout_t * tm)
{
    rc_t rc = 0;
    timeout_t t;

    assert (self != NULL);
    assert (buff != NULL);

    if (tm == NULL)
    {
	tm = &t;
	rc = TimeoutInit (tm, self->timeout);
    }

    if (rc == 0)
    {
	rc = KQueuePush (self->free_q, buff, tm);
	if (rc == 0)
	{
	    /* take ownership of the buffer removing keep alive reference */
	    BufferMgrRelease (self);
	}
    }
    if (rc != 0)
	/* assign ownership back to the caller */
	BufferAddRef(buff);

    return rc;
}
rc_t BufferMgrGetBuffer (BufferMgr * self, Buffer ** buff, timeout_t * tm)
{
    rc_t rc = 0, orc;
    timeout_t t;
    void * bp;

    assert (self != NULL);
    assert (buff != NULL);

    if (tm == NULL)
    {
	tm = &t;
	rc = TimeoutInit (tm, self->timeout);
    }

    if (rc == 0)
    {
	*buff = NULL;

	rc = KQueuePop (self->free_q, &bp, tm);
	if (rc == 0)
	{
	    /* add a keep alive reference to the buffer */
	    rc = BufferMgrAddRef (self);
            if (rc)
                LOGERR (klogInt, rc, "Error adding reference to buffer manager");
            else
                *buff = bp;

	    rc = BufferAddRef(*buff);
            if (orc)
                LOGERR (klogInt, rc, "Error adding reference to a buffer");
	}
    }
    return rc;
}
