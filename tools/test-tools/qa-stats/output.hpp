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
 *  Format JSON output.
 */

#pragma once

#ifndef output_hpp
#define output_hpp

#include <iosfwd>
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
#include <set>

struct JSON_Member {
    std::string name;
};

class JSON_ostream {
    std::ostream &strm;
    bool ws = true;         ///< was the last character inserted a whitespace character.
    bool newline = true;    ///< Is newline needed before the next item.
    bool comma = false;     ///< Was the character a ','? More importanly, will the next character belong to a new list item (or be the end of list).
    bool instr = false;     ///< In a string, therefore apply string escaping rules to the inserted characters.
    bool esc = false;       ///< In a string and the next character is escaped.
    
    /// A list is anything with components that are separated by ',', i.e. JSON Objects and Arrays
    std::vector<bool> listStack; ///< Records if list is empty. false mean empty. back() is the current list.

    /// Send the character to the output stream. Checks for whitespace.
    void insert_raw(char v);
    
    /// If needed, start a newline and indent it.
    void indentIfNeeded();
    
    /// Insert a character using string escaping rules.
    void insert_instr(char v);
    
    /// Insert a string using string escaping rules.
    void insert_instr(char const *const str);

    /// Insert a string without using any escaping rules.
    void insert_raw(char const *const str);

    // start a new list (array or object)
    void newList(char type);

    // start a new list item
    void listItem();

    // end a list (array or object)
    void endList(char type);

public:
    JSON_ostream(std::ostream &os) : strm(os) {}

    template <typename T>
    /// Insert some value (NB, use for number types)
    JSON_ostream &insert(T v) {
        std::ostringstream oss;

        oss << v;
        if (!ws)
            insert_raw(' ');
        insert_raw(oss.str().c_str());
        return *this;
    }
    
    /// Insert a Boolean value
    JSON_ostream &insert(bool const &v) {
        insert_raw(v ? "true" : "false");
        return *this;
    }

    /// Insert a character.
    ///
    /// When not in string mode, the stream reacts to the character.
    /// * '"' will toggle string mode.
    /// * '{' will add a list level and indent.
    /// * '}' will remove a list level and indent.
    /// * '[' will add a list level and indent.
    /// * ']' will remove a list level and indent.
    /// * ',' will end the current list item.
    ///
    /// There is no validation of syntax. But inserting sequences
    /// of the above characters should still produce valid (if weird) JSON.
    ///
    /// When in string mode, the stream reacts to the character inserted.
    /// * certain characters will be escaped as needed.
    /// * '"' will end string mode unless the previous character was the escape character.
    JSON_ostream &insert(char v);

    /// Insert a whole string of characters.
    /// In string mode, it will append to the current string.
    /// Otherwise, it will insert '"' (thus entering string mode),
    /// append the string, and insert '"' (hopefully exiting string mode)
    JSON_ostream &insert(char const *v);

    /// Insert a whole string of characters
    JSON_ostream &insert(std::string const &v) {
        return insert(v.c_str());
    }

    /// Insert an object member name, i.e. a string followed by a ':'
    JSON_ostream &insert(JSON_Member const &v);
};

/// Insert some value (NB, use for number types)
template <typename T>
JSON_ostream &operator <<(JSON_ostream &s, T const &v) {
    return s.insert(v);
}

#endif /* output_hpp */
