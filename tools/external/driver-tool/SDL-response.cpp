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

#include <string>
#include <vector>
#include <algorithm>

#include "json-parse.hpp"
#include "SDL-response.hpp"

bool Response2::ResultEntry::FileEntry::LocationEntry::operator<(LocationEntry const &other) const
{
    if (service < other.service)
        return true;
    if (other.service < service)
        return false;
    if (region < other.region)
        return true;
    if (other.region < region)
        return false;
    return link < other.link;
}

bool Response2::ResultEntry::FileEntryData::operator <(Response2::ResultEntry::FileEntryData const &other) const
{
    if (object && other.object) {
        if (object.value() < other.object.value())
            return true;
        if (other.object.value() < object.value())
            return false;
    }
    if (type < other.type)
        return true;
    if (other.type < type)
        return false;
    if (name < other.name)
        return true;
    if (other.name < name)
        return false;
    return false;
}

std::vector<Response2::ResultEntry::Flat> Response2::ResultEntry::flatten() const
{
    auto result = std::vector<Flat>();

    for (int i = 0; i < (int)files.size(); ++i) {
        auto const &file = files[i];
        for (int j = 0; j < (int)file.locations.size(); ++j)
            result.push_back({i, j});
    }
    std::sort(result.begin(), result.end(), [this](Flat const &a, Flat const &b) {
        auto const fileA = files[a.first];
        auto const fileB = files[b.first];

        if (fileA < fileB)
            return true;
        if (fileB < fileA)
            return false;
        return fileA.locations[a.second] < fileB.locations[b.second];
    });
    return result;
}

Response2::ResultEntry::TypeIndex Response2::ResultEntry::indexTypes() const
{
    auto result = TypeIndex();
    for (int i = 0; i < (int)files.size(); ++i) {
        auto const &p = flattened[i];
        auto const &file = files[p.first];
        result[file.type].push_back(i);
    }
    return result;
}

std::vector<Response2::ResultEntry::Flattened> Response2::ResultEntry::getByType(TypeIndex::key_type const &key) const
{
    auto result = std::vector<Flattened>();
    auto const fnd = byType.find(key);
    if (fnd != byType.end()) {
        for (auto i : fnd->second)
            result.emplace_back(flat(i));
    }
    return result;
}

int Response2::ResultEntry::getCacheFor(Flattened const &match) const
{
    int result = -1;
    auto const &service = match.second.service;
    auto const &region = match.second.region;
    auto const isFromNCBI = (service == "ncbi" || service == "sra-ncbi");
    auto const encrypted = isFromNCBI ? match.second.projectId.has_value() : false;
    auto const fnd = byType.find("vdbcache");
    if (fnd != byType.end()) {
        for (auto i : fnd->second) {
            auto const &p = flattened[i];
            auto const &file = files[p.first];

            if (file.noqual != match.first.noqual) ///< quality must match
                continue;

            auto const &location = file.locations[p.second];

            if (location.service == service && location.region == region)
                return i; ///< a solid match, look no further

            if (location.service == "ncbi" || location.service == "sra-ncbi") {
                auto const thisEncrypted = location.projectId.has_value();
                if (thisEncrypted && encrypted && match.second.projectId == location.projectId)
                    return i; ///< a solid match, look no further
                else if (!thisEncrypted)
                    result = i; ///< a weak match, maybe there's a better one
            }
        }
    }
    return result;
}

using Response2Data = Response2;
namespace impl {
/// @brief parses SDL version 2 response
struct Response2 : public Response2Data {
    using Base = Response2Data;
    struct DecodingError : public Response2Data::DecodingError {
        using Base = Response2Data::DecodingError;

        DecodingError(Base const &other)
        : Base(other)
        {}

        static std::string ordinalString(unsigned ordinal) {
            return std::string("# ") + std::to_string(ordinal);
        }
        DecodingError setCause(std::string const &field) {
            this->field = field;
            return *this;
        }
        DecodingError &setCause(std::string const &field, std::string const &value) {
            this->field = field;
            this->value = value;
            return *this;
        }
        DecodingError &setVictim(std::string const &field) {
            victim = field;
            return *this;
        }
        DecodingError &setLocation(unsigned ordinal) {
            location = ordinalString(ordinal);
            return *this;
        }
        DecodingError &setLocation(std::string const &service) {
            location = service;
            return *this;
        }
        DecodingError &setFile(unsigned ordinal) {
            file = ordinalString(ordinal);
            return *this;
        }
        DecodingError &setFile(std::string const &name) {
            file = name;
            return *this;
        }
        DecodingError &setQuery(unsigned ordinal) {
            query = ordinalString(ordinal);
            return *this;
        }
        DecodingError &setQuery(std::string const &bundle) {
            query = bundle;
            return *this;
        }

