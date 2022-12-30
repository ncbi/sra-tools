#ifndef ALIG_H
#define ALIG_H

#include <string>
#include <algorithm>
#include "database.hpp"

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
