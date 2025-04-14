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

#include "common-arguments.h"
#include "fastq-dump-arguments.h"
#include "tool-arguments.h"

class JSON_Printer {
    std::ostream &out;
    int indent;
    bool indentNext;
    char indentChar;
    std::string state;
    
    void indentIf() {
        if (indentNext) {
            for (auto i = 0; i < indent; ++i)
                out << indentChar;
            indentNext = false;
        }
    }
public:
    
    JSON_Printer()
    : out(std::cout)
    , indent(0)
    , indentNext(false)
    , indentChar('\t')
    {}

    explicit JSON_Printer(std::ostream &a_out, int a_indent = 0, bool a_indentNext = false, char a_indentChar = '\t')
    : out(a_out)
    , indent(a_indent)
    , indentNext(a_indentNext)
    , indentChar(a_indentChar)
    {}

    template <typename T>
    void print(T x) {
        out << x;
    }
    void print(char ch) {
        switch (ch) {
        case '[':
        case '{':
            indentIf();
            ++indent;
            break;
        case '}':
        case ']':
            --indent;
            indentIf();
            break;
        case '\n':
            indentNext = true;
            break;
        default:
            indentIf();
            break;
        }
        out << ch;
    }
    void print(char const *str)
    {
        for (auto cp = str; *cp; ++cp)
            print(*cp);
    }
    void print(std::string const &str) {
        print(str.c_str());
    }
    template <typename T, typename ...Ts>
    void print(T const &x, Ts... ts) {
        print(x);
        print(ts...);
    }
    void print(ParameterDefinition const *def) {
        print("{ \"name\": \"", def->name, "\"");
        if (def->aliases && *def->aliases) {
            print(", \"alias\": [ ");
            for (auto cp = def->aliases; *cp; ++cp) {
                if (cp != def->aliases)
                    print(", ");
                print('"', *cp, '"');
            }
            print(" ]");
        }
        if (def->hasArgument && def->argumentIsOptional)
            print(", \"argument-optional\": true");
        else if (def->hasArgument && !def->argumentIsOptional)
            print(", \"argument-required\": true");
        print(" }");
    }
};

/// Used by `ParamDefinitions` to traverse the command line arguments.
struct ArgvIterator {
private:
    CommandLine const *parent;
    mutable int argind;
    mutable int indarg;
public:
    ArgvIterator(CommandLine const &parent)
    : parent(&parent)
    , argind(0)
    , indarg(0)
    {}

    /// \brief move to next parameter
    ///
    /// In normal mode, moves to next argv element, if any.
    /// In shortArg mode, moves to next short arg, or switches back to normal mode if no more shortArgs.
    ///
    /// \returns 0 if there are no more, 1 if in normal mode, -1 if in shortArg mode.
    int next() const {
        if (argind == parent->argc) return 0;
        if (indarg > 0) {
            ++indarg;
            if (parent->argv[argind][indarg] != '\0')
                return -1;
            indarg = 0;
        }
        ++argind;
        return argind < parent->argc ? 1 : 0;
    }

    /// \brief The current element of argv.
    char const *get() const {
        return parent->argv[argind];
    }

    /// \brief The index in argv of current element of argv.
    int index() const {
        return argind;
    }

    /// \brief Switch to shortArg mode and return the current parameter.
    char const *getChar() const {
        if (indarg == 0)
            indarg = 1;
        return get() + indarg;
    }

