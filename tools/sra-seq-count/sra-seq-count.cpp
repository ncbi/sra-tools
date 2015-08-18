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

#include <klib/text.h>
#include <klib/out.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <list>
#include <algorithm>

#include "options.h"
#include "range.hpp"
#include "tokens.hpp"
#include "string_class.hpp"
#include "string_tokens.hpp"
#include "file_iter.hpp"
#include "translation.hpp"

#include "str_set.hpp"
#include "str_2_str_map.hpp"
#include "str_2_u64_map.hpp"
#include "str_2_strset_map.hpp"
#include "u64_2_feature_map.hpp"

#include "gtf_pre_processor.hpp"
#include "gtf_feature_reader.hpp"

using namespace seq_ranges;


struct feature_range
{
	std::string ref_name;
	std::string feature_id;
	char strand;
	long start, end;
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
				strand( fr.strand ), outer( fr.start, fr.end ), counter( 0 )
		{
			feature_ranges.add( fr.start, fr.end );
		}
	
		bool add( const feature_range &fr )
		{
			bool res = ( feature_id == fr.feature_id );
			if ( res )
			{
				range r( fr.start, fr.end );
				feature_ranges.merge( r );
				outer.include( r );
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

		void print( std::ostream &stream ) const
		{
			stream << ref_name << "." << outer << " id=" << feature_id << std::endl;
		}		
		
		friend std::ostream& operator<< ( std::ostream &stream, const feature &other )
		{
			other.print( stream );
			return stream;
		}

		void sort_ranges( void ) { feature_ranges.sort(); }		
		void get_ref_name( std::string &s ) { s = ref_name; }
		void get_outer_range( range &r ) { r = outer; }

		/* does this feature end before the given range */
		bool ends_before( const range &r ) const { return outer.ends_before( r ); }
		bool is_ref( const std::string &r_name ) { return ( ref_name == r_name ); }
		long start( void ) { return outer.get_start(); }
		
		bool check( const range &r )
		{
			bool res = ( outer.intersect( r ) );
			if ( res ) counter++;
			return res;
		}
};


class gtf_iter
{
	private :
		const std::string filename;
		const std::string idattr;
		const std::string feature_type;
		feature_range * stored_feature_range;
		feature * stored_next_feature;
		std::ifstream inputstream;

		bool extract_feature_id( tokens &t, std::string &feature_id )
		{
			bool found = false;
			std::string attr_name, attr_value;
			while ( t.get( ' ', attr_name ) && t.get( ';', attr_value ) && t.skip( ' ' ) && !found )
			{
				found = ( attr_name == idattr );
				if ( found )
					/* un-quote the value */
					feature_id = attr_value.substr( 1, attr_value.length() - 2 );
			}
			return found;
		}
		
		feature_range * read_next_feature_range( void )
		{
			feature_range * fr = new feature_range;
			bool valid = false;
			if ( inputstream.good() )
			{
				std::string line;
				if ( std::getline( inputstream, line ) )
				{
					if ( line.length() > 0 && line[ 0 ] != '#' )
					{
						tokens t( line, '\t' );
						if ( t.get( fr->ref_name ) && t.skip() )
						{
							std::string this_feature_type;
							if ( t.get( this_feature_type ) && ( feature_type == this_feature_type ) )
							{
								if ( t.get_long( &fr->start ) && t.get_long( &fr->end ) && t.skip() )
								{
									std::string strand ;
									if ( t.get( strand ) && t.skip() && extract_feature_id( t, fr->feature_id ) )
									{
										fr->strand = strand[ 0 ];
										valid = true;
									}
								}
							}
						}
					}
				}
			}
			if ( !valid )
			{
				delete fr;
				fr = NULL;
			}
			return fr;
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
			feature * f = NULL;
			feature_range * fr = next_feature_range();
			if ( fr != NULL )
			{
				f = new feature( *fr );
				delete fr;
				bool done = ( f == NULL );
				while ( !done )
				{
					fr = next_feature_range();
					done = ( fr == NULL );
					if ( !done )
					{
						done = !( f -> add( *fr ) );
						if ( done )
							stored_feature_range = fr;
						else
							delete fr;
					}
				}
				if ( done ) f->sort_ranges();
			}
			return f;
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
			std::cout << "total\t" << total_alignments << std::endl;
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
		
	public :
		feature_list( const std::string &ref_name_, int output_mode_, feature * f )
			: ref_name( ref_name_ ), output_mode( output_mode_ ) { flist.push_back( f ); }
		~feature_list( void ) { clear(); }

		bool empty( void ) { return flist.empty(); }
		
		/* returns how many features have been reported */
		int remove_all_features_before( const range &al_range )
		{
			int res = 0;
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
					res++;
				}
				else
					done = true;
			}
			return res;
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
							if ( f != NULL ) flist.push_back( f );
						}
					}
				}
			}
		}
		
		void count_matches_between_alignment_and_features( const range &al_range, global_counter &counter )
		{
			std::list< feature * >::iterator it;
			for ( it = flist.begin(); it != flist.end(); ++it )
			{
				feature * f = *it;
				if ( f != NULL )
				{
					if ( !f -> check( al_range ) )
						counter.inc_no_feature();
				}
				counter.inc_total_alignments();
			}
		}
};


