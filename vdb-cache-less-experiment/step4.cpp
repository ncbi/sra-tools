#include <iostream>

#include "mmap.hpp"
#include "tools.hpp"
#include "rr_lookup.hpp"

/* =======================================================================
    the tool receives these columns of the ALIGNMENT-table via stdin
    SEQ_SPOT_ID[tab]MAPQ[tab]CIGAR_SHORT[tab]READ
    
    the tool receives these values via commandline :
        argv[ 1 ] ... base-input-output-filename aka BASE
        
    the tool produces the final SAM-output via stdout
        
   ======================================================================= */

class file_lookups {
    public :
        file_lookups( const char * base )
            : f_mates( tools::make_filename( base, "mate" ), 0, false ),
              f_ref_idx( tools::make_filename( base, "ref_idx" ), 0, false ),
              f_ref_ofs( tools::make_filename( base, "ref_ofs" ), 0, false ),
              f_qual_idx( tools::make_filename( base, "qual.idx" ), 0, false ),
              f_qual( tools::make_filename( base, "qual" ), 0, false ),
              f_ref_len( tools::make_filename( base, "ref_len" ), 0, false )
            {}
            
        bool valid( void ) const {
            return f_mates.isValid() && f_ref_idx.isValid() && f_ref_ofs.isValid() &&
                   f_qual_idx.isValid() && f_qual.isValid() && f_ref_len.isValid();
        }

        // id ... zero based
        uint64_t get_mate_id_and_codes( uint64_t id, uint64_t& mate_codes ) {
            uint64_t encoded_mate = f_mates[ id ];
            // then encoding of this value happened in seq_iter.cpp
            mate_codes = encoded_mate >> 60; 
            return encoded_mate & 0x0FFFFFFFFFFFFFFF; // mask off the 4 highest bits
        }
        
        uint32_t get_ref_id_and_reversed( uint64_t id, bool& reversed ) {
            uint32_t encoded = *( f_ref_idx.u32_ptr( id ) );
            reversed = ( ( encoded & 0x80000000 ) == 0x80000000 );
            return encoded & 0x7FFFFFFF;
        }

        uint32_t get_ref_ofs( uint64_t id ) { return *( f_ref_ofs . u32_ptr( id ) ) + 1; }
        uint32_t get_ref_len( uint64_t id ) { return *( f_ref_len . u32_ptr( id ) ); }
        
        uint16_t get_qual_len( uint64_t ofs ) {
            uint16_t res = *( f_qual . u8_ptr( ofs ) );
            uint16_t h = *( f_qual . u8_ptr( ofs + 1 ) );
            res += h << 8;
            return res;
        }

        std::string get_quality( uint64_t id, bool reversed ) {
            uint64_t qual_ofs = f_qual_idx[ id ];
            // first read the string-length ( u16 ) from that location
            uint16_t q_len = get_qual_len( qual_ofs );
            char * qual_ptr = ( char * )f_qual . u8_ptr( qual_ofs + 2 );
            std::string res( qual_ptr, q_len );
            if ( reversed ) { tools::reverse( res ); }
            return res;
        }

    private :
        MemoryMapped f_mates;
        MemoryMapped f_ref_idx;
        MemoryMapped f_ref_ofs;
        MemoryMapped f_qual_idx;
        MemoryMapped f_qual;
        MemoryMapped f_ref_len;
};

class tab_event : public tools::iter_func {
    public :
        
        virtual void operator()( uint64_t id, const std::string& s ) {
            switch( id ) {
                case 0 : seq_id = s; break;
                case 1 : mapq = s; break;
                case 2 : cigar = s; break;
                case 3 : read = s; break;
            }
        }
        std::string seq_id;
        std::string mapq;
        std::string cigar;
        std::string read;
};

class line_event : public tools::iter_func {
    public:
        line_event( RR_Lookup& refs, file_lookups& lookups, bool verbose = false ) 
            : f_refs( refs ), f_lookups( lookups ), f_equal_sign( "=" ) {}
        
