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

#include <ncbi/secure/ref.hpp>
#include <ncbi/secure/busy.hpp>
#include <ncbi/secure/except.hpp>

#include <string>
#include <iostream>

/**
 * @file ncbi/secure/string.hpp
 * @brief security-enhanced string classes
 *
 * These classes may be implemented in terms of plain old STL string, that
 * has the benefit of a lot of testing. But the STL interfaces has some
 * properties that can lead to unintentional and potentially exploitable errors
 * that we can compensate for with a more careful interface.
 */

namespace ncbi
{

    /*=====================================================*
     *                       String                        *
     *=====================================================*/
    
    /**
     * @class String
     * @brief an immutable string class with improved security properties over std::string
     *
     * The world's default "character set" (encoding) has been UNICODE
     * for several decades now, and the default format has been UTF-8;
     * a multi-byte representation. This string class treats characters
     * in two ways: as UTF-8 within the sequence and as UTF-32 when
     * referring to individual characters.
     *
     * The treatment of wnords as basic natural language elements has also
     * been commonplace for decades, with support for immutable strings
     * in languages such as Java.
     *
     * This String is semantically defined to contain zero or more
     * UNICODE characters. It cannot be used to contain any other type
     * of data. There is NO assumption that the internal sequence is
     * NUL-terminated.
     */
    class String
    {
    public:

        /*=================================================*
         *                   PREDICATES                    *
         *=================================================*/

        /**
         * isEmpty
         * @return Boolean true if string contains no data
         */
        bool isEmpty () const noexcept;

        /**
         * isAscii
         * @return Boolean true if all contents are ASCII or self is empty
         *
         * ASCII is a proper subset of UTF-8 where each character
         * occupies a single byte. When contents contain even one
         * non-ASCII UTF-8 character, size() will be > count().
         */
        bool isAscii () const noexcept;


        /*=================================================*
         *                   PROPERTIES                    *
         *=================================================*/

        /**
         * size
         * @return the number of BYTES in the UTF-8 string data
         *
         * NB: since the data contained are potentially multi-byte characters,
         * the size in bytes should NEVER be mistakenly used for the count
         * of characters. For the latter, use the count() method.
         */
        size_t size () const noexcept;

        /**
         * count
         * @return the number of CHARACTERS in the String container
         *
         * In the common case where the character count is identical to the
         * byte count, we can know that all characters are single byte, which
         * in UTF-8 encoding means they are all ASCII with 7 significant bits.
         */
        count_t count () const noexcept
        { return cnt; }

        /**
         * length
         * @return the sequence length measured in CHARACTERS
         *
         * The string length refers to the same quantity as the character count,
         * but can be used when referring to the string as a whole.
         */
        count_t length () const noexcept
        { return count (); }


        /*=================================================*
         *                    ACCESSORS                    *
         *=================================================*/

        /**
         * data
         * @return a raw pointer to UTF-8 string data
         *
         * NB: there is NO guarantee that the byte will be
         * NUL-terminated. Callers should make use of the size()
         * method to determine the extent of the memory.
         *
         * Yes, in this particular implementation with std::string
         * underneath, IT'S data() method will return a NUL-terminated
         * string. But a different implementation is free to avoid
         * this.
         */
        const UTF8 * data () const noexcept;

        /**
         * getChar
         * @param idx an ordinal character index into the string
         * @exception IndexOutOfBounds
         * @return a UTF32 character at the given index
         */
        UTF32 getChar ( count_t idx ) const;


        /*=================================================*
         *                   COMPARISON                    *
         *=================================================*/

        /**
         * equal
         * @param s the string being compared
         * @return boolean if the values are identical
         */
        bool equal ( const String & s ) const noexcept;

        /**
         * compare
         * @param s the string being compared
         * @return tri-state value of UNICODE-based lexical ordering
         *
         * A return value < 0 means the self string sorts before "s"
         * A return value > 0 means the self string sorts after "s"
         * A return value of 0 means the string values are identical
         */
        int compare ( const String & s ) const noexcept;

        /**
         * caseInsensitiveCompare
         * @brief similar to compare() but ignores case for alphabetic characters
         * @param s the string being compared
         * @return tri-state value of UNICODE-based lexical ordering
         *
         * A return value < 0 means the self string sorts before "s"
         * A return value > 0 means the self string sorts after "s"
         * A return value of 0 means the string values are identical
         */
        int caseInsensitiveCompare ( const String & s ) const noexcept;


        /*=================================================*
         *                     SEARCH                      *
         *=================================================*/

        /**
         * find
         * @overload find a sub-string
         * @param sub the sub-string being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t find ( const String & sub, count_t left = 0, count_t len = npos ) const noexcept;

        /**
         * find
         * @overload find a sub-string
         * @param zstr a pointer to a C++ NUL-terminated string representing the sub-string being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         *
         * This overloaded method is intended to support manifest constants
         */
        count_t find ( const UTF8 * zstr, count_t left = 0, count_t len = npos ) const;

