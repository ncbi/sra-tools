#!/bin/sh

TAG="${TAG:-v8.0.1}"
SRC=$(pwd)
SCRATCH=$(mktemp --directory) || exit 1
# echo "${SCRATCH}"

SAVE=$(trap)
# echo "${SAVE}"
trap "cd ${SRC}; rm -rf ${SCRATCH}; trap '${SAVE}' INT EXIT TERM" INT EXIT TERM

cd ${SCRATCH}
git clone --branch "${TAG}" --depth 1 https://github.com/tlk00/BitMagic.git 2>/dev/null || exit 1
mv ${SRC}/bm ${SRC}/bm.moved-aside || exit 1
mv BitMagic/src ${SRC}/bm || exit 1
rm -rf ${SRC}/bm.moved-aside
