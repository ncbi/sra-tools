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
#include <kapp/main.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/md5.h>
#include <kapp/log.h>
#include "copycat-priv.h"


struct Reader
{
    atomic32_t refcount;
    const KFile * f;	/* input file */
    uint64_t o;		/* how far into the input file are we */
    BufferQ * out;	/* two output queues to copy everything read */
    BufferQ * xml;
    BufferMgr * mgr;	/* a manager for allocating a throttling buffer set */
};

rc_t ReaderRelease (const Reader * cself)
{
    rc_t rc = 0;
    Reader * self = (Reader*)cself;

    if (self != NULL)
    {
	if (atomic32_dec_and_test (&self->refcount))
	{
	    /* might already have been released */
	    rc = KFileRelease (self->f);
	    if (rc == 0)
	    {
		rc = BufferQRelease (self->out);
		if (rc == 0)
		{
		    rc = BufferQRelease (self->xml);
		    if (rc == 0)
		    {
			rc = BufferMgrRelease (self->mgr);
			if (rc == 0)
			    free (self);
			else
			    atomic32_set (&self->refcount, 1);
		    }
		}
	    }
	}
    }
    return rc;
}

rc_t ReaderAddRef (const Reader * self)
{
    if (self != NULL)
	atomic32_inc (&((Reader*)self)->refcount);
    return 0;
}

rc_t ReaderMake (Reader ** r, const KDirectory * d, const char * path, Copier * p,
		 Cataloger * g, uint32_t c, uint32_t z, uint32_t t)
{
    rc_t rc = 0;
    Reader * self;

    self = malloc (sizeof * self);
    if (self == NULL)
    {
	rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
	LOGERR (klogErr, rc, "ReaderMake: error allocating Reader");
    }
    else
    {
	atomic32_set (&self->refcount, 1);
	self->o = 0;

	rc = KDirectoryOpenFileRead (d, &self->f, "%s", path);
	if (rc != 0)
	{
	    pLOGERR (klogErr, rc,
		     "ReaderMake: error open file for read $(f)",
		     PLOG_S(f), path);
	}
	else
	{
	    self->out = CopierGetQ(p);
	    if (self->out == NULL)
	    {
		rc = RC (rcExe, rcNoTarg, rcAccessing, rcBuffer, rcCorrupt);
		LOGERR (klogErr, rc, "ReaderMake: corrupt Copier queue");
	    }
	    else
	    {
		rc = BufferQAddRef (self->out);
		if (rc == 0)
		{
		    self->xml = CatalogerGetQ(g);
		    if (self->xml == NULL)
		    {
			rc = RC (rcExe, rcNoTarg, rcCreating, rcQueue, rcCorrupt);
			LOGERR (klogErr, rc, "ReaderMake: corrupt Cataloger queue");
		    }
		    else
		    {
			rc = BufferQAddRef (self->xml);
			if (rc == 0)
			{
			    rc = BufferMgrMake (&self->mgr, c, z, t);
			    if (rc == 0)
			    {
				atomic32_set (&self->refcount, 1);
				*r = self;
				return 0;
			    }
			    LOGERR (klogErr, rc, "ReaderMake: error creating buffer manager");
			    BufferQRelease (self->xml);
			}
			LOGERR (klogErr, rc, "ReaderMake: error setting reference to xml queue");
		    }
		    LOGERR (klogErr, rc, "ReaderMake: error setting reference to xml queue");
		    BufferQRelease (self->out);
		}
		LOGERR (klogErr, rc, "ReaderMake: error setting reference to out queue");
	    }
	    KFileRelease (self->f);
	}
	free (self);
    }
    return rc;
}

rc_t ReaderDoOne (Reader * self)
{
    rc_t rc = 0;

    assert (self != NULL);
    assert (self->mgr != NULL);

    rc = Quitting();
    if (rc == 0)
    {
	Buffer * b = NULL;

	rc = BufferMgrGetBuffer (self->mgr, &b, NULL);
	if (rc != 0)
	{
	    /* -----
	     * if we couldn't get a buffer because we timeout, we don't want to
	     * signal a failure, so reset the rc and exit with no error
	     */
	    if ((GetRCState(rc) == rcExhausted) && (GetRCObject(rc) == rcTimeout))
	    {
		rc = 0;
		LOGMSG (klogInfo,
			"ReaderDoOne: timeout getting a buffer");
	    }
	    else
		LOGERR (klogErr, rc,
			"ReaderDoOne: error getting a buffer");

	}
	else
	{
	    size_t t;
	    size_t r;
	    char * p;

	    /* -----
	     * get the limits/values of the buffer we got
	     */
	    t = BufferPayloadGetSize (b);
	    p = BufferPayloadWrite (b);
	    assert (t > 0);
	    assert (b != NULL);
	    
	    /* attempt to read from the file */
	    rc = KFileRead (self->f, self->o, p, t, &r);
	    if (rc == 0)
	    {
		PLOGMSG (klogDebug10, 
			 "ReaderDoOne: back from KFileRead $(z)",
			 PLOG_U32(z),
			 r);
		/* if we have a read of some length send it to our two 
		 * processor friends: copy and catalog
		 */
		if (r != 0)
		{
		    self->o += r;
		    rc = BufferContentSetSize (b, r);
		    if (rc == 0)
		    {
			rc = BufferQPushBuffer (self->out, b, NULL);
			if (rc == 0)
			{
			    rc = BufferQPushBuffer (self->xml, b, NULL);
			    if (rc != 0)
				LOGERR (klogErr, rc,
					"ReaderDoOne: Failure to send bugger to xml queue");
			}
			else
			    LOGERR (klogErr, rc,
				    "ReaderDoOne: Failure to send bugger to out queue");
		    }
		    else
			LOGERR (klogErr, rc,
				"ReaderDoOne: Failure to set size of buffer");
		}
		/* if we had a successful read of 'nothing' we are at end of file */
		else
		{
		    /* drop the file */
		    rc = KFileRelease (self->f);
		    if (rc == 0)
		    {
			self->f = NULL;
			/* seal off and release the two queues */
			rc = BufferQSeal (self->out);
			if (rc == 0)
			{
			    rc = BufferQRelease (self->out);
			    if (rc == 0)
			    {
				self->out = NULL;
				rc = BufferQSeal (self->xml);
				if (rc == 0)
				{
				    rc = BufferQRelease (self->xml);
				    if (rc == 0)
				    {
					self->xml = NULL;
					rc = BufferMgrRelease (self->mgr);
					if (rc == 0)
					{
					    self->mgr = NULL;
					    rc = RC (rcExe, rcProcess, rcReading, rcThread, rcDone);
					}
					else
					    LOGERR (klogErr, rc,
						    "ReaderDoOne: Failure to release buffer manager");
				    }
				    else
					LOGERR (klogErr, rc,
						"ReaderDoOne: Failure to release buffer xml queue");
				}
				else
				    LOGERR (klogErr, rc,
					    "ReaderDoOne: Failure to seal buffer xml queue");
			    }
			    else
				LOGERR (klogErr, rc,
					"ReaderDoOne: Failure to release buffer out queue");
			}
			else
			    LOGERR (klogErr, rc,
				    "ReaderDoOne: Failure to seal buffer out queue");
		    }
		    else
			LOGMSG (klogErr,
				"ReaderDoOne: Failure to close input file");
		}
	    }
	    else
		LOGERR (klogErr, rc,
			"ReaderDoOne: file read error");
	    BufferRelease (b);
	}
    }
    else
    {
	LOGMSG (klogInt, "ReaderDoOne: Reader quitting");
    }
    return rc;
}


