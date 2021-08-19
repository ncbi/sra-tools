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
CFLAGS += -mmacosx-version-min=10.6
endif

CC = cc $(CFLAGS)
CXX = c++ $(CPPFLAGS)

# ensure definition of paths, namely
# NCBI_VDB_LIBDIR and NGS_LIBDIR
include $(CURDIR)/Makefile.config

TARGETS =         \
    AlignTest

# This rule triggers detection of the libraries and headers
# in addition to building the examples
std: $(NCBI_VDB_LIBDIR)/libncbi-vdb-static.a ncbi-headers \
	 $(NGS_LIBDIR)/libngs-c++.a ngs-headers \
	 $(NGS_BAM_LIBDIR)/libngs-bam-c++.a ngs-bam-headers \
	 $(TARGETS)

clean:
	rm -f $(TARGETS) *.o

.PHONY: default std $(TARGETS)

# C++ applications require two NGS libraries:
#  1. ngs-c++ which contains the NGS "front-end" interfaces
#  2. an NGS "back-end" engine such as ncbi-ngs-c++
#     which in turn relies upon ncbi-vdb to access the SRA
#
TEST_LIBS =              \
	-L$(NCBI_VDB_LIBDIR) \
	-L$(NGS_LIBDIR)      \
	-L$(NGS_BAM_LIBDIR)  \
	-lncbi-ngs-c++       \
	-lngs-c++            \
	-lncbi-vdb-static    \
	-lngs-bam-c++        \
	-lngs-adapt-c++      \
	-lpthread            \
	-ldl                 \
	-lm

# AlignTest #################
#  access alignments
ALIGN_TEST_SRC = \
    AlignTest.cpp

AlignTest: $(ALIGN_TEST_SRC)
	$(CXX) -g -o $@ $^ $(TEST_LIBS)

# ===========================================================================
#
# example runs

run_align: AlignTest
	./$^ ERR225922 1 1


.PHONY: run_align
