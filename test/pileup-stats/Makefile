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

MODULE = test/pileup-stats

TEST_TOOLS = \

ALL_TOOLS = \
	$(TEST_TOOLS) \

include $(TOP)/build/Makefile.env

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

runtests: pileup-stats ignore-args

#-------------------------------------------------------------------------------
# pileup-stats tool tests
#

pileup-stats: $(BINDIR)/pileup-stats
	@ # The line below is the test for --version output. Yes, this is ridiculous to make tests like this (especially the one depending on the path to the sources of the binary being tested) but Anatoly insisted on it here: VDB-2837
	@ $(BINDIR)/pileup-stats -V | sed -e 's/\([1-9][0-9]*\)\.0\.0/\1/' -e 's/\([1-9][0-9]*\.[1-9][0-9]*\)\.0/\1/' -e 's/\([1-9][0-9]*\.[0-9][0-9]*\.[1-9][0-9]*\)/\1/' | grep --quiet "$(shell head -n 1 $(TOP)/tools/pileup-stats/pileup-stats.vers)[^0-9\.]"

slowtests: optional-column-ref-offset-type

optional-column-ref-offset-type:
	@ $(BINDIR)/pileup-stats --align-category primary ERR085308 >/dev/null

vg: $(BINDIR)/pileup-stats
	@ # TODO: a better command line options should be chosen to test for memory leaks
	valgrind --ncbi --suppressions=$(SRCDIR)/valgrind.suppress $(BINDIR)/pileup-stats -V

ignore-args: test-ignore-args.sh
	@ echo "starting test-ignore-args..."
	@ bash test-ignore-args.sh