        DecodingError setCause(std::string const &field) const {
            return DecodingError(*this).setCause(field);
        }
        DecodingError setCause(std::string const &field, std::string const &value) const {
            return DecodingError(*this).setCause(field, value);
        }
        DecodingError setVictim(std::string const &field) const {
            return DecodingError(*this).setVictim(field);
        }
        DecodingError setLocation(unsigned ordinal) const {
            return DecodingError(*this).setLocation(ordinal);
        }
        DecodingError setLocation(std::string const &service) const {
            return DecodingError(*this).setLocation(service);
        }
        DecodingError setFile(unsigned ordinal) const {
            return DecodingError(*this).setFile(ordinal);
        }
        DecodingError setFile(std::string const &name) const {
            return DecodingError(*this).setFile(name);
        }
        DecodingError setQuery(unsigned ordinal) const {
            return DecodingError(*this).setQuery(ordinal);
        }
        DecodingError setQuery(std::string const &bundle) const {
            return DecodingError(*this).setQuery(bundle);
        }
    };

    struct ResultEntry : public Response2Data::ResultEntry {
        using Base = Response2Data::ResultEntry;

        struct FileEntry : public Response2Data::ResultEntry::FileEntry {
            using Base = Response2Data::ResultEntry::FileEntry;

            struct LocationEntry : public Response2Data::ResultEntry::FileEntry::LocationEntry {
                using Base = Response2Data::ResultEntry::FileEntry::LocationEntry;

                struct Delegate final : public JSONParser::Delegate {
                    JSONValueDelegate<std::string> link, service, region, expirationDate, projectId;
                    JSONValueDelegate<bool> ceRequired, payRequired;

                    bool wasSet = false;

                    JSONParser::Delegate *member(std::string const &name)
                    {
                        if (name == "link")
                            return &link;
                        if (name == "service")
                            return &service;
                        if (name == "region")
                            return &region;
                        if (name == "expirationDate")
                            return &expirationDate;
                        if (name == "encryptedForProjectId")
                            return &projectId;
                        if (name == "ceRequired")
                            return &ceRequired;
                        if (name == "payRequired")
                            return &payRequired;
                        return this;
                    }
                    JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
                    {
                        switch (type) {
                        case JSONParser::evt_end:
                            return nullptr;
                        case JSONParser::evt_member_name:
                            wasSet = true;
                            return member(JSONString(value, true));
                        default:
                            break;
                        }
                        return this;
                    }

                    bool isSet() const { return wasSet; }
                    operator bool() const { return isSet(); }
                    void unset() {
                        wasSet = false;
                        link.unset();
                        service.unset();
                        region.unset();
                        expirationDate.unset();
                        projectId.unset();
                        ceRequired.unset();
                        payRequired.unset();
                    }

                    Base get(unsigned ordinal)
                    {
                        auto error = DecodingError({
                            DecodingError::missing, DecodingError::locations
                        });

                        if (!wasSet)
                            throw error.setLocation(ordinal);
                        if (!service)
                            throw error.setLocation(ordinal).setVictim("service");

                        auto const &service = this->service.get();
                        error.setLocation(service);
                        if (!link)
                            throw error.setVictim("URL");

                        return {
                              link.get()
                            , service
                            , region ? region.get() : defaultRegionForService(service)
                            , expirationDate ? opt_string(expirationDate.get()) : opt_string()
                            , projectId ? opt_string(projectId.get()) : opt_string()
                            , ceRequired.isSet() ? ceRequired.get() : false
                            , payRequired.isSet() ? payRequired.get() : false
                        };
                    }

                    static std::string const &defaultRegionForService(std::string const &service) {
                        static auto const table = std::map<std::string, std::string>({
                            {"ncbi", "be-md"}
                        });
                        auto const fnd = table.find(service);
                        if (fnd != table.end())
                            return fnd->second;
                        throw DecodingError({
                            DecodingError::no_default, DecodingError::locations
                        }).setVictim("region").setCause("service", service).setLocation(service);
                    }
                    static std::ostream &print(std::ostream &os, Response2Data::DecodingError const &err) {
                        assert(err.object == Response2::DecodingError::locations);
                        switch (err.errorType) {
                        case Response2::DecodingError::missing:
                            if (!err.haveVictim())
                                os << "Empty record";
                            else
                                os << "No " << err.victim;
                            break;
                        case Response2::DecodingError::no_default:
                            os << "No default value for " << err.victim;
                            break;
                        default:
                            throw std::logic_error("unreachable case");
                        }
                        if (err.haveCause()) {
                            os << " and " << err.field << " is ";
                            if (err.haveCauseValue())
                                os << '\'' << err.value << '\'';
                            else
                                os << "empty";
                        }
                        return os << " for location " << err.location;
                    }
                };
            };

