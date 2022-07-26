#include <iostream>

#include "mmap.hpp"
#include "tools.hpp"

/* =======================================================================
    the tool receives all relevant columns of the SEQUENCE-table via stdin
    QUAL[tab]RDLEN[tab]ALIG_ID[tab]RD_FILTER
    
    the tool receives these values via commandline :
        argv[ 1 ] ... row-count of ALIGNMENT-TABLE aka ROWCOUNT
        argv[ 2 ] ... base-output-filename aka BASE
        
    the tool produces the mate-lookup-file: BASE.mate
        the content of this file is a lookup-table with a 64-bit value
        for each row in the ALIGNMENT-TABLE ( ROWCOUNT )
        each value encodes these bits
        
        63 62 61 60 59 .................................. 0
        A  B  C  D  -----------------E---------------------
        
        A ... has more than 1 alignment
        B ... bit 1 of RD_FILTER
        C ... bit 0 of RD_FILTER
        D ... 0 = is read #1, 1 = is read #2
        E ... mate alignment-id ( if zero, it is half aligned )
        
    the tool produces the split quality lookup : BASE.qual BASE.qual.idx
    
   ======================================================================= */

struct seq_row_t {
    public :
        std::string qual;
        uint16_t read_len[ 2 ];
        uint64_t alig_ids[ 2 ];
        uint8_t rd_filter[ 2 ];
        uint8_t rd_count;
};

class files_t {
    public :
        files_t( uint64_t file_size, const char * basename ) :
            f_file_size( file_size ),
            mates( tools::make_filename( basename, "mate" ), file_size, true ),
            qual_idx( tools::make_filename( basename, "qual.idx" ), file_size, true ),
            qual_file( tools::make_filename( basename, "qual" ), std::ios::out | std::ios::binary ) { }
        
        bool is_valid( void ) {
            return mates.isValid() && qual_idx.isValid() && qual_file.is_open();
        }

        MemoryMapped mates;
        MemoryMapped qual_idx;
        std::ofstream qual_file;

    private :
        uint64_t f_file_size;
};

class writer_t {
    public :
        writer_t( files_t& files ) : f_files( files ), f_offset( 0 ) { }            

        void write_row( const seq_row_t& row ) {
            if ( row.alig_ids[ 0 ] > 0 ) {
                if ( row.alig_ids[ 1 ] > 0 ) {
                    write_row_fully_aligned( row );
                } else {
                    write_row_half_aligned_0( row );
                }
            } else {
                if ( row.alig_ids[ 1 ] > 0 ) {
                    write_row_half_aligned_1( row );
                } else {
                    // fully unaligned: do nothing here...
                }
            }
        }

    private :
        files_t& f_files;
        uint64_t f_offset;
        
        uint64_t encode_mate_info( uint64_t mate_id, uint8_t rd_filter,
                                   uint8_t rd_id, uint8_t rd_count ) {
            // mask off 2 lsb ( values 1 and 2 of rd-filter ... )
            // set bit 3 ( will become the permanently set bit for has more than 1 reads... )
            uint8_t paired = rd_count > 1 ? 0x08 : 0x0;
            uint64_t tmp = ( ( rd_filter & 0x03 ) << 1 ) | paired | rd_id;
            tmp <<= 60; // shift in place...
            uint64_t res = mate_id & 0x0FFFFFFFFFFFFFFF; // mask off 4 bits
            res |= tmp;
            return res;
        }
        
        void write_quality( uint64_t align_id, const std::string& qual ) {
            uint16_t q_len = qual.length();
            f_files.qual_idx[ align_id ] = f_offset;
            f_files.qual_file.write( (char *) &q_len, sizeof( q_len ) );
            f_files.qual_file.write( (char *) qual.c_str(), q_len );
            f_offset += ( q_len += sizeof( q_len ) );
        }

        // both halfs are aligned :
        void write_row_fully_aligned( const seq_row_t& row ) {
            auto q0 = row.qual.substr( 0, row.read_len[ 0 ] );
            auto q1 = row.qual.substr( row.read_len[ 0 ], row.read_len[ 1 ] );
            auto a0 = row.alig_ids[ 0 ] - 1;
            auto a1 = row.alig_ids[ 1 ] - 1;
            write_quality( a0, q0 );
            write_quality( a1, q1 );
            f_files.mates[ a0 ] = encode_mate_info( a1 + 1, row.rd_filter[ 0 ], 0, row . rd_count );
            f_files.mates[ a1 ] = encode_mate_info( a0 + 1, row.rd_filter[ 1 ], 1, row . rd_count );
        }

