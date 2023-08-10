
#include "alignment.hpp"
#include "cigar.hpp"
#include "helper.hpp"

void t_alignment::set_quality( int qual_div ) {
    size_t seq_len = seq.length();
    size_t qual_len = qual.length();
    if ( 0 == qual_len ) {
        // if no quality given: generate random quality the same length as seq
        if ( 0 == qual_div ) {
            qual = random_quality( seq_len );
        } else {
            qual = random_div_quality( seq_len, qual_div );
        }
    } else if ( star_string == qual || qual_len == seq_len ) {
        // if qual is "*" or the same length as seq : do nothing - keep it
    } else {
        // generate qual by repeating the pattern given until seq_len is reached
        qual = pattern_quality( qual, seq_len );
    }
}

void t_alignment::print_opts( std::ostream &out ) const {
    std::stringstream ss( opts );
    std::string elem;
    while ( std::getline( ss, elem, ' ' ) ) {
        // trim white-space
        elem . erase( std::remove( elem . begin(), elem.end(), ' ' ), elem.end() );
        if ( !elem.empty() ) {
            out << tab_string << elem;
        }
    }
}
    
// ctor for PRIM/SEC alignments
t_alignment::t_alignment( const std::string &a_name, int a_flags, const t_reference_ptr a_ref, int a_pos,
                 int a_mapq, const std::string &a_cigar, int a_tlen, const std::string &a_qual,
                 const std::string &a_opts, const std::string &a_ins_bases, int qual_div )
    : name( a_name ), flags( a_flags ), ref( a_ref ), ref_pos( a_pos ), mapq( a_mapq ),
      special_cigar( a_cigar ), pure_cigar( cigar_t::purify( a_cigar ) ), tlen( a_tlen ),
      qual( a_qual ), opts( a_opts ), ins_bases( a_ins_bases )  {
    if ( ref_pos == 0 ) {
        ref_pos = random_int( 1, ref -> length() );
        size_t spec_cig_len = cigar_t::reflen_of( special_cigar );
        size_t rlen = ref -> length();
        if ( ref_pos + spec_cig_len >= rlen ) {
            ref_pos = ref -> length() - ( spec_cig_len + 1 );
        }
    }
    seq = cigar_t::read_at( special_cigar, ref_pos, ref -> get_bases(), ins_bases );
    set_quality( qual_div );
}
    
// ctor for UNALIGNED alignments
t_alignment::t_alignment( const std::string &a_name, int a_flags, const std::string &a_seq,
                 const std::string &a_qual, const std::string &a_opts, int qual_div )
    : name( a_name ), flags( a_flags ), ref( nullptr ), ref_pos( 0 ), mapq( 0 ),
      special_cigar( empty_string ), pure_cigar( star_string ), mate( nullptr ),
      tlen( 0 ), seq( a_seq ), qual( a_qual ), opts( a_opts ), ins_bases( empty_string ) {
        set_quality( qual_div );
}

t_alignment_ptr t_alignment::make( const std::string &name, int flags, const t_reference_ptr ref, int pos,
                                   int mapq, const std::string &cigar, int tlen, const std::string &qual,
                                   const std::string &opts, const std::string &ins_bases, int qual_div ) {
    return std::make_shared< t_alignment >( t_alignment( name, flags, ref, pos, mapq, cigar,
                                                         tlen, qual, opts, ins_bases, qual_div ) );
}
                                 
t_alignment_ptr t_alignment::make( const std::string &name, int flags, const std::string &seq,
                            const std::string &qual, const std::string &opts, int qual_div ) {
    return std::make_shared< t_alignment > ( t_alignment( name, flags, seq, qual, opts, qual_div ) );
}

bool t_alignment::operator<( const t_alignment& other ) const { 
    return ref_pos < other . ref_pos;
}

std::string t_alignment::refname( void ) const {
    if ( ref != nullptr ) { return ref -> get_alias(); } else { return star_string; }
}

void t_alignment::print_SAM( std::ostream &out ) const {
    out << name << tab_string << flags << tab_string << refname() << tab_string;
    out << ref_pos << tab_string << mapq << tab_string << pure_cigar << tab_string;
    if ( mate != nullptr ) {
        out << mate -> refname() << tab_string << mate -> ref_pos << tab_string;
    } else {
        out << "*\t0\t";
    }
    out << tlen << tab_string;
    out << ( ( seq.length() > 0 ) ? seq : star_string );
    out << tab_string;
    out << ( ( qual.length() > 0 ) ? qual : star_string );
    if ( opts.length() > 0 ) { print_opts( out ); }
    out << std::endl;
}

bool t_alignment::has_flag( int mask ) const { 
    return ( flags & mask ) == mask;
}

bool t_alignment::has_ref( void ) const { 
    return ref != nullptr;
}

const std::string& t_alignment::get_name( void ) const { 
    return name;
}

void t_alignment::set_mate( t_alignment_ptr a_mate ) {
    mate = a_mate;
    set_flag( FLAG_NEXT_UNMAPPED, mate -> has_flag( FLAG_UNMAPPED ) );
    set_flag( FLAG_NEXT_REVERSED, mate -> has_flag( FLAG_REVERSED ) );
    set_flag( FLAG_MULTI, true );
}

void t_alignment::set_flag( int flagbit, bool state ) {
    if ( state ) {
        flags |= flagbit;
    } else {
        flags &= ~flagbit;
    }
}
