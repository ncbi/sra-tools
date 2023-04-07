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

#include <algorithm>
#include <map>

#include <insdc/sra.h>

using namespace std;

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

VDB::Table
SraInfo::openSequenceTable( const string & accession ) const
{
    switch ( mgr.pathType( accession ) )
    {
        case VDB::Manager::ptDatabase :
            return mgr.openDatabase(accession)["SEQUENCE"];

        case VDB::Manager::ptTable :
            return mgr.openTable(accession);

        case VDB::Manager::ptPrereleaseTable :
            throw VDB::Error( (accession + ": VDB::Manager::pathType(): unsupported path type").c_str(), __FILE__, __LINE__);

        default :
            throw VDB::Error( (accession + ": VDB::Manager::pathType(): returned invalid data").c_str(), __FILE__, __LINE__);
    }
}

static
string
PlatformToString( const uint32_t id )
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

    VDB::Table table = openSequenceTable( m_accession );
    try
    {
        VDB::Cursor cursor = table.read( { "PLATFORM" } );
        if ( cursor.isStaticColumn( 0 ) )
        {
            VDB::Cursor::RawData rd = cursor.read( 1, 0 );
            ret.insert( PlatformToString( rd.value<uint8_t>() ) );
        }
        else
        {
            auto get_platform = [&](VDB::Cursor::RowID row, const vector<VDB::Cursor::RawData>& values )
            {
                ret.insert( PlatformToString( values[0].value<uint8_t>() ) );
            };
            cursor.foreach( get_platform );
        }
    }
    catch(const VDB::Error & e)
    {
        rc_t rc = e.getRc();
        if ( rc != 0 )
        {   // if the column is not found, return UNDEFINED
            if ( GetRCObject( rc ) == (enum RCObject)rcColumn && GetRCState( rc ) == rcUndefined )
            {
                ret.insert( PlatformToString( SRA_PLATFORM_UNDEFINED ) );
                return ret;
            }
        }
        throw;
    }

    if ( ret.size() == 0 )
    {
        ret.insert( PlatformToString( SRA_PLATFORM_UNDEFINED ) );
    }
    return ret;
}

bool operator < ( const SraInfo::ReadStructures& a, const SraInfo::ReadStructures& b)
{
    if ( a.size() < b.size() ) return true;
    auto it_a = a.begin();
    auto it_b = b.begin();
    while ( it_a != a.end() )
    {
        if ( it_b == b.end() ) return false;
        if ( it_a->type < it_b->type || it_a->length < it_b->length ) return true;
        if ( it_a->type > it_b->type || it_a->length > it_b->length ) return false;
        ++it_a;
        ++it_b;
    }
    return it_b != b.end();
}

static
string
ReadTypeToString( INSDC_read_type value )
{
    string ret;
    switch ( value )
    {
    case 0: return "TECHNICAL";
    case 1: return "BIOLOGICAL";
    case 2: return "TECHNICAL|FORWARD";
    case 3: return "BIOLOGICAL|FORWARD";
    case 4: return "TECHNICAL|REVERSE";
    case 5: return "BIOLOGICAL|REVERSE";
    default: return "<invalid read type>";
    }
}

SraInfo::SpotLayouts
SraInfo::GetSpotLayouts() const // sorted by descending count
{
    map< ReadStructures, size_t > rs_map;
    SpotLayouts ret;

    VDB::Table table = openSequenceTable( m_accession );

    VDB::Cursor cursor = table.read( { "READ_TYPE", "READ_LEN" } );
    auto handle_row = [&](VDB::Cursor::RowID row, const vector<VDB::Cursor::RawData>& values )
    {
        vector<INSDC_read_type> types = values[0].asVector<INSDC_read_type>();
        vector<uint32_t> lengths = values[1].asVector<uint32_t>();
        assert(types.size() == lengths.size());
        ReadStructures r;
        r.reserve(types.size());
        auto i_types = types.begin();
        auto i_lengths = lengths.begin();
        while ( i_types != types.end() )
        {
            ReadStructure rs= { ReadTypeToString( *i_types ), *i_lengths };
            r.push_back( rs );
            ++i_types;
            ++i_lengths;
        }

        auto elem = rs_map.find( r );
        if ( elem != rs_map.end() )
        {
            (*elem).second++;
        }
        else
        {
            rs_map[r] = 1;
        }
    };
    cursor.foreach( handle_row );

    for( auto it = rs_map.begin(); it != rs_map.end(); ++it )
    {
        SpotLayout sl;
        sl.count = it->second;
        sl.reads = it->first;
        ret.push_back( sl );
    }
    sort( ret.begin(),
          ret.end(),
          []( const SpotLayout & a, const SpotLayout & b ) { return a.count > b.count; } // more popular layouts sort first
    );
    return ret;
}

bool
SraInfo::IsAligned() const
{
    try
    {
        if ( mgr.pathType( m_accession ) == VDB::Manager::ptDatabase )
        {   // aligned if there is a non-empty alignment table
            VDB::Table alignment = mgr.openDatabase(m_accession)["PRIMARY_ALIGNMENT"];
            VDB::Cursor cursor = alignment.read( { "ALIGNMENT_COUNT" } );
            return cursor.rowRange().first > 0;
        }
    }
    catch(const VDB::Error &)
    {   // assume the alignment table does not exist
    }
    return false;
}

bool
SraInfo::HasPhysicalQualities() const
{   // QUALITY column is readable, either QUALITY or ORIGINAL_QUALITY is physical
    VDB::Table table = openSequenceTable( m_accession );
    const string QualityColumn = "QUALITY";
    VDB::Table::ColumnNames cols = table.readableColumns();
    if ( find( cols.begin(), cols.end(), QualityColumn ) == cols.end() )
    {
        return false;
    }

    cols = table.physicalColumns();
    if ( find( cols.begin(), cols.end(), QualityColumn ) != cols.end() )
    {
        return true;
    }
    const string OriginalQualityColumn = "ORIGINAL_QUALITY";
    return find( cols.begin(), cols.end(), OriginalQualityColumn ) != cols.end();
}