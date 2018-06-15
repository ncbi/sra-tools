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

by_default: runtests

TOP ?= $(abspath ..)
MODULE = test

include $(TOP)/build/Makefile.shell

include $(TOP)/build/Makefile.config

#-------------------------------------------------------------------------------
# default
#
SUBDIRS =    \
	prefetch        \
	dump-test       \
	vdb-config      \
	vcf-loader      \
	align-info      \
	align-cache     \
	vdb-validate    \
	kar             \
	copycat         \
	vdb-copy        \
	qual-recalib-stat \
	sra-stat        \
	srapath         \
	sra-sort        \
	sra-pileup      \
	fuse            \
	fastq-loader    \
	kget            \
	vdb-dump        \

# under construction
#    ngs-pileup      \

# common targets for non-leaf Makefiles; must follow a definition of SUBDIRS
include $(TOP)/build/Makefile.targets

$(SUBDIRS):
	@ $(MAKE) -C $@

.PHONY: default $(SUBDIRS)

#   time of slowtests in linux release mode
# real    4m 0.076s      user    4m22.256s      sys     0m10.707s     sra-sort
# real    5m21.151s      user    5m59.396s      sys     0m39.296s     fastdump
# real    6m55.921s      user    6m28.035s      sys     6m58.052s     sra-pileup
# real  184m21.347s      user   42m19.318s      sys   259m56.028s     fuse
