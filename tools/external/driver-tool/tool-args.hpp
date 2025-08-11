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
 *  parse command line arguments
 *
 */

#pragma once

#include <vector>
#include <string>
#include <iosfwd>
#include <cstring>
#include <cstdint>

#include "util.hpp"
#define USE_TOOL_HELP 0

struct ParamDefinitions_Common;
struct ParamDefinitions;
struct CommandLine;

/// @brief This structure holds the tool parameter definitions. Populated by TOOL_ARG macro in `tool-args.cpp`.
/// See also `common-arguments.h`, `fastq-dump-arguments.h`, and the auto-generated `tool-arguments.h`.
struct ParameterDefinition {
#if USE_TOOL_HELP
    std::vector<char const *> helpText;
#endif
    char const *name;       ///< long form name
    char const *aliases;    ///< short form aliases
    uint64_t bitMask;       ///< the bitmask assigned to this definition.
    bool hasArgument;       ///< true if the parameter takes an argument.
    bool argumentIsOptional; ///< only fastq-dump has parameters with optional arguments, other tools do not.

    /// @brief The argument appears to be a parameter, but it is not in the list of known parameters for the tool. It does not take an argument.
    static ParameterDefinition const &unknownParameter() {
        static constexpr ParameterDefinition const value { nullptr, nullptr, 0, false, false };
        static_assert(value.hasArgument == false, "");
        static_assert(value.name == nullptr, "");
        return value;
    }

    /// @brief The query is not a parameter for the tool, it is an argument.
    static ParameterDefinition const &argument() {
        static constexpr ParameterDefinition const value { nullptr, nullptr, 0, true, false };
        static_assert(value.hasArgument == true, "");
        static_assert(value.name == nullptr, "");
        return value;
    }
    
    /// @brief False for unknown parameters.
    operator bool() const {
        return !!name;
    }

    bool isArgument() const {
        return this == &argument() || (name == nullptr && hasArgument);
    }

    bool operator== (ParameterDefinition const &other) const {
        return name == other.name || strcmp(name, other.name) == 0;
    }

    bool operator== (char const *name) const {
        return this->name != nullptr && strcmp(this->name, name) == 0;
    }

    bool operator== (std::string const &name) const {
        return this->name != nullptr && name == this->name;
    }

    bool operator< (ParameterDefinition const &other) const {
        return strcmp(this->name, other.name) < 0;
    }

    bool operator< (std::string const &name) const {
        return !(name <= this->name);
    }

    bool operator< (char const *name) const {
        return strcmp(this->name, name) < 0;
    }

    friend bool operator< (std::string const &a, ParameterDefinition const &b) {
        return a < b.name;
    }

    friend bool operator< (char const *a, ParameterDefinition const &b) {
        return strcmp(a, b.name) < 0;
    }
};

/// @brief A command line argument may be an argument to the command, a parameter, or a parameter with an argument.
struct Argument {
    using Ignore = char const *;
    static constexpr Ignore const notFound = { "file not found" };
    static constexpr Ignore const unreadable = { "file not readable" };
    static constexpr Ignore const duplicate = { "duplicate" };
    static constexpr Ignore const notCurrent = { "not the current tool argument" };

    ParameterDefinition const *def; ///< the definition that was used to construct this argument.
    char const *argument;           ///< copied from argv, possibly offset from start of string for short form parameters.
    int argind;                     ///< index in argv
    mutable Ignore reason;

    bool isArgument() const { return def->isArgument(); }
    bool ignore() const { return !isArgument() && reason != nullptr; }
    bool operator ==(ParameterDefinition const &other) const { return def->operator==(other); }
    bool operator ==(char const *name) const { return def->operator==(name); }
    bool operator ==(Argument const &other) const {
        return this == &other
            || (isArgument() && other.isArgument() && strcmp(argument, other.argument) == 0)
            || (!isArgument() && !other.isArgument() && (def == other.def || def->operator==(*other.def)));
    }

    friend std::ostream &operator <<(std::ostream &out, Argument const &arg);
};

