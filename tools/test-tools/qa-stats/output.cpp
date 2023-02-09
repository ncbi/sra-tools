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


#include <cctype>
#include "output.hpp"
#include "hashing.hpp"

void JSON_ostream::insert_raw(char v) {
    strm << v;
    ws = isspace(v);
}

void JSON_ostream::indentIfNeeded() {
    if (newline) {
        insert_raw('\n');
        for (auto _ : listStack)
            insert_raw('\t');
        newline = false;
    }
}

void JSON_ostream::insert_instr(char const *const str) {
    for (auto cp = str; *cp; ++cp)
        insert_instr(*cp);
}

void JSON_ostream::insert_raw(char const *const str) {
    for (auto cp = str; *cp; ++cp)
        insert_raw(*cp);
}

void JSON_ostream::newList(char type) {
    if (!ws)
        insert_raw(' ');
    insert_raw(type);
    newline = true;
    listStack.push_back(false);
}

void JSON_ostream::listItem() {
    if (listStack.back()) {
        insert_raw(',');
        newline = true;
    }
    else
        listStack.back() = true;
    indentIfNeeded();
}

void JSON_ostream::endList(char type) {
    newline = listStack.back();
    listStack.pop_back();
    indentIfNeeded();
    insert_raw(type);
    newline = true;
}

void JSON_ostream::insert_instr(char v) {
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

JSON_ostream &JSON_ostream::insert(char v) {
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
    comma = false;

    switch (v) {
    case '"':
        if (!ws)
            insert(' ');
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
    strm << v;
    ws = isspace(v);
    return *this;
}

JSON_ostream &JSON_ostream::insert(char const *v) {
    auto const need_quotes = !instr;

    if (need_quotes)
        insert('"');
    for (auto cp = v; *cp; ++cp)
        insert_instr(*cp);
    assert(!esc);
    if (need_quotes)
        insert('"');
    return *this;
}

JSON_ostream &JSON_ostream::insert(JSON_Member const &v) {
    comma = true; // doesn't matter how many times you set it.
    insert(v.name);
    insert_raw(':');
    return *this;
}