        /**
         * find
         * @overload find a single character
         * @param ch a UNICODE character
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t find ( UTF32 ch, count_t left = 0, count_t len = npos ) const noexcept;

        /**
         * rfind
         * @overload find a sub-string with reverse search
         * @param sub the sub-string being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t rfind ( const String & sub, count_t xright = npos, count_t len = npos ) const noexcept;

        /**
         * rfind
         * @overload find a sub-string with reverse search
         * @param zstr a pointer to a C++ NUL-terminated substring being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         *
         * This overloaded method is intended to support manifest constants
         */
        count_t rfind ( const UTF8 * zstr, count_t xright = npos, count_t len = npos ) const;

        /**
         * rfind
         * @overload find a single character with reverse search
         * @param ch a UNICODE character
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t rfind ( UTF32 ch, count_t xright = npos, count_t len = npos ) const noexcept;

        /**
         * findFirstOf
         * @overload find the first occurrence of any character in a set, stored as a String
         * @param cset the set of characters being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findFirstOf ( const String & cset, count_t left = 0, count_t len = npos ) const noexcept;

        /**
         * findFirstOf
         * @overload find the first occurrence of any character in a set, stored as a C++ NUL-terminated string.
         * @param zcset a pointer to a C++ NUL-terminated string representing the set
         * @param cset the set of characters being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findFirstOf ( const UTF8 * zcset, count_t left = 0, count_t len = npos ) const;

        /**
         * findLastOf
         * @overload find the first occurrence of any character in a set, stored as a String
         * @param cset the set of characters being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findLastOf ( const String & cset, count_t xright = npos, count_t len = npos ) const noexcept;

        /**
         * findLastOf
         * @overload find the first occurrence of any character in a set, stored as a C++ NUL-terminated string.
         * @param zcset a pointer to a C++ NUL-terminated string representing the set
         * @param cset the set of characters being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findLastOf ( const UTF8 * zcset, count_t xright = npos, count_t len = npos ) const;

        /**
         * beginsWith
         * @overload test whether self String begins with sub String
         * @param sub String with subsequence in question
         * @return Boolean true if self begins with "sub"
         */
        bool beginsWith ( const String & sub ) const noexcept;

        /**
         * beginsWith
         * @overload test whether self String begins with sub UTF8 *
         * @param sub const UTF8 * with subsequence in question
         * @return Boolean true if self begins with "sub"
         */
        bool beginsWith ( const UTF8 * sub ) const;

        /**
         * beginsWith
         * @overload test whether self String begins with character
         * @param ch UTF32 with character in question
         * @return Boolean true if self begins with "ch"
         */
        bool beginsWith ( UTF32 ch ) const;

        /**
         * endsWith
         * @overload test whether self String ends with sub String
         * @param sub String with subsequence in question
         * @return Boolean true if self ends with "sub"
         */
        bool endsWith ( const String & sub ) const noexcept;

        /**
         * endsWith
         * @overload test whether self String ends with sub UTF8 *
         * @param sub const UTF8 * with subsequence in question
         * @return Boolean true if self ends with "sub"
         */
        bool endsWith ( const UTF8 * sub ) const;

        /**
         * endsWith
         * @overload test whether self String ends with character
         * @param ch UTF32 with character in question
         * @return Boolean true if self ends with "ch"
         */
        bool endsWith ( UTF32 ch ) const;


        /*=================================================*
         *               STRING GENERATION                 *
         *=================================================*/

        /**
         * subString
         * @brief create a sub-string of self
         * @param pos an index of the first character to include in sub-string
         * @param len the length of the sub-string
         * @return a new String with length() <= self length()
         */
        String subString ( count_t pos = 0, count_t len = npos ) const;

        /**
         * concat
         * @overload concatenate self and provided String into a new String
         * @param s String to be concatenated to self
         * @return a new String
         */
        String concat ( const String & s ) const;

        /**
         * concat
         * @overload concatenate self and provided C++ string into a new String
         * @param zstr a NUL-terminated C++ string
         * @return a new String
         */
        String concat ( const UTF8 * zstr ) const;

        /**
         * concat
         * @overload concatenate self and provided character into a new String
         * @param ch a UTF-32 UNICODE character
         * @return a new String
         */
        String concat ( UTF32 ch ) const;

        /**
         * toupper
         * @return upper-cased copy of String
         */
        String toupper () const;

        /**
         * tolower
         * @return lower-cased copy of String
         */
        String tolower () const;


        /*=================================================*
         *                     WIPING                      *
         *=================================================*/

        struct Wiper
        {
            void append ( const Wiper & str );

            Wiper ();
            explicit Wiper ( bool wipe );
            Wiper ( const UTF8 * zstr, bool wipe );
            Wiper ( const UTF8 * str, size_t bytes, bool wipe );
            Wiper ( const std :: string & str, bool wipe );
            Wiper ( const Wiper & str, size_t pos, size_t bytes );
            ~ Wiper ();

            std :: string s;
            bool wipe;
        };

        /**
         * clear
         * @brief reinitialize to empty state
         * @param wipe overwrite existing contents if true
         */
        void clear ( bool wipe = false ) noexcept;


        /*=================================================*
         *                   TYPE CASTS                    *
         *=================================================*/