        virtual void operator()( uint64_t id, const std::string& s ) {

            tab_event e;
            tools::for_each_word( s, '\t', e );

            uint64_t mate_codes;
            uint64_t mate_id = f_lookups . get_mate_id_and_codes( id, mate_codes );
            bool is_rd2 = ( mate_codes & 0x1 ) == 0x1;

            bool reversed = false;
            uint32_t ref_id = f_lookups . get_ref_id_and_reversed( id, reversed );

            bool mate_reversed = false;
            bool mate_ref_id = 0;

            if ( mate_id > 0 ) {
                mate_ref_id = f_lookups . get_ref_id_and_reversed( mate_id - 1, mate_reversed );
            }
            uint16_t flags = tools::calc_flags( ref_id, mate_ref_id,
                                         reversed, mate_reversed,
                                         mate_id, mate_codes );

            uint32_t ref_ofs = 0;
            uint32_t mate_ref_ofs = 0;

            RR_Name_result ref_name = f_refs.name_at_idx( ref_id );
            if ( ref_name.is_valid() ) {
                ref_ofs = f_lookups . get_ref_ofs( id );
                const std::string& rname = ref_name.name();

                // now we can print the whole SAM-record:                
                cmn_sam( e.seq_id, flags, rname, ref_ofs, e.mapq, e.cigar );

                if ( mate_id > 0 ) {
                    mate_ref_ofs = f_lookups . get_ref_ofs( mate_id - 1 );
                    if ( ref_id == mate_ref_id ) {
                        const std::string& mate_rname = f_equal_sign;
                        mated_sam( mate_rname, mate_ref_ofs );                        
                    } else {
                        RR_Name_result mate_ref_name = f_refs.name_at_idx( mate_ref_id );
                        if ( mate_ref_name.is_valid() ) {
                            const std::string& mate_rname = mate_ref_name.name();                        
                            mated_sam( mate_rname, mate_ref_ofs );
                        } else {
                            unmated_sam();
                        }
                    }
                } else {
                    unmated_sam();
                }

                int32_t tlen = 0;
                if ( mate_id > 0 && ref_id == mate_ref_id ) {
                    uint32_t ref_len = f_lookups . get_ref_len( id );
                    uint32_t mate_ref_len = f_lookups . get_ref_len( mate_id - 1 );
                    tlen = tools::calc_tlen( reversed, mate_reversed,
                                             ref_ofs, mate_ref_ofs,
                                             ref_len, mate_ref_len, is_rd2 );
                }
                std::cout << tlen << '\t';
                
                // do not reverse-complement read, it is already in the right order!

                std::string quality = f_lookups . get_quality( id, reversed );
                
                std::cout << e . read << '\t' << quality << '\t';
                   
                std::cout << std::endl;
            }
        }

    private:
        RR_Lookup& f_refs;
        file_lookups& f_lookups;
        std::string f_equal_sign;
        
        void cmn_sam( const std::string& seq_id, uint16_t flags,
                      const std::string& ref_name, uint32_t ref_ofs,
                      const std::string& mapq, const std::string& cigar ) {
            std::cout << seq_id << '\t'       // QNAME
                << flags << '\t'              // FLAGS
                << ref_name << '\t'           // RNAME
                << ref_ofs << '\t'            // RPOS
                << mapq << '\t'               // MAPQ
                << cigar << '\t';             // CIGAR
        }

        void mated_sam( const std::string& mate_rname, uint32_t mate_ref_ofs ) {
            std::cout << mate_rname << '\t'   // RNEXT
                << mate_ref_ofs << '\t';      // PNEXT            
            
        }
        
        void unmated_sam( void ) { std::cout << "*\t0\t"; }  // RNEXT PNEXT            
};

const std::string RR_Name_result::empty = "";

int main( int argc, char *argv[] ) {
    if ( argc > 1 ) {
        std::string ref_lookup_filename = tools::make_filename( argv[ 1 ], "ref_lookup" );
        RR_Lookup refs( ref_lookup_filename.c_str() );
        if ( refs.valid() ) {

            file_lookups lookups( argv[ 1 ] );
            if ( lookups.valid() ) {
                line_event e( refs, lookups );
                tools::for_each_line( std::cin, e );
            }
        } else {
            std::cout << "ref-lookup-file is invalid!" << std::endl;
        }
    }
    return 1;
}
