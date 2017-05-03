/* =============================================================================
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
 * =============================================================================
 */

#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>
#include <cctype>
#include <cstdlib>

#include <klib/rc.h>
#include <klib/text.h>
#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include "fasta-file.hpp"

using namespace CPP;

/*
 * Fasta files:
 *  Fasta file consists of one of more sequences.  A sequence in a fasta file
 *  consists of a seqid line followed by lines containing the bases of the
 *  sequence.  A seqid line starts with '>' and the next word (whitespace
 *  delimited) is the seqid.
 */

static char const tr4na[] = {
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ','.',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ','A','B','C','D',' ',' ','G','H',' ',' ','K',' ','M','N',' ',' ',' ','R','S','T','T','V','W','N','Y',' ',' ',' ',' ',' ',' ',
    ' ','A','B','C','D',' ',' ','G','H',' ',' ','K',' ','M','N',' ',' ',' ','R','S','T','T','V','W','N','Y',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
    ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',' ',
};

FastaFile::FastaFile( std::istream &is ) : data( 0 )
{
    char *data = 0;
    size_t size = 0;
    
    {
        size_t limit = 1024u;
        void *mem = malloc( limit );
        if ( mem )
            data = reinterpret_cast<char *>( mem );
        else
            throw std::bad_alloc();

        for ( ; ; )
        {
            int const ch = is.get();
            if ( ch == std::char_traits<char>::eof() )
                break;
            if ( size + 1 < limit )
                data[ size++ ] = char( ch );
            else
            {
                size_t const newLimit = limit << 1;
                
                mem = realloc( mem, newLimit );
                if ( mem )
                {
                    data = reinterpret_cast<char *>( mem );
                    limit = newLimit;
                    data[ size++ ] = char(ch);
                }
                else
                    throw std::bad_alloc();
            }
        }
        
        mem = realloc( mem, size );
        if ( mem )
        {
            this->data = mem;
            data = reinterpret_cast<char *>( mem );
        }
        else
            throw std::bad_alloc();
    }
    
    std::vector< size_t > defline;
    
    {
        int st = 0;
        for ( size_t i = 0; i < size; ++i )
        {
            int const ch = data[ i ];
            if ( st == 0 )
            {
                if ( ch == '\r' || ch == '\n' )
                    continue;
                if ( ch == '>' )
                    defline.push_back(i);
                ++st;
            }
            else if ( ch == '\r' || ch == '\n' )
                st = 0;
        }
        defline.push_back( size );
    }
    
    unsigned const deflines = defline.size() - 1;
    {
        for ( unsigned i = 0; i < deflines; ++i )
        {
            Sequence seq = Sequence();
            
            size_t const offset = defline[ i ];
            size_t const length = defline[ i + 1 ] - offset;
            char *base = data + offset;
            char *const endp = base + length;
            
            while ( base < endp )
            {
                int const ch = *base++;
                if ( ch == '\r' || ch == '\n' )
                    break;
                seq.defline += ch;
            }
            seq.data = base;
            char *dst = base;
            {
                unsigned j = 1;
                while ( j < seq.defline.size() && isspace( seq.defline[j] ) )
                    ++j;
                while ( j < seq.defline.size() && !isspace( seq.defline[j] ) )
                    seq.SEQID += seq.defline[ j++ ];
            }

            while ( base < endp )
            {
                int const chi = *base++;
                if ( chi == '\r' || chi == '\n' )
                    continue;
                int const ch = tr4na[ chi ];
                if ( ch != ' ' )
                    *dst++ = ch;
                else
                {
                    seq.hadErrors = true;
                    *dst++ = 'N';
                }
            }
            seq.length = dst - seq.data;
            
            sequences.push_back( seq );
        }
    }
}

struct cursor_ctx
{
    const VCursor * curs;
    uint32_t rname_idx, read_idx;
    int64_t first_row;
    uint64_t row_count;
};

