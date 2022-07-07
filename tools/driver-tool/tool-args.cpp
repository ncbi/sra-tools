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
 *  provide tool command info
 *
 */

#include <string>
#include <map>
#include <set>
#include <vector>
#include <array>
#include <algorithm>
#include <initializer_list>
#include <iostream>

#include <cstdlib>

#include "globals.hpp"
#include "debug.hpp"
#include "tool-args.hpp"
#include "command-line.hpp"

#if USE_TOOL_HELP
#define TOOL_HELP(...) {__VA_ARGS__}
#else
#define TOOL_HELP(...) {}
#endif
#define TOOL_ARGS(...) {__VA_ARGS__}
#if USE_TOOL_HELP
#define TOOL_ARG(LONG, ALIAS, ARG, HELP) {LONG, ALIAS, ARG, 0, HELP}
#else
#define TOOL_ARG(LONG, ALIAS, ARG, HELP) {LONG, ALIAS, ARG, 0}
#endif

#define TOOL_ARGS_COMMON TOOL_ARGS ( \
    TOOL_ARG("ngc", "", true, TOOL_HELP("PATH to ngc file.", 0)), \
    TOOL_ARG("cart", "", true, TOOL_HELP("To read a kart file.", 0)), \
    TOOL_ARG("perm", "", true, TOOL_HELP("PATH to jwt cart file.", 0)), \
    TOOL_ARG("help", "h?", false, TOOL_HELP("Output a brief explanation for the program.", 0)), \
    TOOL_ARG("version", "V", false, TOOL_HELP("Display the version of the program then quit.", 0)), \
    TOOL_ARG("log-level", "L", true, TOOL_HELP("Logging level as number or enum string.", 0)), \
    TOOL_ARG("verbose", "v", false, TOOL_HELP("Increase the verbosity of the program status messages.", "Use multiple times for more verbosity.", "Negates quiet.", 0)), \
    TOOL_ARG("quiet", "q", false, TOOL_HELP("Turn off all status messages for the program.", "Negates verbose.", 0)), \
    TOOL_ARG("option-file", "", true, TOOL_HELP("Read more options from the file.", 0)), \
    TOOL_ARG("debug", "+", true, TOOL_HELP("Turn on debug output for module.", "All flags if not specified.", 0)), \
    TOOL_ARG("no-user-settings", "", false, TOOL_HELP("Turn off user-specific configuration.", 0)), \
    TOOL_ARG("ncbi_error_report", "", true, TOOL_HELP("Control program execution environment report generation (if implemented).", "One of (never|error|always). Default is error.", 0)), \
    TOOL_ARG(0, 0, 0, TOOL_HELP(0)))

#include "tool-arguments.h"

ParameterDefinition const &ParameterDefinition::unknownParameter() {
    static auto const y = ParameterDefinition(TOOL_ARG(nullptr, nullptr, false, {}));
    return y;
}

ParameterDefinition const &ParameterDefinition::argument() {
    static auto const y = ParameterDefinition(TOOL_ARG("", nullptr, true, {}));
    return y;
}

struct ArgvIterator {
    CommandLine const &parent;
    int argind;
    int indarg;

    ArgvIterator(CommandLine const &parent)
    : parent(parent)
    , argind(0)
    , indarg(0)
    {}

    /// \brief move to next parameter
    ///
    /// In normal mode, moves to next argv element, if any.
    /// In shortArg mode, moves to next short arg, or switches back to normal mode if no more shortArgs.
    ///
    /// \returns 0 if there are no more, 1 if in normal mode, -1 if in shortArg mode.
    int next() {
        if (argind == parent.argc) return 0;
        if (indarg > 0) {
            ++indarg;
            if (parent.argv[argind][indarg] != '\0')
                return -1;
            indarg = 0;
        }
        ++argind;
        return argind < parent.argc ? 1 : 0;
    }

    /// \brief The current element of argv.
    char const *get() const {
        return parent.argv[argind];
    }

    /// \brief The index in argv of current element of argv.
    int index() const {
        return argind;
    }

    /// \brief Switch to shortArg mode and return the current parameter.
    char const *getChar() {
        if (indarg == 0)
            indarg = 1;
        return get() + indarg;
    }

    /// \brief End shortArg mode and advance to next parameter.
    void advance() {
        assert(indarg > 0);
        indarg = 0;
        ++argind;
    }
};

template <typename INDEX>
struct CharIndexElement : public std::pair<char, INDEX>
{
    using Base = std::pair<char, INDEX>;

    CharIndexElement(CharIndexElement const &other) = default;
    CharIndexElement(Base const &base)
    : Base(base)
    {};
    
