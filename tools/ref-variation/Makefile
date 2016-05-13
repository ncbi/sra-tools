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
MODULE = tools/ref-variation

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = \
	ref-variation \
	var-expand

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# ref-variation
#

CPFLAGS += -pthread 

REF_VARIATION_SRC = \
	ref-variation \
	common \
	helper

REF_VARIATION_OBJ = \
	$(addsuffix .$(OBJX),$(REF_VARIATION_SRC))

REF_VARIATION_LIB =   \
	-sncbi-ngs-c++    \
	-sncbi-vdb-static \
	-sngs-c++         \
	-skapp            \
	-stk-version      \
	-sncbi-vdb        \
	-sm               \
	-sload

$(BINDIR)/ref-variation: $(REF_VARIATION_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(REF_VARIATION_LIB)

#-------------------------------------------------------------------------------
# var-expand
#

#CPFLAGS += -pthread 

VAR_EXPAND_SRC = \
	var-expand \
	common \
	helper

VAR_EXPAND_OBJ = \
	$(addsuffix .$(OBJX),$(VAR_EXPAND_SRC))

VAR_EXPAND_LIB =   \
	-sncbi-ngs-c++    \
	-sncbi-vdb-static \
	-sngs-c++         \
	-skapp            \
	-stk-version      \
	-sncbi-vdb        \
	-sm               \
	-sload

$(BINDIR)/var-expand: $(VAR_EXPAND_OBJ)
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(VAR_EXPAND_LIB)
