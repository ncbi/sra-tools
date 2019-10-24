/*==============================================================================
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
 *  Author: Kurt Rodarmer
 *
 * ===========================================================================
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>

/**
 * @file ncbi/secure/types.hpp
 * @brief basic types
 */

namespace ncbi
{
    typedef  int8_t   I8;
    typedef  int16_t I16;
    typedef  int32_t I32;
    typedef  int64_t I64;
    
    typedef uint8_t   U8;
    typedef uint16_t U16;
    typedef uint32_t U32;
    typedef uint64_t U64;

    typedef float    F32;
    typedef double   F64;
    
    /**
     * @typedef ASCII
     * @brief a type indicating single-byte characters in ASCII subset of UTF-8 encoding
     */
    typedef char ASCII;

    /**
     * @typedef UTF8
     * @brief a type indicating multi-byte characters in UTF-8 encoding
     */
    typedef char UTF8;

    /**
     * @typedef UTF16
     * @brief a type indicating an integral UNICODE character in UTF-16 encoding
     */
    typedef unsigned short int UTF16;

    /**
     * @typedef UTF32
     * @brief a type indicating an integral UNICODE character in UTF-32 encoding
     */
    typedef unsigned int UTF32;

    /**
     * @typedef count_t
     * @brief an integral quantity representing a count or cardinality
     */
    typedef std :: size_t count_t;

    /**
     * @class String
     * @brief an immutable string class with improved security properties over std::string
     */
    class String;

    /**
     * @define IN
     * @brief optional decorative modifier indicating an input parameter
     */
#define IN

    /**
     * @define OUT
     * @brief optional decorative modifier indicating an output parameter
     */
#define OUT

    /**
     * @define INOUT
     * @brief optional decorative modifier indicating an input/output parameter
     */
#define INOUT

}
