#ifndef HDR_H
#define HDR_H

#include <string>
#include <algorithm>
#include "database.hpp"

struct sam_hdr_t {
    std::string value;
    
    sam_hdr_t() {}
    
    void print( std::ostream& stream ) const {
        stream << value << std::endl;
    }
};

class sam_hdr_iter_t {
    private :
        mt_database::prep_stm_ptr_t stm;
        uint64_t count;
        
        sam_hdr_iter_t( const sam_hdr_iter_t& ) = delete;
        
    public :
        sam_hdr_iter_t( mt_database::database_t& db, bool HD_hdrs ) : count( 0 ) { 
            stm = db . make_prep_stm( 
                HD_hdrs
                    ? "SELECT VALUE FROM HDR WHERE NOT VALUE LIKE '@HD%';"
                    : "SELECT VALUE FROM HDR WHERE VALUE LIKE '@HD%';", true );
        }
        
        bool next( sam_hdr_t& hdr ) {
            if ( SQLITE_ROW == stm -> get_status() ) {
                hdr . value . assign( stm -> read_string() );
                count ++;
                stm -> step();
                return true;
            }
            return false;
        }
        
        bool done( void ) { return ( SQLITE_DONE == stm -> get_status() ); }
        uint64_t get_count( void ) { return count; }
};

struct ref_hdr_t {
    std::string name;
    uint32_t length;
    std::string other;
    
    ref_hdr_t() {}
    
    void print( std::ostream& stream ) const {
        if ( other . empty() ) {
            stream << "@SQ\tSN:" << name << "\tLN:" << length << std::endl;
        } else {
            stream << "@SQ\tSN:" << name << "\tLN:" << length << "\t" << other << std::endl;                            
        }
    }
};

class ref_hdr_iter_t {
private :
    mt_database::prep_stm_ptr_t stm;
    uint64_t count;
    
    ref_hdr_iter_t( const ref_hdr_iter_t& ) = delete;
    
public :
    ref_hdr_iter_t( mt_database::database_t& db ) : count( 0 ) { 
        stm = db . make_prep_stm(  "SELECT NAME,LENGTH,OTHER FROM REF;", true );
    }
    
    bool next( ref_hdr_t& hdr ) {
        if ( SQLITE_ROW == stm -> get_status() ) {
            hdr . name . assign( stm -> read_string() );
            hdr . length = stm -> read_uint32_t( 1 );
            hdr . other . assign( stm -> read_string( 2 ) );            
            count ++;
            stm -> step();
            return true;
        }
        return false;
    }
    
    bool done( void ) { return ( SQLITE_DONE == stm -> get_status() ); }
    uint64_t get_count( void ) { return count; }
};

#endif
