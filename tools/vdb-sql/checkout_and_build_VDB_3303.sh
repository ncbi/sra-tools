#!/bin/bash

#=========================================================================
RELEASE_BUILD=true
BUILDROOT="$HOME/VDB_3303"
BRANCH_NGS="engineering"
BRANCH_NCBI_VDB="VDB-3303"
BRANCH_SRA_TOOLS="VDB-3303"
#=========================================================================

REPO_NGS="https://github.com/ncbi/ngs.git"
DIR_NGS="ngs"

REPO_NCBI_VDB="https://github.com/ncbi/ncbi-vdb.git"
DIR_NCBI_VDB="ncbi-vdb"

REPO_SRA_TOOLS="https://github.com/ncbi/sra-tools.git"
DIR_SRA_TOOLS="sra-tools"

BUILD_DIR="$BUILDROOT/OUTDIR"
BUILD_DIR_FLAG="--build=$BUILDROOT/OUTDIR"
INSTALL_DIR="$BUILDROOT/INST"

#exit the script if an error occured
set -e

if [ "$RELEASE_BUILD" = true ] ; then
    RELEASE_DEBUG_FLAG="--without-debug"
else
    RELEASE_DEBUG_FLAG="--with-debug"
fi

function checkout_branch
{
    THE_REPO=$1
    THE_DIR=$2
    THE_BRANCH=$3
    
    echo "====================================================================="
    echo "cloning: $THE_REPO"
    
    cd $BUILDROOT
    if [ ! -d $THE_DIR ]; then
        git clone $THE_REPO
    else
        echo "$THE_BRANCH is already cloned"
    fi
    
    echo "switching to branch: $THE_BRANCH"
    cd "$THE_DIR"
    git checkout $THE_BRANCH
    git pull
    echo "."
}

function build_ngs
{
    cd $BUILDROOT/$DIR_NGS
    PREFIX_FLAG="--prefix=$INSTALL_DIR/ngs/ngs-sdk"
    ./configure $PREFIX_FLAG $BUILD_DIR_FLAG $RELEASE_DEBUG_FLAG
    cd ngs-sdk
    make default install
}

function build_ncbi_vdb
{
    cd $BUILDROOT/$DIR_NCBI_VDB
    PREFIX_FLAG="--prefix=$INSTALL_DIR/ncbi-vdb"
    ./configure $PREFIX_FLAG $BUILD_DIR_FLAG $RELEASE_DEBUG_FLAG
    make default install
}

function build_sra_tools
{
    cd $BUILDROOT/$DIR_SRA_TOOLS
    PREFIX_FLAG="--prefix=$INSTALL_DIR/sra-tools"
    ./configure $PREFIX_FLAG $BUILD_DIR_FLAG $RELEASE_DEBUG_FLAG
    make default install
}

# STEP 1 : make build-root
##################################################################
mkdir -p $BUILDROOT

# STEP 2 : check out 5 repositories
##################################################################
checkout_branch $REPO_NGS $DIR_NGS $BRANCH_NGS
checkout_branch $REPO_NCBI_VDB $DIR_NCBI_VDB $BRANCH_NCBI_VDB
checkout_branch $REPO_SRA_TOOLS $DIR_SRA_TOOLS $BRANCH_SRA_TOOLS

# STEP 3 : build the ngs-library
##################################################################
build_ngs

# STEP 4 : build the ncbi-vdb-library
##################################################################
build_ncbi_vdb

# STEP 5 : build the sra-tools
##################################################################
build_sra_tools