        /**
v         * toSTLString
         * @return a std::string object
         *
         * This cast operator does NOT return a const C++ reference
         * for two reasons: first, the underlying implementation
         * may not use std::string, and second, the C++ spec
         * allows mutations to a const std::string.
         */
        std :: string toSTLString () const;


        /*=================================================*
         *                    ITERATION                    *
         *=================================================*/

        /**
         * @class Iterator
         * @brief an abstraction for examining a UTF-8 sequence
         *
         * Iterators are inherently unsafe without the use of higher-level
         * memory management. In particular, it is possible albeit
         * unusual and unlikely that an Iterator can survive its source.
         *
         * However, efficient access to UTF-8 characters is made difficult
         * by virtue of the fact that it is a multi-byte format, where
         * character size is dynamically determined. An iteration
         * paradigm makes walking a character sequence more natural
         * than sequentially addressing with a monotonically incremented
         * integer index.
         */
        class Iterator
        {
        public:

            /**
             * isValid
             * @return Boolean true if Iterator index is within bounds
             */
            bool isValid () const noexcept;

            /**
             * isAscii
             * @return Boolean true if all contents are ASCII or self is empty
             *
             * ASCII is a proper subset of UTF-8 where each character
             * occupies a single byte. When contents contain even one
             * non-ASCII UTF-8 character, size() will be > count().
             */
            bool isAscii () const noexcept;

            /**
             * charIndex
             * @return the current character index
             */
            long int charIndex () const noexcept
            { return idx; }

            /**
             * byteOffset
             * @return the current byte offset to character
             */
            long int byteOffset () const noexcept
            { return off; }

            /**
             * get
             * @brief retrieve character at current position
             * @exception IndexOutOfBounds
             * @return UTF-32 UNICODE character
             */
            UTF32 get () const;

            /**
             * find
             * @overload find a String from the current position forward
             * @param sub the substring to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             */
            bool find ( const String & sub, count_t len = String :: npos );

            /**
             * find
             * @overload find a String from the current position forward
             * @param sub the substring to locate
             * @param xend is the exclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             *
             * NB - "xend" indicates the exclusive stopping point of a
             * forward search, and must therefor point to a successive
             * character for any potential of success.
             */
            bool find ( const String & sub, const Iterator & xend );

            /**
             * find
             * @overload find a String from the current position forward
             * @param zstr a C++ NUL-terminated substring to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             */
            bool find ( const UTF8 * zstr, count_t len = String :: npos );

            /**
             * find
             * @overload find a String from the current position forward
             * @param zstr a C++ NUL-terminated substring to locate
             * @param xend is the exclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             *
             * NB - "xend" indicates the exclusive stopping point of a
             * forward search, and must therefor point to a successive
             * character for any potential of success.
             */
            bool find ( const UTF8 * zstr, const Iterator & xend );

            /**
             * find
             * @overload find a String from the current position forward
             * @param ch a UTF-32 UNICODE character to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find a character and position the iterator
             * to point to it. If not found, the iterator remains unchanged.
             */
            bool find ( UTF32 ch, count_t len = String :: npos );

            /**
             * find
             * @overload find a String from the current position forward
             * @param ch a UTF-32 UNICODE character to locate
             * @param xend is the exclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find a character and position the iterator
             * to point to it. If not found, the iterator remains unchanged.
             *
             * NB - "xend" indicates the exclusive stopping point of a
             * forward search, and must therefor point to a successive
             * character for any potential of success.
             */
            bool find ( UTF32 ch, const Iterator & xend );

            /**
             * rfind
             * @overload find a String from the previous position backward
             * @param sub the substring to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             *
             * NB - "len" indicates the distance backward within the String,
             * and the current character pointed to by self is excluded from
             * the search.
             */
            bool rfind ( const String & sub, count_t len = String :: npos );

            /**
             * rfind
             * @overload find a String from the previous position backward
             * @param sub the substring to locate
             * @param end is the inclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             *
             * NB - "end" indicates the inclusive stopping point of a
             * backward search, and must therefor point to a prior character
             * for any potential of success.
             */
            bool rfind ( const String & sub, const Iterator & end );

            /**
             * rfind
             * @overload find a String from the previous position backward
             * @param zstr a C++ NUL-terminated substring to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             *
             * NB - "len" indicates the distance backward within the String,
             * and the current character pointed to by self is excluded from
             * the search.
             */
            bool rfind ( const UTF8 * zstr, count_t len = String :: npos );

            /**
             * rfind
             * @overload find a String from the previous position backward
             * @param zstr a C++ NUL-terminated substring to locate
             * @param end is the inclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find a substring and position the iterator
             * to point to its first character. If the substring is not found,
             * the iterator remains unchanged.
             *
             * NB - "end" indicates the inclusive stopping point of a
             * backward search, and must therefor point to a prior character
             * for any potential of success.
             */
            bool rfind ( const UTF8 * zstr, const Iterator & end );

            /**
             * rfind
             * @overload find a String from the previous position backward
             * @param ch a UTF-32 UNICODE character to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find a character and position the iterator
             * to point to it. If not found, the iterator remains unchanged.
             *
             * NB - "len" indicates the distance backward within the String,
             * and the current character pointed to by self is excluded from
             * the search.
             */
            bool rfind ( UTF32 ch, count_t len = String :: npos );

