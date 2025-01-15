#pragma once

#include "csra_read.hpp"
#include "csra_seq_rec.hpp"
#include "rnd_cmn.hpp"

using namespace std;

namespace sra_convert {

class cSRASpot;
typedef std::shared_ptr< cSRASpot > cSRASpotPtr;
class cSRASpot {
    private :
        rnd2sra_ini_ptr f_ini;
        RandomPtr f_rnd;
        uint64_t f_spot_id;
        uint32_t f_read_count;
        cSRAReadPtr f_read1;
        cSRAReadPtr f_read2;

        cSRASpot( csra_spot_layout_ptr layout,
                  rnd2sra_ini_ptr ini,
                  RandomPtr rnd )
            : f_ini( ini ), f_rnd( rnd ), f_spot_id( 0 ), f_read_count( 0 ) {
                uint32_t min_len = layout -> get_min_len();
                uint32_t max_len = layout -> get_max_len();

                switch ( layout -> get_aligned() ) {
                    case csra_spot_layout::Aligned2Reads    : Aligned2Reads( min_len, max_len ); break;
                    case csra_spot_layout::AlignedUnaligned : AlignedUnaligned( min_len, max_len ); break;
                    case csra_spot_layout::UnalignedAligned : UnalignedAligned( min_len, max_len ); break;
                    case csra_spot_layout::UnAligned2Reads  : Unaligned2Reads( min_len, max_len ); break;
                    case csra_spot_layout::Aligned1Read     : Aligned1Read( min_len, max_len ); break;
                    case csra_spot_layout::Unaligned1Read   : Unaligned1Read( min_len, max_len ); break;
                    default : Unknown( min_len, max_len );
                }
        }

        void Aligned2Reads( uint32_t min_len, uint32_t max_len ) {
            f_read1 = cSRARead::make( 1, min_len, max_len, cSRARead::Aligned, f_rnd );
            f_read2 = cSRARead::make( 2, min_len, max_len, cSRARead::Aligned, f_rnd );
            f_read_count = 2;
        }

        void AlignedUnaligned( uint32_t min_len, uint32_t max_len ) {
            f_read1 = cSRARead::make( 1, min_len, max_len, cSRARead::Aligned, f_rnd );
            f_read2 = cSRARead::make( 2, min_len, max_len, cSRARead::UnAligned, f_rnd );
            f_read_count = 2;
        }

        void UnalignedAligned( uint32_t min_len, uint32_t max_len ) {
            f_read1 = cSRARead::make( 1, min_len, max_len, cSRARead::UnAligned, f_rnd );
            f_read2 = cSRARead::make( 2, min_len, max_len, cSRARead::Aligned, f_rnd );
            f_read_count = 2;
        }

        void Unaligned2Reads( uint32_t min_len, uint32_t max_len ) {
            f_read1 = cSRARead::make( 1, min_len, max_len, cSRARead::UnAligned, f_rnd );
            f_read2 = cSRARead::make( 2, min_len, max_len, cSRARead::UnAligned, f_rnd );
            f_read_count = 2;
        }

        void Aligned1Read( uint32_t min_len, uint32_t max_len ) {
            f_read1 = cSRARead::make( 1, min_len, max_len, cSRARead::Aligned, f_rnd );
            f_read2 = cSRARead::make( 2, min_len, max_len, cSRARead::None, f_rnd );
            f_read_count = 1;
        }

        void Unaligned1Read( uint32_t min_len, uint32_t max_len ) {
            f_read1 = cSRARead::make( 1, min_len, max_len, cSRARead::UnAligned, f_rnd );
            f_read2 = cSRARead::make( 2, min_len, max_len, cSRARead::None, f_rnd );
            f_read_count = 1;
        }

        void Unknown( uint32_t min_len, uint32_t max_len ) {
            f_read1 = cSRARead::make( 1, min_len, max_len, cSRARead::None, f_rnd );
            f_read2 = cSRARead::make( 2, min_len, max_len, cSRARead::None, f_rnd );
            f_read_count = 0;
        }

        size_t make_random_qual( uint8_t * buffer, int64_t row, size_t read_len ) {
            size_t res = read_len + f_ini -> qual_len_offset( row );
            f_rnd -> random_diff_quals( buffer, res );
            return res;
        }

