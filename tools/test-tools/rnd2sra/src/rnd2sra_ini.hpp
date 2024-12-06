#pragma once

#include "../util/ini.hpp"
#include "../util/random_toolbox.hpp"
#include "product.hpp"
#include "bio_or_tech.hpp"
#include "fwd_or_rev.hpp"
#include "RdFilter.hpp"
#include "../vdb/checksum.hpp"
#include <cstdint>
#include <iostream>

using namespace std;
using namespace vdb;

namespace sra_convert {

class spot_section;
typedef std::shared_ptr< spot_section > spot_section_ptr;
class spot_section {
    private :
        Bio_or_Tech_ptr f_biotech;
        Fwd_or_Rev_ptr f_direction;
        RdFilter_ptr f_filter;
        size_t f_len;
        size_t f_len2;
        size_t f_random_len;

        string direction_and_filter_to_string( void ) const {
            return f_direction -> to_str() + f_filter -> to_str();
        }

        void initialize_lengths( const string& lengths ) {
            auto parts = StrTool::tokenize( lengths, '-' );
            if ( parts . size() > 1 ) {
                f_len  = StrTool::convert< size_t >( parts[ 0 ], 0 );
                f_len2 = StrTool::convert< size_t >( parts[ 1 ], 0 );
            } else if ( parts . size() > 0 ) {
                f_len  = StrTool::convert< size_t >( parts[ 0 ], 0 );
                f_len2 = f_len;
            } else {
                f_len = f_biotech -> dflt_length();
                f_len2 = f_len;
            }
            f_random_len = f_len;
        }

        void initialize_flags( const string& flags ) {
            if ( flags . empty() ) {
                f_direction = Fwd_or_Rev::make();
                f_filter = RdFilter::make();
            } else {
                f_direction = Fwd_or_Rev::make( flags );
                f_filter = RdFilter::make( flags );
            }
        }

        void initialize( const string& read_def ) {
            f_biotech = Bio_or_Tech::make( read_def[ 0 ] );
            auto lengths_flags = StrTool::tokenize( read_def . substr( 1 ), '|' );
            if ( lengths_flags . empty() ) {
                initialize_lengths( "50" );
                initialize_flags( "" );
            } else {
                initialize_lengths( lengths_flags[ 0 ] );
                if ( lengths_flags . size() > 1 ) {
                    initialize_flags( lengths_flags[ 1 ] );
                } else {
                    initialize_flags( "" );
                }
            }
        }

        // >>>>> Ctor <<<<<
        spot_section( const string& read_def ) {
            if ( read_def . empty() ) {
                initialize( "B50" );
            } else {
                initialize( read_def );
            }
        }

    public :
        static spot_section_ptr make( const string& read_def ) {
            return spot_section_ptr( new spot_section( read_def ) );
        }

        size_t get_len( void ) const { return f_random_len; }

        /*
            SRA_READ_FILTER_PASS = 0;
            SRA_READ_FILTER_REJECT = 1;
            SRA_READ_FILTER_CRITERIA = 2;
            SRA_READ_FILTER_REDACTED = 3;
         */
        uint8_t get_read_filter( void ) const {
            return f_filter -> as_u8();
        }

        /*
            SRA_READ_TYPE_TECHNICAL  = 0;
            SRA_READ_TYPE_BIOLOGICAL = 1;
            SRA_READ_TYPE_FORWARD    = 2;
            SRA_READ_TYPE_REVERSE    = 4;
         */
        uint8_t get_read_type( void ) const {
            return f_biotech -> as_u8() | f_direction -> as_u8();
        }

        uint32_t get_bio_base_count( void ) const {
            if ( f_biotech -> is_bio() ) return (uint32_t)f_random_len;
            return 0;
        }

        void randomize( RandomPtr r ) {
            if ( f_len != f_len2 ) {
                f_random_len = r -> random_u32( (uint32_t)f_len, (uint32_t)f_len2 );
            }
        }

        friend auto operator<<( ostream& os, spot_section_ptr o ) -> ostream& {
            os << o -> f_biotech -> to_char();
            os << o -> f_len << "-" << o -> f_len2 << '|' << o -> direction_and_filter_to_string();
            return os;
        }
};

class spot_layout;
typedef std::shared_ptr< spot_layout > spot_layout_ptr;
class spot_layout {
    private :
        vector< spot_section_ptr > f_sections;

        // >>>>> Ctor <<<<<
        spot_layout( const string& layout_def ) {
            for ( auto const& read_def : StrTool::tokenize( layout_def, ':' ) ) {
                f_sections . push_back( spot_section::make( read_def ) );
            }
        }

    public :
        static spot_layout_ptr make( const string& layout ) {
            return spot_layout_ptr( new spot_layout( layout ) );
        }

