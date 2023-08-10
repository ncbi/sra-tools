
#include <iostream>
#include <fstream>
#include <sstream>

#include "helper.hpp"
#include "progline.hpp"

static e_progline_kind cmd_2_kind( std::string &c ) {
    if ( c == "ref" || c == "r" ) { return e_progline_kind::ref; }
    else if ( c == "ref-out" ) { return e_progline_kind::ref_out; }
    else if ( c == "sam-out" ) { return e_progline_kind::sam_out; }
    else if ( c == "dflt-cigar" ) { return e_progline_kind::dflt_cigar; }
    else if ( c == "dflt_mapq" ) { return e_progline_kind::dflt_mapq; }
    else if ( c == "dflt_qdiv" ) { return e_progline_kind::dflt_qdiv; }
    else if ( c == "config" ) { return e_progline_kind::config; }
    else if ( c == "prim" || c == "p" ) { return e_progline_kind::prim; }
    else if ( c == "sec" || c == "s" ) { return e_progline_kind::sec; }
    else if ( c == "unalig" || c == "u" ) { return e_progline_kind::unaligned; }
    else if ( c == "lnk" || c == "l" ) { return e_progline_kind::link; }
    else if ( c == "sort" ) { return e_progline_kind::sort; }
    return e_progline_kind::unknown;
}
    
t_progline::t_progline( const std::string &line, const char * a_file_name, int a_line_nr )
        : filename( a_file_name ), line_nr( a_line_nr ), org( line ),
        cmd(), kind( e_progline_kind::unknown ), kv( t_params::make( line ) ) {
            int colon_pos = line.find( ':' );
            if ( colon_pos != -1 ) {
                cmd = line.substr( 0, colon_pos );
                std::transform( cmd.begin(), cmd.end(), cmd.begin(), ::tolower );
                kind = cmd_2_kind( cmd );
            }
        }
        
t_progline_ptr t_progline::make( const std::string &line, const char * filename, int line_nr ) {
    return std::make_shared< t_progline>( t_progline( line, filename, line_nr ) );
}
        
bool t_progline::is( e_progline_kind k ) { return kind == k; }
        
std::string t_progline::get_org( void ) const {
    std::stringstream ss;
    ss << filename << "#" << line_nr << " : " << org;
    return ss.str();
}
        
const std::string& t_progline::get_string( const std::string& dflt ) {
    return kv -> get_string( dflt );
}
        
int t_progline::get_int( int dflt ) {
    auto value = kv -> get_string( empty_string );
    if ( value . empty() ) { return dflt; }
    return str_to_int( value, dflt );
}
        
int t_progline::get_bool( bool dflt ) {
    auto value = kv -> get_string( empty_string );
    if ( value . empty() ) { return dflt; }
    return str_to_bool( value );
}
        
const std::string& t_progline::get_string_key( const std::string& key, const std::string& dflt ) const {
    return kv -> get_string_key( key, dflt );
}
        
const std::string& t_progline::get_string_key( const std::string& key ) const {
    return get_string_key( key, empty_string );
}
        
bool t_progline::get_bool_key( const std::string& key, bool dflt ) const {
    auto value = kv -> get_string_key( key, empty_string );
    if ( value . empty() ) { return dflt; }
    return str_to_bool( value );
}
        
int t_progline::get_int_key( const std::string& key, int dflt ) const {
    auto value = kv -> get_string_key( key, empty_string );
    if ( value . empty() ) { return dflt; }
    return str_to_int( value, dflt );
}
        
void t_progline::consume_stream( std::istream &stream, const char * file_name, t_proglines& proglines ) {
    int line_nr = 0;
    for ( std::string line; std::getline( stream, line ); ) {
        if ( !( 0 == line.find( '#' ) ) ) {
            proglines . push_back( t_progline::make( line, file_name, line_nr ) );
        }
        line_nr++;
    }
}
        
void t_progline::consume_lines( int argc, char *argv[], t_proglines& proglines ) {
    if ( argc > 1 ) {
        // looping through the file( s ) given at the commandline...
        for ( int i = 1; i < argc; i++ ) {
            std::fstream inputfile;
            inputfile.open( argv[ i ], std::ios::in );
            if ( inputfile.is_open() ) {
                t_progline::consume_stream( inputfile, argv[ 1 ], proglines );
                inputfile.close();
            }
        }
    } else {
        // reading from stdin
        consume_stream( std::cin, "stdin", proglines );
    }
}
