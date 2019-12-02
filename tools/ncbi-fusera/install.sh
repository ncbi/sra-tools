#!/bin/bash

###
##  Location and environment
#
SCRIPT_DIR=`dirname $0`
SCRIPT_DIR=`cd $SCRIPT_DIR; pwd`

SCRIPT_NAME=`basename $0`

CONFIG_TAG="--config"
CONFIG_DFLT="install_standard.cfg"

###
##  Parsing ARGS
#
ARGS=($*)
ARGC=${#ARGS[*]}

msg ()
{
    echo $*
}

warn ()
{
    echo WARNING: $*
}

err ()
{
    echo ERROR: $* >&2
}

syntax ()
{
    if [ $# -ne 0 ]
    then
        echo 
        echo "ERROR: $@"
    fi

    cat <<EOF >&2

That program will download latest fusera and twig packages and prepare
fusera tree.

Syntax: $SCRIPT_NAME [ $CONFIG_TAG CONFIG ] INSTALL_DIR

Where:
        CONFIG - file with installation settings. Optional. Default config :

                 $SCRIPT_DIR/$CONFIG_DFLT

        INSTALL_DIR - directory where make installations

EOF

    return 1
}

read_target_value ()
{
    TARGET_DIR=${ARGS[$(($ARGC - 1 ))]}
    CONFIG_FILE=$SCRIPT_DIR/$CONFIG_DFLT
}

read_config_value ()
{
    TAG_FND=${ARGS[0]}
    if [ "$TAG_FND" == "$CONFIG_TAG" ]
    then
        CONFIG_FILE=${ARGS[1]}
    else
        syntax Invalid argument \"$TAG_FND\"
    fi

    TARGET_DIR=${ARGS[$(($ARGC - 1 ))]}
}

case $ARGC in
    1)
        read_target_value
        ;;
    3)
        read_config_value
        ;;
    *)
        syntax
        ;;
    *)
        syntax Invalid parameters
        ;;
esac

if [ -z "$CONFIG_FILE" ]
then
    err something went wrong ... undefined configuration
    exit 1
fi

if [ -z "$TARGET_DIR" ]
then
    err something went wrong ... undefined target directory
    exit 1
fi

if [ ! -e $CONFIG_FILE ]
then
    err can not stat file \"$CONFIG_FILE\"
    exit 1
fi

if [ -e $TARGET_DIR ]
then
    err target directory already exists \"$TARGET_DIR\"
    exit 1
fi

###
##  Now reading config and start processing
#

##
## Simple syntax : read_config_value NAME VAR_NAME
##                 simple_read_config_value NAME VAR_NAME
##
simple_read_config_value ()
{
    VAL=$1
    VAR=$2

    eval $VAR=`grep -w "^$VAL" $CONFIG_FILE | sed "s#=##1" | awk ' { printf $2 } '`
}

read_config_value ()
{
    simple_read_config_value $@

echo $1 $2 ${!2}

    if [ -z "${!2}" ]
    then
        err can not find config value for \"$1\"
        exit 1
    fi
}

read_config_value FUSERA_GIT FUSERA_GIT
simple_read_config_value FUSERA_BRANCH FUSERA_BRANCH

read_config_value TWIG_GIT TWIG_GIT
simple_read_config_value TWIG_BRANCH TWIG_BRANCH

###
##  Building directory tree
#

##
##  Standart GO directory has layout :
##
##  dir --- bin
##       |- src --- github.com --- mattrbianchi
##                              |- ncbi
##
##  As a big novation we will add base here, which all belong to us
##
##  dir --- bin
##       |- src --- github.com --- mattrbianchi ( soft lint to base )
##       |                      |- ncbi ( soft lint to base )
##       |- base --- fuzera
##                |- twig
##
simple_exec ()
{
    CMD="$@"
    echo "## $CMD"
    eval "$CMD"
    if [ $? -ne 0 ]
    then
        err command failed \"$CMD\"
        exit 1
    fi
}

make_dir ()
{
    msg creating directory \"$1\"
    simple_exec mkdir $1
}

make_dir $TARGET_DIR

## doing that here because it gives us absolute path
##
TARGET_DIR=`cd $TARGET_DIR; pwd`

BIN_DIR=$TARGET_DIR/bin
make_dir $BIN_DIR

PKG_DIR=$TARGET_DIR/pkg
make_dir $PKG_DIR

SRC_DIR=$TARGET_DIR/src
make_dir $SRC_DIR

###
##  Downloading sources
#

###
##  Setting up SRC tree and links
#

## Syntax setup_tree_and_clone URL BRANCH
##
setup_tree_and_clone ()
{
    URL=$1
    BRN=$2

    DIR=`basename $URL .git`

    TREE=`dirname $URL | sed "s#https://##1"`

    echo $DIR
    echo $TREE

    NDS=( `echo $TREE | sed "s#/# #g" ` )
    NDC=${#NDS[*]}

    cd $SRC_DIR

    FIN_DIR=$SRC_DIR

    CNT=0
    while [ $CNT -lt $NDC ]
    do
        FIN_DIR=$FIN_DIR/${NDS[$CNT]}

        if [ ! -d $FIN_DIR ]
        then
            make_dir $FIN_DIR
        fi

        CNT=$(( $CNT + 1 ))
    done

    cd $FIN_DIR

        ## Here we are cloning our git directory
        ##
    simple_exec git clone $URL

    cd $DIR
    simple_exec git pull

    if [ -n "$BRN" ]
    then
        simple_exec git checkout $BRN
    fi
}

setup_tree_and_clone $FUSERA_GIT $FUSERA_BRANCH
setup_tree_and_clone $TWIG_GIT $TWIG_BRANCH

###
##  Doing weird stuff
#
cat <<EOF >$TARGET_DIR/README.txt
Hola, stranger!

That directory contains everything You need to work with FUSERA.
You may edit, compile and do other weird staff

Layout :

    this_dir                    /* this directory lol */
       |
       +-> bin                  /* useful scripts and binaries */
       |
       +-> pkg                  /* GO package objects */
       |
       +-> src                  /* packages */
       |    |
       |    +-> github.com      /* packages from github */
       |              |
       |              +-> mattrbianchi
       |              |         |
       |              |         +-> twig
       |              |
       |              +-> ncbi
       |                    |
       |                    +-> fusera
       |
       +-> README.txt           /* this file */
       |
       +-> ENV.txt              /* file with useful settings ". ENV.txt" */


To run fusera, You need perform that set of commands :

# cd $TARGET_DIR/src/github.com/ncbi/fusera
# export GOPATH=$GOPATH:$TARGET_DIR 
# go run main.go

Enjoy :D
t.

EOF


cat <<EOF >$TARGET_DIR/ENV.txt
#!/bin/bash

## NOTE: it is important to export that varable ... lol
##

export GOPATH=$GOPATH:$TARGET_DIR
EOF