        const vector< spot_section_ptr >& get_sections( void ) const { return f_sections; }

        void randomize( RandomPtr r ) {
            for ( auto& section : f_sections ) { section -> randomize( r ); }
        }

        size_t get_len( void ) const {
            size_t sum = 0;
            for ( auto const& section : f_sections ) { sum += section -> get_len(); }
            return sum;
        }

        size_t section_count( void ) const { return f_sections . size(); }

        void populate_read_start( int32_t* values ) const {
            int32_t start = 0;
            uint32_t idx = 0;
            for ( auto const& section : f_sections ) {
                values[ idx++ ] = start;
                start += (uint32_t)section -> get_len();
            }
        }

        void populate_read_len( uint32_t* values ) const {
            uint32_t idx = 0;
            for ( auto const& section : f_sections ) {
                values[ idx++ ] = (uint32_t)section -> get_len();
            }
        }

        void populate_read_filter( uint8_t* values ) const {
            uint32_t idx = 0;
            for ( auto const& section : f_sections ) {
                values[ idx++ ] = section -> get_read_filter();
            }
        }

        void populate_read_type( uint8_t* values ) const {
            uint32_t idx = 0;
            for ( auto const& section : f_sections ) {
                values[ idx++ ] = section -> get_read_type();
            }
        }

        uint32_t get_bio_base_count( void ) const {
            uint32_t res = 0;
            for ( auto const& section : f_sections ) {
                res += section -> get_bio_base_count();
            }
            return res;
        }

        friend auto operator<<( ostream& os, spot_layout_ptr o ) -> ostream& {
            for ( auto const& section : o -> get_sections() ) {
                os << section << " ";
            }
            return os;
        }
};

class csra_spot_layout;
typedef std::shared_ptr< csra_spot_layout > csra_spot_layout_ptr;
class csra_spot_layout {
    public :
        enum Aligned { Aligned2Reads,
                       AlignedUnaligned,
                       UnalignedAligned,
                       UnAligned2Reads,
                       Aligned1Read,
                       Unaligned1Read,
                       Unknown };

    private :
        Aligned f_aligned;
        uint32_t f_min_len, f_max_len, f_count;

        csra_spot_layout( Aligned aligned, uint32_t min_len, uint32_t max_len, uint32_t count )
            : f_aligned( aligned ), f_min_len( min_len ), f_max_len( max_len ), f_count( count ) {}

        csra_spot_layout( void )
            : f_aligned( Aligned2Reads ), f_min_len( 50 ), f_max_len( 100 ), f_count( 20 ) {}

        static Aligned from_str( const string& s ) {
            if ( s . empty() ) return Unknown;
            switch ( s[ 0 ] ) {
                case 'F' : return Aligned2Reads;
                case '1' : return AlignedUnaligned;
                case '2' : return UnalignedAligned;
                case 'N' : return UnAligned2Reads;
                case 'A' : return Aligned1Read;
                case 'U' : return Unaligned1Read;
                default  : return Unknown;
            }
        }

        string aligned_as_str( void ) const {
            switch( f_aligned ) {
                case Aligned2Reads    : return "Aligned2Reads"; break;
                case AlignedUnaligned : return "AlignedUnaligned"; break;
                case UnalignedAligned : return "UnalignedAligned"; break;
                case UnAligned2Reads  : return "UnAligned2Reads"; break;
                case Aligned1Read     : return "Aligned1Read"; break;
                case Unaligned1Read   : return "Unaligned1Read"; break;
                default : return "Unknown"; break;
            }
        }

    public :
        static csra_spot_layout_ptr make( Aligned aligned, uint32_t min, uint32_t max, uint32_t count ) {
            return csra_spot_layout_ptr( new csra_spot_layout( aligned, min, max, count ) );
        }

        static csra_spot_layout_ptr make( const string& desc ) {
            uint32_t idx = 0;
            Aligned aligned = Unknown;
            uint32_t count = 20;
            uint32_t min_len = 50;
            uint32_t max_len = 0;
            for ( const string& part : StrTool::tokenize( desc, ':' ) ) {
                switch( idx++ ) {
                    case 0 : aligned = from_str( part ); break;
                    case 1 : count = StrTool::convert< uint32_t >( part, 0 ); break;
                    case 2 : min_len = StrTool::convert< uint32_t >( part, 0 ); break;
                    case 3 : max_len = StrTool::convert< uint32_t >( part, 0 ); break;
                }
            }
            if ( 0 == max_len ) { max_len = min_len; }
            return make( aligned, min_len, max_len, count );
        }

        Aligned get_aligned( void ) const { return f_aligned; }
        uint32_t get_min_len( void ) const { return f_min_len; }
        uint32_t get_max_len( void ) const { return f_max_len; }
        uint32_t get_count( void ) const { return f_count; }

