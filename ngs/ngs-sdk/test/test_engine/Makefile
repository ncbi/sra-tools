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

TOP ?= $(abspath ../../../..)
include $(TOP)/build/Makefile.config
INCDIRS += -I$(TOP)/ngs/ngs-sdk/ngs/$(OSINC)/$(ARCH)

TARGETS =      \
    test_engine

all std: $(TARGETS)

clean:
	rm -rf $(LIBDIR)/$(LPFX)test_engine* $(OBJDIR)

.PHONY: default all std $(TARGETS)

test_engine: $(LIBDIR) $(OBJDIR) $(LIBDIR)/$(LPFX)test_engine.$(SHLX)

runtests: test_engine

#-------------------------------------------------------------------------------
# test-ngs
#
TEST_ENGINE_SRC = \
    test_engine

TEST_ENGINE_OBJ = \
	$(addprefix $(OBJDIR)/,$(addsuffix .$(LOBX),$(TEST_ENGINE_SRC)))

TEST_ENGINE_LIB = \
    -lngs-adapt-c++ \
	-lngs-bind-c++ \
	-lngs-disp

$(LIBDIR)/$(LPFX)test_engine.$(SHLX): $(LIBDIR)/$(LPFX)ngs-adapt-c++.$(LIBX) $(TEST_ENGINE_OBJ) 
	$(LP) $(DBG) $(OPT) --shared -o $@ $(TEST_ENGINE_OBJ) -L$(LIBDIR) -L$(ILIBDIR) $(TEST_ENGINE_LIB)

$(LIBDIR)/$(LPFX)ngs-adapt-c++.$(LIBX):
	@ echo missing $@
	@ false
