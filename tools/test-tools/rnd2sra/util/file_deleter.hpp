#pragma once

#include <glob.h>
#include <cstdlib>
#include <memory>
#include <string>
#include <string.h>
#include <vector>
#include <iostream>

#include "../util/fs.hpp"

using namespace std;

namespace sra_convert {

class deleter {
        static vector< string > glob_pattern( const string& pattern ) {
            vector< string > res;

            glob_t files;
            memset( &files, 0, sizeof( files ) );

            if ( 0 == glob( pattern . c_str(), GLOB_TILDE, NULL, &files ) ) {
                for( size_t i = 0; i < files . gl_pathc; ++i ) {
                    res . push_back( string( files . gl_pathv[ i ] ) );
                }
            }

            globfree( &files );
            return res;
        }

        static bool del_stop_on_err( const string& item, bool silent ) {
            bool res = true;
            if ( fs::is_regular_file( item ) ) {
                try {
                    fs::remove( item );
                    if ( !silent ) { cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                    cerr << "removing: >" << item << "< " << e.what() << endl;
                    res = false;
                }
            } else if ( fs::is_directory( item ) ) {
                try {
                    fs::remove_all( item );
                    if ( !silent ) { cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                    cerr << "removing: >" << item << "< " << e.what() << endl;
                    res = false;
                }
            } else {
                cerr << "removing: >" << item << "< is not a file or directory" << endl;
                res = false;
            }
            return res;
        }

        static void del_ignore_err( const string& item, bool silent ) {
            if ( fs::is_regular_file( item ) ) {
                try {
                    fs::remove( item );
                    if ( !silent ) { cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                }
            } else if ( fs::is_directory( item ) ) {
                try {
                    fs::remove_all( item );
                    if ( !silent ) { cout << "removed: >" << item << "<\n"; }
                } catch ( fs::filesystem_error &e ) {
                }
            }
        }

        static bool del_stop_on_err( const vector< string >& items, bool silent ) {
            bool res = true;
            for ( auto& item : items ) {
                if ( res ) {
                    auto files = glob_pattern( item );
                    for ( auto& item2 : files ) {
                        if ( res ) {
                            res = del_stop_on_err( item2, silent );
                        }
                    }
                }
            }
            return res;
        }

        static void del_ignore_err( const vector< string >& items, bool silent ) {
            for ( auto& item : items ) {
                auto files = glob_pattern( item );
                for ( auto& item2 : files ) {
                    del_ignore_err( item2, silent );
                }
            }
        }

    public :
        static bool del( const vector< string >& items, bool silent, bool ignore_err ) {
            bool res = true;
            if ( ignore_err ) {
                del_ignore_err( items, silent );
            } else {
                res = del_stop_on_err( items, silent );
            }
            return res;
        }
};

}
