#pragma once

#include <cstdint>
#include <iostream>
#include "../vdb/wvdb.hpp"
#include "../util/random_toolbox.hpp"
#include "rnd2sra_ini.hpp"
#include "../../libs/inc/fingerprint.hpp"

using namespace std;
using namespace vdb;

namespace sra_convert {

struct base_counters {
    uint64_t total;
    uint64_t bio;
    base_counters( void ) : total( 0 ), bio( 0 ) {}
};

class Rndcmn {
    protected :
        VMgrPtr f_mgr;
        rnd2sra_ini_ptr f_ini;      // custom values from the ini-file
        RandomPtr f_rnd;            // provider of the random-ness
        const string& f_output_dir; // where to write the to
        base_counters f_counters;   // stats to be written
        Fingerprint f_fingerprint;  // collects the fingerprint
        VSchPtr f_schema;           // the schema to use

        // Ctor
        Rndcmn( VMgrPtr mgr,
                rnd2sra_ini_ptr ini,
                RandomPtr rnd,
                const string& output_dir,
                const char * schema_txt )
              : f_mgr( mgr ),
                f_ini( ini ),
                f_rnd( rnd ),
                f_output_dir( output_dir ),
                f_schema( make_schema( mgr, schema_txt ) )
                {  }

        bool with_name( void ) const { return f_ini -> get_with_name(); }
        bool with_spot_group( void ) const { return f_ini -> has_spot_groups(); }

        string make_name( void ) { return f_ini-> name( f_rnd ); }

        size_t make_random_qual( uint8_t * buffer, int64_t row, size_t read_len ) {
            size_t res = read_len + f_ini -> qual_len_offset( row );
            f_rnd -> random_diff_quals( buffer, res );
            return res;
        }

        static VSchPtr make_schema( VMgrPtr mgr, const char * schema_txt ) {
            auto res = mgr -> make_schema();
            if ( ! *res ) {
                cerr << "error creating schema" << endl;
            } else {
                res -> ParseText( schema_txt );
                if ( ! *res ) {
                    cerr << "error parsing schema" << endl;
                    cerr << res -> error() << endl;
                }
            }
            return res;
        }

        static bool write_stats_entry( MetaNodePtr stats, const string& name, uint64_t value ) {
            auto node = stats -> open_update( name );
            if ( *node ) { return node -> write( value ); }
            return false;
        }

        bool write_stats( VTblPtr tbl, base_counters& counters, uint64_t row_count ) {
            if ( f_ini -> get_do_not_write_meta() ) { return true; }
            auto meta = tbl -> open_meta_for_update();
            if ( *meta ) {
                auto stats = meta -> open_node_update( "STATS/TABLE" );
                if ( *stats ) {
                    if ( write_stats_entry( stats, "BASE_COUNT", counters . total ) ) {
                        if ( write_stats_entry( stats, "BIO_BASE_COUNT", counters . bio ) ) {
                            if ( write_stats_entry( stats, "SPOT_COUNT", row_count ) ) {
                                return meta -> commit();
                            }
                        }
                    }
                }
            }
            return false;
        }

       void record_fingerprint( std::string_view const seq ) {
            f_fingerprint . record( seq );
        }

        static bool write_fp_string( MetaNodePtr parent_node, const string& name, const std::string& value ) {
            auto node = parent_node -> open_update( name );
            if ( *node ) { return node -> write( value ); }
            return false;
        }

        bool write_fingerprint( VTblPtr tbl ) {
            if ( f_ini -> get_do_not_write_meta() ) { return true; }
            auto meta = tbl -> open_meta_for_update();
            if ( *meta ) {
                auto fp = meta -> open_node_update( "QC/current" );
                if ( *fp ) {
                    /* <algorithm>'SHA-256'</algorithm> */
                    if ( write_fp_string( fp, "algorithm", "SHA-256" ) ) {
                         /* <digest>'ed736252c65bf3a0938cabcf172a122895e206454ca674e266201e6f41ccd3da'</digest> */
                        if ( write_fp_string( fp, "digest", f_fingerprint . digest() ) ) {
                            /* <fingerprint> ..... </fingerprint> */
                            if ( write_fp_string( fp, "fingerprint", f_fingerprint.JSON( true ) ) ) {
                                /* <format>'json utf-8 compact'</format> */
                                if ( write_fp_string( fp, "format", "json utf-8 compact" ) ) {
                                    /* <version>'1.0.0'</version> */
                                    if ( write_fp_string( fp, "version", "1.0.0" ) ) {
                                        return meta -> commit();
                                    }
                                }
                            }
                        }
                    }
                }
            }
            return true;
        }
};

}  // end of namespace sra_convert
