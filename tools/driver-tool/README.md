# Purpose

To provide a central location for converting user-input accessions to
location-appropriate URLs that can then be used directly by sra-toolkit.

## Description:

With the move of SRA data to various cloud platforms, there becomes more than one source for data. The sra-toolkit will need to respond to this in a reasonable way. Depending on the location of the user, some sources may be fast and cheap and others may be slow and costly to access. Moreover, not all sources will provide the exact same data, e.g. some sources might not provide original spot names and quality scores. There becomes a complicated and arbitrary matrix of choices.

Additionally, users have expected to be able to act directly on accessions that are not runs, e.g. to run fastq-dump on an experiment accession and have that accession be automatically expanded to the set of run accessions. Currently, our tools simply attempt to act on whatever input they are given, leaving it to VDB to make sense of the request. This has lead to cryptic and unactionable error messages from VDB being presented to the user. VDB doesn't know the context of the request, so it is unable to generate an actionable error message. This needs to be done by the tool, preferably before any other action is attempted on the request. But this is a complicated process often requiring two-way interaction with the user before anything can be done.

Rather than change all the tools to be able to perform these tasks, we will create a single tool to interact with the user. This new tool will determine the proper objects and tools to satisfy the user's requests. It will drive the tools with the correct URLs for the runs they are to work on.

## Path resolution:

The driver tool uses the following rules for locating the tools it is supposed to use:

1. Search the same directory as itself
2. Search the directories in the PATH environment variable
3. Search the current directory

If the tool is not found, a message is printed.

## Environment variables for logging and debugging:

1. `SRATOOLS_TRACE` - setting to 1 (or anything perl consider to be true) turns on TRACE printing to STDERR (not fully implemented). E.g. `SRATOOLS_TRACE=1 sratools info SRA000001`
2. `SRATOOLS_DEBUG` - setting to 1 (or anything perl consider to be true) turns on DEBUG printing to STDERR, less verbose than TRACE (not fully implemented). E.g. `SRATOOLS_DEBUG=1 sratools info SRA000001`
3. `SRATOOLS_VERBOSE=<1-5>` - turns on VERBOSE logging, 1 is less verbose than 5. E.g. `SRATOOLS_VERBOSE=1 sratools info SRA000001`
4. `SRATOOLS_IMPERSONATE=<path>` - force $0 to be this. Particularly useful for running the driver tool in its uninstalled state. E.g. this would cause the tool to run srapath: `SRATOOLS_IMPERSONATE=${HOME}/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin/srapath perl sratools.pl SRA000001`
5. `SRATOOLS_DRY_RUN` - setting to 1 will cause the tool to print out the commands it would have executed. Setting `SRATOOLS_VERBOSE=1` will also cause it to print out the environment it would have set for each command.
