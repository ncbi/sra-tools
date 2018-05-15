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
MODULE = tools/sra-toolkit

INT_TOOLS = \

EXT_TOOLS = \
	sra-toolkit

ALL_TOOLS = \
	$(INT_TOOLS) \
	$(EXT_TOOLS)

include $(TOP)/build/Makefile.env

all std: $(ALL_TOOLS)

.PHONY: all std $(ALL_TOOLS) deploy-sra-toolkit

ifeq (,$(QMAKE_BIN))
$(ALL_TOOLS) deploy-sra-toolkit:
	@ echo "NOTE - $(@F) cannot be built:"               \
	       "It requires Qt to be installed"
else

#-------------------------------------------------------------------------------
# outer targets
#
all std: makedirs
	@ $(MAKE_CMD) $(TARGDIR)/$@-cmn

#-------------------------------------------------------------------------------
# clean
#
clean: stdclean
	rm -rf $(OBJDIR); rm -f $(BINDIR)/sra-toolkit*

.PHONY: clean

#-------------------------------------------------------------------------------
# sra-toolkit
#

ifeq (dbg,$(BUILD))
   QT_CONF = debug
   QT_NOCONF = release
else
   QT_CONF = release
   QT_NOCONF = debug
endif

$(BINDIR)/sra-toolkit:
	# configure and run Qt make
	$(QMAKE_BIN) $(SRCDIR)/sra-toolkit.pro CONFIG-=$(QT_NOCONF) CONFIG+=$(QT_CONF) CONFIG+=make
	make -C $(OBJDIR)
	cp $(OBJDIR)/sra-toolkit $(BINDIR)/sra-toolkit$(VERSION_EXEX)
	ln -s sra-toolkit$(VERSION_EXEX) $(BINDIR)/sra-toolkit$(MAJVERS_EXEX)
	ln -s sra-toolkit$(MAJVERS_EXEX) $(BINDIR)/sra-toolkit

# package sra-toolkit as a Linux bundle
ifeq (linux,$(OS))

QT_DIR ?= $(dir $(shell which qmake))..

ifneq ("$(wildcard $(QT_DIR))","")

$(OBJDIR)/linuxdeployqt-continuous-x86_64.AppImage:
	# download the packaging tool
	cd $(OBJDIR); wget https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage
	chmod a+x $@

BUNDLE_DIR =  $(BINDIR)/sra-toolkit-bundle

deploy-sra-toolkit: $(BINDIR)/sra-toolkit $(OBJDIR)/linuxdeployqt-continuous-x86_64.AppImage
	# create the bundle dir
	rm  -rf $(BUNDLE_DIR)
	mkdir -p $(BUNDLE_DIR)/lib
	# copy the executable to the bundle dir
	cp  $(OBJDIR)/sra-toolkit $(BUNDLE_DIR)/sra-toolkit$(VERSION_EXEX)
	# run the packaging tool
	export LD_LIBRARY_PATH=$(VDB_LIBDIR); $(OBJDIR)/linuxdeployqt-continuous-x86_64.AppImage $(BUNDLE_DIR)/sra-toolkit$(VERSION_EXEX) -bundle-non-qt-libs -verbose=1
	# post-process the package: remove junk, add missed libraries
	rm $(BUNDLE_DIR)/AppRun
	cp $(QT_DIR)/lib/libQt5Svg.so.5 $(BUNDLE_DIR)/lib
	cp $(QT_DIR)/plugins/iconengines/ $(BUNDLE_DIR)/plugins -r
	# replace BINDIR/executable with a link to the bundle
	ln -fs sra-toolkit-bundle/sra-toolkit$(VERSION_EXEX) $(BINDIR)/sra-toolkit$(VERSION_EXEX)

else
deploy-sra-toolkit:
	@ echo "NOTE - sra-toolkit app cannot be deployed:"               \
	       "Please provide path to Qt SDK on the make command line, e.g. make deploy-sra-toolkit QT_DIR=/opt/Qt/5.10.1/gcc_64"
endif

endif

endif
