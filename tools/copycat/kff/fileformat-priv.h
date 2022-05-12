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

#ifndef _h_fileformat_priv_
#define _h_fileformat_priv_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

#include <klib/debug.h>

#define	DEBUG_KFF	1

#ifndef	DEBUG_KFF
#define	DEBUG_KFF	0
#endif

#ifdef _DEBUGGING
#define FUNC_ENTRY() /* DBGMSG (DBG_KFS, DBG_FLAG(DBG_KFS_KFFENTRY), ("Enter: %s\n", __func__)) */
#define KFF_DEBUG(msg) DBGMSG (DBG_KFS, DBG_FLAG(DBG_KFS_KFF), msg)
#else
#define FUNC_ENTRY()
#define KFF_DEBUG(msg)
#endif

#define KFILEFORMAT_LATEST 1

#define DESCRLEN_MAX	(256)

typedef struct KFFTables KFFTables;
/* -----
 * KFFTablesMake
 *	Build the tables that contain classes and types.
 *
 * This make installs only a default "unknown" description which
 * will end up with class id and file id of 0.  This FileId 0 will
 * be in class with id of 0.
 */
rc_t KFFTablesMake (KFFTables ** kmmtp);

/* -----
 * KFFTablesAddClass
 *	Add class with description (descr) to the tables
 *	new class id is returned to where pclass points.
 *	if pclass is NULL the new ID is not returned
 */
rc_t KFFTablesAddClass (KFFTables * self,
			KFileFormatClass * pclass, /* returned new ID */
			const char * descr,
			size_t descrlen);

/* -----
 * KFFTablesAddType
 *	Add type with description (descr) to the tables
 *	new type id is returned to where ptype points.
 *	if ptype is NULL the new ID is not returned
 *	the new type will be in the refered class
 */
rc_t KFFTablesAddType (KFFTables * self,
		       KFileFormatType * ptype, /* returned new ID */
		       const char * class,
		       const char * type,
		       size_t clen,
		       size_t tlen);

rc_t KFFTablesAddRef (const KFFTables * self);
rc_t KFFTablesRelease (const KFFTables * cself);

rc_t KFFTablesGetClassDescr (const KFFTables * self,
			     KFileFormatClass tid,
			     size_t * len,
			     char ** pd);
rc_t KFFTablesGetTypeDescr (const KFFTables * self,
			    KFileFormatType tid,
			    size_t * len,
			    char ** pd);
rc_t KFFTablesGetClassId (const KFFTables * self,
			  const char ** pd,
			  KFileFormatClass * cid);
rc_t KFFTablesGetTypeId (const KFFTables * self,
			 const char * pd,
			 KFileFormatType * tid,
			 KFileFormatClass * cid);









#ifdef __cplusplus
}
#endif

#endif /* _h_fileformat_priv_ */
