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

#ifndef _hpp_seq_ranges_
#define _hpp_seq_ranges_

#include <list>

namespace seq_ranges {

enum e_range_relation {
	rr_before,
	rr_begin,
	rr_span,
	rr_inside,
	rr_end,
	rr_after
};


/* -----------------------------------------------------------------------
	range-relations:

	A            |-------------|
	B   |-----|
	A.range_rel( B ) = rr_before

	A      |-------------|
	B   |-----|
	A.range_rel( B ) = rr_begin

	A      |-------------|
	B   |-------------------|
	A.range_rel( B ) = rr_span
	
	A      |-------------|
	B           |-----|
	A.range_rel( B ) = rr_inside
	
	A   |-------------|
	B              |-----|
	A.range_rel( B ) = rr_end

	A   |-------------|
	B                    |-----|
	A.range_rel( B ) = rr_after
	
   ----------------------------------------------------------------------- */

class range
{
	private :
		long start;
		long end;

	public :
		range( void ) : start( 0 ), end( 0 ) { }
		range( long start_, long end_ ) : start( start_ ), end( end_ ) { }
		range( const range &other ) : start( other.start ), end( other.end ) { }
		
		range( const std::string &start_, const std::string &end_ ) : start( 0 ), end( 0 )
		{
			bool valid = ( convert_value( start_, start ) && convert_value( end_, end ) );
			if ( !valid ) set( 0, 0 );
		}

		range& operator= ( const range &other )
		{
			if ( this == &other ) return *this;
			start = other.start;
			end = other.end;
			return *this;
		}

		void set( long start_, long end_ ) { start = start_; end = end_; }

		static bool convert_value( const std::string value_, long &value )
		{
			bool res = true;
			std::istringstream ( value_ ) >> value;
			if ( value < 1 )
				res = false;
			return res;
		}
		
		/* does this range end before the other range starts ? */
		bool ends_before( const range &other ) const { return ( end < other.start ); }

		/* does this range start after the other range ends ? */
		bool starts_after( const range &other ) const { return ( start > other.end ); }
		
		bool intersect( const range &other ) const
		{
			e_range_relation rr = range_relation( other );
			return ( rr == rr_begin || rr == rr_span || rr == rr_inside || rr == rr_end );
		}

		bool intersect( const long value ) const
		{ return ( value >= start && value <= end ); }
		
		void include( const range &other )
		{
			if ( other.start < start ) start = other.start;
			if ( other.end > end ) end = other.end;
		}
		
		bool merge( const range &other )
		{
			bool res = intersect( other );
			if ( res ) include( other );
			return res;
		}

		enum e_range_relation range_relation( const range &other ) const
		{
			enum e_range_relation res;
			if ( other.start < start )
			{
				if ( other.end < start )
					res = rr_before;
				else if ( other.end <= end )
					res = rr_begin;
				else
					res = rr_span;
			}
			else
			{
				if ( other.start <= end )
				{
					if ( other.end <= end )
						res = rr_inside;
					else
						res = rr_end;
				}
				else
					res = rr_after;
			}
			
			return res;
		}
		
		long get_start( void ) const { return start; }
		long get_end( void ) const { return end; }
		bool empty( void ) const { return ( start == 0 && end == 0 ); }

		void print( std::ostream &stream ) const { stream << "[ " << start << " .. " << end << " ]"; }

		friend std::ostream& operator<< ( std::ostream &stream, const range &other )
		{
			other.print( stream );
			return stream;
		}
		
		static std::string range_relation_2_str( enum e_range_relation rr )
		{
			switch ( rr )
			{
				case rr_before : return std::string( "before" ); break;
				case rr_begin  : return std::string( "begin" ); break;
				case rr_span   : return std::string( "span" ); break;
				case rr_inside : return std::string( "inside" ); break;		
				case rr_end    : return std::string( "end" ); break;		
				case rr_after  : return std::string( "after" ); break;				
			}
			return std::string( "unknown" );
		}
		
};


bool compare_ranges ( const range * first, const range * second )
{
	return ( first -> get_start() < second -> get_start() );
}


struct ranges_relation
{
	bool has_inside;
	bool has_partial;

	void clear( void ) { has_inside = has_partial = false; }
	
	void print( std::ostream &stream ) const { stream << "inside: " << has_inside << ", partial: " << has_partial; }
	
	friend std::ostream& operator<< ( std::ostream &stream, const ranges_relation &other )
	{
		other.print( stream );
		return stream;
	}
};


class ranges
{
	private :
		std::list< range * > range_list;
		long count;
		
	public :
		ranges( void ) : count( 0 ) {}
		~ranges( void ) { clear(); }
		
		void clear( void )
		{
			while ( !range_list.empty() )
			{
				range * r = range_list.front();
				range_list.pop_front();
				if ( r != NULL ) delete r;
			}
			count = 0;
		}
		
		void add( range * r ){ if ( r != NULL ) { range_list.push_back( r ); count++; } }
		void add( const range &r ){ add( new range( r ) ); }

		void merge( const range &r1 )
		{
			bool merged = false;
			std::list< range * >::iterator it;
			for ( it = range_list.begin(); it != range_list.end() && !merged; ++it )
			{
				range * r = *it;
				if ( r != NULL )
					merged = r -> merge( r1 );
			}
			if ( !merged ) add( r1 );
		}
		
		void sort( void ) { range_list.sort( compare_ranges ); }
		long get_count( void ) { return count; }
		
		void compare_sample( const ranges &sample, ranges_relation &res ) const
		{
			res.clear();
			bool done = false;
			
			// we take each range of the sample and compare it against each range of self
			std::list< range * >::const_iterator sample_it;
			for ( sample_it = sample.range_list.begin(); sample_it != sample.range_list.end() && !done; ++sample_it )
			{
				const range * sample_range = *sample_it;
				std::list< range * >::const_iterator pattern_it;
				for ( pattern_it = range_list.begin(); pattern_it != range_list.end() && !done; ++pattern_it )	
				{
					const range * pattern_range( *pattern_it );
					enum e_range_relation rr = pattern_range -> range_relation( *sample_range );
					switch( rr )
					{
						case rr_before 	: break;
						case rr_begin	: res.has_partial = true; break;
						case rr_span	: res.has_partial = true; break;
						case rr_inside 	: res.has_inside = true; break;
						case rr_end		: res.has_partial = true; break;
						case rr_after	: break;
					}
					done = ( res.has_partial && res.has_inside );
				}
			}
		}
		
		void print( std::ostream &stream ) const
		{
			std::list< range * >::const_iterator it;
			for ( it = range_list.begin(); it != range_list.end(); ++it )
			{
				range * r = *it;
				if ( r != NULL ) stream << *r << " ";
			}
		}		

		friend std::ostream& operator<< ( std::ostream &stream, const ranges &other )
		{
			other.print( stream );
			return stream;
		}

};

};  // namespace seq_ranges

#endif // _hpp_seq_ranges_

