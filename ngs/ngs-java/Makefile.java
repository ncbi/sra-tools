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

#TOP ?= $(CURDIR)
TOP ?= $(abspath ../..)
MODPATH =
MODULE = ngs/ngs-java
JAVAC ?= javac
include $(TOP)/build/Makefile.env
#include $(TOP)/build/Makefile.config

INCPATHS = $(SRCDIR):$(SRCDIR)/gov/nih/nlm/ncbi/ngs
SRCINC  = -sourcepath $(INCPATHS)

INTLIBS = \

# jar file containing all generated classes
EXTLIBS =       \
	ngs-java    \
	ngs-examples

TARGETS =      \
	$(INTLIBS) \
	$(EXTLIBS)

# if configure was able to locate where the JNI headers go
ifdef JNIPATH
TARGETS += ngs-jni
endif

all std: $(TARGETS)

#-------------------------------------------------------------------------------
# install
#

# unset outside defined variables
ROOT =
LINUX_ROOT =

#fake root for debugging
#uncomment this line and change the test for root ( see under install: ) to succeed:
#ROOT = ~/root

PROFILE_FILE = $(ROOT)/etc/profile.d/ngs-java
JAR_TARGET = $(INST_JARDIR)/ngs-java.jar
DOC_TARGET = $(INST_SHAREDIR)/doc/

ifeq (linux, $(OS))
    ifeq (0, $(shell id -u))
        LINUX_ROOT = true
        DOC_TARGET = $(ROOT)/usr/local/share/doc/ngs/
    endif
endif

install: $(TARGETS) $(INST_JARDIR) $(INST_JARDIR)/ngs-java.jar.$(VERSION) copydocs copyexamples
ifeq (true, $(LINUX_ROOT))
	@ echo "Updating $(PROFILE_FILE).[c]sh"
	@ printf \
"#version $(VERSION)\n"\
"if ! echo \$$CLASSPATH | /bin/grep -q $(JAR_TARGET)\n"\
"then export CLASSPATH=$(JAR_TARGET):\$$CLASSPATH\n"\
"fi" \
        >$(PROFILE_FILE).sh && chmod 644 $(PROFILE_FILE).sh || true;
	@ printf \
"#version $(VERSION)\n"\
"echo \$$CLASSPATH | /bin/grep -q $(JAR_TARGET)\n"\
"if ( \$$status ) setenv CLASSPATH $(JAR_TARGET):\$$CLASSPATH\n"\
        >$(PROFILE_FILE).csh && chmod 644 $(PROFILE_FILE).sh || true;
	@ #TODO: check version of the files above

else
	@ #
	@ echo "Please add $(JAR_TARGET) to your CLASSPATH, i.e.:"
	@ echo "      export CLASSPATH=$(JAR_TARGET):\$$CLASSPATH"
endif

$(INST_JARDIR)/ngs-java.jar.$(VERSION): $(LIBDIR)/ngs-java.jar
	@ echo -n "installing '$(@F)'... "
	@ if cp $^ $@ && chmod 644 $@;                                                    \
	  then                                                                            \
	      rm -f $(patsubst %$(VERSION),%$(MAJVERS),$@) $(patsubst %jar.$(VERSION),%jar,$@);     \
	      ln -s $(@F) $(patsubst %$(VERSION),%$(MAJVERS),$@);                              \
	      ln -s $(patsubst %$(VERSION),%$(MAJVERS),$(@F)) $(patsubst %jar.$(VERSION),%jar,$@) ; \
	      echo;                                                               \
	  else                                                                            \
	      echo failure;                                                               \
	      false;                                                                      \
	  fi

ifeq ($(OS),mac)
    SED = sed -ibak
else
    SED = sed -i
endif

copyexamples:
	@ echo "Installing examples to $(INST_SHAREDIR)/examples-java..."
	@ mkdir -p $(INST_SHAREDIR)/examples-java
	@ cp $(TOP)/ngs/ngs-java/examples/Makefile $(INST_SHAREDIR)/examples-java
	@ $(SED) "s/NGS_CLASS_PATH = ../NGS_CLASS_PATH = $(subst /,\\/,$(INST_JARDIR)/ngs-java.jar)/" $(INST_SHAREDIR)/examples-java/Makefile
	@ cp -r $(TOP)/ngs/ngs-java//examples $(INST_SHAREDIR)/examples-java

