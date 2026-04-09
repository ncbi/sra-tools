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
#include <magic.h>
#include <klib/rc.h>
#include <kfs/file.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/debug.h>
#include <klib/container.h>
#include <kfs/directory.h>
#include <kfg/config.h>
#include <sysalloc.h>

#include "fileformat.h"
#include "fileformat-priv.h"
struct KMagicFileFormat;
#define KFILEFORMAT_IMPL struct KMagicFileFormat
#include "impl.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <os-native.h>
#include <stdio.h>

typedef
struct KMagicNode
{
    BSTNode		node;
    atomic32_t 		refcount;
    KFileFormatType	typeid;
    size_t		kfflen;
    size_t		magiclen;
    char *		magicdescr;
    char		kffdescr [1];
} KMagicNode;

static
rc_t KMagicNodeDestroy (const KMagicNode * cself)
{
    rc_t rc;
    KMagicNode * self;

    FUNC_ENTRY();

    rc = 0;
    self = (KMagicNode*)cself;

    if (self == NULL)
    {
	rc = RC (rcFF, rcIndex, rcDestroying, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KMagicNodeDestroy: self == NULL");
    }
    else
    {
	free (self);
    }
    return rc;
}

static
rc_t KMagicNodeRelease (const KMagicNode * cself)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (cself != NULL)
    {
        KMagicNode *self = (KMagicNode*)cself;
        if (atomic32_dec_and_test (&self->refcount))
            return  KMagicNodeDestroy (cself);
    }
    return rc;
}
/* not used at this time */
#if 0
static
rc_t KMagicNodeAddRef (const KMagicNode * self)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (self != NULL)
        atomic32_inc (&((KFileFormat*)self)->refcount);
    return rc;
}
#endif
static
rc_t KMagicNodeMake (KMagicNode ** kmmp, const KFFTables * tables,
		     const char * magicdescr, size_t magiclen,
		     const char * kffdescr, size_t kfflen)

