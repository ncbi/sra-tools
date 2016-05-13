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
# ===========================================================================


default: std

USE_GCC_IF_ICC = 1

TOP ?= $(abspath ../..)
MODULE = tools/nenctool

include $(TOP)/build/Makefile.env

INT_TOOLS =

EXT_TOOLS = \
	nenctool

ifeq ($(OS),linux)
EXT_TOOLS += \
	nenctest
endif

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
# nenctool
#  Copy files from one directory to another while cataloging the contents.
#  XML files can be redirected as well.
#
NENCTOOL_SRC = \
	nenctool

NENCTOOL_OBJ = \
	$(addsuffix .$(OBJX),$(NENCTOOL_SRC))

NENCTOOL_LIB = \
	-lkapp \
	-stk-version \
	-lvfs \
	-lkns \
	-lkrypto \
	-lkfg \
	-lkfs \
	-lkq \
	-lkproc \
	-lklib

$(BINDIR)/nenctool: $(NENCTOOL_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(NENCTOOL_LIB)


#-------------------------------------------------------------------------------
# nenctest
#  Copy files from one directory to another while cataloging the contents.
#  XML files can be redirected as well.
#
NENCTEST_SRC = \
	nenctest

NENCTEST_OBJ = \
	$(addsuffix .$(OBJX),$(NENCTEST_SRC))

NENCTEST_LIB = \
	-lkapp \
	-stk-version \
	-lvfs \
	-lkns \
	-lkrypto \
	-lkfg \
	-lkfs \
	-lkq \
	-lkproc \
	-lklib

$(BINDIR)/nenctest: $(NENCTEST_OBJ)
	$(LD) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(NENCTEST_LIB)

