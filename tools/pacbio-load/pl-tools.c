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

#include "pl-tools.h"
#include <klib/printf.h>
#include <sysalloc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <kdb/database.h>
#include <vdb/database.h>
#include <vdb/vdb-priv.h>

void lctx_init( ld_context * lctx )
{
    lctx->xml_logger = NULL;
    lctx->xml_progress = NULL;
    lctx->with_progress = false;
    lctx->total_printed = false;
    lctx->cache_content = false;
    lctx->check_src_obj = false;
    lctx->total_seq_bases = 0;
    lctx->total_seq_spots = 0;
}


void lctx_free( ld_context * lctx )
{
    if ( lctx->xml_logger != NULL )
    {
        XMLLogger_Release( lctx->xml_logger );
        lctx->xml_logger = NULL;
    }
    if ( lctx->xml_progress != NULL )
    {
        KLoadProgressbar_Release( lctx->xml_progress, false );
        lctx->xml_progress = NULL;
    }
}


rc_t check_src_objects( const KDirectory *hdf5_dir,
                        const char ** groups, 
                        const char **tables,
                        bool show_not_found )
{
    rc_t rc = 0;
    uint16_t idx = 0;
    uint32_t pt;

    if ( groups != NULL )
    {
        while ( groups[ idx ] != NULL && rc == 0 )
        {
            pt = KDirectoryPathType ( hdf5_dir, "%s", groups[idx] );
            if ( pt != kptDir )
            {
                rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
                if ( show_not_found )
                    PLOGERR( klogErr, ( klogErr, rc, "hdf5-group '$(grp)' not found",
                                    "grp=%s", groups[ idx ] ) );
                else
                    PLOGERR( klogWarn, ( klogWarn, rc, "hdf5-group '$(grp)' not found",
                                    "grp=%s", groups[ idx ] ) );
            }
            else
                idx++;
        }
    }

    idx = 0;
    if ( tables != NULL && rc == 0 )
    {
        while ( tables[ idx ] != NULL && rc == 0 )
        {
            pt = KDirectoryPathType ( hdf5_dir, "%s", tables[idx] );
            if ( pt != kptDataset )
            {
                rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
                if ( show_not_found )
                    PLOGERR( klogErr, ( klogErr, rc, "hdf5-table '$(tbl)' not found",
                                    "tbl=%s", tables[ idx ] ) );
                else
                    PLOGERR( klogWarn, ( klogWarn, rc, "hdf5-table '$(tbl)' not found",
                                    "tbl=%s", tables[ idx ] ) );
            }
            else
                idx++;
        }
    }

    return rc;
}


void init_array_file( af_data * af )
{
    af->f  = NULL;
    af->af = NULL;
    af->extents = NULL;
    af->rc = -1;
    af->content = NULL;
}


void free_array_file( af_data * af )
{
    if ( af->af != NULL )
    {
        KArrayFileRelease( af->af );
        af->af = NULL;
    }
    if ( af->f != NULL )
    {
        KFileRelease( af->f );
        af->f = NULL;
    }
    if ( af->extents != NULL )
    {
        free( af->extents );
        af->extents = NULL;
    }
    if ( af->content != NULL )
    {
        free( af->content );
        af->content = NULL;
    }
}


static rc_t read_cache_content( af_data * af )
{
    rc_t rc = 0;
    uint64_t filesize = ( af->element_bits >> 3 ) * ( af->extents[ 0 ] );
    if ( af->dimensionality == 2 )
        filesize *= af->extents[ 1 ];
    af->content = malloc( filesize );
    if ( af->content == NULL )
        rc = RC ( rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted );
    else
    {
        uint64_t pos2[ 2 ];
        uint64_t read2[ 2 ];
        uint64_t count2[ 2 ];
        rc_t rc;

        pos2[ 0 ] = 0;
        pos2[ 1 ] = 0;
        count2[ 0 ] = af->extents[ 0 ];
        if ( af->dimensionality == 2 )
            count2[ 1 ] = af->extents[ 1 ];
        else
            count2[ 1 ] = 0;

        rc = KArrayFileRead ( af->af, af->dimensionality, pos2, 
                              af->content, count2, read2 );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "error reading arrayfile-data int cache" );
    }
    return rc;
}


