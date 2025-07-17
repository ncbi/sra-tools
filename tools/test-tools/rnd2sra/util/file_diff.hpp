#pragma once

#include <string>
#include <vector>
#include <fstream>

using namespace std;

namespace sra_convert {

class FileDiff {

    private :
        static bool diff_file_to_file( const string& file1, const string& file2, bool ignore_err ) {
            std::ifstream f1( file1, std::ifstream::binary|std::ifstream::ate );
            std::ifstream f2( file2, std::ifstream::binary|std::ifstream::ate );

            // if both files cannot be opened AND we want to ignore that --> SUCCESS
            if ( f1 . fail() && f2 . fail() && ignore_err ) { return true; }

            // if one or both of the files cannot be opened --> FAILURE
            if ( f1 . fail() || f2 . fail() ) { return false; }

            // if the sizes do not match --> FAILURE
            if ( f1 . tellg() != f2 . tellg() ) { return false; }

            // seek back to beginning
            f1 . seekg( 0, std::ifstream::beg );
            f2 . seekg( 0, std::ifstream::beg );

            // let std::equal do the comparison ...
            return std::equal( std::istreambuf_iterator<char>( f1 . rdbuf() ),
                               std::istreambuf_iterator<char>(),
                               std::istreambuf_iterator<char>( f2 . rdbuf() ) );
        }

        static bool diff_patterns( vector< string >& args, bool ignore_err ) {
            bool res = true;
            size_t idx = 0;
            size_t count = args . size();
            string& src_head = args[ idx++ ];
            string& dst_head = args[ idx++ ];
            string& ext = args[ idx++ ];
            while ( ( res ) && ( idx < count ) ) {
                string& tail = args[ idx++ ];
                string src = src_head + tail + ext;
                string dst = dst_head + tail + ext;
                res = diff_file_to_file( src, dst, ignore_err );
            }
            return res;
        }

    public :

        static bool diff( vector< string >& args, bool ignore_err )  {
            bool res = false;
            switch( args . size() ) {
                case 0 : if ( ignore_err ) { res = true; } break;
                case 1 : if ( ignore_err ) { res = true; } break;
                case 2 : res = diff_file_to_file( args[ 0 ], args[ 1 ], ignore_err ); break;
                default : res = diff_patterns( args, ignore_err ); break;
            }
            return res;
        }

};

}
