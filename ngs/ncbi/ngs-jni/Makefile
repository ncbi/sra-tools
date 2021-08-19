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
MODULE = ngs/ncbi/ngs-jni

INT_LIBS = \
	libngs-jni

EXT_LIBS = \

ALL_LIBS = \
	$(INT_LIBS) \
	$(EXT_LIBS)

include $(TOP)/build/Makefile.env
INCDIRS += -I$(VDB_SRCDIR)/libs/klib

INCDIRS += \
	-I$(SRCDIR)/../ngs

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/std

$(INT_LIBS): makedirs jnihdrs
	@ $(MAKE_CMD) $(ILIBDIR)/$@

$(EXT_LIBS): makedirs jnihdrs
	@ $(MAKE_CMD) $(LIBDIR)/$@

.PHONY: all std $(ALL_LIBS)

#-------------------------------------------------------------------------------
# std
#
$(TARGDIR)/std: \
	$(addprefix $(ILIBDIR)/,$(INT_LIBS)) \
	$(addprefix $(LIBDIR)/,$(EXT_LIBS))

.PHONY: $(TARGDIR)/std

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean

.PHONY: clean

#-------------------------------------------------------------------------------
# JNI headers
#
jnihdrs: # headers-generated

$(CLSPATH)/ngs-java.jar:
	@ echo "don't know where $@ is"

JNI_BASE =  \
	Manager

JNI_INC = \
	$(addprefix jni_,$(addsuffix .h,$(JNI_BASE)))

headers-generated: $(CLSPATH)/ngs-java.jar
	javah -classpath $^ $(addprefix gov.nih.nlm.ncbi.ngs.,$(JNI_BASE))
	@ echo 'for f in gov_nih_nlm_ncbi_ngs_*.h; do mv $$f jni_$${f#gov_nih_nlm_ncbi_ngs_}; done' | bash
	@ touch $@

#-------------------------------------------------------------------------------
# ncbi-ngs-jni
#
$(ILIBDIR)/libngs-jni: $(addprefix $(ILIBDIR)/libngs-jni.,$(LIBX))

JNI_FIXED =  \
	String   \
	ErrorMsg

JNI_SRC = \
	$(addprefix jni_,$(JNI_BASE) $(JNI_FIXED))

JNI_OBJ = \
	$(addsuffix .$(LOBX),$(JNI_SRC))

$(ILIBDIR)/libngs-jni.$(LIBX): $(JNI_OBJ)
	$(LD) --slib -o $@ $^

#-------------------------------------------------------------------------------
# fuzz
#
fuzz: std
	cp $(addsuffix .$(LIBX),$(addprefix $(ILIBDIR)/,$(INT_LIBS))) $(FLIBDIR)/
