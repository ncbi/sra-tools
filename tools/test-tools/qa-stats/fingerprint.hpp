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
            switch (value[i % m_maxReadSize])
            {   // do not worry about overflows
                case 'A': case 'a': ++(a[i]); break;
                case 'C': case 'c': ++(c[i]); break;
                case 'G': case 'g': ++(g[i]); break;
                case 'T': case 't': ++(t[i]); break;
                default: n[i]++; break;
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
        std::string comma;
        for(size_t i = 0; i < m_maxReadSize; ++i)
        {
            if ( a[i] != 0 )
            {
                ret << comma << R"({"base":"A", "pos":)" << i << R"(, "count":)" << a[i] << "}";
            }
            if ( c[i] != 0 )
            {
                ret << comma << R"({"base":"C", "pos":)" << i << R"(, "count":)" << c[i] << "}";
            }
            if ( g[i] != 0 )
            {
                ret << comma << R"({"base":"G", "pos":)" << i << R"(, "count":)" << g[i] << "}";
            }
            if ( t[i] != 0 )
            {
                ret << comma << R"({"base":"T", "pos":)" << i << R"(, "count":)" << t[i] << "}";
            }
            if ( n[i] != 0 )
            {
                ret << comma << R"({"base":"N", "pos":)" << i << R"(, "count":)" << n[i] << "}";
            }
            if ( ool[i] != 0 )
            {
                ret << comma << R"({"base":"OoL", "pos":)" << i << R"(, "count":)" << ool[i] << "}";
            }
            if ( i == 0 )
            {
                comma = ",";
            }
        }
        ret << "]";
        return ret.str();
    }

private:
    size_t m_maxReadSize;

    // base accumulators:
    std::vector<size_t> a;
    std::vector<size_t> c;
    std::vector<size_t> g;
    std::vector<size_t> t;
    std::vector<size_t> n; // everything that is not AGCT
    std::vector<size_t> ool; // out of read length
};

