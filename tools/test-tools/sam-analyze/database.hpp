#ifndef MT_DATABASE_H
#define MT_DATABASE_H

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include "sqlite3.h"

namespace mt_database {

class PREP_STM {
    private :
        sqlite3 *db;
        sqlite3_stmt * stmt;
        int16_t status;

    public :
        typedef std::vector< std::string > str_vec;

       
        PREP_STM( sqlite3* a_db, const std::string& sql )
            : db( a_db ), stmt( nullptr ) {
            status = sqlite3_prepare_v2( db, sql.c_str(), sql.length(), &stmt, nullptr );
        }

        ~PREP_STM( void ) {
            sqlite3_finalize( stmt );
        }

        bool ok_or_done( int16_t st ) {
            return ( SQLITE_OK == st || SQLITE_DONE == st );
        }

        int16_t get_status( void ) const { return status; }
        
        void print_status( int16_t status ) {
            const char * e = sqlite3_errstr( status );
            std::cout << "status:" << e << std::endl;
        }
        
        int16_t step( void ) {
            if ( ok_or_done( status ) ) { status = sqlite3_step( stmt ); }
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

        int16_t read_text( std::string& result ) {
            if ( SQLITE_ROW == step() ) {
                const unsigned char * txt = sqlite3_column_text( stmt, 0 );
                std::string s( (const char *)txt );
                result = s;
                step();
            }
            reset();
            return status;
        }

        int16_t read_text_1( const std::string& data, std::string& result ) {
            bind_str( data );
            return read_text( result );
        }
        
        int16_t read_text_n( const str_vec& data, std::string& result ) {
            bind_str_vec( data );
            return read_text( result );            
        }

        int16_t read_long( unsigned long& result ) {
            step();
            if ( SQLITE_ROW == status ) {
                result = sqlite3_column_int64( stmt, 0 );
                while ( SQLITE_ROW == status ) {
                    status = sqlite3_step( stmt );
                }
            }
            reset();
            return status;
        }

        int16_t read_long_1( const std::string& data, unsigned long& result ) {
            bind_str( data );
            return read_long( result );
        }
        
        int16_t read_long_n( const str_vec& data, unsigned long& result ) {
            bind_str_vec( data );
            return read_long( result );            
        }

        int16_t read_row( str_vec& data, int16_t count ) {
            data . clear();
            if ( status == SQLITE_OK || status == SQLITE_ROW ) {
                status = sqlite3_step( stmt );
            }
            if ( SQLITE_ROW == status ) {
                for ( int16_t i = 0; i < count; ++ i ) {
                    const unsigned char * txt = sqlite3_column_text( stmt, i );
                    data . push_back( std::string( ( const char * )txt ) );
                }
            }
            return status;
        }
        
        bool read_2_longs( uint64_t& v1, uint64_t& v2 ) {
            if ( status == SQLITE_OK || status == SQLITE_ROW ) {
                status = sqlite3_step( stmt );
            }
            bool res =  ( SQLITE_ROW == status );
            if ( res ) {
                v1 = sqlite3_column_int64( stmt, 0 );
                res =  ( SQLITE_ROW == status );
                if ( res ) {
                    v2 = sqlite3_column_int64( stmt, 1 );
                    res =  ( SQLITE_ROW == status );
                }
            }
            return res;
        }
};

class DB {
    public :
        typedef std::vector< std::string > str_vec;

        sqlite3 *db;
        int16_t rc_open;
        
        DB( const char * name ) : db( nullptr ) {
            rc_open = sqlite3_open( name, &db );
        }
        
        ~DB( void ) {
            if ( SQLITE_OK == rc_open && nullptr != db ) {
                sqlite3_close( db );
            }
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

        sqlite3* get_db_handle( void ) const { return db; }

        int16_t begin_transaction( void ) { return exec( "BEGIN TRANSACTION" ); }
        int16_t commit_transaction( void ) { return exec( "COMMIT TRANSACTION" ); }
        
        int16_t clear_table( const std::string& tbl_name, bool vacuum = false ) {
            std::string sql = "DELETE FROM " + tbl_name + ";";
            PREP_STM drop( db, sql );
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

        void print_status( int16_t status ) {
            const char * e = sqlite3_errstr( status );
            std::cout << "status:" << e << std::endl;
        }

        void split_at( std::vector< std::string >& dst, const std::string& src, char delim ) {
            std::stringstream ss( src );
            std::string token;
            while( std::getline( ss, token, delim ) ) {
                dst . push_back( token );
            }
        }
};

}; /* namespace mt_database */

#endif
