
#ifndef cigar_hdr
#define cigar_hdr

#include <string>
#include <sstream>
#include <vector>

class cigar_t {
    public :
        cigar_t( const std::string &s ); // ctor
        cigar_t( const cigar_t &other ); //copy-ctor
        
        std::string to_string( void ) const;
        std::string to_read( const std::string &ref_bases, const std::string &ins_bases ) const;
        
        // produces the READ against the given reference-bases, eventually using the insert-bases
        static std::string read_at( const std::string &cigar_str,
                                    int refpos,
                                    const std::string &ref_bases,
                                    const std::string &ins_bases );
                                    
        // merges M with ACGTM ...
        static std::string purify( const std::string& cigar_str );
                                    
        uint32_t reflen( void ) const;
                                    
        static uint32_t reflen_of( const std::string &cigar_str );
                                
    private :
        struct cigar_opt_t {
            char op;
            uint32_t count;
        };
    
        std::vector< cigar_opt_t > operations;
        
        void append( char c, uint32_t count );
        uint32_t readlen( void ) const;
        uint32_t inslen( void ) const;
        static bool can_merge( char op1, char op2 );
    
        cigar_t merge_step( uint32_t * merge_count ) const;
        cigar_t merge( void ) const;
};

#endif
