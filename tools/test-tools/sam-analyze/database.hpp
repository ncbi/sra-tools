#ifndef MT_DATABASE_H
#define MT_DATABASE_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <memory>
#include "sqlite3.h"

namespace mt_database {

struct db_func_t {
    static void print_status( int16_t status ) {
        const char * e = sqlite3_errstr( status );
        std::cerr << "status:" << e << std::endl;
    }
};

class prep_stm_t {
    private :
        sqlite3 *db;
        sqlite3_stmt * stmt;
        int16_t status;

    public :
        typedef std::vector< std::string > str_vec;
       
        prep_stm_t( sqlite3* a_db, const std::string& sql, bool and_step = false )
            : db( a_db ), stmt( nullptr ) {
            status = sqlite3_prepare_v2( db, sql.c_str(), sql.length(), &stmt, nullptr );
            if ( and_step && SQLITE_OK == status ) {
                status = sqlite3_step( stmt );
            }
        }

        ~prep_stm_t( void ) { sqlite3_finalize( stmt ); }

        bool ok_or_done( int16_t st ) { return ( SQLITE_OK == st || SQLITE_DONE == st ); }

        int16_t get_status( void ) const { return status; }
        
        void print_status( void ) const { db_func_t::print_status( status ); }

        int16_t step( void ) { 
            status =  sqlite3_step( stmt );
            return status;
        }

        int16_t reset( void ) {
            if ( ok_or_done( status ) ) { status = sqlite3_reset( stmt ); }
            return status;
        }

        int16_t bind_str_vec( const str_vec& data ) {
            if ( ok_or_done( status ) ) {
                int16_t idx = 1;
                for ( auto it = data.begin(); ok_or_done( status ) && it != data.end(); it++ ) {
                    status = sqlite3_bind_text( stmt,
                                                idx++,
                                                it -> c_str(), it -> length(),
                                                SQLITE_STATIC );
                }
            }
            return status;
        }

        int16_t bind_str( const std::string& data ) {
            if ( ok_or_done( status ) && !data.empty() ) {
                status = sqlite3_bind_text( stmt, 1, data.c_str(), data.length(), SQLITE_STATIC );
            }
            return status;
        }
        
        int16_t bind_and_step( const str_vec& data ) {
            bind_str_vec( data );
            step();
            reset();
            return status;
        }

        std::string read_string( uint32_t idx = 0 ) {
            const unsigned char * txt = sqlite3_column_text( stmt, idx );
            if ( nullptr != txt ) {
                return std::string( (const char *)txt );
            }
            return std::string( "" );
        }

        uint16_t column_count( void ) { return sqlite3_column_count( stmt ); }
        uint64_t read_uint64_t( uint32_t idx = 0 ) { return sqlite3_column_int64( stmt, idx ); }
        uint32_t read_uint32_t( uint32_t idx = 0 ) { return sqlite3_column_int( stmt, idx ); }
        int32_t read_int32_t( uint32_t idx = 0 ) { return sqlite3_column_int( stmt, idx ); }
};

typedef std::shared_ptr< prep_stm_t > prep_stm_ptr_t;

class database_t {
    public :
        typedef std::vector< std::string > str_vec;

        sqlite3 *db;
        int16_t rc_open;
        
        database_t( const char * name ) : db( nullptr ) {
            rc_open = sqlite3_open( name, &db );
        }
        
        ~database_t( void ) {
            if ( SQLITE_OK == rc_open && nullptr != db ) {
                sqlite3_close( db );
            }
        }

        prep_stm_ptr_t make_prep_stm( const std::string& stm, bool and_step = false ) {
            return std::make_shared< prep_stm_t >( db, stm, and_step );
        }
            
        int16_t exec( const char * stm ) {
            int16_t res = -1;
            if ( SQLITE_OK == rc_open && nullptr != db ) {
                char * errMsg;
                res = sqlite3_exec( db, stm, nullptr, 0, &errMsg );
                if ( SQLITE_OK != res ) {
                    std::cerr << errMsg << std::endl;
                }
            }
            return res;
        }

        int16_t begin_transaction( void ) { return exec( "BEGIN TRANSACTION" ); }
        int16_t commit_transaction( void ) { return exec( "COMMIT TRANSACTION" ); }
        
        int16_t clear_table( const std::string& tbl_name, bool vacuum = false ) {
            std::string sql = "DELETE FROM " + tbl_name + ";";
            prep_stm_t drop( db, sql );
            int16_t res = drop . step();
            if ( ok_or_done( res ) && vacuum ) {
                res = exec( "VACUUM" );
            }
            return res;
        }
        
        int16_t clear_tables( const str_vec& tables, bool vacuum = true ) {
            int16_t res = SQLITE_OK;
            for ( auto it = tables.begin(); ok_or_done( res ) && it != tables.end(); it++ ) {
                res = clear_table( *it );
            }
            if ( ok_or_done( res ) && vacuum ) {
                res = exec( "VACUUM" );
            }
            return res;
        }
        
        bool ok_or_done( int16_t status ) {
            return ( SQLITE_OK == status || SQLITE_DONE == status );
        }

        void print_status( int16_t status ) { db_func_t::print_status( status ); }

        void split_at( std::vector< std::string >& dst, const std::string& src, char delim ) {
            std::stringstream ss( src );
            std::string token;
            while( std::getline( ss, token, delim ) ) {
                dst . push_back( token );
            }
        }
        
        int16_t exec_pragma( const std::string& value ) {
            std::string stm( "PRAGMA " );
            stm += value;
            stm += ";";
            return exec( stm . c_str() );
        }

        int16_t synchronous( bool on ) {
            if ( on ) {
                return exec_pragma( "synchronous=ON" );
            } else {
                return exec_pragma( "synchronous=OFF" );
            }
        }
        
};

}; /* namespace mt_database */

#endif
