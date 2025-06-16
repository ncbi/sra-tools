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

#ifndef JSON_ostream_hpp
#define JSON_ostream_hpp

#include <ostream>
#include <string>
#include <vector>
#include <cctype>
#include <string_view>
#include <cassert>

/// @brief Use for inserting member names into JSON_ostream.
/// @details This is mainly a convienence and to make code more self-documenting.
struct JSON_Member {
    std::string name;
};

/// @brief A JSON formatting output stream.
///
/// It is incumbent on the caller to create logically correct JSON. In
/// particular, if you attempt to insert a list seperator (i.e. `,`) when
/// you have not yet created an object or array (a list context), it will
/// break.
///
/// This formatter is stateful. It tracks the list context depth and has a
/// string mode to enable correct handling of special characters in a string.
///
/// When not in string mode, these characters have special meanings: `[]{},"`,
/// see `insert(char)` below.
class JSON_ostream {
    std::ostream &strm;
    bool compact = false;
    bool ws = true;         ///< was the last character inserted a whitespace character.
    bool newline = true;    ///< Is newline needed before the next item.
    bool comma = false;     ///< Was the character a ','? More importanly, will the next character belong to a new list item (or be the end of list).
    bool instr = false;     ///< In a string, therefore apply string escaping rules to the inserted characters.
    bool esc = false;       ///< In a string and the next character is escaped.

    /// A list is anything with components that are separated by ',', i.e. JSON Objects and Arrays
    std::vector<bool> listStack; ///< Records if list is empty. false mean empty. back() is the current list.

    /// Send the character to the output stream. Checks for whitespace.
    void insert_raw(char v) {
        strm << v;
        ws = std::isspace(v);
    }

    /// Insert a string without using any escaping rules.
    void insert_raw(std::string_view v) {
        for (auto && ch : v)
            insert_raw(ch);
    }

    /// Insert a character using string escaping rules.
    void insert_instr(char v) {
        if (esc) {
            esc = false;
            switch (v) {
            case '\\':
            case '"':
                insert_raw(v);
                return;
            default:
                break;
            }
        }
        switch (v) {
        case '\\':
            esc = true;
        case '/':
        case '\b':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            insert_raw('\\');
            break;
        case '"':
            instr = false;
            break;
        default:
            break;
        }
        switch (v) {
        case '\b':
            insert_raw('b');
            break;
        case '\f':
            insert_raw('f');
            break;
        case '\n':
            insert_raw('n');
            break;
        case '\r':
            insert_raw('r');
            break;
        case '\t':
            insert_raw('t');
            break;
        default:
            insert_raw(v);
            break;
        }
    }

    /// If needed, start a newline and indent it.
    void indentIfNeeded() {
        if (!compact && newline) {
            insert_raw('\n');
            for (auto x : listStack) {
                insert_raw('\t');
                ((void)x);
            }
            newline = false;
        }
    }

    // start a new list (array or object)
    void newList(char type) {
        if (!ws && !compact)
            insert_raw(' ');
        insert_raw(type);
        newline = true;
        listStack.push_back(false);
    }

    // start a new list item
    void listItem() {
        assert(!listStack.empty());
        if (listStack.back()) {
            insert_raw(',');
            newline = true;
        }
        comma = false;
        listStack.back() = true;
        indentIfNeeded();
    }

    // end a list (array or object)
    void endList(char type) {
        assert(!listStack.empty());
        newline = listStack.back();
        listStack.pop_back();
        indentIfNeeded();
        insert_raw(type);
        newline = true;
    }

    // These `insert` functions are overloaded for types
    // that have specific representations in JSON.

    /// Insert a Boolean value
    JSON_ostream &insert(bool v) {
        if (comma)
            listItem();
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
    JSON_ostream &insert(char v) {
        if (instr) {
            insert_instr(v);
            return *this;
        }

        switch (v) {
        case ']':
        case '}':
            endList(v);
        case ',':
            comma = true;
            return *this;
        }

        if (comma)
            listItem();

        switch (v) {
        case '"':
            if (!ws && !compact)
                insert_raw(' ');
            instr = true;
            break;
        case '[':
        case '{':
            newList(v);
            comma = true;
            return *this;
        default:
            break;
        }
        insert_raw(v);
        return *this;
    }

    /// Insert a whole string of characters.
    /// In string mode, it will append to the current string.
    /// Otherwise, it will insert '"' (thus entering string mode),
    /// append the string, and insert '"' (hopefully exiting string mode)
    JSON_ostream &insert(std::string_view v) {
        auto const need_quotes = !instr;

        if (need_quotes) {
            if (comma)
                listItem();
            insert('"');
        }
        for (auto && ch : v)
            insert_instr(ch);

        if (need_quotes)
            insert('"');

        return *this;
    }

    /// Insert a whole string of characters.
    JSON_ostream &insert(std::string const &v) {
        return insert(std::string_view{v});
    }

    /// Insert a whole string of characters.
    JSON_ostream &insert(char const *v) {
        return insert(std::string_view{v});
    }

    /// Insert an object member name, i.e. a string followed by a ':'
    JSON_ostream &insert(JSON_Member const &v) {
        comma = true; // doesn't matter how many times you set it.
        insert(v.name);
        insert_raw(':');
        return *this;
    }
    
    template <typename T>
    static std::string c_locale_to_string(T const &v) {
        std::ostringstream ss; ss.imbue(std::locale::classic());
        ss << v;
        auto const &result = ss.str();
        assert(result.find(',') == result.npos); // make sure we don't have disallowed characters
        return result;
    }

    /// This is a catch-all, intended for numeric types
    template <typename T>
    JSON_ostream &insert(T const &v) {
        if (instr) {
            strm << v;
            return *this;
        }
        if (comma)
            listItem();
        if (!ws && !compact)
            insert_raw(' ');
        
        // make sure we use the "C" locale
        strm << c_locale_to_string(v);

        return *this;
    }

public:
    explicit JSON_ostream(std::ostream &os, bool p_compact = false)
    : strm(os)
    , compact(p_compact)
    {}

    bool is_compact() const { return compact; }
    JSON_ostream &set_compact(bool value) { 
        if (value && !ws && !compact)
            insert_raw(' ');
        compact = value; return *this;
    }

    struct Compact { bool value; };
    friend JSON_ostream &operator <<(JSON_ostream &s, Compact v) {
        return s.set_compact(v.value);
    }

    /// This will use the appropriate overload of `insert`.
    /// It is incumbent on the caller to create logically correct JSON.
    /// See `insert(char)` above for how special characters cause state changes.
    template <typename T>
    friend JSON_ostream &operator <<(JSON_ostream &s, T const &v) {
        return s.insert(v);
    }
};

#endif /* JSON_ostream_hpp */
