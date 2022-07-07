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

#include "util.hpp"
#include "command-line.hpp"
#include "tool-args.hpp"
#include "file-path.hpp"
#include "proc.hpp"
#include "../../shared/toolkit.vers.h"

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cctype>
#include <type_traits>


#if WINDOWS
#else
#undef USE_WIDE_API
#endif

#if WINDOWS
#else
static uint32_t removeVersion(std::string &name) {
    uint32_t result = 0;
    int state = 0;
    auto versAt = name.size();
    auto versEnd = name.size();
    
    for (auto i = 0; i < name.size(); ++i) {
        switch (state) {
        case 0:
            if (name[i] == '.') {
                versAt = i;
                ++state;
            }
            break;
        case 1:
            if (isdigit(name[i])) {
                ++state;
                result = (name[i] - '0') << 24;
            }
            else {
        NO_VERS:
                versAt = name.size();
                state = 0;
            }
            break;
        case 2:
            if (name[i] == '.') {
                ++state;
                break;
            }
            if (!isdigit(name[i]))
                goto NO_VERS;
            result = (((result >> 24) * 10 + (name[i] - '0')) << 24);
            break;
        case 3:
            if (isdigit(name[i])) {
                ++state;
                result += ((name[i] - '0') << 16);
            }
            else
                goto NO_VERS;
            break;
        case 4:
            if (name[i] == '.') {
                ++state;
                break;
            }
            if (!isdigit(name[i]))
                goto NO_VERS;
            result = (result & 0xFF000000) + ((((result & 0x00FF0000) >> 16) + (name[i] - '0')) << 16);
            break;
        case 5:
            if (isdigit(name[i])) {
                ++state;
                result += name[i] - '0';
            }
            else
                goto NO_VERS;
            break;
        case 6:
            if (!isdigit(name[i])) {
                ++state;
                versEnd = i;
            }
            else
                result = (result & 0xFFFF0000) + ((result & 0xFFFF) + (name[i] - '0'));
            break;
        default:
            break;
        }
    }
    if (state >= 6) {
        name = name.substr(0, versAt) + name.substr(versEnd);
        return result;
    }
    return 0;
}

static uint32_t getVersion(std::string const &name) {
    std::string dup = name;
    return removeVersion(dup);
}

#if DEBUG || _DEBUGGING
struct removeVersionTest {
    static void test1() {
        std::string versString = "foo.3.2.1";
        auto const vers = removeVersion(versString);
        assert(vers == 0x03020001);
        assert(versString == "foo");
    }
    static void test2() {
        std::string versString = "foo.3.2.1.exe";
        auto const vers = removeVersion(versString);
        assert(vers == 0x03020001);
        assert(versString == "foo.exe");
    }
    static void test3() {
        std::string versString = "foo";
        auto const vers = removeVersion(versString);
        assert(vers == 0);
        assert(versString == "foo");
    }
    static void test4() {
        std::string versString = "foo.exe";
        auto const vers = removeVersion(versString);
        assert(vers == 0);
        assert(versString == "foo.exe");
    }
    static void test5() {
        std::string versString = "foo.1";
        auto const vers = removeVersion(versString);
        assert(vers == 0);
        assert(versString == "foo.1");
    }
    removeVersionTest() {
        test1();
        test2();
        test3();
        test4();
        test5();
    }
} static const removeVersionTest;
#endif
#endif

#if USE_WIDE_API

/// Compute the number of UTF-8 bytes needed to store an array of Windows wchar strings.
/// \Note the count includes null terminators.
static std::pair<size_t, int> needUTF8s(wchar_t *collection[])
{
    size_t bytes = 0;
    int argc = 0;
    for (auto i = 0; collection[i]; ++i) {
        auto const count = Win32Support::unwideSize(collection[i]);
        if (count <= 0)
            throw std::runtime_error("Can not convert command line to UTF-8!?"); ///< Windows shouldn't ever send us strings that it can't convert to UTF8
        bytes += count;
        argc += 1;
    }
    return {bytes, argc};
}

/// Convert an array of Windows wchar strings into an array of UTF-8 strings
static void convert2UTF8(int argc, char *argv[], size_t bufsize, char *buffer, wchar_t *collection[])
{
    int i;
    for (i = 0; i < argc; ++i) {
        auto const count = Win32Support::unwiden(buffer, bufsize, collection[i]);
        assert(0 < count && (size_t)count <= bufsize); ///< should never be < 0, since we should have caught that in `needUTF8s`
        argv[i] = buffer;
        buffer += count;
        bufsize -= count;
    }
    argv[i] = nullptr;
}

