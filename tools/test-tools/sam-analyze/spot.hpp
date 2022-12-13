#ifndef SPOT_H
#define SPOT_H

#include <vector>
#include "alig.hpp"
#include "ref_dict.hpp"
#include "database.hpp"
#include "result.hpp"
#include "md5.hpp"

struct sam_spot_t {
    typedef enum SPOT_TYPE{ SPOT_ALIGNED, SPOT_HALF_ALIGNED, SPOT_UNALIGNED } SPOT_TYPE;
    std::vector< sam_alig_t > aligs;
    
    void clear( sam_alig_t& alig ) {
        aligs . clear();
        if ( ! alig . NAME . empty() ) { aligs . push_back( alig ); }
    }
    
    bool add( sam_alig_t& alig ) {
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
    
    void enter_into_ref_dict( ref_dict_t& ref_dict ) {
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
    
    void count_flags( flag_count_t& flag_counts, uint64_t& flag_problems ) {
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
    
    void count( IUPAC_counts_t& counts  ) {
        for ( auto & alig : aligs ) {
            if ( ! alig . not_primary() ) { alig . count( counts ); }
        }
    }
    
    digest_t md5( void ) {
        digest_t res;
        for ( auto & alig : aligs ) {
            res . digest_xor( alig . md5() );
        }
        return res;
    }
};

class sam_spot_iter_t {
    private :
        sam_alig_iter_t iter;
        uint64_t count;
        sam_alig_t alig;
        
        sam_spot_iter_t( const sam_spot_iter_t& ) = delete;
        
    public :
        sam_spot_iter_t( mt_database::database_t& db ) 
            : iter( db, sam_alig_iter_t::ALIG_ORDER_NAME ), count( 0 ) { }
        
        bool next( sam_spot_t& spot ) {
            bool done = false;
            bool keep = false;
            spot . clear( alig ); // eventually add kept, not-empty alig
            while ( !done ) {
                if ( iter . next( alig ) ) {
                    if ( spot . add( alig ) ) {
                        // belongs to the current spot, continue
                    } else {
                        done = keep = true; // no: different name, keep it in alig for the next call
                    }
                } else {
                    done = true; // no more alignments in the alig-iter
                }
            }
            bool res = ( spot . count() > 0 );
            if ( res ) { count++; }
            if ( !keep ) { alig . NAME . clear(); };
            return res;
        }
        
        uint64_t get_count( void ) const { return count; }
};

struct sam_spot_rname_t {
    std::vector< sam_alig_rname_t > aligs;
    
    void clear( sam_alig_rname_t& alig ) {
        aligs . clear();
        if ( ! alig . NAME . empty() ) { aligs . push_back( alig ); }
    }
    
    bool add( sam_alig_rname_t& alig ) {
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

    void enter_into_ref_dict( ref_dict_t& ref_dict ) {
        for ( auto & alig : aligs ) {
            ref_dict . add ( alig . RNAME );
        }
    }
    
    size_t count( void ) { return aligs . size(); }
};

class sam_spot_rname_iter_t {
private :
    sam_alig_rname_iter_t iter;
    uint64_t count;
    sam_alig_rname_t alig;
    
    sam_spot_rname_iter_t( const sam_spot_rname_iter_t& ) = delete;
    
public :
    sam_spot_rname_iter_t( mt_database::database_t& db )
        : iter( db, sam_alig_iter_t::ALIG_ORDER_NAME ), count( 0 ) { }
    
    bool next( sam_spot_rname_t& spot ) {
        bool done = false;
        bool keep = false;
        spot . clear( alig ); // eventually add kept, not-empty alig
        while ( !done ) {
            if ( iter . next( alig ) ) {
                if ( spot . add( alig ) ) {
                    // belongs to the current spot, continue
                } else {
                    done = keep = true;  // no: different name, keep it in alig for the next call
                }
            } else {
                done = true; // no more alignments in the alig-iter
            }
        }
        bool res = ( spot . count() > 0 );
        if ( res ) { count++; }
        if ( !keep ) { alig . NAME . clear(); }
        return res;
    }
    
    uint64_t get_count( void ) const { return count; }
};

#endif
