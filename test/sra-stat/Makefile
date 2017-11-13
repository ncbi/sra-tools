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

default: runtests

TOP ?= $(abspath ../..)

MODULE = test/sra-stat

TEST_TOOLS = \
	testAssemblyStatistics

include $(TOP)/build/Makefile.env

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------

SRC = \
	testAssemblyStatistics

OBJ = \
	$(addsuffix .$(OBJX),$(SRC))

LIB = \
	-skapp \
	-sktst \
	-sncbi-vdb \

$(TEST_BINDIR)/testAssemblyStatistics: $(OBJ)
	$(LP) --exe -o $@ $^ $(LIB)

#-------------------------------------------------------------------------------

slowtests: test_bases

test_bases:
	@rm   -vrf actual
	@mkdir -vp actual

	@echo
	@echo SRR360929 is a table
	$(BINDIR)/sra-stat -x SRR360929 > actual/SRR360929
	diff actual/SRR360929 expected/SRR360929

	@echo
	@echo SRR495844 is a cSRA with local references
	$(BINDIR)/sra-stat -x SRR495844 > actual/SRR495844
	diff actual/SRR495844 expected/SRR495844-with-Changes

	@echo
	@echo SRR586259 is a cSRA without local references
	$(BINDIR)/sra-stat -x SRR586259 > actual/SRR586259
	diff actual/SRR586259 expected/SRR586259

	@echo
	@rm       actual/*
	@rmdir -v actual

#-------------------------------------------------------------------------------
