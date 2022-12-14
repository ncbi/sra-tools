#ifndef ALIG_H
#define ALIG_H

#include <string>
#include <algorithm>
#include "database.hpp"
#include "tools.hpp"
#include "md5.hpp"

class IUPAC_counts_t {
    private:
        uint64_t A;     /* Adenine for sure */
        uint64_t C;     /* Cytosine for sure */
        uint64_t G;     /* Guanine for sure */
        uint64_t T;     /* Thymine for sure ( or Uracil ) */

        /* ambiguities: */
        uint64_t R;     /* A or G */
        uint64_t Y;     /* C or T */
        uint64_t S;     /* G or C */
        uint64_t W;     /* A or T */
        uint64_t K;     /* G or T */
        uint64_t M;     /* A or C */
        uint64_t B;     /* C or G or T */
        uint64_t D;     /* A or G or T */
        uint64_t H;     /* A or C or T */
        uint64_t V;     /* A or C or G */
        uint64_t N;     /* dont know */
        uint64_t gap;   /* '.' or '-' */
        
        uint64_t invalid;    
        uint64_t total;    
    
        uint64_t *ascii_2_ptr[ 256 ];
        
    public:

        IUPAC_counts_t() : A( 0 ), C( 0 ), G( 0 ), T( 0 ),
            R( 0 ), Y( 0 ), S( 0 ), W( 0 ), K( 0 ), M( 0 ),
            B( 0 ), D( 0 ), H( 0 ), V( 0 ), N( 0 ), gap( 0 ),
            invalid( 0 ), total( 0 ) {
                for ( uint16_t i = 0; i < 256; ++i ) {
                    ascii_2_ptr[ i ] = &invalid;
                }
                ascii_2_ptr[ 45 ] = &gap;   ascii_2_ptr[ 46 ]  = &gap;
                ascii_2_ptr[ 65 ] = &A;     ascii_2_ptr[ 97 ]  = &A;
                ascii_2_ptr[ 66 ] = &B;     ascii_2_ptr[ 98 ]  = &B;
                ascii_2_ptr[ 67 ] = &C;     ascii_2_ptr[ 99 ]  = &C;
                ascii_2_ptr[ 68 ] = &D;     ascii_2_ptr[ 100 ] = &D;
                ascii_2_ptr[ 71 ] = &G;     ascii_2_ptr[ 103 ] = &G;
                ascii_2_ptr[ 72 ] = &H;     ascii_2_ptr[ 104 ] = &H;
                ascii_2_ptr[ 75 ] = &K;     ascii_2_ptr[ 107 ] = &K;
                ascii_2_ptr[ 77 ] = &M;     ascii_2_ptr[ 109 ] = &M;
                ascii_2_ptr[ 78 ] = &N;     ascii_2_ptr[ 110 ] = &N;
                ascii_2_ptr[ 82 ] = &R;     ascii_2_ptr[ 114 ] = &R;
                ascii_2_ptr[ 83 ] = &S;     ascii_2_ptr[ 115 ] = &S;
                ascii_2_ptr[ 84 ] = &T;     ascii_2_ptr[ 116 ] = &T;
                ascii_2_ptr[ 85 ] = &T;     ascii_2_ptr[ 117 ] = &T;
                ascii_2_ptr[ 86 ] = &V;     ascii_2_ptr[ 118 ] = &V;
                ascii_2_ptr[ 87 ] = &W;     ascii_2_ptr[ 119 ] = &W;
                ascii_2_ptr[ 89 ] = &Y;     ascii_2_ptr[ 121 ] = &Y;
        }
    
        uint64_t get_AT( void ) { return A + T; }
        uint64_t get_GC( void ) { return G + C; }
        uint64_t get_N( void ) { return N; }
        uint64_t get_total( void ) { return total; }
        uint64_t get_other( void ) { return invalid + R + Y + S + W + K + M + B + D + H + V + gap; }
        
        void count( const std::string& s ) {
            for ( auto &c : s ) { ( *( ascii_2_ptr[ c & 0x0FF ] ) )++; }
            total += s . length();
        }

