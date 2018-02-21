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
#include "prim-lookup.h"

#ifndef _h_cmn_
#include "cmn.h"
#endif

#ifndef _h_klib_file_
#include <kfs/file.h>
#endif

#ifndef _h_klib_hashtable_
#include <klib/hashtable.h>
#endif

#ifndef _h_klib_hashfile_
#include <klib/hashfile.h>
#endif

typedef struct prim_lookup
{
    KHashFile * hashfile;
    KDirectory * dir;
    KFile * f;
    const char * tmp_file;
} prim_lookup;

typedef struct hashfile_value
{
    uint64_t combined;
    uint64_t seq_spot_id;
} hashfile_value;

void destroy_prim_lookup( prim_lookup * self )
{
    if ( self != NULL )
    {
        if ( self -> hashfile != NULL )
            KHashFileDispose( self -> hashfile );

        if ( self -> f != NULL )
            KFileRelease( self -> f );

        if ( self -> tmp_file != NULL )
            KDirectoryRemove ( self -> dir, true, "%s", self -> tmp_file );
    
        if ( self -> dir != NULL )
            KDirectoryRelease( self -> dir );
            
        free( ( void * ) self );
    }
}

rc_t make_prim_lookup( prim_lookup ** lookup, KDirectory * dir, const char * tmp_file )
{
    rc_t rc = 0;
    prim_lookup * obj = calloc( 1, sizeof * obj );
    if ( obj == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
        ErrMsg( "make_prim_lookup().calloc( %d ) -> %R", ( sizeof * obj ), rc );
    }
    else
    {
        rc = KDirectoryAddRef( dir );
        if ( rc != 0 )
            ErrMsg( "make_prim_lookup().KDirectoryAddRef() -> %R", rc );
        else
            obj -> dir = dir;

        if ( rc == 0 )
        {
            obj -> tmp_file = tmp_file;
            rc = KDirectoryCreateFile( dir, &obj -> f, true, 0664, kcmInit | kcmParents, "%s", tmp_file );
            if ( rc != 0 )
                ErrMsg( "make_prim_lookup().KDirectoryCreateFile( '%s' ) -> %R", tmp_file, rc );
        }
        
        if ( rc == 0 )
        {
            rc = KHashFileMake( &obj -> hashfile, obj -> f );
            if ( rc != 0 )
                ErrMsg( "make_prim_lookup().KHashFileMake() -> %R", rc );
        }
        
        if ( rc != 0 )
            destroy_prim_lookup( obj );
        else
            *lookup = obj;
    }
    return rc;

}

rc_t prim_lookup_enter( prim_lookup * self, const prim_rec * rec )
{
    rc_t rc = 0;
    if ( self == NULL || rec == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "prim_lookup_enter() -> %R", rc );
    }
    else
    {
        /* the lookup will be made based on the primary-row-id as a key */
        uint64_t key = rec -> align_row_id;
        uint64_t key_hash = KHash( ( const char * )&key, sizeof key );
        hashfile_value value;
        
        value . combined = rec -> read_len;
        value . combined <<= 32;
        if ( rec -> ref_orient )
            value . combined |= 1;
        if ( rec -> seq_read_id == 2 )
            value . combined |= 2;
        value . seq_spot_id = rec -> seq_spot_id;
        
        rc = KHashFileAdd( self -> hashfile,
                           ( const void * )&key, sizeof key, key_hash,
                           ( const void * )&value, sizeof value );
        if ( rc != 0 )
            ErrMsg( "prim_lookup_enter().KHashFileAdd( key = %lu ) -> %R", key, rc );
    }
    return rc;
}

rc_t prim_lookup_get( prim_lookup * self,
                      uint64_t align_id,
                      lookup_entry * entry,
                      bool * found )
{
    rc_t rc = 0;
    if ( self == NULL || entry == NULL || found == NULL )
    {
        rc = RC( rcVDB, rcNoTarg, rcWriting, rcParam, rcNull );
        ErrMsg( "prim_lookup_enter() -> %R", rc );
    }
    else
    {
        uint64_t key = align_id;
        uint64_t key_hash = KHash( ( const char * )&key, sizeof key );
        hashfile_value value;
        size_t value_size;
        
        *found = KHashFileFind( self -> hashfile,
                                ( const void * )&key, sizeof key, key_hash,
                                ( void * )&value, &value_size );
        if ( *found )
        {
            if ( value_size == sizeof value )
            {
                entry -> ref_orient  = ( value . combined & 1 ) ? 1 : 0;
                entry -> seq_read_id = ( value . combined & 2 ) ? 2 : 1;
                entry -> read_len    = ( ( value . combined >> 32 ) & 0xFFFFFFFF );
                entry -> seq_spot_id = value . seq_spot_id;
            }
            else
            {
                rc = RC( rcVDB, rcNoTarg, rcValidating, rcParam, rcInvalid );
                ErrMsg( "make_prim_lookup().KHashFileFind( %u != %u ) -> %R", ( sizeof value ), value_size, rc );
                *found = false;
            }
        }
    }
    return rc;
}
