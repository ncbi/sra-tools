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

#include <kfs/extern.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/debug.h>
#include <klib/container.h>
#include <sysalloc.h>

#include "fileformat.h"
#include "fileformat-priv.h"

#include <atomic.h>
#include <stdlib.h>
#include <string.h>

/* ----------------------------------------------------------------------
 * Private type used only with in this compilation unit.
 */
typedef
struct KFFClass
{
    BSTNode	node;
    atomic32_t	refcount;
    KFileFormatClass	class;		/* scalar ID of the class */
    size_t	len;		/* length of the descriptor */
    char 	descr	[1];	/* ASCIZ text description of the class */
} KFFClass;

static
rc_t KFFClassDestroy (const KFFClass * cself)
{
    rc_t rc;
    KFFClass * self;

    FUNC_ENTRY();

    rc = 0;
    self = (KFFClass*)cself;
    if (cself == NULL)
    {
	rc = RC (rcFF, rcIndex, rcDestroying, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFClassDestroy: self == NULL");
    }
    else
    {
	free (self);
    }
    return rc;
}

static
rc_t KFFClassRelease (const KFFClass * cself)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (cself != NULL)
    {
	KFFClass *self = (KFFClass*)cself;
        if (atomic32_dec_and_test (&self->refcount))
	    return  KFFClassDestroy (cself);
    }
    return rc;
}
/* not used at this time */
#if 0
static
rc_t KFFClassAddRef (const KFFClass * self)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (self != NULL)
        atomic32_inc (&((KFFClass*)self)->refcount);
    return rc;
}
#endif
static
rc_t KFFClassMake (KFFClass ** kffcp,	/* pointer to new object */
		   KFileFormatClass class, /* ID of new class */
		   const char * descr,	/* description of new class */
		   size_t len)		/* length of description */
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (kffcp == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFClassMake: kffkmp == NULL");
    }
    else if (descr == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcNull);
	LOGERR (klogErr, rc, "KFFClassMake: descr == NULL");
    }
    else
    {
	if (len > DESCRLEN_MAX)
	{
/* 	    printf("len %u DESCRLEN_MAX %u\n", len, DESCRLEN_MAX); */
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcInvalid);
	    LOGERR (klogErr, rc, "KFFClassMake: descr too long");
	}
	else if (len == 0)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcEmpty);
	    LOGERR (klogErr, rc, "KFFClassMake: descr too short");
	}
	else
	{
	    KFFClass * self = malloc (sizeof (*self) + len);
	    if (self == NULL)
	    {
		rc = RC (rcFF,  rcIndex, rcConstructing, rcMemory, rcExhausted);
		LOGERR (klogErr, rc, "KFFClassMake: object could not be allocated");
	    }
	    else
	    {
		/* self->node needs not be initialized  */
		atomic32_set (&self->refcount,1);
		self->class = class;
		self->len = len;
		memmove (self->descr, descr, len);
		self->descr[self->len] = '\0';
		*kffcp = self;
		return 0;
	    }
	}
    }
    *kffcp = NULL;
    return rc;
}

/* -----
 * Private type used only with in this compilation unit.
 */
typedef
struct KFFType
{
    BSTNode	node;
    atomic32_t 	refcount;	/* how many open references to this object */
    KFileFormatType	type;		/* scalar ID of the type */
    KFileFormatClass	class;		/* scalar ID of the class the type belongs in */
    size_t	len;		/* length of the descriptor */
    char 	descr	[1];	/* ASCIZ text description of the type */
} KFFType;

