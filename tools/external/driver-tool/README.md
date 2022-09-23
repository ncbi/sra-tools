# Purpose

To provide a central location for converting user-input accessions to location-
appropriate URLs that can then be used directly by sra-toolkit.

## Description:

With the move of SRA data to various cloud platforms, there becomes more than
one source for data. The sra-toolkit will need to respond to this in a
reasonable way. Depending on the location of the user, some sources may be fast
and cheap and others may be slow and costly to access. Moreover, not all sources
will provide the exact same data, e.g. some sources might not provide original
spot names and quality scores. There becomes a complicated and arbitrary matrix
of choices.

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

Rather than change all the tools to be able to perform these tasks, we will
create a single tool to interact with the user. This new tool will determine the
proper objects and tools to satisfy the user's requests. It will drive the tools
with the correct URLs for the runs they are to work on.

## Path resolution:

The driver tool uses the following rules for locating the tools it is supposed
to use:

1. Search the same directory as itself
2. Search the directories in the PATH environment variable
3. Search the current directory

If the tool is not found, a message is printed.

## Adding a tool to the driver tool:

In `support2.hpp`,
1. Create an entry in the `Imposter` enum.
2. Add to the comparisons in `WhatImposter::detect_imposter` to map the tool
name to the enum value from Step 1.
3. Add the new tool to the cases in `WhatImposter::imposter_2_string`.
4. Declare a new impersonator function. (Look for `TODO: the impersonators are
declared here.`)

In `sratools.cpp`, add to the `switch` cases in
```
int main(int argc, char *argv[], char *envp[], ToolPath const &toolpath)
```
mapping the new enum value from Step 1 to the new impersonator function from 
Step 4.
 
Finally, create a new impersonator class. The main job of an impersonator is to
declare the command line parameters so that the driver tool can correctly parse
the added tool's command line. 

* Use `imp_fasterq_dump.cpp` as a starting point if the added tool needs
accession resolution via SDL.
* Look for any tool options that would benefit from using SRA Lite files and
override `preferNoQual` to return `true` when appropriate.
* Use `imp_prefetch.cpp` as a starting point if the added tool handles SDL on its own.

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