/* constructor for */
FastaFile::FastaFile( std::string const &accession, size_t cache_capacity ) : data( 0 )
{
    const VDBManager * mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    if ( rc != 0 )
        std::cerr << "cannot create manager..." << std::endl;
    else
    {
        const VDatabase * db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, accession.c_str() );
        if ( rc != 0 )
            std::cerr << "cannot open database: " << accession << std::endl;
        else
        {
            const VTable * tbl;
            rc = VDatabaseOpenTableRead( db, &tbl, "REFERENCE" );
            if ( rc != 0 )
                std::cerr << "cannot open table: " << accession << ".REFERENCE" << std::endl;
            else
            {
                struct cursor_ctx c;
                if ( cache_capacity > 0 )
                    rc = VTableCreateCachedCursorRead( tbl, &c.curs, cache_capacity );
                else
                    rc = VTableCreateCursorRead( tbl, &c.curs );
                if ( rc != 0 )
                    std::cerr << "cannot create cursor: " << accession << ".REFERENCE" << std::endl;
                else
                {
                    rc = VCursorAddColumn( c.curs, &c.rname_idx, "SEQ_ID" );
                    if ( rc != 0 )
                        std::cerr << "cannot add column SEQ_ID to cursor..." << std::endl;
                    if ( rc == 0 )
                    {
                        rc = VCursorAddColumn( c.curs, &c.read_idx, "READ" );
                        if ( rc != 0 )
                            std::cerr << "cannot add column READ to cursor..." << std::endl;
                        
                    }
                    if ( rc == 0 )
                    {
                        rc = VCursorOpen( c.curs );
                        if ( rc != 0 )
                            std::cerr << "cannot open cursor..." << std::endl;
                    }
                    if ( rc == 0 )
                    {
                        rc = VCursorIdRange( c.curs, c.read_idx, &c.first_row, &c.row_count );
                        if ( rc != 0 )
                            std::cerr << "cannot get row-count..." << std::endl;
                    }
                    
                    if ( rc == 0 )
                        rc = Create_From_Reftable_Cursor( &c );
                           
                    VCursorRelease( c.curs );
                }
                VTableRelease( tbl );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
}

struct buffer_t
{
    void * raw_mem;
    char * buffer;
    size_t used;
    size_t ref_start, ref_len;
    
    /* C'tor ( D'tor does not free the memory, because we are handing ponters out... ) */
    buffer_t( size_t initial_size )
    {
        used = 0;
        ref_start = 0;
        ref_len = 0;
        raw_mem = malloc( initial_size );
        if ( raw_mem != NULL )
            buffer = reinterpret_cast<char *>( raw_mem );
        else
            throw std::bad_alloc();
    }

    void append( String * data )
    {
        char * dst = &( buffer[ used ] );
        memmove( dst, data->addr, data->len );
        used += data->len;
        ref_len += data->len;
    }

    
    const char * start( void ) { return &( buffer[ ref_start ] ); }
    unsigned len( void ) { return ref_len; }

    void new_ref( void )
    {
        ref_start = used;
        ref_len = 0;
    }
    
};


unsigned FastaFile::Create_From_Reftable_Cursor( void * p )
{
    rc_t rc = 0;
    bool done = false;
    uint64_t rows_processed = 0;
    const String * current_ref = NULL;
    struct cursor_ctx * c = reinterpret_cast<struct cursor_ctx *>( p );
    buffer_t buf = buffer_t( 5000 * c->row_count );
    int64_t row_id = c->first_row;

    while ( rc == 0 && !done )
    {
        uint32_t elem_bits, boff, row_len;
        String rname, read;
        
        rc = VCursorCellDataDirect( c->curs, row_id, c->rname_idx, &elem_bits, ( const void ** )&rname.addr, &boff, &row_len );
        if ( rc == 0 )
            rname.len = rname.size = row_len;

        if ( rc == 0 )
        {
            rc = VCursorCellDataDirect( c->curs, row_id, c->read_idx, &elem_bits, ( const void ** )&read.addr, &boff, &row_len );
            if ( rc == 0 )
                read.len = read.size = row_len;
        }
        
        
        if ( rc == 0 )
        {
            /* here we handle the current ref-block... */
            int cmp = 1;
            
            if ( current_ref == NULL )
            {
                /*we have not seen a ref-block at all... */
                rc = StringCopy( &current_ref, &rname );
                if ( rc == 0 )
                    std::string s( rname.addr, rname.size );
            }
            else
            {
                /*we have seen a ref-block before... */
                cmp = StringCompare( current_ref, &rname );
                if ( cmp != 0 )
                {
                    /* we enter a new reference, create a Sequence-struct from what is left... */
                    Sequence seq = Sequence();
                    
                    seq.SEQID   = std::string( current_ref->addr, current_ref->len );
                    seq.defline = std::string( current_ref->addr, current_ref->len );
                    seq.data    = buf.start();
                    seq.length  = buf.len();
                    seq.hadErrors = false;
                    
                    sequences.push_back( seq );
                    
                    StringWhack( current_ref );
                    rc = StringCopy( &current_ref, &rname );
                    if ( rc == 0 )
                    {
                        std::string s( rname.addr, rname.size );
                        buf.new_ref();
                    }
                }
            }
            buf.append( &read );
        }

        if ( rc == 0 )
        {
            rows_processed++;
    
            if ( rows_processed >= c->row_count )
            {
                done = true;
                Sequence seq = Sequence();
                
                seq.SEQID   = std::string( current_ref->addr, current_ref->len );
                seq.defline = std::string( current_ref->addr, current_ref->len );
                seq.data    = buf.start();
                seq.length  = buf.len();
                seq.hadErrors = false;
                
                sequences.push_back( seq );
            }
            else
            {
                /* find the next row ( to skip over empty rows... ) */
                int64_t nxt;
                rc = VCursorFindNextRowIdDirect( c->curs, c->rname_idx, row_id + 1, &nxt );
                if ( rc == 0 )
                    row_id = nxt;
            }
        }
    }

    if ( current_ref != NULL )
        StringWhack( current_ref );

    this->data = buf.buffer;

    return rc;
}

std::map< std::string, unsigned > FastaFile::makeIndex() const
{
    std::map< std::string, unsigned > rslt;
    for ( unsigned i = 0; i < sequences.size(); ++i )
    {
        rslt.insert( std::make_pair( sequences[ i ].SEQID, i ) );
    }
    return rslt;
}
