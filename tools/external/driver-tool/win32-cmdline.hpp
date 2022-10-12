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
 *  provide Windows support
 *
 */

#pragma once

#ifndef _H_WIN32_CMDLINE_HPP_
#define _H_WIN32_CMDLINE_HPP_

#include "wide-char.hpp"

#pragma warning (push)
#pragma warning (disable: 4505)

namespace Win32Support {

    namespace {
        /// These are the meaningful characters for Windows command line parsing.
        /// Note: None of the special characters occupy more than one code unit.
        /// And none of the other code units match any of the special characters.
        /// So there is no need to do anything more complicated than simple iteration
        /// of code units to find them in a string.
        template < typename T >
        struct SpecialCharacters {
            enum : T {
                nul = '\0',     ///< end of string
                tab = '\t',     ///< whitespace
                space = ' ',    ///< whitespace
                quote = '"',    ///< the only character that REALLY matters
                slash = '\\',   ///< only special if followed by a quote
            };
        };

        template<> struct SpecialCharacters< wchar_t > {
            enum : wchar_t {
                nul = L'\0',
                tab = L'\t',
                space = L' ',
                quote = L'"',
                slash = L'\\',
            };
        };

        template < typename T, typename U = T >
        static T* copy(T* dst, T const* const end, U const* src, U const* const srcend)
        {
            while (dst < end && src < srcend)
                *dst++ = *src++;
            return dst;
        }

        template < >
        wchar_t* copy< wchar_t, char >(wchar_t* dst, wchar_t const* const end, char const* src, char const* const srcend)
        {
            return dst + Win32Support::widen(dst, end - dst, src, srcend - src);
        }

        template < >
        char* copy< char, wchar_t >(char* dst, char const* const end, wchar_t const* src, wchar_t const* const srcend)
        {
            return dst + Win32Support::unwiden(dst, end - dst, src, srcend - src);
        }

        template < typename T, typename U = T >
        size_t convertedSize(U const* src, size_t const size)
        {
            return size;
            (void)(src);
        }
        template < >
        size_t convertedSize<wchar_t, char>(char const* src, size_t const size)
        {
            return Win32Support::wideSize(src, size);
        }
        template < >
        size_t convertedSize<char, wchar_t>(wchar_t const* src, size_t const size)
        {
            return Win32Support::unwideSize(src, size);
        }
    }


/**
 \brief Builds command line string according to MS C command line parsing string rules.

 \returns A string appropriate for use with CreateProcess.

 I have quoted the relavant parts:
 \see https://docs.microsoft.com/en-us/cpp/c-language/parsing-c-command-line-arguments

 Microsoft C startup code uses the following rules when interpreting arguments given on the operating system command line:

 Arguments are delimited by whitespace characters, which are either spaces or tabs.

 The first argument (argv[0]) is treated specially. It represents the program name. Because it must be a valid pathname, parts surrounded by double quote marks (") are allowed. The double quote marks aren't included in the argv[0] output. The parts surrounded by double quote marks prevent interpretation of a space or tab character as the end of the argument. The later rules in this list don't apply.

 A string surrounded by double quote marks is interpreted as a single argument, whether it contains whitespace characters or not..

 A double quote mark preceded by a backslash (\") is interpreted as a literal double quote mark (").

 Backslashes are interpreted literally, unless they immediately precede a double quote mark.

 If an even number of backslashes is followed by a double quote mark, then one backslash (\) is placed in the argv array for every pair of backslashes (\\), and the double quote mark (") is interpreted as a string delimiter.

 If an odd number of backslashes is followed by a double quote mark, then one backslash (\) is placed in the argv array for every pair of backslashes (\\). The double quote mark is interpreted as an escape sequence by the remaining backslash, causing a literal double quote mark (") to be placed in argv.
 */
template < typename T >
struct CmdLineString
{
public:
    template <typename U>
    CmdLineString(U const* const* argv)
    : value(convert(argv)) {}

    ~CmdLineString() {
        free((void *)value);
    }

    T const* get() const { return value; }

private:
    template <typename U>
    static T* convert(U const* const* const argv)
    {
        using SourceChars = SpecialCharacters<U>;
        using ResultChars = SpecialCharacters<T>;

        struct Arg {
            U const* value;
            bool isarg0;
            size_t incount;
            size_t outcount;
            size_t quotes;
            size_t slashes;
            size_t whiteSpaces;

            Arg(U const* const arg, bool is_arg0 = false)
                : value(arg)
                , isarg0(is_arg0)
                , incount(0)
                , outcount(0)
                , quotes(0)
                , slashes(0)
                , whiteSpaces(0)
            {
                unsigned srun = 0;

                for (auto i = arg; ; ++i) {
                    auto const ch = *i;
                    if (ch == SourceChars::nul)
                        break;

                    ++incount;

                    auto const save = srun;
                    if (ch == SourceChars::slash) {
                        ++srun;
                        continue;
                    }
                    srun = 0;

                    switch (ch) {
                    case SourceChars::tab:
                    case SourceChars::space:
                        ++whiteSpaces;
                        break;
                    case SourceChars::quote:
                        slashes += save;
                        ++quotes;
                        break;
                    default:
                        break;
                    }
                }
                outcount = whiteSpaces > 0 ? 2 : 0;
                if (is_arg0)
                    assert(quotes == 0); // quote chars aren't allowed in paths
                else
                    outcount += slashes + quotes;
                outcount += convertedSize<T, U>(arg, incount);
            }
            T* convert(T* dst, T const* const end) const
            {
                assert(dst < end);
                if (whiteSpaces > 0)
                    *dst++ = ResultChars::quote;

                if (isarg0 || quotes == 0) // no quotes means no escaping
                    dst = copy<T, U>(dst, end, value, value + incount);
                else {
                    unsigned start = 0;
                    unsigned srun = 0;

                    for (unsigned i = 0; i < incount; ++i) {
                        auto const ch = value[i];

                        if (ch == SourceChars::slash) {
                            ++srun;
                            continue;
                        }
                        if (ch == SourceChars::quote) {
                            dst = copy<T, U>(dst, end, value + start, value + i);
                            for (unsigned j = 0; j <= srun; ++j) { // srun == 0 means one backslash needs to be added
                                assert(dst < end);
                                *dst++ = ResultChars::slash;
                            }
                            assert(dst < end);
                            *dst++ = ch;
                            start = i + 1;
                        }
                        srun = 0;
                    }
                    if (start < incount)
                        dst = copy<T, U>(dst, end, value + start, value + incount);
                }

                assert(dst < end);
                if (whiteSpaces > 0)
                    *dst++ = ResultChars::quote;

                return dst;
            }
        };

        unsigned argc = 0;

        for (auto argp = argv; *argp != nullptr; ++argp)
            ++argc;

        size_t outsize = 0;
        std::vector<Arg> args;
        args.reserve(argc);

        for (auto argp = argv; *argp != nullptr; ++argp) {
            auto const& arg = Arg(*argp, argp == argv);
            args.emplace_back(arg);
            outsize += arg.outcount + 1;
        }

        auto const result = (T*)malloc(sizeof(T) * outsize);
        if (result == nullptr)
            throw std::bad_alloc();

        auto dst = result;
        auto const end = dst + outsize;

        for (auto const& arg : args) {
            assert(dst < end);
            dst = arg.convert(dst, end);
            assert(dst < end);
            *dst++ = ResultChars::space;
        }
        dst[-1] = ResultChars::nul;

        return result;
    }
    T const *value = nullptr;
};

}

#pragma warning (pop)

#endif /* _H_WIN32_CMDLINE_HPP_ */
