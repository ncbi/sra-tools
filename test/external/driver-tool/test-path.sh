#!/bin/sh

bin_dir=$1
sratools=$2

TEST_DESC="path resolution via ${sratools}"
echo "testing ${TEST_DESC}"

TEMPDIR=.

mkdir -p actual

# verify that driver tool locates target executable via PATH
output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
	PATH="${bin_dir}:$PATH" \
	SRATOOLS_TESTING=6 \
	SRATOOLS_IMPERSONATE=prefetch \
	${sratools} 2>actual/path1.stderr ; \
	grep "executable-path" actual/path1.stderr | \
	grep -q "${bin_dir}")

res=$?
if [ "$res" != "0" ];
	then echo "Driver tool test ${TEST_DESC}, res=$res output=$output" && exit 1;
fi
rm -rf actual/path1.stderr

if [ -f "${TEMPDIR}/prefetch" ];
	then echo "Driver tool test ${TEST_DESC}, skipping part 2"
else
    touch ${TEMPDIR}/prefetch

    # verify that driver tool locates target executable via PATH and
    # when a non-executable with the same name is found first
    output=$(NCBI_SETTINGS=${TEMPDIR}/tmp.mkfg \
        PATH="${TEMPDIR}:${bin_dir}:$PATH" \
        SRATOOLS_TESTING=6 \
        SRATOOLS_IMPERSONATE=prefetch \
        ${sratools} 2>actual/path2.stderr ; \
        grep "executable-path" actual/path2.stderr | \
        grep -q "${bin_dir}")
    res=$?

    rm -rf ${TEMPDIR}/prefetch

    if [ "$res" != "0" ];
        then echo "Driver tool test ${TEST_DESC} part 2, res=$res output=$output" && exit 1;
    fi
    rm -rf actual/path2.stderr
fi

echo "Driver tool test ${TEST_DESC} is finished"
