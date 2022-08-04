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
#include <kfs/file.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/container.h>

#include "fileformat.h"
#include "fileformat-priv.h"
struct KExtFileFormat;
#define KFILEFORMAT_IMPL struct KExtFileFormat
#include "impl.h"

#include <sysalloc.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <os-native.h>
#include <stdio.h>

struct KDirectory;

typedef
struct KExtNode
{
    BSTNode		node;
    atomic32_t 		refcount;
    KFileFormatType	typeid;
    size_t		kfflen;
    size_t		extlen;
    char *		extdescr;
    char		kffdescr [1];
} KExtNode;

static
rc_t KExtNodeDestroy (const KExtNode * cself)
{
    rc_t rc = 0;
    KExtNode * self = (KExtNode*)cself;

    FUNC_ENTRY();

    if (self == NULL)
    {
	rc = RC (rcFF, rcIndex, rcDestroying, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KExtNodeDestroy: self == NULL");
    }
    else
    {

        /* memory leak?  do we need to release KFFTables? */


	free (self);
    }
    return rc;
}

static
rc_t KExtNodeRelease (const KExtNode * cself)
{
    rc_t rc = 0;
    FUNC_ENTRY();

    if (cself != NULL)
    {
        KExtNode *self = (KExtNode*)cself;
        if (atomic32_dec_and_test (&self->refcount))
            return  KExtNodeDestroy (cself);
    }
    return rc;
}
/* not used at this time */
#if 0
static
rc_t KExtNodeAddRef (const KExtNode * self)
{
    rc_t rc = 0;
    FUNC_ENTRY();

    if (self != NULL)
        atomic32_inc (&((KFileFormat*)self)->refcount);
    return rc;
}
#endif
static
rc_t KExtNodeMake (KExtNode ** kmmp, const KFFTables * tables,
		     const char * extdescr, size_t extlen,
		     const char * kffdescr, size_t kfflen)

{
    KExtNode * self;
    rc_t rc = 0;

    FUNC_ENTRY();


    if (extdescr == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcNull);
	LOGERR (klogErr, rc, "KExtNodeMake: extdescr == NULL");
    }
    else if (kffdescr == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcNull);
	LOGERR (klogErr, rc, "KKffNodeMake: kffdescr == NULL");
    }
    else
    {
	if (extlen > DESCRLEN_MAX)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcInvalid);
	    LOGERR (klogErr, rc, "KExtNodeMake: extdescr too long");
	}
	else if (extlen == 0)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcEmpty);
	    LOGERR (klogErr, rc, "KExtNodeMake: extdescr too short");
	}
	else if (kfflen > DESCRLEN_MAX)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcInvalid);
	    LOGERR (klogErr, rc, "KKffNodeMake: kffdescr too long");
	}
	else if (kfflen == 0)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcEmpty);
	    LOGERR (klogErr, rc, "KKffNodeMake: kffdescr too short");
	}
	else
	{
	    KFileFormatType typeid;
	    rc = KFFTablesGetTypeId (tables, kffdescr, &typeid, NULL);
	    if (rc == 0)
	    {
		self = malloc (sizeof (*self) + extlen + kfflen + 1);
		if (self == NULL)
		{
		    rc = RC (rcFF,  rcIndex, rcConstructing, rcMemory, rcExhausted);
		    LOGERR (klogErr, rc, "KExtNodeMake: self could not be allocated");
		}
		else
		{
		    atomic32_set (&self->refcount,1);
		    self->typeid = typeid;
		    self->kfflen = kfflen;
		    self->extlen = extlen;
		    self->extdescr = self->kffdescr + kfflen + 1;
		    memmove (self->kffdescr, kffdescr, kfflen);
		    memmove (self->extdescr, extdescr, extlen);
		    self->kffdescr[self->kfflen] = '\0';
		    self->extdescr[self->extlen] = '\0';
		    *kmmp = self;
		    return 0;
		}
	    }
	}
    }
    return rc;
}


/* -------------------------
 *
 */
typedef
struct KExtTable
{
    atomic32_t 	refcount;
    BSTree	tree;
} KExtTable;

