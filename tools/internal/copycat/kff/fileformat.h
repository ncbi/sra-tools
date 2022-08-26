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

#ifndef _h_kfs_fileformat_
#define _h_kfs_fileformat_

#ifndef _h_kfs_extern_
#include <kfs/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct KDirectory;

/*
 * Multiple descriptions can be attached to a single key.
 * At most one key can be attached to a single description.
 *
 * Multiple types can be attached to a single class.
 * At most one class can be attached to a single type.
 */


/*--------------------------------------------------------------------------
 * KFileFormatType
 *  describes basic file content types.  The type can be used to choose what
 *  parser is used to extract information for a loader, archive as filesystem,
 *  or decompress for further parsing.
 */
typedef
int32_t KFileFormatType;
enum KFileFormatType_e
{
    kfftError = -2,		/* A file that can not be read for typing */
    kfftNotFound = -1,		/* not found in a search */
    kfftUnknown = 0		/* not yet or file format not understood. */
    /* other types are registered during construction **?** */
};

/* -------------------------------------------------------------------------
 * KFileFormatClass
 *   Describes which class of operations can be performed in a given file.
 *     Unknown: nothing in particular
 *     Compressed: decompressed to reveal different expanded file contents
 *     Archive: treated as a file system to reach contained files
 *     Run: loaded into the SRA DB
 */
typedef
int32_t KFileFormatClass;
enum KFileFormatClass_e
{
    kffcError = -2,		/* A file that can not be read for typing */
    kffcNotFound = -1,		/* not found in a search */
    kffcUnknown = 0		/* not yet or file format not understood. */
};

/*--------------------------------------------------------------------------
 * KFileFormat
 */
typedef struct KFileFormat KFileFormat;

/* AddRef
 * Release
 *  ignores NULL references
 */
KFS_EXTERN rc_t CC KFileFormatAddRef (const KFileFormat *self);
KFS_EXTERN rc_t CC KFileFormatRelease (const KFileFormat *self);

/* Type
 *  intended to be a content type,
 *  if type, class or desc is NULL those types are not returned
 */
KFS_EXTERN rc_t CC KFileFormatGetTypeBuff (const KFileFormat *self, const void * buff, size_t buff_len,
			KFileFormatType * type, KFileFormatClass * class,
			char * description, size_t descriptionmax,
			size_t * length);

#define KFileFormatGetTypeBuffType(self,buff,buff_len,type) \
    KFileFormatGetTypeBuff(self,buff,buff_len,type,NULL,NULL,0,NULL)

#define KFileFormatGetTypeBuffClass(self,buff,buff_len,class) \
    KFileFormatGetTypeBuff(self,buff,buff_len,NULL,class,NULL,0,NULL)

/* useful for logging perhaps */
#define KFileFormatGetTypeBuffDescr(self,buff,buff_len,descr,descr_max,descr_len)	\
    KFileFormatGetTypeBuff(self,buff,buff_len,NULL,NULL,descr,descr_max,descr_len)

KFS_EXTERN rc_t CC KFileFormatGetTypePath(const KFileFormat *self, const struct KDirectory * dir,
                                          const char * path, KFileFormatType * type,
                                          KFileFormatClass * class, char * description,
                                          size_t descriptionmax, size_t * length);
#define KFileFormatGetTypePathType(self,dir,path,type)			\
    KFileFormatGetTypePath(self,dir,path,type,NULL,NULL,0,NULL)

#define KFileFormatGetTypePathClass(self,dir,path,class)			\
    KFileFormatGetTypePath(self,dir,path,NULL,class,NULL,0,NULL)

/* useful for logging perhaps */
#define KFileFormatGetTypePathDescr(self,dir,path,descr,descr_max,descr_len) \
    KFileFormatGetTypePath(self,dir,path,NULL,NULL,descr,descr_max,descr_len)


KFS_EXTERN rc_t CC KFileFormatGetClassDescr (const KFileFormat *self, KFileFormatClass c,
			char * description, size_t descriptionmax);


#ifdef __cplusplus
}
#endif

#endif /* _h_kfs_fileformat_ */