        friend auto operator<<( ostream& os, csra_spot_layout_ptr o ) -> ostream& {
            os << o -> aligned_as_str() << " : " << o -> f_count << " : "
                << o -> f_min_len;
            if ( o -> f_min_len != o -> f_max_len ) {
                os << " : " << o -> f_max_len;
            }
            os << endl;
            return os;
        }
};

class row_offset_pair;
typedef std::shared_ptr< row_offset_pair > row_offset_pair_ptr;
class row_offset_pair {
    private:
        int64_t f_row;
        int32_t f_offset;

        row_offset_pair( int64_t row, int32_t offset )
            : f_row( row ), f_offset( offset ) {}

    public:
        static row_offset_pair_ptr make( void ) {
            return row_offset_pair_ptr( new row_offset_pair( 0, 0 ) );
        }

        static row_offset_pair_ptr make( const string& s ) {
            int64_t row = 0;
            int32_t offset = 0;
            uint32_t idx = 0 ;
            for ( auto const& part : StrTool::tokenize( s, ',' ) ) {
                switch( idx++ ) {
                    case 0 : row = StrTool::convert< int64_t >( part, 0 ); break;
                    case 1 : offset = StrTool::convert< int32_t >( part, 0 ); break;
                }
            }
            return row_offset_pair_ptr( new row_offset_pair( row, offset ) );
        }

        int64_t get_row( void ) const { return f_row; }
        int32_t get_offset( void ) const { return f_offset; }

        int32_t offset( int64_t row ) const {
            if ( f_row == row ) { return f_offset; }
            return 0;
        }

        friend auto operator<<( ostream& os, row_offset_pair_ptr o ) -> ostream& {
            os << "row : " << o -> get_row() << " offset : " << o -> get_offset() << endl;
            return os;
        }
};

class rnd2sra_ini;
typedef std::shared_ptr< rnd2sra_ini > rnd2sra_ini_ptr;
class rnd2sra_ini {
    private:
        string f_output_dir;
        string f_name_pattern;
        uint64_t f_rows;
        uint64_t f_seed;
        uint32_t f_name_len;
        bool f_with_name;
        bool f_echo_values;
        bool f_do_not_write_meta;
        Product_ptr f_product;
        Checksum_ptr f_checksum;
        vector< spot_layout_ptr > f_layouts;
        vector< csra_spot_layout_ptr > f_csra_layouts;
        vector< string > f_spot_groups;
        uint64_t f_autoinc_name;
        row_offset_pair_ptr f_qual_len_offset;
        row_offset_pair_ptr f_read_len_offset;
        row_offset_pair_ptr f_cmp_rd_fault;

        // >>>>> Ctor <<<<<
        rnd2sra_ini( const string_view& ini_filename ) {
            IniPtr f_ini = Ini::make( ini_filename );
            f_output_dir = f_ini -> get( "out", "" );
            f_rows = f_ini -> get_u64( "rows", 10 );
            f_seed = f_ini -> get_u64( "seed", 1010101 );
            f_name_len = f_ini -> get_u32( "name_len", 25 );
            f_name_pattern = f_ini -> get( "name_pattern", "" );
            f_with_name = f_ini -> get( "with_name", "yes" ) == "yes";
            f_echo_values = f_ini -> get( "echo", "no" ) == "yes";
            f_do_not_write_meta = f_ini -> get( "do_not_write_meta", "no" ) == "yes";
            f_product = Product::make( f_ini -> get( "product", "flat" ) );
            f_checksum = Checksum::make( f_ini -> get( "checksum", "none" ) );
            f_qual_len_offset = row_offset_pair::make( f_ini -> get( "qual_len_offset", "" ) );
            f_read_len_offset = row_offset_pair::make( f_ini -> get( "read_len_offset", "" ) );
            f_cmp_rd_fault = row_offset_pair::make( f_ini -> get( "cmp_rd_fault", "" ) );
            for ( const string & layout : f_ini -> get_definitions_of( "layout" ) ) {
                f_layouts . push_back( spot_layout::make( layout ) );
            }
            for ( const string & spot_group : f_ini -> get_definitions_of( "spotgroup" ) ) {
                f_spot_groups . push_back( spot_group );
            }
            for ( const string& layout : f_ini -> get_definitions_of( "spots" ) ) {
                f_csra_layouts . push_back( csra_spot_layout::make( layout ) );
            }
            f_autoinc_name = 1;
        }

        row_offset_pair_ptr get_qual_len_offset( void ) const { return f_qual_len_offset; }
        row_offset_pair_ptr get_read_len_offset( void ) const { return f_read_len_offset; }
        row_offset_pair_ptr get_cmp_rd_fault( void ) const { return f_cmp_rd_fault; }

