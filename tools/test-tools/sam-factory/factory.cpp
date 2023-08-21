
#include <fstream>
#include "factory.hpp"

bool t_factory::phase1( void ) {
    // populate references...
    for ( const t_progline_ptr &pl : proglines ) {
        if ( pl -> is( e_progline_kind::ref ) ) {
            const std::string& t = pl -> get_string_key( "type" );
            if ( t == "random" ) {
                t_reference::random_ref( pl, refs, errors );
            } else if ( t == "fasta" ) {
                t_reference::fasta_ref( pl, refs, errors );
            } else if ( t == "refseq" ) {
                t_reference::refseq_ref( pl, refs, errors );
            } else {
                errors . msg( "unknown type in: ", pl -> get_org() );
            }
        }
    }
        
    // generate error if we do not have any references at this point
    if ( refs . empty() ) { errors . msg( "no references found!", empty_string ); }
    
    // write a reference-fasta-file ( if requested )
    if ( errors . empty() ) {
        settings . set_dflt_alias( t_reference::get_first_ref_alias( refs ) );
        t_reference::write_reference_file( settings.get_ref_out(), refs, errors );
        t_reference::write_config_file( settings.get_config(), refs, errors );
    }
        
    // write reference-file ( in case of random-type )
    return errors . empty();
}
    
int t_factory::get_flags( const t_progline_ptr pl, int type_flags ) const {
    int flags = pl -> get_int_key( "flags", 0 ) | type_flags;
    if ( pl -> get_bool_key( "reverse", false ) ) { flags |= FLAG_REVERSED; }
    if ( pl -> get_bool_key( "bad", false ) ) { flags |= FLAG_BAD; }
    if ( pl -> get_bool_key( "pcr", false ) ) { flags |= FLAG_PCR; }
    return flags;
}
    
void t_factory::generate_single_align( const t_progline_ptr pl, const std::string &name,
                            const t_reference_ptr ref, int flags, int qual_div ) {
    t_alignment_ptr a = t_alignment::make( name, flags );
    // order is important!
    a -> set_reference( ref );
    int ref_pos = pl -> get_int_key( "pos", 0 );
    a -> set_ref_pos( ref_pos );
    a -> set_mapq( pl -> get_int_key( "mapq", settings.get_dflt_mapq() ) );
    a -> set_ins_bases( pl -> get_string_key( "ins" ) );
    a -> set_ref_name_override( pl -> get_string_key( "rname" ) );
    a -> set_cigar( pl -> get_string_key( "cigar", settings.get_dflt_cigar() ) );
    a -> set_opts( pl -> get_string_key( "opts" ) );
    a -> set_tlen( pl -> get_int_key( "tlen", 0 ) );
    a -> set_quality( pl -> get_string_key( "qual" ), qual_div );
    if ( 0 == ref_pos ) {
        a -> adjust_refpos_and_seq();
    }
    t_alignment_group::insert_alignment( a, alignment_groups );
}

void t_factory::generate_multiple_align( const t_progline_ptr pl, const std::string &base_name,
                                const t_reference_ptr ref, int flags, int repeat, int qual_div ) {
    int ref_pos = pl -> get_int_key( "pos", 0 );
    int mapq = pl -> get_int_key( "mapq", settings.get_dflt_mapq() );
    const std::string& ins_bases = pl -> get_string_key( "ins" );
    const std::string& rname = pl -> get_string_key( "rname" );
    const std::string& cigar = pl -> get_string_key( "cigar", settings.get_dflt_cigar() );
    const std::string& opts = pl -> get_string_key( "opts" );
    int tlen = pl -> get_int_key( "tlen", 0 );
    const std::string& qual = pl -> get_string_key( "qual" );
    for ( int i = 0; i < repeat; i++ ) {
        std::ostringstream os;
        os << base_name << "_" << i;
        t_alignment_ptr a = t_alignment::make( os.str(), flags );
        a -> set_reference( ref );
        a -> set_ref_pos( ref_pos );
        a -> set_mapq( mapq );
        a -> set_ins_bases( ins_bases );
        a -> set_ref_name_override( rname );
        a -> set_cigar( cigar );
        a -> set_opts( opts );
        a -> set_tlen( tlen );
        a -> set_quality( qual, qual_div );
        if ( 0 == ref_pos ) {
            a -> adjust_refpos_and_seq();
        }
        t_alignment_group::insert_alignment( a, alignment_groups );
    }
}

