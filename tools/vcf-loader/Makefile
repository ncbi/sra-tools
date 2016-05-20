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
MODULE = tools/vcf-loader

INT_LIBS = \
	libvcfloader
    
ALL_LIBS = \
	$(INT_LIBS) \
	$(EXT_LIBS)

EXT_TOOLS = \

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)    

include $(TOP)/build/Makefile.env

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs 
	@ $(MAKE_CMD) $(TARGDIR)/$@

$(INT_LIBS): makedirs
	@ $(MAKE_CMD) $(ILIBDIR)/$@

$(EXT_TOOLS): makedirs
	@ $(MAKE_CMD) $(BINDIR)/$@    

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

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
INTERM_SRC = \
	$(SRCDIR)/vcf-lex.c \
	$(SRCDIR)/vcf-grammar.c \
	$(SRCDIR)/vcf-grammar.h 

clean: stdclean

.PHONY: clean

.PRECIOUS: $(INTERM_SRC)

#-------------------------------------------------------------------------------
# libvcfloader
#
$(ILIBDIR)/libvcfloader: $(ILIBDIR)/libvcfloader.$(LIBX)

LIBVCF_SRC = \
	vcf-grammar \
	vcf-lex \
    vcf-reader \
    vcf-database
    
# flex/bison should only be invoked manually in an environment ensures the correct versions:
# bison 2.5, flex 2.5.35
bison:
	bison -o $(SRCDIR)/vcf-grammar.c --defines=$(SRCDIR)/vcf-grammar.h -v --no-lines $(SRCDIR)/vcf-grammar.y    

flex:    
	flex -t $(SRCDIR)/vcf-lex.l | grep -v '^#line' > $(SRCDIR)/vcf-lex.c
    
$(SRCDIR)/vcf-grammar.c $(SRCDIR)/vcf-grammar.h: $(SRCDIR)/vcf-grammar.y

LIBVCF_OBJ = \
	$(addsuffix .$(LOBX),$(LIBVCF_SRC))

LIBVCF_LIB = \

$(ILIBDIR)/libvcfloader.$(SHLX): $(INTERM_SRC) $(LIBVCF_OBJ)
	$(LD) --dlib -o $@ $^ $(LIBVCF_LIB)

$(ILIBDIR)/libvcfloader.$(LIBX): $(INTERM_SRC) $(LIBVCF_OBJ)
	$(LD) --slib -o $@ $^ $(LIBVCF_LIB)