rc_t open_array_file( const KDirectory *dir,
                      const char *name,
                      af_data * af,
                      const uint64_t expected_element_bits,
                      const uint64_t expected_cols,
                      bool disp_wrong_bitsize,
                      bool cache_content,
                      bool supress_err_msg )
{
    rc_t rc;

    init_array_file( af );
    /* open the requested "File" (actually a hdf5-table) as KFile 
       the works because the given KDirectory is a HDF5-Directory */
    rc = KDirectoryOpenFileRead ( dir, &af->f, "%s", name );
    if ( rc != 0 )
    {
        if ( !supress_err_msg )
        {
            PLOGERR( klogErr, ( klogErr, rc, "cannot open hdf5-dataset '$(name)'",
                            "name=%s", name ) );
        }
        return rc;
    }
    /* cast the KFile into a KArrayFile */
    rc = MakeHDF5ArrayFile ( af->f, &af->af );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc, "cannot open hdf5-arrayfile '$(name)'",
                            "name=%s", name ) );
        free_array_file( af );
        return rc;
    }
    /* detect the dimensionality of the array-file */
    rc = KArrayFileDimensionality ( af->af, &af->dimensionality );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc, "cannot retrieve dimensionality on '$(name)'",
                            "name=%s", name ) );
        free_array_file( af );
        return rc;
    }
    /* make a array to hold the extent in every dimension */
    af->extents = malloc( af->dimensionality * ( sizeof ( uint64_t ) ) );
    if ( af->extents == NULL )
    {
        rc = RC ( rcApp, rcArgv, rcAccessing, rcMemory, rcExhausted );
        PLOGERR( klogErr, ( klogErr, rc, "cannot allocate enough memory for extents of '$(name)'",
                            "name=%s", name ) );
        free_array_file( af );
        return rc;
    }
    /* read the actuall extents into the created array */
    rc = KArrayFileDimExtents ( af->af, af->dimensionality, af->extents );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc, "cannot retrieve extents of '$(name)'",
                            "name=%s", name ) );
        free_array_file( af );
        return rc;
    }
    /* request the size of the element in bits */
    rc = KArrayFileElementSize ( af->af, &af->element_bits );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc, "cannot retrieve element-size of '$(name)'",
                            "name=%s", name ) );
        free_array_file( af );
        return rc;
    }
    /* compare the discovered bit-size with the expected one */
    if ( af->element_bits != expected_element_bits )
    {
        rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );

        /* display the wrong bitsize only if wanted 
           ( this function can be called to probe the bitsize:
             in this case the wrong one should not be shown as an error )*/
        if ( disp_wrong_bitsize )
            PLOGERR( klogErr, ( klogErr, rc, "unexpected element-bits of $(bsize) in '$(name)'",
                     "bsize=%lu,name=%s", af->element_bits, name ) );

        free_array_file( af );
        return rc;
    }

    /* not generic, we handle only dimensionality of 1 and 2 */
    if ( expected_cols == 1 )
    {
        /* the dimensionality has to be 1 in this case */
        if ( af->dimensionality != 1 )
        {
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
            PLOGERR( klogErr, ( klogErr, rc, "unexpected dimensionality of $(dim) in '$(name)'",
                                "dim=%lu,name=%s", af->dimensionality, name ) );
            free_array_file( af );
            return rc;
        }
    }
    else
    {
        /* the dimensionality has to be 2 in this case */
        if ( af->dimensionality != 2 )
        {
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
            PLOGERR( klogErr, ( klogErr, rc, "unexpected dimensionality of $(dim) in '$(name)'",
                                "dim=%lu,name=%s", af->dimensionality, name ) );
            free_array_file( af );
            return rc;
        }
        else
        {
            if ( af->extents[ 1 ] != expected_cols )
            {
                rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
                PLOGERR( klogErr, ( klogErr, rc, "unexpected extent[1] of $(ext) in '$(name)'",
                                    "ext=%lu,name=%s", af->extents[ 1 ], name ) );
                free_array_file( af );
                return rc;
            }
        }
    }
    if ( rc == 0 && cache_content )
    {
        rc = read_cache_content( af );
    }
    return rc;
}


/* assembles the 'absolute' path to the requested array-file before opening it */
rc_t open_element( const KDirectory *hdf5_dir, 
                   af_data *element, 
                   const char * path,
                   const char * name, 
                   const uint64_t expected_element_bits,
                   const uint64_t expected_cols,
                   bool disp_wrong_bitsize,
                   bool cache_content,
                   bool supress_err_msg )
{
    char src_path[ 64 ];
    size_t num_writ;

    element->rc = string_printf ( src_path, sizeof src_path, &num_writ, "%s/%s", path, name );
    if ( element->rc != 0 )
        LOGERR( klogErr, element->rc, "cannot assemble hdf5-element-name" );
    else
        element->rc = open_array_file( hdf5_dir, src_path, element, 
                                       expected_element_bits, expected_cols,
                                       disp_wrong_bitsize,
                                       cache_content,
                                       supress_err_msg );
    return element->rc;
}


