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

class Fingerprint
{
public:
    static const size_t DefaultMaxReadSize = 4096;

public:
    Fingerprint( size_t p_maxReadSize = DefaultMaxReadSize )
    :   m_maxReadSize( p_maxReadSize ),
        a( p_maxReadSize, 0 ),
        c( p_maxReadSize, 0 ),
        g( p_maxReadSize, 0 ),
        t( p_maxReadSize, 0 ),
        n( p_maxReadSize, 0 ),
        ool( p_maxReadSize, 0 )
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
                case 'A': case 'a': ++(a[idx]); break;
                case 'C': case 'c': ++(c[idx]); break;
                case 'G': case 'g': ++(g[idx]); break;
                case 'T': case 't': ++(t[idx]); break;
                default:            ++(n[idx]); break;
            }
        }
        if ( count < m_maxReadSize )
        {   // fill out ool
            size_t i = count;
            while ( i < m_maxReadSize )
            {
                ++(ool[i]);
                ++i;
            }
        }
    };

    std::string toJson() const
    {
        std::ostringstream ret;
        ret << "[";
        vectorToJson( a, "A", ret ); ret << ",";
        vectorToJson( c, "C", ret ); ret << ",";
        vectorToJson( g, "G", ret ); ret << ",";
        vectorToJson( t, "T", ret ); ret << ",";
        vectorToJson( n, "N", ret ); ret << ",";
        vectorToJson( ool, "OoL", ret );
        ret << "]";
        return ret.str();
    }

private:
    typedef std::vector<size_t> BaseAccumulator;

    void vectorToJson( const BaseAccumulator & v, const char * base, std::ostream & out ) const
    {
        std::string comma;
        for(size_t i = 0; i < m_maxReadSize; ++i)
        {
            out << comma << R"({"base":")" << base << R"(", "pos":)" << i << R"(, "count":)" << v[i] << "}";
            if ( i == 0 )
            {
                comma = ",";
            }
        }
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

