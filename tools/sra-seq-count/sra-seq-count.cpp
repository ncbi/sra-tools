/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <ngs/ncbi/NGS.hpp>
#include <ngs/ErrorMsg.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/AlignmentIterator.hpp>
#include <ngs/Alignment.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <vector>
#include <list>
#include <algorithm>

#include "options.h"
#include "range.hpp"

using namespace seq_ranges;


struct feature_range
{
	const std::string ref_name;
	const std::string feature_id;
	const char strand;
	range ft_range;

	feature_range( const std::string &ref_name_, const std::string &feature_id_,
				   const long start_, const long end_, const char strand_ )
		: ref_name( ref_name_ ), feature_id( feature_id_ ), strand( strand_ ), ft_range( start_, end_ )
	{ }
};


class feature
{
	private :
		const std::string ref_name;
		const std::string feature_id;
		const char strand;		
		ranges feature_ranges;
		range outer;
		long counter;

	public :
		feature( const feature_range &fr ) : ref_name( fr.ref_name ), feature_id( fr.feature_id ),
				strand( fr.strand ), outer( fr.ft_range ), counter( 0 )
		{
			feature_ranges.add( fr.ft_range );
		}
	
		bool add( const feature_range &fr )
		{
			bool res = ( feature_id == fr.feature_id );
			if ( res )
			{
				feature_ranges.merge( fr.ft_range );
				outer.include( fr.ft_range );
			}
			return res;
		}

		void debug_report ( void )
		{
			std::cout << "FEATURE: " << feature_id << " ( refname: " << ref_name << " ) strand = '" << strand << "' " << outer << std::endl;
			std::cout << feature_ranges << std::endl;
			std::cout << "counter : " << counter << std::endl;
			std::cout << std::endl;
		}

		void report ( int output_mode )
		{
			if ( counter > 0 )
			{
				if ( output_mode == SSC_MODE_NORMAL )
				{
					std::cout << feature_id << "\t" << counter << std::endl;
				}
				else
				{
					std::cout << ref_name << "." << outer << "(" << feature_ranges.get_count() << ") "
						<< feature_id << "\t" << counter << std::endl;
				}
			}
		}

		void sort_ranges( void ) { feature_ranges.sort(); }		
		void get_ref_name( std::string &s ) { s = ref_name; }
		void get_outer_range( range &r ) { r = outer; }

		/* does this feature end before the given range */
		bool ends_before( const range &r ) const { return outer.ends_before( r ); }
		bool is_ref( const std::string &r_name ) { return ( ref_name == r_name ); }
		long start( void ) { return outer.get_start(); }
		
		void check( const range &r ) { if ( outer.intersect( r ) ) counter++; }
};


void trim( std::string &s )
{
	bool to_trim = false;
	std::size_t pos = 0;
	std::size_t len = s.length();
	while ( s[ pos ] == ' ' && len > 0 )
	{
		pos++;
		len--;
		to_trim = true;
	}
	while ( s[ pos + len - 1 ] == ' ' )
	{
		len--;
		to_trim = true;
	}
	if ( to_trim )
		s = s.substr( pos, len );
}


void split_attr( const std::string &attr, const std::string &idattr, std::string &feature_id )
{
	std::size_t from = 0;
	std::size_t pos = 0;
	bool done = false;
	feature_id.clear();
	while ( !done )
	{
		pos = attr.find( ";", from );
		done = ( pos == std::string::npos );
		std::string sub_attr = attr.substr( from, pos - from );
		std::size_t sub_pos = sub_attr.find( "\"" );
		if ( sub_pos != std::string::npos )
		{
			std::string sub_name = sub_attr.substr( 0, sub_pos - 1 );
			trim( sub_name );
			if ( sub_name == idattr )
			{
				feature_id = sub_attr.substr( sub_pos + 1, sub_attr.length() - ( sub_pos + 2 ) );
				done = true;
			}
		}
		if ( !done ) from = pos + 1;
	}
}


