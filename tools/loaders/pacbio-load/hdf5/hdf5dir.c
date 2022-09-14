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

typedef struct HDF5Dir HDF5Dir;
#define KDIR_IMPL HDF5Dir

#define H5Gopen_vers 2
#define H5Eset_auto_vers 2

#include <kfs/extern.h> /* may need to be changed */
#include <kfs/impl.h>
#include <hdf5.h>
#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/out.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdarg.h>


#if 0 /* These directives merely serve to limit portability. */
#define _XOPEN_SOURCE 600
#include <fcntl.h>
#include <linux/fadvise.h>
#include <dirent.h>
#endif

typedef struct HDF5File HDF5File;

/* import of HDF5FileMake from hdf5file.c */
rc_t CC HDF5FileMake ( HDF5File **fp, hid_t dataset_handle,
        bool read_enabled, bool write_enabled );

/* object structure */
struct HDF5Dir
{
    KDirectory dad;

    /* extend here */
    KDirectory *parent;
    hid_t hdf5_handle;
    hid_t file_access_property_list;
    bool h5root;

    uint32_t root;
    uint32_t size;
    char path [ PATH_MAX ];
};


/* KSysDirMake
 *  allocate an uninialized object
 */
static
HDF5Dir * HDF5DirMake ( size_t path_size )
{
    HDF5Dir *dir = malloc ( ( sizeof * dir - sizeof dir -> path + 2 ) + path_size );
    return dir;
}


/* forward decl. because the real function has to be placed after the vt */
static
rc_t HDF5DirInit ( HDF5Dir *self, enum RCContext ctx, uint32_t dad_root,
    const char *path, uint32_t path_size, bool update, bool chroot );

static
uint32_t CC HDF5DirPathType ( const HDF5Dir *self,
    const char *path, va_list args );


/* Destroy
 */
static
rc_t CC HDF5DirDestroy ( HDF5Dir *self )
{
    KDirectoryRelease ( ( KDirectory* ) self -> parent );
    if ( self->h5root )
    {
        H5Fclose( self->hdf5_handle );
        H5Pclose( self->file_access_property_list );
    }
    else
        H5Oclose( self->hdf5_handle );
    free( self );
    return 0;
}


/* callback context to be passed to the hdf5-callback-function */
struct dir_list_cb_ctx
{
    VNamelist *groups;
    const HDF5Dir *dir;
    bool ( CC * f ) ( const KDirectory *dir, const char *name, void *data );
    void * data;
};


/* callback function to be passed into "H5Literate" */
static
herr_t dir_list_cb( hid_t loc_id, const char * name,
                    const H5L_info_t * linfo, void * data )
{
    struct dir_list_cb_ctx * ctx = ( struct dir_list_cb_ctx * )data;

    /* avoid compiler warnings */
    loc_id = loc_id;
    linfo = linfo;

    if ( ctx != NULL && ctx->groups != NULL )
    {
        if ( ctx->f != NULL )
        {
            if ( ctx->f( &(ctx->dir->dad), name, ctx->data ) )
                VNamelistAppend ( ctx->groups, name );
        }
        else
            VNamelistAppend ( ctx->groups, name );
    }
    return 0;
}

/* List
 *  create a directory listing
 *
 *  "list" [ OUT ] - return parameter for list object
 *
 *  "f" [ IN, NULL OKAY ] and "data" [ IN, OPAQUE ] - optional
 *  filter function to execute on each path. receives a base directory
 *  and relative path for each entry. if "f" returns true, the name will
 *  be added to the list.
 *
 *  "path" [ IN, NULL OKAY ] - optional parameter for target
 *  directory. if NULL, interpreted to mean "."
 */
