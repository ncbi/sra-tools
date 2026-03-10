#pragma once

#include <cstdlib>
#include <memory>
#include <iostream>
#include "../util/ini.hpp"
#include "../util/file_tool.hpp"
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
        const IniPtr f_ini;
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
            rnd2sra_ini_ptr special_ini = rnd2sra_ini::make( f_ini, f_main_params );
            string parent_out_path, out_leaf;
            auto output_dir = FileTool::remove_traling_separator( f_main_params -> get_output() );
            if ( special_ini -> has_output_dir() ) {
                if ( ! output_dir . empty() ) {
                    // cmd-line overwrite the ini-file setting
                    special_ini -> set_output_dir( output_dir );
                } else {
                    output_dir = FileTool::remove_traling_separator( special_ini -> get_output_dir() );
                }
            } else {
                if ( output_dir . empty() ) {
                    // there is no output-dir in the ini-file
                    // and it is also not given via cmd-line
                    // ---> but we need one!
                    cerr << "missing: output-dir" << endl;
                    res = false;
                } else {
                    special_ini -> set_output_dir( output_dir );
                }
            }

            if ( res ) {
                res = split_path( output_dir, parent_out_path, out_leaf );
            }

            if ( res ) {
                if ( special_ini -> get_echo_values() ) { cerr << special_ini << endl; }

                auto rnd = Random::make( special_ini -> get_seed() );

                auto dir = vdb::KDir::make( parent_out_path );
                if ( ! *dir ) { cerr << "make dir failed\n"; return false; }

                auto mgr = dir -> make_mgr();
                if ( ! *mgr ) {
                    cerr << "make mgr failed :\n";
                    cerr << "current dir     : " << FileTool::current_dir() << endl;
                    cerr << "parent_out_path : '" << parent_out_path << "'" << endl;
                    return false; }

                if ( f_product -> is_flat() ) {
                    res = RndNonecSraFlat::produce( mgr, special_ini, rnd, out_leaf ); // rnd_none_csra.hpp
                } else if ( f_product -> is_db() ) {
                    res = RndNonecSraDb::produce( mgr, special_ini, rnd, out_leaf );  // rnd_none_csra.hpp
                } else if ( f_product -> is_cSRA() ) {
                    res = RndcSRA::produce( mgr, special_ini, rnd, out_leaf ); // rnd_csra.hpp
                } else {
                    cerr << "unknown product\n";
                    res = false;
                }
            }
            return res;
        }

        // Ctor
        Tool_Main( const MainParamsPtr params, const IniPtr ini ) : f_main_params( params ), f_ini( ini ) {
            f_product = Product::make( ini -> get( "product", "" ) );
        }

    public:
        static Tool_MainPtr make( const MainParamsPtr params, const IniPtr ini ) {
            return Tool_MainPtr( new Tool_Main( params, ini ) );
        }

        bool run( void ) {
            /* the same tool can be used to produce an accession or run a test */
            if ( f_product -> is_acc() ) {
                return produce_acc();    // above ( we are producing an accession )
            } else if ( f_product -> is_tst() ) {
                auto r = runner::make( f_ini, f_main_params );
                return r -> run();
            } else {
                cerr << "unknown product!" << endl;
            }
            return false;
        }

}; // end of Tool_Main class

} // end of namespace sra_convert
