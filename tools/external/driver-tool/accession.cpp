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
 *  sratools command line tool
 *
 * Purpose:
 *  NCBI SRA accession parsing
 *
 */

#include <string>
#include <vector>
#include <utility>
#include "accession.hpp"

namespace sratools {

char const *Accession::extensions[] = { ".sra", ".sralite", ".realign", ".noqual", ".sra" };
char const *Accession::qualityTypes[] = { "Full", "Lite" };
char const *Accession::qualityTypeForFull = qualityTypes[0];
char const *Accession::qualityTypeForLite = qualityTypes[1];

Accession::Accession(std::string const &value)
: value(value)
, alphas(0)
, digits(0)
, vers(0)
, verslen(0)
, ext(0)
, valid(false)
{
    static auto constexpr min_alpha = 3;
    static auto constexpr max_alpha = 6; // WGS can go to 6 now
    static auto constexpr min_digit = 6;
    static auto constexpr max_digit = 9;

    auto const size = value.size();

    while (alphas < size) {
        auto const ch = value[alphas];

        if (!isalpha(ch))
            break;

        ++alphas;
        if (alphas > max_alpha)
            return;
    }
    if (alphas < min_alpha)
        return;

    while (digits + alphas < size) {
        auto const ch = value[digits + alphas];

        if (!isdigit(ch))
            break;

        ++digits;
        if (digits > max_digit)
            return;
    }
    if (digits < min_digit)
        return;

    auto i = digits + alphas;
    if (i == size || value[i] == '.')
        valid = true;
    else
        return;

    for ( ; i < size; ++i) {
        auto const ch = value[i];

        if (verslen > 0 && vers == ext + 1) { // in version
            if (ch == '.')
                ext = i; // done with version
            else if (isdigit(ch))
                ++verslen;
            else
                vers = 0;
            continue;
        }
        if (ch == '.') {
            ext = i;
            continue;
        }
        if (i == digits + alphas + 1 && isdigit(ch)) { // this is the first version digit.
            vers = i;
            verslen = 1;
        }
    }
}

enum AccessionType Accession::type() const {
    if (valid)
        switch (toupper(value[0])) {
        case 'D':
        case 'E':
        case 'S':
            if (toupper(value[1]) == 'R')
                switch (toupper(value[2])) {
                case 'A': return submitter;
                case 'P': return project;
                case 'R': return run;
                case 'S': return study;
                case 'X': return experiment;
                }
        }
    return unknown;
}

std::vector<std::pair<unsigned, unsigned>> Accession::sraExtensions() const
{
    auto result = allExtensions();
    auto i = result.begin();

    while (i != result.end()) {
        auto const &ext = value.substr(i->first, i->second);
        auto found = false;

        for (auto j = 0; j < 4; ++j) {
            if (ext != extensions[j])
                continue;

            *i = {unsigned(j), i->first};
            found = true;
            break;
        }
        if (found)
            ++i;
        else
            i = result.erase(i);
    }

    return result;
}

std::vector<std::pair<unsigned, unsigned>> Accession::allExtensions() const
{
    std::vector<std::pair<unsigned, unsigned>> result;

    for (auto i = digits + alphas; i < value.size(); ++i) {
        if (vers <= i + 1 && i + 1 < vers + verslen)
            continue;
        if (value[i] != '.')
            continue;
        result.push_back({i, 1});
    }
    for (auto & i : result) {
        while (i.first + i.second < value.size() && value[i.first + i.second] != '.')
            ++i.second;
    }
    return result;
}
}