class iter_window
{
	private :
		ngs::ReadCollection &run;	
		gtf_iter &gtf_it;
		translation trans;
		global_counter counter;
		long int gene_count;
		long int max_genes;
		int output_mode;
		bool quiet;
		
	public :
		iter_window( ngs::ReadCollection &run_, gtf_iter &gtf_it_,
					 int output_mode_, int max_genes_, const char * trans_file, bool quiet_ )
			: run( run_ ), gtf_it( gtf_it_ ), trans( trans_file ),
			  gene_count( 0 ), max_genes( max_genes_ ), output_mode( output_mode_ ), quiet( quiet_ )
			  { }

		void walk( void )
		{
			feature * f = gtf_it.next_feature();
			while ( f != NULL )
			{
				/* get the name of the reference of the fist feature in here... */
				std::string ref_name;
				f -> get_ref_name( ref_name );
				trans.translate( ref_name, ref_name );
				try
				{
					ngs::Reference ref = run.getReference ( ref_name );
					try
					{
						bool done = false;
						feature_list flist( ref_name, output_mode, f );
						ngs::AlignmentIterator al_iter = ref.getAlignments( ngs::Alignment::primaryAlignment );

						if ( !quiet )
						{
							std::cout << std::endl << "processing ref: " << ref_name << std::endl;
							std::cout << "-------------------------------------------" << std::endl;
						}
						counter.inc_refs();
						
						/* now walk all alignments of this al_iter */
						while ( al_iter.nextAlignment() && !done )
						{
							int64_t  pos = al_iter.getAlignmentPosition() + 1; /* al_iter returns 0-based ! */
							uint64_t len = al_iter.getAlignmentLength();
							
							const range al_range( pos, pos + len - 1 );
							
							/* remove and report all features that end before this alignment */
							gene_count += flist.remove_all_features_before( al_range );
							flist.load_all_features_starting_inside( al_range, gtf_it );
							
							/* now we can count matches between the feature-list and this alignment */
							flist.count_matches_between_alignment_and_features( al_range, counter );
							
							/* look if we have to cancel the loop if the feature-list is empty
							   and we have no more features in this reference */
							if ( flist.empty() )
							{
								done = ( gtf_it.peek_next_feature_start() == 0 );
								if ( !done )
									done = !gtf_it.peek_next_feature_is_ref( ref_name );
							}

							if ( max_genes > 0 && !done )
								done = ( gene_count > max_genes );
						}
						
						if ( max_genes > 0 && ( gene_count > max_genes ) )
							f = NULL;
						else
							f = gtf_it.forward_to_next_ref( ref_name );
					}
					catch ( ngs::ErrorMsg e )
					{
						if ( !quiet )
						{
							std::cout << "error in ref " << ref_name << " : " << e.what() << std::endl;
						}
						f = NULL;
					}
				}
				catch ( ngs::ErrorMsg e )
				{
					f = gtf_it.forward_to_next_ref( ref_name );
				}

			}
			counter.report();
			trans.report();
		}
};


