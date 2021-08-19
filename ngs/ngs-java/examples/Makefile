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


default: std

#DEBUGGING = -d

TARGETS =             \
	NGS-JavaTest.jar

std: $(TARGETS)

clean:
	rm -f $(TARGETS) examples/*.class

.PHONY: default std $(TARGETS)

NGS_EXAMPLES = \
	AlignSliceTest \
	AlignTest \
	DumpReferenceFASTA \
	FragTest \
	PileupTest \
	ReadGroupTest \
	RefTest \

NGS_EXAMPLES_PATH = \
	$(addprefix examples/,$(addsuffix .java,$(NGS_EXAMPLES)))

NGS_CLASS_PATH = ..

NGS-JavaTest.jar: $(NGS_EXAMPLES_PATH)
	javac $(DEBUGGING) $^ -d .
	( jar cf $@ `find examples -name "*.class"`; chmod -x,o-w $@ ) || ( rm -f $@ && false )

# ===========================================================================
#
# example runs

JAVAFLAGS = -classpath $(CLASSPATH):$(NGS_CLASS_PATH):.

# Expect libngs-sdk.so and libncbi-vdb.so somewhere inside $LD_LIBRARY_PATH;
#   if the libraries are not found, error out.
# Comment out to rely on "smart load" instead (may download libraries from NCBI):
JAVAFLAGS += -Djava.library.path=$(LD_LIBRARY_PATH) -Dvdb.System.loadLibrary=1

# uncomment to see detailed log"
#JAVAFLAGS += -Dvdb.log=FINEST

run_frag: NGS-JavaTest.jar
	java $(JAVAFLAGS) examples.FragTest ERR225922 10000 2  $(REDIRECT)

run_align: NGS-JavaTest.jar
	java $(JAVAFLAGS) examples.AlignTest ERR225922 10000 2 $(REDIRECT)

run_dump: NGS-JavaTest.jar
	java $(JAVAFLAGS) examples.DumpReferenceFASTA SRR520124 1 $(REDIRECT)

run_align_slice: NGS-JavaTest.jar
	java $(JAVAFLAGS) examples.AlignSliceTest SRR1121656 1 1 9999 $(REDIRECT)

run_pileup: NGS-JavaTest.jar
	java $(JAVAFLAGS) examples.PileupTest SRR1121656 1 9999 10003 $(REDIRECT)

run_ref: NGS-JavaTest.jar
	java $(JAVAFLAGS) examples.RefTest SRR1121656 $(REDIRECT)
    
run_read_group: NGS-JavaTest.jar
	java $(JAVAFLAGS) examples.ReadGroupTest SRR1121656 $(REDIRECT)
    
ALL_TESTS = run_frag run_align run_align_slice \
	run_pileup run_ref run_read_group run_dump

run_all: $(ALL_TESTS)

.PHONY: $(ALL_TESTS)

# ===========================================================================
#
# expected results for auto-testing
#
expected.txt update_expected: 
	rm -f expected.txt; LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) $(MAKE) run_all REDIRECT=">>expected.txt"

run_and_diff: expected.txt  
	@ rm -f actual.txt; LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) $(MAKE) run_all REDIRECT=">>actual.txt"
	@ diff expected.txt actual.txt && rm actual.txt && echo NGS Java examples work as expected

.PHONY: update_expected run_and_diff