        void report_if( std::ostream * sink, uint64_t value, const char * txt ) {
            if ( value > 0 ) {
                *sink << "\t" << txt << " : " << tools_t::with_ths( value ) << std::endl;                
            }
        }
        
        void report( std::ostream * sink, bool full ) {
            *sink << "\tAT    : " << tools_t::with_ths( A + T ) << std::endl;
            *sink << "\tCG    : " << tools_t::with_ths( C + G ) << std::endl;
            report_if( sink, N,       "N    " );
            report_if( sink, invalid, "inval" );            
            *sink << "\ttotal : " << tools_t::with_ths( total ) << std::endl;
            if ( full ) {
                *sink << "\tA : " << tools_t::with_ths( A ) << std::endl;
                *sink << "\tC : " << tools_t::with_ths( C ) << std::endl;
                *sink << "\tG : " << tools_t::with_ths( G ) << std::endl;
                *sink << "\tT : " << tools_t::with_ths( T ) << std::endl;
                report_if( sink, R, "R" );
                report_if( sink, Y, "Y" );
                report_if( sink, S, "S" );
                report_if( sink, W, "W" );
                report_if( sink, K, "K" );
                report_if( sink, M, "M" );
                report_if( sink, B, "B" );
                report_if( sink, D, "D" );
                report_if( sink, H, "H" );
                report_if( sink, V, "V" );
                report_if( sink, gap, "gap" );
            }
        }
};

struct sam_alig_t {
    std::string NAME;
    uint16_t FLAGS;
    std::string RNAME;
    int32_t RPOS;
    uint32_t MAPQ;
    std::string CIGAR;
    std::string MRNAME;
    int32_t MRPOS;
    int32_t TLEN;
    std::string SEQ;
    std::string QUAL;
    std::string TAGS;
    
    sam_alig_t() {}
    
    void print( std::ostream& stream ) const {
        stream << NAME << "\t" << FLAGS << "\t" << RNAME << "\t" << RPOS << "\t" << MAPQ << "\t"
        << CIGAR << "\t" << MRNAME << "\t" << MRPOS << "\t" << TLEN << "\t" << SEQ << "\t"
        << QUAL << "\t" << TAGS << std::endl;
    }
    
    void fix_name( void ) {
        std::replace( NAME . begin(), NAME . end(), ' ', '_' );
    }
    
    bool same_name( const sam_alig_t& other ) const {
        return ( NAME == other . NAME );
    }
    
    bool paired_in_seq( void )  { return ( ( FLAGS & 0x01 ) == 0x01 ); }
    bool propper_mapped( void ) { return ( ( FLAGS & 0x02 ) == 0x02 ); }
    bool unmapped( void ) { return ( ( FLAGS & 0x04 ) == 0x04 ); }
    bool mate_unmapped( void ) { return ( ( FLAGS & 0x08 ) == 0x08 ); }
    bool reversed( void ) { return ( ( FLAGS & 0x10 ) == 0x10 ); }
    bool mate_reversed( void ) { return ( ( FLAGS & 0x20 ) == 0x20 ); }
    bool first_in_pair( void ) { return ( ( FLAGS & 0x40 ) == 0x40 ); }
    bool second_in_pair( void ) { return ( ( FLAGS & 0x80 ) == 0x80 ); }
    bool not_primary( void ) { return ( ( FLAGS & 0x100 ) == 0x100 ); }
    bool bad_qual( void ) { return ( ( FLAGS & 0x200 ) == 0x200 ); }
    bool duplicate( void ) { return ( ( FLAGS & 0x400 ) == 0x400 ); }
    
    void count( IUPAC_counts_t& counts  ) { counts. count( SEQ ); };

    digest_t md5( void ) {
        MD5 md5_1( SEQ );
        MD5 md5_2( tools_t::reverse_complement( SEQ ) );        
        digest_t res = md5_1.get_digest();
        res . digest_xor( md5_2 . get_digest() );
        return res;
    }
};

class sam_alig_iter_t {
    private :
        mt_database::prep_stm_ptr_t stm;
        uint64_t count;
        
