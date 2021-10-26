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

TOP ?= $(abspath ../../../../../../..)
MODULE = ngs/ngs-java/gov/nih/nlm/ncbi/ngs

JARS = \
	ngs-ncbi.jar

ALL_TARGS = \
	$(JARS)

# default settings for java do not work here, override    
CLSPATH = $(TARGDIR)/ngs/ngs-java
JAVASOURCEPATH = $(TOP)/ngs/ngs-java
include $(TOP)/build/Makefile.env
#$(warning JMAKE_CMD=$(JMAKE_CMD))

#-------------------------------------------------------------------------------
# outer targets
#
all std: $(ALL_TARGS)

$(JARS): makejdirs
	@ $(JMAKE_CMD) $(CLSPATH)/$@

.PHONY: all std $(ALL_TARGS)

#-------------------------------------------------------------------------------
# clean
#
clean: stdjclean

.PHONY: clean

#-------------------------------------------------------------------------------
# classes for ngs-ncbi
#

LIB_SRC =                          \
	HttpException              \
	HttpManager                \
	LibManager                 \
	LibPathIterator	           \
	Logger      	           \
	Manager      	           \
	ReadCollectionItf          \
	Refcount                   \
	ReadGroupIteratorItf       \
	ReadGroupItf               \
	ReferenceIteratorItf       \
	ReferenceItf               \
	AlignmentItf               \
	AlignmentIteratorItf       \
	ReadItf                    \
	ReadIteratorItf            \
	PileupIteratorItf          \
	PileupItf                  \
	FragmentItf                \
	PileupEventIteratorItf     \
	PileupEventItf             \
	TestyTheBear               \

NGS_SRC =                      \
    NGS                        \
	$(LIB_SRC)

#VARS = -Dvdb.System.loadLibrary=1
#VARS = -Dvdb.log=FINEST
#J=/net/pan1/sra-test/local/jdk1.7.0_51/bin/
#JAVABITS=-d$(BITS)

$(SRCDIR)/NGS.java: \
	$(addprefix $(SRCDIR)/,$(addsuffix .java,$(LIB_SRC)))

NGS_CLS = \
	$(addsuffix .class,$(NGS_SRC))

$(CLSPATH)/ngs-ncbi.jar: $(NGS_CLS)
	( cd $(CLSPATH); jar -cf $@ `find ngs gov -name *.class` )

#-------------------------------------------------------------------------------
# manual integration test
#
test: $(CLSPATH)/ngs-ncbi.jar 
	cd $(CLSPATH); $(J)java $(JAVABITS) -classpath ngs-ncbi.jar $(VARS) \
		-Djava.library.path=$(LIBDIR) gov.nih.nlm.ncbi.ngs.TestyTheBear
