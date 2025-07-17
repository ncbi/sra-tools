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

#pragma once

#include <string>
#include <functional>
#include <set>
#include <exception>

#include <vdb.hpp>
#include <fingerprint.hpp>

#include <insdc/sra.h>
#include <kdb/manager.h>

class SraInfo
{
public:
    SraInfo();
    ~SraInfo();

    void SetAccession( const std::string& accession );
    const std::string& GetAccession() const { return m_accession; }

    // Platform
    typedef std::set<std::string> Platforms;
    Platforms GetPlatforms() const; // may be empty or more than 1 value

    // Spot structure
    typedef enum {
        Short,
        Abbreviated,
        Full,
        Verbose
    } Detail;
    struct ReadStructure
    {
        INSDC_read_type type = 0;
        uint32_t length = 0;

        ReadStructure() {}
        ReadStructure( INSDC_read_type t, uint32_t l );

        std::string Encode( Detail ) const;
        std::string TypeAsString( Detail = Verbose ) const;
    };
    struct ReadStructures : std::vector<ReadStructure> {};
    struct SpotLayout
    {
        uint64_t count = 0;
        ReadStructures reads;
    };
    typedef std::vector<SpotLayout> SpotLayouts; // sorted by descending count
    SpotLayouts GetSpotLayouts(
        Detail detail,
        bool useConsensus = true,
        uint64_t topRows = 0 ) const; // only use topRows rows of the table; 0 to use all

    // Alignment
    bool IsAligned() const;

    // Qualities
    bool HasPhysicalQualities() const;

    const VDB::SchemaInfo GetSchemaInfo() const;

    typedef std::unique_ptr< KDBContents, std::function<void(KDBContents*)> > Contents;
    Contents GetContents() const;

    bool HasLiteMetadata() const;

    char const *QualityDescription() const;

    struct TreeNode
    {
        typedef enum { Value, Element, Array } NodeType;

        std::string key;
        NodeType type;
        std::string value; // used only if type == Value
        std::vector<TreeNode> subnodes;  // used if type == Element or Array

        TreeNode( const std::string & p_key, const std::string & p_value )
        : key(p_key), type( Value ), value(p_value) {}
        TreeNode( const std::string & p_key, NodeType p_type )
        : key(p_key), type(p_type) {}
    };
    typedef std::vector< TreeNode > Fingerprints;
    Fingerprints GetFingerprints( Detail detail ) const;

private:
    bool isDatabase() const;
    bool isTable() const;
    bool hasTable(std::string const &name) const;
    VDB::MetadataCollection topLevelMetadataCollection() const;
    VDB::Table openSequenceTable(bool useConsensus = false) const;
    VDB::Schema openSchema() const;
    void releaseDataObject();

private:
    VDB::Manager m_mgr;
    std::string m_accession;
    VDB::Manager::PathType m_type;
    union {
        VDB::Database const *db;
        VDB::Table const *tbl;
    } m_u;
};
