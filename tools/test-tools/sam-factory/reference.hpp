
#ifndef reference_hdr
#define reference_hdr

#include <memory>
#include <map>
#include <string>

#include "helper.hpp"
#include "progline.hpp"

class t_reference;
typedef std::shared_ptr< t_reference > t_reference_ptr;
typedef std::map< std::string, t_reference_ptr > t_reference_map;

class t_reference {
    private :
        std::string name;
        std::string alias;
        std::string bases;
    
    public :
        t_reference ( const std::string &a_name, const std::string &a_alias );
        
        static t_reference_ptr make( const std::string &a_name, const std::string &a_alias );
        
        static t_reference_ptr make( const std::string &a_name );
    
        void make_random( int length );
        void from_string( const std::string &a_bases );
        void write_fasta( std::ofstream &out ) const;
        void write_config( std::ofstream &out ) const;
        int length( void ) const;
        const std::string& get_bases( void ) const;
        const std::string& get_alias( void ) const;
        const std::string& get_name( void ) const;
    
        void print_HDR( std::ostream &out ) const;
   
        void static insert_ref( const t_reference_ptr ref, t_reference_map& references, t_errors& errors );
        static void random_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors );
        static void insert_name_and_bases( const std::string &name, const std::string &bases,
                                       t_reference_map& refs, t_errors& errors );
        
        static void fasta_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors );
        static void refseq_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors );
        
        static std::string get_first_ref_alias( const t_reference_map& refs );
        
        static void write_reference_file( const std::string& filename, const t_reference_map& refs,
                                          t_errors& errors );

                                                                         
        static void write_config_file( const std::string& filename, const t_reference_map& refs,
                                    t_errors& errors );
                                    
        static const t_reference_ptr lookup( const std::string &alias, const t_reference_map& refs );
                                                                                                        
};

#endif