/* we are reading data from an array-file,
   the underlying array-file knows the size of an element */
rc_t array_file_read_dim1( af_data * af, const uint64_t pos,
                           void *dst, const uint64_t count,
                           uint64_t *n_read )
{
    rc_t rc = 0;
    if ( af->content == NULL )
        rc = KArrayFileRead ( af->af, 1, &pos, dst, &count, n_read );
    else
    {
        if ( ( pos + count ) > af->extents[ 0 ] )
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
        else
        {
            uint64_t buf_idx = ( af->element_bits >> 3 ) * pos;
            size_t num = ( af->element_bits >> 3 ) * count;
            char * src = af->content;
            src+=buf_idx;
            memcpy( dst, src, num );
            *n_read = count;
        }
    }
    if ( rc != 0 )
        LOGERR( klogErr, rc, "error reading arrayfile-data (1 dim)" );
    return rc;
}


/* we are reading values in 2 dimensions from the array-file */
rc_t array_file_read_dim2( af_data * af, const uint64_t pos,
                           void *dst, const uint64_t count,
                           const uint64_t ext2, uint64_t *n_read )
{
    rc_t rc = 0;
    if ( af->content == NULL )
    {
        uint64_t pos2[ 2 ];
        uint64_t read2[ 2 ];
        uint64_t count2[ 2 ];
        rc_t rc;

        pos2[ 0 ] = pos;
        pos2[ 1 ] = 0;
        count2[ 0 ] = count;
        count2[ 1 ] = ext2;
        rc = KArrayFileRead ( af->af, 2, pos2, dst, count2, read2 );
        if ( rc != 0 )
            LOGERR( klogErr, rc, "error reading arrayfile-data (2 dim)" );
        *n_read = read2[ 0 ];
    }
    else
    {
        if ( ( pos + count ) > af->extents[ 0 ] )
            rc = RC ( rcExe, rcNoTarg, rcLoading, rcData, rcInconsistent );
        else
        {
            uint64_t buf_idx = ( af->element_bits >> 3 ) * pos * af->extents[ 1 ];
            size_t num = ( af->element_bits >> 3 ) * count * af->extents[ 1 ];
            char * src = af->content;
            src+=buf_idx;
            memcpy( dst, src, num );
            *n_read = count * af->extents[ 1 ];
        }
    }
    return rc;
}


rc_t add_columns( VCursor * cursor, uint32_t count, int32_t exclude_this,
                  uint32_t * idx_vector, const char ** names )
{
    rc_t rc = 0;
    uint32_t i;
    for ( i = 0; i < count && rc == 0; ++i )
    {
        if ( i != exclude_this )
        {
            rc = VCursorAddColumn( cursor, &(idx_vector[i]), "%s", names[i] );
            if ( rc != 0 )
                PLOGERR( klogErr, ( klogErr, rc, "cannot add column '$(name)' to vdb-cursor",
                                    "name=%s", names[i] ) );
        }
    }
    return rc;
}

bool check_table_count( af_data *tab, const char * name,
                        const uint64_t expected )
{
    bool res = ( tab->extents[ 0 ] == expected );
    if ( !res )
    {
        rc_t rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
        PLOGERR( klogErr, ( klogErr, rc, "'$(name)'.count != expected",
                            "name=%s", name ) );
    }
    return res;
}


rc_t transfer_bits( VCursor *cursor, const uint32_t col_idx,
    af_data *src, char * buffer, const uint64_t offset, const uint64_t count,
    const uint32_t n_bits, const char * explanation )
{
    uint64_t n_read;
    rc_t rc = array_file_read_dim1( src, offset, buffer, count, &n_read );
    if ( rc == 0 )
    {
        if ( count != n_read )
        {
            rc = RC( rcExe, rcNoTarg, rcAllocating, rcParam, rcInvalid );
            PLOGERR( klogErr, ( klogErr, rc, "cannot read enought data from hdf5-table for '$(name)'",
                                "name=%s", explanation ) );
        }
        if ( rc == 0 )
        {
            rc = VCursorWrite( cursor, col_idx, n_bits, buffer, 0, count );
            if ( rc != 0 )
                PLOGERR( klogErr, ( klogErr, rc, "cannot write data to vdb for '$(name)'",
                                    "name=%s", explanation ) );
        }
    }
    return rc;
}


rc_t vdb_write_value( VCursor *cursor, const uint32_t col_idx,
                      void * src, const uint32_t n_bits,
                      const uint32_t n_elem, const char *explanation )
{
    rc_t rc = VCursorWrite( cursor, col_idx, n_bits, src, 0, n_elem );
    if ( rc != 0 )
        PLOGERR( klogErr, ( klogErr, rc, "cannot write data to vdb for '$(name)'",
                            "name=%s", explanation ) );
    return rc;
}


