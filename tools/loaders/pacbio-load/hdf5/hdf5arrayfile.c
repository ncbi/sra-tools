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

typedef struct HDF5ArrayFile HDF5ArrayFile;
#define KARRAYFILE_IMPL HDF5ArrayFile

#include "hdf5arrayfile-priv.h"

#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/text.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static
rc_t HDF5ArrayFile_open ( HDF5ArrayFile * self, hid_t dataset_handle )
{
    rc_t rc = 0;

    self -> dataset_handle = dataset_handle;

    /* take care of the datatype */
    self->datatype_handle = H5Dget_type( self->dataset_handle );
    if ( self->datatype_handle < 0 )
        return RC( rcFS, rcFile, rcOpening, rcData, rcInvalid );
    self->datatype_class = H5Tget_class( self->datatype_handle );
    if ( self->datatype_class == H5T_NO_CLASS )
        rc = RC( rcFS, rcFile, rcOpening, rcData, rcInvalid );
    else
    {
        self->element_size  = H5Tget_size( self->datatype_handle );
        if ( self->element_size == 0 )
            rc = RC( rcFS, rcFile, rcOpening, rcData, rcInvalid );
    }
    if ( rc != 0 )
    {
        H5Tclose( self->datatype_handle );
        return rc;
    }

    /* detect the dimensions ( dataspace ) */
    self->dataspace_handle = H5Dget_space( self->dataset_handle );
    if ( self->dataspace_handle < 0 )
        return RC( rcFS, rcFile, rcOpening, rcData, rcInvalid );
    self->dimensionality = H5Sget_simple_extent_ndims( self->dataspace_handle );
    if ( self->dimensionality < 1 )
        rc = RC( rcFS, rcFile, rcOpening, rcData, rcInvalid );
    else
    {
        self->extents = malloc( sizeof( hsize_t ) * self->dimensionality );
        if ( self->extents == NULL )
            rc = RC( rcFS, rcFile, rcOpening, rcMemory, rcExhausted );
        else
        {
            int h5res = H5Sget_simple_extent_dims( self->dataspace_handle, self->extents, NULL );
            if ( h5res < 0 )
                rc = RC( rcFS, rcFile, rcOpening, rcData, rcInvalid );
            else
            {
                int i;
                self->total_elements = self->extents[ 0 ];
                for ( i = 1; i < self->dimensionality; ++i )
                    self->total_elements *= self->extents[ i ];
                self->total_bytes = self->total_elements * self->element_size;
            }
            if ( rc != 0 )
                free( self->extents );
        }
    }

    if ( rc != 0 )
        H5Dclose( self->dataspace_handle );
    return rc;
}


static
rc_t HDF5ArrayFile_close ( HDF5ArrayFile * self )
{
    if ( self->extents != NULL )
        free( self->extents );
    if ( self->datatype_handle >= 0 )
        H5Tclose( self->datatype_handle );
    if ( self->dataspace_handle >= 0 )
        H5Sclose( self->dataspace_handle );
    if ( self->dataset_handle >= 0 )
        H5Dclose( self->dataset_handle );
    return 0;
}



/* Destroy
 */
static
rc_t CC HDF5ArrayFileDestroy ( HDF5ArrayFile *self )
{
    HDF5ArrayFile_close ( self );
    free ( self );
    return 0;
}


/* Dimensionality
 *  returns the number of dimensions in the ArrayFile
 *
 *  "dim" [ OUT ] - return parameter for number of dimensions
 */
static
rc_t CC HDF5ArrayFileDimensionality ( const HDF5ArrayFile *self, uint8_t *dim )
{
    *dim = self->dimensionality;
    return 0;
}


/* SetDimensionality
 *  sets the number of dimensions in the ArrayFile
 *
 *  "dim" [ IN ] - new number of dimensions; must be > 0
 */
static
rc_t CC HDF5ArrayFileSetDimensionality ( HDF5ArrayFile *self, uint8_t dim )
{
    self->dimensionality = dim;
    return 0;
}


/* DimExtents
 *  returns the extent of every dimension
 *
 *  "dim" [ IN ] - the dimensionality of "extents"
 *
 *  "extents" [ OUT ] - returns the extent for every dimension
 */
