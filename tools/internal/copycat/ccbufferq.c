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
#include <os-native.h>

#include <klib/rc.h>
#include <kapp/log.h>
#include <kcont/queue.h>

#include "copycat-priv.h"



struct BufferQ
{
    atomic32_t	refcount;	/* how many references to this object */
    uint32_t	timeout;	/* default timeout in millisecs */
    KQueue *	q;		/* The underlying thread safe queue */
};

rc_t BufferQAddRef (const BufferQ * self)
{
    if (self != NULL)
	atomic32_inc (&((BufferQ*)self)->refcount);
    return 0;
}

rc_t BufferQRelease (const BufferQ *cself)
{
    BufferQ * self  = (BufferQ*)cself;
    rc_t rc = 0;
    if ( self != NULL )
    {
        if ( atomic32_dec_and_test (&self->refcount))
        {
	    const Buffer * b;
	    while (rc == 0)
	    {
		rc = BufferQPopBuffer (self, &b, NULL);
		BufferRelease (b);
	    }
/* this might need rework especially if KQueue changes */
	    if ((GetRCState(rc) == rcExhausted) && (GetRCObject(rc) == rcTimeout))
		rc = 0;
	    if (rc == 0)
	    {
		rc = KQueueRelease (self->q);
		if (rc == 0)
		{
		    free (self);
		    return 0;
		}
	    }
	    atomic32_inc (&((BufferQ*)self)->refcount);
        }
    }
    return rc;
}


rc_t BufferQMake (BufferQ ** q, uint32_t timeout, uint32_t length)
{
    rc_t rc = 0;
    BufferQ * self;

    assert (q != NULL);

    self = malloc (sizeof * self);
    if (self == NULL)
	rc = RC (rcExe, rcQueue, rcAllocating, rcMemory, rcExhausted);
    else
    {
	rc = KQueueMake (&self->q, length);
	if (rc == 0)
	{
	    self->timeout = timeout;
	    atomic32_set (&self->refcount, 1);
	    *q = self;
	}
    }

    return rc;
}

rc_t BufferQPushBuffer (BufferQ * self, const Buffer * buff, timeout_t * tm)
{
    rc_t rc = 0;
    timeout_t t;

    assert (self != NULL);
    assert (buff != NULL);

    if (tm == NULL)	/* do we need the default timeout? */
    {
	tm = &t;
	rc = TimeoutInit (tm, self->timeout);
    }

    if (rc == 0)
    {
	rc = KQueuePush (self->q, buff, tm);
	if (rc == 0)
	{
	    /* share ownership of the buffer removing keep alive reference */
	    rc = BufferAddRef (buff);
	}
    }

    return rc;
}
rc_t BufferQPopBuffer (BufferQ * self, const Buffer ** buff, timeout_t * tm)
{
    rc_t rc = 0;
    timeout_t t;
    void * p;
    LOGMSG (klogDebug10, "BufferQPopBuffer");
    assert (self != NULL);
    assert (buff != NULL);

    if (tm == NULL)
    {
	LOGMSG (klogDebug10, "BufferQPopBuffer tm was NULL");
	tm = &t;
	rc = TimeoutInit (tm, self->timeout);
    }

    if (rc == 0)
    {
	LOGMSG (klogDebug10, "BufferQPopBuffer call KQueuePop");
	rc = KQueuePop (self->q, &p, tm);
	PLOGMSG (klogDebug10, "BufferQPopBuffer back from KQueuePop $(rc)", PLOG_U32(rc), rc);
	if (rc == 0)
	    *buff = p;
	else
	{
	    *buff = NULL;
	}
    }
    LOGMSG (klogDebug10, "leave BufferQPopBuffer");
    return rc;
}
rc_t BufferQSeal (BufferQ * self)
{
    return KQueueSeal (self->q);
}
bool BufferQSealed (BufferQ *self)
{
    return KQueueSealed (self->q);
}
