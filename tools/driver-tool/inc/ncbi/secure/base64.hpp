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

#include <ncbi/secure/payload.hpp>
#include <ncbi/secure/string.hpp>
#include <ncbi/secure/except.hpp>


namespace ncbi
{

    /*=====================================================*
     *                       Base64                        *
     *=====================================================*/

    /**
     * @class Base64
     * @brief Base64 encoding management
     */
    class Base64
    {
    public:
    
        /**
         * @var allow_whitespace
         * @brief constant used to allow embedded whitespace in text
         */
        static const bool allow_whitespace = true;

        /**
         * @var strict_charset
         * @brief constant used to disallow any non-coding characters in text
         */
        static const bool strict_charset = false;

        /**
         * encode
         * @brief apply base64 encoding to binary payload
         * @param payload pointer to start of binary payload to encode
         * @param bytes size in bytes of binary payload to encode
         * @return base64-encoded String
         */
        static const String encode ( const void * payload, size_t bytes );

        /**
         * decode
         * @brief remove base64 encoding from binary payload
         * @param base64 the base64-encoded text
         * @param allow_whitespace if true, allow whitespace in text
         * @return Payload of extracted binary data
         */
        static const Payload decode ( const String & base64, bool allow_whitespace );

        /**
         * decodeText
         * @brief remove base64 encoding where content is expected to be UTF-8 text
         * @param base64 the base64-encoded text
         * @param allow_whitespace if true, allow whitespace in text
         * @return String with extracted text
         */
        static const String decodeText ( const String & base64, bool allow_whitespace );
    
        /**
         * urlEncode
         * @brief apply base64-url encoding to binary payload
         * @param payload pointer to start of binary payload to encode
         * @param bytes size in bytes of binary payload to encode
         * @return base64url-encoded String
         */
        static const String urlEncode ( const void * payload, size_t bytes );

        /**
         * urlDecode
         * @brief remove base64-url encoding from binary payload
         * @param base64url the base64url-encoded text
         * @param allow_whitespace if true, allow whitespace in text
         * @return Payload of extracted binary data
         */
        static const Payload urlDecode ( const String & base64url, bool allow_whitespace );

        /**
         * urlDecodeText
         * @brief remove base64-url encoding where content is expected to be UTF-8 text
         * @param base64url the base64url-encoded text
         * @param allow_whitespace if true, allow whitespace in text
         * @return String with extracted text
         */
        static const String urlDecodeText ( const String & base64url, bool allow_whitespace );

    private:

        Base64 ();
    };
    
    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/

}
