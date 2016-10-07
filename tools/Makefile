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

by_default: default

TOP ?= $(abspath ..)
MODULE = tools

include $(TOP)/build/Makefile.shell

include $(TOP)/build/Makefile.config

#-------------------------------------------------------------------------------
# default
#
SUBDIRS =             \
	util              \
	align-cache       \
	kar               \
	kqsh              \
	vdb-dump          \
	sra-dump          \
	sra-pileup        \
	bam-loader        \
	fastq-loader      \
	srapath           \
	sra-stat          \
	vdb-copy          \
	rd-filter-redact  \
	vdb-config        \
	align-info        \
	qual-recalib-stat \
	prefetch          \
	vdb-decrypt       \
	vdb-validate      \
	sra-sort          \
	cache-mgr         \
	vcf-loader        \
	copycat           \
	sra-load          \
	ccextract         \
	pacbio-load       \
	fuse              \
	vdb-diff          \
	kget              \
	ngs-pileup        \
	general-loader    \
	fastdump          \

ifneq (win,$(BUILD_OS))
ifneq (rwin,$(BUILD_OS))
	SUBDIRS += cg-load pileup-stats
endif
endif

# common targets for non-leaf Makefiles; must follow a definition of SUBDIRS
include $(TOP)/build/Makefile.targets

$(SUBDIRS):
	@ $(MAKE) -C $@

.PHONY: default $(SUBDIRS)

#-------------------------------------------------------------------------------
# pass-through
#
out debug profile release GCC ICC VC++:
	@ $(MAKE) TOP=$(TOP) -f $(TOP)/build/Makefile.env $@

.PHONY: out debug profile release GCC ICC VC++