static
rc_t CC HDF5DirList ( const HDF5Dir *self, struct KNamelist **list,
    bool ( CC * f ) ( const KDirectory *dir, const char *name, void *data ),
    void *data, const char *path, va_list args )
{
    rc_t rc;
    if ( path[0] == '.' && path[1] == 0 )
    {
        struct dir_list_cb_ctx ctx;
        rc = VNamelistMake ( &ctx.groups, 5 );
        if ( rc == 0 )
        {
            ctx.dir = self;
            ctx.f = f;
            ctx.data = data;
            H5Literate( self->hdf5_handle, H5_INDEX_NAME, H5_ITER_INC,
                        NULL, dir_list_cb, &ctx );
            rc = VNamelistToNamelist ( ctx.groups, list );
            VNamelistRelease ( ctx.groups );
        }
    }
    else
    {
        const KDirectory *sub;
        rc = KDirectoryVOpenDirRead ( &self->dad, &sub, false, path, args );
        if ( rc == 0 )
        {
            rc = KDirectoryList( sub, list, f, data, "." );
            KDirectoryRelease( sub );
        }
    }
    return rc;
}


static
rc_t HDF5DirVisitcb( const HDF5Dir *self,
    rc_t ( CC * f ) ( const KDirectory *dir, uint32_t type, const char *name, void *data ),
    const uint32_t type, const char * name, void * data )
{
    int status;
    char buffer[ PATH_MAX ];

    if ( self->h5root )
        status = snprintf ( buffer, sizeof buffer, "/%s", name );
    else
        status = snprintf ( buffer, sizeof buffer, "/%s%s", self->path, name );

    if ( status < 0 || ( size_t ) status >= sizeof buffer )
        return RC ( rcFS, rcDirectory, rcVisiting, rcPath, rcExcessive );
    
    return f( &self->dad, type, buffer, data );
}

/* Visit
 *  visit each path under designated directory,
 *  recursively if so indicated
 *
 *  "recurse" [ IN ] - if true, recursively visit sub-directories
 *
 *  "f" [ IN ] and "data" [ IN, OPAQUE ] - function to execute
 *  on each path. receives a base directory and relative path
 *  for each entry. if "f" returns true, the iteration will
 *  terminate and that value will be returned. NB - "dir" will not
 *  be the same as "self".
 *
 *  "path" [ IN ] - NUL terminated string in directory-native character set
 *
 * VisitFull hits all files types that including those are normally hidden
 */
static
rc_t CC HDF5DirVisit ( const HDF5Dir *self, bool recurse,
    rc_t ( CC * f ) ( const KDirectory *dir, uint32_t type, const char *name, void *data ),
    void *data, const char *path, va_list args )
{
    KNamelist *objects;
    rc_t rc = HDF5DirList ( self, &objects, NULL, NULL, path, args );
    if ( rc == 0 )
    {
        uint32_t idx, count;
        rc = KNamelistCount ( objects, &count );
        for ( idx = 0; idx < count && rc == 0; ++idx )
        {
            const char * name;
            rc = KNamelistGet ( objects, idx, &name );
            if ( rc == 0 )
            {
                uint32_t type = KDirectoryPathType ( &self->dad, "%s", name );
                switch ( type )
                {
                case kptDataset  :
                case kptDatatype :
                case kptDir :
                    rc = HDF5DirVisitcb( self, f, type, name, data );
                    break;
                }
                if ( recurse && type == kptDir )
                {
                    const KDirectory *sub;
                    rc = KDirectoryOpenDirRead ( &self->dad, &sub, false, "%s", name );
                    if ( rc == 0 )
                    {
                        KDirectoryVisit( sub, true, f, "%s", data );
                        KDirectoryRelease( sub );
                    }
                }
            }
        }
        KNamelistRelease( objects );
    }
    return rc;
}


/* VisitUpdate
 *  like Visit except that the directory passed back to "f"
 *  is available for update operations
 */
static
rc_t CC HDF5DirVisitUpdate ( HDF5Dir *self, bool recurse,
    rc_t ( CC * f ) ( KDirectory *dir, uint32_t type, const char *name, void *data ),
    void *data, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}