/* 1st attempt walk the gtf-file-references and line the read-collection up to it */
int perform_seq_counting_1( const struct sra_seq_count_options * options )
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
		iter_window window( run, gtf_it, options->output_mode, options->max_genes,
							options->ref_trans, options->silent );
		
		/* walk the window... */
		window.walk();
	}
	catch ( ngs::ErrorMsg e )
	{
		std::cout << "cannot open " << options->sra_accession << " because " << e.what() << std::endl;
	}
	return res;
}



/* ===================================================================================== */
/* 2nd attempt walk read-collection reference-wise and line the gtf-file up against that */

int perform_seq_counting_2( const struct sra_seq_count_options * options )
{
	int res = 0;

	/* instantiate a file iter ( can break a text-file into lines without using STL ) */
	file_iter f( options->gtf_file );
	if ( !f.good() )
	{
		if ( !options->silent )
			KOutMsg( "cannot open gtf-file '%s', aborting!\n", options->gtf_file );
		return 1;
	}
	
	/* instantiate a dictionary of string->uint64_t to record the starting offset of each
	   reference in the gtf-file */
	str_2_u64_map ref_starts;
	
	/* instantiate and run the pre-processor to record the start-position of each
		reference in the ref-starts-map */
	gtf_pre_processor pre( f, ref_starts, options->silent );
	if ( !options->silent )
		KOutMsg( "\npre-processing gtf-file:\n" );
	pre.run();
	
	/* check how many references we have extracted from the gtf-file */
	if ( ref_starts.get_count() == 0 )
	{
		if ( !options->silent )
			KOutMsg( "No references found the gtf-file. aborting!\n" );
		return 2;
	}
	else
	{
		if ( !options->silent )
			KOutMsg( "%lu references found in the gtf-file '%s'\n", ref_starts.get_count(), options->gtf_file );
	}
	
	
	/* create the ref-name-translation object and read from optional tranlation-file */
	str_2_str_map ref_translation;
	ref_translation.read_from_file( options->ref_trans, '=' );
	
	/* report how many reference-translations have been loaded */
	if ( !options->silent && ref_translation.get_count() > 0 )
		KOutMsg( "%lu reference-name translations loaded from '%s'\n", ref_translation.get_count(), options->ref_trans );

	/* if we restrict the tool to only use these ref-names: extract the set from the options */
	str_set only_these_refs;
	if ( options->refs != NULL )
	{
		String RefString, RefToken;
		StringInitCString( &RefString, options->refs );
		string_tokens ref_tokens( RefString, ',' ); /* comma-separated list of ref-names */
		while ( ref_tokens.get( RefToken ) )
			only_these_refs.insert( &RefToken );
		if ( !options->silent )
		{
			KOutMsg( "restricted to these references: " );
			only_these_refs.report( "%S, " );
			KOutMsg( "\n" );
		}
	}

	/* create the ngs-iterator, which delivers references and alignments */
	try
	{
		ngs::ReadCollection run ( ncbi::NGS::openReadCollection( options->sra_accession ) );
		str_set refs_in_run;
		try
		{
			ngs::ReferenceIterator ref_iter( run.getReferences() );
			while( ref_iter.nextReference () )
			{
				const std::string run_ref_name = ref_iter.getCommonName();
				refs_in_run.insert( run_ref_name.c_str() );
			}
		}
		catch ( ngs::ErrorMsg e )
		{
			std::cout << "cannot get reference-iterator " << options->sra_accession << " because " << e.what() << std::endl;
			res = 3;
		}
		
		if ( res == 0 )
		{
			if ( refs_in_run.get_count() == 0 )
			{
				if ( !options->silent )
					KOutMsg( "No alignments found in accession (contains only unaligned data). aborting! \n" );
				return 4;
			}
			else
			{
				if ( !options->silent )
					KOutMsg( "%lu references in accession '%s'\n", refs_in_run.get_count(), options->sra_accession );

				str_2_strset_map ref_groups;
				
				/* here we continue to create a list of reference-groups to process :
				   walk the list of ref-names from sra-accession, translate them ( if we can ) into gtf-file-refnames,
				   find match in ref-names of the gtf-file... */
				const String * AccRef;
				refs_in_run.first();
				while ( refs_in_run.get_next( &AccRef ) )
				{
					bool use_this_ref = ( options->refs == NULL );
					if ( !use_this_ref ) use_this_ref = only_these_refs.has( AccRef );
					if ( use_this_ref )
					{
						const String * AccRefTrans;
						uint64_t gtf_file_offset, gtf_line_count;
						if ( ref_translation.get_value( AccRef, &AccRefTrans ) )
						{
							ref_starts.get_value( AccRefTrans, &gtf_file_offset, &gtf_line_count );
							ref_groups.insert( AccRefTrans, AccRef );
						}
						else
						{
							ref_starts.get_value( AccRef, &gtf_file_offset, &gtf_line_count );
							ref_groups.insert( AccRef, AccRef );
						}
					}
				}

				/* now walk the reference-groups : */
				const String  * GtfRef;
				str_set * AccRefSet;
				ref_groups.first();
				while( ref_groups.get_next( &GtfRef, &AccRefSet ) && res == 0 )
				{
					KOutMsg( "gtf: '%S'\n", GtfRef );
					/* read all features of this reference from the gtf-file */
					/* ===================================================== */

					uint64_t start_ofs, line_count;
					ref_starts.get_value( GtfRef, &start_ofs, &line_count );
					f.seek( start_ofs );
					u64_2_feature_map features; /* in this map we store all features of this reference */
					
					gtf_feature_reader reader( f, features, line_count, options->id_attrib, options->feature_type );
					reader.run();
					KOutMsg( "%lu features in '%S'\n", features.get_count(), GtfRef );
					features.report();
					
					AccRefSet -> first();
					while ( AccRefSet -> get_next( &AccRef ) && res == 0 )
					{
						/* get a reference-iterator from the ngs-readcollection for this reference */
						/* ======================================================================= */
						const std::string acc_ref_std_string( AccRef->addr );
						try
						{
							ngs::Reference Reference = run.getReference ( acc_ref_std_string );
						
							/* get all alignments of this reference and count present it to the feature-set */
							/* ============================================================================ */
							try
							{
								uint64_t align_count = 0;
								
								ngs::AlignmentIterator al_iter = Reference.getAlignments ( ngs::Alignment::primaryAlignment );
								KOutMsg( "   * acc: '%S'\n", AccRef );

								while ( al_iter.nextAlignment() )
								{
									/* get the alignment as a feature */
									bool process;
									if ( options->mapq > 0 )
									{
										int mapq = al_iter.getMappingQuality();			/* we might filter by that... */
										process = ( mapq >= options->mapq );
									}
									else
										process = true;

									if ( process )
									{
										uint64_t start		= al_iter.getAlignmentPosition() + 1;	/* al_iter returns 0-based ! */
										uint64_t end   		= start + al_iter.getAlignmentLength() - 1;

										ngs::StringRef cigar( al_iter.getShortCigar( true ) );		/* get short cigar... */
										String Cigar;
										StringInit( &Cigar, cigar.data(), cigar.size(), cigar.size() );
										
										bool reversed		= al_iter.getIsReversedOrientation();	/* on which strand */

										gtf_feature alignment( reversed, start, end, Cigar );
										
										/* now match it with the gtf-features... */
									}
									
									align_count++;
									if ( align_count % 10000 == 0 )
										KOutMsg( "." );
								}
								KOutMsg( "\n%lu alignments\n", align_count );
								
							}
							catch ( ngs::ErrorMsg e )
							{
								std::cout << "cannot get ngs-alignment-iterator " << acc_ref_std_string << " because " << e.what() << std::endl;
								res = 6;
							}
						}
						catch ( ngs::ErrorMsg e )
						{
							std::cout << "cannot get ngs-reference for " << acc_ref_std_string << " because " << e.what() << std::endl;
							res = 5;
						}
					}
				}
			}
		}
		
	}
	catch ( ngs::ErrorMsg e )
	{
		std::cout << "cannot open " << options->sra_accession << " because " << e.what() << std::endl;
		res = 5;
	}
	return res;
}

