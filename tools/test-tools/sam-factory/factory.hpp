
#ifndef factory_hdr
#define factory_hdr

#include "reference.hpp"
#include "settings.hpp"
#include "alignment_group.hpp"

class t_factory {
    private :
        const t_proglines& proglines;
        t_reference_map refs;
        t_errors& errors;
        t_settings settings;
        t_alignment_group_map alignment_groups;
        
        bool phase1( void );
    
        int get_flags( const t_progline_ptr pl, int type_flags ) const;
    
        void generate_single_align( const t_progline_ptr pl, const std::string &name,
                                    const t_reference_ptr ref, int flags, int qual_div );
        
        void generate_multiple_align( const t_progline_ptr pl, const std::string &base_name,
                                      const t_reference_ptr ref, int flags, int repeat, int qual_div );

        void generate_align( const t_progline_ptr pl, int type_flags );

        void generate_unaligned( const t_progline_ptr pl );

        bool phase2( void );

        void sort_and_print( std::ostream &out );

        void print_all( std::ostream &out );

        bool phase3( void );
                                                              
    public :
        t_factory( const t_proglines& lines, t_errors& error_list );
    
        int produce( void );
};

#endif
