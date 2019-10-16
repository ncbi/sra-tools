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
MODULE = tools/driver-tool

INT_TOOLS =

EXT_TOOLS = \
	sratools

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env
CPFLAGS += -std=c++11
CPFLAGS += -I$(SRCDIR)/inc
LDFLAGS += -L$(SRCDIR)/utf8proc

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (win,$(OS))
all std:
	@ echo "not building sratools under Windows"
else
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn
endif

$(ALL_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@

.PHONY: all std $(ALL_TOOLS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# sratools
#

$(SRCDIR)/utf8proc/libutf8proc.a:
	$(MAKE) -C $(SRCDIR)/utf8proc

$(ILIBDIR)/libsecure.$(LIBX):
	$(MAKE) -C $(SRCDIR)/secure

$(ILIBDIR)/libjson.$(LIBX):
	$(MAKE) -C $(SRCDIR)/json

SRATOOLS_SRC = \
	sratools \
	split_path \
	config \
	proc \
	which \
	tool-args \
	parse_args \
	run-source \
	fastq-dump

SRATOOLS_OBJ = \
	$(addsuffix .$(OBJX),$(SRATOOLS_SRC))

SRATOOLS_LIB = \
	-sutf8proc \
	-ssecure \
	-sjson \
	-lm

$(BINDIR)/sratools: $(SRATOOLS_OBJ) $(ILIBDIR)/libjson.$(LIBX) $(ILIBDIR)/libsecure.$(LIBX) $(SRCDIR)/utf8proc/libutf8proc.a
	$(LP) --exe --vers $(SRCDIR)/../../shared/toolkit.vers -o $@ $^ $(SRATOOLS_LIB)