{
    KMagicNode * self;
    rc_t rc = 0;

    FUNC_ENTRY();

    if (magicdescr == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcNull);
	LOGERR (klogErr, rc, "KMagicNodeMake: magicdescr == NULL");
    }
    else if (kffdescr == NULL)
    {
	rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcNull);
	LOGERR (klogErr, rc, "KKffNodeMake: kffdescr == NULL");
    }
    else
    {
	if (magiclen > DESCRLEN_MAX)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcInvalid);
	    LOGERR (klogErr, rc, "KMagicNodeMake: magicdescr too long");
	}
	else if (magiclen == 0)
	{
	    rc = RC (rcFF, rcIndex, rcConstructing, rcParam, rcEmpty);
	    LOGERR (klogErr, rc, "KMagicNodeMake: magicdescr too short");
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
		self = malloc (sizeof (*self) + magiclen + kfflen + 1);
		if (self == NULL)
		{
		    rc = RC (rcFF,  rcIndex, rcConstructing, rcMemory, rcExhausted);
		    LOGERR (klogErr, rc, "KMagicNodeMake: self could not be allocated");
		}
		else
		{
		    atomic32_set (&self->refcount,1);
		    self->typeid = typeid;
		    self->kfflen = kfflen;
		    self->magiclen = magiclen;
		    self->magicdescr = self->kffdescr + kfflen + 1;
		    memmove (self->kffdescr, kffdescr, kfflen);
		    memmove (self->magicdescr, magicdescr, magiclen);
		    self->kffdescr[self->kfflen] = '\0';
		    self->magicdescr[self->magiclen] = '\0';
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
struct KMagicTable
{
    atomic32_t 	refcount;
    BSTree	tree;
} KMagicTable;

static
void KMagicNodeWhack (BSTNode * n, void * ignored)
{
    FUNC_ENTRY();

    (void)KMagicNodeRelease((KMagicNode*)n);
}
static
rc_t KMagicTableDestroy (KMagicTable * cself)
{
    rc_t rc;
    KMagicTable * self;

    FUNC_ENTRY();

    rc = 0;
    self = (KMagicTable*)cself;
    if (self == NULL)
    {
	rc = RC (rcFF, rcTable, rcDestroying, rcSelf, rcNull);
	LOGERR (klogErr, rc, "KMagicTableDestroy: self == NULL");
    }
    else
    {
	BSTreeWhack (&self->tree, KMagicNodeWhack, NULL);
	free (self);
    }
    return rc;
}
static
rc_t KMagicTableRelease (const KMagicTable * cself)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (cself != NULL)
    {
	KMagicTable *self = (KMagicTable*)cself;
        if (atomic32_dec_and_test (&self->refcount))
	    return  KMagicTableDestroy (self);
    }
    return rc;
}
#if 0
static /* not used at this time */
rc_t KMagicTableAddRef (const KMagicTable * self)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    if (self != NULL)
        atomic32_inc (& ((KFileFormat*) self)->refcount);
    return rc;
}
#endif
static
rc_t KMagicTableMake (KMagicTable ** kmmtp)
{
    KMagicTable * self;
    rc_t rc = 0;

    FUNC_ENTRY();

    self = malloc (sizeof *self);
    if (self == NULL)
    {
        rc = RC (rcFF, rcTable, rcConstructing, rcParam, rcNull);
        LOGERR (klogErr, rc, "KMagicTableMake: self could not be allocated");
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
int64_t KMagicNodeCmp (const void* item, const BSTNode * n)
{
    size_t len;
    KMagicNode * mn = (KMagicNode *)n;

    FUNC_ENTRY();

    /* -----
     * we only check this many characters of the comparison item
     * we need only this part to match and ignore characters after
     * this in the comparison string
     */
    len = mn->magiclen;
    return strncmp (item, mn->magicdescr, len);
}

static
rc_t KMagicTableFind (KMagicTable * self, KMagicNode ** node, const char * str)
{
    rc_t rc = 0;

    FUNC_ENTRY();

    *node = (KMagicNode*)BSTreeFind (&self->tree, str, KMagicNodeCmp);
    if (*node == NULL)
    {
/* 	rc = RC (rcFF, rcTable, rcSearching, rcNode, rcNotFound); */
        KFF_DEBUG (("%s: Could not find %s\n", __func__, str));
    }
    return rc;
}

static
int64_t KMagicNodeSort (const BSTNode* item, const BSTNode * n)
{
    const char * str1;
    const char * str2;

    FUNC_ENTRY();

    str1 = ((KMagicNode *)item)->magicdescr;
    str2 = ((KMagicNode *)n)->magicdescr;
    return strcmp (str1, str2);
}
static
rc_t KMagicTableInsert (KMagicTable * self, KMagicNode *node)
{
    FUNC_ENTRY();

    return (BSTreeInsert (&self->tree, &node->node, KMagicNodeSort));
}

/* -----
 * format is
 * whitechar := {' '|'\f'|'\t'|'\v'}
 * whitespace := whitechar*
 * magic-str = !whitespace!{'\t'|'\n'}*
 * kff-str = !whitespace!{'\t'|'\n'}*
 *
 * A line is
 * [<whitespace>]#<comment line skipped>\n
 * Or
 * [<whitespace>]<magic-str>\t[<whitespace>]<kff-str>\n
 * Or
 * [<whitespace>]\n
 *
 * NOTE: whitespace at the right end of the two strings is included in the strings
 * NOTE: We do not look for '\v'|'\f' within the strings though we maybe should
 * NOTE: the magic string definitely allows white space and punctuation
 */
static
rc_t KMagicTableInit (KMagicTable * self, const KFFTables * tables, const char * buffer, size_t len)
{
    rc_t rc;
    const char * magic;
    const char * kff;
    const char * tab;
    const char * newline;
    const char * line;
    const char * limit;
    KMagicNode * node;

    FUNC_ENTRY();

    rc = 0;

    /* -----
     * we try to go all the way through the buffer line by line
     * which by this coding could actually be all blank
     */
    limit = buffer + len;
    for (line = buffer; line < limit; line = newline+1)
    {
	for (magic = line; isspace (*magic); magic++, len--)
	{
	    if (len == 0) /* last of the file was all whitespace so quit */
		break;
	}
	newline = memchr (magic, '\n', len);
	if (newline == NULL)
	    newline = magic + len;
	/* -----
	 * If the first character on the line is #
	 * we treat it as a comment (matches sh/bash/libmagic/etc.
	 */
	if (*magic == '#')
	{
	    /* -----
	     * skip this line
	     */
	    len -= newline+1 - magic;
	    continue;
	}

	tab = memchr (magic, '\t', len);
	if (tab == NULL)
	{
	    rc = RC (rcFF, rcFileFormat, rcConstructing, rcFile, rcInvalid);
	    LOGERR (klogFatal, rc, "No <TAB> between magic and kff");
	    break;
	}
	kff = tab + 1;
	for (len -= kff - magic;
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
 	rc = KMagicNodeMake (&node, tables, magic, tab-magic, kff, newline-kff);
	if (rc != 0)
	{
	    LOGERR (klogFatal, rc, "Failure to make node");
	    break;
	}
	rc = KMagicTableInsert(self, node);
	if (rc != 0)
	{
	    LOGERR (klogFatal, rc, "Failure to insert node");
	    break;
	}
    }
    if (rc == 0)
    {
	char unknown[] = "Unknown";

	rc = KMagicNodeMake (&node, tables, unknown, sizeof (unknown) - 1, unknown, sizeof (unknown) - 1);
	if (rc != 0)
	{
	    LOGERR (klogFatal, rc, "Failure to make node");
	}
	else
	{
	    rc = KMagicTableInsert(self, node);
	    if (rc != 0)
	    {
		LOGERR (klogFatal, rc, "Failure to insert node");
	    }
	}
    }
    return rc;
}


/*--------------------------------------------------------------------------
 * KMagicFileFormat
 *  a file content (format) categorizer
 */

typedef
struct KMagicFileFormat
{
    KFileFormat	  dad;
    magic_t 	  cookie;
    KMagicTable * table;
} KMagicFileFormat;

static rc_t KMagicFileFormatDestroy (KMagicFileFormat *self);
static rc_t KMagicFileFormatGetTypeBuff (const KMagicFileFormat *self,
					 const void * buff, size_t buff_len,
					 KFileFormatType * type,
					 KFileFormatClass * class,
					 char * description,
					 size_t descriptionmax,
					 size_t * descriptionlength);
static rc_t KMagicFileFormatGetTypePath (const KMagicFileFormat *self,
					 const KDirectory * dir,
					 const char * path,
					 KFileFormatType * type,
					 KFileFormatClass * class,
					 char * description,
					 size_t descriptionmax,
					 size_t * descriptionlength);
static
KFileFormat_vt_v1 vt_v1 =
{
    1, 1, /* maj, min */
    KMagicFileFormatDestroy,
    KMagicFileFormatGetTypeBuff,
    KMagicFileFormatGetTypePath
};



/* Destroy
 *  destroy FileFormat
 */
static
rc_t KMagicFileFormatDestroy (KMagicFileFormat *self)
{
    FUNC_ENTRY();

    rc_t rc = KMagicTableRelease (self->table);
    magic_close (self->cookie);
    {
        rc_t rc2 = KFFTablesRelease (self->dad.tables);
        if ( rc == 0 )
            rc = rc2;
    }
    free (self);
    return rc;
}

/* Type
 *  returns a KMagicFileFormatDesc
 *  [OUT] rc_t               return
 *  [IN]  const KMagicFileFormat *  self
 *  [IN]  void **            buffer       buffer to hold returned description
 *  [IN]  size_t             buffer_size  size of the buffer
 *  [OUT] char **            descr        text description of file type
 *  [IN]  size_t             descr_max    maximum size of string descr can hold
 *  [OUT] size_t *           descr_len    length of returned descr (not including NUL
 */
static
rc_t KMagicFileFormatGetTypePath (const KMagicFileFormat *self, const KDirectory * dir, const char * path,
				  KFileFormatType * type, KFileFormatClass * class,
				  char * descr, size_t descr_max, size_t *descr_len)
{
    rc_t rc = 0;
    uint8_t	buff	[8192];
    size_t	bytes_read;

    rc = KDirectoryAddRef (dir);
    if (rc == 0)
    {
	const KFile * file;
	rc = KDirectoryOpenFileRead (dir, &file, "%s", path);
	if (rc == 0)
	{
	    rc = KFileRead (file, 0, buff, sizeof buff, &bytes_read);
	    {
		rc = KMagicFileFormatGetTypeBuff (self,buff, bytes_read,
						  type, class, descr,
						  descr_max, descr_len);
	    }
	    KFileRelease (file);
	}
	KDirectoryRelease (dir);
    }
    return rc;
}
static
rc_t KMagicFileFormatGetTypeBuff (const KMagicFileFormat *self, const void * buff, size_t buff_len,
				  KFileFormatType * type, KFileFormatClass * class,
				  char * descr, size_t descr_max, size_t *descr_len)
{
    rc_t rc = 0;
    const char * b;

    FUNC_ENTRY();

    if (type != NULL)
	*type = kfftError;
    if (class != NULL)
	*class = kffcError;
    b = magic_buffer (self->cookie, buff, buff_len);
    if (b == NULL)
	rc = RC (rcFF, rcFileFormat, rcParsing, rcFormat, rcUnrecognized);
    else
    {
	KMagicNode * node;
	size_t c;

        KFF_DEBUG (("magic_buffer returned %s\n", b));
#if 1
 	rc = KMagicTableFind (self->table, &node, b);
	if (rc == 0)
	{
	    KFileFormatClass cid;
	    KFileFormatType tid;
#define TABLES self->dad.tables
	    if (node == NULL)
		rc = KMagicTableFind (self->table, &node, "Unknown");
	    if (rc == 0)
		rc = KFFTablesGetTypeId (TABLES, node->kffdescr, &tid, &cid);
	    if (rc == 0)
	    {
		c = node->kfflen;
		if (c > descr_max)
		    c = descr_max-1;
		if (descr)
		    string_copy(descr, descr_max, node->kffdescr, c);
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
    size_t size;
	size_t l = string_measure(b, &size);
	if (descr != NULL)
	{
	    string_copy (descr, descr_max, b, l);
	    if (descr_max < l)
		descr[descr_max-1] = 0;
	}
	if (descr_len != NULL)
	    *descr_len = strlen (b);
	if (type != NULL)
	    *type = kfftUnknown;
	if (class != NULL)
	    *class = kffcUnknown;
#endif
    }
    return rc;
}


rc_t KMagicFileFormatMake (KFileFormat ** pft, const char* magic, size_t magiclen,
			   const char * typeAndClass, size_t tclen)
{
    rc_t rc = 0;
    KMagicFileFormat * self;

    FUNC_ENTRY();

    self = malloc (sizeof * self);
    if (self == NULL)
    {
        rc = RC (rcFF, rcFileFormat, rcAllocating, rcMemory, rcExhausted);
        LOGERR (klogFatal, rc, "Failed to allocate for KMagicFileFormat");
    }
    else
    {
        rc = KFileFormatInit (&self->dad, (const KFileFormat_vt *)&vt_v1, typeAndClass, tclen);
        if (rc == 0)
        {
            rc = KMagicTableMake (&self->table);
            if (rc == 0)
            {
                rc = KMagicTableInit (self->table, self->dad.tables, magic, magiclen);
                if (rc == 0)
                {
                    self->cookie = magic_open (MAGIC_PRESERVE_ATIME);
        /* 		    self->cookie = magic_open (MAGIC_PRESERVE_ATIME|MAGIC_DEBUG|MAGIC_CHECK); */
                    if (self->cookie == NULL)
                    {
                        rc = RC (rcFF, rcFileFormat, rcConstructing, rcResources, rcNull);
                        LOGERR (klogFatal, rc, "Unable to obtain libmagic cookie");
                    }
                    else
                    {
                        int load_code = magic_load (self->cookie, NULL); // VDB-1524: always use the default magic database file
                        if (load_code != 0) /* defined as 0 success and -1 as fail */
                        {
                            KFF_DEBUG (("%s: magic_load() failed with load code %d(%s)\n", __func__, load_code, magic_error (self->cookie) ));
                            rc = RC (rcFF, rcFileFormat, rcLoading, rcLibrary, rcUnexpected);
                        }
                        else
                        {
                            *pft = &self->dad;
                            KFF_DEBUG (("%s Success\n", __func__));
                            return 0;
                        }

                        magic_close (self->cookie);
                    }
                }
                else
                    LOGERR (klogErr, rc, "Fail from KMagicTableInit");

                KMagicTableRelease (self->table);
            }
            else
                LOGERR (klogErr, rc, "Fail from KMagicTableMake");
        }
        else
            LOGERR (klogErr, rc, "Fail from KFileFormatInit");
        free (self);
    }
    return rc;
}

