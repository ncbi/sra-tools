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


#-------------------------------------------------------------------------------
# environment
#
TOP ?= $(CURDIR)
include $(TOP)/build/Makefile.shell


#-------------------------------------------------------------------------------
# default
#
SUBDIRS = \
	tools \

# common targets for non-leaf Makefiles; must follow a definition of SUBDIRS
include $(TOP)/build/Makefile.targets

default: $(SUBDIRS)

test: $(SUBDIRS)

$(SUBDIRS) test:
	@ $(MAKE) -C $@

.PHONY: default $(SUBDIRS) test

#-------------------------------------------------------------------------------
# all
#
$(SUBDIRS_ALL):

#-------------------------------------------------------------------------------
# std
#
$(SUBDIRS_STD):

#-------------------------------------------------------------------------------
# install
#
install: std
	$(MAKE) -s TOP=$(CURDIR) -f build/Makefile.install install

.PHONY: install

#-------------------------------------------------------------------------------
# clean
#
clean: clean_test

clean_test:
	@ $(MAKE) -s -C test clean

#-------------------------------------------------------------------------------
# runtests
#
runtests: runtests_test

runtests_test:
	@ $(MAKE) -s -C test runtests

#	@ $(MAKE) -s -C ngs runtests

#-------------------------------------------------------------------------------
# slowtests
#
slowtests: slowtests_test

slowtests_test:
	@ $(MAKE) -s -C test slowtests

#-------------------------------------------------------------------------------
# pass-through targets
#
COMPILERS = GCC ICC VC++ CLANG
ARCHITECTURES = i386 x86_64 sparc32 sparc64
CONFIG = debug profile release
PUBLISH = scm pubtools
REPORTS = bindir targdir osdir config compilers architecture architectures
PASSTHRUS = \
	out \
	CC $(COMPILERS) \
	$(ARCHITECTURES) \
	$(CONFIG) $(PUBLISH) \
	purify purecov \
	local static dynamic

$(RHOSTS):
	@ $(MAKE) -s TOP=$(CURDIR) -f build/Makefile.env local
	@ $(MAKE) -s TOP=$(CURDIR) -f build/Makefile.env require-proxy-exec
	@ $(MAKE) -s TOP=$(CURDIR) -f build/Makefile.env $@
	@ $(MAKE) -s TOP=$(CURDIR) -f build/Makefile.env rebuild-dirlinks config

$(PASSTHRUS):
	@ $(MAKE) -s TOP=$(CURDIR) -f build/Makefile.env $@
	@ $(MAKE) -s TOP=$(CURDIR) -f build/Makefile.env rebuild-dirlinks config

$(REPORTS):
	@ $(MAKE) -s TOP=$(CURDIR) -f build/Makefile.env $@

.PHONY: $(PASSTHRUS) $(RHOSTS) $(REPORTS)


#-------------------------------------------------------------------------------
# configuration help
#
help configure:
	@ echo "Before initial build, run './configure --build-prefix=<out>' from"
	@ echo "the project root to set the output directory of your builds."
	@ echo
	@ echo "To select a compiler, run 'make <comp>' where"
	@ echo "comp = { "$(COMPILERS)" }."
	@ echo
	@ echo "For hosts that support cross-compilation ( only Macintosh today ),"
	@ echo "you can run 'make <arch>' where arch = { "$(ARCHITECTURES)" }."
	@ echo
	@ echo "To set a build configuration, run 'make <config>' where"
	@ echo "config = { "$(CONFIG)" }."
	@ echo
	@ echo "To select a remote build configuration, run 'make <rhost>' where"
	@ echo "rhost = { "$(RHOSTS)" }."
	@ echo

.PHONY: help configure
