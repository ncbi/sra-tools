#!/usr/bin/env sh

[ ${#} -gt 0 ] || { echo "Usage: ${0} <sratools binaries directory>"; exit 1; }
[ -d ${1} ] || { echo "${1} no such directory"; exit 1; }

BINDIR=$(cd "${1}"; echo $PWD) || exit 1
[ -d "${BINDIR}" ] || { echo "${BINDIR} no such directory" >&2 ; exit 1; }

CONV_OUT="${PWD}/tool-arguments.h"
{
    echo "/* Tool arguments definitions"
    echo " * auto-generated"
    echo " */"
    echo ""
} > "${CONV_OUT}"
(
    for exe in 'vdb-dump' 'fasterq-dump' 'sam-dump' 'sra-pileup' 'prefetch'
    do
        EXEPATH="${BINDIR}/${exe}"
        [ -x "${EXEPATH}" ] || { echo "${EXEPATH} no such executable" >&2 ; exit 1; }
        SRATOOLS_DUMP_OPTIONS="${CONV_OUT}" "${EXEPATH}" || exit $?
    done
) && { date -u +"* generated at %Y-%m-%dT%H:%M:%SZ * " > "${CONV_OUT}.timestamp"; }
exit $?