static
rc_t CC HDF5ArrayFileDimExtents ( const HDF5ArrayFile *self, uint8_t dim, uint64_t *extents )
{
    uint8_t i;

    if ( dim != self->dimensionality )
        return RC( rcFS, rcFile, rcAccessing, rcParam, rcInvalid );
    for ( i = 0; i < dim; ++i )
        extents[ i ] = (uint64_t) self->extents[ i ];
    return 0;
}


/* SetDimExtents
 *  sets the new extents for every dimension
 *
 *  "dim" [ IN ] - the dimensionality of "extents"
 *
 *  "extents" [ IN ] - new extents for every dimension
 */
static
rc_t CC HDF5ArrayFileSetDimExtents ( HDF5ArrayFile *self, uint8_t dim, uint64_t *extents )
{
    uint8_t i;

    if ( dim != self->dimensionality )
        return RC( rcFS, rcFile, rcResizing, rcParam, rcInvalid );
    for ( i = 0; i < dim; ++i )
        self->extents[ i ] = (hsize_t) extents[ i ];
    return 0;
}


/* ElementSize
 *  returns the element size in bits
 *
 *  "elem_bits" [ OUT ] - size of each element in bits
 */
static
rc_t CC HDF5ArrayFileElementSize ( const HDF5ArrayFile *self, uint64_t *elem_bits )
{
    *elem_bits = ( self->element_size * 8 );
    return 0;
}


static
rc_t HDF5ArrayMakeRdWrOfsCnt( uint8_t dim, 
                              const uint64_t *pos, const uint64_t *elem_count,
                              hsize_t **ofs, hsize_t **cnt )
{
    uint8_t i;

    *ofs = malloc( sizeof ( hsize_t ) * dim );
    if ( *ofs == NULL )
        return RC( rcFS, rcFile, rcReading, rcMemory, rcExhausted );
    *cnt = malloc( sizeof ( hsize_t ) * dim );
    if ( *cnt == NULL )
    {
        free( *ofs );
        return RC( rcFS, rcFile, rcReading, rcMemory, rcExhausted );
    }
    for ( i = 0; i < dim; ++i )
    {
        (*ofs)[ i ] = (hsize_t)pos[ i ];
        (*cnt)[ i ] = (hsize_t)elem_count[ i ];
    }
    return 0;
}


static
hsize_t HDF5ArrayCalcTotal( const uint8_t dim, const hsize_t *count )
{
    uint8_t i;
    hsize_t res = count[ 0 ];
    for ( i = 1; i < dim; ++i )
        res *= count[ i ];
    return res;
}


static
rc_t HDF5ArrayFileRead_intern ( const HDF5ArrayFile *self,
    const hsize_t *ofs, void *buffer, const hsize_t *count )
{
    rc_t rc = 0;
    herr_t status;

    /* first we select a hyperslab in the source-dataspace */
    status = H5Sselect_hyperslab( self->dataspace_handle, 
                H5S_SELECT_SET, ofs, NULL, count, NULL );
    if ( status < 0 )
        rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
    else
    {
        /* calculate how many data-elements to read total */
        hsize_t total = HDF5ArrayCalcTotal( self->dimensionality, count );
        if ( total < 1 )
            rc = RC( rcFS, rcFile, rcReading, rcParam, rcInvalid );
        else
        {
            /* then we create a temp. dst-dataspace */
            hid_t dst_space = H5Screate_simple( 1, &total, NULL );
            if ( dst_space < 0 )
                rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
            else
            {
                hid_t native_type = H5Tget_native_type( self->datatype_handle, H5T_DIR_ASCEND );
                status = H5Dread( self->dataset_handle,   /* src dataset */
                                  native_type,            /* discovered datatype */
                                  dst_space,              /* temp. 1D dataspace */
                                  self->dataspace_handle, /* src dataspace */
                                  H5P_DEFAULT,            /* default transfer property list */
                                  buffer );               /* dst-buffer */
                if ( status < 0 )
                    rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );

                H5Tclose( native_type );
                H5Sclose( dst_space );
            }
        }
    }
    return rc;
}

