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

bool operator < (const SraInfo::ReadStructure& a, const SraInfo::ReadStructure& b)
{
    if (a.type < b.type) return true;
    if (a.type > b.type) return false;
    if (a.length < b.length) return true;
    if (a.length > b.length) return false;
	return false;
}

bool operator < (const SraInfo::ReadStructures& a, const SraInfo::ReadStructures& b)
{
	if (a.size() < b.size()) return true;
	if (a.size() > b.size()) return false;
	auto it_a = a.begin();
	auto it_b = b.begin();
	while (it_a != a.end())
	{
		if (*it_a < *it_b) return true;
		if (*it_b < *it_a) return false;
		++it_a;
		++it_b;
	}
	return false;
}

SraInfo::ReadStructure::ReadStructure( INSDC_read_type t, uint32_t l )
: type( t ), length( l )
{
}

string
SraInfo::ReadStructure::TypeAsString( Detail detail) const
{
    string ret;
    switch ( detail )
    {
    case Short: return string();
    case Abbreviated:
        switch ( type )
        {
        case 0:
        case 2:
        case 4: return "T";
        case 1:
        case 3:
        case 5: return "B";
        default:
            throw VDB::Error( "SraInfo::ReadStructure::TypeAsString(): invalid read type", __FILE__, __LINE__);
        }
    case Full:
        switch ( type )
        {
        case 0: return "T";
        case 1: return "B";
        case 2: return "TF";
        case 3: return "BF";
        case 4: return "TR";
        case 5: return "BR";
        default:
            throw VDB::Error( "SraInfo::ReadStructure::TypeAsString(): invalid read type", __FILE__, __LINE__);
        }
    case Verbose:
        switch ( type )
        {
        case 0: return "TECHNICAL";
        case 1: return "BIOLOGICAL";
        case 2: return "TECHNICAL|FORWARD";
        case 3: return "BIOLOGICAL|FORWARD";
        case 4: return "TECHNICAL|REVERSE";
        case 5: return "BIOLOGICAL|REVERSE";
        default:
            throw VDB::Error( "SraInfo::ReadStructure::TypeAsString(): invalid read type", __FILE__, __LINE__);
        }
    default:
        throw VDB::Error( "SraInfo::GetSpotLayouts(): unexpected detail level", __FILE__, __LINE__);
    }
}

string
SraInfo::ReadStructure::Encode( Detail detail ) const
{
    switch( detail )
    {
    case Short:
    case Abbreviated: return TypeAsString( detail );
    case Full:
    {
        ostringstream ret;
        ret << TypeAsString( detail ) << length;
        return ret.str();
    }
    case Verbose:
    {
        ostringstream ret;
        ret << TypeAsString( detail ) << "(length=" << length << ")";
        return ret.str();
    }
    default:
        throw VDB::Error( "SraInfo::GetSpotLayouts(): unexpected detail level", __FILE__, __LINE__);
    }
}

SraInfo::SpotLayouts
SraInfo::GetSpotLayouts( Detail detail, bool useConsensus ) const // sorted by descending count
{
    map< ReadStructures, size_t > rs_map;
    SpotLayouts ret;

    VDB::Table table;
    if ( mgr.pathType( m_accession ) == VDB::Manager::ptDatabase && useConsensus )
    {
        const char * CONSENSUS_TABLE = "CONSENSUS";
        VDB::Database db = mgr.openDatabase( m_accession );
        if ( db.hasTable( CONSENSUS_TABLE ) )
        {
            table = mgr.openDatabase( m_accession )[CONSENSUS_TABLE];
        }
        else
        {
            table = mgr.openDatabase( m_accession )["SEQUENCE"];
        }
    }
    else
    {
        table = openSequenceTable( m_accession );
    }

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
            ReadStructure rs;
            switch (detail)
            {
            case Verbose:
            case Full:
                rs.type = *i_types;
                rs.length = *i_lengths;
                break;
            case Abbreviated: // ignore read lengths
                rs.type = *i_types;
                break;
            case Short: // ignore read types and lengths
                break;
            default:
                throw VDB::Error( "SraInfo::GetSpotLayouts(): unexpected detail level", __FILE__, __LINE__);
            }

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
          []( const SpotLayout & a, const SpotLayout & b )
          { // more popular layouts sort first
            return a.count > b.count || (a.count == b.count && b.reads < a.reads);
          }
    );
    return ret;
}

bool
SraInfo::IsAligned() const
{
    if ( mgr.pathType( m_accession ) == VDB::Manager::ptDatabase )
    {   // aligned if there is a non-empty alignment table
        VDB::Database db = mgr.openDatabase(m_accession);
        const string PrimaryAlignmentTable = "PRIMARY_ALIGNMENT";
        if (db.hasTable(PrimaryAlignmentTable))
        {
            VDB::Table alignment = db[PrimaryAlignmentTable];
            VDB::Cursor cursor = alignment.read( { "ALIGNMENT_COUNT" } );
            return cursor.rowRange().first > 0;
        }
    }
    return false;
}

bool
SraInfo::HasPhysicalQualities() const
{   // QUALITY column is readable, either QUALITY or ORIGINAL_QUALITY is physical
    VDB::Table table = openSequenceTable( m_accession );
    const string QualityColumn = "QUALITY";
    VDB::Table::ColumnNames readable = table.readableColumns();
    if ( find( readable.begin(), readable.end(), QualityColumn ) != readable.end() )
    {
        const string OriginalQualityColumn = "ORIGINAL_QUALITY";
        VDB::Table::ColumnNames physical = table.physicalColumns();
        if ( find( physical.begin(), physical.end(), OriginalQualityColumn ) != physical.end() )
        {
            return true;
        }
        if ( find( physical.begin(), physical.end(), QualityColumn ) != physical.end() )
        {
            return true;
        }
    }
    return false;
}

const string SraInfo::GetSchemaVersion(void) const {
    return "schema";
}