static
void KExtNodeWhack (BSTNode * n, void * ignored)
{
    FUNC_ENTRY();

    (void)KExtNodeRelease((KExtNode*)n);
}
static
rc_t KExtTableDestroy (KExtTable * cself)
{
    rc_t rc;
    KExtTable * self;

    FUNC_ENTRY();

    rc = 0;
    self = (KExtTable*)cself;
    if (self == NULL)
    {
	rc = RC (rcFF, rcTable, rcDestroying, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KExtTableDestroy: self == NULL");
    }
    else
    {
	BSTreeWhack (&self->tree, KExtNodeWhack, NULL);
	free (self);
    }
    return rc;
}
static
rc_t KExtTableRelease (const KExtTable * cself)
{
    rc_t rc = 0;
    FUNC_ENTRY();

    if (cself != NULL)
    {
	KExtTable *self = (KExtTable*)cself;
        if (atomic32_dec_and_test (&self->refcount))
	    return  KExtTableDestroy (self);
    }
    return rc;
}
#if 0
static /* not used at this time */
rc_t KExtTableAddRef (const KExtTable * self)
{
    rc_t rc = 0;
    FUNC_ENTRY();

    if (self != NULL)
        atomic32_inc (& ((KFileFormat*) self)->refcount);
    return rc;
}
#endif
static
rc_t KExtTableMake (KExtTable ** kmmtp)
{
    KExtTable * self;
    rc_t rc = 0;

    FUNC_ENTRY();

    self = malloc (sizeof *self);
    if (self == NULL)
    {
        rc = RC (rcFF, rcTable, rcConstructing, rcParam, rcNull);
        LOGERR (klogErr, rc, "KExtTableMake: self could not be allocated");
    }
    else
    {
        atomic32_set (&self->refcount,1);
        BSTreeInit (&self->tree);
        *kmmtp = self;
    }
    return rc;
}

static
int64_t KExtNodeCmp (const void* item, const BSTNode * n)
{
    size_t len;
    KExtNode * mn = (KExtNode *)n;
    String *s = ( String * ) item;

    FUNC_ENTRY();

    /* -----
     * we only check this many characters of the comparison item
     * we need only this part to match and ignore characters after
     * this in the comparison string
     */
    len = mn->extlen;
    return strcase_cmp ( s -> addr, s -> len , mn->extdescr, len, len );
}

static
rc_t KExtTableFind (KExtTable * self, KExtNode ** node, const char * str)
{
    rc_t rc = 0;
    String s;

    FUNC_ENTRY();

    StringInitCString ( &s, str );

    *node = (KExtNode*)BSTreeFind (&self->tree, &s, KExtNodeCmp);
    if (*node == NULL)
    {
/* 	rc = RC (rcFF, rcTable, rcSearching, rcNode, rcNotFound); */
        KFF_DEBUG (("%s: Could not find %s\n", __func__, str));
    }
    return rc;
}
/* maxlen includes the terminating NUL */
#if 0 /* not in use at this time */
static
rc_t KExtTableFindKFFDescr (KExtTable * self, const char * str, char * kff, size_t maxlen)
{
    rc_t rc;
    KExtNode * np;

    FUNC_ENTRY();


    if (self == NULL)
    {
	rc = RC (rcFF, rcFileFormat, rcSearching, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KExtTableFindKFFDecr:self == NULL");
	return rc;
    }
    if (str == NULL)
    {
	rc = RC (rcFF, rcFileFormat, rcSearching, rcParam, rcNull);
	LOGERR (klogErr, rc, "KExtTableFindKFFDecr: searchstring is NULL");
	return rc;
    }
    if (kff == NULL)
    {
	rc = RC (rcFF, rcFileFormat, rcSearching, rcParam, rcNull);
	LOGERR (klogErr, rc, "KExtTableFindKFFDecr: found storage is NULL");
	return rc;
    }
    rc = KExtTableFind (self, &np, str);
    if (maxlen <= np->kfflen) /* kfflen does not include NUL */
    {
	rc = RC (rcFF, rcFileFormat, rcSearching, rcParam, rcTooLong);
	LOGERR (klogErr, rc, "KExtTableFindKFFDecr: found storage is NULL");
	return rc;
    }
    memmove (kff, np->kffdescr, np->kfflen);
    kff[np->kfflen] = '\0';
    return rc;
}
#endif
static
int64_t KExtNodeSort (const BSTNode* item, const BSTNode * n)
{
    KExtNode *n1 = ( KExtNode * ) item;
    KExtNode *n2 = ( KExtNode * ) n;

    FUNC_ENTRY();

    return strcase_cmp ( n1 -> extdescr, n1 -> extlen,
                         n2 -> extdescr, n2 -> extlen, n2 -> extlen );
}
static
rc_t KExtTableInsert (KExtTable * self, KExtNode *node)
{

    FUNC_ENTRY();

    return (BSTreeInsert (&self->tree, &node->node, KExtNodeSort));
}
/* not is use at this time */
#if 0
static
rc_t KExtTableBufferRead (KExtTable * self, KFFTables * tables,
			    const char * buff, size_t bufflen)
{
    rc_t rc = 0;
    const char * kff;
    const char * ext;
    const char * nl;
    size_t kfflen;
    size_t extlen;


    FUNC_ENTRY();


    /* -----
     * until we get all the way through the buffer
     * which by this coding could actually be all blank
     */
    while (bufflen)
    {
	ext = buff;
	/* -----
	 * allow leading white space including blank lines
	 */
	if (isspace (*ext))
	{
	    buff++;
	    bufflen --;
	    continue;
	}
	/* -----
	 * not a comment line so find the tab splitting the sections
	 */
	kff = memchr (ext, '\t', bufflen);
	if (kff == NULL)
	{
	    /* couldn't find it so blae the document and quit */
	bad_line:
	    rc = RC (rcFF, rcBuffer, rcParsing, rcFormat, rcCorrupt);
	    /* log error */
	    bufflen = 0;
	    continue;
	}
	/* -----
	 * the ext portion of the line is from the first non-white space
	 * through the character before the tab.
	 */
	extlen = kff - ext;
	bufflen -= extlen + 1;
	kff++; /* point past the tab */
	while (bufflen) /* skip white space */
	{
	    if (*kff == '\n') /* end of line now is a format error */
	    {
		goto bad_line;
	    }
	    if (!isspace (*kff)) /* break at non shite space character */
		break;
	    bufflen --;
	    kff ++;
	}
	if (bufflen == 0) /* no kff descr */
	    goto bad_line;
	nl = memchr (kff, '\n', bufflen);
	if (nl == NULL) /* no EOL but last line in buffer */
	{
	    kfflen = bufflen;
	    bufflen = 0;
	}
	else /* not last unfinished line */
	{
	    kfflen = nl - ext;
	    bufflen -= kfflen + 1;
	    buff = nl + 1;
	}
	{
	    KExtNode * np;
	    rc = KExtNodeMake (&np, tables, ext, extlen,
				 kff, kfflen);
	    if (rc != 0)
	    {
		/* LOG ERR */
		break;
	    }
	}
    }
    ( break;
	    }
	}
    }
    return rc;
}
#endif
/* not used at this time */
#if 0
LIB_EXPORT rc_t CC KExtTableRead (KExtTable * self, const KFile * file))
{
    rc_t rc = 0;
    /* setup KMMap */
    /* call KExtTableBufferRead */
    return rc;
}

