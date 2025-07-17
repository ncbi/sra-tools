#pragma once

#include "csra_prim_cols.hpp"
#include "../util/random_toolbox.hpp"

using namespace std;

namespace sra_convert {

class cSRARead;
typedef std::shared_ptr< cSRARead > cSRAReadPtr;
class cSRARead {
    public :
        enum State { Aligned, UnAligned, None };

    private :
        State f_state;
        uint64_t f_align_id;
        uint64_t f_spot_id;
        uint32_t f_read_id;
        string f_bases;

        cSRARead( uint32_t read_id, uint32_t len, State state, RandomPtr rnd )
            : f_state( state ),
              f_align_id( 0 ),
              f_spot_id( 0 ),
              f_read_id( read_id ) {
                if ( state != None ) { f_bases = rnd -> random_bases( len ); }
            }

    public:

        const string& get_bases( void ) const { return f_bases; }
        State get_state( void ) const { return f_state; }
        bool is_aligned( void ) const { return f_state == Aligned; }
        uint64_t get_align_id( void ) const { return f_align_id; }
        uint64_t get_spot_id( void ) const { return f_spot_id; }
        uint32_t get_read_id( void ) const { return f_read_id; }
        uint32_t get_len( void ) const { return f_bases . length(); }

        uint64_t set_ids( uint64_t spot_id, uint64_t align_id ) {
            f_spot_id = spot_id;
            if ( f_state == Aligned ) {
                f_align_id = align_id;
                return align_id + 1;
            }
            return align_id;
        }

        static cSRAReadPtr make( uint32_t read_id,
                                 uint32_t min_len,
                                 uint32_t max_len,
                                 State state,
                                 RandomPtr rnd ) {
            uint32_t len = min_len;
            if ( len < max_len ) {
                len = rnd -> random_u32( min_len, max_len );
            }
            return cSRAReadPtr( new cSRARead( read_id, len, state, rnd ) );
        }

        bool write_prim_cols( PrimColsPtr writer ) {
            bool res = true;
            if ( f_state == Aligned ) { res = writer -> write( f_spot_id, f_read_id, f_bases ); }
            return res;
        }

        friend auto operator<<( ostream& os, cSRAReadPtr const& o ) -> ostream& {
            switch( o -> f_state ) {
                case Aligned   : os << "A"; break;
                case UnAligned : os << "U"; break;
                case None      : os << "-"; break;
            }
            os << o -> f_read_id;
            os << " Spot-ID:" << o -> f_spot_id;
            os << " Align-ID:" << o -> f_align_id;
            os << " Len:" << o -> f_bases . length() << endl;
            return os;
        }
};

}