static
rc_t KFFTypeDestroy (const KFFType * cself)
{
    rc_t rc = 0;
    KFFType * self = (KFFType*)cself;

    FUNC_ENTRY();

    if (self == NULL)
    {
	rc = RC (rcFF, rcIndex, rcDestroying, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFTypeDestroy: self == NULL");
    }
    else
    {
	free (self);
    }
    return rc;
}

static
rc_t KFFTypeRelease (const KFFType * cself)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (cself != NULL)
    {
	KFFType *self = (KFFType*)cself;
        if (atomic32_dec_and_test (&self->refcount))
	    return  KFFTypeDestroy (cself);
    }
    return rc;
}
/* not used at this time */
#if 0
static
rc_t KFFTypeAddRef (const KFFType * self)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (self != NULL)
        atomic32_inc (&((KFFType*)self)->refcount);
    return rc;
}
#endif
static
rc_t KFFTypeMake (KFFType ** kfftp,
		  KFileFormatType  type,
		  KFileFormatClass class,
		  const char * descr,
		  size_t len)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (kfftp == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFTypeMake: kffkmp == NULL");
    }
    else if (descr == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcNull);
	LOGERR (klogErr, rc, "KFFTypeMake: descr == NULL");
    }
    else
    {
	if (len > DESCRLEN_MAX)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcInvalid);
	    LOGERR (klogErr, rc, "KFFTypeMake: descr too long");
	}
	else if (len == 0)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcEmpty);
	    LOGERR (klogErr, rc, "KFFTypeMake: descr too short");
	}
	else
	{
	    KFFType * self = malloc (sizeof (*self) + len);
	    if (self == NULL)
	    {
		rc = RC (rcFF,  rcIndex, rcConstructing, rcMemory, rcExhausted);
		LOGERR (klogErr, rc, "KFFTypeMake: object could not be allocated");
	    }
	    else
	    {
		/* self->node needs not be initialized  */
		atomic32_set (&self->refcount,1);
		self->len = len;
		memmove (self->descr, descr, len);
		self->descr[self->len] = '\0';
		self->type = type;
		self->class = class;
		*kfftp = self;
		return 0;
	    }
	}
    }
    *kfftp = NULL;
    return rc;
}


/* ----------------------------------------------------------------------
 *
 * A table will have a descr based bstree for type, a descr based bstree
 * for class,
 *
 * A type indexed and class indexed set of indexes into the same stuff.
 *
 * All table are thus the "node" and pointer to a node.  the same nodes
 * are referred to by the descr based bstree and the linear tables.
 *
 * Linear tables are pre allocated arrays that are increased by chunks.
*/

struct KFFTables
{
    atomic32_t 	refcount;
    BSTree	classtree;
    BSTree	typetree;
    KFFClass **	classindex;
    KFFType **	typeindex;
    uint32_t	typecount;
    uint32_t	classcount;
    uint32_t	typesize;
    uint32_t	classsize;

};

static
void KFFTypeNodeWhack (BSTNode * n, void * ignored)
{
    FUNC_ENTRY();

    KFFTypeDestroy ((KFFType*)n);
}
static
void KFFClassNodeWhack (BSTNode * n, void * ignored)
{
    FUNC_ENTRY();
    KFFClassDestroy ((KFFClass*)n);
}
static
rc_t KFFTablesDestroy (const KFFTables * cself)
{
    KFFTables * self = (KFFTables*)cself;
    rc_t rc = 0;
/*     uint32_t ix; */
    FUNC_ENTRY();

    BSTreeWhack (&self->typetree, KFFTypeNodeWhack, NULL);
    BSTreeWhack (&self->classtree, KFFClassNodeWhack, NULL);
    if (self->classindex != NULL)
	free (self->classindex);
    if (self->typeindex != NULL)
	free (self->typeindex);
    free (self);
    return rc;
}

rc_t CC KFFTablesRelease (const KFFTables * cself)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (cself != NULL)
    {
	KFFTables *self = (KFFTables*)cself;
        if (atomic32_dec_and_test (&self->refcount))
	    return  KFFTablesDestroy (self);
    }
    return rc;
}

rc_t CC KFFTablesAddRef (const KFFTables * self)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (self != NULL)
        atomic32_inc (& ((KFFTables*) self)->refcount);
    return rc;
}

/* -----
 * if descr is too short, not NUL terminated, and at the edge of legal memory
 * there is a chance the next two functions could have a segmentation fault
 */