    /// \brief End shortArg mode and advance to next parameter.
    void advance() const {
        assert(indarg > 0);
        indarg = 0;
    }
    friend std::ostream &operator <<(std::ostream &out, ArgvIterator const &arg) {
        auto const argc = arg.parent->argc;
        auto const argind = arg.argind;
        auto const indarg = arg.indarg;
        if (argind == argc)
            return out << argind << "/" << argc << "(at end)";
        auto const val = arg.parent->argv[argind];
        if (indarg > 0)
            return out << argind << "." << indarg << "/" << argc << " '" << val[indarg] << "'";
        return out << argind << "/" << argc << " '" << val << "'";
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

/// \brief Base class for Argument Definitions, contains the indexed definitions and common implementation for a tool.
struct ParamDefinitions_Common {
    using Container = UniqueOrderedList<ParameterDefinition>;
    using LongIndex = std::map<std::string, Container::Index>;
    using ShortIndex = UniqueOrderedList<CharIndexElement>;

    Container container;
    ShortIndex shortIndex;

    virtual ~ParamDefinitions_Common() {}

    ParamDefinitions_Common(size_t count, ParameterDefinition const *defs)
    : container(count)
    {
        for (auto cur = defs; cur != defs + count; ++cur)
            container.insert(*cur);
    }

    ParamDefinitions_Common(ParamDefinitions_Common const &common, size_t count, ParameterDefinition const *defs)
    : container(common.container.size() + count)
    {
        for (auto &def : common.container)
            container.insert(def);

        for (auto cur = defs; cur != defs + count; ++cur)
            container.insert(*cur);

        // Update indices and assign bit masks.
        unsigned i = 0;
        uint64_t mask = 1;
        for (auto &def : container) {
            if (def.aliases) {
                for (auto const &ch : std::string(def.aliases))
                    shortIndex.insert(CharIndexElement({ch, i}));
            }
            if (!common.container.contains(def)) {
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
    void printParameterBitmasks(std::string const &tool, std::ostream &out) const {
        char buffer[] = "0x0000000000000001 (1 <<  0)";
        char *numAt = buffer + 17, *const shiftAt = numAt + 9;
        unsigned shift = 0;

        assert(numAt[0] == '1' && numAt[1] == ' ');
        assert(shiftAt[0] == '0' && shiftAt[1] == ')');
        for (auto &def : container) {
            if (def.isArgument()) // obviously, doesn't apply to tool arguments.
                continue;
            if (def.bitMask == 0) // common tool parameters don't have bits assigned to them.
                continue;

            *numAt = "1248"[shift % 4];
            assert(std::stoul(buffer, nullptr, 0) == def.bitMask);

            if (shift > 9)
                shiftAt[-1] = (shift / 10) % 10 + '0';
            shiftAt[0] = shift % 10 + '0';

            out << tool << '\t'
                << sratools::Version::currentString << '\t'
                << buffer << '\t'
                << def.name << '\n';

            ++shift;
            if ((shift % 4) == 0)
                *numAt-- = '0';
            shiftAt[-1] = shiftAt[0] = ' ';
        }
        out << std::flush;
    }

    void printParameterJSON(std::string const &tool, JSON_Printer &out, bool first = false) const {
        if (!first)
            out.print(",\n");
        out.print("{\n",
            "\"name\": \"", tool, "\",\n" \
            "\"version\": \"", sratools::Version::currentString, "\",\n" \
            "\"parameters\": [\n");
        
        auto firstArg = true;
        for (auto &def : container) {
            if (def.isArgument())
                continue;
            if (!firstArg)
                out.print(",\n");
            firstArg = false;
            out.print(&def);
        }
        out.print("\n]\n}");
    }

    /// @brief There are two flavors of args parsing, one for fastq-dump and one for all the others.
    /// @param dst The container that will hold the parsed arguments.
    /// @param iter An iterator on argv.
    /// @return false if no progress could be made, e.g. all argument have beed parsed.
    virtual bool parseArg(Arguments::Container *dst, ArgvIterator const &iter) const = 0;

    /// @brief Parse all arguments from the command line.
    /// @param cmdLine the command line to be parsed.
    /// @return a container with all the parsed arguments.
    /// @note Generally, this function is infallible. However, if the command line doesn't
    ///       match the definitions, it is a garbage-in/garbage-out situation.
    Arguments parse(CommandLine const &cmdLine) const {
        Arguments::Container result;
        TRACE(cmdLine.argc);
        auto iter = ArgvIterator(cmdLine);

        result.reserve(cmdLine.argc);

        while (parseArg(&result, iter))
            ;

        uint64_t argsHash = 0;

        for (auto const &used : result)
            argsHash |= used.def->bitMask;

        return Arguments(result, argsHash);
    }
};

/// @brief This is used for all tools other than fastq-dump, i.e. the tools that use klib args parsing.
struct ParamDefinitions final : public ParamDefinitions_Common
{
private:
    ParamDefinitions(size_t count, ParameterDefinition const *defs)
    : ParamDefinitions_Common(count, defs)
    {}
public:
    ParamDefinitions(ParamDefinitions_Common const &common, size_t count, ParameterDefinition const *defs)
    : ParamDefinitions_Common(common, count, defs)
    {}

    /// \brief Find the index of the definition.
    ///
    /// \Returns index of definition and a pointer to parameter's argument if it is attached to the string.
    std::pair< int, char const * > findLong(char const *const param) const
    {
        auto const eq = strchr(param, '=');
        auto const arg = eq ? (eq + 1) : nullptr;
        auto const paramName = eq ? std::string(param, (char const *)eq) : std::string(param);
        auto const fnd = container.find(paramName);

        if (fnd.first != fnd.second)
            return {iterDistance(container.begin(), fnd.first), arg};

        return {-1, nullptr};
    }

    /// @brief Args parsing that is compatible with klib's args parser.
    /// @param dst the container to recieve the argument and possibly its parameter.
    /// @param i the iterator on argv.
    /// @return false if done.
    bool parseArg(Arguments::Container *dst, ArgvIterator const &i) const override {
        auto nextIsArg = 0;
        auto index = -1;

        TRACE(i);
        for ( ; ; ) {
            switch (i.next()) {
            case 0:
                TRACE(nextIsArg);
                if (nextIsArg) {
                    TRACE(index);
                    dst->emplace_back(Argument({&container[index], nullptr, -1}));
                    TRACE_OUT << "added parameter: " << dst->back() << std::endl;
                    return true;
                }
                return false;
            case 1:
                TRACE(nextIsArg);
                if (nextIsArg) {
                    TRACE(index);
                    dst->emplace_back(Argument({&container[index], i.get(), i.index() - 1}));
                    TRACE_OUT << "added parameter: " << dst->back() << std::endl;
                    return true;
                }
                else {
                    auto const arg = i.get();

                    TRACE(arg);
                    TRACE(i.index());
                    if (arg[0] != '-') {
                        dst->emplace_back(Argument({&ParameterDefinition::argument(), arg, i.index()}));
                        TRACE_OUT << "added argument: " << dst->back() << std::endl;
                        return true;
                    }
                    if (arg[1] == '-') {
                        auto const f = findLong(arg + 2);
                        if (f.first >= 0) {
                            index = f.first;
                            TRACE(index);
                            auto const &def = container[index];
                            TRACE(def);
                            if (f.second && def.hasArgument) {
                                dst->emplace_back(Argument({&def, f.second, i.index()}));
                                TRACE_OUT << "added parameter: " << dst->back() << std::endl;
                                return true;
                            }
                            if (!f.second && !def.hasArgument) {
                                dst->emplace_back(Argument({&def, nullptr, i.index()}));
                                TRACE_OUT << "added parameter: " << dst->back() << std::endl;
                                return true;
                            }
                            if (def.hasArgument) {
                                TRACE_OUT << "looking for argument" << std::endl;
                                ++nextIsArg;
                                continue;
                            }
                        }
                        TRACE_OUT << "unknown parameter: " << arg << ", index: " << i.index() << std::endl;
                        dst->emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, i.index()}));
                        return true;
                    }
                }
                TRACE_OUT << "parsing short argument" << std::endl;
                // fallthrough;
            case -1:
                {
                    auto const arg = i.getChar();
                    TRACE(arg);
                    TRACE(nextIsArg);
                    switch (nextIsArg) {
                    case 0:
                        {
                            auto const f = shortIndex.find(*arg);
                            if (f.first != f.second) {
                                index = f.first->second;
                                TRACE(index);
                                auto const &def = container[index];
                                TRACE(def);
                                if (def.hasArgument) {
                                    TRACE_OUT << "looking for argument" << std::endl;
                                    ++nextIsArg;
                                    continue;
                                }
                                dst->emplace_back(Argument({&def, arg, i.index()}));
                                TRACE_OUT << "added parameter: " << dst->back() << std::endl;
                                return true;
                            }
                            TRACE_OUT << "unknown parameter: " << arg << ", index: " << i.index() << std::endl;
                            dst->emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, i.index()}));
                            return true;
                        }
                    case 1:
                        if (*arg == '=') {
                            TRACE_OUT << "looking for argument" << std::endl;
                            ++nextIsArg;
                            continue;
                        }
                        // fallthrough;
                    case 2:
                        dst->emplace_back(Argument({&container[index], arg, i.index()}));
                        TRACE_OUT << "added parameter: " << dst->back() << std::endl;
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
        return ParamDefinitions(sizeof(defs)/sizeof(defs[0]) - 1, defs);
    }
};

/// @brief This is used for fastq-dump, which uses its own args parsing.
struct ParamDefinitions_FQD final : public ParamDefinitions_Common
{
    ParamDefinitions_FQD(ParamDefinitions_Common const &common, size_t count, ParameterDefinition const *defs)
    : ParamDefinitions_Common(common, count, defs)
    {}

    /// @brief Args parsing that is unique to fastq-dump. Major differences are optional parameters and parameters must be seperated from the argument.
    /// @param dst the container to recieve the argument and possibly its parameter.
    /// @param i the iterator on argv.
    /// @return false if done.
    bool parseArg(Arguments::Container *result, ArgvIterator const &iter) const override {
        int index = -1;
        bool nextMayBeArg = false;
        bool nextMustBeArg = false;

        for ( ; ; ) {
            switch (iter.next()) {
            case 0:
                if (nextMayBeArg) {
                    // optional argument did not show up
                    assert(index >= 0 && index < (int)container.size());
                    result->emplace_back(Argument({&container[index], nullptr, iter.index() - 1}));
                }
                return false;
            case 1:
                {
                    auto const arg = iter.get();
                    if (nextMustBeArg || (arg[0] != '-' && nextMayBeArg)) {
                        assert(index >= 0 && index < (int)container.size());
                        result->emplace_back(Argument({&container[index], arg, iter.index() - 1}));
                        return true;
                    }
                    if (arg[0] == '-') {
                        if (nextMayBeArg) {
                            // optional argument did not show up
                            assert(index >= 0 && index < (int)container.size());
                            result->emplace_back(Argument({&container[index], nullptr, iter.index() - 1}));
                        }
                        if (arg[1] == '-') {
                            auto const f = container.find(arg + 2);
                            if (f.first == f.second) {
                                result->emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, iter.index()}));
                                return true;
                            }
                            index = (int)iterDistance(container.begin(), f.first);
                        }
                        else {
                            auto const f = shortIndex.find(arg[1]);
                            if (f.first == f.second) {
                                result->emplace_back(Argument({&ParameterDefinition::unknownParameter(), arg, iter.index()}));
                                return true;
                            }
                            index = f.first->second;
                        }
                        auto const &def = container[index];
                        if (!def.hasArgument) {
                            result->emplace_back(Argument({&def, nullptr, iter.index()}));
                            return true;
                        }
                        nextMayBeArg = true;
                        nextMustBeArg = !def.argumentIsOptional;
                        continue;
                    }
                    result->emplace_back(Argument({&ParameterDefinition::argument(), arg, iter.index()}));
                    return true;
                }
                break;
            }
        }
    }
};

