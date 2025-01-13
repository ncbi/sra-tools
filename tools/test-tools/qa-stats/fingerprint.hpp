/* ===========================================================================
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
 * Project:
 *  Loader QA Stats
 *
 * Purpose:
 *  Fingerprinting reads
 */

#include <string>
#include <vector>
#include <sstream>

#include "output.hpp"

class Fingerprint
{
public:
    static const size_t DefaultMaxReadSize = 4096;

public:
    Fingerprint( size_t p_maxReadSize = DefaultMaxReadSize )
    :   m_maxReadSize( p_maxReadSize ),
        a( "A", p_maxReadSize ),
        c( "C", p_maxReadSize ),
        g( "G", p_maxReadSize ),
        t( "T", p_maxReadSize ),
        n( "N", p_maxReadSize ),
        ool( "OoL", p_maxReadSize )
    {
    }

    void update( const std::string & value )
    {
        size_t count = value.length();
        for(size_t i = 0; i < count; ++i)
        {
            size_t idx = i % m_maxReadSize;
            switch ( value[i] )
            {   // do not worry about overflows
                case 'A': case 'a': ++(a.counts[idx]); break;
                case 'C': case 'c': ++(c.counts[idx]); break;
                case 'G': case 'g': ++(g.counts[idx]); break;
                case 'T': case 't': ++(t.counts[idx]); break;
                default:            ++(n.counts[idx]); break;
            }
        }
        if ( count < m_maxReadSize )
        {   // fill out ool
            size_t i = count;
            while ( i < m_maxReadSize )
            {
                ++(ool.counts[i]);
                ++i;
            }
        }
    };

    friend JSON_ostream &operator <<(JSON_ostream &out, Fingerprint const &self)
    {
        out << BaseAccumulator( self.a );
        out << BaseAccumulator( self.c );
        out << BaseAccumulator( self.g );
        out << BaseAccumulator( self.t );
        out << BaseAccumulator( self.n );
        out << BaseAccumulator( self.ool );
        return out;
    }

private:
    struct BaseAccumulator
    {
        BaseAccumulator( std::string p_base, size_t p_reserve )
        : base( p_base ), counts( p_reserve, 0 )
        {
        }
        std::string base;
        std::vector<size_t> counts;
    } ;
    friend JSON_ostream &operator <<(JSON_ostream &out, BaseAccumulator const &self)
    {
        for(size_t i = 0; i < self.counts.size(); ++i)
        {
            // if ( i == 0 )
            //     out << ",";
            out << '{'
                << JSON_Member{"base"} << self.base
                << JSON_Member{"pos"} << i
                << JSON_Member{"count"} << self.counts[i]
            << '}';
        }
        return out;
    }

    size_t m_maxReadSize;

    // base accumulators:
    BaseAccumulator a;
    BaseAccumulator c;
    BaseAccumulator g;
    BaseAccumulator t;
    BaseAccumulator n; // everything that is not AGCT
    BaseAccumulator ool; // out of read length
};

