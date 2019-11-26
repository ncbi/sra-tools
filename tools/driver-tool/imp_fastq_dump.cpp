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

#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <ncbi/secure/string.hpp>

#include "cmdline.hpp"
#include "support2.hpp"

namespace sratools2
{

struct FastqParams : ToolOptions
{
    ncbi::String accession_replacement;
    ncbi::String table_name;
    ncbi::String read_filter;
    ncbi::String aligned_region;
    ncbi::String matepair_dist;
    std::vector < ncbi::String > spot_groups;
    bool split_spot, clip, qual_filter, qual_filter1;
    bool aligned, unaligned, skip_tech;
    ncbi::U64 minSpotId;
    ncbi::U32 minSpotIdCount;
    ncbi::U64 maxSpotId;
    ncbi::U32 maxSpotIdCount;
    ncbi::U32 minReadLen;
    ncbi::U32 minReadLenCount;
    
    FastqParams() : accession_replacement( "" )
        , table_name( "" )
        , read_filter( "" )
        , split_spot( false )
        , clip( false )
        , qual_filter( false )
        , qual_filter1( false )
        , aligned( false )
        , unaligned( false )
        , skip_tech( false )
        , minSpotId( 0 )
        , minSpotIdCount( 0 )
        , maxSpotId( 0 )
        , maxSpotIdCount( 0 )
        , minReadLen( 0 )
        , minReadLenCount( 0 )

    {}

    void add_options( ncbi::Cmdline &cmdline )
    {
        cmdline . addOption ( accession_replacement, nullptr, "A", "accession", "<accession>",
            "Replaces accession derived from <path> in filename(s) and deflines ( only for single table dump)" );

        cmdline . addOption ( table_name, nullptr, "", "table", "<table-name>",
            "Table name within cSRA object, default is \"SEQUENCE\"" );

        cmdline . addOption ( split_spot, "", "split-spot", "Split spots into individual reads" );

        cmdline . addOption ( minSpotId, &minSpotIdCount, "N", "minSpotId", "<rowid>", "Minimum spot id" );
        cmdline . addOption ( maxSpotId, &maxSpotIdCount, "X", "maxSpotId", "<rowid>", "Maximum spot id" );

        cmdline . addListOption( spot_groups, ',', 255, "", "spot-groups", "<[list]>",
            "Filter by SPOT_GROUP (member): name[,...]" );

        cmdline . addOption ( clip, "W", "clip", "Remove adapter sequences from reads" );

        cmdline . addOption ( minReadLen, &minReadLenCount, "M", "minReadLen", "<len>",
            "Filter by sequence length >= <len>" );

        cmdline . addOption ( read_filter, nullptr, "R", "read-filter", "<[filter]>",
            "Split into files by READ_FILTER value, optionally filter by value: [pass|reject|criteria|redacted]" );

        cmdline . addOption ( qual_filter, "E", "qual-filter",
            "Filter used in early 1000 Genomes data: no sequences starting or ending with >= 10N" );

        cmdline . addOption ( qual_filter1, "", "qual-filter-1",
            "Filter used in current 1000 Genomes data" );

        cmdline . addOption ( aligned, "", "aligned", "Dump only aligned sequences" );
        cmdline . addOption ( unaligned, "", "unaligned", "Dump only unaligned sequences" );

        cmdline . addOption ( aligned_region, nullptr, "", "aligned-region", "<name[:from-to]>",
            "Filter by position on genome. Name can eiter by accession.version (ex: NC_000001.10) or file specific name (ex: \"chr1\" or \"1\". \"from\" and \"to\" are 1-based coordinates" );

        cmdline . addOption ( matepair_dist, nullptr, "", "matepair_distance", "<from-to|unknown>",
            "Filter by distance between matepairs. Use \"unknown\" to find matepairs split between the references. Use from-to to limit matepair distance on the same reference" );

        cmdline . addOption ( skip_tech, "", "skip-technical", "Dump only biological reads" );
    }

