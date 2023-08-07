#ifndef __VDB_DATA_HPP_INCLUDED__
#define __VDB_DATA_HPP_INCLUDED__ 1

/* =============================================================================
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
 * Structure that contain information about Schema
 * and its formatting for sra-info
 */

#include <vector>

namespace VDB {

struct SchemaData {
    typedef std::vector<SchemaData> Data;

    std::string name;
    std::vector<SchemaData> parent;

    void print(const std::string &space, int n, std::string &out) const {
        for (int i = 0; i < n; ++i) out += space;
        out +=  name + "\n";
        for (auto it = parent.begin(); it < parent.end(); ++it)
            (*it).print(space, n + 1, out);
    }

    void print(int indent, const std::string &space, std::string &out,
        bool first,
        const std::string &open, const std::string &openNext,
        const std::string &closeName, const std::string &close,
        const std::string &openParent1 = "",
        const std::string &openParent2 = "",
        const std::string &closeParent1 = "",
        const std::string &closeParent2 = "",
        const std::string &noParent = "") const
    {
        if (!first) out += openNext;

        for (int i = 0; i < indent; ++i) out += space;
        out += open + name + closeName;

        if (!parent.empty()) {
            out += openParent1;
            if (!openParent2.empty()) {
                for (int i = 0; i < indent; ++i) out += space;
                out += openParent2;
                ++indent;
            }
 
            bool frst = true;
            for (auto it = parent.begin(); it < parent.end(); it++) {
                (*it).print(indent + 1, space, out, frst, open, openNext,
                    closeName, close, openParent1, openParent2,
                    closeParent1, closeParent2, noParent);
                frst = false;
            }
 
            if (!closeParent2.empty()) {
                out += closeParent1;
                for (int i = 0; i < indent; ++i) out += space;
                out += closeParent2;
            }
            if (!openParent2.empty())
                --indent;
        }
        else
            out += noParent;

        for (int i = 0; i < indent; ++i) out += space;
        out += close;
    }
};

struct SchemaInfo {
    SchemaData::Data db;
    SchemaData::Data table;
    SchemaData::Data view;
};

} // namespace VDB

#endif // __VDB_DATA_HPP_INCLUDED__
