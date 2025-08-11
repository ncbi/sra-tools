#pragma once

#include <cstdlib>
#include <memory>
#include <iostream>
#include "../util/ini.hpp"
#include "../util/file_tool.hpp"
#include "../vdb/wvdb.hpp"
#include "product.hpp"
#include "rnd2sra_ini.hpp"
#include "main_params.hpp"
#include "runner.hpp"
#include "rnd_none_csra.hpp"
#include "rnd_csra.hpp"

using namespace std;

namespace sra_convert {

class Tool_Main;
typedef std::shared_ptr< Tool_Main > Tool_MainPtr;
class Tool_Main {
    private:
        const MainParamsPtr f_main_params;
        Product_ptr f_product;

        bool split_path( const string& input, string& p1, string& p2 ) const {
            bool res = true;
            fs::path p( input );
            p1 = p . parent_path() . string();
            p2 = p . filename() . string();
            if ( ! p1 . empty() ) {
                error_code ec;
                if ( ! fs::exists( p1, ec ) ) {
                    res = fs::create_directories( p1, ec );
                }
            }
            return res;
        }

        // produce an artificial accession
        bool produce_acc( void ) {
            bool res = true;
            string parent_out_path, out_leaf;
            auto ini = rnd2sra_ini::make( f_main_params -> get_ini_file() );
            auto output_dir = FileTool::remove_traling_separator(
                                f_main_params -> get_output() );
            if ( ini -> has_output_dir() ) {
                if ( ! output_dir . empty() ) {
                    // cmd-line overwrite the ini-file setting
                    ini -> set_output_dir( output_dir );
                } else {
                    output_dir = FileTool::remove_traling_separator(
                                    ini -> get_output_dir() );
                }
            } else {
                if ( output_dir . empty() ) {
                    // there is no output-dir in the ini-file
                    // and it is also not given via cmd-line
                    // ---> but we need one!
                    cerr << "missing: output-dir" << endl;
                    res = false;
                } else {
                    ini -> set_output_dir( output_dir );
                }
            }

            if ( res ) {
                res = split_path( output_dir, parent_out_path, out_leaf );
            }

            if ( res ) {
                if ( ini -> get_echo_values() ) { cerr << ini << endl; }

                auto rnd = Random::make( ini -> get_seed() );

                auto dir = vdb::KDir::make( parent_out_path );
                if ( ! *dir ) { cerr << "make dir failed\n"; return false; }

                auto mgr = dir -> make_mgr();
                if ( ! *mgr ) {
                    cerr << "make mgr failed :\n";
                    cerr << "current dir     : " << FileTool::current_dir() << endl;
                    cerr << "parent_out_path : '" << parent_out_path << "'" << endl;
                    return false; }

                if ( f_product -> is_flat() ) {
                    res = RndNonecSraFlat::produce( mgr, ini, rnd, out_leaf ); // rnd_none_csra.hpp
                } else if ( f_product -> is_db() ) {
                    res = RndNonecSraDb::produce( mgr, ini, rnd, out_leaf );  // rnd_none_csra.hpp
                } else if ( f_product -> is_cSRA() ) {
                    res = RndcSRA::produce( mgr, ini, rnd, out_leaf ); // rnd_csra.hpp
                } else {
                    cerr << "unknown product\n";
                    res = false;
                }
            }
            return res;
        }

        // run a test of a tool against an artificial accession
        bool run_test( void ) {
            auto ini = Ini::make( f_main_params -> get_ini_file() );
            auto r = runner::make( ini, f_main_params );
            return r -> run();
        }

        // Ctor
        Tool_Main( const MainParamsPtr params ) : f_main_params( params ) {
            string s_product = Ini::make( params -> get_ini_file() ) -> get( "product", "" );
            f_product = Product::make( s_product );
        }

    public:
        static Tool_MainPtr make( const MainParamsPtr params ) {
            return Tool_MainPtr( new Tool_Main( params ) );
        }

        bool run( void ) {
            bool res = false;
            string ini_file = f_main_params -> get_ini_file();
            if ( ini_file . empty() ) {
                // without a config/ini - file we cannot do anything ...
                cerr << "missing: config/ini-file" << endl;
            } else if ( ! FileTool::exists( ini_file ) ) {
                // there is a config-file specified, but it cannot be found
                cerr << "config-file '" << ini_file << "' not found" << endl;
                cerr << "ini-dir: " << f_main_params->get_ini_file_loc() << endl;
            } else {
                /* the same tool can be used to produce an accession or run a test */
                if ( f_product -> is_acc() ) {
                    res = produce_acc();    // above ( we are producing an accession )
                } else if ( f_product -> is_tst() ) {
                    res = run_test();       // above ( we are running a test )
                } else {
                    cerr << "unknown product!" << endl;
                }
            }
            return res;
        }

}; // end of Tool_Main class

} // end of namespace sra_convert
