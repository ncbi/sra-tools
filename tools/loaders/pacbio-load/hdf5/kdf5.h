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

#ifndef _h_kdf5_
#define _h_kdf5_

#ifndef _h_hdf5_extern_
#include <hdf5/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifndef _h_kfs_file_
#include <kfs/file.h>
#endif

#ifndef _h_kfs_arrayfile_
#include <kfs/arrayfile.h>
#endif

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif


/* MakeHDF5RootDir
 *
 *  creates a root-hdf5-directory (in read-only mode)
 *  from a absolute or relative path
 *
 *  "self"     [ IN ]  - directory, used to resolve the path
 *
 *  "hdf5_dir" [ OUT ] - return parameter for the created directory
 *
 *  "absolute" [ IN ]  - flag: absolute or relative
 *
 *  "path"     [ IN ]  - absolute or relative path to hdf5-file
 */
HDF5_EXTERN rc_t CC MakeHDF5RootDir ( KDirectory * self, KDirectory ** hdf5_dir, 
                                      bool absolute, const char *path );


/* MakeHDF5ArrayFile
 *
 *  creates a KArrayFile (in read-only mode) from a KFile
 *
 *  "self"     [ IN ]  - file, this KFile has to be made from a KDirectory
 *                       the interanly is a HDF5Directory
 *
 *  "f"        [ OUT ] - return parameter for the created KArrayFile
 */
HDF5_EXTERN rc_t CC MakeHDF5ArrayFile ( const KFile * self, KArrayFile ** f );

#ifdef __cplusplus
}
#endif

#endif /* _h_kdf5_ */
