============================================================================
    sam-analyze - tool
============================================================================

The purpose of this tool is to analyze SAM- or BAM-files.
If a BAM-file has to be analyzed, it has to be converted into the SAM-format.
( In the future there might be an option to let this tool perform that task too.
  Another possible future function would be to import directly from a cSRA-accession. )

----------------------------------------------------------------------------

The tool can:

1) import a SAM-file and write it into a SQLITE-database

2) analyze the SQLITE-database for problems ( FLAGS etc. ), print a report

3) export all or some of the SPOTS written to the SQLITE-database in SAM-format

4) create a reference-report ( frequencies of reference-usage by spots )

----------------------------------------------------------------------------

The tool can be used to:

- explore what is wrong with a given SAM/BAM-file

- predict if loading a SAM/BAM-file with bam-load would succeed
    ( faster than probing with bam-load itself )

- fix problems before running bam-load ( for instance fixing invalid names )

- create short examples from existing SAM/BAM-files or cSRA-accessions
    for the purpose of creating fast tests for other tools

----------------------------------------------------------------------------

The tool leaves a SQLITE-database behind, which can be manually examined
with 'sqlite3' - the SQLITE-commandline-tool or other SQL-explorer-tools.
( In the future there might be an option to remove the database after
 the EXPORT-step. )

The sqlite-library is statically linked into the tool - it does not need
to be installed for the tool to function.

----------------------------------------------------------------------------

IMPORT:
=======

just import the SAM-file 'filename.SAM'
    $./sam-analyze -i filename.SAM
    ( the produced SQLITE-database defaults to 'sam.db' )

import the same file, but show progress
    $./sam-analyze -i filename.SAM -p

import the same file, show progress and print report
    $./sam-analyze -i filename.SAM -pr

import the same file, use 'example.db' as output
    $./sam-analyze -i filename.SAM -d example.db

import the same file, into an in-memory-database
    $./sam-analyze -i filename.SAM -d :memory:
    ( this makes no sense on its own, but if the import is followed by analyze and export
      and the machine has lots of RAM, this can be faster )

import the same file, increase transaction size to 100k ( default=50k )
    $./sam-analyze -i filename.SAM -t 100000
    ( the transaction-size is in lines imported, the higher the faster )

import the same file via stdin
    $cat filename.SAM | ./sam-analyze -i stdin
    ( the special name 'stdin' is used to import via a pipe )

import the same file, but limit the number of alignments read to 1000
    $./sam-analyze -i filename.SAM -l 1000
    ( the limit is on the number of alignments, all headers are imported,
      this can result in half-aligned spots, even if the source does not have them )

----------------------------------------------------------------------------

