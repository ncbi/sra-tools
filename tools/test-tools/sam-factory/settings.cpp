
#include "settings.hpp"
    
void t_settings::set_string( const t_progline_ptr pl, const char * msg, std::string *out,
                            t_errors & errors ) {
    const std::string& fn = pl -> get_string( "name" );
    if ( fn.empty() ) {
        errors . msg( msg, pl -> get_org() );
    } else {
        *out = fn;
    }
}

t_settings::t_settings( const t_proglines& proglines, t_errors & errors )
    : dflt_cigar( "30M" ), dflt_mapq( 20 ), dflt_qdiv( 0 ), sort_alignments( true ) {
        for ( const t_progline_ptr &pl : proglines ) {
            if ( pl -> is( e_progline_kind::ref_out ) ) {
                set_string( pl, "missing ref-file-name in: ", &ref_out, errors );
            } else if ( pl -> is( e_progline_kind::sam_out ) ) {
                set_string( pl, "missing sam-file-name in: ", &sam_out, errors );
            } else if ( pl -> is( e_progline_kind::config ) ) {
                set_string( pl, "missing config-file-name in: ", &config, errors );
            } else if ( pl -> is( e_progline_kind::dflt_mapq ) ) {
                dflt_mapq = pl -> get_int( dflt_mapq );
            } else if ( pl -> is( e_progline_kind::dflt_qdiv ) ) {
                dflt_qdiv = pl -> get_int( dflt_qdiv );
            } else if ( pl -> is( e_progline_kind::dflt_cigar ) ) {
                set_string( pl, "missing value in: ", &dflt_cigar, errors );
            } else if ( pl -> is( e_progline_kind::sort ) ) {
                sort_alignments = pl -> get_bool( sort_alignments );
            }
        }
    }
    
const std::string& t_settings::get_ref_out( void ) const { return ref_out; }
const std::string& t_settings::get_sam_out( void ) const { return sam_out; }
const std::string& t_settings::get_config( void ) const { return config; }
const std::string& t_settings::get_dflt_alias( void ) const { return dflt_alias; }
void t_settings::set_dflt_alias( std::string alias ) { dflt_alias = alias; }
const std::string& t_settings::get_dflt_cigar( void ) const { return dflt_cigar; }
int t_settings::get_dflt_mapq( void ) { return dflt_mapq; }
int t_settings::get_dflt_qdiv( void ) { return dflt_qdiv; }
bool t_settings::get_sort_alignments( void ) { return sort_alignments; }
