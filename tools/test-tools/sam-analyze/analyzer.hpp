#ifndef ANALYZER_H
#define ANALYZER_H

#include "params.hpp"
#include "result.hpp"
#include "sam_db.hpp"

class SPOT_ITER {
    private :
        SAM_DB &db;

        SPOT_ITER( const SPOT_ITER& ) = delete;

    public :
        SPOT_ITER( SAM_DB &a_db ) : db( a_db ) { }
};

class ANALYZER {
    private :
        const ANALYZE_PARAMS& params;
        ANALYZE_RESULT& result;
        SAM_DB &db;

        ANALYZER( const ANALYZER& ) = delete;
        
    public :
        ANALYZER( SAM_DB& a_db, const ANALYZE_PARAMS& a_params, ANALYZE_RESULT& a_result )
            : params( a_params ),
              result( a_result ),
              db( a_db ) { }

        void run( void ) {
            result . spot_count = db . spot_count();
            result . refs_in_use = db . refs_in_use_count();

            result . unaligned = db . count_unaligned();
            result . fully_aligned = db . count_fully_aligned();
            result . half_aligned = db . count_half_aligned();
            
            std::shared_ptr< mt_database::PREP_STM > get_spot_counts =
                db . make_prep_stm( "select count(NAME),CNT from SPOTS group by CNT;" );
            unsigned long v1, v2;
            while ( get_spot_counts -> read_2_longs( v1, v2 ) ) {
                result . add_spot_size( v1, v2 );
            }
            result . success = true;
        }
};

#endif
