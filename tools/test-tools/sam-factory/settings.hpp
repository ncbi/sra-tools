
#ifndef settings_hdr
#define settings_hdr

#include <string>

#include "helper.hpp"
#include "progline.hpp"

class t_settings {
    private :
        std::string ref_out;
        std::string sam_out;
        std::string config;
        std::string dflt_alias;
        std::string dflt_cigar;
        int dflt_mapq;
        int dflt_qdiv;
        bool sort_alignments;
        
        void set_string( const t_progline_ptr pl, const char * msg, std::string *out,
                        t_errors & errors );
                        
    public :
        t_settings( const t_proglines& proglines, t_errors & errors );
        
        const std::string& get_ref_out( void ) const;
        const std::string& get_sam_out( void ) const;
        const std::string& get_config( void ) const;
        const std::string& get_dflt_alias( void ) const;
        void set_dflt_alias( std::string alias );
        const std::string& get_dflt_cigar( void ) const;
        int get_dflt_mapq( void );
        int get_dflt_qdiv( void );
        bool get_sort_alignments( void );
};

#endif
