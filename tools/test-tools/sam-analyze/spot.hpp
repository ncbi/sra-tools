#ifndef SPOT_H
#define SPOT_H

#include <vector>
#include "alig.hpp"
#include "ref_dict.hpp"
#include "database.hpp"
#include "result.hpp"

struct SAM_SPOT {
    typedef enum SPOT_TYPE{ SPOT_ALIGNED, SPOT_HALF_ALIGNED, SPOT_UNALIGNED } SPOT_TYPE;
    std::vector< SAM_ALIG > aligs;
    
    void clear( SAM_ALIG& alig ) {
        aligs . clear();
        if ( ! alig . NAME . empty() ) { aligs . push_back( alig ); }
    }
    
    bool add( SAM_ALIG& alig ) {
        bool res = true;
        if ( aligs . empty() ) {
            aligs . push_back( alig );
        } else {
            auto last = aligs . back();
            res = ( alig . same_name( last ) );
            if ( res ) {
                aligs . push_back( alig );
            }
        }
        return res;
    }
    
    size_t count( void ) { return aligs . size(); }
    
    size_t aligned_count( void ) {
        size_t res = 0;
        for ( auto & alig : aligs ) {
            if ( alig . RNAME != "*" ) res++;
        }
        return res;
    }
    
    void enter_into_reflist( ref_dict_t& ref_dict ) {
        for ( auto & alig : aligs ) {
            ref_dict . add ( alig . RNAME );
        }
    }
    
    SPOT_TYPE spot_type( void ) {
        SPOT_TYPE res;
        size_t ac = aligned_count();
        if ( ac == count() ) {
            res = SPOT_ALIGNED;
        } else if ( ac == 0 ) {
            res = SPOT_UNALIGNED;
        } else {
            res = SPOT_HALF_ALIGNED;
        }
        return res;
    }
    
    void count_flags( FLAG_COUNTS& flag_counts, uint64_t& flag_problems ) {
        for ( auto & alig : aligs ) {
            if ( alig . paired_in_seq() ) { flag_counts . paired_in_seq ++; }
            if ( alig . propper_mapped() ) { flag_counts . propper_mapped ++; }
            if ( alig . unmapped() ) { flag_counts . unmapped ++; }
            if ( alig . mate_unmapped() ) { flag_counts . mate_unmapped ++; }
            if ( alig . reversed() ) { flag_counts . reversed ++; }
            if ( alig . mate_reversed() ) { flag_counts . mate_reversed ++; }
            if ( alig . first_in_pair() ) { flag_counts . first_in_pair ++; }
            if ( alig . second_in_pair() ) { flag_counts . second_in_pair ++; }
            if ( alig . not_primary() ) { flag_counts . not_primary ++;  }
            if ( alig . bad_qual() ) { flag_counts . bad_qual ++; }
            if ( alig . duplicate() ) { flag_counts . duplicate ++; }
        }
        
    }
};

class SPOT_ITER {
    private :
        ALIG_ITER alig_iter;
        SAM_ALIG alig;
        
        SPOT_ITER( const SPOT_ITER& ) = delete;
        
    public :
        SPOT_ITER( mt_database::DB& db ) : alig_iter( db, ALIG_ITER::ALIG_ORDER_NAME ) { }
        
        bool next( SAM_SPOT& spot ) {
            bool done = false;
            bool res = false;
            spot . clear( alig );
            while ( !done ) {
                res = alig_iter . next( alig );
                if ( res ) {
                    if ( spot . add( alig ) ) {
                        // belongs to the current spot, continue
                    } else {
                        done = true;
                        // no: different name, keep it in alig for the next call
                    }
                } else {
                    // no more alignments in the alig-iter
                    done = true;
                }
            }
            return res;
        }
};

#endif
