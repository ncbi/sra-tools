===========================================================================
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
===========================================================================

***************************************************************************
*
*        AccessionTester
*
***************************************************************************

AccessionTester is a java commandline-tool to perform a large blackbox-test
on a selection of SRA-tools and accessions.

The tool needs some 'infrastructure' to work:

(1) binary sra-tools: and a known path to them:
        ( fastq-dump, vdb-dump, sam-dump, sra-pileup ... )
    and a known path to them, for instance in:
        "~/devel/ncbi/OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/"

(2) either an existing "AccessionTester.jar" or the sources in "./src".
    "AccessionTester.jar" has no other dependencies than the standard JDK.

(3) a subdirectory with *.TEST and *.PLATFORM files ( default : ./tests )

(4) an accession-file ( default: acc.txt )

(5) the current directory has to be writable ( log-file, md5-file )


Example of manually starting the test:

$java -jar AccessionTester.jar -t ~/devel/ncbi/OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/


What is the tool doing in detail?
It reads the accession-file. This file is a comma-separated text-file.

The fields are:
OBJECT-TYPE,ACCESSION,TAB_OR_DB,PLATFORM,SLICE
The SLICE-field is optional.

examples without and with a slice:
NCBI:SRA:_454_:tbl:v2#1.0.7-454-fastq-load(2.0.2)-T,SRR124144,T,454
NCBI:align:db:alignment_sorted#1.2-ILLUMINA-bam-load(2.1.16)-D,SRR501069,D,ILLUMINA,gi|169794206|ref|NC_010410.1|:15001-20000

For each line it first takes the platform and tries to find out which tests
to run for this platform. This information is found in platform-files. They
are named like '454.PLATFORM' or 'ILLUMINA.PLATFORM', and there is a common
one named 'COMMON.PLATFORM'. These files are text files with the name of
one test per line. For example the 'COMMON.PLATFORM'-file looks like this:

FASTA.t.test
FASTQ.t.test
FASTA.d.test
FASTQ.d.test
VDBDUMP.t.test
VDBDUMP.d.test
PILEUP.test
SAMDUMP.test

In case of the first example we have a platform of '454'. The tool tries to
find the files 'COMMON.PLATFORM' and '454.PLATFORM'. If both of them are missing,
the tool does not process this line in the accession-file. If it found at least
one of the 2 platform-files, it mixes them into a list of tests to run.
For each test a job will be scheduled. A test file looks like this:

FATA.t.test:

tool=fastq-dump
database=false
args={acc} -Z -X 10 --fasta
timeout=20

The tool will run the executable 'fastq-dump' like this:
~/devel/ncbi/OUTDIR/sra-tools/linux/gcc/x86_64/dbg/bin/fastq-dump SRR124144 -Z -X 10 --fasta

The tool will capture the output ( stdout and stderr combined ), the return-code
and the time it took to run. The output is used to created a md5-sum.

A test is considered a success if the return-code is zero, the run time did not
exeed the timeout-time ( in seconds ) from the test-file, the output had at least
one byte and the md5-sum matches the one in the md5-sum-file. If the md5-sum-file
had no entry for this test yet ( because the tool is run for the first time, or
a test and/or a accession has been added ) then the test is still treaded as
a success and the new md5-sum is entered into the md5-sum-file.

If all tests are a success then the tool itself returns zero as return-code.
Otherwise it returns the sum of errors, md5-sum-mismatches, timeouts and empty
outputs.

The tool recognizes these options:

	--tools   -t : use the binary tools at this path (no default!)
    --acc     -a : file containing the accession to test (dflt: 'acc.txt')
    --tests   -e : path to the test-files (dflt: './tests' )
    --md5     -m : file containing the md5-sums (dflt: './md5.txt')
    --threads -n : use this many threads (dflt: 12)
    --log     -l : log file path (dflt: '.') , '-' do not write log
    --run     -u : run only these tests
    --force   -f : overwrite md5-sum in md5-file if different
	--ordered -o : process accessions in the order found in the file
    --help    -h : print this help


In case a tool or an accession has been changed/fixed and now produces a
different md5-sum as output, the tool can be run with the --force option.
The tool will then not create an error because of the md5-sum-mismatch,
but update the md5-sum-file.

It is also possible to run a subset of the accessions in the input-file.

$java -jar AccessionTester.jar -t ~/...../bin/ SRR124144 SRR501069

runs the tests only on these 2 accessions, however the given accessions
must be found in the input-file - you cannot give any accession.

You can also limit the tests to be run:

$java -jar AccessionTester.jar -t ~/...../bin/ --run FASTA.t.test

runs only this test. It is also possible to combine the 2 restrictions:

$java -jar AccessionTester.jar -t ~/...../bin/ --run FASTA.t.test SRR124144

runs only the test FASTA.t.test on the accession SRR124144.

