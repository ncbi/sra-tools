
#include <iostream>
#include <fstream>

#include "reference.hpp"

t_reference::t_reference ( const std::string &a_name, const std::string &a_alias )
    : name( a_name ), alias( a_alias ) { }
    
t_reference_ptr t_reference::make( const std::string &a_name, const std::string &a_alias ) {
    return std::make_shared< t_reference >( t_reference( a_name, a_alias ) );
}
    
t_reference_ptr t_reference::make( const std::string &a_name ) {
    return std::make_shared< t_reference >( t_reference( a_name, a_name ) );
}
    
void t_reference::make_random( int length ) { 
    bases = random_bases( length );
}

void t_reference::from_string( const std::string &a_bases ) { 
    bases = a_bases;
}

void t_reference::write_fasta( std::ofstream &out ) const { 
    out << ">" << name << std::endl << bases << std::endl;
}

void t_reference::write_config( std::ofstream &out ) const { 
    out << alias << tab_string << name << std::endl;
}

int t_reference::length( void ) const { 
    return bases.length();
}

const std::string& t_reference::get_bases( void ) const { 
    return bases;
}

const std::string& t_reference::get_alias( void ) const { 
    return alias;
}

const std::string& t_reference::get_name( void ) const { 
    return name;
}
    
void t_reference::print_HDR( std::ostream &out ) const {
    int l = length();
    out << "@SQ\tSN:" << name << "\tAS:n/a" << "\tLN:" << l << tab_string;
    out << "UR:file:rand_ref.fasta";
    out << std::endl;
}
    
void t_reference::insert_ref( const t_reference_ptr ref, t_reference_map& references,
                              t_errors& errors ) {
    const std::string& alias = ref -> get_alias();
    auto found = references . find( alias );
    if ( found == references . end() ) {
        references . insert( std::pair< std::string, t_reference_ptr >( alias, ref ) );
    } else {
        errors . msg( "duplicate reference: ", alias );
    }
}
    
void t_reference::random_ref( const t_progline_ptr pl, t_reference_map& refs,
                              t_errors& errors ) {
    const std::string& name = pl -> get_string_key( "name" );
    if ( name . empty() ) {
        errors . msg( "name missing in: ", pl -> get_org() );
    } else {
        int length = pl -> get_int_key( "length", 10000 );
        auto ref = t_reference::make( name );
        ref -> make_random( length );
        t_reference::insert_ref( ref, refs, errors );
    }
}
    
void t_reference::insert_name_and_bases( const std::string &name, const std::string &bases,
                                    t_reference_map& refs, t_errors& errors ) {
    if ( !( name . empty() ) && !( bases . empty() ) )
    {
        auto ref = t_reference::make( name );
        ref ->  from_string( bases );
        t_reference::insert_ref( ref, refs, errors );
    }
}
                                       
void t_reference::fasta_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors ) {
    const std::string& file_name = pl -> get_string_key( "file" );
    if ( file_name . empty() ) {
        errors . msg( "file_name missing in: ", pl -> get_org() );
    } else {
        std::fstream input;
        input.open( file_name, std::ios::in );
        if ( input.is_open() ) {
            std::string ref_name;
            std::string bases;
            for ( std::string line; std::getline( input, line ); ) {
                if ( line[ 0 ] == '>' ) {
                    t_reference::insert_name_and_bases( ref_name, bases, refs, errors );
                    ref_name = line.substr( 1, line.length() - 1 );
                    bases = empty_string;
                } else {
                    bases += line;
                }
            }
            insert_name_and_bases( ref_name, bases, refs, errors );
            input.close();
        } else {
            errors . msg( "cannot open: ", pl -> get_org() );
        }
    }
}

void t_reference::refseq_ref( const t_progline_ptr pl, t_reference_map& refs, t_errors& errors ) {
    errors . msg( "not implemented yet: ", pl -> get_org() );
}

std::string t_reference::get_first_ref_alias( const t_reference_map& refs ) {
    auto ref0 = refs.begin();
    if ( ref0 == refs.end() ) { return empty_string; }
    return ref0 -> first;
}

void t_reference::write_reference_file( const std::string& filename, const t_reference_map& refs,
                                    t_errors& errors ) {
    if ( ! filename . empty() ) {
        if ( refs . empty() ) {
            errors . msg( "no references - cannot write reference-file: ", filename );
        } else {
            std::ofstream out( filename );
            auto iter = refs . begin();
            while ( iter != refs . end() ) {
                iter -> second -> write_fasta( out );
                iter++;
            }
            out.close();
        }
    }
}

void t_reference::write_config_file( const std::string& filename, const t_reference_map& refs,
                            t_errors& errors ) {
    if ( ! filename . empty() ) {
        if ( refs.empty() ) {
            errors . msg( "no references - cannot write config-file: ", filename );
        } else {
            std::ofstream out( filename );
            auto iter = refs . begin();
            while ( iter != refs . end() ) {
                iter -> second ->  write_config( out );
                iter++;
            }
            out.close();
        }
    }
}

const t_reference_ptr t_reference::lookup( const std::string &alias,
                                           const t_reference_map& refs ) {
    auto found = refs . find( alias );
    if ( found == refs . end() ) { return nullptr; }
    return found -> second;
}