        sam_alig_iter_t( const sam_alig_iter_t& ) = delete;
        
    public :
        typedef enum ALIG_ORDER{ ALIG_ORDER_NONE, ALIG_ORDER_NAME, ALIG_ORDER_REFPOS } ALIG_ORDER;
        
        sam_alig_iter_t( mt_database::database_t& db, ALIG_ORDER spot_order ) : count( 0 ) { 
            std::string cols( "NAME,FLAG,RNAME,RPOS,MAPQ,CIGAR,MRNAME,MRPOS,TLEN,SEQ,QUAL,TAGS" );
            std::string sstmu( "SELECT " + cols + " FROM ALIG;" );
            std::string sstms( "SELECT " + cols + " FROM ALIG order by RNAME,RPOS;" );
            std::string sstmn( "SELECT " + cols + " FROM ALIG order by NAME;" );
            std::string sst;
            switch( spot_order ) {
                case ALIG_ORDER_NAME   : sst = sstmn; break;
                case ALIG_ORDER_REFPOS : sst = sstms; break;
                default : sst = sstmu; break;
            };
            stm = db . make_prep_stm( sst, true );
        }
        
        bool next( sam_alig_t& alig ) {
            if ( SQLITE_ROW == stm -> get_status() ) {
                alig . NAME . assign( stm -> read_string( 0 ) );
                alig . FLAGS = stm -> read_uint32_t( 1 );
                alig . RNAME . assign( stm -> read_string( 2 ) );
                alig . RPOS = stm -> read_int32_t( 3 );
                alig . MAPQ = stm -> read_uint32_t( 4 );
                alig . CIGAR . assign( stm -> read_string( 5 ) );
                alig . MRNAME . assign( stm -> read_string( 6 ) );
                alig . MRPOS = stm -> read_int32_t( 7 );
                alig . TLEN = stm -> read_int32_t( 8 );
                alig . SEQ . assign( stm -> read_string( 9 ) );
                alig . QUAL . assign( stm -> read_string( 10 ) );
                alig . TAGS . assign( stm -> read_string( 11 ) );
                count++;
                stm -> step();
                return true;
            } else {
                alig . NAME . clear();                
            }
            return false;
        }
        
        bool done( void ) { return ( SQLITE_DONE == stm -> get_status() ); }
        uint64_t get_count( void ) { return count; }
};

struct sam_alig_rname_t {
    std::string NAME;
    std::string RNAME;
    
    sam_alig_rname_t() {}
    
    bool same_name( const sam_alig_rname_t& other ) const {
        return ( NAME == other . NAME );
    }
};

class sam_alig_rname_iter_t {
private :
    mt_database::prep_stm_ptr_t stm;
    uint64_t count;
    
    sam_alig_rname_iter_t( const sam_alig_rname_iter_t& ) = delete;
    
public :
    sam_alig_rname_iter_t( mt_database::database_t& db, sam_alig_iter_t::ALIG_ORDER spot_order ) : count( 0 ) { 
        std::string cols( "NAME,RNAME" );
        std::string sstmu( "SELECT " + cols + " FROM ALIG;" );
        std::string sstms( "SELECT " + cols + " FROM ALIG order by RNAME,RPOS;" );
        std::string sstmn( "SELECT " + cols + " FROM ALIG order by NAME;" );
        std::string sst;
        switch( spot_order ) {
            case sam_alig_iter_t::ALIG_ORDER_NAME   : sst = sstmn; break;
            case sam_alig_iter_t::ALIG_ORDER_REFPOS : sst = sstms; break;
            default : sst = sstmu; break;
        };
        stm = db . make_prep_stm( sst, true );
    }
    
    bool next( sam_alig_rname_t& alig ) {
        if ( SQLITE_ROW == stm -> get_status() ) {
            alig . NAME . assign( stm -> read_string() );
            alig . RNAME . assign( stm -> read_string( 1 ) );
            count++;
            stm -> step();
            return true;
        }
        return false;
    }
    
    bool done( void ) { return ( SQLITE_DONE == stm -> get_status() ); }
    uint64_t get_count( void ) { return count; }
};

#endif