            /**
             * rfind
             * @overload find a String from the previous position backward
             * @param ch a UTF-32 UNICODE character to locate
             * @param end is the inclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find a character and position the iterator
             * to point to it. If not found, the iterator remains unchanged.
             *
             * NB - "end" indicates the inclusive stopping point of a
             * backward search, and must therefor point to a prior character
             * for any potential of success.
             */
            bool rfind ( UTF32 ch, const Iterator & end );

            /**
             * findFirstOf
             * @overload find a String from the current position forward
             * @param cset the set of characters to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find any character within "cset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             */
            bool findFirstOf ( const String & cset, count_t len = String :: npos );

            /**
             * findFirstOf
             * @overload find a String from the current position forward
             * @param cset the set of characters to locate
             * @param xend is the exclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find any character within "cset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             *
             * NB - "end" indicates the inclusive stopping point of a
             * backward search, and must therefor point to a prior character
             * for any potential of success.
             */
            bool findFirstOf ( const String & cset, const Iterator & xend );

            /**
             * findFirstOf
             * @overload find a String from the current position forward
             * @param zcset a C++ NUL-terminated set of characters to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find any character within "zcset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             */
            bool findFirstOf ( const UTF8 * zcset, count_t len = String :: npos );

            /**
             * findFirstOf
             * @overload find a String from the current position forward
             * @param zcset a C++ NUL-terminated set of characters to locate
             * @param xend is the exclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find any character within "zcset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             *
             * NB - "end" indicates the inclusive stopping point of a
             * backward search, and must therefor point to a prior character
             * for any potential of success.
             */
            bool findFirstOf ( const UTF8 * zcset, const Iterator & xend );

            /**
             * findLastOf
             * @overload find a String from the previous position backward
             * @param cset the set of characters to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find any character within "cset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             */
            bool findLastOf ( const String & cset, count_t len = String :: npos );

            /**
             * findLastOf
             * @overload find a String from the previous position backward
             * @param cset the set of characters to locate
             * @param end is the inclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find any character within "cset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             *
             * NB - "xend" indicates the exclusive stopping point of a
             * forward search, and must therefor point to a successive
             * character for any potential of success.
             */
            bool findLastOf ( const String & cset, const Iterator & end );

            /**
             * findLastOf
             * @overload find a String from the previous position backward
             * @param zcset a C++ NUL-terminated set of characters to locate
             * @param len is the size of the region of search in characters
             * @return Boolean true if found
             *
             * The idea is to find any character within "zcset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             */
            bool findLastOf ( const UTF8 * zcset, count_t len = String :: npos );

            /**
             * findLastOf
             * @overload find a String from the previous position backward
             * @param zcset a C++ NUL-terminated set of characters to locate
             * @param end is the inclusive endpoint of the region to be searched
             * @return Boolean true if found
             *
             * The idea is to find any character within "zcset" and position the
             * iterator to point to its first character. If the substring is not
             * found, the iterator remains unchanged.
             *
             * NB - "xend" indicates the exclusive stopping point of a
             * forward search, and must therefor point to a successive
             * character for any potential of success.
             */
            bool findLastOf ( const UTF8 * zcset, const Iterator & end );

            /**
             * operator++
             * @brief a pre-increment operator
             * @exception IntegerOverflow
             * @return C++ reference to self
             */
            Iterator & operator ++ ();

            /**
             * operator--
             * @brief a pre-decrement operator
             * @exception IntegerUnderflow
             * @return C++ reference to self
             */
            Iterator & operator -- ();

            /**
             * operator*
             * @brief dereference operator
             * @exception IndexOutOfBounds
             * @return UTF-32 UNICODE character
             */
            UTF32 operator * () const
            { return get (); }

            /**
             * operator==
             * @brief compare two Iterators for equality
             *
             * Iterators are considered equal iff they refer to the
             * same String object and have identical index values.
             */
            bool operator == ( const Iterator & it ) const noexcept;

            /**
             * operator!=
             * @brief compare two Iterators for inequality
             *
             * Iterators are considered unequal if they refer to
             * different String objects or have different index values.
             */
            bool operator != ( const Iterator & it ) const noexcept;

            /**
             * operator-
             * @brief returns the difference in characters
             * @param it another Iterator on the same String
             * @return a long int with the difference in character positions
             */
            long int operator - ( const Iterator & it ) const;

            /**
             * operator+=
             * @brief offset an iterator
             * @param adjust a signed adjustment
             * @return a reference to self
             */
            Iterator & operator += ( long int adjust );

            /**
             * operator-=
             * @brief offset an iterator
             * @param adjust a signed adjustment
             * @return a reference to self
             */
            Iterator & operator -= ( long int adjust );

            /**
             * operator=
             * @overload assignment copy operator
             * @param it the Iterator to be copied
             * @return a C++ reference to self
             */
            Iterator & operator = ( const Iterator & it );

            /**
             * Iterator
             * @overload copy constructor
             * @param it the Iterator to be copied
             */
            Iterator ( const Iterator & it );