/* ===================================================================================== */

void list_gtf_refs( const struct sra_seq_count_options * options )
{
	long feature_count = 0;
	long feature_ref_count = 0;
	std::string ref_name;
	
	std::cout << "references in gtf-file:" << std::endl;
	
	std::string id_attr( options->id_attrib );
	std::string feature_type( options->feature_type );

	/* create the gtf-iterator, which delivers gtf-features */		
	gtf_iter gtf_it( options->gtf_file, id_attr, feature_type );
	
	feature * f = gtf_it.next_feature();
	while ( f != NULL )
	{
		feature_count ++;
		if ( f -> is_ref( ref_name ) )
			feature_ref_count++;
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
}


void list_acc_refs( const struct sra_seq_count_options * options )
{
	std::cout << "references in accession:" << std::endl;
	try
	{
		ngs::ReadCollection run ( ncbi::NGS::openReadCollection( options->sra_accession ) );
		try
		{
			ngs::ReferenceIterator ref_iter( run.getReferences() );
			while( ref_iter.nextReference () )
				std::cout << ref_iter.getCommonName() << std::endl;
		}
		catch ( ngs::ErrorMsg e )
		{
			std::cout << "cannot get reference-iterator " << options->sra_accession << " because " << e.what() << std::endl;
		}
		
	}
	catch ( ngs::ErrorMsg e )
	{
		std::cout << "cannot open " << options->sra_accession << " because " << e.what() << std::endl;
	}
}


bool is_gtf_sorted( const struct sra_seq_count_options * options )
{
	bool res = true;
	long start = 0;
	std::string id_attr( options->id_attrib );
	std::string feature_type( options->feature_type );
	std::string ref_name;

	/* create the gtf-iterator, which delivers gtf-features */		
	gtf_iter gtf_it( options->gtf_file, id_attr, feature_type );
	
	feature * f = gtf_it.next_feature();
	while ( f != NULL && res )
	{
		if ( f -> is_ref( ref_name ) )
		{
			/* we are on the same-ref */
			long f_start = f->start();
			if ( !( f_start >= start ) )
				std::cout << "this feature breaks sorting: " << *f << " previous was at " << start << std::endl;
			start = f_start;
		}
		else
		{
			/* we are on a new ref */
			start = 0;
			f -> get_ref_name( ref_name );
		}

		/* dispose feature */
		delete f;
		if ( res )
			f = gtf_it.next_feature();
		else
			f = NULL;
	}
	
	return res;
}

bool is_acc_sorted( const struct sra_seq_count_options * options )
{
	return true;
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


void perform_ranges_test( const struct sra_seq_count_options * options )
{
	ranges r1 ( 1, options->sra_accession );
	ranges r2 ( 1, options->gtf_file );
	
	std::cout << std::endl << "ranges_1: " << r1 << std::endl;
	std::cout << "ranges_2: " << r2 << std::endl;
	
	ranges_relation rr_1_vs_2;
	r1.compare_vs_ranges( r2, rr_1_vs_2, true );
	std::cout << "range-relation ( r1 vs r2 ): " << rr_1_vs_2 << std::endl;

	ranges_relation rr_2_vs_1;
	r2.compare_vs_ranges( r1, rr_2_vs_1, true );
	std::cout << "range-relation ( r2 vs r1 ): " << rr_2_vs_1 << std::endl;
}


void token_tests( const struct sra_seq_count_options * options )
{
	file_iter f( options->gtf_file );
	str_2_u64_map ref_starts;
	
	gtf_pre_processor pre( f, ref_starts, false );
	pre.run();
	
	ref_starts.report( NULL );
	/*
	str_2_str_map dict;
	if ( dict.read_from_file( options -> ref_trans, '=' ) );
		dict.report();
	*/
}

void index_gtf( const struct sra_seq_count_options * options )
{
	file_iter f( options->gtf_file );
	if ( f.good() )
	{
		if ( options->max_genes > 0 )
		{
			int n = 0;
			uint64_t pos;
			String S;
			while ( f.get_line( S, &pos ) && n++ < options->max_genes )
				KOutMsg( "[%lu] %S\n", pos, &S );
		}
		else
		{
			String S;
			while ( f.get_line( S ) )
				KOutMsg( "%S\n", &S );
		}
	}	
}