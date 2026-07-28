#pragma once

#include <string>
#include <vector>
#include <unistd.h>

#include "fs.hpp"

using namespace std;

namespace sra_convert {

class FileReport {
    public :

        static string status_2_string( fs::file_status status ) {
            switch( status . type() ) {
                case fs::file_type::block : return "block"; break;
                case fs::file_type::character : return "char"; break;
                case fs::file_type::directory : return "dir"; break;
                case fs::file_type::fifo : return "fifo"; break;
                case fs::file_type::none : return "none"; break;
                case fs::file_type::not_found : return "not_found"; break;
                case fs::file_type::regular : return "regular"; break;
                case fs::file_type::socket : return "socket"; break;
                case fs::file_type::symlink : return "symlink"; break;
                case fs::file_type::unknown : return "unknown"; break;
            }
            return "what?";
        }

        static uint32_t count_open_files( void ) {
            uint32_t still_open = 0;
            fs::path p( "/proc/" + to_string( getpid() ) + "/fd" );
            for ( auto const& dir_entry : fs::directory_iterator{ p } ) {
                still_open++;
            }
            return still_open;
        }

        static vector< string > enumerate_open_files( void ) {
            vector< string > res;
            fs::path p( "/proc/" + to_string( getpid() ) + "/fd" );
            for ( auto const& dir_entry : fs::directory_iterator{ p } ) {
                fs::file_status status{ fs::status( dir_entry . path() ) };
                string s{ dir_entry . path() };
                s += " - ";
                s += status_2_string( status );
                res . push_back( s );
            }
            return res;
        }
};

}
