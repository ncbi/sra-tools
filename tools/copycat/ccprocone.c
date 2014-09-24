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
#include <string.h>
#include <stdio.h>

#include <kapp/log.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/md5.h>
#include <kfs/teefile.h>
#include <kfs/buffile.h>
/* #include <klib/rc.h> */

#include "copycat.h"

/* ==========
 * Process is a generic buffer to be used to pass data between co-routines or 
 * threads.
 */

struct ProcessOne
{
    atomic32_t	refcount;	/* how many references to this object */
    const KDirectory * dir;
    KDirectory * xml;
    const KFile * file;
    CCFileFormat * ff;
    KMD5SumFmt *md5;
    char path [4096];
};

rc_t ProcessOneMake (ProcessOne ** ppo, const KDirectory * dir, KDirectory * xml,
		     const KFile * file, KMD5SumFmt *md5, CCFileFormat * ff,
		     const char * path)
{
    ProcessOne * self;
    rc_t rc = 0;
    size_t pathlen;

    PLOGMSG (klogDebug10, "ProcessOneMake $(f)", PLOG_S(f), path);
    /* legit seeming inputs? these could be replaced with RC returns */
    assert (ppo != NULL);
    assert (file != NULL);
    assert (path != NULL);

    /* allocate the object */
    pathlen = strlen (path);
    self = malloc (sizeof (*self) - sizeof(self->path) + pathlen + 1);
    if (self == NULL)
    {
	rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
    }
    else
    {
	atomic32_set (&self->refcount, 1);
	KDirectoryAddRef (dir);
	KDirectoryAddRef (xml);
	KMD5SumFmtAddRef (md5);
	CCFileFormatAddRef (ff);
	KFileAddRef (file);
	self->dir = dir;
	self->xml = xml;
	self->md5 = md5;
	self->file = file;
	self->ff = ff;
	memcpy (self->path, path, pathlen);
	self->path[pathlen] = '\0';
	rc = 0;
    }
    *ppo = self;
    return rc;
}

rc_t ProcessOneAddRef (const ProcessOne * self)
{
    if (self != NULL)
	atomic32_inc (&((ProcessOne*)self)->refcount);
    return 0;
}
rc_t ProcessOneRelease (const ProcessOne * cself)
{
    ProcessOne * self = (ProcessOne *)cself;
    rc_t rc = 0;

    if (self != NULL)
    {
	PLOGMSG (klogDebug10, "ProcessOneRelease $(f)", PLOG_S(f), self->path);
	if (atomic32_dec_and_test (&self->refcount))
	{
	    KDirectoryRelease (self->dir);
	    KDirectoryRelease (self->xml);
	    KMD5SumFmtRelease (self->md5);
	    CCFileFormatRelease (self->ff);
	    KFileRelease (self->file);
	    free (self);
	}
    }
    return rc;
}
const char * typeToString (enum KPathType type)
{
    switch (type)
    {
    default:
	return "bad-KPathType";
    case kptNotFound:
	return "not-found";
    case kptBadPath:
	return "bad-path";
    case kptFile:
	return "file";
    case kptDir:
	return "directory";
    case kptCharDev:
	return "character-device";
    case kptBlockDev:
	return "block-device";
    case kptFIFO:
	return "fifo";
    case kptAlias|kptNotFound:
	return "link-to-not-found";
    case kptAlias|kptBadPath:
	return "link-to-bad-path";
    case kptAlias|kptFile:
	return "link-to-file";
    case kptAlias|kptDir:
	return "link-to-directory";
    case kptAlias|kptCharDev:
	return "link-to-character-device";
    case kptAlias|kptBlockDev:
	return "link-to-block-device";
    case kptAlias|kptFIFO:
	return "link-to-fifo";
    }
}
rc_t ProcessOneDoFile (ProcessOne * self)
{
    rc_t rc = 0;
    KFile * mfile;
    
    PLOGMSG (klogInfo, "ProcessOneDoFile: $(F)", PLOG_S(F), self->path);


    rc = KFileMakeNewMD5Read (&mfile, self->file, self->md5, self->path);
    if (rc == 0)
    {
	const KFile * bfile;
	rc = KFileMakeBuf (&bfile, mfile, 64*1024);
	if (rc == 0)
	{
	    /* add more here */

	    KFileRelease (bfile);
	}
	else
	{
	    pLOGERR (klogErr, rc, "Failure to initiate buffer $(F)", PLOG_S(F), self->path);
	    KFileRelease (mfile);
	}
    }
    else
	pLOGERR (klogErr, rc, "Failure to initiate MD5 summing $(F)", PLOG_S(F), self->path);

    return rc;
}
rc_t ProcessOneDo (ProcessOne * self)
{
    static const char F[] = PLOG_2(PLOG_S(p),PLOG_S(t));
    enum KPathType type;
    rc_t rc = 0;
    
    type = KDirectoryPathType (self->dir, "%s", self->path);

    switch (type)
    {
    case kptFile:
	rc = ProcessOneDoFile (self);
 	break;
/*     case kptDir: */
/* 	break; */
/*     case kptAlias|kptFile: */
/* 	break; */
/*     case kptAlias|kptDir: */
/* 	break; */
    default:
	PLOGMSG (klogInfo, "+ Skipping $(p) of type $(t)", F, self->path, typeToString(type));
	break;
    }
    return rc;
}
