
#include "cigar.hpp"
#include "helper.hpp"

cigar_t::cigar_t( const std::string &s ) { // ctor
    uint32_t count = 0;
    for( const char& c : s ) {
        if ( isdigit( c ) ) {
            count = ( count * 10 ) + ( c - '0' );
        } else {
            if ( count == 0 ) count = 1;
            append( c, count );
            count = 0;
        }
    }
}
    
cigar_t::cigar_t( const cigar_t &other ) { //copy-ctor
    for( const cigar_opt_t &opt : other . operations ) {
        operations . push_back( opt );
    }
}
    
std::string cigar_t::to_string( void ) const {
    std::string res;
    for( const cigar_opt_t &opt : operations ) {
        std::stringstream ss;
        ss << opt . count << opt . op;
        res . append( ss . str() );
    }
    return res;
}
    
std::string cigar_t::to_read( const std::string &ref_bases, const std::string &ins_bases ) const {
    std::string res;
    if ( readlen() > 0 ) {
        if ( ref_bases . length() >= reflen() && ins_bases . length() >= inslen() ) {
            uint32_t ref_idx = 0;
            uint32_t ins_idx = 0;
            for( const cigar_opt_t &opt : operations ) {
                switch ( opt . op ) {
                    case 'A' :
                    case 'C' :
                    case 'G' :
                    case 'T' : for ( uint32_t i = 0; i < opt . count; ++i ) { res += opt . op; }
                    // break omited for reason!
                    case 'D' : ref_idx += opt . count;
                    break;
                    case 'I' : res += ins_bases . substr ( ins_idx, opt . count );
                    ins_idx += opt . count;
                    break;
                    case 'M' : res += ref_bases . substr ( ref_idx, opt . count );
                    ref_idx += opt . count;
                    break;
                }
            }
        }
    }
    return res;
}
    
// produces the READ against the given reference-bases, eventually using the insert-bases
std::string cigar_t::read_at( const std::string &cigar_str,
                                    int refpos,
                                    const std::string &ref_bases,
                                    const std::string &ins_bases ) {
    cigar_t cigar( cigar_str );
    uint32_t reflen = cigar . reflen();
    const std::string &ref_slice = ref_bases . substr( refpos - 1, reflen );
    return cigar . to_read( ref_slice, ins_bases );
}
                                
// merges M with ACGTM ...
std::string cigar_t::purify( const std::string& cigar_str ) {
    cigar_t cigar( cigar_str );
    cigar_t merged = cigar . merge();
    return merged . to_string();
}
                                
uint32_t cigar_t::reflen( void ) const {
    uint32_t res = 0;
    for( const cigar_opt_t &opt : operations ) {
        switch( opt . op ) {
            case 'A'    : res += opt . count; break;
            case 'C'    : res += opt . count; break;
            case 'G'    : res += opt . count; break;
            case 'T'    : res += opt . count; break;
            case 'D'    : res += opt . count; break;
            case 'M'    : res += opt . count; break;
            // 'I' intenionally omited ...
        }
    }
    return res;
}
                                
uint32_t cigar_t::reflen_of( const std::string &cigar_str ) {
    cigar_t cigar( cigar_str );
    return cigar . reflen();
}
                                
void cigar_t::append( char c, uint32_t count ) {
    cigar_opt_t op{ c, count };
    operations . push_back( op );
}
    
uint32_t cigar_t::readlen( void ) const {
    uint32_t res = 0;
    for( const cigar_opt_t &opt : operations ) {
        if ( opt . op != 'D' ) { res += opt . count; }
    }
    return res;
}

uint32_t cigar_t::inslen( void ) const {
        uint32_t res = 0;
        for( const cigar_opt_t &opt : operations ) {
            if ( opt . op == 'I' ) { res += opt . count; }
        }
        return res;
    }
    
bool cigar_t::can_merge( char op1, char op2 ) {
    char mop1 = op1;
    char mop2 = op2;
    if ( mop1 == 'A' || mop1 == 'C' || mop1 == 'G' || mop1 == 'T' ) { mop1 = 'M'; }
    if ( mop2 == 'A' || mop2 == 'C' || mop2 == 'G' || mop2 == 'T' ) { mop2 = 'M'; }
    return ( mop1 == mop2 );
}
    
cigar_t cigar_t::merge_step( uint32_t * merge_count ) const {
    cigar_t res( empty_string );
    uint32_t idx = 0;
    for( const cigar_opt_t &opt : operations ) {
        if ( idx == 0  ) {
            res . append( opt . op, opt . count );
            idx++;
        } else {
            cigar_opt_t last_op = res . operations[ idx - 1 ];
            if ( can_merge( opt . op, last_op . op ) ) {
                res . operations[ idx - 1 ] . count += opt . count;
                res . operations[ idx - 1 ] . op = 'M';
                ( *merge_count )++;
            } else {
                res . append( opt . op, opt . count );
                idx++;
            }
        }
    }
    return res;
}
    
cigar_t cigar_t::merge( void ) const {
    cigar_t res( *this );
    bool done = false;
    while ( !done ) {
        uint32_t merge_count = 0;
        res = res . merge_step( &merge_count );
        done = ( 0 == merge_count ); 
    }
    return res;
}

int string_to_flag( const std::string& s ) {
    if ( "MULTI" == s ) {
        return FLAG_MULTI;
    } else if ( "PROPPER" == s ) {
        return FLAG_PROPPER;
    } else if ( "UNMAPPED" == s ) {
        return FLAG_UNMAPPED;
    } else if ( "NEXT_UNMAPPED" == s  ) {
        return FLAG_NEXT_UNMAPPED;
    } else if ( "REVERSED" == s ) {
        return FLAG_REVERSED;
    } else if ( "NEXT_REVERSED" == s ) {
        return FLAG_NEXT_REVERSED;
    } else if ( "FIRST" == s ) {
        return FLAG_FIRST;
    } else if ( "LAST" == s ) {
        return FLAG_LAST;
    } else if ( "SECONDARY" == s ) {
        return FLAG_SECONDARY;
    } else if ( "BAD" == s ) {
        return FLAG_BAD;
    } else if ( "PCR" == s ) {
        return FLAG_PCR;
    }
    return 0;
}
