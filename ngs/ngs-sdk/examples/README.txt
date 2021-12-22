/*===========================================================================
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
*/

This file contains instructions on building and running NGS C++ example programs.

The source code is contained in *.cpp files. Each source file corresponds to a separate executable that demonstrates use of a particular feature of the NGS C++ API.

To be built, the executables require some external libraries and the corresponding headers. See 7) for default locations and overrides.

1. Build all
    - run "make". This will build executables for all examples and place them in the current directory.

2. Build separately
    - run "make <example>", where <example> is one of "AlignSliceTest", "AlignTest", "DumpReferenceFASTA", "FragTest", "PileupTest", "RefTest". This will build (if necessary) and run the selected example with predefined arguments.

3. Run all
    - run "make run_all". This will run all examples with their predefined arguments. The output of all the examples goes to stdout and for some of them there is a lot of it, so it is advisable to redirect stdout to a file, e.g. "make run_all >output.txt"

4. Run and diff
    - run "make run_and_diff". This will execute all the examples with their predefined arguments, run, them, capture the output in a temporary file, and then use "diff" to compare the actual output against a file containing the expected output (./expected.txt). Any discrepancies will be displayed. If the actual output matches the expected, there will be a message "NGS C++ examples work as expected" at the end of execution.

5. Run separately
    - run "make <example>" where <example" is one of "run_frag", "run_align", "run_dump", "run_align_slice", "run_pileup", "run_ref". This will run the selected example with some predefined arguments.
    - In order to run an individual executable with custom arguments, do so from the command line, e.g. "./AlignSliceTest SRR1121656 1 2 9999". When run without arguments, the executables will output a very short description of required parameters.

6. Static vs Dynamic Linking
    - The executables can be build using static or dynamic linkage. Static linking is the default.
    - To build dynamically, run "make DYNAMIC=1". Executables built with this option will require the path to libncbi-ngs.so to be in $LD_LIBRARY_PATH at the time of execution.

7. Alternative Library Locations
    - By default, the makefile accesses the required object liraries and headers via paths relative to the current directory, assuming that the examples are a part of a standard installation of sra-tools.
    - To change the location of the libraries and/or headers to use, specify the following varibles on the make command (e.g. "make NGS_INCDIR=/some-path/"):
        -- NCBI_VDB_LIBDIR - the location of libncvi-vdb.a (default ../../../ncbi-vdb/lib64). Used for static linking only
        -- NCBI_VDB_INCDIR - the location of VDB header files (default ../../../ncbi-vdb/interfaces)
        -- NGS_LIBDIR - the location of NGS object libraries (default ../../lib64)
        -- NGS_INCDIR - the location of NGS C++ headers (default ../../include)

8. Clean
    - run "make clean" to remove the executables.