LIB_EXPORT rc_t CC KExtTableWrite (const KExtTable * self, KFile * file)
{
    rc_t rc = 0;

    return rc;
}
#endif
/* -----
 * format is
 * whitechar := {' '|'\f'|'\t'|'\v'}
 * whitespace := whitechar*
 * ext-str = !whitespace!{'\t'|'\n'}*
 * kff-str = !whitespace!{'\t'|'\n'}*
 *
 * A line is
 * [<whitespace>]#<comment line skipped>\n
 * Or
 * [<whitespace>]<ext-str>\t[<whitespace>]<kff-str>\n
 * Or
 * [<whitespace>]\n
 *
 * NOTE: whitespace at the right end of the two strings is included in the strings
 * NOTE: We do not look for '\v'|'\f' within the strings though we maybe should
 * NOTE: the ext string definitely allows white space and punctuation
 */
static
rc_t KExtTableInit (KExtTable * self, const KFFTables * tables, const char * buffer, size_t len)
{
    rc_t rc = 0;
    const char * ext;
    const char * kff;
    const char * tab;
    const char * newline;
    const char * line;
    const char * limit;
    KExtNode * node;

    FUNC_ENTRY();

    /* moved this block of code from the bottom of the function
       to assure that this node is always created */
	char unknown[] = "Unknown";

	rc = KExtNodeMake (&node, tables, unknown, sizeof (unknown) - 1, unknown, sizeof (unknown) - 1);
	if (rc != 0)
	{
	    LOGERR (klogFatal, rc, "Failure to make node");
        return rc;
	}

    rc = KExtTableInsert(self, node);
    if (rc != 0)
    {
        LOGERR (klogFatal, rc, "Failure to insert node");
	    return rc;
	}

    /* -----
     * we try to go all the way through the buffer line by line
     * which by this coding could actually be all blank
     */
    limit = buffer + len;
    for (line = buffer; line < limit; line = newline+1)
    {
        for (ext = line; isspace (*ext); ext++, len--)
        {
            if (len == 0) /* last of the file was all whitespace so quit */
                break;
        }
        newline = memchr (ext, '\n', len);
        if (newline == NULL)
            newline = ext + len;
        /* -----
         * If the first character on the line is #
         * we treat it as a comment (matches sh/bash/libext/etc.
         */
        if (*ext == '#')
        {
            /* -----
             * skip this line
             */
            len -= newline+1 - ext;
            continue;
        }

        tab = memchr (ext, '\t', len);
        if (tab == NULL)
        {
            rc = RC (rcFF, rcFileFormat, rcConstructing, rcFile, rcInvalid);
            LOGERR (klogFatal, rc, "No <TAB> between ext and kff");
            break;
        }
        kff = tab + 1;
        for (len -= kff - ext;
             isspace (*kff);
             len--, kff++)
        {
            if (len == 0)
            {
                rc = RC (rcFF, rcFileFormat, rcConstructing, rcFile, rcInvalid);
                LOGERR (klogFatal, rc, "No kff after <TAB>");
                break;
            }
        }
        if (newline == kff)
        {
            rc = RC (rcFF, rcFileFormat, rcConstructing, rcFile, rcInvalid);
            LOGERR (klogFatal, rc, "No kff after whitespace");
            break;
        }
        len -= newline+1 - kff;
        rc = KExtNodeMake (&node, tables, ext, tab-ext, kff, newline-kff);
        if (rc != 0)
        {
            LOGERR (klogFatal, rc, "Failure to make node");
            break;
        }
        rc = KExtTableInsert(self, node);
        if (rc != 0)
        {
            LOGERR (klogFatal, rc, "Failure to insert node");
            break;
        }
    }
    return rc;
}


