
#ifndef prog_line_params_hdr
#define prog_line_params_hdr

#include <string>
#include <map>
#include <memory>
#include <algorithm>

class t_params;
typedef std::shared_ptr< t_params > t_params_ptr;

class t_params {
    private :
        std::map< std::string, std::string > values;
        void split_kv( const std::string &kv );
        
    public :
        t_params( const std::string &line );
        static t_params_ptr make( const std::string &line );
        const std::string& get_string( const std::string& dflt );
        const std::string& get_string_key( const std::string& key, const std::string& dflt ) const;
};

#endif
