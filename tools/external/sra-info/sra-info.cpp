/* ===========================================================================
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

#include <kdb/manager.h>
#include <kdb/kdb-priv.h>

#include <algorithm>
#include <map>

using namespace std;

/*
* SraInfo
*/

SraInfo::SraInfo()
{
    m_type = VDB::Manager::ptInvalid;
    m_u.db = nullptr;
}

void SraInfo::releaseDataObject() {
    switch (m_type) {
    case VDB::Manager::ptDatabase:
        delete m_u.db;
        break;
    case VDB::Manager::ptTable:
    case VDB::Manager::ptPrereleaseTable:
        delete m_u.tbl;
        break;
    default:
        assert(m_u.db == nullptr);
        break;
    }
    m_type = VDB::Manager::ptInvalid;
    m_u.db = nullptr;
}

SraInfo::~SraInfo() {
    releaseDataObject();
}

void
SraInfo::SetAccession( const std::string& p_accession )
{
    releaseDataObject();
    m_accession = p_accession;
    m_type = m_mgr.pathType(m_accession);

    switch (m_type) {
    case VDB::Manager::ptDatabase:
        m_u.db = new VDB::Database{ m_mgr.openDatabase(m_accession) };
        return;
    case VDB::Manager::ptTable:
        try {
            m_u.tbl = new VDB::Table { m_mgr.openTable(m_accession) };
            return;
        }
        catch (VDB::Error &err) {
            if ((int)err.getRc() != 1434782232) // if not "schema not found while opening table within virtual database module"
                throw;
            err.handled = true;
        }
        // fall through
    case VDB::Manager::ptPrereleaseTable :
        throw VDB::Error( (m_accession + ": obsolete data type").c_str() );
    default:
        throw VDB::Error( (m_accession + ": unknown data type").c_str() );
    }
}

bool SraInfo::isDatabase() const {
    return m_type == VDB::Manager::ptDatabase;
}

bool SraInfo::isTable() const {
    return m_type == VDB::Manager::ptTable;
}

bool SraInfo::hasTable(std::string const &name) const {
    if (!isDatabase()) return false;

    return m_u.db->hasTable(name);
}

VDB::Table
SraInfo::openSequenceTable(bool useConsensus) const
{
    if (isDatabase()) {
        auto const &db = *m_u.db;
        return (useConsensus && db.hasTable("CONSENSUS"))
            ? db["CONSENSUS"]
            : db["SEQUENCE"];
    }
    return *m_u.tbl;
}

VDB::Schema SraInfo::openSchema() const
{
    if (isDatabase())
        return m_u.db->openSchema();
    else
        return m_u.tbl->openSchema();
}

static
string
PlatformToString( const uint32_t id )
{
    static const char *platform_symbolic_names[] = { INSDC_SRA_PLATFORM_SYMBOLS };
    if ( id < sizeof ( platform_symbolic_names ) / sizeof( *platform_symbolic_names ) )
    {
        return platform_symbolic_names[ id ];
    }

    return platform_symbolic_names[ SRA_PLATFORM_UNDEFINED ];
}