    public:

        static rnd2sra_ini_ptr make( const string_view& ini_filename ) {
            return rnd2sra_ini_ptr( new rnd2sra_ini( ini_filename ) );
        }

        bool has_output_dir( void ) const { return !f_output_dir . empty(); }
        void set_output_dir( const string& value ) { f_output_dir = value; }
        const string& get_output_dir( void ) const { return f_output_dir; }
        uint64_t get_rows( void ) const { return f_rows; }
        uint64_t get_seed( void ) const { return f_seed; }
        bool get_with_name( void ) const { return f_with_name; }
        bool get_echo_values( void ) const { return f_echo_values; }
        bool get_do_not_write_meta( void ) const { return f_do_not_write_meta; }
        Product_ptr get_product( void ) const { return f_product; }
        Checksum_ptr get_checksum( void ) const { return f_checksum; }
        const vector< spot_layout_ptr >& get_layouts( void ) const { return f_layouts; }
        const vector< csra_spot_layout_ptr >& get_csra_layouts( void ) const { return f_csra_layouts; }

        int32_t qual_len_offset( int64_t row ) const {
            return get_qual_len_offset() -> offset( row );
        }

        int32_t read_len_offset( int64_t row ) const {
            return get_read_len_offset() -> offset( row );
        }

        bool cmp_rd_fault( int64_t row ) const {
            return ( 0 != get_cmp_rd_fault() -> offset( row ) );
        }

        spot_layout_ptr select_spot_layout( RandomPtr r ) {
            size_t count = f_layouts . size();
            if ( count == 1 ) {
                return f_layouts[ 0 ];
            } else if ( count > 1 ) {
                uint32_t selected = r -> random_u32( 0, (uint32_t)count - 1 );
                return f_layouts[ selected ];
            }
            auto layout = spot_layout::make( "T5:B50:T5:B50" );
            f_layouts . push_back( layout );
            return layout;
        }

        bool has_spot_groups( void ) const { return !f_spot_groups . empty(); }
        const string select_spot_group( RandomPtr r ) {
            size_t count = f_spot_groups . size();
            if ( count == 1 ) {
                return f_spot_groups[ 0 ];
            } else if ( count > 1 ) {
                uint32_t selected = r -> random_u32( 0, (uint32_t)count - 1 );
                return f_spot_groups[ selected ];
            }
            return string();
        }

        // every occurance of '#' is filled with a auto-inc value
        // every occurance of '%' is filled with a random-value between 1 and 100
        // every occurance of '$' is filled with a random-char between 'a' ... 'z'
        // every occurance of '&' is filled with a random-char between 'A' ... 'Z'
        string filled_name_pattern( const string& pattern, RandomPtr r ) {
            stringstream ss;
            for ( char c : pattern ) {
                switch( c ) {
                    case '#' : ss << f_autoinc_name++; break;
                    case '%' : ss << r -> random_u32( 1, 100 ); break;
                    case '$' : ss << r -> random_lower_char(); break;
                    case '&' : ss << r -> random_upper_char(); break;
                    default  : ss << c; break;
                }
            }
            return ss . str();
        }

        string name( RandomPtr r ) {
            if ( f_name_pattern . empty() ) {
                return r -> random_string( f_name_len );
            }
            return filled_name_pattern( f_name_pattern, r );
        }

        static string yes_no( bool flag ) {
            if ( flag ) return "yes";
            return "no";
        }

        friend auto operator<<( ostream& os, rnd2sra_ini_ptr o ) -> ostream& {
            os << "out      = " << o -> get_output_dir() << endl;
            os << "rows     = " << o -> get_rows() << endl;
            os << "seed     = " << o -> get_seed() << endl;
            os << "name     = " << yes_no( o -> get_with_name() ) << endl;
            os << "no meta  = " << yes_no( o -> get_do_not_write_meta() ) << endl;
            os << "prod     = " << o -> get_product() << endl;
            os << "checks   = " << o -> get_checksum() << endl;
            os << "q-ofs    = " << o -> get_qual_len_offset() << endl;
            os << "r-ofs    = " << o -> get_read_len_offset() << endl;
            os << "cmp-rd   = " << o -> get_cmp_rd_fault() << endl;
            for ( auto const& layout : o -> get_layouts() ) {
                os << "layout   = " << layout << endl;
            }
            for ( auto const& sg : o -> f_spot_groups ) {
                os << "spot_grp = " << sg << endl;
            }
            for ( auto const& layout : o -> get_csra_layouts() ) {
                os << "csra     = " << layout << endl;
            }
            return os;
        }

}; // end class rnd2sra_ini

}; // end namespace sra_convert
