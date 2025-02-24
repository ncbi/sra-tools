#pragma once

#include "csra_spot.hpp"
#include "csra_seq_cols.hpp"

using namespace std;

namespace sra_convert {

class cSRASpotList;
typedef std::shared_ptr< cSRASpotList > cSRASpotListPtr;
class cSRASpotList {
    private :
        rnd2sra_ini_ptr f_ini;
        util::RandomPtr f_rnd;
        vector< cSRASpotPtr > f_spots;

        cSRASpotList( rnd2sra_ini_ptr ini, util::RandomPtr rnd ) : f_ini( ini ), f_rnd( rnd ) {}

        void add_layout( csra_spot_layout_ptr layout ) {
            for ( uint32_t idx = 0; idx < layout -> get_count(); ++idx ) {
                f_spots . push_back( cSRASpot::make( layout, f_ini, f_rnd ) );
            }
        }

        void shuffle( uint32_t requested_loops = 0 ) {
            size_t item_count = f_spots . size();
            if ( item_count < 1 ) return;
            uint32_t loops = requested_loops;
            if ( 0 == loops ) { loops = item_count * 2; }
            while ( loops > 0 ) {
                auto first = f_spots . begin();
                auto idx1 = f_rnd -> random_u32( 0, item_count - 1 );
                auto idx2 = f_rnd -> random_u32( 0, item_count - 1 );
                if ( idx1 != idx2 ) {
                    std::swap( first[ idx1 ], first[ idx2 ] );
                    loops--;
                }
            }
        }

        void set_ids( void ) {
            uint64_t spot_id = 1;
            uint64_t align_id = 1;
            for ( auto& spot : f_spots ) {
                align_id = spot -> set_ids( spot_id++, align_id );
            }
        }

    public :
        static cSRASpotListPtr make( rnd2sra_ini_ptr ini, util::RandomPtr rnd ) {
            auto res = cSRASpotListPtr( new cSRASpotList( ini, rnd ) );
            // add the list of different layouts to the spot-list
            // each layout then creates the number requested spots per layout
            for ( auto const& layout : ini -> get_csra_layouts() ) {
                res -> add_layout( layout );
            }
            res -> shuffle();
            res -> set_ids();
            return res;
        }

        uint64_t row_count( void ) const { return f_spots . size(); }

        bool write_seq_cols( SeqColsPtr writer, base_counters& bc ) {
            bool res = true;
            SeqRec rec;
            rec . set_row_id( 1 );
            for ( auto const& spot : f_spots ) {
                spot -> populate_seq_rec( rec, bc );
                res = writer -> write( rec );
                rec . inc_row_id();
                if ( !res ) break;
            }
            return res;
        }

        bool write_prim_cols( PrimColsPtr writer ) {
            bool res = true;
            for ( auto const& spot : f_spots ) {
                res = spot -> write_prim_cols( writer );
                if ( !res ) break;
            }
            return res;
        }

        friend auto operator<<( ostream& os, cSRASpotListPtr const& o ) -> ostream& {
            os << "cSRA-Spots:\n";
            for ( auto const& spot : o -> f_spots ) {
                os << spot;
            }
            return os;
        }
};


}