    public :
        uint64_t get_spot_id( void ) const { return f_spot_id; }
        cSRAReadPtr get_read1( void ) const { return f_read1; }
        cSRAReadPtr get_read2( void ) const { return f_read2; }

        uint64_t set_ids( uint64_t spot_id, uint64_t align_id ) {
            f_spot_id = spot_id;
            uint64_t res = f_read1 -> set_ids( spot_id, align_id );
            return f_read2 -> set_ids( spot_id, res );
        }

        static cSRASpotPtr make( csra_spot_layout_ptr layout,
                                 rnd2sra_ini_ptr ini,
                                 RandomPtr rnd ) {
            return cSRASpotPtr( new cSRASpot( layout, ini, rnd ) );
        }

        void populate_seq_rec2( SeqRec& rec ) {
            // concatenate the 2 reads for the 'computed' READ-column
            rec . set_read( f_read1 -> get_bases() + f_read2 -> get_bases() );

            if ( f_read1 -> is_aligned() ) {
                if ( f_read2 -> is_aligned() ) {
                    // fully aligned :
                    rec . clear_cmp_read();
                } else {
                    // half-aligned : ALIGNED - UNALIGNED
                    rec . set_cmp_read( f_read2 -> get_bases() );
                }
            } else {
                if ( f_read2 -> is_aligned() ) {
                    // half-aligned : UNALIGNED - ALIGNED
                    rec . set_cmp_read( f_read1 -> get_bases() );
                } else {
                    // fully unaligned :
                    rec . set_cmp_read( f_read1 -> get_bases() + f_read2 -> get_bases() );
                }
            }
            rec . set_read_type( 1, 1 ); // BIO
            rec . set_read_filter( 0, 0 ); // PASS

            rec . set_read_start( 0, f_read1 -> get_len() );
            // optional fault injection!
            size_t rd_len_ofs = f_ini -> read_len_offset( rec . get_row_id() );
            rec . set_read_len( f_read1 -> get_len() + rd_len_ofs, f_read2 -> get_len() );
            rec . set_prim_al_id( f_read1 -> get_align_id(), f_read2 -> get_align_id() );
        }

        void populate_seq_rec1( SeqRec& rec ) {
            // concatenate the 2 reads for the 'computed' READ-column
            rec . set_read( f_read1 -> get_bases() );
            if ( f_read1 -> is_aligned() ) {
                rec . clear_cmp_read();
            } else {
                rec . set_cmp_read( f_read1 -> get_bases() );
            }
            rec . set_read_type( 1 ); // BIO
            rec . set_read_filter( 0 ); // PASS

            rec . set_read_start( 0 );
            rec . set_read_len( f_read1 -> get_len() );
            rec . set_prim_al_id( f_read1 -> get_align_id() );
        }

        void populate_seq_rec( SeqRec& rec, base_counters& bc ) {
            if ( f_read_count == 0 ) { return; }
            // generate a random name...
            rec . set_name( f_ini -> name( f_rnd ) );
            // pick a random spot-group
            rec . set_spot_grp( f_ini -> select_spot_group( f_rnd ) );

            if ( f_read_count == 2 ) {
                populate_seq_rec2( rec );   // default case : 2 READS per SPOT
            } else if ( f_read_count == 1 ) {
                populate_seq_rec1( rec );   // special case : 1 READ per SPOT
            }

            if ( f_ini -> cmp_rd_fault( rec . get_row_id() ) ) {
                // special treatment for SPOT's with the whole READ in the CMP_READ-column
                rec . set_cmp_read( rec . get_read() );
            }

            // optional fault injection!
            size_t spot_len = rec . get_read() . length() + f_ini -> qual_len_offset( rec . get_row_id() );
            rec . make_random_qual( f_rnd, spot_len );

            bc . bio += rec . get_read() . length();
        }

        bool write_prim_cols( PrimColsPtr writer ) {
            bool res = f_read1 -> write_prim_cols( writer );
            if ( res ) { res = f_read2 -> write_prim_cols( writer ); }
            return res;
        }

        friend auto operator<<( ostream& os, cSRASpotPtr const& o ) -> ostream& {
            os << "\tID : " << o -> f_spot_id << endl;
            os << "\t\t" << o -> f_read1;
            os << "\t\t" << o -> f_read2;
            return os;
        }
};

}
