# ==============================================================================
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
# ==============================================================================

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

runtests: test_bases Mismatch

slowtests: slow_bases

quick_bases:
	@rm   -rf actual
	@mkdir -p actual

	@echo
	@echo SRR053325 is a small table
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR053325 > actual/SRR053325
	diff actual/SRR053325 expected/SRR053325-biological

	@echo
	@echo SRR600096 is a small non-cSRA DB
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR600096 > actual/SRR600096
	diff actual/SRR600096 expected/SRR600096

	@echo
	@echo SRR618333 is a small CS_NATIVE table
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR618333 > actual/SRR618333
	diff actual/SRR618333 expected/SRR618333

	@echo
	@echo SRR413283 is a small cSRA with local references
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR413283 > actual/SRR413283
	diff actual/SRR413283 expected/SRR413283-with-AssemblyStatistics

	@echo
	@echo SRR619505 is a small cSRA with N-s without local references
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR619505 | perl -w strip-path.pl \
	                                                > actual/SRR619505
	diff actual/SRR619505 expected/SRR619505

	@echo
	@echo SRR1985136 is a small cSRA with a local reference and 0-length tech-rd
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR1985136 | perl -w strip-path.pl \
                                                    > actual/SRR1985136
	diff actual/SRR1985136 expected/SRR1985136-with-Changes

	@echo
	@rm actual/*

R=SRR8483030
test_bases: quick_bases
	@echo
	@echo ${R} is a run having first 0-lenght bio reads
	if perl check-run-perm.pl $(BINDIR)/srapath ${R} > /dev/null 2>&1 ; \
	then                      $(BINDIR)/sra-stat -x ${R} > /dev/null ; fi

	@echo
	@echo SRR495844 is a cSRA with local references
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR495844 | perl -w strip-path.pl \
                                                    > actual/SRR495844
	diff actual/SRR495844 expected/SRR495844-with-Changes

	@echo
	@echo SRR390427 is a non-cSRA DB
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR390427 > actual/SRR390427
	diff actual/SRR390427 expected/SRR390427

	@echo
	@echo SRR360929 is a table
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR360929 > actual/SRR360929
	diff actual/SRR360929 expected/SRR360929-biological

	@echo
	@rm    actual/*
	@rm -r actual

slow_bases:
	@rm   -vrf actual
	@mkdir -vp actual

	@echo
	@echo SRR586259 is a cSRA without local references
	NCBI_SETTINGS=/ $(BINDIR)/sra-stat -x SRR586259 | perl -w strip-path.pl \
                                                    > actual/SRR586259
	diff actual/SRR586259 expected/SRR586259

	@echo
	@rm -v  actual/*
	@rm -vr actual

all_bases: test_bases slow_bases

Mismatch:
	@mkdir -p actual
	@$(BINDIR)/sra-stat db/SRR6336806.Mismatch >/dev/null 2> actual/Mismatch
	@if grep -q 'Mismatch between calculated and recorded statis' actual/Mismatch;\
	 then echo "Mismatch between calculated and recorded statists was found"; \
		exit 1; fi
	echo Mismatch OK
	@rm -r actual

slowest_bases:
	NCBI_SETTINGS=/ time $(BINDIR)/sra-stat -xp SRR5362833

#-------------------------------------------------------------------------------