static
int64_t classcmp (const void * descr, const BSTNode * n)
{
    KFFClass * nn = (KFFClass*)n;
    return strncmp ((const char *)descr, nn->descr, nn->len);
}
static
int64_t typecmp (const void * descr, const BSTNode * n)
{
    KFFType * nn = (KFFType *)n;

    return strncmp ((const char *)descr, nn->descr, nn->len);
}
static
int64_t classsort (const BSTNode * i ,const BSTNode * n)
{
    KFFClass * ii = (KFFClass *)i;
    KFFClass * nn = (KFFClass *)n;
    return strncmp (ii->descr, nn->descr, nn->len);
}
static
int64_t typesort (const BSTNode * i, const BSTNode * n)
{
    KFFType * ii = (KFFType *)i;
    KFFType * nn = (KFFType *)n;
    return strncmp (ii->descr, nn->descr, nn->len);
}

static
KFFClass * KFFTablesFindKFFClass (KFFTables * self,
				  const char * descr )
{
    BSTNode * pbn;
    if ((self == NULL) || (descr == NULL))
	return NULL;
    pbn = BSTreeFind (&self->classtree, descr, classcmp);
    return (KFFClass*)pbn;
}
static
KFFType * KFFTablesFindKFFType (KFFTables * self,
				const char * descr )
{
    BSTNode * pbn;
    FUNC_ENTRY ();
    if ((self == NULL) || (descr == NULL))
	return NULL;
    pbn = BSTreeFind (&self->typetree, descr, typecmp);
    return (KFFType*)pbn;
}
/* Not currently used
static
KFileFormatClass KFFTablesFindKFileFormatClass (KFFTables * self,
						const char * descr )
{
    BSTNode * pbn;
    LOGENTRY (10, "(10, "KFFTablesFindKFileFormatClass"));
    if ((self == NULL) || (descr == NULL))
	return kffcError;
    pbn = BSTreeFind (&self->classtree, descr, classcmp);
    if (pbn == NULL)
	return kffcUnknown;
    return ((KFFClass*)pbn)->class;
}
*/
static
KFileFormatType KFFTablesFindKFileFormatType (const KFFTables * self,
					      const char * descr )
{
    union
    {
	BSTNode * bn;
	KFFType * kt;
    } node;

    FUNC_ENTRY();

    if ((self == NULL) || (descr == NULL))
    {
	LOGMSG(klogWarn,"Not Found");
	return kfftError;
    }
    node.bn = BSTreeFind (&self->typetree, descr, typecmp);
    if (node.bn == NULL)
    {
        PLOGMSG(klogWarn,(klogWarn,"Unknown $(D)", PLOG_S(D),descr));
	return kfftUnknown;
    }

/*     printf("KFFTablesFindKFileFormatType:\n" */
/* 	   "\trefcount\t%u\n" */
/* 	   "\ttype\t\t%u\n" */
/* 	   "\tclass\t\t%u\n" */
/* 	   "\tlen\t\t%lu\n" */
/* 	   "\tdescr\t\t%s\n", */
/* 	   node.kt->refcount,  */
/* 	   node.kt->type, */
/* 	   node.kt->class, */
/* 	   node.kt->len, */
/* 	   node.kt->descr); */

    return (node.kt->type);
}

rc_t CC KFFTablesAddClass (KFFTables * self,
                           KFileFormatClass * pclassid, /* returned ID: NULL okay */
                           const char * descr,
                           size_t len)
{
    rc_t rc;

    FUNC_ENTRY();

/*     PLOGMSG ((klogDebug10, "Descr is $(L) $(D)", PLOG_2(PLOG_U32(L),PLOG_S(D)), len,descr)); */
    rc = 0;
    if (self == NULL)
    {
	rc = -1;
	LOGERR (klogErr, rc, "Error making type: NULL pointer");
    }
    else if (descr == NULL)
    {
	rc = -1;
	LOGERR (klogErr, rc, "Error making type: NULL descr");
    }
    else
    {
	KFFClass * pclass;

	pclass = KFFTablesFindKFFClass (self, descr);
	if (pclass != NULL)
	{
	    rc = -1;
	    PLOGERR (klogErr, (klogErr, rc, "Class already inserted <$(d)>", PLOG_S(d),  descr));
	}
	else
	{
	    /* not thread safe if multiple "creators" */
	    if (self->classcount * sizeof(KFFClass*) == self->classsize)
	    {
		void * vp;
		self->classsize += 32 * sizeof(KFFClass*);
		/* realloc will not be undone if anything fails */
		vp = realloc (self->classindex,self->classsize);
		if (vp == NULL)
		{
		    rc = -1;
		    LOGERR (klogErr, rc, "Error allocating class table");
		}
		self->classindex = vp;
	    }
	}
	if (rc == 0)
	{
	    KFileFormatClass cid;
	    KFFClass * kc;
	    cid = self->classcount++;
	    rc = KFFClassMake (&kc, cid, descr, len);
	    if (rc != 0)
	    {
		PLOGERR (klogErr, (klogErr, rc, "Error making class: $(c) $(d)",
			  PLOG_2(PLOG_U32(c),PLOG_S(d)),
			  cid, descr));
	    }
	    else
	    {
		rc = BSTreeInsert (&self->classtree, &kc->node,
				   classsort);
		if (rc == 0)
		{
		    self->classindex[cid] = kc;
		    if (pclassid)
			*pclassid = cid;
		    return 0;
		}
		PLOGERR (klogErr, (klogErr, rc, "Error inserting class $(c) $(d)",
                                   PLOG_2(PLOG_U32(c),PLOG_S(d)),
                                   cid, descr));
		KFFClassRelease (kc);
	    }
	    self->classcount--;
	}
    }
    if (pclassid)
	*pclassid = 0;
    return rc;
}

