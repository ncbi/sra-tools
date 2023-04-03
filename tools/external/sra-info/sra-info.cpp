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

#include "sra-info.hpp"

#include <sstream>
#include <memory>

#include <vdb/table.h>
#include <vdb/database.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <vdb/manager.h>

#include <kdb/manager.h>

#include <klib/printf.h>

#include <insdc/sra.h>

using namespace std;

/*
* SraInfo::Error
*/

SraInfo::Error::Error( rc_t rc, const char* accession, const char * message )
{
    stringstream out;
    out << accession << ": " << message;
    if ( rc != 0 )
    {
        // obtain the required size
        size_t num_writ;
        string_printf ( nullptr, 0, &num_writ, "%R", rc );

        unique_ptr<char> rc_text ( new char[num_writ] );
        string_printf ( rc_text.get(), num_writ, nullptr, "%R", rc );
        out << ", rc = " << rc_text.get();
    }
    m_text = out.str();
}

SraInfo::Error::Error( const char * message )
{
    m_text = message;
}

SraInfo::Error::Error( const string & message )
{
    m_text = message;
}

const char*
SraInfo::Error::what() const noexcept
{
    return m_text.c_str();
}

/*
* SraInfo
*/

SraInfo::SraInfo()
{
}

void
SraInfo::SetAccession( const std::string& p_accession )
{
    m_accession = p_accession;
}

static
const VTable *
openSequenceTable( const char * accession )
{
    KDirectory * cur_dir = nullptr;
    const VDBManager * manager = nullptr;
    const VDatabase * database = nullptr;
    const VTable * ret = nullptr;

    try
    {
        rc_t rc = KDirectoryNativeDir( &cur_dir );
        if ( rc != 0 )
        {
            throw SraInfo::Error( rc, accession, "KDirectoryNativeDir() failed");
        }
        else
        {
            rc = VDBManagerMakeRead ( &manager, cur_dir );
            if ( rc != 0 )
            {
                throw SraInfo::Error( rc, accession, "VDBManagerMakeRead() failed");
            }

            int pt = ( VDBManagerPathType( manager, "%s", accession ) & ~ kptAlias );
            switch ( pt )
            {
                case kptDatabase :
                {
                    rc = VDBManagerOpenDBRead( manager, &database, NULL, "%s", accession );
                    if ( rc != 0 )
                    {
                        throw SraInfo::Error( rc, accession, "VDBManagerOpenDBRead() failed");
                    }
                    rc = VDatabaseOpenTableRead( database, & ret, "%s", "SEQUENCE" );
                    if ( rc != 0 )
                    {
                        throw SraInfo::Error( rc, accession, "VDatabaseOpenTableRead() failed");
                    }
                    break;
                }

                case kptTable :
                {
                    rc = VDBManagerOpenTableRead( manager, & ret, NULL, "%s", accession );
                    if ( rc != 0 )
                    {
                        throw SraInfo::Error( rc, accession, "VDBManagerOpenTableRead() failed");
                    }
                    break;
                }

                case kptPrereleaseTbl :
                    throw SraInfo::Error( rc, accession, "VDBManagerPathType(): unsupported path type");

                default :
                    throw SraInfo::Error( rc, accession, "VDBManagerPathType() returned invalid data");
            }
        }
    }
    catch(const std::exception& e)
    {
        VTableRelease( ret );
        VDatabaseRelease( database );
        VDBManagerRelease( manager );
        KDirectoryRelease( cur_dir );
        throw;
    }

    VDatabaseRelease( database );
    VDBManagerRelease( manager );
    KDirectoryRelease( cur_dir );
    return ret;
}

const char *vdcd_get_platform_txt( const uint32_t id )
{
#define CASE( id ) \
    case id : return # id; break

    switch( id )
    {
        CASE ( SRA_PLATFORM_UNDEFINED );
        CASE ( SRA_PLATFORM_454 );
        CASE ( SRA_PLATFORM_ILLUMINA );
        CASE ( SRA_PLATFORM_ABSOLID );
        CASE ( SRA_PLATFORM_COMPLETE_GENOMICS );
        CASE ( SRA_PLATFORM_HELICOS );
        CASE ( SRA_PLATFORM_PACBIO_SMRT );
        CASE ( SRA_PLATFORM_ION_TORRENT );
        CASE ( SRA_PLATFORM_CAPILLARY );
        CASE ( SRA_PLATFORM_OXFORD_NANOPORE );
    }
#undef CASE

    return "unknown platform";
}

