#ifndef EXPORTER_H
#define EXPORTER_H

#include <algorithm>

#include "line_split.hpp"
#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"
#include "ref_dict.hpp"
#include "spot.hpp"

class EXPORTER {
    private :
        const EXPORT_PARAMS& params;
        SAM_DB &db;
        ref_dict_t &ref_dict;
        EXPORT_RESULT result;
        
        EXPORTER( const EXPORTER& ) = delete;
        
        bool export_HD_headers( std::ostream& stream, bool print_HD_hdrs ) {
            auto stm = db . make_prep_stm( 
                print_HD_hdrs
                    ? "SELECT VALUE FROM HDR WHERE NOT VALUE LIKE '@HD%';"
                    : "SELECT VALUE FROM HDR WHERE VALUE LIKE '@HD%';" );
            uint16_t st( stm -> get_status() );
            if ( SQLITE_OK == st ) {
                st = stm -> step();
                while( SQLITE_ROW == st ) {
                    stream << stm -> read_string() << std::endl;
                    result . header_lines += 1;
                    st = stm -> step();
                }
            }
            return ( SQLITE_DONE == st );
        }

        bool export_ref_headers( std::ostream& stream ) {
            auto stm = db . make_prep_stm(  "SELECT NAME,LENGTH,OTHER FROM REF;" );
            uint16_t st( stm -> get_status() );
            if ( SQLITE_OK == st ) {
                st = stm -> step();
                while ( SQLITE_ROW == st ) {
                    uint16_t col_cnt = stm -> column_count();
                    std::string ref_name = stm -> read_string();
                    bool used = true;
                    if ( params . only_used_refs ) {
                        used = ref_dict . contains( ref_name );
                    }
                    if ( used  ) {
                        uint32_t ln = stm -> read_uint32_t( 1 );
                        std::string other = stm -> read_string( 2 );
                        if ( other . empty() ) {
                            stream << "@SQ\tSN:" << ref_name << "\tLN:" << ln << std::endl;
                        } else {
                            stream << "@SQ\tSN:" << ref_name << "\tLN:" << ln << "\t" << other << std::endl;                            
                        }
                        result . header_lines += 1;
                    }
                    st = stm -> step();
                }
            }
            return ( SQLITE_DONE == st );            
        }

        bool export_alignments( std::ostream& stream ) {
            ALIG_ITER alig_iter( db, params . alig_order );
            bool show_progress = params . cmn . progress;
            SAM_ALIG alig;
            while ( alig_iter . next( alig ) ) {
                if ( params . fix_names ) { alig . fix_name(); }
                alig . print( stream );
                result . alignment_lines += 1;
                if ( show_progress ) {
                    if ( ( result . alignment_lines % params . cmn . transaction_size ) == 0 ) {
                        std::cerr << '.';
                    }
                }
            }
            if ( show_progress ) { std::cerr << std::endl; }
            return true;
        }

        void populate_ref_dict( void ) {
            SAM_SPOT spot;
            SPOT_ITER spot_iter( db );
            while ( spot_iter . next( spot ) ) {
                spot . enter_into_reflist( ref_dict );
            }
        }

    public :
        EXPORTER( SAM_DB& a_db, const EXPORT_PARAMS& a_params, ref_dict_t& a_ref_dict )
            : params( a_params ), db( a_db ), ref_dict( a_ref_dict ) { }

        bool run( bool show_report ) {
            if ( show_report ) {
                std::cerr << "EXPORT:" << std::endl;
            }
            if ( params . only_used_refs && ref_dict . size() == 0 ) {
                populate_ref_dict();
            }
            FILE_WRITER writer( params . export_filename );
            std::ostream *stream = writer . get();
            result . success = export_HD_headers( *stream, false );
            if ( result . success ) { result . success = export_ref_headers( *stream ); }
            if ( result . success ) { result . success = export_HD_headers( *stream, true ); }
            if ( result . success ) { result . success = export_alignments( *stream ); }
            if ( result . success && show_report ) { result . report(); }
            return result . success;
        }

};


#endif