            /**
             * operator=
             * @overload assignment move operator
             * @param it the Iterator to be copied
             * @return a C++ reference to self
             */
            Iterator & operator = ( const Iterator && it );

            /**
             * Iterator
             * @overload move constructor
             * @param it the Iterator to be copied
             */
            Iterator ( const Iterator && it );

            /**
             * Iterator
             * @overload default constructor
             */
            Iterator ();

            /**
             * ~Iterator
             * @brief disconnect from String
             */
            ~ Iterator ();

        private:

            Iterator ( const SRef < Wiper > & str, count_t cnt, size_t origin, count_t index );

            //!< retain pointer to String
            SRef < Wiper > str;
            count_t cnt;

            //!< character index into the String sequence
            long int idx;

            //!< byte-offset into the String sequence
            long int off;

            friend class String;
        };

        /**
         * makeIterator
         * @return an Iterator to an initial point within String
         *
         * Since Iterators allow both forward and backward movement,
         * and can be created at any arbitrary point in the string,
         * the traditional C++ "begin" and "end" concepts do not apply.
         */
        Iterator makeIterator ( count_t origin = 0 ) const noexcept;


        /*=================================================*
         *           C++ OPERATOR OVERLOADS                *
         *=================================================*/

        /**
         * operator[]
         * @param idx an ordinal character index into the string
         * @exception IndexOutOfBounds
         * @return a UTF32 character at the given index
         */
        UTF32 operator [] ( count_t idx ) const
        { return getChar ( idx ); }


        bool operator < ( const String & s ) const noexcept
        { return compare ( s ) < 0; }
        bool operator <= ( const String & s ) const noexcept
        { return compare ( s ) <= 0; }
        bool operator == ( const String & s ) const noexcept
        { return equal ( s ); }
        bool operator != ( const String & s ) const noexcept
        { return ! equal ( s ); }
        bool operator >= ( const String & s ) const noexcept
        { return compare ( s ) >= 0; }
        bool operator > ( const String & s ) const noexcept
        { return compare ( s ) > 0; }

        bool operator < ( const UTF8 * s ) const
        { return compare ( s ) < 0; }
        bool operator <= ( const UTF8 * s ) const
        { return compare ( s ) <= 0; }
        bool operator == ( const UTF8 * s ) const
        { return equal ( s ); }
        bool operator != ( const UTF8 * s ) const
        { return ! equal ( s ); }
        bool operator >= ( const UTF8 * s ) const
        { return compare ( s ) >= 0; }
        bool operator > ( const UTF8 * s ) const
        { return compare ( s ) > 0; }

        String operator + ( const String & s ) const
        { return concat ( s ); }
        String operator + ( const UTF8 * zstr ) const
        { return concat ( zstr ); }
        String operator + ( UTF32 ch ) const
        { return concat ( ch ); }


        //!< create a copy of the source string
        String & operator = ( const String & s );

        //!< create a copy of the source string
        String ( const String & s );

        //!< move source string
        String & operator = ( const String && s );

        //!< steal contents of source string
        String ( const String && s );

        //!< create an empty string
        String ();

        //!< initialize from a single character
        explicit String ( UTF32 ch );

        //!< initialize from a NUL-terminated manifest constant
        String ( const UTF8 * zstr );

        //!< initialize from a buffer of UTF-8 data
        String ( const UTF8 * str, size_t bytes );

        //!< initialize from a NUL-terminated UTF-16 string
        explicit String ( const UTF16 * zstr );

        //!< initialize from a buffer of UTF-16 data
        String ( const UTF16 * str, count_t length );

        //!< initialize from a NUL-terminated UTF-32 string
        explicit String ( const UTF32 * zstr );

        //!< initialize from a buffer of UTF-32 data
        String ( const UTF32 * str, count_t length );

        //!< initialize from a C++ STL string
        explicit String ( const std :: string & str );

        //!< reclaim string data
        ~ String ();

        static const count_t npos = -1;

    private:

        count_t unicode ( const UTF8 * utf8, count_t elem_count );
        count_t unicode ( const UTF16 * utf16, count_t elem_count );
        count_t unicode ( const UTF32 * utf32, count_t elem_count );
        count_t normalize ( UTF32 * utf32, count_t char_count );

    protected:

        //!< base implementation on struct holding std :: string
        SRef < Wiper > str;

        //!< the number of characters (not bytes) in string
        count_t cnt;

        friend class StringBuffer;
    };


    /*=====================================================*
     *                 NULTerminatedString                 *
     *=====================================================*/
    
    /**
     * @class NULTerminatedString
     * @brief an immutable string class that guarantees upon construction a NUL terminator
     *
     * As mentioned in the String class, this ends up being guaranteed
     * by C++11 std::string in the current implementation, but may not
     * always use std::string.
     */
    class NULTerminatedString : public String
    {
    public:

        /*=================================================*
         *                    ACCESSORS                    *
         *=================================================*/

        /**
         * c_str
         * @return a raw pointer to a NUL-terminated C++ string
         */
        const UTF8 * c_str () const noexcept;

        /**
         * operator=
         * @overload assignment operator from a String
         *
         * creates an object that guarantees NUL termination
         */
        NULTerminatedString & operator = ( const String & s );