/// @brief Holds the definitions for the universally supported arguments, like `--help`, `--version`, etc.
static ParamDefinitions_Common const &commonParams = ParamDefinitions::makeCommonParams();

/// Each tool has its definitions in its own namespace.
/// This macro defines the contents of that namespace.
#define DEFINE_ARGS(NAME, PARSE_TYPE) \
namespace NAME { \
    using Parser = PARSE_TYPE; \
    static auto const toolName = TOOL_NAME_ ## NAME; \
    static ParameterDefinition const defs[] = TOOL_ARGS_ ## NAME; \
    static ParamDefinitions_Common const &parser = Parser(commonParams, sizeof(defs)/sizeof(defs[0]) - 1, defs); \
}

/// MARK: Tool definitions here.
/// This imports the definition from the `tool-arguments.h` file.
/// @note When adding a tool, this list needs to be updated.
DEFINE_ARGS(PREFETCH, ParamDefinitions)
DEFINE_ARGS(FASTERQ_DUMP, ParamDefinitions)
DEFINE_ARGS(SAM_DUMP, ParamDefinitions)
DEFINE_ARGS(VDB_DUMP, ParamDefinitions)
DEFINE_ARGS(SRA_PILEUP, ParamDefinitions)
DEFINE_ARGS(FASTQ_DUMP, ParamDefinitions_FQD)