rc_t CC KFFTablesAddType (KFFTables * self,
                          KFileFormatType * ptype, /* returned new ID */
                          const char * class,
                          const char * type,
                          size_t clen,
                          size_t tlen)
{
    rc_t rc;
    KFFType * ptn;
    KFFClass * pcn;
    KFileFormatClass classid;
    KFileFormatType pid;

    FUNC_ENTRY();

    rc = 0;

    if (self == NULL)
    {
	rc = RC (rcFF, rcFileFormat, rcConstructing, rcSelf, rcNull);
	LOGERR (klogErr, rc, "Error making type: NULL pointer");
	goto quickout;
    }
    if ((class == NULL)||(type == NULL))
    {
 	rc = -1;
	LOGERR (klogErr, rc, "Error making type: NULL parameter");
	goto quickout;
    }
    ptn = KFFTablesFindKFFType (self, type);
    pcn = KFFTablesFindKFFClass (self, class);
    if (ptn != NULL)
    {
	rc = -1;
	PLOGERR (klogErr, (klogErr, rc, "Type already inserted <$(d)>", PLOG_S(d), type));
	goto quickout;
    }

    if (pcn == NULL)
    {
	rc = KFFTablesAddClass (self, &classid, class, clen);
	if (rc != 0)
	{
	    PLOGERR (klogErr, (klogErr, rc, "unable to insert new class <$(d)>", PLOG_S(d), class));
	    goto quickout;
	}
    }
    else
    {
	classid = pcn->class;
    }

    /* not thread safe if multiple "creators" */
    if (self->typecount * sizeof(KFFType*) == self->typesize)
    {
	void * vp;
	self->typesize += 32 * sizeof(KFFType*);
	/* realloc will not be undone of anything fails */
	vp = realloc (self->typeindex,self->typesize);
	if (vp == NULL)
	{
	    rc = -1;
	    LOGERR (klogErr, rc, "Error allocating type table");
	    goto quickout;
	}
	self->typeindex = vp;

    }
    pid = self->typecount++;
    rc = KFFTypeMake (&ptn, pid, classid, type, tlen);
    if (rc != 0)
    {
	PLOGERR (klogErr, (klogErr, rc, "Error making type: $(c) $(d)",
                           PLOG_2(PLOG_I32(c),PLOG_S(d)),
                           pid, type));
    }
    else
    {
	rc = BSTreeInsert (&self->typetree, &ptn->node, typesort);
	if (rc == 0)
	{
	    self->typeindex[pid] = ptn;
	    if (ptype)
		*ptype = pid;
	    return 0;
	}
	KFFTypeRelease (ptn);
    }
    self->typecount--;

quickout:
    if (ptype)
	*ptype = 0;
    return rc;
}

