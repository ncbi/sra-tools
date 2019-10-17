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

TOP ?= $(abspath ../../..)
MODULE = tools/driver-tool/json

INT_LIBS = \
	libjson

ALL_LIBS = \
	$(INT_LIBS) \
	$(EXT_LIBS)

include $(TOP)/build/Makefile.env
SECURE_DIR = $(abspath $(SRCDIR)/../secure)
UTF8PROC_DIR = $(abspath $(SRCDIR)/../utf8proc)

CPFLAGS += -std=c++11
CPFLAGS += -I$(abspath $(SRCDIR)/../inc)

#-------------------------------------------------------------------------------
# outer targets
#
ifeq (win,$(OS))
all std:
	@ echo "not building json under Windows"
else
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@

$(INT_LIBS): makedirs
	@ $(MAKE_CMD) $(ILIBDIR)/$@

endif

.PHONY: all std $(ALL_LIBS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(ILIBDIR)/,$(INT_LIBS)) \
	$(addprefix $(LIBDIR)/,$(EXT_LIBS)) \
	$(addprefix $(BINDIR)/,$(EXT_TOOLS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# all
#
$(TARGDIR)/all: \
	$(addprefix $(ILIBDIR)/,$(INT_LIBS)) \
	$(addprefix $(LIBDIR)/,$(EXT_LIBS)) \
	$(addprefix $(BINDIR)/,$(ALL_TOOLS))

.PHONY: $(TARGDIR)/all

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean
	@ $(MAKE) SRCDIR=$(UTF8PROC_DIR) -C $(UTF8PROC_DIR) clean
	@ $(MAKE) SRCDIR=$(SECURE_DIR) -C $(SECURE_DIR) clean

.PHONY: clean

#-------------------------------------------------------------------------------
# json
#
$(ILIBDIR)/libjson: $(ILIBDIR)/libjson.$(LIBX)

$(UTF8PROC_DIR)/utf8proc.o:
	@ $(MAKE) -C $(UTF8PROC_DIR)

$(ILIBDIR)/libsecure.$(LIBX):
	@ $(MAKE) -C $(SECURE_DIR)

JSON_SRC = \
	json \
	json-array \
	json-object \
	json-primitive \
	json-value \
	json-wrapper

JSON_OBJ = \
	$(addsuffix .$(LOBX),$(JSON_SRC)) \
	$(UTF8PROC_DIR)/utf8proc.o

JSON_LIB = \
	-ssecure \
	-sutf8proc

$(ILIBDIR)/libjson.$(LIBX): $(JSON_OBJ) $(ILIBDIR)/libsecure.$(LIBX) $(UTF8PROC_DIR)/utf8proc.o
	$(LP) --slib -o $@ $^ $(JSON_LIB)