void t_factory::generate_align( const t_progline_ptr pl, int type_flags ) {
    const std::string& name = pl -> get_string_key( "name" );
    if ( name.empty() ) {
        errors . msg( "missing name in: ", pl -> get_org() );
    } else {
        const std::string& ref_alias = pl -> get_string_key( "ref", settings.get_dflt_alias() );
        const t_reference_ptr ref = t_reference::lookup( ref_alias, refs );
        if ( ref == nullptr ) {
            errors . msg( "cannot find reference: ", pl -> get_org() );
        } else {
            int repeat = pl -> get_int_key( "repeat", 0 );
            int qual_div = pl -> get_int_key( "qdiv", settings.get_dflt_qdiv() );
            if ( repeat == 0 ) {
                generate_single_align( pl, name, ref, get_flags( pl, type_flags ), qual_div );
            } else {
                generate_multiple_align( pl, name, ref, get_flags( pl, type_flags ), repeat, qual_div );
            }
        }
    }
}

void t_factory::generate_unaligned( const t_progline_ptr pl ) {
    const std::string& name = pl -> get_string_key( "name" );
    if ( name . empty() ) {
        errors . msg( "missing name in: ", pl -> get_org() );
    } else {
        int flags = pl -> get_int_key( "flags", 0 ) | FLAG_UNMAPPED;
        int qual_div = pl -> get_int_key( "qdiv", settings.get_dflt_qdiv() );
        std::string qual = pl -> get_string_key( "qual" );
        std::string opts = pl -> get_string_key( "opts" );        
        std::string seq = pl -> get_string_key( "seq" ); // not const ref, because we may overwrite it
        if ( seq.empty() ) {
            int len = pl -> get_int_key( "len", 0 );
            if ( len == 0 ) {
                errors . msg( "missing seq or len in: ", pl -> get_org() );
            } else {
                seq = random_bases( len );
            }
        }
        t_alignment_ptr a = t_alignment::make( name, flags );
        a -> set_seq( seq );
        a -> set_opts( opts );
        a -> set_quality( qual, qual_div );        
        t_alignment_group::insert_alignment( a, alignment_groups );
    }
}

bool t_factory::phase2( void ) {
    // handle the different line-types prim|sec||unaligned
    for ( const t_progline_ptr &pl : proglines ) {
        if ( pl -> is( e_progline_kind::prim ) ) {
            generate_align( pl, FLAG_PROPPER );
        } else if ( pl -> is( e_progline_kind::sec ) ) {
            generate_align( pl, FLAG_SECONDARY );
        } else if ( pl -> is( e_progline_kind::unaligned ) ) {
            generate_unaligned( pl );
        }
    }
    return errors . empty();
}

void t_factory::sort_and_print( std::ostream &out ) {
    t_alignment_bins by_ref;
    t_alignment_vec without_ref;
    
    // bin the alignments by reference
    for( const auto &ag : alignment_groups ) {
        ag . second -> bin_by_refname( by_ref, without_ref );
    }
    
    // sort each reference...
    for ( auto &rb : by_ref ) {
        std::sort( rb . second . begin(), rb . second . end(), t_alignment_comparer() );
    }
    
    // produce the SAM-output...
    for ( const auto &rb : by_ref ) {
        for ( const auto &a : rb . second ) { a -> print_SAM( out ); }
    }
    for ( const auto &a : without_ref ) { a -> print_SAM( out ); }
}

void t_factory::print_all( std::ostream &out ) {
    out << "@HD" << tab_string <<	"VN:1.0" << tab_string <<	"SO:coordinate" << std::endl;
    for ( const auto &ref : refs ) {
        ref . second -> print_HDR( out );
    }
    out << "@RG\tID:default" << std::endl;
    
    if ( settings.get_sort_alignments() ) {
        sort_and_print( out );
    } else {
        for( const auto &ag : alignment_groups ) {
            ag . second -> print_SAM( out );
        }
    }
}

bool t_factory::phase3( void ) {
    // set flags / next_ref / next_pos
    for( auto ag : alignment_groups ) { ag . second -> finish_alignments(); }
    
    // finally produce SAM-output
    if ( errors.empty() ) {
        // if sam-out-filename is given, create that file and write SAM into it,
        // otherwise write SAM to stdout
        const std::string& sam_out = settings.get_sam_out();
        if ( sam_out . empty() ) {
            print_all( std::cout );
        } else {
            std::ofstream out( sam_out );
            print_all( out );
        }
    }
    return errors . empty();
}
                                                              
t_factory::t_factory( const t_proglines& lines, t_errors& error_list )
    : proglines( lines ), errors( error_list ), settings( lines, error_list ) {

}
    
int t_factory::produce( void ) {
    if ( phase1() ) {
        if ( phase2() ) {
            if ( phase3() ) {
                return 0;
            }
        }
    }
    return 1;
}