    bool operator< (char query) const {
        return this->first < query;
    }
    friend bool operator< (char const &a, CharIndexElement const &b) {
        return a < b.first;
    }
};

/// \brief Indexed Argument Definitions for a tool.
struct ParamDefinitions {
    using Container = UniqueOrderedList<ParameterDefinition>;
    using LongIndex = std::map<std::string, Container::Index>;
    using ShortIndexElement = CharIndexElement<Container::Index>;
    using ShortIndex = UniqueOrderedList<ShortIndexElement>;
    
    std::string tool;
    Container container;
    ShortIndex shortIndex;
    
    bool contains(ParameterDefinition const &def) const {
        auto const fnd = container.find(def);
        return fnd.first != fnd.second;
    }

    /// \brief Find the index of the definition.
    ///
    /// Also, returns pointer to parameter's argument if it is attached to the string.
    std::pair<int, char const *> findLong(char const *const arg) const
    {
        auto const fnd = container.find(arg);
        if (fnd.first != fnd.second)
            return {container.begin() - fnd.first, nullptr};

        auto const eq = strchr(arg, '=');
        if (eq) {
            auto const fnd = container.find(std::string(arg, eq - arg));
            if (fnd.first != fnd.second)
                return {container.begin() - fnd.first, eq + 1};
        }
        
        return {-1, nullptr};
    }

    /// \brief Update indices and assign bit masks.
    void finalize() {
        auto i = 0;
        uint64_t mask = 1;
        for (auto &def : container) {
            if (def.aliases) {
                for (auto ch : std::string(def.aliases)) {
                    shortIndex.insert(ShortIndexElement({ch, i}));
                }
            }
            if (!commonParams.contains(def)) {
                def.bitMask = mask;
                mask <<= 1;
            }
            ++i;
        }
    }

    Arguments parseArgv(ArgvIterator &) const;
    static Arguments parseArgvForFastqDump(ArgvIterator &);
    
    /// \brief Parse the next parameter/argument from argv and add it to the container
    ///
    /// \returns false when done.
    bool parseArg(Arguments::Container &dst, ArgvIterator &i) const;
    
    static ParamDefinitions const commonParams;
    
#define TOOL_DEFINE(NAME) \
    static ParamDefinitions make_ ## NAME () { \
        ParameterDefinition const defs[] = TOOL_ARGS_ ## NAME ; \
        ParamDefinitions result = {TOOL_NAME_ ## NAME, Container(sizeof(defs)/sizeof(defs[0]) - 1 + commonParams.container.size())} ; \
        for (auto &def : commonParams.container) result.container.insert(def); \
        for (auto def = defs; def->name != nullptr; ++def) \
            result.container.insert(*def); \
        result.finalize(); \
        return result; \
    }
    
    /// MARK: add tools here
    TOOL_DEFINE(FASTERQ_DUMP)
    TOOL_DEFINE(SAM_DUMP)
    TOOL_DEFINE(SRA_PILEUP)
    TOOL_DEFINE(VDB_DUMP)

#undef TOOL_DEFINE
    
    static inline ParamDefinitions toolFor(std::string const &name) {
        /// MARK: and here
        if (name == TOOL_NAME_FASTERQ_DUMP)
            return make_FASTERQ_DUMP();
        if (name == TOOL_NAME_SAM_DUMP)
            return make_SAM_DUMP();
        if (name == TOOL_NAME_SRA_PILEUP)
            return make_SRA_PILEUP();
        if (name == TOOL_NAME_VDB_DUMP)
            return make_VDB_DUMP();
        throw UnknownToolException();
    }
    static inline ParamDefinitions makeCommonParams() {
        static ParameterDefinition const defs[] = TOOL_ARGS_COMMON;
        ParamDefinitions result;
        
        for (auto def = defs; def->name != nullptr; ++def)
            result.container.insert(*def);

        return result;
    }
};

ParamDefinitions const ParamDefinitions::commonParams(ParamDefinitions::makeCommonParams());

