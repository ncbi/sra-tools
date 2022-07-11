#include <iostream>

#include "mmap.hpp"
#include "tools.hpp"
#include "rr_lookup.hpp"

/* =======================================================================
    the tool receives all relevant columns of the ALIGNMENT-table via stdin
    GRS[tab]REF_ORIENT
    
    the tool receives these values via commandline :
        argv[ 1 ] ... file-name of ref-lookup-file
        argv[ 2 ] ... base-output-filename aka BASE
        
    the tool produces these files:
        BASE.ref_idx ... uint32_t for each ALIGNMENT the idx of the reference
        BASE.ref_ofs ... uint32_t for each ALIGNMENT the offset into the reference
        
   ======================================================================= */

class writer_t {
    public:
        writer_t( const char * base )
            : f_ref_idx( tools::make_filename( base, "ref_idx" ), std::ios::out | std::ios::binary ),
              f_ref_ofs( tools::make_filename( base, "ref_ofs" ), std::ios::out | std::ios::binary ),
              f_ref_len( tools::make_filename( base, "ref_len" ), std::ios::out | std::ios::binary ) {}

        bool is_valid( void ) {
            return f_ref_idx.is_open() && f_ref_ofs.is_open() && f_ref_len.is_open();
        }

        void write( uint32_t idx, uint32_t ofs, bool orientation, uint32_t ref_len ) {
            uint32_t v = idx & 0x7FFFFFFF;
            if ( orientation ) v |= 0x80000000;
            f_ref_idx.write( (char*) &v, sizeof v );
            f_ref_ofs.write( (char*) &ofs, sizeof ofs );
            f_ref_len.write( (char*) &ref_len, sizeof ref_len );
        }

    private:
        std::ofstream f_ref_idx;
        std::ofstream f_ref_ofs;
        std::ofstream f_ref_len;        
};

class tab_event : public tools::iter_func {
    public :
        
        virtual void operator()( uint64_t id, const std::string& s ) {
            switch( id ) {
                case 0 : grs = tools::str_2_uint64_t( s ); break;
                case 1 : ro = ( 0 == s.compare( "true" ) ) ; break;
                case 2 : ref_len = tools::str_2_uint32_t( s ); break;
            }
        }
        uint64_t grs;
        bool ro;
        uint32_t ref_len;
};

class line_event : public tools::iter_func {
    public:
        line_event( const RR_Lookup& lookup, writer_t& writer ) 
            : f_lookup( lookup ), f_writer( writer ), f_invalid( 0 ) {}
        
        virtual void operator()( uint64_t id, const std::string& s ) {
            tab_event e;
            tools::for_each_word( s, '\t', e );
            RR_Idx_result idx = f_lookup.find( e.grs + 1 );
            if ( idx.is_valid() ) {
                RR_Ofs_result ofs = f_lookup.ofs_at_idx( idx.idx(), e.grs + 1 );
                if ( ofs.is_valid() ) {
                    f_writer.write( idx.idx(), ofs.ofs(), e.ro, e.ref_len );
                } else {
                    f_invalid++;
                    std::cout << "#3" << id << " : " << s << std::endl;
                }
            } else {
                f_invalid++;
                std::cout << "#" << id << " : " << s << std::endl;
            }
        }

        uint64_t in_valid( void ) const { return f_invalid; }

    private:
        const RR_Lookup& f_lookup;
        writer_t& f_writer;
        uint64_t f_invalid;
};

int main( int argc, char *argv[] ) {
    if ( argc > 1 ) {

        std::string ref_lookup_filename = tools::make_filename( argv[ 1 ], "ref_lookup" );
        RR_Lookup lookup( ref_lookup_filename.c_str() );
        if ( lookup.valid() ) {
            writer_t writer( argv[ 1 ] );
            if ( writer.is_valid() ) {
                line_event e( lookup, writer );
                uint64_t lines = tools::for_each_line( std::cin, e );
                if ( 0 == e.in_valid() ) {
                    return 0;                
                } else {
                    std::cout << e.in_valid() << " invalid values found in " << lines << " lines" << std::endl;
                }
            } else {
                std::cout << "cannot open output-files!" << std::endl;
            }
        } else {
            std::cout << "ref-lookup-file is invalid!" << std::endl;
        }
    }
    return 1;
}