/* Read
 *  read from n-dimensional position
 *
 *  "dim" [ IN ] - the dimensionality of all vectors
 *
 *  "pos"  [ IN ] - n-dimensional starting position in elements
 *
 *  "buffer" [ OUT ] and "elem_count" [ IN ] - return buffer for read
 *  where "elem_count" is n-dimensional in elements
 *
 *  "num_read" [ OUT ] - n-dimensional return parameter giving back
 *      the number of read elements in every dimension
 */
static
rc_t CC HDF5ArrayFileRead ( const HDF5ArrayFile *self, uint8_t dim,
    const uint64_t *pos, void *buffer, const uint64_t *elem_count,
    uint64_t *num_read )
{
    rc_t rc;
    hsize_t * hf_ofs;
    hsize_t * hf_count;

    if ( dim != self->dimensionality )
        return RC( rcFS, rcFile, rcReading, rcParam, rcInvalid );

    rc = HDF5ArrayMakeRdWrOfsCnt( dim, pos, elem_count, &hf_ofs, &hf_count );
    if ( rc != 0 )
        return rc;

    rc = HDF5ArrayFileRead_intern ( self, hf_ofs, buffer, hf_count );
    if ( rc == 0 )
    {
        uint8_t i;
        for ( i = 0; i < dim; ++i )
            num_read[ i ] = elem_count[ i ];
    }

    free( hf_count );
    free( hf_ofs );

    return rc;
}


static
rc_t HDF5ArrayFileRead_v_intern ( const HDF5ArrayFile *self, uint8_t dim,
    const hsize_t *ofs, char * buffer, const uint64_t buffer_size, uint64_t * num_read )
{
    rc_t rc = 0;

    hsize_t * hf_count = malloc( sizeof ( hsize_t ) * dim );
    if ( hf_count == NULL )
        return RC( rcFS, rcFile, rcReading, rcMemory, rcExhausted );
    else
    {
        herr_t status;
        uint8_t i;

        for ( i = 0; i < dim; ++i ) hf_count[ i ] = 1;

        /* first we select a hyperslab in the source-dataspace */
        status = H5Sselect_hyperslab( self->dataspace_handle, 
                    H5S_SELECT_SET, ofs, NULL, hf_count, NULL );
        if ( status < 0 )
            rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
        else
        {
            /* calculate how many data-elements to read total */
            hsize_t total = HDF5ArrayCalcTotal( self->dimensionality, hf_count );
            if ( total < 1 )
                rc = RC( rcFS, rcFile, rcReading, rcParam, rcInvalid );
            else
            {
                /* then we create a temp. dst-dataspace */
                hid_t dst_space = H5Screate_simple( 1, &total, NULL );
                if ( dst_space < 0 )
                    rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
                else
                {
                    hid_t memtype = H5Tcopy ( H5T_C_S1 );
                    status = H5Tset_size ( memtype, H5T_VARIABLE );
                    if ( status < 0 )
                        rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
                    else
                    {
                        char * rdata[ 1 ];
                        status = H5Dread ( self->dataset_handle, memtype, dst_space,
                                           self->dataspace_handle, H5P_DEFAULT, rdata );
                        if ( status < 0 )
                            rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
                        else
                        {
                            uint32_t len = string_size ( rdata[ 0 ] );
                            *num_read = string_copy ( buffer, buffer_size, rdata[ 0 ], len );
                        }
                        H5Dvlen_reclaim ( memtype, dst_space, H5P_DEFAULT, rdata );
                    }
                    H5Tclose( memtype );
                    H5Sclose( dst_space );
                }
            }
        }
        free( hf_count );
    }
    return rc;
}


/* Read_v
 *  reads ONE element from n-dimensional position, but the element is of variable length
 *
 *  "dim" [ IN ] - the dimensionality of all vectors
 *
 *  "pos"  [ IN ] - n-dimensional starting position in elements
 *
 *  "buffer" [ OUT ] and "elem_count" [ IN ] - return buffer for read
 *  where "buffer_size" is the size of the buffer
 *
 *  "num_read" [ OUT ] - return parameter giving back the variable length of the one
 *    element
 */
