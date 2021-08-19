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
    test-ngs

all std: $(TARGETS)

clean:
	rm -rf $(OBJDIR) $(BINDIR)/test-ngs*

.PHONY: default all std $(TARGETS)

test-ngs: $(BINDIR) $(OBJDIR) $(BINDIR)/test-ngs$(EXEX)

#-------------------------------------------------------------------------------
# test-ngs
#
TEST_NGS_SRC = \
    main

TEST_NGS_OBJ = \
	$(addprefix $(OBJDIR)/,$(addsuffix .$(OBJX),$(TEST_NGS_SRC)))

TEST_NGS_LIB = \
    -ltest_engine \
    -lngs-bind-c++ \
    -lngs-disp \

$(BINDIR)/test-ngs$(EXEX): $(TEST_NGS_OBJ) 
	$(LP) $(DBG) $(OPT) -o $@ $^ -L$(LIBDIR) -L$(ILIBDIR) $(TEST_NGS_LIB) 

runtests: std $(BINDIR)/test-ngs$(EXEX)
	@ export LD_LIBRARY_PATH=$(LIBDIR):$(LD_LIBRARY_PATH); echo "Running $^ ..."; $(BINDIR)/test-ngs$(EXEX)