SraInfo::Platforms
SraInfo::GetPlatforms() const
{
    Platforms ret;

    auto const table = openSequenceTable();
    try
    {
        table.read({ "PLATFORM" })
        .foreach([&](VDB::Cursor::RowID /*row*/, const vector<VDB::Cursor::RawData>& values) {
            ret.insert( PlatformToString( values[0].valueOr<uint8_t>(SRA_PLATFORM_UNDEFINED) ) );
        });
    }
    catch(VDB::Error & e)
    {
        rc_t rc = e.getRc();
        if ( rc != 0 )
        {   // if the column is not found, return UNDEFINED
            if ( GetRCObject( rc ) == (enum RCObject)rcColumn && GetRCState( rc ) == rcUndefined )
            {
                ret.insert( PlatformToString( SRA_PLATFORM_UNDEFINED ) );
                e.handled = true;
                return ret;
            }
        }
        throw;
    }

    if ( ret.size() == 0 )
    {
        ret.insert( "none provided" );
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

SraInfo::SpotLayouts // sorted by descending count
SraInfo::GetSpotLayouts(
    Detail detail,
    bool useConsensus,
    uint64_t topRows ) const
{
    map< ReadStructures, size_t > rs_map;
    SpotLayouts ret;

    auto const &table = openSequenceTable(useConsensus);
    auto const &cursor = table.read( { "READ_TYPE", "READ_LEN", "SPOT_ID" } );
    auto handle_row = [&](VDB::Cursor::RowID /*row*/, const vector<VDB::Cursor::RawData>& values)
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
    if ( topRows == 0 )
    {   // all rows
        cursor.foreach( handle_row );
    }
    else
    {   // read at most topRows
        auto const range = cursor.rowRange();
        size_t count = range.second - range.first;
        if ( count > topRows )
        {
            count = topRows;
        }

        std::vector< VDB :: Cursor :: RawData > data;
        data.resize( cursor.columns() );

        for (size_t i = 0; i < count; ++i)
        {
            auto rowId = range.first + i;
            for ( unsigned int j = 0; j < cursor.columns(); ++j)
            {
                data[ j ] = cursor.read( rowId, j );
            }
            handle_row( rowId, data );
        }
    }

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
{ // is a database and has a primary alignment table with at least one row.
    auto const PrimaryAlignmentTable = std::string{"PRIMARY_ALIGNMENT"};
    return isDatabase() && m_u.db->hasTable(PrimaryAlignmentTable)
        && (*m_u.db)[PrimaryAlignmentTable]
            .read({"ALIGNMENT_COUNT"})
            .rowRange().first > 0;
}

bool
SraInfo::HasPhysicalQualities() const
{   // QUALITY column is readable, either QUALITY or ORIGINAL_QUALITY is physical
    VDB::Table const &table = openSequenceTable();
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

const VDB::SchemaInfo SraInfo::GetSchemaInfo(void) const {
    return openSchema().GetInfo();
}

SraInfo::Contents
SraInfo::GetContents() const
{
    const KDBManager * kdb;
    rc_t rc = KDBManagerMakeRead ( &kdb, nullptr );
    if ( rc != 0 )
    {
        throw VDB::Error( "KDBManagerMakeRead() failed", __FILE__, __LINE__);
    }

    const KDBContents * ret;
    rc = KDBManagerPathContents( kdb, & ret, lod_Full, m_accession.c_str() );
    KDBManagerRelease( kdb );
    if ( rc != 0 )
    {
        KDBContentsWhack( ret );
        throw VDB::Error( (m_accession + ": not a valid VDB object").c_str(), __FILE__, __LINE__);
    }
    return Contents(const_cast<KDBContents *>( ret ), []( KDBContents *c ) { KDBContentsWhack( c ); }  );
}

VDB::MetadataCollection SraInfo::topLevelMetadataCollection() const
{
    if (isDatabase())
        return m_u.db->metadata();
    else
        return m_u.tbl->metadata();
}

static bool isLiteMetadata(VDB::MetadataCollection const &meta) {
    try {
        return meta.childNode("SOFTWARE/delite").attributeValue("name") == "delite";
    }
    catch (VDB::Error &rc) { rc.handled = true; }
    return false;
}

bool SraInfo::HasLiteMetadata() const {
    return isLiteMetadata(topLevelMetadataCollection());
}

char const *SraInfo::QualityDescription() const {
    return HasPhysicalQualities() ? "STORED"
         : HasLiteMetadata() ? "REMOVED"
         : "NONE";
}

// convert a string containing a uint64_t to a string representing the same value as a decimal
static
string
U64StringToDecString( const string & encoded )
{
    assert( encoded.size() == sizeof(uint64_t) );
    uint64_t v = *(const uint64_t*) encoded.data();
    ostringstream ret;
    ret << v;
    return ret.str();
}

SraInfo::Fingerprints
SraInfo::GetFingerprints( Detail detail ) const
{
    Fingerprints ret;

    if ( ! isDatabase() || ! m_u.db->hasTable( "SEQUENCE" ))
    {
        return ret;
    }

    VDB::Table seq = (*m_u.db)["SEQUENCE"];
    VDB::MetadataCollection seqmeta = seq.metadata();

    if ( ! seqmeta.hasChildNode("QC"))
    {   // no fingerprint info in this database
        return ret;
    }

    // current output fp
    ret.push_back( TreeNode( "fingerprint", seqmeta["QC/current/fingerprint"].value() ) );
    ret.push_back( TreeNode( "digest",      seqmeta["QC/current/digest"].value()) );
    ret.push_back( TreeNode( "algorithm",   seqmeta["QC/current/algorithm"].value()) );

    if ( detail > Short )
    {
        ret.push_back( TreeNode( "timestamp",   U64StringToDecString( seqmeta["QC/current/timestamp"].value() ) ) );
        ret.push_back( TreeNode( "version",     seqmeta["QC/current/version"].value()) );
        ret.push_back( TreeNode( "format",      seqmeta["QC/current/format"].value()) );

        // history of the output fp updates (optional)
        if ( seqmeta.hasChildNode("QC/history") )
        {
            VDB::Metadata seq_history = seqmeta.childNode("QC/history");
            VDB::NameList updates = seq_history.children();

            TreeNode history { "history", TreeNode::Array };
            const string start { "update_" };
            for ( unsigned int i = 0; i < updates.count(); ++ i)
            {
                const string& name = updates[i];
                if ( name.compare(0, start.size(), start) == 0 )
                {
                    TreeNode h { name, TreeNode::Element };

                    h.subnodes.push_back( TreeNode ( "update", to_string( i + 1 ) ) );

                    h.subnodes.push_back( TreeNode ( "fingerprint", seq_history[ name ] [ "fingerprint" ].value() ) );
                    h.subnodes.push_back( TreeNode ( "digest",      seq_history[ name ] [ "digest" ].value() ) );
                    h.subnodes.push_back( TreeNode ( "algorithm",   seq_history[ name ] [ "algorithm"].value() ) );
                    h.subnodes.push_back( TreeNode ( "timestamp",   U64StringToDecString ( seq_history[ name ] [ "timestamp"].value() ) ) );
                    h.subnodes.push_back( TreeNode ( "version",     seq_history[ name ] [ "version"].value() ) );
                    h.subnodes.push_back( TreeNode ( "format",      seq_history[ name ] [ "format"].value() ) );

                    history.subnodes.push_back( h );
                }
            }
            if ( ! history.subnodes.empty() )
            {
                ret.push_back( history );
            }
        }

        if ( detail > Abbreviated )
        {   // input fp(s)
            VDB::MetadataCollection dbmeta = m_u.db -> metadata();
            if ( dbmeta.hasChildNode( "LOAD/QC" ) )
            {
                VDB::Metadata db_inputs = dbmeta.childNode("LOAD/QC");
                VDB::NameList children = db_inputs.children();

                TreeNode inputs { "inputs", TreeNode::Array };
                const string start { "file_" };
                for ( unsigned int i = 0; i < children.count(); ++ i)
                {
                    const string& name = children[i];
                    if ( name.compare(0, start.size(), start) == 0 )
                    {
                        TreeNode in { name, TreeNode::Element };

                        in.subnodes.push_back( TreeNode ( "file", to_string( i + 1 ) ) );

                        in.subnodes.push_back( TreeNode( "name",        db_inputs [ name ] . attributeValue("name") ) );
                        in.subnodes.push_back( TreeNode( "fingerprint", db_inputs [ name ] . value() ) );
                        in.subnodes.push_back( TreeNode( "algorithm",   db_inputs [ name ] . attributeValue("algorithm") ) );
                        in.subnodes.push_back( TreeNode( "digest",      db_inputs [ name ] . attributeValue("digest") ) );
                        in.subnodes.push_back( TreeNode( "version",      db_inputs [ name ] . attributeValue("version") ) );
                        in.subnodes.push_back( TreeNode( "format",      db_inputs [ name ] . attributeValue("format") ) );

                        inputs.subnodes.push_back( in );
                    }
                }
                if ( ! inputs.subnodes.empty() )
                {
                    ret.push_back( inputs );
                }
            }
        }
    }

    return ret;
}