static
rc_t CC HDF5ArrayFileRead_v ( const HDF5ArrayFile *self, uint8_t dim,
    const uint64_t *pos, char * buffer, const uint64_t buffer_size,
    uint64_t *num_read )
{
    rc_t rc = 0;
    hsize_t * hf_ofs;
    uint8_t i;

    if ( dim != self->dimensionality )
        return RC( rcFS, rcFile, rcReading, rcParam, rcInvalid );

    *num_read = 0;
    hf_ofs = malloc( sizeof ( hsize_t ) * dim );
    if ( hf_ofs == NULL )
        return RC( rcFS, rcFile, rcReading, rcMemory, rcExhausted );

    for ( i = 0; i < dim; ++i )
        hf_ofs[ i ] = ( hsize_t )pos[ i ];

    rc = HDF5ArrayFileRead_v_intern ( self, dim, hf_ofs, buffer, buffer_size, num_read );

    free( hf_ofs );

    return rc;
}


static
rc_t HDF5ArrayFileWrite_intern ( const HDF5ArrayFile *self,
    const hsize_t *ofs, const void *buffer, const hsize_t *count )
{
    rc_t rc = 0;
    herr_t status;

    /* first we select a hyperslab in the source-dataspace */
    status = H5Sselect_hyperslab( self->dataspace_handle, 
                H5S_SELECT_SET, ofs, NULL, count, NULL );
    if ( status < 0 )
        rc = RC( rcFS, rcFile, rcWriting, rcTransfer, rcInvalid );
    else
    {
        /* calculate how many data-elements to write total */
        hsize_t total = HDF5ArrayCalcTotal( self->dimensionality, count );
        if ( total < 1 )
            rc = RC( rcFS, rcFile, rcWriting, rcParam, rcInvalid );
        else
        {
            /* then we create a temp. dst-dataspace */
            hid_t src_space = H5Screate_simple( 1, &total, NULL );
            if ( src_space < 0 )
                rc = RC( rcFS, rcFile, rcWriting, rcTransfer, rcInvalid );
            else
            {
                status = H5Dwrite( self->dataset_handle,   /* src dataset */
                                   self->datatype_handle,  /* discovered datatype */
                                   src_space,              /* temp. 1D dataspace */
                                   self->dataspace_handle, /* src dataspace */
                                   H5P_DEFAULT,            /* default transfer property list */
                                   buffer );               /* dst-buffer */
                if ( status < 0 )
                    rc = RC( rcFS, rcFile, rcWriting, rcTransfer, rcInvalid );
                H5Sclose( src_space );
            }
        }
    }
    return rc;
}


/* Write
 *  write into n-dimensional position
 *
 *  "dim" [ IN ] - the dimensionality of all vectors
 *
 *  "pos"  [ IN ] - n-dimensional offset where to write to
 *                   in elements
 *
 *  "buffer" [ IN ] and "elem_count" [ IN ] - data to be written
 *  where "elem_count" is n-dimensional in elements
 *
 *  "num_writ" [ OUT, NULL OKAY ] - optional return parameter
 *  giving number of elements actually written per dimension
 */
static
rc_t CC HDF5ArrayFileWrite ( HDF5ArrayFile *self, uint8_t dim,
    const uint64_t *pos, const void *buffer, const uint64_t *elem_count,
    uint64_t *num_writ )
{
    rc_t rc;
    hsize_t * hf_ofs;
    hsize_t * hf_count;

    if ( dim != self->dimensionality )
        return RC( rcFS, rcFile, rcReading, rcParam, rcInvalid );

    rc = HDF5ArrayMakeRdWrOfsCnt( dim, pos, elem_count, &hf_ofs, &hf_count );
    if ( rc != 0 )
        return rc;
    rc = HDF5ArrayFileWrite_intern ( self, hf_ofs, buffer, hf_count );
    if ( rc == 0 )
    {
        uint8_t i;
        for ( i = 0; i < dim; ++i )
            num_writ[ i ] = elem_count[ i ];
    }
    free( hf_count );
    free( hf_ofs );
    return rc;
}