bool ParamDefinitions::parseArg(Arguments::Container &dst, ArgvIterator &i) const
{
    auto nextIsArg = 0;
    auto index = -1;

    for ( ; ; ) {
        switch (i.next()) {
        case 0:
            if (nextIsArg) {
                dst.emplace_back(Argument({container[index], nullptr, -1}));
                return true;
            }
            return false;
        case 1:
            if (nextIsArg) {
                dst.emplace_back(Argument({container[index], i.get(), i.argind - 1}));
                return true;
            }
            else {
                auto const arg = i.get();
                
                if (arg[0] != '-') {
                    dst.emplace_back(Argument({container[index], arg, i.argind}));
                    return true;
                }
                if (arg[1] == '-') {
                    auto const f = findLong(arg + 2);
                    if (f.first >= 0) {
                        index = f.first;
                        auto const &def = container[index];
                        if (f.second && def.hasArgument) {
                            dst.emplace_back(Argument({def, f.second, i.argind}));
                            return true;
                        }
                        if (!f.second && !def.hasArgument) {
                            dst.emplace_back(Argument({def}));
                            return true;
                        }
                        if (def.hasArgument) {
                            ++nextIsArg;
                            continue;
                        }
                    }
                    dst.emplace_back(Argument({ParameterDefinition::unknownParameter(), arg, i.argind}));
                    return true;
                }
            }
            // fallthrough;
        case -1:
            {
                auto const arg = i.getChar();
                switch (nextIsArg) {
                case 0:
                    {
                        auto const f = shortIndex.find(*arg);
                        if (f.first != f.second) {
                            index = f.first->second;
                            auto const &def = container[index];
                            if (def.hasArgument) {
                                ++nextIsArg;
                                continue;
                            }
                            dst.emplace_back(Argument({def, arg, i.argind}));
                            return true;
                        }
                        dst.emplace_back(Argument({ParameterDefinition::unknownParameter(), arg, i.argind}));
                        return true;
                    }
                case 1:
                    if (*arg == '=') {
                        ++nextIsArg;
                        continue;
                    }
                    // fallthrough;
                case 2:
                    dst.emplace_back(Argument({container[index], arg, i.argind}));
                    i.advance();
                    return true;
                default:
                    assert(!"reachable");
                }
            }
            break;
        default:
            assert(!"reachable");
        }
    }
}

Arguments ParamDefinitions::parseArgv(ArgvIterator &iter) const
{
    Arguments::Container result;
    uint64_t argsHash = 0;

    result.reserve(iter.parent.argc);
    
    while (parseArg(result, iter))
        ;

    for (auto const &used : result)
        argsHash |= used.def.bitMask;
    
    return Arguments(result, argsHash);
}

class FastqDumpParameterInfo {
public:
    struct Definition {
        ParameterDefinition def;
        bool argumentIsRequired;
        bool argumentIsOptional;
    };
private:
    std::array<Definition, 64> definitionTable;
    std::array<Definition, 64>::iterator end;
    unsigned shortToLong[256];
public:
    Definition const &operator[](std::string const &name) const {
        auto const i = lowerBound(definitionTable, name, [](Definition const &a, std::string const &b) {
            return !(b <= a.def.name);
        });
        return i->def.name == name ? *i : *end;
    }
    Definition const &operator[](char ch) const {
        return ch > 0 ? definitionTable[shortToLong[unsigned(ch)]] : *end;
    }
    FastqDumpParameterInfo()
    {
        // in alphabetical order
        static constexpr char const *longNames[] {
            "accession",
            "clip",
            "dumpbase",
            "dumpcs",
            "group-in-dirs",
            "keep-empty-files",
            "log-level",
            "maxSpotId",
            "minReadLen",
            "minSpotId",
            "offset",
            "origfmt",
            "outdir",
            "qual-filter",
            "read-filter",
            "readids",
            "spot-group",
            "stdout",
            "version",
            nullptr
        };
        /// Note: 1-to-1 correspondence with longNames array
        static constexpr char aliases[] = {
            'A', '\0',
            'W', '\0',
            'B', '\0',
            'C', '\0',
            'T', '\0',
            'K', '\0',
            'L', '\0',
            'X', '\0',
            'M', '\0',
            'N', '\0',
            'Q', '\0',
            'F', '\0',
            'O', '\0',
            'E', '\0',
            'R', '\0',
            'I', '\0',
            'G', '\0',
            'Z', '\0',
            'V', '\0',
            '\0'
        };
        // in alphabetical order
        static constexpr char const *const required[] = {
            "accession",
            "aligned-region",
            "defline-qual",
            "defline-seq",
            "matepair-distance",
            "maxSpotId",
            "minReadLen",
            "minSpotId",
            "offset",
            "outdir",
            "read-filter",
            "spot-groups",
            "table",
            nullptr
        };
        // in alphabetical order
        static constexpr char const *const optional[] = {
            "dumpcs",
            "fasta",
            nullptr
        };
        auto il = 0; /// iterates over longNames
        auto ir = 0; /// iterates over required
        auto io = 0; /// iterates over optional

        end = definitionTable.begin();
        for ( ; ; ) {
            auto next = longNames[il];
            if (next == nullptr || (required[ir] != nullptr && strcmp(next, required[ir]) > 0))
                next = required[ir];
            if (next == nullptr || (optional[io] != nullptr && strcmp(next, optional[io]) > 0))
                next = optional[io];
            auto const alias = (longNames[il] && strcmp(next, longNames[il]) == 0) ? &aliases[il * 2] : nullptr;
            auto const argReq = (required[ir] && strcmp(next, required[ir]) == 0);
            auto const argOpt = (optional[io] && strcmp(next, optional[io]) == 0);

            assert(alias != nullptr || argReq || argOpt); // we are only keeping track of the minimum
            *end++ = { {next, alias, argReq || argOpt }, argReq, argOpt };
            *end = { {nullptr, nullptr, false}, false, false };
            assert(iterDistance(definitionTable.begin(), end) < 64);

            if (alias)
                ++il;
            if (argReq)
                ++ir;
            if (argOpt)
                ++io;
            if (longNames[il] == nullptr && required[ir] == nullptr && optional[io] == nullptr)
                break;
        }

        auto const n = iterDistance(definitionTable.begin(), end);
        for (auto i = 0; i < 256; ++i)
            shortToLong[i] = n;
        for (auto i = 0; i != n; ++i) {
            auto const alias = definitionTable[i].def.aliases;
            if (alias)
                shortToLong[(uint8_t)(alias[0])] = i;
        }
        // spot checks
        assert(definitionTable[shortToLong[(unsigned)'A']].def == "accession");
        assert(definitionTable[shortToLong[(unsigned)'V']].def == "version");
        assert(definitionTable[shortToLong[(unsigned)'Z']].def == "stdout");
    }
};
static FastqDumpParameterInfo const fastqDumpParameterInfo;