        /**
         * NULTerminatedString
         * @overload constructor from a String
         *
         * guarantees NUL termination
         */
        explicit NULTerminatedString ( const String & s );

        /**
         * operator=
         * @overload copy operator for a NUL terminated string
         */
        NULTerminatedString & operator = ( const NULTerminatedString & zs );

        /**
         * NULTerminatedString
         * @overload copy constructor for a NUL terminated string
         */
        NULTerminatedString ( const NULTerminatedString & zs );

        /**
         * operator=
         * @overload move operator for a NUL terminated string
         */
        NULTerminatedString & operator = ( const NULTerminatedString && zs );

        /**
         * NULTerminatedString
         * @overload move constructor for a NUL terminated string
         */
        NULTerminatedString ( const NULTerminatedString && zs );

        /**
         * ~NULTerminatedString
         * @brief serves no purpose
         */
        ~ NULTerminatedString () {}
    };


    /*=====================================================*
     *                    StringBuffer                     *
     *=====================================================*/
    
    /**
     * @class StringBuffer
     * @brief a mutable string class for performing text manipulation
     */
    class StringBuffer
    {
    public:


        /*=================================================*
         *                   PREDICATES                    *
         *=================================================*/

        /**
         * isEmpty
         * @return Boolean true if string contains no data
         */
        bool isEmpty () const;

        /**
         * isAscii
         * @return Boolean true if all contents are ASCII
         *
         * ASCII is a proper subset of UTF-8 where each character
         * occupies a single byte.
         */
        bool isAscii () const;


        /*=================================================*
         *                   PROPERTIES                    *
         *=================================================*/

        /**
         * size
         * @return the number of BYTES in the UTF-8 string data
         *
         * NB: since the data contained are potentially multi-byte characters,
         * the size in bytes should never be mistakenly used for the count
         * of characters. For the latter, use the count() method.
         */
        size_t size () const;

        /**
         * capacity
         * @return the number of BYTES in the UTF-8 string buffer
         */
        size_t capacity () const;

        /**
         * count
         * @return the number of CHARACTERS in the String container
         *
         * In the common case where the character count is identical to the
         * byte count, we can know that all characters are single byte, which
         * in UTF-8 encoding means they are all ASCII with 7 significant bits.
         */
        count_t count () const;

        /**
         * length
         * @return the sequence length measured in CHARACTERS
         *
         * The string length refers to the same quantity as the character count.
         */
        count_t length () const;


        /*=================================================*
         *                    ACCESSORS                    *
         *=================================================*/

        /**
         * data
         * @return a raw pointer to UTF-8 string data
         *
         * NB: there is NO guarantee that the byte will be
         * NUL-terminated. Callers should make use of the size()
         * method to determine the extent of the memory.
         */
        const UTF8 * data () const;

        /**
         * getChar
         * @param idx an ordinal character index into the string
         * @exception IndexOutOfBounds
         * @return a UTF32 character at the given index
         */
        UTF32 getChar ( count_t idx ) const;


        /*=================================================*
         *                   COMPARISON                    *
         *=================================================*/

        /**
         * equal
         * @param s the string being compared
         * @return boolean if the values are identical
         */
        bool equal ( const String & s ) const;

        /**
         * equal
         * @param sb the string being compared
         * @return boolean if the values are identical
         */
        bool equal ( const StringBuffer & sb ) const;

        /**
         * compare
         * @param s the string being compared
         * @return tri-state value of UNICODE-based lexical ordering
         *
         * A return value < 0 means the self string sorts before "s"
         * A return value > 0 means the self string sorts after "s"
         * A return value of 0 means the string values are identical
         */
        int compare ( const String & s ) const;

        /**
         * compare
         * @param sb the string being compared
         * @return tri-state value of UNICODE-based lexical ordering
         *
         * A return value < 0 means the self string sorts before "s"
         * A return value > 0 means the self string sorts after "s"
         * A return value of 0 means the string values are identical
         */
        int compare ( const StringBuffer & sb ) const;

        /**
         * caseInsensitiveCompare
         * @brief similar to compare() but ignores case for alphabetic characters
         * @param s the string being compared
         * @return tri-state value of UNICODE-based lexical ordering
         *
         * A return value < 0 means the self string sorts before "s"
         * A return value > 0 means the self string sorts after "s"
         * A return value of 0 means the string values are identical
         */
        int caseInsensitiveCompare ( const String & s ) const;

        /**
         * caseInsensitiveCompare
         * @brief similar to compare() but ignores case for alphabetic characters
         * @param sb the string being compared
         * @return tri-state value of UNICODE-based lexical ordering
         *
         * A return value < 0 means the self string sorts before "s"
         * A return value > 0 means the self string sorts after "s"
         * A return value of 0 means the string values are identical
         */
        int caseInsensitiveCompare ( const StringBuffer & sb ) const;


        /*=================================================*
         *                     SEARCH                      *
         *=================================================*/

        /**
         * find
         * @overload find a sub-string
         * @param sub the sub-string being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t find ( const String & sub, count_t left = 0, count_t len = String :: npos ) const;

        /**
         * find
         * @overload find a sub-string
         * @param zstr a pointer to a C++ NUL-terminated string representing the sub-string being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         *
         * This overloaded method is intended to support manifest constants
         */
        count_t find ( const UTF8 * zstr, count_t left = 0, count_t len = String :: npos ) const;