static uint32_t HDF5DirPathTypeOnBuffer( const HDF5Dir *self, const char *buffer )
{
    H5O_info_t obj_info;
    herr_t h5e = H5Oget_info_by_name( self->hdf5_handle, buffer, &obj_info, H5P_DEFAULT );
    if ( h5e >= 0 )
    {
        switch( obj_info.type )
        {
        case H5O_TYPE_GROUP : return kptDir; break;
        case H5O_TYPE_DATASET : return kptDataset; break;
        case H5O_TYPE_NAMED_DATATYPE : return kptDatatype; break;
        default : return kptBadPath; break;
        }
    }
    else
        return kptBadPath;
}

/* PathType
 *  returns a KPathType
 *
 *  "path" [ IN ] - NUL terminated string in directory-native character set
 */
static uint32_t CC HDF5DirPathType ( const HDF5Dir *self, const char *path, va_list args )
{
    char buffer[ PATH_MAX ];
    rc_t rc = string_vprintf ( buffer, sizeof buffer, NULL, path, args );
    if ( rc != 0 )
        return kptBadPath;

    return HDF5DirPathTypeOnBuffer( self, buffer );
}

/* ResolvePath
 *  resolves path to an absolute or directory-relative path
 *
 *  "absolute" [ IN ] - if true, always give a path starting
 *  with '/'. NB - if the directory is chroot'd, the absolute path
 *  will still be relative to directory root.
 *
 *  "resolved" [ OUT ] and "rsize" [ IN ] - buffer for
 *  NUL terminated result path in directory-native character set
 *  the resolved path will be directory relative
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target path. NB - need not exist.
 */