copydocs:
	@ echo "Copying html docs to $(DOC_TARGET)..."
	@ mkdir -p $(DOC_TARGET)
	@ cp -r $(LIBDIR)/javadoc/* $(DOC_TARGET)


TO_UNINSTALL = $(INST_JARDIR)/ngs-java.jar* $(DOC_TARGET) $(INST_SHAREDIR)/examples-java
TO_UNINSTALL_AS_ROOT = $(PROFILE_FILE).sh $(PROFILE_FILE).csh

uninstall:
	@ echo "Uninstalling $(TO_UNINSTALL) ..."
	@ rm -rf $(TO_UNINSTALL)
ifeq (true, $(LINUX_ROOT))
	@ echo "Uninstalling $(TO_UNINSTALL_AS_ROOT) ..."
	@ rm -rf $(TO_UNINSTALL_AS_ROOT)
endif
	@ echo "done."

clean:
	rm -rf $(LIBDIR)/ngs-* $(LIBDIR)/javadoc $(CLSDIR)

.PHONY: default all std clean install uninstall copyexamples copydocs $(TARGETS)

#-------------------------------------------------------------------------------
# JAVA NGS
#
ngs-java: makejdirs $(LIBDIR) $(CLSDIR) $(LIBDIR)/ngs-java.jar $(LIBDIR)/ngs-src.jar $(LIBDIR)/ngs-doc.jar

# java API
NGS_SRC =                  \
	ErrorMsg               \
	Statistics             \
	Fragment               \
	FragmentIterator       \
	Read                   \
	ReadIterator           \
	ReadGroup              \
	ReadGroupIterator      \
	Alignment              \
	AlignmentIterator      \
	PileupEvent            \
	PileupEventIterator    \
	Pileup                 \
	PileupIterator         \
	Reference              \
	ReferenceIterator      \
	ReadCollection         \
	Package

NGS_SRC_PATH = \
	$(addprefix $(SRCDIR)/ngs/,$(addsuffix .java,$(NGS_SRC)))

$(CLSDIR)/ngs-java-api: $(NGS_SRC_PATH)
	$(JAVAC) $(DBG) $^ -d $(CLSDIR) $(CLSPATH) $(SRCINC) && touch $@

# java language bindings
ITF_SRC =                  \
	Refcount               \
	StatisticsItf          \
	FragmentItf            \
	FragmentIteratorItf    \
	ReadItf                \
	ReadIteratorItf        \
	ReadGroupItf           \
	ReadGroupIteratorItf   \
	AlignmentItf           \
	AlignmentIteratorItf   \
	PileupEventItf         \
	PileupEventIteratorItf \
	PileupItf              \
	PileupIteratorItf      \
	ReferenceItf           \
	ReferenceIteratorItf   \
	ReadCollectionItf

ITF_SRC_PATH = \
	$(addprefix $(SRCDIR)/ngs/itf/,$(addsuffix .java,$(ITF_SRC)))

$(CLSDIR)/ngs-java-itf: $(CLSDIR)/ngs-java-api $(ITF_SRC_PATH)
	$(JAVAC) $(DBG) $(ITF_SRC_PATH) -d $(CLSDIR) $(CLSPATH) $(SRCINC) && touch $@

# NCBI engine bindings
NCBI_SRC =                 \
	DownloadManager        \
	FileCreator            \
	HttpManager            \
	LibDependencies        \
	LibManager             \
	LibPathIterator        \
	LibVersionChecker      \
	LMProperties           \
	Logger                 \
	Manager                \
	NGS                    \
	Version                \
	error/LibraryLoadError     \
	error/LibraryNotFoundError \
	error/LibraryIncompatibleVersionError   \
	error/cause/ConnectionProblemCause      \
	error/cause/DownloadDisabledCause       \
	error/cause/InvalidLibraryCause         \
	error/cause/JvmErrorCause               \
	error/cause/LibraryLoadCause            \
	error/cause/OutdatedJarCause            \
	error/cause/PrereleaseReqLibCause       \
	error/cause/UnsupportedArchCause        \

NCBI_SRC_PATH = \
	$(addprefix $(SRCDIR)/gov/nih/nlm/ncbi/ngs/,$(addsuffix .java,$(NCBI_SRC)))

$(CLSDIR)/ngs-java-ncbi: $(CLSDIR)/ngs-java-itf $(NCBI_SRC_PATH)
	$(JAVAC) $(DBG) $(NCBI_SRC_PATH) -d $(CLSDIR) $(CLSPATH) $(SRCINC) && touch $@

# rule to produce the jar
JAR=jar cf
$(LIBDIR)/ngs-java.jar: $(CLSDIR)/ngs-java-api $(CLSDIR)/ngs-java-itf $(CLSDIR)/ngs-java-ncbi
	( cd $(CLSDIR); $(JAR) $@ `find . -name "*.class"`; chmod -x,o-w,g+w $@ ) || ( rm -f $@ && false )

$(LIBDIR)/ngs-src.jar: $(ITF_SRC_PATH) $(NCBI_SRC_PATH)
	( cd $(SRCDIR); $(JAR) $@ `find gov ngs -name "*.java"`; chmod -x,o-w,g+w $@ ) || ( rm -f $@ && false )

#-------------------------------------------------------------------------------
# NGS examples
#
ngs-examples: $(LIBDIR) $(CLSDIR) $(LIBDIR)/ngs-examples.jar

# java examples
NGS_EXAMPLES =     \
	AlignTest      \
	AlignSliceTest \
	FragTest       \
	PileupTest     \
	RefTest        \
	ReadGroupTest  \

NGS_EXAMPLES_PATH = \
	$(addprefix $(SRCDIR)/examples/,$(addsuffix .java,$(NGS_EXAMPLES)))

$(CLSDIR)/ngs-examples: $(NGS_EXAMPLES_PATH)
	$(JAVAC) $(DBG) $^ -d $(CLSDIR) $(CLSPATH) $(SRCINC) && touch $@

# rule to produce the jar
$(LIBDIR)/ngs-examples.jar: $(CLSDIR)/ngs-examples
	( cd $(CLSDIR); $(JAR) $@ `find examples -name "*.class"`; chmod -x,o-w,g+w $@  ) || ( rm -f $@ && false )

#-------------------------------------------------------------------------------
# JNI headers
#
ifdef JNIPATH

ngs-jni: $(JNIPATH) $(JNIPATH)/headers-generated

JNI_SRC =                  \
	Package

JNI_ITF =                  \
	ReadCollectionItf      \
	ReadGroupItf           \
	ReadGroupIteratorItf   \
	ReferenceItf           \
	ReferenceIteratorItf   \
	PileupItf              \
	PileupIteratorItf      \
	PileupEventItf         \
	PileupEventIteratorItf \
	AlignmentItf           \
	AlignmentIteratorItf   \
	ReadItf                \
	ReadIteratorItf        \
	FragmentItf            \
	StatisticsItf          \
	Refcount

JNI_CLASSES =                        \
	$(addprefix ngs.,$(JNI_SRC))     \
	$(addprefix ngs.itf.,$(JNI_ITF))

$(JNIPATH)/headers-generated: $(MAKEFILE) $(LIBDIR)/ngs-java.jar
	cd $(JNIPATH); $(JAVAH) -classpath $(CLSDIR) $(JNI_CLASSES)
	@ cd $(JNIPATH); echo 'for f in ngs_itf_*.h; do mv $$f jni_$${f#ngs_itf_}; done' | bash
	@ cd $(JNIPATH); echo 'for f in ngs_*.h; do mv $$f jni_$${f#ngs_}; done' | bash
	@ touch $@

endif

#-------------------------------------------------------------------------------
# javadoc
#
$(LIBDIR)/ngs-doc.jar :
	@ echo "Generating javadocs..."
	@ javadoc -quiet -notimestamp $(CLSPATH) -sourcepath . gov.nih.nlm.ncbi.ngs ngs -d $(LIBDIR)/javadoc
	( cd $(LIBDIR)/javadoc ; $(JAR) $@ `find . -type f`; chmod -x,o-w,g+w $@  ) || ( rm -f $@ && false )

.PHONY: javadoc
