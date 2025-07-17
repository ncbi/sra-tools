#!/bin/sh

PYTHON="$(which python3)" || { echo "Skipped; python3 not found"; exit 2; }
PERL="$(which perl)" || { echo "Skipped; perl not found"; exit 2; }

if [ "$(uname -s)" = "Linux" ]; then
    [ "$(uname -o)" = "GNU/Linux" ] || { echo "Skipped; not GNU/Linux"; exit 2; }
fi

DIRTOTEST=${1}
DIRTOTEST=$(cd ${DIRTOTEST}; pwd) || exit 1

SOURCEDIR=${2:-${DIRTOTEST}}
SOURCEDIR=$(cd ${SOURCEDIR}; pwd) || exit 1

STATSTOOL="${DIRTOTEST}/qa-stats"
DIFFTOOL="${SOURCEDIR}/diff-tool.py"
INPUT_MAKER="${PWD}/make-test-input.pl"
REVERSER="${PWD}/reverse.pl"
SHUFFLER="$(which sort)"

make_input() {
    "${PERL}" "${INPUT_MAKER}" "${@}"
}

reverse() {
    "${PERL}" "${REVERSER}" "${@}"
}

shuffle() {
    "${SHUFFLER}" --random-sort "${@}"
}

do_comparison() {
    "${PYTHON}" "${DIFFTOOL}" "${@}"
}

do_loose_comparison() {
    "${PYTHON}" "${DIFFTOOL}" --ignoreMissing "${@}"
}

runtests() {
    "${STATSTOOL}" "${1}" > 'stats.json' || {
        ec=${?}
        echo "${STATSTOOL} failed!"
        exit ${ec}
    }
    do_loose_comparison 'summary.json' 'stats.json' >>/dev/null || {
        do_loose_comparison 'summary.json' 'stats.json' 
        echo "${STATSTOOL} failed; result didn't compare to expected."
        exit 1
    }
    reverse "${1}" | "${STATSTOOL}" > 'reversed.json' || {
        ec=${?}
        echo "${STATSTOOL} failed!"
        exit ${ec}
    }
    do_comparison 'stats.json' 'reversed.json' >>/dev/null || {
        do_comparison 'stats.json' 'reversed.json'
        echo "${STATSTOOL} mismatch with reverse complemented input."
        exit 1
    }
    shuffle "${1}" | "${STATSTOOL}" > 'shuffled.json' || {
        ec=${?}
        echo "${STATSTOOL} failed!"
        exit ${ec}
    }
    do_comparison 'stats.json' 'shuffled.json' >>/dev/null || {
        echo "${STATSTOOL} mismatch with re-ordered input."
        do_comparison 'stats.json' 'shuffled.json'
        exit 1
    }
}

# Verify diff-tool can see differences.
# The counts of AT bases are swapped with the count of CG bases,
# and the nodes are also swapped.
do_comparison stats.json stats.changed.json >>/dev/null && {
    echo "diff-tool test failed! Differences were not detected!"
    exit 1
}

# Verify diff-tool can detect missing nodes.
do_comparison stats.no-spots.json stats.json >>/dev/null && {
    echo "diff-tool test failed! Deletions were not detected!"
    exit 1
}

# Verify diff-tool can ignore missing nodes when asked.
do_loose_comparison stats.no-spots.json stats.json >>/dev/null || {
    echo "diff-tool test failed! Deletions were not skipped over!"
    exit 1
}

# Now that we know the diff-tool is working,
# let's use it to test the stats tool.

SCRATCH=$(mktemp -d qa-stats-tests.XXXXXX) || {
    echo "Can't make a tempdir!?"
    exit 1
}
ROOT=$(pwd)
trap exit INT HUP TERM
trap "ec=$?; cd ${ROOT}; rm -rf ${SCRATCH}; trap - EXIT; exit ${ec}" EXIT 

cd ${SCRATCH}

echo "Testing with default parameters."
make_input > 'test.dump' || {
    echo "Failed to generate test input!"
    exit 1
}
runtests 'test.dump'

echo "Testing with one biological read."
make_input "layout=150B" > 'test.dump' || {
    echo "Failed to generate test input!"
    exit 1
}
runtests 'test.dump'

echo "Testing with one technical read."
make_input "layout=150T" > 'test.dump' || {
    echo "Failed to generate test input!"
    exit 1
}
runtests 'test.dump'

echo "Testing with two biological read."
make_input "layout=150BF150BR" > 'test.dump' || {
    echo "Failed to generate test input!"
    exit 1
}
runtests 'test.dump'

echo "Testing with one biological and one technical read."
make_input "layout=150BT" > 'test.dump' || {
    echo "Failed to generate test input!"
    exit 1
}
runtests 'test.dump'

echo "Testing with multiple layouts."
make_input "layout=150B,150T,150T150B,150B150T,150BF150BR,FR" > 'test.dump' || {
    echo "Failed to generate test input!"
    exit 1
}
runtests 'test.dump'
