
#ifndef alignment_hdr
#define alignment_hdr

#include <map>
#include <vector>
#include <memory>
#include <string>

#include "reference.hpp"

class t_alignment;
typedef std::shared_ptr< t_alignment > t_alignment_ptr;
typedef std::map< std::string, std::vector< t_alignment_ptr > > t_alignment_bins;
typedef std::vector< t_alignment_ptr > t_alignment_vec;

class t_alignment {
    private :
        const std::string name;
        std::string special_cigar;
        std::string pure_cigar;
        std::string seq;
        std::string qual;
        std::string opts;
        std::string ins_bases;
        std::string ref_name_override;
        t_reference_ptr ref;
        t_alignment_ptr mate;
        int flags;
        int ref_pos;
        int mapq;
        int tlen;
        
        void print_opts( std::ostream &out ) const;
        int  random_refpos( void );

    public :
        // common ctor, just name and flags ( everything else set to defaults )
        t_alignment( const std::string &a_name, int a_flags );
        static t_alignment_ptr make( const std::string &name, int flags );

        void set_reference( const t_reference_ptr a_ref );
        void set_ref_pos( int a_pos ); // needs: ref and cigar
        void set_mapq( int a_mapq );
        void set_ins_bases( const std::string &a_bases );
        void set_ref_name_override( const std::string &a_ref_name );
        void set_opts( const std::string &a_opts );
        void set_cigar( const std::string &a_cigar ); // needs: ref, ref_pos, ins_bases
        void set_seq( const std::string &a_seq );
        void set_tlen( int a_tlen );
        void set_mate( t_alignment_ptr a_mate );
        void set_quality( const std::string &a_qual, int qual_div ); //needs seq ( cigar )
        void adjust_refpos_and_seq( void );

        bool operator<( const t_alignment& other ) const;

        std::string get_refname( void ) const;
        std::string get_print_refname( void ) const;
        void print_SAM( std::ostream &out ) const;
        bool has_flag( int mask ) const;
        bool has_ref( void ) const;
        const std::string& get_name( void ) const;
        void toggle_flag( int flagbit, bool state );
};

//we need that because we have a vector of smart-pointers - not objects, to sort...
class t_alignment_comparer {
    public :
        bool operator()( const t_alignment_ptr& a, const t_alignment_ptr& b ) {
            return *a < *b && !(*b < *a);
        }
};

#endif