bool split_line( const std::string &line, const std::string feature_type, const std::string &idattr,
				 std::string &ref_name, std::string &feature_id,
				 long &start, long &end, char &strand )
{
	bool res = ( ( line.length() > 0 )&&( line[ 0 ] != '#' ) );
	if ( res )
	{
		int field = 0;
		std::string attr;
		std::size_t from = 0;
		std::size_t pos = 0;
		start = end = 0;
		long value;
		bool done = false;
		while ( !done )
		{
			pos = line.find( "\t", from );
			done = ( pos == std::string::npos );
			switch( field++ )
			{
				case 0 : ref_name = line.substr( from, pos - from ); break;
				case 2 : if ( feature_type != line.substr( from, pos - from ) )
						  {
								done = true;
								res = false;
						  }
						  break;
				case 3 : if ( range::convert_value( line.substr( from, pos - from ), value ) ) start = value; break;
				case 4 : if ( range::convert_value( line.substr( from, pos - from ), value ) ) end = value; break;
				case 6 : if ( ( pos - from ) > 0 ) strand = line[ from ]; else strand = '?'; break;
				case 8 : attr = line.substr( from, pos - from ); break;
			}
			if ( !done ) from = pos + 1;
		}
		
		if ( res ) split_attr( attr, idattr, feature_id );
	}
	return res;
}


class gtf_iter
{
	private :
		const std::string filename;
		const std::string idattr;
		const std::string feature_type;
		feature_range * stored_feature_range;
		feature * stored_next_feature;
		std::ifstream inputstream;
		
		feature_range * read_next_feature_range( void )
		{
			feature_range * res = NULL;
			if ( inputstream.good() )
			{
				std::string line;
				if ( std::getline( inputstream, line ) )
				{
					std::string ref_name, feature_id;
					long start, end;
					char strand;
					if ( split_line( line, feature_type, idattr,
									  ref_name, feature_id, start, end, strand ) )
					{
						res = new feature_range( ref_name, feature_id, start, end, strand );
					}
				}
			}
			return res;
		}

		feature_range * next_feature_range( void )
		{
			feature_range * res = NULL;
			if ( stored_feature_range != NULL )
			{
				res = stored_feature_range;
				stored_feature_range = NULL;
			}
			else while ( inputstream.good() && res == NULL )
			{
				/* because read_next_feature_range can return NULL if the line contains the wrong feature_type */
				res = read_next_feature_range();
			}
			return res;
		}

		feature * read_next_feature( void )
		{
			feature * res = NULL;
			feature_range * fr = next_feature_range();
			if ( fr != NULL )
			{
				res = new feature( *fr );
				delete fr;
				bool done = ( res == NULL );
				while ( !done )
				{
					fr = next_feature_range();
					done = ( fr == NULL );
					if ( !done )
					{
						done = !( res -> add( *fr ) );
						if ( done )
							stored_feature_range = fr;
						else
							delete fr;
					}
				}
				if ( done ) res->sort_ranges();
			}
			return res;
		}
		
	public :
		gtf_iter( const char * filename_, const std::string &idattr_, const std::string &feature_type_  ) :
			filename( filename_ ), idattr( idattr_ ), feature_type( feature_type_ ),
			stored_feature_range( NULL ), inputstream( filename_ )
			{
				stored_next_feature = read_next_feature();
			}
		
		bool good( void ) { return ( stored_next_feature != NULL ); }
		
		feature * next_feature( void )
		{
			feature * res = stored_next_feature;
			if ( res != NULL )
				stored_next_feature = read_next_feature();
			return res;
		}

		feature * forward_to_next_ref( const std::string & ref )
		{
			feature * res = NULL;
			bool done = false;
			while ( !done )
			{
				res = next_feature();
				done = ( res == NULL );
				if ( !done )
				{
					if ( res -> is_ref( ref ) )
						delete res;
					else
						done = true;
				}
			}
			return res;
		}
		
		long peek_next_feature_start( void )
		{
			if ( stored_next_feature != NULL )
				return stored_next_feature -> start();
			else
				return 0;
		}

		bool peek_next_feature_range( range &r )
		{
			bool res = ( stored_next_feature != NULL );
			if ( res ) stored_next_feature -> get_outer_range( r );
			return res;
		}
		
		bool peek_next_feature_is_ref( const std::string &ref_name )
		{
			if ( stored_next_feature != NULL )
				return stored_next_feature -> is_ref( ref_name );
			else
				return false;
		}
};


class global_counter
{
	private:
		long refs;
		long total_alignments;
		long no_feature;
		long ambiguous;
		long too_low_qual;
		long not_aligned;
		long not_unique;
		
	public:
		global_counter( void ) : refs( 0 ), total_alignments( 0 ), no_feature( 0 ), ambiguous( 0 ),
				too_low_qual( 0 ), not_aligned( 0 ), not_unique( 0 ) {}

