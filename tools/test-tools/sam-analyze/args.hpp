#ifndef ARGS_H
#define ARGS_H

#include <iostream>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>

/*  scans commandline for options and arguments
 *  prog a b c      ... a, b, c will be arguments
 *  prog -a -b c    ... -a, and -b will be value-less options, c is argument
 *  prog -a=10 -b=hallo c   ... -a, and -b will be values with options, c is argument
 *  prog -a 10 -b 20 c ... -a and -b will be value-less again! 10, 20, and c are arguments
 */

class args_t {
    typedef std::map< std::string, std::string > str_map_t;
    typedef str_map_t::const_iterator str_mapt_iter_t;
    
    public:
        typedef std::vector< std::string > str_vec_t;
        args_t( int argc, char * argv[], const str_vec_t& hints ) {
            scan( argc, argv, hints );
        };

        const std::string get_str( const char * short_key,
                             const char *long_key = nullptr,
                             const char * dflt = nullptr ) const {
            auto it = find( short_key, long_key );
            if ( it == values . end() ) { 
                if ( nullptr != dflt ) {
                    return std::string( dflt );
                } else {
                    return std::string();
                }
            }
            return std::string( it -> second );
        }

        template < typename T >
        T get_int( const char * short_key,
                   const char *long_key = nullptr, 
                   T dflt = 0 ) const {
            auto it = find( short_key, long_key );
            if ( it == values . end() ) { return dflt; };
            return from_string<T>( it -> second, dflt );
        }

        bool has( const char * short_key, const char *long_key ) const {
            return ( find( short_key, long_key ) != values.end() );
        }

        size_t arg_count( void ) const { return arguments . size(); }

        const std::string get_arg( size_t idx ) const {
            if ( idx < arguments.size() ) {
                return arguments[ idx ];
            }
            return std::string();
        }
        
        void report( void ) const {
            for ( size_t i = 0; i < arg_count(); ++i ) {
                std::cout << "arg[" << i << "] = " << get_arg( i ) << std::endl;
            }
            for ( auto it = values . begin(); it != values . end(); ++it ) {
                std::string key = it -> first;
                std::string value = get_str( key.c_str(), nullptr, nullptr );
                std::cout << "[ '" << key << "' ] = '" << value << "'" << std::endl;
            }
        }

    private:
        str_map_t values;
        str_vec_t arguments;

        template < typename T >
        T from_string( const std::string& s, T dflt ) const {
            T res = dflt;
            std::stringstream ss;
            ss << s;
            try {
                ss >> res;
            } catch( const std::exception& e ) {
                res = dflt;
            }
            return res;
        }
        
        str_mapt_iter_t find( const char * short_key, const char *long_key ) const {
            auto it = values . end();
            if ( nullptr != short_key ) { it = values . find( short_key ); }
            if ( nullptr != long_key && it == values . end() ) { it = values . find( long_key ); }
            return it;
        }

        static bool str_vector_contains( const str_vec_t& v, const std::string& key ) {
            return std::find( v . begin(), v . end(), key ) != v . end();
        }

        void store_key_and_value( const std::string& key, const std::string& value ) {
            values.insert( std::pair< std::string, std::string >( key, value ) );
        }

        void store_key_flags( const std::string& key ) {
            std::string value;
            auto l = key . length();
            if ( l > 2 ) {
                // potential multi-flag
                if ( key . substr( 1, 1 ) == "-" ) {
                    // no, it was not ( s            tarts with -- )
                    store_key_and_value( key, value );
                } else {
                    for ( size_t i = 1; i < l; ++i ) {
                        std::string ins_key( "-" );
                        ins_key += key[ i ];
                        store_key_and_value( ins_key, value );
                    }
                }
            } else {
                store_key_and_value( key, value );
            }
        }

        void scan( int argc, char * argv[], const str_vec_t& hints ) {
            if ( argv != nullptr ) {
                std::string key;
                for ( int32_t i = 1; i < argc; ++i ) {
                    if ( argv[ i ] != nullptr ) {
                        std::string arg( argv[ i ] );
                        if ( ! arg . empty() ) {
                            if ( arg . substr( 0, 1 ) == "-" ) {
                                // check to see if it starts with at least one dash...
                                int eq = arg . find( "=" );
                                if ( eq > 0 ) {
                                    // arg is in the form of '--key=value'
                                    key = arg . substr( 0, eq );
                                    std::string value = arg . substr( eq + 1 );
                                    store_key_and_value( key, value );
                                    key . clear();
                                } else {
                                    // s is in the form of --key or -k
                                    // !!! the next arg is the value!
                                    if ( str_vector_contains( hints, arg ) ) {
                                        // the arg is in the hints, the next arg is the value!
                                        // insert nothing yet, but keep the key for the next loop-iteration
                                        key = arg;
                                    } else {
                                        // the arg is NOT in the hints, store key with empty value
                                        store_key_flags( arg );
                                        key . clear();
                                    }
                                }
                            } else {
                                // does not start with -- or -
                                if ( key . empty() ) {
                                    arguments . push_back( arg );
                                } else {
                                    store_key_and_value( key, arg );
                                    key . clear();
                                }
                            }
                        }
                    }
                }
            }
        }
};

#endif
