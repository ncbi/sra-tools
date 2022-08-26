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

typedef struct HDF5File HDF5File;
#define KFILE_IMPL HDF5File

#include <kfs/extern.h> /* may need to be changed */
#include "hdf5arrayfile-priv.h"

#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/text.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* object structure */
struct HDF5File
{
    KFile dad;

    /* it maintains a inner HDF5ArrayFile */
    struct HDF5ArrayFile * array_file;
};

typedef struct KSysFile KSysFile;

/* import of HDF5ArrayFileMake from hdf5arrayfile.c */
rc_t HDF5ArrayFileMake ( struct HDF5ArrayFile **fp,
        KFile *parent, hid_t dataset_handle,
        bool read_enabled, bool write_enabled );


/* Destroy
 */
static
rc_t CC HDF5FileDestroy ( HDF5File *self )
{
    /* release the ref-counted inner array-file */
    rc_t rc = KArrayFileRelease( & self -> array_file->dad );
    free ( self );
    return rc;
}


/* GetSysFile
 *  returns an underlying system file object
 *  and starting offset to contiguous region
 *  suitable for memory mapping, or NULL if
 *  no such file is available.
 */
static
struct KSysFile * CC HDF5FileGetSysFile ( const HDF5File *self, uint64_t *offset )
{
    return NULL;
}


/* RandomAccess
 *  ALMOST by definition, the file is random access
 *  certain file types ( notably compressors ) will refuse random access
 *
 *  returns 0 if random access, error code otherwise
 */
static
rc_t CC HDF5FileRandomAccess ( const HDF5File *self )
{
    return 0;
}


/* Size
 *  returns size in bytes of file
 *
 *  "size" [ OUT ] - return parameter for file size
 */
static
rc_t CC HDF5FileSize ( const HDF5File *self, uint64_t *size )
{
    *size = self->array_file->total_bytes;
    return 0;
}


/* SetSize
 *  sets size in bytes of file
 *
 *  "size" [ IN ] - new file size
 */
static
rc_t CC HDF5FileSetSize ( HDF5File *self, uint64_t size )
{
    return RC ( rcFS, rcFile, rcAccessing, rcInterface, rcUnsupported );
}


static
rc_t CC HDF5FileCalcTotalBytes( const HDF5File *self, const uint8_t dim,
                                uint64_t * total_elements )
{
    rc_t rc;
    uint64_t * extents;

    *total_elements = 0;
    extents = malloc( dim * sizeof ( *extents ) );
    if ( extents == NULL )
        return RC ( rcFS, rcFile, rcAccessing, rcMemory, rcExhausted );
    rc = KArrayFileDimExtents ( &( self->array_file->dad ), dim, extents );
    if ( rc == 0 )
    {
        uint8_t i;
        *total_elements = extents[ 0 ];
        for ( i = 1; i < dim; ++i )
            *total_elements *= extents[ i ];
    }
    free( extents );
    return rc;
}


static
rc_t CC HDF5FileReadDim1( const HDF5File *self, uint64_t elem_size, uint64_t pos,
    void *buffer, size_t bsize, size_t *num_read )
{
    uint64_t af_offset, af_count, af_read;
    rc_t rc;

    af_offset = ( pos / elem_size );
    af_count = ( bsize / elem_size );
    if ( ( af_count % elem_size ) > 0 )
        af_count++;
    if ( ( af_count * elem_size ) > bsize )
        return RC ( rcFS, rcFile, rcReading, rcOffset, rcInvalid );

    rc = KArrayFileRead ( &( self->array_file->dad ), 1,
                          &af_offset, buffer, &af_count, &af_read );
    if ( rc != 0 ) return rc;
    *num_read = ( af_read * elem_size );
    return 0;
}


static
bool zero_in_array( uint8_t dim, uint64_t *a )
{
    bool res = false;
    uint8_t i;
    for ( i = 0; i < dim && res == false; ++i )
        if ( a[ i ] == 0 )
            res = true;
    return res;
}

/* translates a flat position into a multi-dim coordinate */
static
void pos_2_coord( uint8_t dim, uint64_t *extents, 
                  uint64_t pos, uint64_t *coord )
{
    int8_t i;
    for ( i = dim - 1; i >= 0; --i )
    {
        uint64_t ext = extents[ i ];
        coord[ i ] = pos % ext;
        pos /= ext;
    }
}


static
rc_t CC HDF5FileReadOneElement( const HDF5File *self, uint8_t dim, uint64_t elem_size,
    uint64_t pos, void *buffer, size_t bsize, size_t *num_read )
{
    rc_t rc;
    uint64_t * extents;
    uint64_t * coord;
    uint64_t * elem_count;
    uint64_t * elem_read;
    uint8_t i;

    *num_read = 0;
    extents = malloc( dim * 4 * sizeof ( *extents ) );
    if ( extents == NULL )
        return RC ( rcFS, rcFile, rcAccessing, rcMemory, rcExhausted );
    coord      = ( extents + dim );
    elem_count = ( coord + dim );
    elem_read  = ( elem_count + dim );

    rc = KArrayFileDimExtents ( &( self->array_file->dad ), dim, extents );
    if ( rc != 0 )
    {
        free( extents );
        return rc;
    }
    if ( zero_in_array( dim, extents ) )
    {
        free( extents );
        return RC ( rcFS, rcFile, rcAccessing, rcParam, rcNull );
    }

    pos /= elem_size;
    pos_2_coord( dim, extents, pos, coord );
    for ( i = 0; i < dim; ++i )
        elem_count[ i ] = 1;
    rc = KArrayFileRead ( &( self->array_file->dad ), dim,
                          coord, buffer, elem_count, elem_read );
    if ( rc == 0 )
        *num_read = elem_size;
    free( extents );
    return rc;
}