        /**
         * find
         * @overload find a single character
         * @param ch a UNICODE character
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t find ( UTF32 ch, count_t left = 0, count_t len = String :: npos ) const;

        /**
         * rfind
         * @overload find a sub-string with reverse search
         * @param sub the sub-string being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t rfind ( const String & sub, count_t xright = String :: npos, count_t len = String :: npos ) const;

        /**
         * rfind
         * @overload find a sub-string with reverse search
         * @param zstr a pointer to a C++ NUL-terminated string representing the sub-string being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         *
         * This overloaded method is intended to support manifest constants
         */
        count_t rfind ( const UTF8 * zstr, count_t xright = String :: npos, count_t len = String :: npos ) const;

        /**
         * rfind
         * @overload find a single character with reverse search
         * @param ch a UNICODE character
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first character of matched sub-string or npos if not found
         */
        count_t rfind ( UTF32 ch, count_t xright = String :: npos, count_t len = String :: npos ) const;

        /**
         * findFirstOf
         * @overload find the first occurrence of any character in a set, stored as a String
         * @param cset the set of characters being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findFirstOf ( const String & cset, count_t left = 0, count_t len = String :: npos ) const;

        /**
         * findFirstOf
         * @overload find the first occurrence of any character in a set, stored as a C++ NUL-terminated string.
         * @param zcset a pointer to a C++ NUL-terminated string representing the set
         * @param cset the set of characters being sought
         * @param left the left index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findFirstOf ( const UTF8 * zcset, count_t left = 0, count_t len = String :: npos ) const;

        /**
         * findLastOf
         * @overload find the first occurrence of any character in a set, stored as a String
         * @param cset the set of characters being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findLastOf ( const String & cset, count_t xright = String :: npos, count_t len = String :: npos ) const;

        /**
         * findLastOf
         * @overload find the first occurrence of any character in a set, stored as a C++ NUL-terminated string.
         * @param zcset a pointer to a C++ NUL-terminated string representing the set
         * @param cset the set of characters being sought
         * @param xright the exclusive right index of substring to search
         * @param len the length of substring to search
         * @return the index of the first matched character or npos if not found
         */
        count_t findLastOf ( const UTF8 * zcset, count_t xright = String :: npos, count_t len = String :: npos ) const;

        /**
         * beginsWith
         * @overload test whether self String begins with sub String
         * @param sub String with subsequence in question
         * @return Boolean true if self begins with "sub"
         */
        bool beginsWith ( const String & sub ) const;

        /**
         * beginsWith
         * @overload test whether self String begins with sub UTF8 *
         * @param sub const UTF8 * with subsequence in question
         * @return Boolean true if self begins with "sub"
         */
        bool beginsWith ( const UTF8 * sub ) const;

        /**
         * beginsWith
         * @overload test whether self String begins with character
         * @param ch UTF32 with character in question
         * @return Boolean true if self begins with "ch"
         */
        bool beginsWith ( UTF32 ch ) const;

        /**
         * endsWith
         * @overload test whether self String ends with sub String
         * @param sub String with subsequence in question
         * @return Boolean true if self ends with "sub"
         */
        bool endsWith ( const String & sub ) const;

        /**
         * endsWith
         * @overload test whether self String ends with sub UTF8 *
         * @param sub const UTF8 * with subsequence in question
         * @return Boolean true if self ends with "sub"
         */
        bool endsWith ( const UTF8 * sub ) const;

        /**
         * endsWith
         * @overload test whether self String ends with character
         * @param ch UTF32 with character in question
         * @return Boolean true if self ends with "ch"
         */
        bool endsWith ( UTF32 ch ) const;


        /*=================================================*
         *                     UPDATE                      *
         *=================================================*/

        /**
         * append
         * @overload append provided String onto self
         * @param s String to be appended to self
         * @return C++ reference to self
         */
        StringBuffer & append ( const String & s );

        /**
         * append
         * @overload append provided String onto self
         * @param sb StringBuffer to be appended to self
         * @return C++ reference to self
         */
        StringBuffer & append ( const StringBuffer & sb );

        /**
         * append
         * @overload append provided C++ string onto self
         * @param zstr a NUL-terminated C++ string
         * @return C++ reference to self
         */
        StringBuffer & append ( const UTF8 * zstr );

        /**
         * append
         * @overload append provided character onto self
         * @param ch a UTF-32 UNICODE character
         * @return C++ reference to self
         */
        StringBuffer & append ( UTF32 ch );

        /**
         * toupper
         * @brief upper-case contents
         * @return C++ reference to self
         */
        StringBuffer & toupper ();

        /**
         * tolower
         * @brief upper-case contents
         * @return C++ reference to self
         */
        StringBuffer & tolower ();

        /**
         * clear
         * @brief reset internal state to empty
         * @param wipe overwrite existing contents if true
         */
        void clear ( bool wipe = false );


        /*=================================================*
         *               STRING GENERATION                 *
         *=================================================*/

