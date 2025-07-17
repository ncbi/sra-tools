#pragma once

#include <cstdint>
#include <list>
#include <memory>
#include <iostream>

namespace vdb {

struct RowRange;
typedef std::list< RowRange > list_of_range;
struct RowRange {
    int64_t first;
    uint64_t count;

    RowRange() : first( 0 ), count( 0 ) {}
    RowRange( int64_t a_first, uint64_t a_count ) : first( a_first ), count( a_count ) {}
    RowRange( const RowRange& other ) : first( other.first ), count( other.count ) {}

    RowRange& operator=( const RowRange& other ) {
        if ( this != &other ) {
            first = other . first;
            count = other . count;
        }
        return *this;
    }

    bool empty( void ) const { return ( 0 == count ); }

    list_of_range split( uint32_t parts, uint32_t min_count ) const {
        list_of_range res;
        if ( count > 0 ) {
            if ( count < min_count ) {
                res. push_back( RowRange{ first, count } );
            } else {
                uint64_t part_size = ( count / parts );
                int64_t start = first;
                for ( uint32_t loop = 0; loop < parts - 1; ++loop ) {
                    res. push_back( RowRange{ start, part_size } );
                    start += part_size;
                }
                res. push_back( RowRange{ start, count - start + 1 } );
            }
        }
        return res;
    }

    friend auto operator<<( std::ostream& os, RowRange const& r ) -> std::ostream& {
        int64_t end = r . first + r . count - 1;
        return os << r . first << ".." << end << " (" << r . count << ")";
    }
};

struct RowRangeIter{
    const RowRange range;
    uint64_t offset;

    RowRangeIter( const RowRange& a_range ) : range( a_range ), offset( 0 ) { }

    bool empty( void ) const { return range . empty(); }
    bool done( void ) const { return offset >= range . count; }
    void next( void ) { offset++; }

    bool next( int64_t& row ) {
        if ( empty() || done() ) { return false; }
        row = offset++ + range . first;
        return true;
    }

    int64_t get_row( void ) const { return offset + range . first; }

    bool has_data( int64_t& row, bool inc = false) {
        bool res = !empty() && offset < range . count;
        if ( res ) {
            row = offset + range . first;
            if ( inc ) offset++;
        }
        return res;
    }

    friend auto operator<<( std::ostream& os, RowRangeIter const& it ) -> std::ostream& {
        return os << it . range << " | offset = " << it . offset;
    }
};

};
