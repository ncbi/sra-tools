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
        
        bool export_HD_headers( std::ostream& stream, bool negate ) {
            auto stm = db . make_prep_stm( 
                negate ? "SELECT VALUE FROM HDR WHERE NOT VALUE LIKE '@HD%';"
                       : "SELECT VALUE FROM HDR WHERE VALUE LIKE '@HD%';" );
            uint16_t st( stm -> get_status() );
            if ( SQLITE_OK == st ) {
                mt_database::DB::str_vec data;
                do {
                    st = stm -> read_row( data, 1 );
                    if ( SQLITE_ROW == st ) {
                        stream << data[ 0 ] << std::endl;
                        result . header_lines += 1;
                    }
                } while ( SQLITE_ROW == st );
            }
            bool res = ( SQLITE_DONE == st );
            return res;
        }

        bool export_ref_headers( std::ostream& stream ) {
            std::string sstm( "SELECT NAME,LENGTH,OTHER FROM REF;" );
            auto stm = db . make_prep_stm( sstm );
            uint16_t st( stm -> get_status() );
            
            if ( SQLITE_OK == st || SQLITE_DONE == st ) {
                mt_database::DB::str_vec data;
                do {
                    st = stm -> read_row( data, 3 );
                    bool used = true;
                    if ( params . only_used_refs ) {
                        used = ref_dict . contains( data[ 0 ] );
                    }
                    if ( used && SQLITE_ROW == st ) {
                        stream << "@SQ\tSN:" << data[ 0 ] << "\tLN:" << data[ 1 ] << std::endl;
                        result . header_lines += 1;
                    }
                } while ( SQLITE_ROW == st );
            }
            bool res = ( SQLITE_DONE == st );            
            return res;
        }

        bool export_headers( std::ostream& stream ) {
            bool res = export_HD_headers( stream, false );
            if ( res ) { res = export_ref_headers( stream ); }
            if ( res ) { res = export_HD_headers( stream, true ); }
            return res;
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
            bool ok = export_headers( *stream );
            if ( ok ) { ok = export_alignments( *stream ); }
            result . success = ok;
            if ( ok && show_report ) { result . report(); }
            return ok;
        }

};


#endif