Arguments ParamDefinitions::parseArgvForFastqDump(ArgvIterator &iter)
{
    auto const &none = fastqDumpParameterInfo['\0'];
    Arguments::Container result;
    auto current = fastqDumpParameterInfo['\0'];

    result.reserve(iter.parent.argc);
    
    for ( ; ; ) {
        switch (iter.next()) {
        case 1:
            {
                auto const arg = iter.get();
                if (current.argumentIsRequired) {
                    // it is unconditionally an argument
                    assert(!!current.def);
                    result.emplace_back(Argument({current.def, arg, iter.argind - 1}));
                    current = none;
                    break;
                }
                if (arg[0] == '-') {
                    if (current.argumentIsOptional) {
                        // the optional argument did not show up
                        result.emplace_back(Argument({current.def, nullptr, iter.argind - 1}));
                        current = none;
                    }
                    if (arg[1] == '-') {
                        // it's a long parameter
                        current = fastqDumpParameterInfo[arg + 2];
                        if (!current.def) {
                            // long parameters can be unknown, but not if they take an argument
                            result.emplace_back(Argument({ParameterDefinition({arg + 2}), nullptr, iter.argind}));
                            current = none;
                            break;
                        }
                    }
                    else {
                        // it's a short parameter
                        current = fastqDumpParameterInfo[arg[1]];
                        assert(!!current.def);
                    }
                    if (current.argumentIsOptional || current.argumentIsRequired)
                        /* expect an argument */;
                    else {
                        result.emplace_back(Argument({current.def, nullptr, iter.argind}));
                        current = none;
                    }
                    break;
                }
                if (current.argumentIsOptional) {
                    // this is the optional argument
                    result.emplace_back(Argument({current.def, arg, iter.argind - 1}));
                }
                else {
                    result.emplace_back(Argument({ParameterDefinition::argument(), arg, iter.argind}));
                }
                current = none;
            }
            break;
        default:
            if (current.argumentIsRequired || current.argumentIsOptional) {
                // the optional? argument did not show up
                result.emplace_back(Argument({current.def, nullptr, iter.argind - 1}));
            }
            return Arguments(result, 0);
        }
    }
}

Arguments argumentsParsed(CommandLine const &cmdLine)
{
    auto iter = ArgvIterator(cmdLine);
    if (cmdLine.toolName == "fastq-dump")
        return ParamDefinitions::parseArgvForFastqDump(iter);
    return ParamDefinitions::toolFor(cmdLine.toolName).parseArgv(iter);
}

std::ostream &operator <<(std::ostream &out, Argument const &arg) {
    if (arg.isArgument())
        return out << arg.argument;
    if (!arg.def.hasArgument)
        return out << arg.def.name;
    if (!arg.argument)
        return out << arg.def.name << " (null)";
    else
        return out << arg.def.name << " " << arg.argument;
}
