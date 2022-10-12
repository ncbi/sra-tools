# Purpose

To provide a central location for converting user-input accessions to URLs that
can be used directly by the sra-toolkit tools. The URLs are appropriate for the
location, i.e. if the user is in a cloud region that has a copy of the requested
data, the URL will be to that copy.

# Description:

With the move of SRA data to various cloud platforms, there becomes more than
one source for data. The sra-toolkit responds to this in a reasonable way.
Depending on the location of the user, some sources may be fast and cheap and
others may be slow and costly to access. Moreover, not all sources will provide
the exact same data, e.g. some sources might not provide original spot names and
quality scores. There becomes a complicated and arbitrary matrix of choices.

Additionally, users have expected to be able to act directly on accessions that
are not runs, e.g. to run fastq-dump on an experiment accession and have that
accession be automatically expanded to the set of run accessions. Currently, our
tools simply attempt to act on whatever input they are given, leaving it to VDB
to make sense of the request. This has lead to cryptic and unactionable error
messages from VDB being presented to the user. VDB doesn't know the context of
the request, so it is unable to generate an actionable error message. This needs
to be done by the tool, preferably before any other action is attempted on the
request. But this is a complicated process often requiring two-way interaction
with the user before anything can be done.

Rather than change all the tools to be able to perform these tasks, we have
created a single tool to interact with the user. This tool determines the proper
objects and options to satisfy the user's request. It drives the tools with the
correct URLs for the data they are to work on.

# Executable path resolution:

The driver tool uses the following rules for locating the tools it is supposed
to use:
- `<tool name>` is taken from `argv[0]`.
- The search is limited to the same directory as the `sratools` executable.
- On Windows, it tries `<tool name>-orig.exe`.
- On everything else,
  1. It tries `<tool name>-orig.version`.
  2. If that is not executable, it tries `<tool name>-orig`.
  3. In debug builds, if that is not executable, it tries `<tool name>`.

If the tool is not found, a message is printed.

----

# Developer notes

## `generate-args-info.sh`:

This script runs each tool in its list, with an environment variable
(`SRATOOLS_DUMP_OPTIONS`) set. This causes the `kapp` args processing code to
print out the options definitions and exit. For each tool, the output is a pair
of preprocessor defines:
  1. `TOOL_NAME_<...>` is the name of the tool.
  2. `TOOL_ARGS_<...>` is the list of arguments.

`<...>` is the uppercased name of the tool. This is all collected into
`tool-arguments.h`. This only works with a debug build.

## Adding a new command line option for a driven tool:

1. Add the new option to the tool and build it.
2. Run `generate-args-info.sh`.

## Adding a tool to the driver tool:

1. In `generate-args-info.sh`, add the tool to the `for ... in` on line 17 (the
 order doesn't matter).
2. In `tool-args.cpp`, search for "MARK: NEW TOOLS GO HERE" and add it to the
 list (the order doesn't matter).
3. If the new tool has options that change whether or not quality scores are
 used, add a function to `sratools.cpp` to return true if quality scores are not
 used. Search for "MARK: Checks for QUALITY", and add the tool name check and
 function.

## Environment variables for logging and debugging:

* `SRATOOLS_TRACE` - setting to 1 (or anything not falsy) turns on TRACE
printing to STDERR (not fully implemented).
* `SRATOOLS_DEBUG` - setting to 1 (or anything not falsy) turns on DEBUG
printing to STDERR, less verbose than TRACE (not fully implemented).
* `SRATOOLS_VERBOSE=[1-9]` - turn on VERBOSE logging, 1 is less verbose than 9.
* `SRATOOLS_IMPERSONATE=<path>` - force `$0` to be this. Particularly useful
for running the driver tool in its uninstalled state. E.g. this would cause the
tool to run `srapath`: `SRATOOLS_IMPERSONATE=srapath sratools SRR000001`
* `SRATOOLS_DRY_RUN` - setting to 1 (or anything not falsy) will cause the tool
to print out the commands it would have executed.
* `SRATOOLS_TESTING=[1-9]` - setting various values with invoke various testing
modes.
  1. Run internal tests and quit.
  2. Print resulting command line.
  3. Print resulting command line and changed environment variable names.
  4. Print environment variables and command line.
  5. Print changed environment variables and their values.
