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
 *  Parse SDL Response JSON
*
*/

#pragma once

#include <string>
#include <vector>
#include <map>
#include <utility>

#include "opt_string.hpp"
#include "util.hpp"

/// @brief holds SDL version 2 response
/// @Note member names generally match the corresponding member names in the SDL response JSON
struct Response2 {
    struct DecodingError {
        enum Type {
            bad_version = 1,
            missing,
            no_default,
        } errorType;
        enum Object {
            topLevel = 0,
            result,
            files,
            locations
        } object;
        std::string field;      ///< name of field that is the cause.
        std::string value;      ///< value of the field if it is the value that is the cause.
        std::string victim;     ///< name of field that is the problem.
        std::string location;   ///< "service" field or locations array ordinal.
        std::string file;       ///< "name" field or files array ordinal.
        std::string query;      ///< "bundle" field or result array ordinal.

        bool haveVictim() const { return !victim.empty(); }
        bool haveCause() const { return !field.empty(); }
        bool haveCauseValue() const { return !value.empty(); }

        bool haveVictim(std::string const &field) const { return victim == field; }
        bool haveCause(std::string const &field) const { return this->field == field; }

        friend std::ostream &operator <<(std::ostream &, DecodingError const &);
    };
    /// @brief holds a single element of the result array
    struct ResultEntry {
        struct FileEntryData {
            std::string type, name;
            opt_string object, size, md5, format, modificationDate;
            bool noqual;

            opt_string objectType() const {
                auto result = opt_string();
                if (object) {
                    auto const &obj = object.value();
                    auto const sep = obj.find('|');
                    if (sep != std::string::npos)
                        result = obj.substr(0, sep);
                }
                return result;
            }
            bool hasExtension(std::string const &ext) const {
                return ends_with(ext, name);
            }
            bool operator <(FileEntryData const &other) const;
        };
        /// @brief holds a single file (element of the files array)
        struct FileEntry : public FileEntryData {
            /// @brief holds a single element of the locations array
            struct LocationEntry {
                std::string link, service, region;
                opt_string expirationDate, projectId;
                bool ceRequired, payRequired;

                bool operator <(LocationEntry const &other) const;
            };

            using Locations = std::vector<LocationEntry>;
            Locations locations;

            FileEntry(FileEntryData &&base, Locations &&locations)
            : FileEntryData(std::move(base))
            , locations(std::move(locations))
            {}
            FileEntry(FileEntryData const &base, Locations const &locations)
            : FileEntryData(base)
            , locations(locations)
            {}
        };

        using Files = std::vector<FileEntry>;
        using Flat = std::pair<unsigned, unsigned>; // files index, and files location index
        std::string query;
        std::string status;
        std::string message;
        Files files;

        std::vector<Flat> flattened;
        std::vector<Flat> flatten() const;
        using Flattened = std::pair<FileEntryData const &, FileEntry::LocationEntry const &>;
        Flattened flat(unsigned index) const {
            auto const &p = flattened[index];
            auto const &file = files[p.first];
            return {file, file.locations[p.second]};
        }

        using TypeIndex = std::map<std::string, std::vector<unsigned>>;
        TypeIndex byType;
        TypeIndex indexTypes() const;
        std::vector<Flattened> getByType(std::string const &key) const;

        /// \brief The cache file can come from either the same location, or it can come from NCBI if it is encrypted for the same project or is not encrypted.
        /// \returns -1 if none found, else index in flattened.
        int getCacheFor(Flattened const &match) const;

        ResultEntry(std::string const &query, std::string const &status, std::string const &message, Files const &files)
        : query(query)
        , status(status)
        , message(message)
        , files(files)
        , flattened(flatten())
        , byType(indexTypes())
        {}
    };

    using Results = std::vector<ResultEntry>;
    std::string status;
    std::string message;
    opt_string nextToken;
    Results results;

    static Response2 makeFrom(char const *json);
    static Response2 makeFrom(std::string const &json) {
        return makeFrom(json.c_str());
    }
};
