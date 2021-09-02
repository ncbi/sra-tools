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

MODULE = test/copycat

TEST_TOOLS = \

include $(TOP)/build/Makefile.env

DIRTOTEST ?= $(BINDIR)
TOOLTOTEST = $(DIRTOTEST)/copycat
ifneq ("$(wildcard $(TOOLTOTEST))","")
    BIN_EXISTS = 1
else
    BIN_EXISTS = 0
endif

$(TEST_TOOLS): makedirs
	@ $(MAKE_CMD) $(TEST_BINDIR)/$@

.PHONY: $(TEST_TOOLS)

clean: stdclean

#-------------------------------------------------------------------------------
# scripted tests
#
ifeq (1,$(HAVE_MAGIC))
  ifeq (0,$(BIN_EXISTS))
runtests: no-test
  else
runtests: copy
  endif
else
runtests:
	@ echo "NOTE - copycat tests are skipped:"          \
		"copycat was not built"
	@ echo "because it requires our internal library 'libkff'" \
		"which requires 'libmagic' and its development headers."
endif

no-test:
	@ echo $(TOOLTOTEST) does not exist. Test skipped.

copy: 
	@ echo "Testing $(DIRTOTEST)/copycat"
	@ rm -rf actual
	@ $(DIRTOTEST)/copycat -h >/dev/null
	@ export PATH=$(DIRTOTEST):$(PATH); vdb-config | grep bin > /dev/null && \
	 copycat ./input/1.xml actual/ >/dev/null && diff ./input/1.xml actual/1.xml\
	    > /dev/null
	@ rm -rf actual
