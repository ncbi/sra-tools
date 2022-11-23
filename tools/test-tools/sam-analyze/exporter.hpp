#ifndef EXPORTER_H
#define EXPORTER_H

#include <algorithm>

#include "line_split.hpp"
#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"
#include "ref_dict.hpp"
#include "spot.hpp"
#include "hdr.hpp"

class exporter_t {
    private :
        const export_params_t& params;
        sam_database_t &db;
        ref_dict_t &ref_dict;
        export_result_t result;
        
        exporter_t( const exporter_t& ) = delete;
        
        bool export_HD_headers( std::ostream& stream, bool HD_hdrs ) {
            sam_hdr_t hdr;
            sam_hdr_iter_t iter( db, HD_hdrs );
            while ( iter . next( hdr ) ) { hdr . print( stream ); }
            result . header_lines += iter . get_count();
            return iter . done();
        }

        bool export_all_ref_headers( std::ostream& stream ) {
            ref_hdr_t hdr;
            ref_hdr_iter_t iter( db );
            while ( iter . next( hdr ) ) { hdr . print( stream ); }
            result . header_lines += iter . get_count();
            return iter . done();
        }

        bool export_ref_headers_in_dict( ref_dict_t &dict, std::ostream& stream ) {
            ref_hdr_t hdr;
            ref_hdr_iter_t iter( db );
            while ( iter . next( hdr ) ) {
                if ( dict . contains( hdr . name ) ) { hdr . print( stream ); }
            }
            result . header_lines += iter . get_count();
            return iter . done();
        }
        
        bool export_all_alignments( std::ostream& stream ) {
            bool show_progress = params . cmn . progress;
            sam_alig_t alig;            
            sam_alig_iter_t iter( db, params . alig_order );
            while ( iter . next( alig ) ) {
                if ( params . fix_names ) { alig . fix_name(); }
                alig . print( stream );
                if ( show_progress && 
                     ( iter . get_count() % params . cmn . transaction_size ) == 0 ) {
                        std::cerr << '.';
                    }
            }
            if ( show_progress ) { std::cerr << std::endl; }
            result . alignment_lines += iter . get_count();
            return iter . done();
        }

        bool export_some_alignments( std::ostream& stream ) {
            bool show_progress = params . cmn . progress;
            sam_alig_t alig;            
            sam_alig_iter_t iter( db, params . alig_order );
            while ( iter . next( alig ) && iter . get_count() < params . spot_limit ) {
                if ( params . fix_names ) { alig . fix_name(); }
                alig . print( stream );
                if ( show_progress && 
                    ( iter . get_count() % params . cmn . transaction_size ) == 0 ) {
                    std::cerr << '.';
                    }
            }
            if ( show_progress ) { std::cerr << std::endl; }
            result . alignment_lines += iter . get_count();
            return iter . done();
        }
        
        void populate_all_ref_dict( void ) {
            sam_alig_rname_t alig;
            // it does not matter which order, we want all of them, so pick the fastest statement possible
            // we do not need name here, but it is part of the interface of sam_alig_rname_iter_t
            sam_alig_rname_iter_t iter( db, sam_alig_iter_t::ALIG_ORDER::ALIG_ORDER_NONE );
            while ( iter . next( alig ) ) {
                ref_dict . add( alig . RNAME );
            }
        }

        void populate_some_ref_dict( ref_dict_t& dict ) {
            sam_spot_rname_t spot;  // special spot, contains only NAME and RNAME
            // since the given limit is on the number of spots - not the number of alignments 
            // and the order does matter, but we need only the RNAME out of the iterator
            // we need a special spot-iterator here...

            sam_spot_rname_iter_t iter( db );
            while ( iter . next( spot ) && iter . get_count() < params . spot_limit ) {
                spot . enter_into_ref_dict( dict );
            }
        }
        
        void produce_ref_report( void ) {
            file_writer_t writer( params . ref_report_filename );
            std::ostream& stream( *( writer . get() ) );
            ref_by_freq_t ref_by_freq( ref_dict );
            ref_freq_t entry;
            while ( ref_by_freq . next( entry ) ) {
                stream << entry . name << " : " << entry . count << std::endl;
            }
        }

        /* ------------------------------------------------------------------ */

        bool export_all_spots_only_used_refs( std::ostream& stream ) {
            bool res = export_HD_headers( stream, false );
            if ( res ) { res = export_ref_headers_in_dict( ref_dict, stream ); }
            if ( res ) { res = export_HD_headers( stream, true ); }
            if ( res ) { res = export_all_alignments( stream ); }
            return res;
        }
        
        bool export_all_spots_all_refs( std::ostream& stream ) {
            bool res = export_HD_headers( stream, false );
            if ( res ) { res = export_all_ref_headers( stream ); }
            if ( res ) { res = export_HD_headers( stream, true ); }
            if ( res ) { res = export_all_alignments( stream ); }
            return res;
        }
        
        bool export_all_spots( std::ostream& stream ) {
            if ( params . only_used_refs ) {
                return export_all_spots_only_used_refs( stream );
            }
            return export_all_spots_all_refs( stream );
        }

        /* ------------------------------------------------------------------ */

        bool export_some_spots_only_used_refs( std::ostream& stream ) {
            bool res = export_HD_headers( stream, false );
            if ( res ) {
                ref_dict_t used_in_first_n_spots;
                populate_some_ref_dict( used_in_first_n_spots );
                res = export_ref_headers_in_dict( used_in_first_n_spots, stream );
            }
            if ( res ) { res = export_HD_headers( stream, true ); }
            if ( res ) { res = export_some_alignments( stream ); }            
            return res;
        }

        bool export_some_spots_all_refs( std::ostream& stream ) {
            bool res = export_HD_headers( stream, false );
            if ( res ) { res = export_all_ref_headers( stream ); }
            if ( res ) { res = export_HD_headers( stream, true ); }
            if ( res ) { res = export_some_alignments( stream ); }
            return res;
        }
        
        bool export_some_spots( std::ostream& stream ) {
            if ( params . only_used_refs ) {
                return  export_some_spots_only_used_refs( stream );
            }
            return export_some_spots_all_refs( stream );                
        }

        /* ------------------------------------------------------------------ */
        
    public :
        exporter_t( sam_database_t& a_db, const export_params_t& a_params, ref_dict_t& a_ref_dict )
            : params( a_params ), db( a_db ), ref_dict( a_ref_dict ) { }

        bool run() {
            if (  params . cmn . report ) {
                std::cerr << "EXPORT:" << std::endl;
            }
            if ( params . only_used_refs && ref_dict . size() == 0 ) {
                populate_all_ref_dict();
            }
            file_writer_t writer( params . export_filename );
            std::ostream *stream = writer . get();
            
            if ( 0 == params . spot_limit ) {
                result . success = export_all_spots( *stream );
            } else {
                result . success = export_some_spots( *stream );
            }

            if ( result . success && params . cmn . report ) { result . report(); }            
            if ( result . success && !params . ref_report_filename . empty() ) { 
                produce_ref_report();
            }

            return result . success;
        }
};


#endif