    std::string as_string()
    {
        std::stringstream ss;
        if ( !accession_replacement.isEmpty() )  ss << "acc-replace : " << accession_replacement << std::endl;
        if ( !table_name.isEmpty() )  ss << "table-name : " << table_name << std::endl;
        if ( split_spot ) ss << "split-spot" << std::endl;
        if ( minSpotIdCount > 0 ) ss << "minSpotId : " << minSpotId << std::endl;
        if ( maxSpotIdCount > 0 ) ss << "maxSpotId : " << maxSpotId << std::endl;
        if ( spot_groups.size() > 0 )
        {
            ss << "spot-groups : ";
            int i = 0;
            for ( auto const &value : spot_groups )
            {
                if ( i++ > 0 ) ss << ',';
                ss << value;
            }
        }
        if ( clip ) ss << "clip" << std::endl;
        if ( minReadLenCount > 0 ) ss << "minReadLen : " << minReadLen << std::endl;
        if ( !read_filter.isEmpty() )  ss << "read-filter : " << read_filter << std::endl;
        if ( qual_filter ) ss << "qual-filter" << std::endl;
        if ( qual_filter1 ) ss << "qual-filter-1" << std::endl;
        if ( aligned ) ss << "aligned" << std::endl;
        if ( unaligned ) ss << "unaligned" << std::endl;
        if ( !aligned_region.isEmpty() )  ss << "aligned-region : " << aligned_region << std::endl;
        if ( !matepair_dist.isEmpty() )  ss << "matepair-dist : " << matepair_dist << std::endl;
        if ( skip_tech ) ss << "skip-tech" << std::endl;        
        return ss.str();
    }

    void populate_argv_builder( ArgvBuilder & builder )
    {
        if ( !accession_replacement.isEmpty() ) builder . add_option( "-A", accession_replacement );
        if ( !table_name.isEmpty() ) builder . add_option( "--table", table_name );
        if ( split_spot ) builder . add_option( "--split-spot" );
        if ( minSpotIdCount > 0 ) builder . add_option( "-N", minSpotId );
        if ( maxSpotIdCount > 0 ) builder . add_option( "-X", maxSpotId );
        if ( spot_groups.size() > 0 ) builder . add_list_option( "--spot-groups", ',', spot_groups );
        if ( clip ) builder . add_option( "-W" );
        if ( minReadLenCount > 0 ) builder . add_option( "-M", minReadLen );
        if ( !read_filter.isEmpty() ) builder . add_option( "-R", read_filter );
        if ( qual_filter ) builder . add_option( "-E" );
        if ( qual_filter1 ) builder . add_option( "--qual-filter-1" );
        if ( aligned ) builder . add_option( "--aligned" );
        if ( unaligned ) builder . add_option( "--unaligned" );
        if ( !aligned_region.isEmpty() ) builder . add_option( "--aligned-region", aligned_region );
        if ( !matepair_dist.isEmpty() ) builder . add_option( "--matepair-distance", matepair_dist );
        if ( skip_tech ) builder . add_option( "--skip-technical" );
    }
};

int impersonate_fastq_dump( const Args &args )
{
    int res = 0;

    // Cmdline is a class defined in cmdline.hpp
    ncbi::Cmdline cmdline( args._argc, args._argv );
    
    // CmnOptAndAccessions is defined in support2.hpp
    CmnOptAndAccessions cmn( "fastq-dump" );

    // add all common options and the parameters to the parser
    cmn.add( cmdline );

    // FastqParams is a derived class of ToolOptions, defined in support2.hpp
    FastqParams params;
    
    // add all the tool-specific options to the parser
    params.add_options( cmdline );

    try
    {
        // let the parser parse the original args,
        // and let the parser handle help,
        // and let the parser write all values into cmn and params
        cmdline . parse ( true );
        cmdline . parse ();

        // just to see what we got
        // std::cout << cmn . as_string() << std::endl;

        // just to see what we got
        //std::cout << params . as_string() << std::endl;

        // create an argv-builder 
        ArgvBuilder builder;
        params . populate_argv_builder( builder );

        // what should happen before executing the tool
        int argc;
        char ** argv = builder . generate_argv( argc );
        if ( argv != nullptr )
        {
            for ( int i = 0; i < argc; ++i )
                std::cout << "argv[" << i << "] = '" << argv[ i ] << "'" << std::endl;
            builder . free_argv( argc, argv );
        }
    }
    catch ( ncbi::InvalidArgument const &e )
    {
        std::cerr << "An error occured: " << e.what() << std::endl;
    }

    return res;
}

} // namespace