		void inc_refs( void ) { refs++; }		
		void inc_total_alignments( void ) { total_alignments++; }
		void inc_no_feature( void ) { no_feature++; }
		void inc_ambiguous( void ) { ambiguous++; }
		void inc_too_low_qual( void ) { too_low_qual++; }
		void inc_not_aligned( void ) { not_aligned++; }
		void inc_not_unique( void ) { not_unique++; }
		
		void report( void )
		{
			std::cout << std::endl;
			std::cout << "__no_feature\t" << no_feature << std::endl;
			std::cout << "__ambiguous\t" << ambiguous << std::endl;
			std::cout << "__too_low_aQual\t" << too_low_qual << std::endl;
			std::cout << "__not_aligned\t" << not_aligned << std::endl;
			std::cout << "__alignment_not_unique\t" << not_unique << std::endl;

			std::cout << std::endl << "total\t" << total_alignments << std::endl;
			std::cout << "refs\t" << refs << std::endl << std::endl;
		}
};


class feature_list
{
	private :
		const std::string &ref_name;
		int output_mode;
		std::list< feature * > flist;

		void clear( void )
		{
			while ( !flist.empty() )
			{
				feature * f = flist.front();
				flist.pop_front();
				if ( f != NULL ) delete f;
			}
		}

		void add( feature * f )
		{
			flist.push_back( f );
		}
		
	public :
		feature_list( const std::string &ref_name_, int output_mode_, feature * f )
			: ref_name( ref_name_ ), output_mode( output_mode_ ) { add( f ); }
		~feature_list( void ) { clear(); }

		bool empty( void ) { return flist.empty(); }
		
		void remove_all_features_before( const range &al_range )
		{
			bool done = flist.empty();
			while ( !done )
			{
				feature * f = flist.front();
				if ( f -> ends_before( al_range ) )
				{
					flist.pop_front();
					f -> report( output_mode );
					done = flist.empty();
					delete f;
				}
				else
					done = true;
			}
		}

		void load_all_features_starting_inside( const range &al_range, gtf_iter &gtf_it )
		{
			bool done = false;
			while ( !done )
			{
				done = !gtf_it.peek_next_feature_is_ref( ref_name );
				if ( !done )
				{
					range r;
					done = !gtf_it.peek_next_feature_range( r );
					if ( !done )
					{
						if ( r.ends_before( al_range ) )
						{
							/* we can skip this feature! */
							feature * f = gtf_it.next_feature();
							delete f;
						}
						else if ( r.starts_after( al_range ) )
						{
							/* we are done, this feature if for alignments that come later */
							done = true;
						}
						else
						{
							feature * f = gtf_it.next_feature();
							if ( f != NULL ) add( f );
						}
					}
				}
			}
		}
		
		void count_matches_between_alignment_and_features( const range &al_range )
		{
			std::list< feature * >::iterator it;
			for ( it = flist.begin(); it != flist.end(); ++it )
			{
				feature * f = *it;
				if ( f != NULL )
				{
					f -> check( al_range );
				}
			}
		}
};


class iter_window
{
	private :
		ngs::ReadCollection &run;	
		gtf_iter &gtf_it;
		int output_mode;
		global_counter counter;
		
	public :
		iter_window( ngs::ReadCollection &run_, gtf_iter &gtf_it_, int output_mode_ )
			: run( run_ ), gtf_it( gtf_it_ ), output_mode( output_mode_ ) {}

		void walk( void )
		{
			feature * f = gtf_it.next_feature();
			while ( f != NULL )
			{
				/* get the name of the reference of the fist feature in here... */
				std::string ref_name;
				f -> get_ref_name( ref_name );
				try
				{
					ngs::Reference ref = run.getReference ( ref_name );
					try
					{
						bool done = false;
						feature_list flist( ref_name, output_mode, f );
						ngs::AlignmentIterator al_iter = ref.getAlignments( ngs::Alignment::primaryAlignment );

						std::cout << std::endl << "processing ref: " << ref_name << std::endl;
						std::cout << "-------------------------------------------" << std::endl;
						counter.inc_refs();
						
						/* now walk all alignments of this al_iter */
						while ( al_iter.nextAlignment() && !done )
						{
							int64_t  pos = al_iter.getAlignmentPosition() + 1; /* al_iter returns 0-based ! */
							uint64_t len = al_iter.getAlignmentLength();
							
							const range al_range( pos, pos + len - 1 );
							
							/* remove and report all features that end before this alignment */
							flist.remove_all_features_before( al_range );
							flist.load_all_features_starting_inside( al_range, gtf_it );
							
							/* now we can count matches between the feature-list and this alignment */
							flist.count_matches_between_alignment_and_features( al_range );
							
							/* look if we have to cancel the loop if the feature-list is empty
							   and we have no more features in this reference */
							if ( flist.empty() )
							{
								done = ( gtf_it.peek_next_feature_start() == 0 );
								if ( !done )
									done = !gtf_it.peek_next_feature_is_ref( ref_name );
							}
							counter.inc_total_alignments();
						}
						f = gtf_it.forward_to_next_ref( ref_name );
					}
					catch ( ngs::ErrorMsg e )
					{
						std::cout << "error in ref " << ref_name << " : " << e.what() << std::endl;
						f = NULL;
					}
				}
				catch ( ngs::ErrorMsg e )
				{
					f = gtf_it.forward_to_next_ref( ref_name );
				}

			}
			counter.report();
		}
};


