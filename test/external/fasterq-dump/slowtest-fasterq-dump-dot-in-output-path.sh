set -e

TOOL="$HOME/ncbi-outdir/sra-tools/linux/gcc/x86_64/dbg/bin/fasterq-dump.3.2.1"

function step() {
    NAME="$1"
    ACC="$2"
    OPT="-p $3"
    DIR="$4"
    echo "$NAME ($ACC):"
    rm -rf $DIR
    $TOOL $ACC $OPT
    echo "$NAME ($ACC) ---> $?"
    tree -L 3 $DIR
    rm -rf $DIR
}

#there is a dot in the relative path of the output-name
function step1() {
    step STEP1 $1 "-o AA/BB.CC/DD" AA
}

#there is a dot in the relative path of the output-name and the name itself
function step2() {
    step STEP2 $1 "-o AA/BB.CC/DD.special" AA
}

#there is a dot in the absolute path of the output-name
function step3() {
    step STEP3 $1 "-o ${HOME}/AA/BB.CC/DD" ${HOME}/AA
}

#there is a dot in the absolute path of the output-name and the name itself
function step4() {
    step STEP4 $1 "-o ${HOME}/AA/BB.CC/DD.special" ${HOME}/AA
}

#there is a dot in the relative output-path
function step5() {
    step STEP5 $1 "-O AA/BB.CC/DD" AA
}

#there is a dot in the absolute output-path
function step6() {
    step STEP6 $1 "-O ${HOME}/AA/BB.CC/DD" ${HOME}/AA
}

#there is a dot in the already existing relative output-path
function step7() {
    mkdir "EE.FF"
    cd EE.FF
    step STEP7 $1 "-O AA/BB.CC/DD" AA
    cd ..
    rm -rf EE.FF
}

#there is a dot in the already existing absolute output-path
function step8() {
    CURR=`pwd`
    mkdir "${HOME}/EE.FF"
    cd ${HOME}/EE.FF
    step STEP8 $1 "-O ${HOME}/EE.FF/AA/BB.CC/DD" ${HOME}/EE.FF/AA
    cd $CURR
    rm -rf "${HOME}/EE.FF"
}

#there are dots in the combination of relative output-path and output-name
function step9() {
    step STEP9 $1 "-O AA/BB.CC -o DD.EE/FF" AA
}

#there are dots in the combination of absolute output-path and output-name
function step10() {
    step STEP10 $1 "-O ${HOME}/AA/BB.CC -o DD.EE/FF" ${HOME}/AA
}

function steps() {
    step1 $1
    step2 $1
    step3 $1
    step4 $1
    step5 $1
    step6 $1
    step7 $1
    step8 $1
    step9 $1
    step10 $1
}

ACC_cSRA="/mnt/data/SRA/ACC/cSRA/SRR341578"
ACC_none_cSRA="/mnt/data/SRA/ACC/TABLES_flat/SRR000001"

steps $ACC_cSRA
steps $ACC_none_cSRA
