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

# default response to "make"
default: std

std: subdirs

# the sub-directories
SUBDIRS =    \
	ngs \
	ngs-c++ \
	ngs-jni \
	ngs-py \

SUBDIRS_CLN = \
	$(addsuffix _cln,$(SUBDIRS))

SUBDIRS_INST = \
	$(addsuffix _inst,$(SUBDIRS))

SUBDIRS_TST = \
	$(addsuffix _test,$(SUBDIRS))

subdirs: $(SUBDIRS)

clean: $(SUBDIRS_CLN)

test: $(SUBDIRS_TST)

install: $(SUBDIRS_INST)

$(SUBDIRS):
	@ $(MAKE) -C $@

$(SUBDIRS_CLN):
	@ $(MAKE) -C $(subst _cln,,$@) clean

$(SUBDIRS_INST):
	@ $(MAKE) -s --no-print-directory -C $(subst _inst,,$@) install

$(SUBDIRS_TST):
	@ $(MAKE) -C $(subst _test,,$@) test

.PHONY: $(SUBDIRS) $(SUBDIRS_CLN) $(SUBDIRS_INST)
