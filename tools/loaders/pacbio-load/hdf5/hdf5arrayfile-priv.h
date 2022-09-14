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

#ifndef _h_hdf5arrayfile_priv_
#define _h_hdf5arrayfile_priv_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _h_kfs_extern_
#include <kfs/extern.h>
#endif

#ifndef _h_kfs_impl_
#include <kfs/impl.h>
#endif

#ifndef _HDF5_H
#include <hdf5.h>
#endif

/* object structure */
struct HDF5ArrayFile
{
    KArrayFile dad;

    KFile * parent;
    /* the handle of the dataset, equivalent of the file-handle
       opened in  */
    hid_t dataset_handle;

    /* the handle of the datatype of the elemens */
    hid_t datatype_handle;
    /* the enum of the datatype-class */
    H5T_class_t datatype_class;
    /* the actual size in bytes of the datatype */
    size_t element_size;

    /* the handle of the table-layout of the data in this dataset */
    hid_t dataspace_handle;
    uint8_t dimensionality;
    hsize_t * extents;
    uint64_t total_elements;
    uint64_t total_bytes;

    /* the handle of the buffer-layout of the dest. buffer */
    hid_t dst_dataspace_handle;
};


#ifdef __cplusplus
}
#endif

#endif /* _h_hdf5arrayfile_priv_ */