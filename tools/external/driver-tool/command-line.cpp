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
#include "build-version.hpp"

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cctype>
#include <type_traits>

using Version = sratools::Version;

#if WINDOWS

template <typename T>
static inline int countOfCollection(T *const *const collection) {
	for (int i = 0; ; ++i) {
		if (collection[i] == nullptr)
			return i;
	}
}

/// Compute the number of UTF-8 bytes needed to store an array of Windows wchar strings.
/// \Note the count includes null terminators.
static inline size_t neededToConvert(int const argc, wchar_t const *const *const collection)
{
    size_t bytes = 0;
    for (int i = 0; i != argc; ++i) {
        auto const count = Win32Support::unwideSize(collection[i]);
        if (count <= 0)
            throw std::runtime_error("Can not convert command line to UTF-8!?"); ///< Windows shouldn't ever send us strings that it can't convert to UTF8
        bytes += count;
    }
    return bytes;
}

/// Compute the number of Windows wchars needed to store an array of UTF-8 strings.
/// \Note the count includes null terminators.
static inline size_t neededToConvert(int argc, char const *const *const collection)
{
    size_t bytes = 0;
    for (int i = 0; i != argc; ++i) {
        auto const count = Win32Support::wideSize(collection[i]);
        if (count <= 0)
            throw std::runtime_error("Can not convert command line to wide!?"); ///< Windows shouldn't ever send us strings that it can't convert to UTF8
        bytes += count;
    }
    return bytes;
}

/// Convert an array of Windows wchar strings into an array of UTF-8 strings
static inline void convert(int argc, char *rslt[], size_t bufsize, char *buffer, wchar_t *in[])
{
    int i;
    for (i = 0; i < argc; ++i) {
        auto const count = Win32Support::unwiden(buffer, bufsize, in[i]);
        assert(0 < count && (size_t)count <= bufsize); ///< should never be < 0, since we should have caught that in `needUTF8s`
        rslt[i] = buffer;
        buffer += count;
        bufsize -= count;
    }
    rslt[i] = nullptr;
}

/// Convert an array of Windows wchar strings into an array of UTF-8 strings
static inline void convert(int argc, wchar_t *rslt[], size_t bufsize, wchar_t *buffer, char *in[])
{
    int i;
    for (i = 0; i < argc; ++i) {
        auto const count = Win32Support::widen(buffer, bufsize, in[i]);
        assert(0 < count && (size_t)count <= bufsize); ///< should never be < 0, since we should have caught that in `needUTF8s`
        rslt[i] = buffer;
        buffer += count;
        bufsize -= count;
    }
    rslt[i] = nullptr;
}

template <typename S, typename T>
static inline void convertArgsAndEnv(void **const allocated, T *const **const destArgs, T *const **const destEnv, int const argc, S *const *const argv, S *const *const envp)
{
	auto const nEnv = countOfCollection(envp);
	auto const szEnv = neededToConvert(nEnv, envp);
	auto const szArgs = neededToConvert(argc, argv);
	size_t const szAlloc = (argc + 1 + nEnv + 1) * sizeof(T *) + (szArgs + szEnv) * sizeof(T);
	auto const alloced = (T **)malloc(szAlloc);
	if (alloced == nullptr)
		throw std::bad_alloc();

	memset(alloced, 0, szAlloc);
	convert(argc, argv, szArgs, alloced);
	convert(nEnv, envp, szEnv, alloced + argc + 1);

	*allocated = (void *)alloced;
	*destArgs = (T *const *)(alloced);
	*destEnv = (T *const *)(alloced + argc + 1);
}

#else // WINDOWS
#undef USE_WIDE_API

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

#endif // WINDOWS

void CommandLine::initialize(void)
{
	fullPathToExe = FilePath::fullPathToExecutable(argv, envp, extra);

	auto const p = fullPathToExe.split();
	fullPath = p.first; /// directory containing the executable
	realName.assign(p.second); /// real name of the executable

    baseName = FilePath::baseName(argv[0]); // from the user
#if WINDOWS
	// Adding version to exe name is not used.
#else
    versionFromName = Version::fromName(realName).packed;
    runAsVersion = Version::fromName(baseName).packed; // e.g. 3.0.3 from fasterq-dump.3.0.3
#endif
    toolName = baseName;

	// remove any trailing version (or extension or whatever).
	auto const dot = toolName.find('.');
	if (dot != toolName.npos)
		toolName = toolName.substr(0, dot);

#if WINDOWS
	// fake name feature not used
#else
    if (fakeName)
        toolName = fakeName;
#endif

    toolPath = getToolPath();
}

