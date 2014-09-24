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
#include <atomic.h>

#include <kapp/main.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/md5.h>
#include <kapp/log.h>

#include "copycat-priv.h"


struct Copier
{
    atomic32_t refcount;
    BufferQ * q;	/* input q for buffers to copy to file */
    KFile * f;		/* output file (actually a KMD5File) */
    uint64_t o;		/* how far into the output file are we */
};


rc_t CopierRelease (const Copier * cself)
{
    rc_t rc = 0;
    Copier * self = (Copier*)cself;

    if (self != NULL)
    {
	if (atomic32_dec_and_test (&self->refcount))
	{
	    rc = KFileRelease (self->f);
	    if (rc == 0)
	    {
		rc = BufferQRelease (self->q);
		if (rc == 0)
		{
		    free (self);
		}
	    }
	}
    }
    return rc;
}

rc_t CopierAddRef (const Copier * self)
{
    if (self != NULL)
	atomic32_inc (&((Copier*)self)->refcount);
    return 0;
}

static
rc_t CopierMakeF (Copier * cp, KDirectory * dir, const char * path)
{
    rc_t rc, orc;
    KFile * fm;

    rc = KDirectoryCreateFile (dir, &fm, true, 0666, kcmCreate|kcmParents,
				"%s.md5", path);
    if (rc == 0)
    {
	KMD5SumFmt * md5f;
	rc = KMD5SumFmtMakeUpdate (&md5f, fm);
	if (rc == 0)
	{
	    KFile * f;
	    rc = KDirectoryCreateFile (dir, &f, false, 0666, kcmCreate, 
                                    "%s", path);
	    if (rc == 0)
	    {
		KMD5File * fmd5;
		rc = KMD5FileMakeWrite (&fmd5, f, md5f, path);
		if (rc == 0)
		{
		    cp->f = KMD5FileToKFile (fmd5);
		    orc = KMD5SumFmtRelease (md5f);
                    if (orc)
                        LOGERR (klogInt, orc, "Failure releasing MD5 format");
		    cp->o = 0; /* start of file */
		    return rc;
		}
		orc = KFileRelease (f);
                if (orc)
                    LOGERR (klogInt, orc, "Failure releasing Copier file");
	    }
	    orc = KMD5SumFmtRelease (md5f);
            if (orc)
                LOGERR (klogInt, orc, "Failure releasing MD5 format");
	}
	orc = KFileRelease (fm);
        if (orc)
            LOGERR (klogInt, orc, "Failure releasing MD5SUM file");
    }
    return rc;
}

rc_t CopierMake (Copier ** c, KDirectory * dir, const char * path, uint32_t timeout, uint32_t length)
{
    rc_t rc = 0, orc;
    Copier * self;

    self = malloc (sizeof *self);
    if (self == NULL)
    {
	rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
    }
    else
    {
	atomic32_set (&self->refcount, 1);
	self->f = NULL;
	self->q = NULL;
	rc = CopierMakeF (self, dir, path);
	if (rc == 0)
	{
	    rc = BufferQMake (&self->q, timeout, length);
	    if (rc == 0)
	    {
		*c = self;
		atomic32_set (&self->refcount, 1);
		return rc;
	    }	    
	}
	orc = CopierRelease (self);
        if (orc)
            LOGERR (klogInt, orc, "Error releasing Copier");
    }
    return rc;
}

rc_t CopierDoOne (Copier * self)
{
    rc_t rc = 0;
    const Buffer * b;

    LOGMSG (klogDebug10, "CopierDoOne");
    rc = Quitting();
    if (rc == 0)
    {
	LOGMSG (klogDebug10, "call BufferQPopBuffer");
	rc = BufferQPopBuffer (self->q, &b, NULL);
	if (rc == 0)
	{
	    size_t w;
	    size_t z;
	    LOGMSG (klogDebug10, "call BufferContentGetSize");
	    z = BufferContentGetSize (b);
	    rc = KFileWrite (self->f, self->o, b, z, &w);
	    self->o += w;
	    if (w != z)
		rc = RC (rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete);
	    else
		rc = BufferRelease (b);
	}
	/* ow this is ugly! */
	/* is the rc a "exhausted" on a timeout? */
	else if ((GetRCObject(rc) == rcTimeout) && (GetRCState(rc) == rcExhausted))
	{
	    rc = 0;
	    LOGMSG (klogDebug10, "CopierDoOne timeout");
	    /* if so is the queue also sealed? */
	    if (BufferQSealed (self->q) == true)
	    {
		LOGMSG (klogDebug10, "CopierDoOne sealed");
		/* if both then we are done and so signal */
		rc = KFileRelease (self->f);
		PLOGMSG (klogDebug10, "CopierDoOne back from KFileRelease $(rc)",PLOG_U32(rc),rc);
		if (rc == 0)
		{
		    self->f = NULL;
		    rc = BufferQRelease (self->q);
		    if (rc == 0)
		    {
			self->q = NULL;
			rc = RC (rcExe, rcNoTarg, rcCopying, rcNoTarg, rcDone );
		    }
		}
	    }
	}
	else
	    LOGMSG (klogDebug10, "CopierDoOne pop failure");

    }
    else
	LOGMSG (klogDebug10, "CopierDoOne: quitting");
    return rc;
}

BufferQ * CopierGetQ (Copier *self)
{
    return (self->q);
}