SraInfo::Platforms
SraInfo::GetPlatforms() const
{
    Platforms ret;

    const VTable * table = nullptr;
    const VCursor * cursor = nullptr;

    try
    {
        table = openSequenceTable( m_accession.c_str() );

        bool col_exists = false;
        { /* check if the column PLATFORM exists to avoid a scary log message "failed to resolve" from VCursorOpen */
            KNamelist *names = NULL;
            rc_t rc = VTableListCol( table, & names);
            if ( rc != 0 )
            {
                throw SraInfo::Error( rc, m_accession.c_str(), "VTableListCol() failed");
            }
            col_exists = KNamelistContains( names, "PLATFORM" );
            rc = KNamelistRelease( names );
            if ( rc != 0 )
            {
                throw SraInfo::Error( rc, m_accession.c_str(), "KNamelistRelease() failed");
            }
        }

        if (col_exists )
        {
            rc_t rc = VTableCreateCursorRead( table, &cursor );
            if ( rc != 0 )
            {
                throw SraInfo::Error( rc, m_accession.c_str(), "VTableCreateCursorRead() failed");
            }

            uint32_t col_idx;
            rc = VCursorAddColumn ( cursor, &col_idx, "PLATFORM" );
            if ( rc != 0 )
            {
                throw SraInfo::Error( rc, m_accession.c_str(), "VCursorAddColumn() failed");
            }

            rc = VCursorOpen(cursor);
            if ( rc != 0 )
            {   // if the column is not found, return empty set
                if ( ! ( GetRCObject( rc ) == (enum RCObject)rcColumn && GetRCState( rc ) == rcUndefined ) )
                {
                    throw SraInfo::Error( rc, m_accession.c_str(), "VCursorOpen() failed");
                }
            }
            else
            {
                bool is_static;
                rc = VCursorIsStaticColumn ( cursor, col_idx, & is_static );
                if ( rc != 0 )
                {
                    throw SraInfo::Error( rc, m_accession.c_str(), "VCursorIsStaticColumn() failed");
                }

                if ( is_static )
                {
                    rc = VCursorOpenRow( cursor );
                    if ( rc != 0 )
                    {
                        throw SraInfo::Error( rc, m_accession.c_str(), "VCursorOpenRow() failed");
                    }

                    uint32_t p = 0;
                    uint32_t row_len;
                    rc = VCursorRead ( cursor, col_idx, 8, &p, sizeof(p), &row_len );
                    if ( rc != 0 || row_len != 1 )
                    {
                        throw SraInfo::Error( rc, m_accession.c_str(), "VCursorRead() failed");
                    }

                    // we do not seem to need VCursorCloseRow()
                    ret.insert( vdcd_get_platform_txt(p) );
                }
                else
                {
                    while( true )
                    {
                        rc = VCursorOpenRow( cursor );
                        if ( rc != 0 )
                        {
                            throw SraInfo::Error( rc, m_accession.c_str(), "VCursorOpenRow() failed");
                        }

                        uint32_t p = 0;
                        uint32_t row_len;
                        rc = VCursorRead ( cursor, col_idx, 8, &p, sizeof(p), &row_len );
                        if ( rc != 0 )
                        {
                            if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
                            {   // end of data
                                break;
                            }
                            throw SraInfo::Error( rc, m_accession.c_str(), "VCursorRead() failed");
                        }
                        if ( row_len != 1 )
                        {
                            throw SraInfo::Error( rc, m_accession.c_str(), "VCursorRead() returned invalid value for PLATFORM");
                        }

                        ret.insert( vdcd_get_platform_txt(p) );

                        rc = VCursorCloseRow( cursor );
                        if ( rc != 0 )
                        {
                            throw SraInfo::Error( rc, m_accession.c_str(), "VCursorCloseRow() failed");
                        }
                    }
                }
            }
        }
    }
    catch(const std::exception& e)
    {
        VCursorRelease( cursor );
        VTableRelease( table );
        throw;
    }

    VCursorRelease( cursor );
    VTableRelease( table );
    return ret;
}