rc_t CC KFFTablesMake (KFFTables ** kmmtp)
{
    KFFTables * self;
    rc_t rc = 0;

    FUNC_ENTRY();

    self = calloc (1,sizeof *self); /* we need it zeroed */
    if (self == NULL)
    {
	rc = RC (rcFF, rcTable, rcConstructing, rcParam, rcNull);
	LOGERR (klogErr, rc, "KFFTablesMake: self could not be allocated");
    }
    else
    {
/* 	uint32_t unknown; */
/* calloc does all this
	self->typecount = self->typesize = self->classcount =
	    self->classsize = 0;
	KFFClass = NULL;
	KFFType = NULL;
*/
/* might not want to count on BSTreeInit to set to zero and nothing else */
	BSTreeInit (&self->typetree);
	BSTreeInit (&self->classtree);
	atomic32_set (&self->refcount,1);
	/* initialize the tables to have an unknown class and unknown type */
#if 1
/* is this wise? */
	rc = KFFTablesAddType (self, NULL, "Unknown", "Unknown", 7, 7);
	if (rc == 0)
	{
	    *kmmtp = self;
	    return 0;
	}
#else
	rc = KFFTablesAddClass (self, &unknown,
				"Unknown", 7);
	if (rc == 0)
	{
	    uint32_t u;
	    rc = KFFTablesAddType (self, &u, unknown,
				   "Unknown", 7);
	    if (rc == 0)
	    {
		*kmmtp = self;
		return 0;
	    }
	}
#endif
	KFFTablesRelease (self);
    }
    return rc;
}

rc_t CC KFFTablesGetClassDescr (const KFFTables * self,
                                KFileFormatClass tid,
                                size_t * len,
                                char ** pd)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    *pd = NULL;
    *len = 0;
    if (self == NULL)
    {
	rc = RC (rcFF, rcTable, rcAccessing, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFTablesGetClassDescr: self is Null");
    }
    else
    {
	if ( ( tid < 0 )||( (uint32_t)tid > self->classcount ) )
	{
	    rc = RC (rcFF, rcTable, rcAccessing, rcParam, rcNotFound);
	    LOGERR (klogErr, rc, "KFFTablesGetClassDescr: class ID out of range");
	}
	else
	{
	    *pd = self->classindex[tid]->descr;
	    *len = self->classindex[tid]->len;
	}
    }
    return rc;
}

rc_t CC KFFTablesGetTypeDescr (const KFFTables * self,
                               KFileFormatType tid,
                               size_t * len,
                               char ** pd)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    *pd = NULL;
    *len = 0;
    if (self == NULL)
    {
	rc = RC (rcFF, rcTable, rcAccessing, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFTablesGetTypeDescr: self is Null");
    }
    else
    {
	if ( (uint32_t)tid > self->typecount )
	{
	    rc = RC (rcFF, rcTable, rcAccessing, rcParam, rcNotFound);
	    LOGERR (klogErr, rc, "KFFTablesGetTypeDescr: type ID out of range");
	}
	else
	{
	    *pd = self->typeindex[tid]->descr;
	    *len = self->typeindex[tid]->len;
	}
    }
    return rc;
}

/* not currently used and incomplete
rc_t KFFTablesGetClassId (const KFFTables * self,
			  const char ** pd,
			  KFileFormatClass * cid)
{
    rc_t rc;

    FUNC_ENTRY();

    rc = 0;
    *cid = 0;
    if (self == NULL)
    {
	rc = RC (rcFF, rcTable, rcAccessing, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFTablesGetClassId: self is Null");
    }
    else
    {
    }
    return rc;
}
*/
rc_t CC KFFTablesGetTypeId (const KFFTables * self,
                            const char * pd,
                            KFileFormatType * tid,
                            KFileFormatClass * cid)
{
    rc_t rc;

    FUNC_ENTRY();

    rc = 0;
    *tid = 0;
    if (self == NULL)
    {
	rc = RC (rcFF, rcTable, rcAccessing, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KFFTablesGetTypeId: self is Null");
    }
    else
    {
	KFileFormatType tt;
	tt = KFFTablesFindKFileFormatType (self, pd);
	if (tt > 0)
	{
	    if (tid)
		*tid = tt;
	    if (cid)
		*cid = self->typeindex[tt]->class;
	}
	else
	{
	    if (tid)
		*tid = tt;
	    if (cid)
		*cid = tt; /* same fail values */
	}
    }
    return rc;
}