/// Convert an array of Windows wchar strings into an array of UTF8 strings
/// \returns a new array that can be `free`d
/// \Note don't free the individual strings in the array
static char const *const *convertWStrings(wchar_t *collection[])
{
    auto const sn = needUTF8s(collection);
    auto const &chars = sn.first;
    auto const &argc = sn.second;
    auto const argv = (char **)malloc((argc + 1) * sizeof(char *) + chars);
    auto const buffer = (char *)&argv[argc + 1];

    if (argv != NULL)
        convert2UTF8(argc, argv, chars, buffer, collection);

    return argv;
}

#endif // WINDOWS && USE_WIDE_API

#if USE_WIDE_API
/// Used by wmain
CommandLine::CommandLine(int argc, wchar_t **wargv, wchar_t **wenvp, char **extra)
: argv(convertWStrings(wargv))
, envp(convertWStrings(wenvp))
, extra(nullptr)
, wargv(wargv)
, wenvp(wenvp)
#else
/// Used by normal main
CommandLine::CommandLine(int argc, char **argv, char **envp, char **extra)
: argv(argv)
, envp(envp)
, extra(extra)
#endif
#if WINDOWS
#else
, fakeName(getFakeName())
#endif
, fullPathToExe(FilePath::fullPathToExecutable(argv, envp, extra))
, argc(argc)
, buildVersion(TOOLKIT_VERS)
{
    {
        auto const p = fullPathToExe.split();
        fullPath = p.first;
        realName = p.second;
    }
    {
        auto const p = pathForArgument(0).split();
        baseName = argv[0] + (p.first.empty() ? 0 : (p.first.size() + 1));
    }
#if WINDOWS
#else
    versionFromName = getVersion(realName);
    runAsVersion = getVersion(baseName);
#endif 

    toolName = baseName;
    {
        auto const dot = toolName.find('.');
        if (dot != toolName.npos)
            toolName = toolName.substr(0, dot);
    }
#if WINDOWS
#else
    if (fakeName)
        toolName = fakeName;
#endif

    toolPath = getToolPath();
}

CommandLine::~CommandLine() {
#if USE_WIDE_API
    free((void *)argv);
    free((void *)envp);
#endif
}

#if WINDOWS
#else
static std::string versionFromU32(uint32_t const version)
{
               // "255.255.65535"
    char buffer[3 + 1 + 3 + 1 + 5 + 1];
    char *out = buffer + sizeof(buffer);
    {
        auto vers = version & 0xFFFF;
        
        *--out = '\0';
        do {
            *--out = '0' + vers % 10;
            vers /= 10;
        } while (vers != 0);
        
        *--out = '.';
    }
    {
        auto vers = (version >> 16) & 0xFF;
        do {
            *--out = '0' + vers % 10;
            vers /= 10;
        } while (vers != 0);
        
        *--out = '.';
    }
    {
        auto vers = (version >> 24) & 0xFF;
        do {
            *--out = '0' + vers % 10;
            vers /= 10;
        } while (vers != 0);
    }
    return out;
}
#if DEBUG || _DEBUGGING
struct testVersionFromU32 {
    testVersionFromU32() {
        {
            auto const result = versionFromU32(0);
            assert(result == "0.0.0");
        }
        {
            auto const result = versionFromU32(1);
            assert(result == "0.0.1");
        }
        {
            auto const result = versionFromU32(0x00010000);
            assert(result == "0.1.0");
        }
        {
            auto const result = versionFromU32(0x01000000);
            assert(result == "1.0.0");
        }
        {
            auto const result = versionFromU32(0x01020003);
            assert(result == "1.2.3");
        }
        {
            auto const result = versionFromU32(0xFFFFFFFF);
            assert(result == "255.255.65535");
        }
    }
} static testVersionFromU32;
#endif

static std::string versionFromU32(uint32_t fromName, uint32_t runAs, uint32_t fromBuild)
{
    return versionFromU32(fromName ? fromName : runAs ? runAs : fromBuild);
}
#endif

FilePath CommandLine::getToolPath() const {
#if WINDOWS
    auto const origToolName = toolName + "-orig.exe";
    auto result = fullPath.append(origToolName);
#else
    auto const versString = versionFromU32(versionFromName, runAsVersion, buildVersion);
    auto result = fullPath.append(FilePath(toolName + "-orig." + versString));
    
    if (!result.executable()) {
        result.removeSuffix(versString.size() + 1); // remove version
#if DEBUG || _DEBUGGING
        if (!result.executable()) {
            result.removeSuffix(5); // remove -orig
        }
#endif
    }
#endif // !WINDOWS
    return result;
}

#if WINDOWS
#else
char const *CommandLine::getFakeName() const
{
    auto const name = "SRATOOLS_IMPERSONATE";
    for (auto cur = envp; *cur; ++cur) {
        auto entry = *cur;
        for (auto i = 0; ; ++i) {
            if (name[i] == '\0' && entry[i] == '=')
                return entry + i + 1;
            if (name[i] != entry[i])
                break;
        }
    }
    return nullptr;
}
#endif
