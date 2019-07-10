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

MODULE = test/tui-example

TEST_TOOLS =

include $(TOP)/build/Makefile.env

INT_TOOLS = \

EXT_TOOLS = \
    tui-example

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)
	
#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# all
#
$(TARGDIR)/all: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/all

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(EXT_TOOLS))

.PHONY: $(TEST_TOOLS) run9

#-------------------------------------------------------------------------------
# the tool
#
TOOL_SRC = \
	example_0 \
	example_1 \
	example_2 \
	example_3 \
	example_4 \
	example_5 \
	example_6 \
	example_7 \
	example_8 \
	example_9 \
	example_10 \
	tui_examples

TOOL_OBJ = \
	$(addsuffix .$(OBJX),$(TOOL_SRC))

TOOL_LIB = \
	-stui_cpp    \
	-stui        \
	-skapp       \
	-stk-version \
	-sncbi-vdb   \
	-sm

   
$(BINDIR)/tui-example: $(TOOL_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(TOOL_LIB) \
	-ltui_cpp \
	-ltui \
	-ldl \
	-lstdc++

clean:
	rm -f *.o *.d
    
run9:
	$(BINDIR)/tui-example 9

run10:
	$(BINDIR)/tui-example 10
