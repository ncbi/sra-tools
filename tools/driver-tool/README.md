# Purpose

To provide a central location for converting user-input accessions to
location-appropriate URLs that can then be used directly by sra-toolkit.

## Description:

With the move of SRA data to the various cloud platforms, there becomes more than one source for data.
The sra-toolkit will need to respond to this in a reasonable way.
Depending on the location of the user, some sources may be fast and cheap and others may be slow and costly to access.
Moreover, not all sources will provide the exact same data, e.g. some sources might not provide original spot names and quality scores.
There becomes a complicated and arbitrary matrix of choices.

Additionally, users have expected to be able to act directly on accessions that are not runs,
e.g. to run fastq-dump on an experiment accession and have that accession be automatically expanded to the set of run accessions.
Currently, our tools simply attempt to act on whatever input they are given, leaving it to VDB to make sense of the request.
This has lead to cryptic and unactionable error messages from VDB being presented to the user.
VDB doesn't know the context of the request, so it is unable to generate an actionable error message.
This needs to be done by the tool, preferably before any other action is attempted on the request.
But this is a complicated process often requiring two-way interaction with the user before anything can be done.

Rather than change all the tools to be able to perform these tasks, we will create a single tool to interact with the user.
This new tool will determine the proper objects and tools to satisfy the user's requests.
It will drive the tools with the correct URLs for the runs they are to work on.
