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
#include <cinttypes>

#include "globals.hpp"
#include "debug.hpp"
#include "tool-args.hpp"
#include "command-line.hpp"
#include "build-version.hpp"

#if USE_TOOL_HELP
#define TOOL_HELP(...) {__VA_ARGS__}
#else
#define TOOL_HELP(...) {}
#endif
#define TOOL_ARGS(...) {__VA_ARGS__}
#if USE_TOOL_HELP
#define TOOL_ARG(LONG, ALIAS, ARG, HELP) {HELP, LONG, ALIAS, 0, ARG, false}
#define FQD_TOOL_ARG(LONG, ALIAS, ARG, HELP) {HELP, LONG, ALIAS, 0, (ARG) != 0, (ARG) < 0}
#else
#define TOOL_ARG(LONG, ALIAS, ARG, HELP) {LONG, ALIAS, 0, ARG, false}
#define FQD_TOOL_ARG(LONG, ALIAS, ARG, HELP) {LONG, ALIAS, 0, (ARG) != 0, (ARG) < 0}
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

#include "fastq-dump-arguments.h"
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

struct CharIndexElement : public std::pair<char, unsigned>
{
    using Base = std::pair<char, unsigned>;

    CharIndexElement(CharIndexElement const &other) = default;
    CharIndexElement(Base const &base)
    : Base(base)
    {};
    
    bool operator< (char const &query) const {
        return first < query;
    }
    friend bool operator< (char const &a, CharIndexElement const &b) {
        return a < b.first;
    }
};

/// \brief Indexed Argument Definitions for a tool.
struct ParamDefinitions_Common {
    using Container = UniqueOrderedList<ParameterDefinition>;
    using LongIndex = std::map<std::string, Container::Index>;
    using ShortIndex = UniqueOrderedList<CharIndexElement>;

    static ParamDefinitions_Common const &commonParams;
    
    std::string tool;
    Container container;
    ShortIndex shortIndex;

    ParamDefinitions_Common(std::string const &name, size_t capacity)
    : tool(name)
    , container(capacity)
    {}

    /// \Returns true if the collection of definitions contains the query definition.
    bool contains(ParameterDefinition const &def) const {
        auto const fnd = container.find(def);
        return fnd.first != fnd.second;
    }
    
    /// \brief Update indices and assign bit masks.
    void finalize() {
        auto i = 0;
        uint64_t mask = 1;
        for (auto &def : container) {
            if (def.aliases) {
                for (auto ch : std::string(def.aliases)) {
                    shortIndex.insert(CharIndexElement({ch, i}));
                }
            }
            if (!commonParams.contains(def)) {
                def.bitMask = mask;
                mask <<= 1;
            }
            ++i;
        }
    }
    
    /** Print tool's parameter bits; format is tab-delimited.
     *
     * Fields: tool name, '(' bit shift ')' decimal value, parameter long name
     */
    void printParameterBitmasks(std::ostream &out) const {
        char buffer[32]; // e.g. "0x0000000000000001 (1 <<  0)"
        
        for (auto &def : container) {
            if (def.isArgument()) // obviously, doesn't apply to tool arguments.
                continue;
            if (def.bitMask == 0) // common tool parameters don't have bits assigned to them.
                continue;

            int shift = 0;
            uint64_t mask = def.bitMask;
            
            while (mask > 1) {
                mask >>= 1;
                shift += 1;
            }
            
            auto const n = snprintf(buffer, sizeof(buffer), "0x%016" PRIx64 " (1 << %2u)", def.bitMask, shift);
            assert(n <= sizeof(buffer));
            out << tool << '\t'
                << sratools::Version::current << '\t'
                << buffer << '\t'
                << def.name << '\n';
        }
        out << std::flush;
    }

    /// \brief Find the index of the definition.
    ///
    /// \Returns index of definition and a pointer to parameter's argument if it is attached to the string.
    virtual std::pair< int, char const * > findLong(char const *arg) const = 0;

    virtual bool parseArg(Arguments::Container &dst, ArgvIterator &iter) const = 0;
    
    Arguments parseArgv(ArgvIterator &iter) const {
        Arguments::Container result;
        uint64_t argsHash = 0;

        result.reserve(iter.parent.argc);
        
        while (parseArg(result, iter))
            ;

        for (auto const &used : result)
            argsHash |= used.def->bitMask;
        
        return Arguments(result, argsHash);
    }
};

#define TOOL_DEFINE(SELF, NAME) \
    static inline SELF make_ ## NAME () { \
        ParameterDefinition const defs[] = TOOL_ARGS_ ## NAME ; \
        auto result = SELF(TOOL_NAME_ ## NAME, sizeof(defs)/sizeof(defs[0]) - 1 + commonParams.container.size()); \
        for (auto &def : commonParams.container) result.container.insert(def); \
        for (auto def = defs; def->name != nullptr; ++def) \
            result.container.insert(*def); \
        result.finalize(); \
        return result; \
    }


struct ParamDefinitions final : public ParamDefinitions_Common
{
    ParamDefinitions(std::string const &name, size_t capacity)
    : ParamDefinitions_Common(name, capacity)
    {}
    
    std::pair< int, char const * > findLong(char const *const arg) const override
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
    
