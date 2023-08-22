#ifndef progline_hdr
#define progline_hdr

#include <string>
#include <vector>
#include <memory>

#include "progline_params.hpp"

class t_progline;
typedef std::shared_ptr< t_progline > t_progline_ptr;
typedef std::vector< t_progline_ptr > t_proglines;

enum e_progline_kind { ref, ref_out, sam_out, dflt_cigar, dflt_mapq, dflt_qdiv, config, prim, sec,
    unaligned, tag, link, sort, unknown };
    
class t_progline {
    private :
        const char * filename;
        int line_nr;
        std::string org;
        std::string cmd;
        e_progline_kind kind;
        t_params_ptr kv;
        
    public :
        t_progline( const std::string &line, const char * a_file_name, int a_line_nr );
       
        static t_progline_ptr make( const std::string &line, const char * filename, int line_nr );
        
        bool is( e_progline_kind k );
        std::string get_org( void ) const;
        const std::string& get_string( const std::string& dflt );
        int get_int( int dflt );
        int get_bool( bool dflt );
        const std::string& get_string_key( const std::string& key, const std::string& dflt ) const;
        const std::string& get_string_key( const std::string& key ) const;
        bool get_bool_key( const std::string& key, bool dflt ) const;
        int get_int_key( const std::string& key, int dflt ) const;
        
        static void consume_stream( std::istream &stream, const char * file_name, t_proglines& proglines );
        static void consume_lines( int argc, char *argv[], t_proglines& proglines );
    };

#endif
