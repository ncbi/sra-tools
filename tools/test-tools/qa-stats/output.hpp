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

struct JSON_Member {
    std::string name;
};

class JSON_ostream {
    std::ostream &strm;
    bool ws = true;
    bool newline = true;
    bool comma = false;
    bool instr = false;
    bool esc = false;
    std::vector<bool> stack;

    void insert_raw(char v);
    void indentIfNeeded();
    void insert_instr(char v);
    void insert_instr(char const *const str);
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
    JSON_ostream &insert(T v) {
        std::ostringstream oss;

        oss << v;
        if (!ws)
            insert_raw(' ');
        insert_raw(oss.str().c_str());
        return *this;
    }
    JSON_ostream &insert(bool const &v) {
        insert_raw(v ? "true" : "false");
        return *this;
    }
    JSON_ostream &insert(char v);
    JSON_ostream &insert(char const *v);
    JSON_ostream &insert(std::string const &v) {
        return insert(v.c_str());
    }
    JSON_ostream &insert(JSON_Member const &v) {
        comma = true; // doesn't matter how many times you set it.
        insert(v.name);
        insert_raw(':');
        return *this;
    }
};

template <typename T>
JSON_ostream &operator <<(JSON_ostream &s, T const &v) {
    return s.insert(v);
}

#endif /* output_hpp */