static
rc_t CC HDF5DirResolvePath ( const HDF5Dir *self, bool absolute,
    char *resolved, size_t rsize, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* ResolveAlias
 *  resolves an alias path to its immediate target
 *  NB - the resolved path may be yet another alias
 *
 *  "absolute" [ IN ] - if true, always give a path starting
 *  with '/'. NB - if the directory is chroot'd, the absolute path
 *  will still be relative to directory root.
 *
 *  "resolved" [ OUT ] and "rsize" [ IN ] - buffer for
 *  NUL terminated result path in directory-native character set
 *  the resolved path will be directory relative
 *
 *  "alias" [ IN ] - NUL terminated string in directory-native
 *  character set denoting an object presumed to be an alias.
 */
static
rc_t CC HDF5DirResolveAlias ( const HDF5Dir *self, bool absolute,
    char *resolved, size_t rsize, const char *alias, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* Rename
 *  rename an object accessible from directory, replacing
 *  any existing target object of the same type
 *
 *  "from" [ IN ] - NUL terminated string in directory-native
 *  character set denoting existing object
 *
 *  "to" [ IN ] - NUL terminated string in directory-native
 *  character set denoting existing object
 *
 *  "force" [ IN ] - not false means try to do more if it fails internally
 */
static
rc_t CC HDF5DirRename ( HDF5Dir *self, bool force, const char *from, const char *to )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* Remove
 *  remove an accessible object from its directory
 *
 *  "force" [ IN ] - if true and target is a directory,
 *  remove recursively
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 */
static
rc_t CC HDF5DirRemove ( HDF5Dir *self, bool force,
    const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* ClearDir
 *  remove all directory contents
 *
 *  "force" [ IN ] - if true and directory entry is a
 *  sub-directory, remove recursively
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 */
static
rc_t CC HDF5DirClearDir ( HDF5Dir *self, bool force,
    const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}


static
rc_t HDF5DirVAccess ( const HDF5Dir *self,
    uint32_t *access, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}


/* SetAccess
 *  set access to object a la Unix "chmod"
 *
 *  "recurse" [ IN ] - if non zero and "path" is a directory,
 *  apply changes recursively.
 *
 *  "access" [ IN ] and "mask" [ IN ] - definition of change
 *  where "access" contains new bit values and "mask defines
 *  which bits should be changed.
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 */
static
rc_t CC HDF5DirSetAccess ( HDF5Dir *self, bool recurse,
    uint32_t access, uint32_t mask, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}


static
rc_t HDF5DirVDate ( const HDF5Dir *self,
    KTime_t * date, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}


static
rc_t HDF5DirVSetDate ( HDF5Dir * self, bool recurse,
    KTime_t date, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}


static
struct KSysDir * HDF5DirGetSysdir ( const HDF5Dir *cself )
{
    return NULL;
}


/* CreateAlias
 *  creates a path alias according to create mode
 *
 *  "access" [ IN ] - standard Unix directory access mode
 *  used when "mode" has kcmParents set and alias path does
 *  not exist.
 *
 *  "mode" [ IN ] - a creation mode ( see explanation above ).
 *
 *  "targ" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target object
 *
 *  "alias" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target alias
 */
static
rc_t CC HDF5DirCreateAlias ( HDF5Dir *self,
    uint32_t access, KCreateMode mode,
    const char *targ, const char *alias )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* OpenFileRead
 *  opens an existing file with read-only access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirOpenFileRead ( const HDF5Dir *self,
    struct KFile const **f, const char *path, va_list args )
{
    char buffer[ PATH_MAX ];
    uint32_t path_type;
    size_t psize;
    hid_t dataset_handle;
    rc_t rc;
    HDF5File *f5p;

    *f = NULL;

    rc = string_vprintf ( buffer, sizeof buffer, &psize, path, args );
    if ( rc != 0 )
        return rc;

    path_type = HDF5DirPathTypeOnBuffer( self, buffer );
    if ( path_type != kptDataset )
        return RC( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );

    dataset_handle = H5Dopen2( self->hdf5_handle, buffer, H5P_DEFAULT );
    if ( dataset_handle < 0 )
        return RC( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );

    rc = HDF5FileMake ( &f5p, dataset_handle, true, false );
    if ( rc == 0 )
        *f = ( struct KFile const * )f5p;

    return rc;
}

/* OpenFileWrite
 *  opens an existing file with write access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "update" [ IN ] - if true, open in read/write mode
 *  otherwise, open in write-only mode
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirOpenFileWrite ( HDF5Dir *self,
    struct KFile **f, bool update, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* CreateFile
 *  opens a file with write access
 *
 *  "f" [ OUT ] - return parameter for newly opened file
 *
 *  "update" [ IN ] - if true, open in read/write mode
 *  otherwise, open in write-only mode
 *
 *  "access" [ IN ] - standard Unix access mode, e.g. 0664
 *
 *  "mode" [ IN ] - a creation mode ( see explanation above ).
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirCreateFile ( HDF5Dir *self, struct KFile **f,
    bool update, uint32_t access, KCreateMode mode, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* FileSize
 *  returns size in bytes of target file
 *
 *  "size" [ OUT ] - return parameter for file size
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirFileSize ( const HDF5Dir *self,
    uint64_t *size, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* FilePhysicalSize
 *  returns physical allocated size in bytes of target file.  It might
 * or might not differ form FileSize
 *
 *  "size" [ OUT ] - return parameter for file size
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirFilePhysicalSize ( const HDF5Dir *self,
    uint64_t *size, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* SetFileSize
 *  sets size in bytes of target file
 *
 *  "size" [ IN ] - new file size
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirSetFileSize ( HDF5Dir *self,
    uint64_t size, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* FileLocator
 *  returns a 64-bit key pretinent only to the particular file
 *  system device holding tha file.
 *
 *  It can be used as a form of sort key except that it is not 
 *  guaranteed to be unique.
 *
 *  "locator" [ OUT ] - return parameter for file locator
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirFileLocator ( const HDF5Dir *self,
    uint64_t *locator, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* FileContiguous
 *  returns size in bytes of target file
 *
 *  "size" [ OUT ] - return parameter for file size
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target file
 */
static
rc_t CC HDF5DirFileContiguous ( const HDF5Dir *self,
    bool *contiguous, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* OpenDirRead
 * OpenDirUpdate
 *  opens a sub-directory
 *
 *  "chroot" [ IN ] - if true, the new directory becomes
 *  chroot'd and will interpret paths beginning with '/'
 *  relative to itself.
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 */
static
rc_t CC HDF5DirOpenDirRead ( const HDF5Dir *self,
    const KDirectory **sub, bool chroot, const char *path, va_list args )
{
    rc_t rc;
    char buffer[ PATH_MAX ];
    size_t buffer_size;
    uint32_t type;
    HDF5Dir * new_dir;

    *sub = NULL;

    rc = string_vprintf ( buffer, sizeof buffer, &buffer_size, path, args );
    if ( rc != 0 )
        return rc;

    type = HDF5DirPathTypeOnBuffer( self, buffer );
    switch( type )
    {
    case kptNotFound :
        return RC ( rcFS, rcDirectory, rcOpening, rcPath, rcNotFound );
    case kptBadPath :
        return RC ( rcFS, rcDirectory, rcOpening, rcPath, rcInvalid );
    case kptDir : break;
    default :
        return RC ( rcFS, rcDirectory, rcOpening, rcPath, rcIncorrect );
    }

    new_dir = HDF5DirMake ( buffer_size );
    if ( new_dir == NULL )
        return RC ( rcFS, rcDirectory, rcOpening, rcMemory, rcExhausted );

                    /* self, RCContext, dad_root, path, path_size, update, chroot */
    rc = HDF5DirInit ( new_dir, rcOpening, 0, buffer, buffer_size, false, false );
    if ( rc == 0 )
    {
        new_dir -> hdf5_handle = H5Gopen( self -> hdf5_handle, buffer, H5P_DEFAULT );
        if ( new_dir -> hdf5_handle >= 0 )
        {
            new_dir -> parent = (KDirectory *)&self->dad;
            new_dir -> h5root = false;
            KDirectoryAddRef ( &self->dad );
            * sub = & new_dir -> dad;
            return 0;
        }
        else
            rc = RC( rcFS, rcDirectory, rcOpening, rcItem, rcInvalid );
    }
    free( new_dir );
    return rc;
}

static
rc_t CC HDF5DirOpenDirUpdate ( HDF5Dir *self,
    KDirectory **sub, bool chroot, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}

/* CreateDir
 *  create a sub-directory
 *
 *  "access" [ IN ] - standard Unix directory mode, e.g.0775
 *
 *  "mode" [ IN ] - a creation mode ( see explanation above ).
 *
 *  "path" [ IN ] - NUL terminated string in directory-native
 *  character set denoting target directory
 */
static
rc_t CC HDF5DirCreateDir ( HDF5Dir *self,
    uint32_t access, KCreateMode mode, const char *path, va_list args )
{
    return RC ( rcFS, rcDirectory, rcAccessing, rcInterface, rcUnsupported );
}


/* vtable & factory */
static struct KDirectory_vt_v1 HDF5Dir_vt =
{
    /* interface version */
    1, 3,

    HDF5DirDestroy,
    HDF5DirList,
    HDF5DirVisit,
    HDF5DirVisitUpdate,
    HDF5DirPathType,
    HDF5DirResolvePath,
    HDF5DirResolveAlias,
    HDF5DirRename,
    HDF5DirRemove,
    HDF5DirClearDir,
    HDF5DirVAccess,
    HDF5DirSetAccess,
    HDF5DirCreateAlias,
    HDF5DirOpenFileRead,
    HDF5DirOpenFileWrite,
    HDF5DirCreateFile,
    HDF5DirFileSize,
    HDF5DirSetFileSize,
    HDF5DirOpenDirRead,
    HDF5DirOpenDirUpdate,
    HDF5DirCreateDir,
    NULL, /* we don't track files*/
    /* end minor version 0 methods*/

    /* start minor version 1 methods*/
    HDF5DirVDate,
    HDF5DirVSetDate,
    HDF5DirGetSysdir,
    /* end minor version 1 methods*/

    /* start minor version 2 methods */
    HDF5DirFileLocator,
    HDF5DirFilePhysicalSize,
    HDF5DirFileContiguous,
    /* end minor version 2 methods */

    /* start minor version 3 methods */
    /*
    HDF5DirOpenDatasetRead,
    HDF5DirOpenDatasetUpdate
    */
};


static
rc_t HDF5DirInit ( HDF5Dir *self, enum RCContext ctx, uint32_t dad_root,
    const char *path, uint32_t path_size, bool update, bool chroot )
{
    rc_t rc = KDirectoryInit ( & self -> dad, ( const KDirectory_vt* ) & HDF5Dir_vt,
                          "HDF5Dir", path ? path : "(null)", update );
    if ( rc != 0 )
        return ResetRCContext ( rc, rcFS, rcDirectory, ctx );

    if ( path != NULL )
        memmove ( self -> path, path, path_size );
    self -> root = chroot ? path_size : dad_root;
    self -> size = path_size + 1;
    self -> path [ path_size ] = '/';
    self -> path [ path_size + 1 ] = 0;

    return 0;
}


/* MakeHDF5RootDir
    - takes a KDirectory and a path
    - if the path points to a hdf5-file, the object will be created
 */
LIB_EXPORT rc_t CC MakeHDF5RootDir ( KDirectory * self, KDirectory ** hdf5_dir,
                                     bool absolute, const char *path )
{
    rc_t rc;
    char resolved[ PATH_MAX ];
    uint32_t path_type;
    HDF5Dir * new_dir;
    size_t path_size;

    if ( self == NULL )
        return RC ( rcFS, rcDirectory, rcConstructing, rcSelf, rcNull );
    if ( hdf5_dir == NULL )
        return RC ( rcFS, rcDirectory, rcConstructing, rcParam, rcNull );
    * hdf5_dir = NULL;

    rc = KDirectoryResolvePath ( self, absolute, resolved, sizeof resolved, "%s", path );
    if ( rc != 0 ) return rc;
    path_size = string_size( resolved );

    path_type = ( KDirectoryPathType ( self, "%s", resolved ) & ~ kptAlias );
    switch ( path_type )
    {
    case kptNotFound :
        return RC ( rcFS, rcDirectory, rcConstructing, rcPath, rcNotFound );
    case kptBadPath :
        return RC ( rcFS, rcDirectory, rcConstructing, rcPath, rcInvalid );
    case kptFile : break;
    default :
        return RC ( rcFS, rcDirectory, rcConstructing, rcPath, rcIncorrect );
    }

    /* mute the error stack */
    H5Eset_auto( H5E_DEFAULT, NULL, NULL );

    new_dir = HDF5DirMake ( path_size );
    if ( new_dir == NULL )
        return RC ( rcFS, rcDirectory, rcConstructing, rcMemory, rcExhausted );

                     /* self, RCContext, dad_root, path, path_size, update, chroot */
    rc = HDF5DirInit ( new_dir, rcAccessing, 0, resolved, path_size, false, false );
    if ( rc == 0 )
    {
        /* create a access-property list for file-access */
        new_dir -> file_access_property_list = H5Pcreate( H5P_FILE_ACCESS );
        /* set the property-list to use the stdio (buffered) VFL-driver */
        H5Pset_fapl_stdio ( new_dir -> file_access_property_list );
        /* open the hdf5-file using the given property-list */
        new_dir -> hdf5_handle = H5Fopen( resolved, 
                                          H5F_ACC_RDONLY, 
                                          new_dir -> file_access_property_list /*H5P_DEFAULT*/ );
        if ( new_dir -> hdf5_handle >= 0 )
        {
            new_dir -> parent = self;
            new_dir -> h5root = true;
            * hdf5_dir = & new_dir -> dad;
            KDirectoryAddRef ( & new_dir -> dad );
            return 0;
        }
        else
            rc = RC( rcFS, rcDirectory, rcConstructing, rcItem, rcInvalid );
    }
    free( new_dir );
    return rc;
}
