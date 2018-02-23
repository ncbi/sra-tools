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

runtests:

TOP ?= $(abspath ../..)

MODULE = test/fuse

TEST_TOOLS =

include $(TOP)/build/Makefile.env

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# remote-fuser-test
#
REMOTE_FUSER_TEST_SRC = \
	remote-fuser-test   \
	utils

REMOTE_FUSER_TEST_OBJ = \
	$(addsuffix .$(OBJX),$(REMOTE_FUSER_TEST_SRC))
    
REMOTE_FUSER_TEST_LIB =   \
	-sncbi-vdb-static   \
	-skapp

$(TEST_BINDIR)/remote-fuser-test: $(REMOTE_FUSER_TEST_OBJ)
	$(LP) --exe -o $@ $^ $(REMOTE_FUSER_TEST_LIB)

#-------------------------------------------------------------------------------
# slowtests: match output vs sra-pileup
#

ifeq (linux,$(BUILD_OS))
    ifneq (Ubuntu,$(OS_DISTRIBUTOR))
        ifeq (,$(NOT_VERY_SLOW))
slowtests: run-test

run-test: $(TEST_BINDIR)/remote-fuser-test
	@$(SRCDIR)/remote_fuser_test.sh standard 180 $(TARGDIR)/bin

        else
slowtests: no-test
no-test:
	@echo SKIPPING VERY SLOW remote-fuser-test
        endif
    else
slowtests:
    endif
else
slowtests:
endif

.PHONY: run-test
