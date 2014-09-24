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

#include "copycat-priv.h"

#include <klib/rc.h>
#include <kfs/arc.h>
#include <kfs/sra.h>
#include <kfs/toc.h>
#include <kfs/file.h>
#include <kfs/subfile.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/status.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/debug.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <os-native.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


typedef struct CCNodeSraDir CCNodeSraDir;
#define KDIR_IMPL struct CCNodeSraDir
#include <kfs/impl.h>

/* must be after kfs/impl */
#include "debug.h"

struct CCNodeSraDir
{
    KDirectory dad;
    const KFile * file;
    const char * dir_name;
    const char * sub_name;
    size_t name_size;

};

static bool CCNodeSraDirLegalPath (const CCNodeSraDir * self, const char * path)
{
    if (*path == '/')
        return (strncmp (self->sub_name, path+1, self->name_size + 1) == 0);
    else
        return (strncmp (self->sub_name, path, self->name_size + 1) == 0);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirDestroy
 */
static rc_t CC CCNodeSraDirDestroy (CCNodeSraDir *self)
{
    if (self)
    {
        KFileRelease (self->file);
        free (self);
    }
    return 0;
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirList
 *  create a directory listing
 *
 *  "list" [ OUT ] - return parameter for list object
 *
 *  "path" [ IN, NULL OKAY ] - optional parameter for target
 *  directory. if NULL, interpreted to mean "."
 */
static
rc_t CC CCNodeSraDirList (const CCNodeSraDir *self,
                          KNamelist **listp,
                          bool (CC* f) (const KDirectory *dir, const char *name, void *data),
                          void *data,
                          const char *path,
                          va_list args)
{
    assert (0);

    return 0;
}


/* ----------------------------------------------------------------------
 * CCNodeSraDirVisit
 *  visit each path under designated directory,
 *  recursively if so indicated
 *
 *  "recurse" [ IN ] - if non-zero, recursively visit sub-directories
 *
 *  "f" [ IN ] and "data" [ IN, OPAQUE ] - function to execute
 *  on each path. receives a base directory and relative path
 *  for each entry, where each path is also given the leaf name
 *  for convenience. if "f" returns non-zero, the iteration will
 *  terminate and that value will be returned. NB - "dir" will not
 *  be the same as "self".
 *
 *  "path" [ IN ] - NUL terminated string in directory-native character set
 */
static 
rc_t CC CCNodeSraDirVisit (const CCNodeSraDir *self, 
                           bool recurse,
                           rc_t (CC* f) (const KDirectory *, uint32_t, const char *, void *), 
                           void *data,
                           const char *path,
                           va_list args)
{
    assert (0);

    return 0;
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirVisitUpdate
 */
static rc_t CC CCNodeSraDirVisitUpdate (CCNodeSraDir *self,
                                        bool recurse,
                                        rc_t (CC*f) (KDirectory *,uint32_t,const char *,void *),
                                        void *data,
                                        const char *path,
                                        va_list args)
{
    return RC (rcFS, rcDirectory, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirPathType
 *  returns a KPathType
 *
 *  "path" [ IN ] - NUL terminated string in directory-native character set
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static uint32_t CC CCNodeSraDirPathType (const CCNodeSraDir *self, const char *path, va_list args)
{
    if (CCNodeSraDirLegalPath (self, path))
        return kptFile;
    return kptNotFound;
}


/* ----------------------------------------------------------------------
 * CCNodeSraDirRelativePath
 *  makes "path" relative to "root"
 *  both "root" and "path" MUST be absolute
 *  both "root" and "path" MUST be canonical, i.e. have no "//", "/./" or "/../" sequences
 */
/*
static
rc_t CCNodeSraDirRelativePath (const CCNodeSraDir *self, enum RCContext ctx,
                               const char *root, char *path, size_t path_max)
{
    assert (0);
    return 0;
}
*/

/* ----------------------------------------------------------------------
 * CCNodeSraDirResolvePath
 *
 *  resolves path to an absolute or directory-relative path
 *
 * [IN]  const CCNodeSraDir *self		Objected oriented self
 * [IN]	 bool 		absolute	if non-zero, always give a path starting
 *  					with '/'. NB - if the directory is 
 *					chroot'd, the absolute path
 *					will still be relative to directory root.
 * [OUT] char *		resolved	buffer for NUL terminated result path in 
 *					directory-native character set
 * [IN]	 size_t		rsize		limiting size of resolved buffer
 * [IN]  const char *	path		NUL terminated string in directory-native
 *					character set denoting target path. 
 *					NB - need not exist.
 *
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static rc_t CC CCNodeSraDirResolvePath (const CCNodeSraDir *self,
                                        bool absolute,
                                        char *resolved,
                                        size_t rsize,
                                        const char *path_fmt,
                                        va_list args)
{
    char path[4096];
    int size = args ?
        vsnprintf ( path, sizeof path, path_fmt, args ) :
        snprintf  ( path, sizeof path, "%s", path_fmt );
    if ( size < 0 || size >= (int) sizeof path )
        return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcExcessive );
    if (absolute && (path[0] != '/'))
    {
        string_printf (resolved, rsize, NULL, "/%s", path);
    }       
    else
        string_copy (resolved, rsize, path, self->name_size+1);
    return 0;
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirResolveAlias
 *  resolves an alias path to its immediate target
 *  NB - the resolved path may be yet another alias
 *
 *  "alias" [ IN ] - NUL terminated string in directory-native
 *  character set denoting an object presumed to be an alias.
 *
 *  "resolved" [ OUT ] and "rsize" [ IN ] - buffer for
 *  NUL terminated result path in directory-native character set
 *
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static rc_t CC CCNodeSraDirResolveAlias (const CCNodeSraDir * self, 
                                         bool absolute,
                                         char * resolved,
                                         size_t rsize,
                                         const char *alias,
                                         va_list args)
{
    assert (0);
    return 0;
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirRename
 *  rename an object accessible from directory, replacing
 *  any existing target object of the same type
 *
 *  "from" [ IN ] - NUL terminated string in directory-native
 *  character set denoting existing object
 *
 *  "to" [ IN ] - NUL terminated string in directory-native
 *  character set denoting existing object
 */
static
rc_t CC CCNodeSraDirRename (CCNodeSraDir *self, bool force, const char *from, const char *to)
{
    assert (self != NULL);
    assert (from != NULL);
    assert (to != NULL);

    return RC (rcFS, rcNoTarg, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirRemove
 *  remove an accessible object from its directory
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 *
 *  "force" [ IN ] - if non-zero and target is a directory,
 *  remove recursively
 */
static
rc_t CC CCNodeSraDirRemove (CCNodeSraDir *self, bool force, const char *path, va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirClearDir
 *  remove all directory contents
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 *
 *  "force" [ IN ] - if non-zero and directory entry is a
 *  sub-directory, remove recursively
 */
static
rc_t CC CCNodeSraDirClearDir (CCNodeSraDir *self, bool force, const char *path, va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirAccess
 *  get access to object
 *
 *  "access" [ OUT ] - return parameter for Unix access mode
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static rc_t CC CCNodeSraDirVAccess (const CCNodeSraDir *self,
                                    uint32_t *access,
                                    const char *path,
                                    va_list args)
{
    assert (self != NULL);
    assert (access != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcReading, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirSetAccess
 *  set access to object a la Unix "chmod"
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 *
 *  "access" [ IN ] and "mask" [ IN ] - definition of change
 *  where "access" contains new bit values and "mask defines
 *  which bits should be changed.
 *
 *  "recurse" [ IN ] - if non zero and "path" is a directory,
 *  apply changes recursively.
 */
static rc_t CC CCNodeSraDirSetAccess (CCNodeSraDir *self,
                                      bool recurse,
                                      uint32_t access,
                                      uint32_t mask,
                                      const char *path,
                                      va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcUpdating, rcSelf, rcUnsupported);
}


static	rc_t CC CCNodeSraDirVDate (const CCNodeSraDir *self,
                                   KTime_t *date,
                                   const char *path,
                                   va_list args)
{
    assert (self != NULL);
    assert (date != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcReading, rcSelf, rcUnsupported);
}
static	rc_t CC CCNodeSraDirSetDate		(CCNodeSraDir *self,
                                                 bool recurse,
                                                 KTime_t date,
                                                 const char *path,
                                                 va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcUpdating, rcSelf, rcUnsupported);
}

static
struct KSysDir *CC CCNodeSraDirGetSysDir ( const CCNodeSraDir *self )
{
    return NULL;
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirCreateAlias
 *  creates a path alias according to create mode
 *
 *  "targ" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 *
 *  "alias" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target alias
 *
 *  "access" [ IN ] - standard Unix directory access mode
 *  used when "mode" has kcmParents set and alias path does
 *  not exist.
 *
 *  "mode" [ IN ] - a creation mode (see explanation above).
 */
static
rc_t CC CCNodeSraDirCreateAlias (CCNodeSraDir *self,
                                 uint32_t access,
                                 KCreateMode mode,
                                 const char *targ,
                                 const char *alias)
{
    assert (self != NULL);
    assert (targ != NULL);
    assert (alias != NULL);

    return RC (rcFS, rcNoTarg, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirOpenFileRead
 *  opens an existing file with read-only access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC CCNodeSraDirOpenFileRead	(const CCNodeSraDir *self,
					 const KFile **f,
					 const char *path_fmt,
					 va_list args)
{
    rc_t	rc;
    char path[4096];
    int size;

    assert (self != NULL);
    assert (f != NULL);
    assert (path_fmt != NULL);

    size = args ?
        vsnprintf ( path, sizeof path, path_fmt, args ) :
        snprintf  ( path, sizeof path, "%s", path_fmt );
    if ( size < 0 || size >= (int) sizeof path )
        return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcExcessive );

    if (CCNodeSraDirLegalPath (self, path))
    {
        rc = KFileAddRef (self->file);
        if (rc == 0)
        {
            *f = self->file;
            return 0;
        }
        return rc;
    }
    else
        rc = RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcNotFound);
    *f = NULL;
    return rc;
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirOpenFileWrite
 *  opens an existing file with write access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "update" [ IN ] - if non-zero, open in read/write mode
 *  otherwise, open in write-only mode
 */
static
rc_t CC CCNodeSraDirOpenFileWrite	(CCNodeSraDir *self,
					 KFile **f,
					 bool update,
					 const char *path,
					 va_list args)
{
    assert (self != NULL);
    assert (f != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirCreateFile
 *  opens a file with write access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "access" [ IN ] - standard Unix access mode, e.g. 0664
 *
 *  "update" [ IN ] - if non-zero, open in read/write mode
 *  otherwise, open in write-only mode
 *
 *  "mode" [ IN ] - a creation mode (see explanation above).
 */
static
rc_t CC CCNodeSraDirCreateFile	(CCNodeSraDir *self,
                                 KFile **f,
                                 bool update,
                                 uint32_t access,
                                 KCreateMode cmode,
                                 const char *path,
                                 va_list args)
{
    assert (self != NULL);
    assert (f != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirFileLocator
 *  returns locator in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "locator" [ OUT ] - return parameter for file locator
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC CCNodeSraDirFileLocator		(const CCNodeSraDir *self,
					 uint64_t *locator,
					 const char *path_fmt,
					 va_list args)
{
    char path[4096];
    int size;

    assert (self != NULL);
    assert (locator != NULL);
    assert (path_fmt != NULL);

    size = args ?
        vsnprintf ( path, sizeof path, path_fmt, args ) :
        snprintf  ( path, sizeof path, "%s", path_fmt );
    if ( size < 0 || size >= (int) sizeof path )
        return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcExcessive );

    *locator = 0;       /* undefined for this situation */
    if (CCNodeSraDirLegalPath (self, path))
        return 0;
    return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcNotFound);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirFileSize
 *  returns size in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "size" [ OUT ] - return parameter for file size
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC CCNodeSraDirFileSize		(const CCNodeSraDir *self,
					 uint64_t *size,
					 const char *path_fmt,
					 va_list args)
{
    char path[4096];
    int path_size;

    assert (self != NULL);
    assert (size != NULL);
    assert (path_fmt != NULL);

    path_size = args ?
        vsnprintf ( path, sizeof path, path_fmt, args ) :
        snprintf  ( path, sizeof path, "%s", path_fmt );
    if ( path_size < 0 || path_size >= (int) sizeof path )
        return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcExcessive );

    if (CCNodeSraDirLegalPath (self, path))
        return (KFileSize (self->file, size)); /* we have to assume physical and logical size are the same */

    *size = 0;
    return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcNotFound);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirFileSize
 *  returns size in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "size" [ OUT ] - return parameter for file size
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC CCNodeSraDirFilePhysicalSize (const CCNodeSraDir *self,
                                      uint64_t *size,
                                      const char *path_fmt,
                                      va_list args)
{
    char path[4096];
    int path_size;

    assert (self != NULL);
    assert (size != NULL);
    assert (path_fmt != NULL);

    path_size = args ?
        vsnprintf ( path, sizeof path, path_fmt, args ) :
        snprintf  ( path, sizeof path, "%s", path_fmt );
    if ( path_size < 0 || path_size >= (int) sizeof path )
        return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcExcessive );

    if (CCNodeSraDirLegalPath (self, path))
        return (KFileSize (self->file, size)); /* we have to assume physical and logical size are the same */

    *size = 0;
    return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcNotFound);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirSetFileSize
 *  sets size in bytes of target file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t CC CCNodeSraDirSetFileSize	(CCNodeSraDir *self,
                                 uint64_t size,
                                 const char *path,
                                 va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcWriting, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirOpenDirRead
 *
 *  opens a sub-directory
 *
 * [IN]  const CCNodeSraDir *	self	Object Oriented C CCNodeSraDir self
 * [OUT] const KDirectory **	subp	Where to put the new KDirectory/CCNodeSraDir
 * [IN]  bool			chroot	Create a chroot cage for this new subdirectory
 * [IN]  const char *		path	Path to the directory to open
 * [IN]  va_list		args	So far the only use of args is possible additions to path
 */
static 
rc_t CC CCNodeSraDirOpenDirRead	(const CCNodeSraDir *self,
                                 const KDirectory **subp,
                                 bool chroot,
                                 const char *path,
                                 va_list args)
{
    assert (self != NULL);
    assert (subp != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcReading, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirOpenDirUpdate
 *  opens a sub-directory
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 *
 *  "chroot" [ IN ] - if non-zero, the new directory becomes
 *  chroot'd and will interpret paths beginning with '/'
 *  relative to itself.
 */
static
rc_t CC CCNodeSraDirOpenDirUpdate	(CCNodeSraDir *self,
					 KDirectory ** subp, 
					 bool chroot, 
					 const char *path, 
					 va_list args)
{
    assert (self != NULL);
    assert (subp != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcUpdating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirCreateDir
 *  create a sub-directory
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 *
 *  "access" [ IN ] - standard Unix directory permissions
 *
 *  "mode" [ IN ] - a creation mode (see explanation above).
 */
static
rc_t CC CCNodeSraDirCreateDir	(CCNodeSraDir *self,
                                 uint32_t access,
                                 KCreateMode mode,
                                 const char *path,
                                 va_list args)
{
    assert (self != NULL);
    assert (path != NULL);

    return RC (rcFS, rcNoTarg, rcCreating, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirDestroyFile
 */
static
rc_t CC CCNodeSraDirDestroyFile	(CCNodeSraDir *self,
                                 KFile * f)
{
    assert (self != NULL);
    assert (f != NULL);

    return RC (rcFS, rcNoTarg, rcDestroying, rcSelf, rcUnsupported);
}

/* ----------------------------------------------------------------------
 * CCNodeSraDirFileContiguous
 *  
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 *
 *  "contiguous" [ OUT ] - return parameter for file status
 * NOTE: Does not meet a design target of on stack (localized variable) allocation of single 4kb path
 */
static
rc_t CC CCNodeSraDirFileContiguous (const CCNodeSraDir *self,
                                    bool * contiguous,
                                    const char *path_fmt,
                                    va_list args)
{
    char path[4096];
    int size;

    assert (self);
    assert (contiguous);
    assert (path_fmt);

    size = args ?
        vsnprintf ( path, sizeof path, path_fmt, args ) :
        snprintf  ( path, sizeof path, "%s", path_fmt );
    if ( size < 0 || size >= (int) sizeof path )
        return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcExcessive );

    if (CCNodeSraDirLegalPath (self, path))
    {
        *contiguous = true;
        return 0;
    }
    *contiguous = false;
    return RC (rcFS, rcNoTarg, rcAccessing, rcPath, rcNotFound);
}


static KDirectory_vt_v1 CCNodeSraDir_vt = 
{
    /* version 1.0 */
    1, 3,

    /* start minor version 0 methods*/
    CCNodeSraDirDestroy,
    CCNodeSraDirList,
    CCNodeSraDirVisit,
    CCNodeSraDirVisitUpdate,
    CCNodeSraDirPathType,
    CCNodeSraDirResolvePath,
    CCNodeSraDirResolveAlias,
    CCNodeSraDirRename,
    CCNodeSraDirRemove,
    CCNodeSraDirClearDir,
    CCNodeSraDirVAccess,
    CCNodeSraDirSetAccess,
    CCNodeSraDirCreateAlias,
    CCNodeSraDirOpenFileRead,
    CCNodeSraDirOpenFileWrite,
    CCNodeSraDirCreateFile,
    CCNodeSraDirFileSize,
    CCNodeSraDirSetFileSize,
    CCNodeSraDirOpenDirRead,
    CCNodeSraDirOpenDirUpdate,
    CCNodeSraDirCreateDir,
    CCNodeSraDirDestroyFile,
    /* end minor version 0 methods*/
    /* start minor version 1 methods*/
    CCNodeSraDirVDate,
    CCNodeSraDirSetDate,
    CCNodeSraDirGetSysDir,
    /* end minor version 2 methods*/
    CCNodeSraDirFileLocator,
    /* end minor version 2 methods*/
    /* end minor version 3 methods*/
    CCNodeSraDirFilePhysicalSize,
    CCNodeSraDirFileContiguous
    /* end minor version 3 methods*/
};
static const char root_name[] = "/";
static
rc_t KDirectoryMakeSraNodeDir (const KDirectory ** pself, const KFile * file,
                               const char * name)
{
    CCNodeSraDir * self;
    size_t name_size;
    rc_t rc;

    assert (pself);

    name_size = string_size (name);
    self = malloc (sizeof (*self) + name_size + 1);
    if (self == NULL)
        rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
    else
    {
        rc = KFileAddRef (file);
        if (rc == 0)
        {
            rc_t orc;
            self->file = file;
            self->dir_name = root_name;
            self->sub_name = (const char *)(self+1);
            self->name_size = name_size;
            strcpy ((char*)self->sub_name, name);
            rc = KDirectoryInit (&self->dad, (const KDirectory_vt*)&CCNodeSraDir_vt,
                                 "CCSraNodeDir", root_name, false);
            if (rc == 0)
            {
                *pself = &self->dad;
                return 0;
            }
            orc = KFileRelease (file);
            if (orc)
            {
                PLOGERR (klogErr,
                         (klogErr, orc,
                          "Error releaseing sub file '$(F) in a KAR archive",
                          "F=%s", name));
                if (rc == 0)
                    rc = orc;
            }
        }
    }
    return rc;
}


typedef struct list_item
{
    KPathType      type;
    uint32_t       access;
    uint64_t       size;
    uint64_t       loc;
    KTime_t        mtime;
    char *         path;
    char *         link;

} list_item;

static
void CC list_item_whack (void * item, void * data)
{
    free (item);
}
static
int CC list_item_cmp (const void * item, const void * n)
{
    const list_item * l = item;
    const list_item * r = n;

/* a bit of a hack to get around issue with CCTree
 * it has to have a directory say "dir" inserted before
 * a file within it say dir/file
 */

    /* dirs before others */
    if ((l->type == kptDir) && (r->type != kptDir))
        return -1;
    if ((r->type == kptDir) && (l->type != kptDir))
        return 1;
    
    /* then by location */
    if (l->loc > r->loc)
        return 1;
    if (l->loc < r->loc)
        return -1;

    /* if a file is zero sized, but it before a non-zero sized file */
    if ((l->size == 0) && (r->size > 0))
        return -1;
    if ((r->size == 0) && (l->size > 0))
        return 1;

    /* if type is the same, location is the same and size is the same
     * go alphabetically.  This puts dirs before sub dirs since
     * strcmp says "dir" comes before "dir/dir"
     */
    return (strcmp (l->path, r->path));
}


typedef struct list_adata
{
    Vector list;
    Vector sort;
    bool has_zombies;
} list_adata;


static rc_t list_adata_init (list_adata * self)
{
    VectorInit (&self->list, 0, 512);
    VectorInit (&self->sort, 0, 512);
    self->has_zombies = false;
    return 0;
}


static void list_adata_whack (list_adata * self)
{
    VectorWhack (&self->list, NULL, NULL);
    VectorWhack (&self->sort, list_item_whack, NULL);
}


/* filter will let us add the add to and extract by name things to kar */
static
rc_t step_through_dir (const KDirectory * dir, const char * path,
                       rc_t (*action)(const KDirectory *, const char *, void *),
                       void * adata)
{
    rc_t rc;
    KNamelist * names;

    STSMSG (4, ("step_through_dir %s\n", path));

    rc = KDirectoryList (dir, &names, NULL, NULL, "%s", path);
    if (rc == 0)
    {
        uint32_t limit;
        rc = KNamelistCount (names, &limit);
        if (rc == 0)
        {
            uint32_t idx;
            size_t pathlen;

            pathlen = strlen(path);
            for (idx = 0; (rc == 0) && (idx < limit); idx ++)
            {
                const char * name;
                rc = KNamelistGet (names, idx, &name);
                if (rc == 0)
                {
                    size_t namelen = strlen (name);
                    size_t new_pathlen = pathlen + 1 + namelen;
                    char * new_path = malloc (new_pathlen + 1);

                    if (new_path != NULL)
                    {
                        char * recur_path;
                        if (pathlen == 0)
                        {
                            memcpy (new_path, name, namelen);
                            new_path[namelen] = '\0';
                        }
                        else
                        {
                            memcpy (new_path, path, pathlen);
                            new_path[pathlen] = '/';
                            memcpy (new_path + pathlen + 1, name, namelen);
                            new_path[pathlen+1+namelen] = '\0';
                        }
                        recur_path = malloc (pathlen + 1 + namelen + 1);
                        if (recur_path != NULL)
                        {
                            rc = KDirectoryResolvePath (dir, false, recur_path,
                                                         pathlen + 1 + namelen + 1,
                                                         "%s", new_path);

                            if (rc == 0)
                                rc = action (dir, recur_path, adata);

                            free (recur_path);
                        }
                        free (new_path);
                    }
                }
            }
        }
        KNamelistRelease (names);
    }
    return rc;
}
static
rc_t list_action (const KDirectory * dir, const char * path, void * _adata)
{
    rc_t           rc = 0;
    list_adata *   data = _adata;
    list_item *    item = NULL;
    KPathType      type  = KDirectoryPathType (dir, "%s", path);
    size_t         pathlen = strlen (path);
    size_t         linklen = 0;
    char           link [2 * 4096]; /* we'll truncate? */

    if (type & kptAlias)
    {
        rc = KDirectoryVResolveAlias (dir, false, link, sizeof (link),
                                      path, NULL);
        if (rc == 0)
            linklen = strlen (link);
    }

    if (rc == 0)
    {
        item = calloc (sizeof (*item) + pathlen + linklen + 2, 1); /* usually one too many */
        if (item == NULL)
        {
            rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
        }
        else
        {
            do
            {
                item->path = (char *)(item+1);
                strcpy (item->path, path);
                item->type = type;
                rc = KDirectoryAccess (dir, &item->access, "%s", path);
                if (rc) break;

                rc = KDirectoryDate (dir, &item->mtime, "%s", path);
                if (rc) break;

                if (type & kptAlias)
                {
                    item->link = item->path + pathlen + 1;
                    strcpy (item->link, link);
                }
                else switch (type & ~kptAlias)
                {
                case kptNotFound:
                    rc = RC (rcExe, rcDirectory, rcAccessing, rcPath, rcNotFound);
                    break;
                case kptBadPath:
                    rc = RC (rcExe, rcDirectory, rcAccessing, rcPath, rcInvalid);
                    break;
                case kptZombieFile:
                    data->has_zombies = true;
                case kptFile:
                    rc = KDirectoryFileSize (dir, &item->size, "%s", path);
                    if (rc == 0)
                        rc = KDirectoryFileLocator (dir, &item->loc, "%s", path);
                    DBGMSG (DBG_APP, 1, ("%s: found file %s size %lu at %lu\n",
                                         __func__, item->path, item->size, item->loc));
                    break;
                case kptDir:
                    DBGMSG (DBG_APP, 1, ("%s: found directory %s\n",
                                         __func__, item->path));
                    break;
                default:
                    DBGMSG (DBG_APP, 1, ("%s: found unknown %s\n",
                                         __func__, item->path));
                    break;
                }
            } while (0);
        }
    }
    if (rc == 0)
    {
        VectorAppend (&data->list, NULL, item);
        VectorInsert (&data->sort, item, NULL, list_item_cmp);

        if (type == kptDir)
            rc = step_through_dir (dir, path, list_action, data);
    }
    return rc;
}


typedef struct CCSra
{
    CCTree * tree;
    const KDirectory * ndir;
    const KDirectory * adir;
    const KFile * file;
} CCSra;


static
rc_t CCSraInit (CCSra ** pself, CCTree * tree, const KFile * sf, const char * name)
{
    CCSra * self;
    rc_t rc;

    assert (pself);
    assert (sf);
    assert (name);

    self = malloc (sizeof (*self));
    if (self == NULL)
        rc = RC (rcExe, rcMemory, rcAllocating, rcMemory, rcExhausted);
    else
    {
        KFileAddRef (self->file = sf);

        rc = KDirectoryMakeSraNodeDir (&self->ndir, sf, name);
        if (rc == 0)
        {
            rc = KDirectoryOpenSraArchiveReadUnbounded (self->ndir, &self->adir, true, "%s", name);
            if (rc == 0)
            {
                self->tree = tree;
                *pself = self;
                return 0;
            }
            KDirectoryRelease (self->ndir);
        }
        free (self);
    }
    return rc; /* error out */
}

static
bool CC CCSraOneItem (void * item_, void * data_)
{
    CCSra * self = data_;
    list_item * item = item_;
    rc_t rc;

    DBGMSG (DBG_APP, 1, ("%s: %s\n", __func__, item->path));


    switch (item->type)
    {
    default:
        DBGMSG (DBG_APP, 1, ("%s: item->type not processed (%d)\n", __func__, item->type));
        rc = 0;
        break;
    case kptFile:
    {
        CCArcFileNode * node;
        rc = CCArcFileNodeMake (&node, item->loc, item->size);
        if (rc == 0)
        {
            const KFile * sfile;
            rc = KFileMakeSubRead (&sfile, self->file, item->loc, item->size);
            if (rc == 0)
            {
                void * save;

                copycat_log_set (&node->dad.logs, &save);

                rc = ccat_md5 (self->tree, sfile, item->mtime, ccArcFile, &node->dad,
                               item->path);

                copycat_log_set (save, NULL);

                KFileRelease (sfile);
            }
        }
        DBGMSG (DBG_APP, 1, ("%s: kptFile processed %lu at %lu\n", __func__, item->size, item->loc));
        break;
    }
    case kptDir:
    {
        CCTree * node;
        rc = CCTreeMake (&node);
        if (rc == 0)
        {
            rc = CCTreeInsert (self->tree, item->mtime, ccDirectory, node, item->path);
            DBGMSG (DBG_APP, 1, ("%s: insert directory %s\n", __func__, item->path));
        }
        break;
    }
    }
    DBGMSG (DBG_APP, 1, ("%s: exiting rc (%R) (%d)\n", __func__, rc, (rc !=0)));
    return (rc != 0);
}

static
void CCSraWhack (CCSra * self)
{
    rc_t rc, orc;
    rc = KDirectoryRelease (self->ndir);
    orc = KDirectoryRelease (self->adir);
    if (rc == 0)
        rc = orc;
    orc = KFileRelease (self->file);
    if (rc == 0)
        rc = orc;
    if (rc)
        LOGERR(klogWarn, rc, "error releaseing CCSra");
    free (self);
}
rc_t ccat_sra ( CCContainerNode *np, const KFile *sf, const char *name )
{
    rc_t rc;
    CCSra * sra;

    rc = CCSraInit (&sra, &np->sub, sf, name);

    if (rc == 0)
    {
        list_adata ldata;

        list_adata_init (&ldata);
        rc = step_through_dir (sra->adir, ".", list_action, &ldata);
        if (rc == 0)
        {
            DBGMSG (DBG_APP, 1, ("Vector sizes list (%u) sort (%u)\n", 
                                 VectorLength(&ldata.list),
                                 VectorLength(&ldata.sort)));
            VectorDoUntil (&ldata.sort, false, CCSraOneItem, sra);
/*             VectorDoUntil (&ldata.list, false, CCSraOneItem, sra); */
        }
        list_adata_whack (&ldata);
        CCSraWhack (sra);
    }
    DBGMSG (DBG_APP, 1, ("Done with %s\n", name));
    return rc;
}