        /**
         * toString
         * @brief create an immutable String from internal data
         * @return a new String
         */
        String toString () const;

        /**
         * stealString
         * @brief create an immutable String from internal data and leave contents empty
         * @return a new String
         */
        String stealString ();


        /*=================================================*
         *           C++ OPERATOR OVERLOADS                *
         *=================================================*/

        /**
         * operator[]
         * @param idx an ordinal character index into the string
         * @exception IndexOutOfBounds
         * @return a UTF32 character at the given index
         */
        UTF32 operator [] ( count_t idx ) const
        { return getChar ( idx ); }


        bool operator < ( const String & s ) const
        { return compare ( s ) < 0; }
        bool operator <= ( const String & s ) const
        { return compare ( s ) <= 0; }
        bool operator == ( const String & s ) const
        { return equal ( s ); }
        bool operator != ( const String & s ) const
        { return ! equal ( s ); }
        bool operator >= ( const String & s ) const
        { return compare ( s ) >= 0; }
        bool operator > ( const String & s ) const
        { return compare ( s ) > 0; }

        bool operator < ( const UTF8 * s ) const
        { return compare ( s ) < 0; }
        bool operator <= ( const UTF8 * s ) const
        { return compare ( s ) <= 0; }
        bool operator == ( const UTF8 * s ) const
        { return equal ( s ); }
        bool operator != ( const UTF8 * s ) const
        { return ! equal ( s ); }
        bool operator >= ( const UTF8 * s ) const
        { return compare ( s ) >= 0; }
        bool operator > ( const UTF8 * s ) const
        { return compare ( s ) > 0; }

        StringBuffer & operator += ( const StringBuffer & sb )
        { return append ( sb . str ); }
        StringBuffer & operator += ( const String & s )
        { return append ( s ); }
        StringBuffer & operator += ( const UTF8 * zstr )
        { return append ( zstr ); }
        StringBuffer & operator += ( UTF32 ch )
        { return append ( ch ); }


        //!< create a copy of the source
        StringBuffer & operator = ( const StringBuffer & s );

        //!< move the source
        StringBuffer & operator = ( const StringBuffer && s );

        //!< create a copy of the source string
        StringBuffer ( const StringBuffer & s );

        //!< move the source string
        StringBuffer ( const StringBuffer && s );

        //!< create an empty buffer
        StringBuffer ();

        //!< reclaim string data
        ~ StringBuffer ();

    private:

        String str;
        BusyLock busy;
    };


    /*=================================================*
     *             FUNCTIONS AND OPERATORS             *
     *=================================================*/

    /**
     * decToLongLongInteger
     * @return a signed long long integer
     *
     * throws exception if "s" was not 100% Integer numeral.
     */
    long long int decToLongLongInteger ( const String & s );

    /**
     * operator<<
     * @overload throws a String at an Exception
     */
    inline XP & operator << ( XP & xp, const String & s )
    { xp . putUTF8 ( s . data (), s . size () ); return xp; }

    /**
     * operator<<
     * @overload throws a String at a std :: ostream
     */
    inline std :: ostream & operator << ( std :: ostream & os, const String & s )
    { os . write ( s . data (), s . size () ); return os; }

    /**
     * operator<<
     * @overload throws a String :: Iterator at an Exception
     */
    inline XP & operator << ( XP & xp, const String :: Iterator & it )
    {
        if ( ! it . isValid () )
            xp << "INVALID";
        else
            xp . putChar ( * it );
        return xp;
    }

    /**
     * operator<<
     * @overload throws a String :: Iterator at a std :: ostream
     */
    inline std :: ostream & operator << ( std :: ostream & os, const String :: Iterator & it )
    {
        if ( ! it . isValid () )
            os << "INVALID";
        else if ( it . isAscii () )
            os << ( char ) * it;
        else
            os << String ( * it );
        return os;
    }

    /**
     * operator<<
     * @overload throws a StringBuffer at an Exception
     */
    inline XP & operator << ( XP & xp, const StringBuffer & sb )
    { xp . putUTF8 ( sb . data (), sb . size () ); return xp; }

    /**
     * operator<<
     * @overload throws a StringBuffer at a std :: ostream
     */
    inline std :: ostream & operator << ( std :: ostream & os, const StringBuffer & sb )
    { os . write ( sb . data (), sb . size () ); return os; }


    /*=====================================================*
     *                     EXCEPTIONS                      *
     *=====================================================*/

    DECLARE_SEC_MSG_EXCEPTION ( InvalidUTF8String, InvalidArgument );
    DECLARE_SEC_MSG_EXCEPTION ( InvalidUTF16String, InvalidArgument );
    DECLARE_SEC_MSG_EXCEPTION ( InvalidUTF32Character, InvalidArgument );
    DECLARE_SEC_MSG_EXCEPTION ( InvalidNumeral, InvalidArgument );
    DECLARE_SEC_MSG_EXCEPTION ( TooManyIterators, ConstraintViolation );
    DECLARE_SEC_MSG_EXCEPTION ( IteratorsDiffer, ConstraintViolation );
    DECLARE_SEC_MSG_EXCEPTION ( InvalidIterator, InvalidArgument );
}
