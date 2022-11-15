#ifndef ANALYZER_H
#define ANALYZER_H

#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"
#include "ref_dict.hpp"
#include "spot.hpp"

class ANALYZER {
    private :
        const ANALYZE_PARAMS& params;
        ref_dict_t& ref_dict;
        SAM_DB &db;
        ANALYZE_RESULT result;
        
        ANALYZER( const ANALYZER& ) = delete;

        void analyze_spots( void ) {
            SAM_SPOT spot;
            SPOT_ITER spot_iter( db );
            while ( spot_iter . next( spot ) ) {
                result . spot_count += 1;
                result . counter_dict . add( spot .count() );
                spot . enter_into_reflist( ref_dict );
                switch ( spot . spot_type() ) {
                    case SAM_SPOT::SPOT_ALIGNED      : result . fully_aligned += 1; break;
                    case SAM_SPOT::SPOT_UNALIGNED    : result . unaligned += 1; break;
                    case SAM_SPOT::SPOT_HALF_ALIGNED : result . half_aligned += 1; break;
                }
                spot . count_flags( result . flag_counts, result . flag_problems );
            }
        }

    public :
        ANALYZER( SAM_DB& a_db, const ANALYZE_PARAMS& a_params, ref_dict_t& a_ref_dict )
            : params( a_params ), db( a_db ), ref_dict( a_ref_dict ) { }

        bool run( void ) {
            std::cerr << "ANALYZE:" << std::endl;
            analyze_spots();
            result . refs_in_use = ref_dict . size();
            result . success = true;
            result . report();
            return result . success;
        }
};

#endif
