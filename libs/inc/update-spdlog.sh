#!/bin/sh

TAG="${TAG:-v1.12.0}"
SRC=$(pwd)
rm -rf spdlog
SCRATCH=$(mktemp --directory) || exit 1
# echo "${SCRATCH}"

SAVE=$(trap)
# echo "${SAVE}"
trap "cd ${SRC}; rm -rf ${SCRATCH}; trap '${SAVE}' INT EXIT TERM" INT EXIT TERM

cd ${SCRATCH}
git clone --branch "${TAG}" --depth 1 https://github.com/gabime/spdlog 2>/dev/null
mv spdlog/include/spdlog ${SRC}/spdlog
