
#include "alignment_group.hpp"

t_alignment_group::t_alignment_group( std::string a_name ) : name( a_name ) {
}
    
t_alignment_group_ptr t_alignment_group::make( t_alignment_ptr a ) {
    t_alignment_group_ptr ag = std::make_shared< t_alignment_group >( t_alignment_group( a -> get_name() ) );
    ag -> add( a );
    return ag;
}
    
void t_alignment_group::add( t_alignment_ptr a ) {
    if ( a -> has_flag( FLAG_UNMAPPED ) ) {
        unaligned . push_back( a );
    } else if ( a -> has_flag( FLAG_SECONDARY ) ) {
        sec_alignments . push_back( a );
    } else {
        prim_alignments . push_back( a );
    }
}
    
void t_alignment_group::print_SAM( std::ostream &out ) const {
    for ( const t_alignment_ptr &a : prim_alignments ) { a -> print_SAM( out ); }
    for ( const t_alignment_ptr &a : sec_alignments ) { a -> print_SAM( out ); }
    for ( const t_alignment_ptr &a : unaligned ) { a -> print_SAM( out ); }
}
    
void t_alignment_group::finish_alignmnet_vector( t_alignment_vec& v ) {
    int cnt = v.size();
    if ( cnt > 1 ) {
        int idx = 0;
        for( t_alignment_ptr a : v ) {
            if ( idx == 0 ) {
                // first ...
                a -> set_mate( v[ 1 ] );
                a -> set_flag( FLAG_FIRST, true );
                a -> set_flag( FLAG_LAST, false );
            } else if ( idx == cnt -1 ) {
                // last ...
                a -> set_mate( v[ 0 ] );
                a -> set_flag( FLAG_FIRST, false );
                a -> set_flag( FLAG_LAST, true );
            } else {
                // in between
                a -> set_mate( v[ idx + 1 ] );
                a -> set_flag( FLAG_FIRST, false );
                a -> set_flag( FLAG_LAST, false );
            }
            idx++;
        }
    } else if ( cnt == 1 ) {
        v[ 0 ] -> set_flag( FLAG_MULTI, false );
    }
}
    
void t_alignment_group::finish_alignments( void ) {
    t_alignment_group::finish_alignmnet_vector( prim_alignments );
    t_alignment_group::finish_alignmnet_vector( sec_alignments );
}
    
void t_alignment_group::bin_alignment_by_ref( t_alignment_ptr a, t_alignment_bins &by_ref,
                                              t_alignment_vec &without_ref ) {
    if ( a -> has_ref() ) {
        std::string name = a -> refname();
        auto bin = by_ref . find( name );
        if ( bin == by_ref . end() ) {
            // no : we have to make one ...
            t_alignment_vec v{ a };
            by_ref . insert( std::pair< std::string, std::vector< t_alignment_ptr > >( name, v ) );
        } else {
            // yes : just add it
            bin -> second . push_back( a );
        }
        
    } else {
        // has no reference, comes at the end...
        without_ref . push_back( a );
    }
}
    
void t_alignment_group::bin_by_refname( t_alignment_bins &by_ref, t_alignment_vec &without_ref ) {
    for ( const auto &a : prim_alignments ) { bin_alignment_by_ref( a, by_ref, without_ref ); }
    for ( const auto &a : sec_alignments ) { bin_alignment_by_ref( a, by_ref, without_ref ); }
    for ( const auto &a : unaligned ) { without_ref . push_back( a ); }
}

void t_alignment_group::insert_alignment( t_alignment_ptr a, t_alignment_group_map& alignment_groups ) {
    const std::string& name = a -> get_name();
    auto found = alignment_groups . find( name );
    if ( found == alignment_groups . end() ) {
        alignment_groups . insert(
            std::pair< std::string, t_alignment_group_ptr >( name, t_alignment_group::make( a ) ) );
    } else {
        ( found -> second ) -> add( a );
    }
}
