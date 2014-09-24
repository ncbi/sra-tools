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

default: install

# determine a few things
TOP ?= $(abspath ..)
include $(TOP)/build/Makefile.env

# current distribution point
ifeq (linux,$(OS))
TRACE_SOFTWARE = /panfs/traces01/trace_software/vdb
endif
ifeq (mac,$(OS))
TRACE_SOFTWARE = /net/traces01/trace_software/vdb
endif
ifeq (win,$(OS))
TRACE_SOFTWARE = //panfs/traces01/trace_software/vdb
endif


#-------------------------------------------------------------------------------
# targets
#
INSTALL_TARGETS = \
	interfaces \
	schema \
	os \
	arch

install: $(INSTALL_TARGETS)

.PHONY: install $(INSTALL_TARGETS)


#-------------------------------------------------------------------------------
# interfaces
#  populates the interfaces directory
#
interfaces:
	@ bash cp.sh $(TOP)/interfaces $(TRACE_SOFTWARE)/interfaces "-name *.h -o -name *.hpp"


#-------------------------------------------------------------------------------
# schema
#  populates the schema directory
#
schema:
	@ bash cp.sh $(TOP)/interfaces $(TRACE_SOFTWARE)/schema "-name *.vschema"


#-------------------------------------------------------------------------------
# operating system
#  populates any os-specific things, such as configuration
#
os:
	@ true


#-------------------------------------------------------------------------------
# architecture
#  populates build results
#
arch: arch-$(BUILD)

arch-dbg:
	@ bash cp.sh $(BINDIR) $(TRACE_SOFTWARE)/$(OS)/debug/$(ARCH)/bin "-type d -a -name ncbi -prune -o ! -type d -print"
	@ bash cp.sh $(LIBDIR) $(TRACE_SOFTWARE)/$(OS)/debug/$(ARCH)/lib "-type d -a -name ncbi -prune -o ! -type d -print"
	@ bash cp.sh $(ILIBDIR) $(TRACE_SOFTWARE)/$(OS)/debug/$(ARCH)/ilib "! -type d -print"

arch-rel:
	@ bash cp.sh $(BINDIR) $(TRACE_SOFTWARE)/$(OS)/release/$(ARCH)/bin "-type d -a -name ncbi -prune -o ! -type d -print"
	@ bash cp.sh $(LIBDIR) $(TRACE_SOFTWARE)/$(OS)/release/$(ARCH)/lib "-type d -a -name ncbi -prune -o ! -type d -print"