            struct Delegate final : public JSONParser::Delegate {
                JSONValueDelegate<std::string> type, name, object, md5, format, modificationDate;
                JSONValueDelegate<std::string, std::string> size;
                JSONValueDelegate<bool> noqual;
                ArrayDelegate<LocationEntry, Base::Locations> locations;

                bool wasSet = false;

                JSONParser::Delegate *member(std::string const &name)
                {
                    if (name == "type")
                        return &type;
                    if (name == "name")
                        return &this->name;
                    if (name == "object")
                        return &object;
                    if (name == "md5")
                        return &md5;
                    if (name == "format")
                        return &format;
                    if (name == "modificationDate")
                        return &modificationDate;
                    if (name == "size")
                        return &size;
                    if (name == "noqual")
                        return &noqual;
                    if (name == "locations")
                        return &locations;
                    return this;
                }
                JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
                {
                    switch (type) {
                    case JSONParser::evt_end:
                        return nullptr;
                    case JSONParser::evt_member_name:
                        wasSet = true;
                        return member(JSONString(value, true));
                    default:
                        break;
                    }
                    return this;
                }

                bool isSet() const { return wasSet; }
                operator bool() const { return isSet(); }
                void unset() {
                    wasSet = false;
                    type.unset();
                    name.unset();
                    object.unset();
                    md5.unset();
                    format.unset();
                    modificationDate.unset();
                    size.unset();
                    noqual.unset();
                    locations.unset();
                }

                Base get(unsigned ordinal)
                {
                    auto error = DecodingError({
                        DecodingError::missing, DecodingError::files
                    });

                    if (!wasSet)
                        throw error.setFile(ordinal);
                    if (!name)
                        throw error.setFile(ordinal).setVictim("name");

                    auto const name = this->name.get();
                    error.setFile(name);

                    if (!type && !object)
                        throw error.setVictim("type").setCause("object");

                    try {
                        return {
                            {
                                  type ? type.get() : typeFromObject(object.get())
                                , name
                                , object ? opt_string(object.get()) : opt_string()
                                , size ? opt_string(size.get()) : opt_string()
                                , md5 ? opt_string(md5.get()) : opt_string()
                                , format ? opt_string(format.get()) : opt_string()
                                , modificationDate ? opt_string(modificationDate.get()) : opt_string()
                                , noqual.isSet() ? noqual.get() : false
                            }
                            , locations.get()
                        };
                    }
                    catch (DecodingError const &e) {
                        // Add FileEntry name to the exception and rethrow
                        throw e.setFile(name);
                    }
                }

                static std::string const &typeFromObject(std::string const &object) {
                    static auto const table = std::map<std::string, std::string>({
                        {"wgs", "sra"}
                    });
                    auto const pipe_at = object.find('|');
                    if (pipe_at != std::string::npos) {
                        auto const type = object.substr(0, pipe_at);
                        auto const fnd = table.find(type);
                        if (fnd != table.end())
                            return fnd->second;
                    }
                    throw DecodingError({
                        DecodingError::no_default, DecodingError::files
                    }).setVictim("type").setCause("object", object);
                }

                static std::ostream &print(std::ostream &os, Response2Data::DecodingError const &err) {
                    assert(err.object == Response2::DecodingError::files);
                    switch (err.errorType) {
                    case Response2::DecodingError::missing:
                        if (!err.haveVictim())
                            os << "Empty record";
                        else
                            os << "No " << err.victim;
                        break;
                    case Response2::DecodingError::no_default:
                        os << "No default value for " << err.victim;
                        break;
                    default:
                        throw std::logic_error("unreachable case");
                    }
                    if (err.haveCause()) {
                        os << " and " << err.field << " is ";
                        if (err.haveCauseValue())
                            os << '\'' << err.value << '\'';
                        else
                            os << "empty";
                    }
                    return os << " for file " << err.file;
                }
            };
        };

        struct Delegate final : public JSONParser::Delegate {
            JSONValueDelegate<std::string> query, message;
            JSONValueDelegate<std::string, JSONNumber> status;
            ArrayDelegate<FileEntry, Base::Files> files;

            bool wasSet = false;

            JSONParser::Delegate *member(std::string const &name)
            {
                if (name == "status")
                    return &status;
                if (name == "msg")
                    return &message;
                if (name == "bundle")
                    return &query;
                if (name == "files")
                    return &files;
                return this;
            }
            JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
            {
                switch (type) {
                case JSONParser::evt_end:
                    return nullptr;
                case JSONParser::evt_member_name:
                    wasSet = true;
                    return member(JSONString(value, true));
                default:
                    break;
                }
                return this;
            }

