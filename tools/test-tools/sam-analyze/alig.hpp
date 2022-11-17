#ifndef ALIG_H
#define ALIG_H

#include <string>
#include <algorithm>
#include "database.hpp"

struct SAM_ALIG {
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
    
    SAM_ALIG() {}
    
    void print( std::ostream& stream ) const {
        stream << NAME << "\t" << FLAGS << "\t" << RNAME << "\t" << RPOS << "\t" << MAPQ << "\t"
        << CIGAR << "\t" << MRNAME << "\t" << MRPOS << "\t" << TLEN << "\t" << SEQ << "\t"
        << QUAL << "\t" << TAGS << std::endl;
    }
    
    void fix_name( void ) {
        std::replace( NAME . begin(), NAME . end(), ' ', '_' );
    }
    
    bool same_name( const SAM_ALIG& other ) const {
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
};

class ALIG_ITER {
    private :
        mt_database::PREP_STM_PTR stm;
        
        ALIG_ITER( const ALIG_ITER& ) = delete;
        
    public :
        typedef enum ALIG_ORDER{ ALIG_ORDER_NONE, ALIG_ORDER_NAME, ALIG_ORDER_REFPOS } ALIG_ORDER;
        
        ALIG_ITER( mt_database::DB& db, ALIG_ORDER spot_order ) { 
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
            stm = db . make_prep_stm( sst );
            stm -> step();
        }
        
        bool next( SAM_ALIG& alig ) {
            bool res = ( SQLITE_ROW == stm -> get_status() );
            if ( res ) {
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
                stm -> step();
            }
            return res;;
        }
};

#endif
