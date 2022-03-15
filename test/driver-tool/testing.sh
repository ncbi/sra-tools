#!/bin/sh

[ $# -gt 0 ] || { echo "Usage: $0 <binaries directory>"; exit 1; }

SRATOOLS="$1/sratools"
[ -x "${SRATOOLS}" ] || { echo "no sratools in '$1'"; exit 1; }

SETTINGS=$(cat << 'EOF'
/LIBS/GUID = "c1d99592-6ab7-41b2-bfd0-8aeba5ef8498"
/libs/cloud/report_instance_identity = "true"
EOF
)

SETTINGS_FILE=$(mktemp "${TMPDIR:-/tmp}/$(basename $0).XXXXXX.mkfg")
printf '%s' "${SETTINGS}" > "${SETTINGS_FILE}" || exit 1

mkdir -p actual

echo 'Checking that test modes allow bad inputs.'

env NCBI_SETTINGS="${SETTINGS_FILE}" \
    SRATOOLS_TESTING=2 \
    SRATOOLS_IMPERSONATE=prefetch \
    "${SRATOOLS}" --perm foo.jwt --cart foo.cart --ngc foo.ngc DRX000001 2>actual/testing.stderr
    
diff -q expected/testing.stderr actual/testing.stderr >/dev/null || diff -q expected/testing-cloudy.stderr actual/testing.stderr >/dev/null
res=$?

rm -rf "${NCBI_SETTINGS}"
[ ${res} -eq 0 ] && { echo "Driver tool test 'testing' finished."; exit 0; }

echo "Driver tool test 'testing' FAILED!"
diff expected/testing.stderr actual/testing.stderr || diff expected/testing-cloudy.stderr actual/testing.stderr 
exit 1
