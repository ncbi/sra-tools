
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
        std::string name;
        int flags;
        const t_reference_ptr ref;
        int ref_pos;
        int mapq;
        std::string special_cigar;
        std::string pure_cigar;
        t_alignment_ptr mate;
        int tlen;
        std::string seq;
        std::string qual;
        const std::string opts;
        std::string ins_bases;
        
        void set_quality( int qual_div );
        void print_opts( std::ostream &out ) const;
    
    public :
        // ctor for PRIM/SEC alignments
        t_alignment( const std::string &a_name, int a_flags, const t_reference_ptr a_ref, int a_pos,
                    int a_mapq, const std::string &a_cigar, int a_tlen, const std::string &a_qual,
                    const std::string &a_opts, const std::string &a_ins_bases, int qual_div );
    
        // ctor for UNALIGNED alignments
        t_alignment( const std::string &a_name, int a_flags, const std::string &a_seq,
                    const std::string &a_qual, const std::string &a_opts, int qual_div );
    
        static t_alignment_ptr make( const std::string &name, int flags, const t_reference_ptr ref, int pos,
                                    int mapq, const std::string &cigar, int tlen, const std::string &qual,
                                    const std::string &opts, const std::string &ins_bases, int qual_div );

        static t_alignment_ptr make( const std::string &name, int flags, const std::string &seq,
                                    const std::string &qual, const std::string &opts, int qual_div );

        bool operator<( const t_alignment& other ) const;

        std::string refname( void ) const;
        void print_SAM( std::ostream &out ) const;
        bool has_flag( int mask ) const;
        bool has_ref( void ) const;
        const std::string& get_name( void ) const;
        void set_mate( t_alignment_ptr a_mate );
        void set_flag( int flagbit, bool state );
};

//we need that because we have a vector of smart-pointers - not objects, to sort...
class t_alignment_comparer {
    public :
        bool operator()( const t_alignment_ptr& a, const t_alignment_ptr& b ) {
            return *a < *b && !(*b < *a);
        }
};

#endif
