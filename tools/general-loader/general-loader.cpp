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

#include "general-loader.hpp"

#include <klib/rc.h>
#include <klib/log.h>

#include <kns/stream.h>

#include <kfs/directory.h>

#include "general-writer.h"

using namespace std;

///////////// GeneralLoader::Reader

GeneralLoader::Reader::Reader( const struct KStream& p_input )
:   m_input ( p_input ),
    m_buffer ( 0 ),
    m_bufSize ( 0 ),
    m_readCount ( 0 )
{
    KStreamAddRef ( & m_input );
}

GeneralLoader::Reader::~Reader()
{
    KStreamRelease ( & m_input );
    free ( m_buffer );
}

rc_t 
GeneralLoader::Reader::Read( void * p_buffer, size_t p_size )
{
    pLogMsg ( klogDebug, 
             "general-loader: reading $(s) bytes, offset=$(o)", 
             "s=%u,o=%lu", 
             ( unsigned int ) p_size, m_readCount );

    m_readCount += p_size;
    return KStreamReadExactly ( & m_input, p_buffer, p_size );
}

rc_t 
GeneralLoader::Reader::Read( size_t p_size )
{
    if ( p_size > m_bufSize )
    {
        m_buffer = realloc ( m_buffer, p_size );
        if ( m_buffer == 0 )
        {
            m_bufSize = 0;
            m_readCount = 0;
            return RC ( rcExe, rcFile, rcReading, rcMemory, rcExhausted );
        }
    }
    
    pLogMsg ( klogDebug, "general-loader: reading $(s) bytes", "s=%u", ( unsigned int ) p_size );
    
    m_readCount += p_size;
    return KStreamReadExactly ( & m_input, m_buffer, p_size );
}

void 
GeneralLoader::Reader::Align( uint8_t p_bytes )
{
    if ( m_readCount % p_bytes != 0 )
    {
        Read ( p_bytes - m_readCount % p_bytes );
    }
}


///////////// GeneralLoader

GeneralLoader::GeneralLoader ( const std::string& p_programName, const struct KStream& p_input )
:   m_programName ( p_programName ),
    m_reader ( p_input )
{
}

GeneralLoader::~GeneralLoader ()
{
}

void 
GeneralLoader::SetTargetOverride( const std::string& p_path )
{
    m_targetOverride = p_path;
}

void
GeneralLoader::SplitAndAdd( Paths& p_paths, const string& p_path )
{
    size_t startPos = 0;
    size_t colonPos = p_path . find ( ':', startPos );
    while ( colonPos != string::npos )
    {
        p_paths . push_back ( p_path . substr ( startPos, colonPos - startPos ) );
        startPos = colonPos + 1;
        colonPos = p_path . find ( ':', startPos );    
    }
    p_paths . push_back ( p_path . substr ( startPos ) );
}

void 
GeneralLoader::AddSchemaIncludePath( const string& p_path )
{
    SplitAndAdd ( m_includePaths, p_path );
}

void 
GeneralLoader::AddSchemaFile( const string& p_path )
{
    SplitAndAdd ( m_schemas, p_path );
}

rc_t 
GeneralLoader::Run()
{
    bool packed;
    rc_t rc = ReadHeader ( packed );
    if ( rc == 0 ) 
    {
        DatabaseLoader loader ( m_programName, m_includePaths, m_schemas, m_targetOverride );
        if ( packed )
        {
            PackedProtocolParser p;
            rc = p . ParseEvents ( m_reader, loader );
        }
        else
        {
            UnpackedProtocolParser p;
            rc = p . ParseEvents ( m_reader, loader );
        }
    
        if ( rc != 0 && ! loader . GetDatabaseName() . empty () )
        {
            KDirectory* wd;
            KDirectoryNativeDir ( & wd );
            KDirectoryRemove ( wd, true, loader . GetDatabaseName() . c_str () );
            KDirectoryRelease ( wd );
        }
    }
    
    return rc;
}

rc_t 
GeneralLoader::ReadHeader ( bool& p_packed )
{
    struct gw_header_v1 header;

    rc_t rc = m_reader . Read ( & header, sizeof header );
    if ( rc == 0 )
    { 
        if ( strncmp ( header . dad . signature, GeneralLoaderSignatureString, sizeof ( header . dad . signature ) ) != 0 )
        {
            rc = RC ( rcExe, rcFile, rcReading, rcHeader, rcCorrupt );
        }
        else 
        {
            switch ( header . dad . endian )
            {
            case GW_GOOD_ENDIAN:
                if ( header . dad . version > GW_CURRENT_VERSION ) /* > comparison so it can read multiple versions */
                {
                    rc = RC ( rcExe, rcFile, rcReading, rcHeader, rcBadVersion );
                }
                else
                {
                    rc = 0;
                }
                break;
            case GW_REVERSE_ENDIAN:
                LogMsg ( klogErr, "general-loader event: Detected reverse endianness (not yet supported)" );
                rc = RC ( rcExe, rcFile, rcReading, rcFormat, rcUnsupported );
                //TODO: apply byte order correction before checking the version number
                break;
            default:
                rc = RC ( rcExe, rcFile, rcReading, rcFormat, rcInvalid );
                break;
            }
        }
    }
    
    if ( rc == 0 && header . dad . hdr_size > sizeof header ) 
    {   
        rc = m_reader . Read ( header . dad . hdr_size - sizeof header );
    }
    
    if ( rc == 0 )
    {
        p_packed = header. packing != 0;
    }
    
    return rc;
}
