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

#include "util.hpp"
#define USE_TOOL_HELP 0

struct ParamDefinitions_Common;
struct ParamDefinitions;
struct CommandLine;

struct ParameterDefinition {
#if USE_TOOL_HELP
    std::vector<char const *> helpText;
#endif
    char const *name;
    char const *aliases;
    uint64_t bitMask;
    bool hasArgument;
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

/// \brief A command line argument may be an argument to the command, a parameter, or a parameter with an argument.
struct Argument {
    using Ignore = char const *;
    static constexpr Ignore const notFound = { "file not found" };
    static constexpr Ignore const unreadable = { "file not readable" };
    static constexpr Ignore const duplicate = { "duplicate" };
    static constexpr Ignore const notCurrent = { "not the current tool argument" };

    ParameterDefinition const *def;
    char const *argument;
    int argind;
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
    uint64_t argsUsed() const { return argsBits; }
    unsigned countOfParameters() const { return parameters; }
    unsigned countOfCommandArguments() const { return arguments; }

    std::vector<Argument>::const_iterator begin() const {
        return container.begin();
    }
    std::vector<Argument>::const_iterator end() const {
        return container.end();
    }

    template <typename F>
    void each(F && call) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && !arg.isArgument())
                call(arg);
    }
    template <typename F>
    void each(char const *matching, F && call) const {
        for (auto & arg : container)
            if (arg == matching)
                call(arg);
    }
    template <typename F>
    void first(char const *matching, F && call) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == matching) {
                call(arg);
                return;
            }
    }
    template <typename F>
    bool firstWhere(char const *matching, F && call) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == matching && call(arg))
                return true;
        return false;
    }

    template <typename F>
    void eachArgument(F && f) const {
        for (auto & arg : container)
            if (arg.isArgument())
                f(arg);
    }
    unsigned countMatching(char const *name) const {
        unsigned result = 0;
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == name)
                ++result;
        return result;
    }
    bool any(char const *name) const {
        for (auto & arg : container)
            if (arg.reason == nullptr && arg == name)
                return true;
        return false;
    }
    /// \returns the indices in argv to skip, excluding the one.
    UniqueOrderedList<int> keep(Argument const &keep) const;
};

struct UnknownToolException {};
Arguments argumentsParsed(CommandLine const &);
void printParameterBitmasks(std::ostream &out);
void printParameterJSON(std::ostream &out);