#if WINDOWS

#if USE_WIDE_API
/// Used by wmain
CommandLine::CommandLine(int in_argc, wchar_t **in_wargv, wchar_t **in_wenvp, char **dummy_extra)
	: argv(nullptr)
	, envp(nullptr)
	, extra(nullptr)
	, wargv(in_wargv)
	, wenvp(in_wenvp)
	, allocated(nullptr)
	, argc(in_argc)
	, buildVersion(Version::current.packed)
{
	convertArgsAndEnv(&allocated, &argv, &envp, argc, wargv, wenvp);
	initialize();
	(void)dummy_extra;
}
#endif
/// Used by test code
CommandLine::CommandLine(int in_argc, char **in_argv, char **in_envp, char **dummy_extra)
	: argv(in_argv)
	, envp(in_envp)
	, extra(nullptr)
	, wargv(nullptr)
	, wenvp(nullptr)
	, allocated(nullptr)
	, argc(in_argc)
	, buildVersion(Version::current.packed)
{
	convertArgsAndEnv(&allocated, &wargv, &wenvp, argc, argv, envp);
	initialize();
	(void)dummy_extra;
}
#else
CommandLine::CommandLine(int in_argc, char **in_argv, char **in_envp, char **in_extra)
	: argv(in_argv)
	, envp(in_envp)
	, extra(in_extra)
	, allocated(nullptr)
	, fakeName(getFakeName())
	, argc(in_argc)
	, buildVersion(Version::current.packed)
{
	initialize();
}
#endif

static uint32_t effectiveVersion(uint32_t fromName, uint32_t runAs, uint32_t fromBuild)
{
    return fromName ? fromName : runAs ? runAs : fromBuild;
}

static std::vector<FilePath> getToolPaths(FilePath const &baseDir, std::string const &toolName, uint32_t version)
{
    std::vector<FilePath> result;
#if WINDOWS
    result.emplace_back(baseDir.append(toolName + "-orig.exe"));
#if DEBUG || _DEBUGGING
    result.emplace_back(baseDir.append(toolName + ".exe"));
#endif
    (void)(version); // not used in the name on Windows
#else
    std::string const versString = Version(version);

    result.emplace_back(baseDir.append(toolName + "-orig." + versString));
    result.emplace_back(result.back().copy());
    result.back().removeSuffix(versString.size() + 1); // remove ".M.m.r"
#if DEBUG || _DEBUGGING
    result.emplace_back(result.back().copy());
    result.back().removeSuffix(5); // remove "-orig"
#endif
#endif
    return result;
}

FilePath CommandLine::getToolPath() const {
#if WINDOWS
    auto const &tries = getToolPaths(fullPath, toolName, buildVersion);
#else
    auto const &tries = getToolPaths(fullPath, toolName, effectiveVersion(versionFromName, runAsVersion, buildVersion));
#endif // WINDOWS
    for (auto && path : tries) {
        if (path.executable())
            return path;
    }
    return tries.back();
}

#if WINDOWS
#else
#endif

std::ostream &operator<< (std::ostream &os, CommandLine const &obj)
{
    auto const effVers = Version(
#if WINDOWS
        effectiveVersion(0, 0, obj.buildVersion)
#else
        effectiveVersion(obj.versionFromName, obj.runAsVersion, obj.buildVersion)
#endif
        );
    os <<
    "{\n" \
        "    fullPathToExe: " << (std::string)obj.fullPathToExe << "\n" \
        "    basename(argv[0]): " << obj.baseName << "\n" \
        "    effectiveVersion: " << effVers << "\n" \
        "    buildVersion: " << Version(obj.buildVersion) << "\n";
#if WINDOWS
#else
    os <<
        "    versionFromName: " << Version(obj.versionFromName) << "\n" \
        "    runAsVersion: " << Version(obj.runAsVersion) << "\n" \
        "    fakeName: " << (char const *)(obj.fakeName ? obj.fakeName : "(not set)") << "\n";
#endif
    os <<
        "    wouldExec: " << (std::string)obj.toolPath << "\n" \
        "    withArgv: [\n" \
        "        " << obj.toolName << "\n";
    for (auto i = 1; i < obj.argc; ++i)
        os << "        " << obj.argv[i] << "\n";
    os << "    ]\n}" << std::endl;

    return os;
}
