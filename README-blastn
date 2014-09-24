# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================


The NCBI SRA ( Sequence Read Archive )


Contact: sra-tools@ncbi.nlm.nih.gov
http://trace.ncbi.nlm.nih.gov/Traces/sra/std


Stand-alone BLAST searches against SRA runs in their native format.
-------------------------------------------------------------------

A stand-alone blastn application to perform BLAST searches directly against 
native SRA files is included in this distribution. This application has been
tested in-house at the NCBI, but has not been heavily used, so this should be
considered a preliminary (alpha) release to a few experienced users. A 64-bit
LINUX application has been built for this testing. 

The application is called "blastn_vdb".

The application can be invoked in much the same manner as the standard 
blastn application:

1) blastn_vdb -help or blastn_vdb -h will produce usage messages.

2) The BLAST+ command-line manual at http://www.ncbi.nlm.nih.gov/books/NBK1763/ 
provides more details on the options, though not all blastn options are
available with blastn_vdb. Some options simply do not apply to sequences in SRA
(e.g., -gilist is missing as these sequences have not been assigned GI's). Some
options have not yet been implemented (e.g., -num_threads is currently disabled).


To search cached or on-demand SRA objects.
------------------------------------------
An example blastn_vdb command-line would be:

./blastn_vdb -db "ERR039542 ERR047215 ERR039539 ERR039540" -query nt.test -out test.out

The file nt.test contains the query in FASTA format, and it will be searched against 
the reads in runs with accessions ERR039542 ERR047215 ERR039539 ERR039540.

If you have not already downloaded these objects using the vdb "prefetch" tool,
they will be retrieved on-demand from NCBI under standard configuration. For
alternative configuration information, please see the "README-vdb-config" file
in this distribution.

Searching with manually downloaded files.
-----------------------------------------
If you have manually downloaded files, e.g. via aspera or wget, etc., they may
be referred to as "local" files. You can pass one or more file paths to be used
collectively as the database. In this case the blastn_vdb command-line would be:

./blastn_vdb -db <SRR_file> -query <input_file> -out <output_file>
	
Where
<SRR_file> is the path (relative or absolute) and name of the SRRxxxxx file
<input_file> is a fasta file containing the sequence(s) to be BLASTed
<output_file> is the name specified for the output report of the blast search.

Example:

./blastn_vdb -db ./subdir/ERR039542.sra -query nt.test -out test.out

Querying multiple SRR files simultaneously:

./blastn_vdb -db "<SRR_file1> <SRR_file2> <SRR_file3>" -query <input.fa> -out <output_file>

Enclose the group of files to be included in the search set in "quotes", e.g.
"./SRR_file1.sra ./SRR_file2.sra ./SRR_file3.sra"

Example:

./blastn_vdb -db "./ERR039542.sra ./ERR047215.sra ./ERR039539.sra ./ERR039540.sra" -query nt.test -out test.out

Caveats
-------
There are some limitations on the currently available application:

1) Individual SRA data files containing more than 2 billion reads are not yet supported. For a 
paired-end experiment this is actually a limitation of about 1 billion "spots".

2) Compressed SRA ("cSRA") is not yet fully supported. Currently, only the
unaligned fraction of reads are searched. Compressed SRA are runs containing
alignments (e.g., ERR230455). Runs can be checked with "vdb-dump" to report if
they contain alignment information:

    $ vdb-dump -E ERR230455
    enumerating the tables of database 'ERR230455'
    tbl #1: PRIMARY_ALIGNMENT
    tbl #2: REFERENCE
    tbl #3: SEQUENCE

3) You may need to prefix "./" to the run name for files in your current
directory.

4) The blast_formatter is not currently able to read native SRA files, so 
reformatting of results saved as a blast archive is not yet supported.

Common errors and fixes.
------------------------

1) Failure to provide relative path to manually downloaded SRR file:

./blastn_vdb -db SRR770754.sra -query srr770754_test.fa -out test.out
Error: NCBI C++ Exception:
    "vdb2blast_util.cpp", line 253: Error: ncbi::CVDBBlastUtil::x_MakeSRASeqSrc() 
    - VDB BlastSeqSrc construction failed: Failed to add any run to VDB runset: unsupported while allocating
    
Fix:
Include relative (e.g., "../" or "./") or absolute (e.g., "/home/user/SRA_BLAST_data/") file path with SRR file
