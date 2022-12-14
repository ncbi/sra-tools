#ifndef ANALYZER_H
#define ANALYZER_H

#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"
#include "ref_dict.hpp"
#include "spot.hpp"
#include "md5.hpp"
#include "file_writer.hpp"

class analyzer_t {
    private :
        sam_database_t &db;
        const analyze_params_t& params;
        ref_dict_t& ref_dict;
        bool produce_fingerprint;        
        analyze_result_t result;
        digest_t total_md5;
        
        analyzer_t( const analyzer_t& ) = delete;

        digest_t md5_of_alig_iter( sam_alig_iter_t::ALIG_ORDER order ) {
            sam_alig_iter_t iter( db, order );
            digest_t md5;
            sam_alig_t alig;
            while ( iter . next( alig ) ) {
                md5 . digest_xor( alig.md5() );
            }
            return md5;
        }
        
        void analyze_spots( void ) {
            sam_spot_t spot;
            sam_spot_iter_t spot_iter( db );
            while ( spot_iter . next( spot ) ) {
                result . spot_count += 1;
                result . counter_dict . add( spot .count() );
                spot . enter_into_ref_dict( ref_dict );
                switch ( spot . spot_type() ) {
                    case sam_spot_t::SPOT_ALIGNED      : result . fully_aligned += 1; break;
                    case sam_spot_t::SPOT_UNALIGNED    : result . unaligned += 1; break;
                    case sam_spot_t::SPOT_HALF_ALIGNED : result . half_aligned += 1; break;
                }
                result . alig_count += spot . count();
                spot . count_flags( result . flag_counts, result . flag_problems );

                if ( produce_fingerprint ) {
                    spot . count( result . counts );
                    total_md5 . digest_xor( spot . md5() );
                }
            }
            
        }

    public :
        analyzer_t( sam_database_t& a_db, const analyze_params_t& a_params, ref_dict_t& a_ref_dict )
            : db( a_db ), params( a_params ), ref_dict( a_ref_dict ),
              produce_fingerprint( !a_params . fingerprint . empty() ) {}

        bool run( std::ostream * sink ) {
            *sink << "ANALYZE:" << std::endl;
            
            analyze_spots();
            result . refs_in_use = ref_dict . size();
            result . success = true;
            result . report( sink, produce_fingerprint );
            
            if ( produce_fingerprint ) {
                json_doc_t j;
                j . add( "spot-count", result . spot_count );
                j . add( "alignment-count", result . alig_count );                
                j . add( "md5", total_md5 . to_string() );
                j . add( "AT", result . counts . get_AT() );
                j . add( "GC", result . counts . get_GC() );
                j . add( "N", result . counts . get_N() );
                j . add( "total", result . counts . get_total() );
                j . add( "other", result . counts . get_other() );
                
                file_writer_t writer( params . fingerprint );
                j . print( writer . get() );
            }
            
            return result . success;
        }
};

#endif