/// @brief Holds all the parameters and argument parsed from a command line.
class Arguments {
public:
    using Container = std::vector<Argument>;

private:
    friend ParamDefinitions_Common;

    Container container;
    uint64_t argsBits;
    unsigned parameters;
    unsigned arguments;

    Arguments() {}

    Arguments(Container const &args, uint64_t argsBits)
    : container(args)
    , argsBits(argsBits)
    , parameters(0)
    , arguments(0)
    {
        for (auto &arg : container) {
            if (arg.isArgument())
                ++arguments;
            else
                ++parameters;
            arg.reason = nullptr;
        }
    }

public:
    /// @brief The bit flags of parameters used in the command line.
    uint64_t argsUsed() const { return argsBits; }

    /// @brief The number of parameters (a parameter starts with a e.g. `-`)
    unsigned countOfParameters() const { return parameters; }

    /// @brief The number of arguments (arguments don't start with `-`)
    unsigned countOfCommandArguments() const { return arguments; }

    std::vector<Argument>::const_iterator begin() const {
        return container.begin();
    }
    std::vector<Argument>::const_iterator end() const {
        return container.end();
    }

    /// @brief Do this for each (non-ignored) parameter.
    /// @tparam F the type of `call`.
    /// @param call the thing to do.
    template <typename F>
    void each(F && call) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && !arg.isArgument())
                call(arg);
    }

    /// @brief Do this for each element that matches.
    /// @tparam F the type of `call`.
    /// @param matching the string to match.
    /// @param call the thing to do.
    template <typename F>
    void each(char const *matching, F && call) const {
        for (auto & arg : container)
            if (arg == matching)
                call(arg);
    }

    /// @brief Do this for the first (non-ignored) element that matches.
    /// @tparam F the type of `call`.
    /// @param matching the string to match.
    /// @param call the thing to do.
    template <typename F>
    void first(char const *matching, F && call) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == matching) {
                call(arg);
                return;
            }
    }

    /// @brief Do this for the first (non-ignored) element that matches.
    /// @tparam F the type of `call`.
    /// @param matching the string to match.
    /// @param call the thing to do.
    /// @returns true if some `call` returned true.
    template <typename F>
    bool firstWhere(char const *matching, F && call) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == matching && call(arg))
                return true;
        return false;
    }

    /// @brief Do this for each argument (i.e. not a parameter).
    /// @tparam F the type of `f`.
    /// @param f the thing to do.
    template <typename F>
    void eachArgument(F && f) const {
        for (auto & arg : container)
            if (arg.isArgument())
                f(arg);
    }

    /// @brief Count the number of (non-ignored) elements that match.
    /// @param name the string to match.
    /// @return the count.
    unsigned countMatching(char const *name) const {
        unsigned result = 0;
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == name)
                ++result;
        return result;
    }

    /// @brief True if any (non-ignored) elements match.
    /// @param name the string to match.
    /// @returns True if any (non-ignored) elements match.
    bool any(char const *name) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == name)
                return true;
        return false;
    }

    /// @returns the indices in argv to skip.
    /// If an element of argv is a plain argument (i.e. not a parameter or a param argument),
    /// it will not be kept, unless it is == `keep`.
    /// A parameter (and its argument) that has been marked to ignore will not be kept.
    UniqueOrderedList<int> keep(Argument const &keep) const;
};

struct UnknownToolException {};

/// @brief Parses the argument from the command line using the definitions and parsing rules for the named tool.
/// @param cmdLine the command line.
/// @return The parsed arguments.
/// @throws `UnknownToolException` if the named tool is not recognized.
Arguments argumentsParsed(CommandLine const &);

/// @brief Print parameter bitmasks used for every parameter of every tool.
/// @param out the stream to write to.
///
/// Format is tab-delimited.
/// Fields: tool name, '(' bit shift ')' decimal value, parameter long name
void printParameterBitmasks(std::ostream &out);

/// @brief Print parameter bitmasks used for every parameter of every tool.
/// @param out the stream to write to.
///
/// Format is JSON.
void printParameterJSON(std::ostream &out);
