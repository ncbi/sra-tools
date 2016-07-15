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

TOP ?= $(abspath ../..)
MODULE = tools/rd-filter-redact

INT_TOOLS = \
	test-read-write-cursor

EXT_TOOLS = \
	read-filter-redact

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

#------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# read-filter-redact
#
READ_FILTER_REDACT_SRC = \
	read-filter-redact \

READ_FILTER_REDACT_OBJ = \
	$(addsuffix .$(OBJX),$(READ_FILTER_REDACT_SRC))

READ_FILTER_REDACT_LIB = \
	-lkapp \
	-stk-version \
	-sncbi-wvdb \
	-lm

$(BINDIR)/read-filter-redact: $(READ_FILTER_REDACT_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(READ_FILTER_REDACT_LIB)

#-------------------------------------------------------------------------------
# test-read-write-cursor
#
TEST_READ_WRITE_CURSOR_SRC = \
	test-read-write-cursor \

TEST_READ_WRITE_CURSOR_OBJ = \
	$(addsuffix .$(OBJX),$(TEST_READ_WRITE_CURSOR_SRC))

TEST_READ_WRITE_CURSOR_LIB = \
	-lkapp \
	-stk-version \
	-lncbi-wvdb \

$(BINDIR)/test-read-write-cursor: $(TEST_READ_WRITE_CURSOR_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(TEST_READ_WRITE_CURSOR_LIB)