            bool isSet() const { return wasSet; }
            operator bool() const { return isSet(); }
            void unset() {
                wasSet = false;
                query.unset();
                status.unset();
                message.unset();
                files.unset();
            }

            Base get(unsigned ordinal) const
            {
                auto error = DecodingError({
                    DecodingError::missing, DecodingError::result
                });

                if (!wasSet)
                    throw error.setQuery(ordinal);
                if (!query)
                    throw error.setQuery(ordinal).setVictim("bundle");

                error.setQuery(query.get());

                if (!status)
                    throw error.setVictim("status");
                if (!message)
                    throw error.setVictim("message");

                return {
                      query.get()
                    , status.get()
                    , message.get()
                    , files.get()
                };
            }
            static std::ostream &print(std::ostream &os, Response2Data::DecodingError const &err) {
                assert(err.object == Response2::DecodingError::result);
                switch (err.errorType) {
                case Response2::DecodingError::missing:
                    if (!err.haveVictim())
                        return os << "Empty result " << err.query;
                    return os << "No " << err.victim << " for result " << err.query;
                default:
                    throw std::logic_error("unreachable case");
                }
            }
        };
    };

    struct Delegate final : public JSONParser::Delegate {
        JSONValueDelegate<std::string> version, message, nextToken;
        JSONValueDelegate<std::string, JSONNumber> status;
        ArrayDelegate<ResultEntry, Base::Results> results;

        bool wasSet = false; ///< becomes true if we received at least one member event

        JSONParser::Delegate *member(std::string const &name)
        {
            if (name == "version")
                return &version;
            if (name == "status")
                return &status;
            if (name == "message")
                return &message;
            if (name == "nextToken")
                return &nextToken;
            if (name == "result")
                return &results;
            return this;
        }
        JSONParser::Delegate *receive(JSONParser::EventType type, StringView const &value)
        {
            switch (type) {
            case JSONParser::evt_end:
                return nullptr;
            case JSONParser::evt_member_name:
                wasSet = true;
                return member(JSONString(value, true));
            default:
                break;
            }
            return this;
        }
        bool isSet() const { return wasSet; }
        operator bool() const { return isSet(); }
        void unset() { wasSet = false; }

        Base get()
        {
            auto error = DecodingError(DecodingError::Base{
                DecodingError::missing, DecodingError::topLevel
            });
            if (!wasSet)
                throw error;
            if (!version)
                throw error.setVictim("version");

            auto const version = this->version.get();
            if (version == "2" || version.substr(0, 2) == "2.")
                ;
#if DEBUG || _DEBUGGING
            else if (version == "unstable")
                ;
#endif
            else
                throw DecodingError({
                    DecodingError::bad_version, DecodingError::topLevel
                }).setCause("version", version);

            if (!status || status.rawValue() == "200") {
                return {
                      "200", "OK"
                    , nextToken ? opt_string(nextToken.get()) : opt_string()
                    , results.get()
                };
            }
            return {
                  status.get()
                , message ? message.get() : ""
            };
        }
        static std::ostream &print(std::ostream &os, Response2Data::DecodingError const &err) {
            switch (err.errorType) {
            case Response2::DecodingError::missing:
                if (!err.haveVictim())
                    return os << "Empty response from SDL!";
                os << "No " << err.victim << " in response from SDL";
                if (err.haveCause()) {
                    os << " and " << err.field << " is ";
                    if (err.haveCauseValue())
                        os << '\'' << err.value << '\'';
                    else
                        os << "empty";
                }
                return os;
            case Response2::DecodingError::bad_version:
                return os << "Unexpected SDL version: " << err.value;
            default:
                throw std::logic_error("unreachable case");
            }
        }
    };
};
} // namespace impl

Response2 Response2::makeFrom(char const *json) {
    auto result = impl::Response2::Delegate();
    auto tlo = TopLevelObjectDelegate(&result);
    auto parser = JSONParser(json, &tlo);

    parser.parse();
    return result.get();
}

std::ostream &operator <<(std::ostream &os, Response2::DecodingError const &err)
{
    switch (err.object) {
    case Response2::DecodingError::topLevel:
        return impl::Response2::Delegate::print(os, err);
    case Response2::DecodingError::result:
        return impl::Response2::ResultEntry::Delegate::print(os, err);
    case Response2::DecodingError::files:
        return impl::Response2::ResultEntry::FileEntry::Delegate::print(os, err);
    case Response2::DecodingError::locations:
        return impl::Response2::ResultEntry::FileEntry::LocationEntry::Delegate::print(os, err);
    default:
        throw std::logic_error("unreachable case");
    }
}
