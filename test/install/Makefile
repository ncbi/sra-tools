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

default: runtests

TOP ?= $(abspath ../..)

MODULE = test/install

ALL_TARGS = \

include $(TOP)/build/Makefile.env

ROOTDIR = $(SRCDIR)/root

SUBMAKE = $(MAKE) --no-print-directory

ifeq (true,$(LINUX_ROOT))
    #settings for the fake root mode
    INSTALL_DIR = $(ROOTDIR)
    KONFIG_DIR = $(ROOTDIR)/etc/ncbi
    SUBMAKE += LINUX_ROOT=$(LINUX_ROOT) ROOT=$(INSTALL_DIR)
else
    INSTALL_DIR = $(SRCDIR)/install
    KONFIG_DIR = $(INSTALL_DIR)/bin/ncbi
endif

# Override installation location
INST_BINDIR = $(INSTALL_DIR)/bin

SUBMAKE += -C $(TOP) INST_BINDIR=$(INST_BINDIR)

#-------------------------------------------------------------------------------
# runtests
#

INSTALLED_FILES = \
	$(KONFIG_DIR)/certs.kfg                     \
	$(KONFIG_DIR)/default.kfg                   \
	$(KONFIG_DIR)/vdb-copy.kfg                  \


runtests: test-install

test-install: cleanup
	@ echo "installing to $(INSTALL_DIR)"
	@ #
	@ echo "1. clean install"
	@ #
	@ $(SUBMAKE) install
	@ for i in $(INSTALLED_FILES); do test -f "$$i" || ( echo "$$i is not found" >>$(INSTALL_DIR)/missing ); done
	@ ! test -f $(INSTALL_DIR)/missing || ! cat $(INSTALL_DIR)/missing
	@ #
	@ #
	@ echo "2. uninstall"
	@ #
	@ $(SUBMAKE) uninstall
	@ for i in $(INSTALLED_FILES); do ! test -f "$$i" || ( echo "$$i still exists" >>$(INSTALL_DIR)/exist ); done
	@ ! test -f $(INSTALL_DIR)/exist || ! cat $(INSTALL_DIR)/exist
	@ #
	@ #
	@ echo "3. re-install, preserving modified .kfg files"
	@ #
	@ # install
	@ $(SUBMAKE) install
	@ # modify .kfg files
	@ echo newcert  >$(KONFIG_DIR)/certs.kfg
	@ echo newdflt  >$(KONFIG_DIR)/default.kfg
	@ echo newcopy  >$(KONFIG_DIR)/vdb-copy.kfg
	@ echo newncbi  >$(KONFIG_DIR)/ncbi-vdb.kfg     # this should be renamed and never installed again
	@ # reinstall
	@ $(SUBMAKE) install
	@ # verify changed files have been saved
	@ ( grep newcert  $(KONFIG_DIR)/certs.kfg.orig       >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/certs.kfg.orig has unexpected content" && exit 1 )
	@ ( grep newdflt  $(KONFIG_DIR)/default.kfg.orig     >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/default.kfg.orig has unexpected content" && exit 1 )
	@ ( grep newcopy  $(KONFIG_DIR)/vdb-copy.kfg.orig     >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/vdb-copy.kfg.orig has unexpected content" && exit 1 )
	@ ( grep newncbi  $(KONFIG_DIR)/ncbi-vdb.kfg.orig    >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/ncbi-vdb.kfg.orig has unexpected content" && exit 1 )
	@ ( ! test -f $(KONFIG_DIR)/ncbi-vdb.kfg  ) || ( echo "Error: $(KONFIG_DIR)/ncbi-vdb.kfg exists" && exit 1 )  # gone ...
	@ # verify that further reinstall does not overwrite previously saved files if there is no change
	@ $(SUBMAKE) install
	@ # verify saved files are still there
	@ ( grep newcert  $(KONFIG_DIR)/certs.kfg.orig       >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/certs.kfg.orig has unexpected content after reinstallation" && exit 1 )
	@ ( grep newdflt  $(KONFIG_DIR)/default.kfg.orig     >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/default.kfg.orig has unexpected content after reinstallation" && exit 1 )
	@ ( grep newcopy  $(KONFIG_DIR)/vdb-copy.kfg.orig     >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/vdb-copy.kfg.orig has unexpected content after reinstallation" && exit 1 )
	@ ( grep newncbi  $(KONFIG_DIR)/ncbi-vdb.kfg.orig    >/dev/null ) || ( echo "Error: $(KONFIG_DIR)/ncbi-vdb.kfg.orig has unexpected content after reinstallation" && exit 1 )
	@ ! test -f $(KONFIG_DIR)/ncbi-vdb.kfg          # ... and stays gone
	@ #
	@ #
	@ echo "4. install, discarding older versions of .kfg files"
	@ rm -f $(KONFIG_DIR)/*.orig
	@ $(SUBMAKE) install
	@ # replace .kfg files with older versions
	@ cp $(SRCDIR)/default.kfg.2.8.1 	$(KONFIG_DIR)/default.kfg
	@ cp $(SRCDIR)/certs.kfg.2.8.1 		$(KONFIG_DIR)/certs.kfg
	@ cp $(SRCDIR)/vdb-copy.kfg.2.8.1 	$(KONFIG_DIR)/vdb-copy.kfg
	@ # reinstall
	@ $(SUBMAKE) install
	@ # verify newer files are installed
	@ diff $(BINDIR)/ncbi/default.kfg 			$(KONFIG_DIR)/default.kfg
	@ diff $(BINDIR)/ncbi/certs.kfg 			$(KONFIG_DIR)/certs.kfg
	@ diff $(TOP)/tools/vdb-copy/vdb-copy.kfg 	$(KONFIG_DIR)/vdb-copy.kfg
	@ # verify no old files were saved
	@ ( ! test -f $(KONFIG_DIR)/default.kfg.orig ) 	|| ( echo "Error: $(KONFIG_DIR)/default.kfg.orig exists" && exit 1 )
	@ ( ! test -f $(KONFIG_DIR)/certs.kfg.orig ) 	|| ( echo "Error: $(KONFIG_DIR)/certs.kfg.orig exists" && exit 1 )
	@ ( ! test -f $(KONFIG_DIR)/vdb-copy.kfg.orig ) || ( echo "Error: $(KONFIG_DIR)/vdb-copy.kfg.orig exists" && exit 1 )
	@ #
	@ #
ifeq (,$(LINUX_ROOT))
	#
	# Run the same tests in simulated root mode
	#
	@ $(MAKE) LINUX_ROOT=true
endif

cleanup:
	rm -rf $(INSTALL_DIR)
ifeq (true,$(LINUX_ROOT))
	# in root mode, recreate "system" directories
	mkdir -p $(ROOTDIR)/usr/include  $(ROOTDIR)/etc/profile.d
endif

.PHONY: install