rc_t vdb_write_uint32( VCursor *cursor, const uint32_t col_idx,
                       uint32_t value, const char *explanation )
{
    return vdb_write_value( cursor, col_idx, &value, 32, 1, explanation );
}


rc_t vdb_write_uint16( VCursor *cursor, const uint32_t col_idx,
                       uint16_t value, const char *explanation )
{
    return vdb_write_value( cursor, col_idx, &value, 16, 1, explanation );
}


rc_t vdb_write_uint8( VCursor *cursor, const uint32_t col_idx,
                      uint8_t value, const char *explanation )
{
    return vdb_write_value( cursor, col_idx, &value, 8, 1, explanation );
}


rc_t vdb_write_float32( VCursor *cursor, const uint32_t col_idx,
                        float value, const char *explanation )
{
    return vdb_write_value( cursor, col_idx, &value, 32, 1, explanation );
}


rc_t prepare_table( VDatabase * database, VCursor ** cursor,
                    const char * template_name,
                    const char * table_name )
{
    VTable * table;
    rc_t rc = VDatabaseCreateTable( database, &table, template_name, 
                                    kcmInit | kcmMD5 | kcmParents, "%s", table_name );
    if ( rc != 0 )
    {
        PLOGERR( klogErr, ( klogErr, rc, "cannot create vdb-table '$(name)'",
                            "name=%s", table_name ) );
    }
    else
    {
        rc = VTableCreateCursorWrite( table, cursor, kcmInsert );
        if ( rc != 0 )
        {
            PLOGERR( klogErr, ( klogErr, rc, "cannot create vdb-cursor for '$(name)'",
                                "name=%s", table_name ) );
        }
        VTableRelease( table );
    }
    return rc;
}


rc_t load_table( VDatabase * database, KDirectory * hdf5_src, ld_context *lctx,
                 const char * template_name, const char * table_name, loader_func func )
{
    VTable * table;
    rc_t rc = VDatabaseCreateTable( database, &table, template_name, 
                                    kcmInit | kcmMD5 | kcmParents, "%s", table_name );
    if ( rc != 0 )
        PLOGERR( klogErr, ( klogErr, rc, "cannot create vdb-table '$(name)'",
                            "name=%s", table_name ) );
    else
    {
        VCursor * cursor;
        rc = VTableCreateCursorWrite( table, &cursor, kcmInsert );
        if ( rc != 0 )
            PLOGERR( klogErr, ( klogErr, rc, "cannot create vdb-cursor for '$(name)'",
                                "name=%s", table_name ) );
        else
        {
            VTableRelease( table );
            rc = func( lctx, hdf5_src, cursor, table_name ); /* the callback does the job! */
            VCursorRelease( cursor );
        }
    }
    return rc;
}


rc_t progress_chunk( const KLoadProgressbar ** xml_progress, const uint64_t chunk )
{
    rc_t rc;
    /* release the old progressbar... */
    if ( *xml_progress != NULL )
    {
        KLoadProgressbar_Release( *xml_progress, false );
        *xml_progress = NULL;
    }
    rc = KLoadProgressbar_Make( xml_progress, 0 );
    if ( rc == 0 )
        rc = KLoadProgressbar_Append( *xml_progress, chunk );
    else
        LOGERR( klogErr, rc, "cannot make KLoadProgressbar" );

    return rc;
}


rc_t progress_step( const KLoadProgressbar * xml_progress )
{
    if ( xml_progress != NULL )
       return KLoadProgressbar_Process( xml_progress, 1, false );
    else
        return 0;
}


void print_log_info( const char * info )
{
    KLogLevel tmp_lvl = KLogLevelGet();
    KLogLevelSet( klogInfo );
    LOGMSG( klogInfo, info );
    KLogLevelSet( tmp_lvl );
}


/* was once intended to make the SEQ-table a alias to the CONSENSU-table,
   this step was removed, but the decision could be reinstate again
   because of that the function to do so is still here */
rc_t pacbio_make_alias( VDatabase * vdb_db,
                        const char *existing_obj, const char *alias_to_create )
{
    KDatabase *kdb;
    rc_t rc = VDatabaseOpenKDatabaseUpdate ( vdb_db, & kdb );
    if ( rc == 0 )
    {
        rc = KDatabaseAliasTable ( kdb, existing_obj, alias_to_create );
        KDatabaseRelease ( kdb );
    }
    return rc;
}