    bool parseArg(Arguments::Container &dst, ArgvIterator &i) const override {
        auto nextIsArg = 0;
        auto index = -1;

        for ( ; ; ) {
            switch (i.next()) {
            case 0:
                if (nextIsArg) {
                    dst.emplace_back(Argument({&container[index], nullptr, -1}));
                    return true;
                }
                return false;
            case 1:
                if (nextIsArg) {
                    dst.emplace_back(Argument({&container[index], i.get(), i.argind - 1}));
                    return true;
                }
                else {
                    auto const arg = i.get();
                    
                    if (arg[0] != '-') {
                        dst.emplace_back(Argument({&ParameterDefinition::argument(), arg, i.argind}));
                        return true;
                    }
                    if (arg[1] == '-') {
                        auto const f = findLong(arg + 2);
                        if (f.first >= 0) {
                            index = f.first;
                            auto const &def = container[index];
                            if (f.second && def.hasArgument) {
                                dst.emplace_back(Argument({&def, f.second, i.argind}));
                                return true;
                            }
                            if (!f.second && !def.hasArgument) {
                                dst.emplace_back(Argument({&def, nullptr, i.argind}));
                                return true;
                            }
                            if (def.hasArgument) {
                                ++nextIsArg;
                                continue;
                            }
                        }
                        dst.emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, i.argind}));
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
                                dst.emplace_back(Argument({&def, arg, i.argind}));
                                return true;
                            }
                            dst.emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, i.argind}));
                            return true;
                        }
                    case 1:
                        if (*arg == '=') {
                            ++nextIsArg;
                            continue;
                        }
                        // fallthrough;
                    case 2:
                        dst.emplace_back(Argument({&container[index], arg, i.argind}));
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

    static inline ParamDefinitions makeCommonParams() {
        static ParameterDefinition const defs[] = TOOL_ARGS_COMMON;
        ParamDefinitions result("", sizeof(defs)/sizeof(defs[0]) - 1);
        
        for (auto def = defs; def->name != nullptr; ++def)
            result.container.insert(*def);

        return result;
    }
    
    TOOL_DEFINE(ParamDefinitions, FASTERQ_DUMP)
    TOOL_DEFINE(ParamDefinitions, SAM_DUMP)
    TOOL_DEFINE(ParamDefinitions, VDB_DUMP)
    TOOL_DEFINE(ParamDefinitions, SRA_PILEUP)
    
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
};

ParamDefinitions_Common const &ParamDefinitions_Common::commonParams = ParamDefinitions::makeCommonParams();

struct ParamDefinitions_FQD : public ParamDefinitions_Common
{
    ParamDefinitions_FQD(std::string const &name, size_t capacity)
    : ParamDefinitions_Common(name, capacity)
    {}
    
    std::pair< int, char const * > findLong(char const *const arg) const override
    {
        auto const fnd = container.find(arg);
        if (fnd.first != fnd.second)
            return {container.begin() - fnd.first, nullptr};

        return {-1, nullptr};
    }
    bool parseArg(Arguments::Container &result, ArgvIterator &iter) const override {
        int index = -1;
        bool nextMayBeArg = false;
        bool nextMustBeArg = false;
        
        for ( ; ; ) {
            switch (iter.next()) {
            case 0:
                if (nextMayBeArg) {
                    // optional argument did not show up
                    assert(index >= 0 && index < container.size());
                    result.emplace_back(Argument({&container[index], nullptr, iter.argind - 1}));
                }
                return false;
            case 1:
                {
                    auto const arg = iter.get();
                    if (nextMustBeArg || (arg[0] != '-' && nextMayBeArg)) {
                        assert(index >= 0 && index < container.size());
                        result.emplace_back(Argument({&container[index], arg, iter.argind - 1}));
                        return true;
                    }
                    if (arg[0] == '-') {
                        if (nextMayBeArg) {
                            // optional argument did not show up
                            assert(index >= 0 && index < container.size());
                            result.emplace_back(Argument({&container[index], nullptr, iter.argind - 1}));
                        }
                        if (arg[1] == '-') {
                            auto const f = findLong(arg + 2);
                            if (f.first < 0) {
                                result.emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, iter.argind}));
                                return true;
                            }
                            index = f.first;
                        }
                        else {
                            auto const f = shortIndex.find(arg[1]);
                            if (f.first == f.second) {
                                result.emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, iter.argind}));
                                return true;
                            }
                            index = f.first->second;
                        }
                        auto const &def = container[index];
                        if (!def.hasArgument) {
                            result.emplace_back(Argument({&def, nullptr, iter.argind}));
                            return true;
                        }
                        nextMayBeArg = true;
                        nextMustBeArg = !def.argumentIsOptional;
                        continue;
                    }
                    result.emplace_back(Argument({&ParameterDefinition::argument(), arg, iter.argind}));
                    return true;
                }
                break;
            }
        }
        return false;
    }
    TOOL_DEFINE(ParamDefinitions_FQD, FASTQ_DUMP)
};

Arguments argumentsParsed(CommandLine const &cmdLine)
{
    auto iter = ArgvIterator(cmdLine);
    if (cmdLine.toolName == "fastq-dump")
        return ParamDefinitions_FQD::make_FASTQ_DUMP().parseArgv(iter);
    return ParamDefinitions::toolFor(cmdLine.toolName).parseArgv(iter);
}

std::ostream &operator <<(std::ostream &out, Argument const &arg) {
    if (arg.isArgument())
        return out << arg.argument;
    if (!arg.def->hasArgument)
        return out << arg.def->name;
    if (!arg.argument)
        return out << arg.def->name << " (null)";
    else
        return out << arg.def->name << " " << arg.argument;
}

void printParameterBitmasks(std::ostream &out) {
    ParamDefinitions_FQD::make_FASTQ_DUMP().printParameterBitmasks(out);
    ParamDefinitions::make_FASTERQ_DUMP().printParameterBitmasks(out);
    ParamDefinitions::make_SAM_DUMP().printParameterBitmasks(out);
    ParamDefinitions::make_SRA_PILEUP().printParameterBitmasks(out);
    ParamDefinitions::make_VDB_DUMP().printParameterBitmasks(out);
}