/* Read
 *  read file from known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ OUT ] and "bsize" [ IN ] - return buffer for read
 *
 *  "num_read" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of bytes actually read
 */
static
rc_t CC HDF5FileRead ( const HDF5File *self, uint64_t pos,
    void *buffer, size_t bsize, size_t *num_read )
{
    rc_t rc;
    uint64_t elem_size, total_elements;
    uint8_t dim;

    *num_read = 0;
    rc = KArrayFileElementSize ( &( self->array_file->dad ), &elem_size );
    if ( rc != 0 ) return rc;
    elem_size >>= 3; /* KArrayFileElementSize() returns value in bits */

    rc = KArrayFileDimensionality ( &( self->array_file->dad ), &dim );
    if ( rc != 0 ) return rc;

    rc = HDF5FileCalcTotalBytes( self, dim, &total_elements );
    if ( rc != 0 ) return rc;

    /* dont read behind the end of the data */
    if ( pos >= ( total_elements * elem_size ) )
        return RC ( rcFS, rcFile, rcReading, rcOffset, rcInvalid );
    /* read only at offsets that are multiples of the datatype_size */
    if ( ( pos % elem_size ) != 0 )
        return RC ( rcFS, rcFile, rcReading, rcOffset, rcInvalid );

    if ( dim == 1 )
        rc = HDF5FileReadDim1( self, elem_size, pos, buffer, bsize, num_read );
    else
        rc = HDF5FileReadOneElement( self, dim, elem_size, pos, buffer, bsize, num_read );

    return rc;
}


/* Write
 *  write file at known position
 *
 *  "pos" [ IN ] - starting position within file
 *
 *  "buffer" [ IN ] and "size" [ IN ] - data to be written
 *
 *  "num_writ" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of bytes actually written
 */
static
rc_t CC HDF5FileWrite ( HDF5File *self, uint64_t pos,
    const void *buffer, size_t size, size_t *num_writ)
{
    return RC ( rcFS, rcFile, rcAccessing, rcInterface, rcUnsupported );
}


/* Type
 *  returns a KFileDesc
 *  not intended to be a content type,
 *  but rather an implementation class
 */
static
uint32_t CC HDF5FileType ( const HDF5File *self )
{
    return RC ( rcFS, rcFile, rcAccessing, rcInterface, rcUnsupported );
}


static KFile_vt_v1 vtHDF5File =
{
    /* version 1.1 */
    1, 1,

    /* start minor version 0 methods */
    HDF5FileDestroy,
    HDF5FileGetSysFile,
    HDF5FileRandomAccess,
    HDF5FileSize,
    HDF5FileSetSize,
    HDF5FileRead,
    HDF5FileWrite,
    /* end minor version 0 methods */

    /* start minor version == 1 */
    HDF5FileType
    /* end minor version == 1 */
};


/* not static because imported by hdf5file.c via forward decl. */
rc_t HDF5FileMake ( HDF5File **fp, hid_t dataset_handle,
        bool read_enabled, bool write_enabled )
{
    rc_t rc;
    HDF5File *f;

    if ( dataset_handle < 0 )
        return RC ( rcFS, rcFile, rcConstructing, rcFileDesc, rcInvalid );

    f = malloc ( sizeof *f );
    if ( f == NULL )
        return RC ( rcFS, rcFile, rcConstructing, rcMemory, rcExhausted );

    rc = KFileInit ( & f -> dad, ( const KFile_vt* ) &vtHDF5File,
                         "HDF5File", "no-name", read_enabled, write_enabled );
    if ( rc == 0 )
    {
        /* make the inner Arrayfile...*/
        rc = HDF5ArrayFileMake ( & f -> array_file, & f -> dad, dataset_handle,
                             read_enabled, write_enabled );
        if ( rc == 0 )
        {
            * fp = f;
            return 0;
        }
        KArrayFileRelease( & f -> array_file -> dad );
    }
    free ( f );
    return rc;
}


LIB_EXPORT rc_t CC MakeHDF5ArrayFile ( const KFile * self, KArrayFile ** f )
{
    HDF5File * hdf5_self;

    if ( f == NULL )
        return RC ( rcFS, rcFile, rcConstructing, rcParam, rcNull );
    *f = NULL;
    if ( self == NULL )
        return RC ( rcFS, rcFile, rcConstructing, rcSelf, rcNull );
    /* check if the addr of the vt is the one in this file
       if not - then self is not of the type HDF5File */
    if ( self->vt != ( const KFile_vt* )&vtHDF5File )
        return RC ( rcFS, rcFile, rcConstructing, rcParam, rcInvalid );
    hdf5_self = ( HDF5File * )self;
    *f = ( KArrayFile * ) hdf5_self -> array_file;
    return KArrayFileAddRef ( *f );
}
