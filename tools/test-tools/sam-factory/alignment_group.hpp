
#ifndef alignment_group_hdr
#define alignment_group_hdr

#include <map>

#include "alignment.hpp"

/* -----------------------------------------------------------------
 * the alignment-group ( binned by name )
 * ----------------------------------------------------------------- */

class t_alignment_group;
typedef std::shared_ptr< t_alignment_group > t_alignment_group_ptr;
typedef std::map< std::string, t_alignment_group_ptr > t_alignment_group_map;

class t_alignment_group {
    private :
        std::string name;
        t_alignment_vec prim_alignments;
        t_alignment_vec sec_alignments;
        t_alignment_vec unaligned;
    
    public :
        t_alignment_group( std::string a_name );
        
        static t_alignment_group_ptr make( t_alignment_ptr a );
    
        void add( t_alignment_ptr a );

        void print_SAM( std::ostream &out ) const;
    
        static void finish_alignmnet_vector( t_alignment_vec& v );
    
        void finish_alignments( void );

        void handle_tagline( const t_progline_ptr &pl );
        
        void bin_alignment_by_ref( t_alignment_ptr a, t_alignment_bins &by_ref,
                                   t_alignment_vec &without_ref );
    
        void bin_by_refname( t_alignment_bins &by_ref, t_alignment_vec &without_ref );
    
        static void insert_alignment( t_alignment_ptr a, t_alignment_group_map& alignment_groups );
};

#endif
