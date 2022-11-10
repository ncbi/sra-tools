#ifndef EXPORTER_H
#define EXPORTER_H

#include <algorithm>

#include "line_split.hpp"
#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"

class EXPORTER {
    private :
        const EXPORT_PARAMS& params;
        EXPORT_RESULT& result;
        SAM_DB &db;

        EXPORTER( const EXPORTER& ) = delete;
        
        bool export_HD_headers( std::ostream& stream, bool negate ) {
            auto stm = db . make_prep_stm( 
                negate ? "SELECT VALUE FROM HDR WHERE NOT VALUE LIKE '@HD%';"
                       : "SELECT VALUE FROM HDR WHERE VALUE LIKE '@HD%';" );
            if ( SQLITE_OK == stm -> get_status() ) {
                mt_database::DB::str_vec data;
                int st;
                do {
                    st = stm -> read_row( data, 1 );
                    if ( SQLITE_ROW == st ) {
                        stream << data[ 0 ] << std::endl;
                        result . header_lines += 1;
                    }
                } while ( SQLITE_ROW == st );
            }
            return ( SQLITE_DONE == stm -> get_status() );
        }

        bool export_ref_headers( std::ostream& stream ) {
            std::string sstm( "SELECT NAME,LENGTH,OTHER FROM REF" );
            if ( params . only_used_refs ) {
                sstm +=" WHERE ( SELECT CNT FROM REF_CNT WHERE RNAME = REF.NAME ) > 0;";
            } else {
                sstm += ";";
            }
            auto stm = db . make_prep_stm( sstm );
            if ( SQLITE_OK == stm -> get_status() ) {
                mt_database::DB::str_vec data;
                int16_t st;
                do {
                    st = stm -> read_row( data, 3 );
                    if ( SQLITE_ROW == st ) {
                        stream << "@SQ\tSN:" << data[ 0 ] << "\tLN:" << data[ 1 ] << std::endl;
                        result . header_lines += 1;
                    }
                } while ( SQLITE_ROW == st );
            }
            return ( SQLITE_DONE == stm -> get_status() );
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

    public :
        EXPORTER( SAM_DB& a_db, const EXPORT_PARAMS& a_params, EXPORT_RESULT& a_result )
            : params( a_params ),
              result( a_result ),
              db( a_db ) { }

        void run( void ) {
            FILE_WRITER writer( params . export_filename );
            std::ostream *stream = writer . get();
            bool ok = export_headers( *stream );
            if ( ok ) { ok = export_alignments( *stream ); }
            result . success = ok;
        }

};


#endif