/// @brief Finds the definitions for the given named tool.
/// @returns a reference to the common base class containing the definitions.
/// @throws UnknownToolException
/// @note When adding a tool, this function needs to be updated.
static ParamDefinitions_Common const &parserForTool(std::string const &toolName)
{
    if (toolName == PREFETCH::toolName)
        return PREFETCH::parser;

    if (toolName == FASTERQ_DUMP::toolName)
        return FASTERQ_DUMP::parser;

    if (toolName == FASTQ_DUMP::toolName)
        return FASTQ_DUMP::parser;

    if (toolName == VDB_DUMP::toolName)
        return VDB_DUMP::parser;

    if (toolName == SAM_DUMP::toolName)
        return SAM_DUMP::parser;

    if (toolName == SRA_PILEUP::toolName)
        return SRA_PILEUP::parser;

    throw UnknownToolException();
}

Arguments argumentsParsed(CommandLine const &cmdLine)
{
    return parserForTool(cmdLine.toolName).parse(cmdLine);
}

std::ostream &operator <<(std::ostream &out, Argument const &arg) {
    if (arg.isArgument())
        return out << arg.argument << " (argv[" << arg.argind << "])";
    if (!arg.def->hasArgument)
        return out << "--" << arg.def->name << " (argv[" << arg.argind << "])";
    if (!arg.argument)
        return out << "--" <<arg.def->name << "=(null)" << " (argv[" << arg.argind << "])";
    else
        return out << "--" <<arg.def->name << "=" << arg.argument << " (argv[" << arg.argind << "])";
}

