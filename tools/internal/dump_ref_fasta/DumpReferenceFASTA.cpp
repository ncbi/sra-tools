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

#include <ncbi/NGS.hpp>
#include <ngs/ErrorMsg.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/ReadIterator.hpp>
#include <ngs/Read.hpp>

#include <kapp/vdbapp.h>
#include <kapp/main.h>

#include <math.h>
#include <iostream>
#include <sstream>
#include <limits>

using namespace ngs;
using namespace std;

const char UsageDefaultName[] = "dump-ref-fasta";

const String COLON = std::string( ":" );
const String DASH  = std::string( "-" );
const String DOT   = std::string( "." );

class Range
{
    public :
        uint64_t offset;
        uint64_t len;

        Range() : offset( 0 ), len( 0 ) {}

        Range( const String & spec )
        {
            const string::size_type dash_pos = spec.find( DASH );
            if ( dash_pos == string::npos )
            {
                const string::size_type dot_pos = spec.find( DOT );
                if ( dot_pos == string::npos )
                {
                    offset = to_uint64_t( spec );
                    len = 0;
                }
                else
                {
                    const String offset_string = spec.substr( 0, dot_pos );
                    offset = to_uint64_t( offset_string );
                    const String len_string = spec.substr( dot_pos + 1 );
                    len = len_string.empty() ? std::numeric_limits< uint64_t >::max() : to_uint64_t( len_string );
                }
            }
            else
            {
                const String offset_string = spec.substr( 0, dash_pos );
                offset = to_uint64_t( offset_string );
                const String end_string = spec.substr( dash_pos + 1 );
                uint64_t end = end_string.empty() ? std::numeric_limits< uint64_t >::max() : to_uint64_t( end_string );
                len = offset <= end ? end - offset : offset - end;
                if ( offset > end ) { offset = end; }
            }

            /* we expect the coordinates 1-based, but we are using internally 0-based offsets */
            if ( offset > 0 ) { offset--; }
        }

        static uint64_t to_uint64_t( const String & num )
        {
            uint64_t res = 0;
            std::istringstream ss( num );
            ss >> res;
            return res;
        }

        void adjust_by_reflen( const uint64_t reflen )
        {
            if ( offset == 0 )
            {
                if ( len > reflen || len == 0 ) { len = reflen; }
            }
            else
            {
                if ( offset >= reflen ) { offset = reflen - 1; }

                if ( len == 0 )
                {
                    len = reflen - offset;
                }
                else
                {
                    if ( offset + len > reflen ) { len = reflen - offset; }
                }
            }
        }
};

class Slice
{
    public :
        String name;
        Range range;

        Slice() {}

        // spec = 'name[:from[-to]]' or 'name[:from[.count]]'
        Slice( const String & spec )
        {
            const string::size_type colon_pos = spec.find( COLON );
            if ( colon_pos == string::npos )
            {
                name = spec;
            }
            else
            {
                name = spec.substr( 0, colon_pos );
                const String range_string = spec.substr( colon_pos + 1 );
                range = Range( range_string );
            }
        }
};

const uint64_t CHUNK_SIZE = 5000;
const uint64_t LINE_LEN = 70;

class DumpReferenceFASTA
{
    public :
        static void process ( const Reference & ref, const Range & range )
        {
            cout << '>' << ref.getCanonicalName () << endl; // VDB-3665: do not need the slice // << ':' << range.offset + 1 << '.' << range.len << '\n';
            try
            {
                size_t line = 0;
                uint64_t count = 0;
                uint64_t offset = range.offset;
                uint64_t left = range.len;

                while ( left > 0 )
                {
                    StringRef chunk = ref.getReferenceChunk( offset, left > CHUNK_SIZE ? CHUNK_SIZE : left );
                    size_t chunk_len = chunk.size ();
                    for ( size_t chunk_idx = 0; chunk_idx < chunk_len; )
                    {
                        StringRef chunk_line = chunk.substr( chunk_idx, LINE_LEN - line );
                        line += chunk_line.size ();
                        chunk_idx += chunk_line.size ();

                        cout << chunk_line;
                        if ( line >= LINE_LEN )
                        {
                            cout << '\n';
                            line = 0;
                        }
                    }
                    offset += chunk_len;
                    count += chunk_len;
                    left -= chunk_len;
                }
                if ( line != 0 )
                    cout << '\n';
            }
            catch ( const ErrorMsg & x )
            {
                cerr <<  x.toString() << '\n';
            }
        }

