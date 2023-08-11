
#include "alignment.hpp"
#include "cigar.hpp"
#include "helper.hpp"

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

int t_alignment::random_refpos( void ) {
    int res = ref_pos;
    if ( nullptr != ref ) {
        res = random_int( 1, ref -> length() );
        size_t spec_cig_len = cigar_t::reflen_of( special_cigar );
        size_t rlen = ref -> length();
        if ( res + spec_cig_len >= rlen ) {
            res = ref -> length() - ( spec_cig_len + 1 );
        }
    }
    return res;
}

void t_alignment::adjust_refpos_and_seq( void ) {
    if ( nullptr != ref ) {
        size_t spec_cig_len = cigar_t::reflen_of( special_cigar );
        size_t rlen = ref -> length();
        if ( ref_pos + spec_cig_len >= rlen ) {
            ref_pos = ref -> length() - ( spec_cig_len + 1 );
            seq = cigar_t::read_at( special_cigar, ref_pos, ref -> get_bases(), ins_bases );
        }
    }
}

// common ctor, just name and flags ( everything else set to defaults )
t_alignment::t_alignment( const std::string &a_name, int a_flags )
    : name( a_name ),
      special_cigar( empty_string ),
      pure_cigar( star_string ),
      seq( empty_string ),
      qual( empty_string ),
      opts( empty_string ),
      ins_bases( empty_string ),
      ref( nullptr ),
      mate( nullptr ),
      flags( a_flags ),
      ref_pos( 0),
      mapq( 0 ),
      tlen( 0 ) { }
                          
t_alignment_ptr t_alignment::make( const std::string &name, int flags ) {
    return std::make_shared< t_alignment >( t_alignment( name, flags ) );
}

void t_alignment::set_reference( const t_reference_ptr a_ref ) {
    ref = a_ref;
}

void t_alignment::set_ref_pos( int a_ref_pos ) {
    if ( nullptr != ref && 0 == a_ref_pos ) {
        ref_pos = random_refpos();        
    } else {
        ref_pos = a_ref_pos;
    }
}

void t_alignment::set_mapq( int a_mapq ) {
    mapq = a_mapq;
}

void t_alignment::set_ins_bases( const std::string &a_bases ) {
    ins_bases = a_bases;
}

void t_alignment::set_opts( const std::string &a_opts ) {
    opts = a_opts;
}

void t_alignment::set_cigar( const std::string &a_cigar ) {
    special_cigar = a_cigar;
    pure_cigar = cigar_t::purify( a_cigar );
    if ( nullptr != ref ) {
        seq = cigar_t::read_at( special_cigar, ref_pos, ref -> get_bases(), ins_bases );
    }
}

void t_alignment::set_seq( const std::string &a_seq ) {
    seq = a_seq;
}

void t_alignment::set_tlen( int a_tlen ) {
    tlen = a_tlen;
}

void t_alignment::set_mate( t_alignment_ptr a_mate ) {
    mate = a_mate;
    toggle_flag( FLAG_NEXT_UNMAPPED, mate -> has_flag( FLAG_UNMAPPED ) );
    toggle_flag( FLAG_NEXT_REVERSED, mate -> has_flag( FLAG_REVERSED ) );
    toggle_flag( FLAG_MULTI, true );
}

void t_alignment::set_quality( const std::string &a_qual, int qual_div ) {
    qual = a_qual;
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

bool t_alignment::operator<( const t_alignment& other ) const { 
    return ref_pos < other . ref_pos;
}

std::string t_alignment::get_refname( void ) const {
    if ( ref != nullptr ) { return ref -> get_alias(); } else { return star_string; }
}

void t_alignment::print_SAM( std::ostream &out ) const {
    out << name << tab_string << flags << tab_string << get_refname() << tab_string;
    out << ref_pos << tab_string << mapq << tab_string << pure_cigar << tab_string;
    if ( mate != nullptr ) {
        out << mate -> get_refname() << tab_string << mate -> ref_pos << tab_string;
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

void t_alignment::toggle_flag( int flagbit, bool state ) {
    if ( state ) {
        flags |= flagbit;
    } else {
        flags &= ~flagbit;
    }
}
