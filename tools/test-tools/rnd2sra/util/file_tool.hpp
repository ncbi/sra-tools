#pragma once

#include <string>

#include "../util/fs.hpp"

using namespace std;

namespace sra_convert {

class FileTool {

    public :

        static string location( const string& path ) {
            if ( ! path . empty() ) {
                fs::path fs_path{ path };
                fs::path parent_path{ fs_path . parent_path() };
                if ( parent_path . empty() ) return string();
                if ( !exists( parent_path ) ) return string();
                fs::path abs_parent_path{ fs::absolute( parent_path ) };
                return fs::canonical( abs_parent_path );
            }
            return string();
        }

        static bool exists( const string& path ) {
            bool res = ! path . empty();
            if ( res ) {
                fs::path fs_path{ path };
                res = fs::exists( fs_path );
            }
            return res;
        }

        static bool make_dir_if_not_exists( const string& path ) {
            bool res = exists( path );
            if ( !res ) {
                error_code ec;
                res = fs::create_directories( path, ec );
            }
            return res;
        }

        static string remove_traling_separator( const string& path ) {
            auto res{ path };
            while ( ( ! path . empty() ) &&
                    ( ( res . back() == '/') || ( res.back() == '\\' ) ) ) {
                    res . erase( res . size() - 1 );
            }
            return res;
        }

        static string current_dir( void ) {
            return fs::current_path();
        }
};

}
