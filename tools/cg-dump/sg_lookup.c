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

#include <os-native.h>
#include <sysalloc.h>

#include "sg_lookup.h"
#include "line_token_iter.h"
#include <klib/log.h>
#include <klib/out.h>
#include <kdb/meta.h>

#include <stdlib.h>


typedef struct sg_lookup
{
    BSTree spotgroups;
    const char * buffer;
    size_t buflen;
} sg_lookup;


rc_t make_sg_lookup( struct sg_lookup ** self )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
    {
        (*self) = calloc( 1, sizeof( sg_lookup ) );
        if ( *self == NULL )
            rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    return rc;
}


static int CC String_sg_cmp ( const void * item, const BSTNode * n )
{
    const String * name = ( const String * ) item;
    const sg * sg_item = ( const sg * ) n;
    return StringCompare ( &sg_item->name, name );
}


static int CC sg_sg_cmp ( const BSTNode * item, const BSTNode * n )
{
    const sg * sg1 = ( const sg * ) item;
    const sg * sg2 = ( const sg * ) n;
    return StringCompare ( &sg2->name, &sg1->name );
}


static sg * find_entry( struct sg_lookup * self, const String * name )
{
    return ( sg * )BSTreeFind ( &self->spotgroups, ( void * )name, String_sg_cmp );
}


static rc_t make_sg( String * name, String * field_size, String * lib, String * sample, sg ** entry )
{
    rc_t rc = 0;
    ( *entry ) = malloc( sizeof ** entry );
    if ( *entry == NULL )
    {
        rc = RC( rcExe, rcDatabase, rcReading, rcMemory, rcExhausted );
        (void)LOGERR( klogErr, rc, "memory exhausted when creating new spotgroup-lookup-entry" );
    }
    else
    {
        StringInit( ( String * )&( *entry )->name, name->addr, name->size, name->len );
        StringInit( ( String * )&( *entry )->field_size, field_size->addr, field_size->size, field_size->len );
        StringInit( ( String * )&( *entry )->lib, lib->addr, lib->size, lib->len );
        StringInit( ( String * )&( *entry )->sample, sample->addr, sample->size, sample->len );
    }
    return rc;
}


static rc_t parse_buffer( struct sg_lookup * self )
{
    struct buf_line_iter bli;
    bool valid = true;
    rc_t rc = buf_line_iter_init( &bli, self->buffer, self->buflen );
    while ( ( rc == 0 ) && valid )
    {
        String line;
        uint32_t line_nr;
        rc = buf_line_iter_get( &bli, &line, &valid, &line_nr );
        if ( rc == 0 && valid )
        {
            token_iter ti;
            bool ti_valid = true;
            String name, field_size, lib, sample;

            StringInit( &name, NULL, 0, 0 );
            StringInit( &field_size, NULL, 0, 0 );
            StringInit( &lib, NULL, 0, 0 );
            StringInit( &sample, NULL, 0, 0 );

            rc = token_iter_init( &ti, &line, '\t' );
            while ( ( rc == 0 ) && ti_valid )
            {
                String token;
                uint32_t token_nr;
                rc = token_iter_get( &ti, &token, &ti_valid, &token_nr );
                if ( rc == 0 && ti_valid )
                {
                    if ( token_nr == 0 && ( string_cmp( token.addr, token.len, "@RG", 3, 3 ) != 0 ) )
                        ti_valid = false;
                    else
                    {
                        token_iter sub_ti;
                        String *value_dst = NULL;
                        bool sub_ti_valid = true;
                        rc = token_iter_init( &sub_ti, &token, ':' );
                        while ( ( rc == 0 ) && sub_ti_valid )
                        {
                            String sub_token;
                            uint32_t sub_token_nr;
                            rc = token_iter_get( &sub_ti, &sub_token, &sub_ti_valid, &sub_token_nr );
                            if ( rc == 0 && sub_ti_valid )
                            {
                                if ( sub_token_nr == 0 )
                                {
                                    if ( string_cmp( sub_token.addr, sub_token.len, "ID", 2, 2 ) == 0 )
                                        value_dst = &name;
                                    else if ( string_cmp( sub_token.addr, sub_token.len, "DS", 2, 2 ) == 0 )
                                        value_dst = &field_size;
                                    else if ( string_cmp( sub_token.addr, sub_token.len, "LB", 2, 2 ) == 0 )
                                        value_dst = &lib;
                                    else if ( string_cmp( sub_token.addr, sub_token.len, "SM", 2, 2 ) == 0 )
                                        value_dst = &sample;
                                }
                                else if ( sub_token_nr == 1 )
                                {
                                    if ( value_dst != NULL )
                                        StringInit( value_dst, sub_token.addr, sub_token.size, sub_token.len );
                                }
                            }
                        }
                    }
                }
            }

            if ( name.addr != NULL && field_size.addr != NULL &&
                 lib.addr != NULL && sample.addr != NULL )
            {
                sg * entry = find_entry( self, &name );
                /* KOutMsg( "entry: name='%S' fs='%S' lib='%S' sample='%S'\n", &name, &field_size, &lib, &sample ); */
                if ( entry == NULL )
                {
                    rc = make_sg( &name, &field_size, &lib, &sample, &entry );
                    if ( rc == 0 )
                    {
                        rc = BSTreeInsert ( &self->spotgroups, ( BSTNode * )entry, sg_sg_cmp );
                        if ( rc != 0 )
                        {
                            (void)LOGERR( klogErr, rc, "cannot insert new spotgroup" );
                            free( entry );
                        }
                    }
                }
                else
                {
                    /* so far we ignore it if we find the same spotgroup-name twice in the meta-data */
                }
            }
        }
    }
    return rc;
}


rc_t parse_sg_lookup( struct sg_lookup * self, const VDatabase * db )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
    {
        const KMetadata *meta;
        rc_t rc = VDatabaseOpenMetadataRead ( db, &meta );
        if ( rc != 0 )
        {
            (void)LOGERR( klogErr, rc, "cannot open metdata on database" );
        }
        else
        {
            const KMDataNode *node;
            rc_t rc1 = KMetadataOpenNodeRead ( meta, &node, "BAM_HEADER" );
            if ( rc1 == 0 )
            {
                size_t num_read;
                /* explore how much data we must read... */
                rc = KMDataNodeRead ( node, 0, NULL, 0, &num_read, &self->buflen );
                if ( rc == 0 )
                {
                    if ( self->buffer != NULL )
                        free( (void *)self->buffer );

                    self->buffer = malloc( self->buflen + 1 );
                    if ( self->buffer != NULL )
                    {
                        size_t num_read2;
                        rc = KMDataNodeReadCString ( node, (char *)self->buffer, self->buflen + 1, &num_read2 );
                        if ( rc == 0 )
                            rc = parse_buffer( self );
                    }
                }
                KMDataNodeRelease ( node );
            }
            KMetadataRelease ( meta );
        }
    }
    return rc;
}


rc_t perform_sg_lookup( struct sg_lookup * self, const String * name, sg ** entry )
{
    rc_t rc = 0;
    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    else
    {
        ( *entry ) = find_entry( self, name );
    }
    return rc;
}


static void CC whack_spotgroup( BSTNode *n, void *data )
{
    free( ( void * )n );
}


rc_t destroy_sg_lookup( struct sg_lookup * self )
{
    rc_t rc = 0;

    if ( self == NULL )
        rc = RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
    else
    {
        BSTreeWhack ( &self->spotgroups, whack_spotgroup, NULL );
        free( (void *)self->buffer );
        free( self );
    }
    return rc;
}