    static void run( const String & acc, Slice * slices, int n, bool local_only )
    {
        // open requested accession using SRA implementation of the API
        ReadCollection run = ncbi::NGS::openReadCollection( acc );
        Reference ref = run.getReference( slices[ 0 ].name );
        for ( int i = 0; i < n; ++i )
        {
            if ( i > 0 && !slices[ i ].name.empty() )
            {
                ref = run.getReference( slices[ i ].name );
            }
            if ( ! local_only || ref.getIsLocal() )
            {
                slices[ i ].range.adjust_by_reflen( ref.getLength() );
                process( ref, slices[ i ].range );
            }
        }
    }

    static void run( const String & acc, bool local_only )
    {

        // open requested accession using SRA implementation of the API
        ReadCollection run = ncbi::NGS::openReadCollection( acc );
        ReferenceIterator refs = run.getReferences();
        while ( refs.nextReference () )
        {
            if ( ! local_only || refs.getIsLocal() )
            {
                Slice slice( refs.getCanonicalName() );
                slice.range.adjust_by_reflen( refs.getLength() );
                process( refs, slice.range );
            }
        }
    }
};

static void print_help ( void )
{
    cout << "Usage: dump-ref-fasta accession [ reference[ slice ] ] ... " << endl;

    cout
        << '\n'
        << "Usage:\n"
        << "  " << UsageDefaultName << " [options] accession [ reference[ slice ] ] ... "
        << "\n\n"
        << "Options:\n"
        << "  -l|--localref                    Skip non-local references. \n"
        << "  -h|--help                        Output brief explanation for the program. \n"
        << "  -v|-V|--version                  Display the version of the program then\n"
        << "                                   quit.\n"
        ;

    HelpVersion ( UsageDefaultName, KAppVersion () );
}

static void print_version ( void )
{
    HelpVersion ( UsageDefaultName, KAppVersion () );
}

int run ( int argc, char *argv[] )
{
    if ( argc < 2 )
    {
        print_help ();
        return 1;
    }
    else try
    {
        ncbi::NGS::setAppVersionString ( "DumpReferenceFASTA.1.0.0" );

        String acc;
        Slice * slices = 0;
        int n = 0;
        bool local_only = false;

        int i = 1;
        while ( i < argc )
        {
            const String arg = argv [ i ];

            if ( arg == "-h" || arg == "--help" )
            {
                print_help ();
                return 0;
            }
            else if ( arg == "-v" || arg == "-V" || arg == "--version" )
            {
                print_version ();
                return 0;
            }
            else if ( arg == "-l" || arg == "--localref" )
            {
                local_only = true;
            }
            else
            {
                acc = arg;
                if ( argc > i + 1 )
                {
                    n = ( argc - i - 1 );
                    slices = new Slice[ n ];
                    for ( int j = 0; j < n; ++j )
                        slices[ j ] = Slice( argv[ i + j + 1] );
                    i += n;
                }
            }

            ++i;
        }

        if ( n != 0 )
        {
            DumpReferenceFASTA::run( acc, slices, n, local_only );
        }
        else
        {
            DumpReferenceFASTA::run( acc, local_only );
        }

        delete [] slices;
        return 0;
    }
    catch ( ErrorMsg &x )
    {
        cerr <<  x.toString() << '\n';
    }
    catch ( exception &x )
    {
        cerr <<  x.what() << '\n';
    }
    catch ( ... )
    {
        cerr <<  "unknown exception\n";
    }

    return 10;
}

MAIN_DECL(argc, argv)
{
    VDB::Application app(argc, argv);

    try
    {
        return run ( argc, app.getArgV() );
    }
    catch ( ErrorMsg & x )
    {
        std :: cerr <<  x.toString () << '\n';
        return -1;
    }
    catch ( std :: exception & x )
    {
        std :: cerr <<  x.what () << '\n';
        return -1;
    }
    catch ( const char x [] )
    {
        std :: cerr <<  x << '\n';
        return -1;
    }
    catch ( ... )
    {
        std :: cerr <<  "unknown exception\n";
        return -1;
    }

    return 0;
}