/// @brief Print parameter bitmasks used for every parameter of every tool.
/// @param out the stream to write to.
/// @note When adding a tool, this function needs to be updated.
void printParameterBitmasks(std::ostream &out) {
    PREFETCH::parser.printParameterBitmasks(PREFETCH::toolName, out);
    FASTERQ_DUMP::parser.printParameterBitmasks(FASTERQ_DUMP::toolName, out);
    FASTQ_DUMP::parser.printParameterBitmasks(FASTQ_DUMP::toolName, out);
    SAM_DUMP::parser.printParameterBitmasks(SAM_DUMP::toolName, out);
    SRA_PILEUP::parser.printParameterBitmasks(SRA_PILEUP::toolName, out);
    VDB_DUMP::parser.printParameterBitmasks(VDB_DUMP::toolName, out);
}

/// @brief Print parameter bitmasks used for every parameter of every tool.
/// @param out the stream to write to.
/// @note When adding a tool, this function needs to be updated.
void printParameterJSON(std::ostream &out) {
    JSON_Printer printer(out);
    printer.print("[\n");
    PREFETCH::parser.printParameterJSON(PREFETCH::toolName, printer, true);
    FASTERQ_DUMP::parser.printParameterJSON(FASTERQ_DUMP::toolName, printer);
    FASTQ_DUMP::parser.printParameterJSON(FASTQ_DUMP::toolName, printer);
    SAM_DUMP::parser.printParameterJSON(SAM_DUMP::toolName, printer);
    SRA_PILEUP::parser.printParameterJSON(SRA_PILEUP::toolName, printer);
    VDB_DUMP::parser.printParameterJSON(VDB_DUMP::toolName, printer);
    printer.print("\n]\n");
    out.flush();
}

UniqueOrderedList<int> Arguments::keep(Argument const &keep) const {
    int max_argind = 0;
    for (auto & arg : container) {
        if (max_argind < arg.argind)
            max_argind = arg.argind;
    }

    std::vector< bool > used(max_argind + 1, false);
    for (auto & arg : container)
        used[arg.argind] = true;

    UniqueOrderedList< int > ignored(container.size());

    for (auto & arg : container) {
        auto const argind = arg.argind;
        if (arg.isArgument()) {
            if (arg == keep)
                continue;
            ignored.insert(argind);
        }
        else if (arg.ignore()) {
            bool const next_is_used = used[argind + 1];
            ignored.insert(argind);
            if (!next_is_used)
                ignored.insert(argind + 1);
        }
    }
    return ignored;
}
