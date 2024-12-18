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
*  Communicate with SDL
*
*/

#pragma once
#include <string>
#include <vector>
#include "util.hpp"
#include "constants.hpp"
#include "opt_string.hpp"
#include "sratools.hpp"
#include "command-line.hpp"
#include "tool-args.hpp"

/// @brief Contains the response from SDL and/or local file info.
class data_sources {
public:
    class accession {
    public:
        class info {
            friend class accession;
            info(Dictionary const *info, unsigned index);
        public:
            std::string service;
            opt_string qualityType;
            opt_string project;
            Dictionary environment;

            bool haveQualityType() const {
                return qualityType.has_value();
            }
            bool haveFullQuality() const {
                return qualityType.has_value() && qualityType.value() == sratools::Accession::qualityTypeForFull;
            }
            bool haveZeroQuality() const {
                return qualityType.has_value() && qualityType.value() == sratools::Accession::qualityTypeForLite;
            }
            bool encrypted() const {
                return project.has_value();
            }
        };
        struct const_iterator {
            const_iterator &operator ++() {
                index += 1;
                return *this;
            }
            const_iterator operator ++(int) {
                auto const save = index++;
                return const_iterator(parent, save);
            }
            bool operator ==(const_iterator const &other) const {
                return parent == other.parent && index == other.index;
            }
            bool operator !=(const_iterator const &other) const {
                return !(*this == other);
            }
            accession::info operator *() const {
                return parent->get(index);
            }
        private:
            friend accession;
            
            explicit const_iterator(accession const *parent, unsigned index)
            : parent(parent)
            , index(index)
            {}

            accession const *parent;
            unsigned index;
        };
    private:
        friend class data_sources;
        friend const_iterator;

        Dictionary queryInfo;
        unsigned first;
        unsigned count;

        info get(unsigned index) const {
            return info(&queryInfo, index);
        }
        accession(Dictionary const &info);
    public:
        const_iterator begin() const { return const_iterator(this, first); }
        const_iterator end() const { return const_iterator(this, count); }
    };
private:
    std::string ce_token_;
    bool have_ce_token;

public:
    std::map<std::string, Dictionary> queryInfo;

    data_sources(CommandLine const &cmdline, Arguments const &parsed);
    data_sources(CommandLine const &cmdline, Arguments const &parsed, bool withSDL);

    accession operator [](std::string const &name) const {
        auto const fnd = queryInfo.find(name);
        assert(fnd != queryInfo.end());
        if (fnd != queryInfo.end())
            return accession(fnd->second);
        throw std::out_of_range(name + " is not in the set.");
    }
    
    /// @brief informative only
    std::string const &ce_token() const {
        return ce_token_;
    }
    
    /// @brief set/unset CE Token environment variable
    void set_ce_token_env_var() const;

    static void set_param_bits_env_var(uint64_t bits);
    
    static void preferNoQual();

    struct QualityPreference {
        bool isSet;
        bool isFullQuality;
    };
    static QualityPreference qualityPreference();

    /// @brief Call SDL with accesion/query list and process the results.
    /// Can use local file info if no response from SDL.
    static data_sources preload(CommandLine const &cmdline, Arguments const &parsed);
};