/* GetMeta
 *  extracts metadata into a string-vector
 *
 *  "key"   [ IN ]  - the key which part of the metadata to retrieve
 *
 *  "list"  [ OUT ] - the metadata will be filled into this list
 *
 */
static
rc_t CC HDF5ArrayFileGetMeta ( const HDF5ArrayFile *self, const char *key,
    const KNamelist **list )
{
    rc_t rc;
    hid_t attr = H5Aopen_by_name( self->dataset_handle, ".", key, H5P_DEFAULT, H5P_DEFAULT );
    if ( attr < 0 )
        rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
    else
    {
        hid_t datatype = H5Aget_type( attr );
        if ( datatype < 0 )
            rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
        else
        {
            hid_t space = H5Aget_space( attr );
            if ( space < 0 )
                rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
            else
            {
                hsize_t dims[1];
                char ** rdata;
                H5Sget_simple_extent_dims( space, dims, NULL );
                rdata = ( char ** )malloc( dims[0] * sizeof( char *) );
                if ( rdata == NULL )
                    rc = RC( rcFS, rcFile, rcOpening, rcMemory, rcExhausted );
                else
                {
                    hid_t memtype = H5Tcopy( H5T_C_S1 );
                    if ( memtype < 0 )
                        rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
                    else
                    {
                        herr_t status = H5Tset_size( memtype, H5T_VARIABLE );
                        if ( status < 0 )
                            rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
                        else
                        {
                            status = H5Aread( attr, memtype, rdata );
                            if ( status < 0 )
                                rc = RC( rcFS, rcFile, rcReading, rcTransfer, rcInvalid );
                            else
                            {
                                VNamelist *names;
                                rc = VNamelistMake ( &names, 5 );
                                if ( rc == 0 )
                                {
                                    int i;
                                    for ( i = 0; i < dims[0]; ++i )
                                        VNamelistAppend ( names, rdata[i] );
                                    rc = VNamelistToConstNamelist ( names, list );
                                    VNamelistRelease ( names );
                                }
                            }
                        }
                        H5Dvlen_reclaim( memtype, space, H5P_DEFAULT, rdata );
                        H5Tclose( memtype );
                    }
                    free( rdata );
                }
                H5Sclose( space );
            }
            H5Tclose( datatype );
        }
        H5Aclose( attr );
    }
    return rc;
}


static KArrayFile_vt_v1 vtHDF5ArrayFile =
{
    /* version 1.0 */
    1, 0,

    /* start minor version 0 methods */
    HDF5ArrayFileDestroy,
    HDF5ArrayFileDimensionality,
    HDF5ArrayFileSetDimensionality,
    HDF5ArrayFileDimExtents,
    HDF5ArrayFileSetDimExtents,
    HDF5ArrayFileElementSize,
    HDF5ArrayFileRead,
    HDF5ArrayFileWrite,
    HDF5ArrayFileGetMeta,
    HDF5ArrayFileRead_v
    /* end minor version 0 methods */

};


/* not static because imported by hdf5file.c via forward decl. */
rc_t HDF5ArrayFileMake ( struct HDF5ArrayFile **fp,
        KFile * parent, hid_t dataset_handle,
        bool read_enabled, bool write_enabled )
{
    rc_t rc;
    HDF5ArrayFile *f;

    if ( fp == NULL )
        return RC ( rcFS, rcFile, rcConstructing, rcSelf, rcNull );
    *fp = NULL;

    if ( parent == NULL )
        return RC ( rcFS, rcFile, rcConstructing, rcParam, rcNull );

    f = malloc ( sizeof *f );
    if ( f == NULL )
        return RC ( rcFS, rcFile, rcConstructing, rcMemory, rcExhausted );

    rc = KArrayFileInit ( & f -> dad, ( const KArrayFile_vt * )&vtHDF5ArrayFile, 
                          read_enabled, write_enabled );
    if ( rc == 0 )
    {
        f -> parent = parent;
        rc = HDF5ArrayFile_open ( f, dataset_handle );
        if ( rc == 0 )
        {
            * fp = f;
            return 0;
        }
    }
    free ( f );
    return rc;

}