/*--------------------------------------------------------------------------
 * KExtFileFormat
 *  a file content (format) categorizer
 */

typedef
struct KExtFileFormat
{
    KFileFormat	  dad;
    KExtTable * table;
} KExtFileFormat;

static rc_t KExtFileFormatDestroy (KExtFileFormat *self);
static rc_t KExtFileFormatGetTypeBuff (const KExtFileFormat *self,
				       const void * buff, size_t buff_len,
				       KFileFormatType * type,
				       KFileFormatClass * class,
				       char * description,
				       size_t descriptionmax,
				       size_t * descriptionlength);
static rc_t KExtFileFormatGetTypePath (const KExtFileFormat *self,
				       const struct  KDirectory * dir, const char * path,
				       KFileFormatType * type,
				       KFileFormatClass * class,
				       char * description,
				       size_t descriptionmax,
				       size_t * descriptionlength);
static
KFileFormat_vt_v1 vt_v1 =
{
    1, 1, /* maj, min */
    KExtFileFormatDestroy,
    KExtFileFormatGetTypeBuff,
    KExtFileFormatGetTypePath
};



/* Destroy
 *  destroy FileFormat
 */
static
rc_t KExtFileFormatDestroy (KExtFileFormat *self)
{
    FUNC_ENTRY();
    rc_t rc = KFFTablesRelease (self->dad.tables);
    {
        rc_t rc2 = KExtTableRelease (self->table);
        if (rc == 0)
            rc = rc2;
    }
    free (self);
    return 0;
}

