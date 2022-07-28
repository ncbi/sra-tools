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

#include "pl-basecalls_cmn.h"
#include <sysalloc.h>

void init_BaseCalls_cmn( BaseCalls_cmn *tab )
{
    zmw_init( &tab->zmw );
    init_array_file( &tab->Basecall );
    init_array_file( &tab->QualityValue );
    init_array_file( &tab->DeletionQV );
    init_array_file( &tab->DeletionTag );
    init_array_file( &tab->InsertionQV );
    init_array_file( &tab->SubstitutionQV );
    init_array_file( &tab->SubstitutionTag );
}


void close_BaseCalls_cmn( BaseCalls_cmn *tab )
{
    zmw_close( &tab->zmw );
    free_array_file( &tab->Basecall );
    free_array_file( &tab->QualityValue );
    free_array_file( &tab->DeletionQV );
    free_array_file( &tab->DeletionTag );
    free_array_file( &tab->InsertionQV );
    free_array_file( &tab->SubstitutionQV );
    free_array_file( &tab->SubstitutionTag );
}


rc_t open_BaseCalls_cmn( const KDirectory *hdf5_dir, BaseCalls_cmn *tab,
                         const bool num_passes, const char * path,
                         bool cache_content, bool supress_err_msg )
{
    rc_t rc;

    init_BaseCalls_cmn( tab );
    rc = zmw_open( hdf5_dir, &tab->zmw, num_passes, path, supress_err_msg );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->Basecall, path, "Basecall", 
                           BASECALL_BITSIZE, BASECALL_COLS,
                           true, cache_content, supress_err_msg );
    if ( rc == 0 )
        rc = open_element( hdf5_dir, &tab->QualityValue, path, "QualityValue", 
                           QUALITY_VALUE_BITSIZE, QUALITY_VALUE_COLS,
                           true, cache_content, supress_err_msg );
    if ( rc == 0 )
    {
        open_element( hdf5_dir, &tab->DeletionQV, path, "DeletionQV",
                           DELETION_QV_BITSIZE, DELETION_QV_COLS,
                           true, cache_content, true );

        open_element( hdf5_dir, &tab->DeletionTag, path, "DeletionTag", 
                           DELETION_TAG_BITSIZE, DELETION_TAG_COLS,
                           true, cache_content, true );

        open_element( hdf5_dir, &tab->InsertionQV, path, "InsertionQV", 
                           INSERTION_QV_BITSIZE, INSERTION_QV_COLS,
                           true, cache_content, true );

        open_element( hdf5_dir, &tab->SubstitutionQV, path, "SubstitutionQV",
                           SUBSTITUTION_QV_BITZISE, SUBSTITUTION_QV_COLS,
                           true, cache_content, true );

        open_element( hdf5_dir, &tab->SubstitutionTag, path, "SubstitutionTag",
                           SUBSTITUTION_TAG_BITSIZE, SUBSTITUTION_TAG_COLS,
                           true, cache_content, true );
    }
    if ( rc != 0 )
        close_BaseCalls_cmn( tab ); /* releases only initialized elements */
    return rc;
}
