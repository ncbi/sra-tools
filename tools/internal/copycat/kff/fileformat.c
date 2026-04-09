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
#include <sysalloc.h>


/* #include <kfs/directory.h> */

#include "kff/fileformat.h"
#include "kff/impl.h"

#include <stdio.h> /* remove after debugging */
#include <ctype.h>
#include <os-native.h>
#include <string.h>

struct KDirectory;


/*--------------------------------------------------------------------------
 * KFileFormat
 *
 */


/* Destroy
 *  destroy file
 */
rc_t CC KFileFormatDestroy ( KFileFormat *self )
{
    FUNC_ENTRY();

    if (self == NULL)
        return RC (rcFS, rcFile, rcDestroying, rcSelf, rcNull);

    switch (self->vt->v1 . maj)
    {
    case 1:
        return (* self->vt->v1 . destroy) (self);
    }

    return RC (rcFS, rcFile, rcDestroying, rcInterface, rcBadVersion);
}

/* AddRef
 *  creates a new reference
 *  ignores NULL references
 */
LIB_EXPORT rc_t CC KFileFormatAddRef ( const KFileFormat *self )
{
    FUNC_ENTRY();
    if (self != NULL)
        atomic32_inc (& ((KFileFormat*) self)->refcount);
    return 0;
}

/* Release
 *  discard reference to file
 *  ignores NULL references
 */
LIB_EXPORT rc_t CC KFileFormatRelease ( const KFileFormat *cself )
{
    FUNC_ENTRY();
    if (cself != NULL)
    {
	KFileFormat *self = (KFileFormat*)cself;
        if (atomic32_dec_and_test (&self->refcount))
	    return  KFileFormatDestroy (self);
    }
    return 0;
}


/* Type
 *  returns a KFileFormatDesc
 *  [OUT] rc_t               return
 *  [IN]  const KFileFormat *  self
 *  [IN]  void **            buffer       buffer to hold returned description
 *  [IN]  size_t             buffer_size  size of the buffer
 *  [OUT] char **            descr        text description of file type
 *  [IN]  size_t             descr_max    maximum size of string descr can hold
 *  [OUT] size_t *           descr_len    length of returned descr (not including NUL
 */
LIB_EXPORT rc_t CC KFileFormatGetTypePath ( const KFileFormat *self,
			     const struct KDirectory * dir, const char * path,
			     KFileFormatType * type, KFileFormatClass * class,
			     char * description, size_t descriptionmax,
			     size_t * descriptionlength )
{
    FUNC_ENTRY();

    if (self == NULL)
        return RC (rcFF, rcFileFormat, rcClassifying, rcSelf, rcNull);

    switch (self->vt->v1.maj)
    {
    case 1:
        if (self->vt->v1.min >= 1)
	    return (* self->vt->v1 . gettypepath) (self, dir, path, type, class,
						   description, descriptionmax,
						   descriptionlength);
        break;
    }
    return RC (rcFF, rcFileFormat, rcClassifying, rcInterface, rcBadVersion);
}

LIB_EXPORT rc_t CC KFileFormatGetTypeBuff ( const KFileFormat *self, const void * buff, size_t buff_len,
			     KFileFormatType * type, KFileFormatClass * class,
			     char * description, size_t descriptionmax,
			     size_t * descriptionlength )
{
    FUNC_ENTRY();

    if (self == NULL)
        return RC (rcFF, rcFileFormat, rcClassifying, rcSelf, rcNull);

    switch (self->vt->v1.maj)
    {
    case 1:
        if (self->vt->v1.min >= 1)
	    return (* self->vt->v1 . gettypebuff) (self, buff, buff_len, type, class,
						   description, descriptionmax,
						   descriptionlength);
        break;
    }
    return RC (rcFF, rcFileFormat, rcClassifying, rcInterface, rcBadVersion);
}

LIB_EXPORT rc_t CC KFileFormatGetClassDescr ( const KFileFormat *self, KFileFormatClass c,
			       char * description, size_t descriptionmax )
{
    rc_t rc;
    size_t max;

#undef ERROR
#define ERROR "ERROR"
#undef NOT_FOUND
#define NOT_FOUND "NOT FOUND"

    FUNC_ENTRY();

    if (c < kffcError)
    {
    error:
	max = (sizeof (ERROR) > descriptionmax-1) ? descriptionmax-1 : sizeof (ERROR)-1;
	memmove (description, ERROR, max);
	description[max] = '\0';
	return 0;
    }
    else if (c == kffcNotFound)
    {
	max = (sizeof (NOT_FOUND) > descriptionmax-1) ? descriptionmax-1 : sizeof (NOT_FOUND)-1;
	memmove (description, NOT_FOUND, max);
	description[max] = '\0';
	return 0;
    }
    else
    {
	char * cp;
	size_t z;

	rc = KFFTablesGetClassDescr(self->tables, c, &z, &cp);
	if (rc)
	    goto error;
	max = (z > descriptionmax-1) ? descriptionmax-1 : z;
	memmove (description, cp, max);
	description[max] = '\0';
	return 0;
    }

#undef ERROR
#undef NOT_FOUND
}
static
rc_t KFileFormatInitTypeAndClass (KFileFormat *self, const char * typeAndClass,
			    size_t len)
{
    rc_t rc;
    const char * type;
    const char * class;
    const char * tab;
    const char * newline;
    const char * line;
    const char * limit;

    FUNC_ENTRY();

    rc = 0;
    limit = typeAndClass + len;
    for (line = typeAndClass; line < limit; line = newline+1)
    {
	for (type = line; isspace (*type) && len; len--, type++)
	{
	    if (len == 0)
		break;
	}
	newline = memchr (type, '\n', len);
	if (newline == NULL)
	    newline = type + len;
	if (*type == '#')
	{
	    /* -----
	     * skip this line
	     */
	    len -= newline+1 - type;
	    continue;
	}
	tab = memchr (type, '\t', len);
	if (tab == NULL)
	{
	    rc = RC (rcFF, rcFileFormat, rcConstructing, rcFile, rcInvalid);
	    LOGERR (klogFatal, rc, "No <TAB> between type and class");
	    break;
	}
	class = tab + 1;
	for ( len -= class - line;
	      isspace (*class); len--, class++)
	{
	    if (len == 0)
	    {
		rc = RC (rcFF, rcFileFormat, rcConstructing, rcFile, rcInvalid);
		LOGERR (klogFatal, rc, "No class after <TAB>");
		break;
	    }
	}
	if (newline == class)
	{
	    rc = RC (rcFF, rcFileFormat, rcConstructing, rcFile, rcInvalid);
	    LOGERR (klogFatal, rc, "No class after whitespace");
	    break;
	}
	rc = KFFTablesAddType (self->tables, NULL, class, type, newline-class, tab-type);
	if (rc != 0)
	    break;
    }

    return rc;
}

rc_t CC KFileFormatInit ( KFileFormat *self, const KFileFormat_vt *vt,
		      const char * typeAndClass, size_t len )
{
    rc_t rc = 0;

    FUNC_ENTRY();

    self->vt = vt;
    atomic32_set (&self->refcount,1);

    rc = KFFTablesMake(&self->tables);
    if (rc == 0)
    {
	rc = KFileFormatInitTypeAndClass (self, typeAndClass, len);


        /* memory leak?  If Tables Make succeeds and InitType and Class fails do we leak? */

    }
    return rc;
}




