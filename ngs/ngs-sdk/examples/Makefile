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

ifeq (Darwin,$(shell uname -s))
    MFLAGS += -mmacosx-version-min=10.10
endif

CC = gcc $(MFLAGS) $(CFLAGS)
CXX = g++ $(MFLAGS) $(CPPFLAGS)

TOP ?= $(abspath ../../..)
# ensure definition of paths, namely
# NCBI_VDB_LIBDIR and NGS_LIBDIR
include $(CURDIR)/Makefile.config
#$(warning CPPFLAGS=$(CPPFLAGS))
$(info $$INCDIRS BEFORE is [${INCDIRS}])
INCDIRS += -I$(TOP)/ngs/ngs-sdk
$(info $$INCDIRS AFTER is [${INCDIRS}])

TARGETS =               \
	AlignSliceTest      \
	AlignTest           \
	DumpReferenceFASTA  \
	FastqTableDump      \
	FragTest            \
	PileupTest          \
	RefTest             \

# This rule triggers detection of the libraries and headers
# in addition to building the examples
std: $(TARGETS)

$(TARGETS): $(NCBI_VDB_LIBDIR)/libncbi-vdb-static.a ncbi-headers $(NGS_LIBDIR)/libngs-c++.a ngs-headers

clean:
	echo $(LD_LIBRARY_PATH)
	rm -f $(TARGETS) *.o
	rm -rf *.dSYM

.PHONY: default std $(TARGETS)

# C++ applications require several static libraries:
#  1. libngs-c++.a and libngs-adapt-c++.a which contain the NGS "front-end" interfaces.
#  2. an NGS "back-end" engine such as libncbi-ngs-c++.a
#     which in turn relies upon ncbi-vdb to access the SRA (libncbi-ngs.a, libncbi-vdb-static.a)
#
TEST_LIBS =              \
	-L$(NCBI_VDB_LIBDIR) \
	-L$(NGS_LIBDIR)      \
	-lncbi-ngs-c++       \
	-lncbi-ngs           \
	-lncbi-vdb-static    \
	-lngs-c++            \
	-lngs-adapt-c++      \
	-lpthread            \
	-ldl                 \
	-lm                  \


# AlignSliceTest
#  slice a ReadCollection and output alignments
ALIGNSLICE_TEST_SRC = \
	AlignSliceTest.cpp

AlignSliceTest: $(ALIGNSLICE_TEST_SRC)
	$(CXX) -g -o $@ $(ALIGNSLICE_TEST_SRC) $(TEST_LIBS) $(INCDIRS)


# AlignTest #################
#  access alignments
ALIGN_TEST_SRC = \
	AlignTest.cpp

AlignTest: $(ALIGN_TEST_SRC)
	$(CXX) -g -o $@ $(ALIGN_TEST_SRC) $(TEST_LIBS) $(INCDIRS)


# DumpReferenceFASTA
DUMP_SRC = \
	DumpReferenceFASTA.cpp
DumpReferenceFASTA: $(DUMP_SRC)
	$(CXX) -g -o $@ $(DUMP_SRC) $(TEST_LIBS) $(INCDIRS)


# FastqTableDump
#  produce fastq-like table
FASTQ_TABLE_DUMP_OBJ = \
	FastqTableDump.cpp

FastqTableDump: $(FASTQ_TABLE_DUMP_OBJ)
	$(CXX) -g -o $@ $(FASTQ_TABLE_DUMP_OBJ) $(TEST_LIBS) $(INCDIRS)


# FragTest
#  access read fragments
FRAG_TEST_SRC = \
	FragTest.cpp

FragTest: $(FRAG_TEST_SRC)
	$(CXX) -g -o $@ $(FRAG_TEST_SRC) $(TEST_LIBS) $(INCDIRS)


# PileupTest
#  slice a ReadCollection and produce pileups
PILEUP_TEST_OBJ = \
	PileupTest.cpp

PileupTest: $(PILEUP_TEST_OBJ)
	$(CXX) -g -o $@ $(PILEUP_TEST_OBJ) $(TEST_LIBS) $(INCDIRS)


# RefTest ###################
REF_TEST_SRC = \
	RefTest.cpp

RefTest: $(REF_TEST_SRC)
	$(CXX) -g -o $@ $(REF_TEST_SRC) $(TEST_LIBS) $(INCDIRS)

# ===========================================================================
#
# installation
#
install:
	@ cp *.cpp Makefile Makefile.config $(INST_TARGET)

# ===========================================================================
#
# example runs

run_frag:  FragTest
	./$^ ERR225922 10000 2 $(REDIRECT)

run_align: AlignTest
	./$^ ERR225922 10000 2 $(REDIRECT)

run_dump: DumpReferenceFASTA
	./$^ SRR520124 1 $(REDIRECT)

run_align_slice: AlignSliceTest
	./$^ SRR1121656 1 1 9999 $(REDIRECT)

run_pileup: PileupTest
	./$^ SRR1121656 1 9999 10003 $(REDIRECT)

run_ref: RefTest
	./$^ SRR1121656 $(REDIRECT)

ALL_TESTS = run_frag run_align run_align_slice run_pileup run_ref run_dump

run_all: $(ALL_TESTS)

.PHONY: $(ALL_TESTS)

# ===========================================================================
#
# expected results for auto-testing
#
expected.txt update_expected:
	rm -f expected.txt
	export LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) ; $(MAKE) run_all REDIRECT=">>expected.txt"

run_and_diff: expected.txt
	@ rm -f actual.txt
	@ echo http_proxy=$(http_proxy)
	@ export LD_LIBRARY_PATH=$(LD_LIBRARY_PATH) ; $(MAKE) run_all REDIRECT=">>actual.txt"
	@ diff expected.txt actual.txt && rm actual.txt && echo "NGS C++ examples work as expected"

.PHONY: update_expected run_and_diff install