        // only first half is aligned :
        void write_row_half_aligned_0( const seq_row_t& row ) {
            auto q0 = row.qual.substr( 0, row.read_len[ 0 ] );
            auto a0 = row.alig_ids[ 0 ] - 1;
            write_quality( a0, q0 );
            f_files.mates[ a0 ] = encode_mate_info( 0, row.rd_filter[ 0 ], 0, row . rd_count );
        }

        // only second half is aligned :
        void write_row_half_aligned_1( const seq_row_t& row ) {
            auto q1 = row.qual.substr( row.read_len[ 0 ], row.read_len[ 1 ] );
            auto a1 = row.alig_ids[ 1 ] - 1;
            write_quality( a1, q1 );
            f_files.mates[ a1 ] = encode_mate_info( 0, row.rd_filter[ 1 ], 1, row . rd_count );
        }

};

class rdlen_event : public tools::iter_func {
    public :
        rdlen_event( seq_row_t& row ) : f_row( row ) {}

        virtual void operator() ( uint64_t id, const std::string& s ) {
            if ( id < 2 ) { f_row . read_len[ id ] = tools::str_2_uint16_t( s );  }
        }

    private :
        seq_row_t& f_row;
};

class alig_id_event : public tools::iter_func {
    public :
        alig_id_event( seq_row_t& row ) : f_row( row ) { f_row . rd_count = 0; }

        virtual void operator() ( uint64_t id, const std::string& s ) {
            switch( id ) {
                case 0 : f_row . alig_ids[ 0 ] = tools::str_2_uint64_t( s );
                         f_row . rd_count = 1;
                         break;
                case 1 : f_row . alig_ids[ 1 ] = tools::str_2_uint64_t( s );
                         f_row . rd_count = 2;
                         break;
            }
        }
    private :
        seq_row_t& f_row;
};

class rd_filter_event : public tools::iter_func {
    public :
        rd_filter_event( seq_row_t& row ) : f_row( row ) {}

        virtual void operator() ( uint64_t id, const std::string& line ) {
            if ( id < 2 ) { f_row . rd_filter[ id ] = tools::str_2_uint8_t( line );  }
        }
    private :
        seq_row_t& f_row;
};

class tab_event : public tools::iter_func {
    public :
        tab_event( seq_row_t& row ) : f_row( row ) {}

        virtual void operator() ( uint64_t id, const std::string& s ) {
            switch ( id ) {
                case 0 : f_row.qual = s; break;
                case 1 : on_rd_len( s ); break;
                case 2 : on_alig_id( s ); break;
                case 3 : on_rd_filter( s ); break;                
            }
        }

    private :
        seq_row_t& f_row;
        
        void on_rd_len( const std::string& s ) {
            rdlen_event e( f_row ); tools::for_each_word( s, ',', e );
        }
        void on_alig_id( const std::string& s ) {
            alig_id_event e( f_row ); tools::for_each_word( s, ',', e );
        }
        void on_rd_filter( const std::string& s ) {
            rd_filter_event e( f_row ); tools::for_each_word( s, ',', e );
        }
        
};

class line_event : public tools::iter_func {
    public:
        line_event( writer_t& writer ) : f_writer( writer ) {}

        virtual void operator()( uint64_t id, const std::string& s ) {
            seq_row_t row;
            tab_event e( row );
            tools::for_each_word( s, '\t', e );
            f_writer.write_row( row );
        }

    private:
        writer_t& f_writer;
};

int main( int argc, char *argv[] ) {
    if ( argc > 2 ) {

        // get row-count from the commandline-argument #1
        uint64_t row_count = tools::str_2_uint64_t( argv[ 1 ] );
        uint64_t file_size = ( row_count + 1 ) * sizeof( row_count );
        files_t files( file_size, argv[ 2 ] );
        
        if ( files.is_valid() ) {
            writer_t writer( files );
            line_event e( writer );
            tools::for_each_line( std::cin, e );
            return 0;
        }
    }
    return 1;
}