int matching( const struct sra_seq_count_options * options )
{
	int res = 0;
	
	std::string id_attr( options->id_attrib );
	std::string feature_type( options->feature_type );

	/* create the ngs-iterator, which delivers references and alignments */
	try
	{
		ngs::ReadCollection run ( ncbi::NGS::openReadCollection( options->sra_accession ) );
		
		/* create the gtf-iterator, which delivers gtf-features */		
		gtf_iter gtf_it( options->gtf_file, id_attr, feature_type );
		
		/* create a iterator window that walks both iterators to count alignments for the features */
		iter_window window( run, gtf_it, options->output_mode );
		
		/* walk the window... */
		window.walk();
	}
	catch ( ngs::ErrorMsg e )
	{
		std::cout << "cannot open " << options->sra_accession << " because " << e.what() << std::endl;
	}
	return res;
}


int list_refs_in_gtf( const char * gtf )
{
	int res = 0;
	long feature_count = 0;
	long feature_ref_count = 0;
	std::string ref_name;
	std::string id_attr( "gene_id" );
	std::string feature_type( "exon" );

	/* create the gtf-iterator, which delivers gtf-features */		
	gtf_iter gtf_it( gtf, id_attr, feature_type );
	
	feature * f = gtf_it.next_feature();
	while ( f != NULL )
	{
		/* count feature */
		feature_count ++;

		if ( f -> is_ref( ref_name ) )
		{
			feature_ref_count++;
		}
		else
		{
			if ( feature_ref_count > 0 )
			{
				std::cout << ref_name << "\t" << feature_ref_count << std::endl;
				feature_ref_count = 0;
			}
			f -> get_ref_name( ref_name );
		}

		/* dispose feature */
		delete f;
		f = gtf_it.next_feature();	
	}
	std::cout << feature_count << " features" << std::endl;
	
	return res;
}


void print_rr( const range &A, const range &B )
{
	std::cout << A << " --- " << B << " : " << range::range_relation_2_str( A.range_relation( B ) ) << std::endl;
}

void rr_test( void )
{
	/*
	print_rr( range( 10, 20 ), range( 5, 7 ) );
	print_rr( range( 10, 20 ), range( 5, 10 ) );

	print_rr( range( 10, 20 ), range( 5, 12 ) );
	print_rr( range( 10, 20 ), range( 5, 20 ) );
	
	print_rr( range( 10, 20 ), range( 5, 22 ) );
	
	print_rr( range( 10, 20 ), range( 10, 20 ) );
	print_rr( range( 10, 20 ), range( 5, 25 ) );
	
	print_rr( range( 10, 20 ), range( 15, 25 ) );
	print_rr( range( 10, 20 ), range( 20, 25 ) );

	print_rr( range( 10, 20 ), range( 25, 35 ) );
	*/
	
	ranges rl;
	rl.add( range( 10, 20 ) );
	rl.add( range( 70, 80 ) );	
	rl.add( range( 30, 40 ) );
	rl.add( range( 50, 60 ) );
	rl.merge( range( 18, 22 ) );	
	
	rl.sort();
	
	std::cout << rl << std::endl;
}

/*
int main( int argc, const char* argv[] )
{
	int res = 0;
	if ( argc < 3 )
	{
		std::cout << "\nusage : gene_count sra_accession genes.gtf\n";
		res = -1;
	}
	else
	{
		matching( argv[ 1 ], argv[ 2 ] );
		// list_refs_in_gtf( argv[ 2 ] );
		// rr_test();
	}
	
    return res;
}
*/