/* Type
 *  returns a KExtFileFormatDesc
 *  [OUT] rc_t               return
 *  [IN]  const KExtFileFormat *  self
 *  [IN]  void **            buffer       buffer to hold returned description
 *  [IN]  size_t             buffer_size  size of the buffer
 *  [OUT] char **            descr        text description of file type
 *  [IN]  size_t             descr_max    maximum size of string descr can hold
 *  [OUT] size_t *           descr_len    length of returned descr (not including NUL
 */
static
rc_t CC KExtFileFormatGetTypeBuff (const KExtFileFormat *self, const void * buff, size_t buff_len,
				KFileFormatType * type, KFileFormatClass * class,
				char * descr, size_t descr_max, size_t *descr_len)
{

    FUNC_ENTRY();

    return RC (rcFF, rcFileFormat, rcSearching, rcFormat, rcUnsupported);
}


static
rc_t CC KExtFileFormatGetTypePath (const KExtFileFormat *self,
                                const struct KDirectory * dir_ignored, const char * path,
                                KFileFormatType * type, KFileFormatClass * class,
                                char * descr, size_t descr_max, size_t *descr_len)
{
    rc_t rc = 0;
    const char * b;
    const char * s;
    size_t size;


    FUNC_ENTRY();


    if (type != NULL)
	*type = kfftError;
    if (class != NULL)
	*class = kffcError;

    s = strrchr (path, '/');
    if (s == NULL)
	s = path;


    b = strrchr (s, '.');
    if (b == NULL)
	b = path + string_measure(path, &size) - 1; /* will be an empty string when calls KExtTableFind */
    {
	KExtNode * node;
	size_t c;

        KFF_DEBUG (("%s: extension is %s\n", __func__, b+1));
#if 1
 	rc = KExtTableFind (self->table, &node, ++b);
	if (rc == 0)
	{
	    KFileFormatClass cid;
	    KFileFormatType tid;
	    if (node == NULL)
		rc = KExtTableFind (self->table, &node, "Unknown");
	    if (rc == 0)
		rc = KFFTablesGetTypeId (self->dad.tables, node->kffdescr, &tid, &cid);
	    if (rc == 0)
	    {
		c = node->kfflen;
		if (c > descr_max)
		    c = descr_max-1;
		if (descr)
		    string_copy (descr, descr_max, node->kffdescr, c);
		descr[c] = '\0';
		if (descr_len)
		    *descr_len = c;
		if (type)
		    *type = tid;
		if (class)
		    *class = cid;
	    }
	}
	if (rc != 0)
	{
	    if (descr_len != NULL)
		*descr_len = 0;
	    if (type != NULL)
		*type = kfftNotFound;
	    if (class != NULL)
		*class = kffcNotFound;
	}


#else
	size_t l = string_measure (b, &size);
	if (desc != NULL)
	{
	    string_copy (desc, desc_max, b, l);
	    if (desc_max < l)
		desc[desc_max-1] = 0;
	}
	if (descr_len != NULL)
	    *descr_len = l;
	if (type != NULL)
	    *type = kfftUnknown;
	if (class != NULL)
	    *class = kffcUnknown;
#endif
    }
    return rc;
}


LIB_EXPORT rc_t CC KExtFileFormatMake (KFileFormat ** pft,
			 const char* ext, size_t extlen,
			 const char * typeAndClass, size_t tclen)
{
    rc_t rc = 0;
    KExtFileFormat * self;


    FUNC_ENTRY();

    self = malloc (sizeof * self);
    if (self == NULL)
    {
	rc = RC (rcFF, rcFileFormat, rcAllocating, rcMemory, rcExhausted);
	LOGERR (klogFatal, rc, "Failed to allocate for KExtFileFormat");
    }
    else
    {
	rc = KFileFormatInit (&self->dad, (const KFileFormat_vt *)&vt_v1, typeAndClass, tclen);
	if (rc == 0)
	{
	    rc = KExtTableMake (&self->table);
	    if (rc == 0)
	    {
		rc = KExtTableInit (self->table, self->dad.tables, ext, extlen);
		{
		    if (rc == 0)
		    {
			*pft = &self->dad;
			return 0;
		    }
		}
		KExtTableRelease (self->table);
	    }
	}
	free (self);
    }
    return rc;
}

