#!python3
import sys
import argparse
from pathlib import PurePath
import json


def yes_or_no(v):
    if v.lower() in ['y', 'yes', 't', 'true', 1]:
        return True
    if v.lower() in ['n', 'no', 'f', 'false', 0]:
        return False
    raise argparse.ArgumentTypeError("Expected a boolean or yes/no")


parser = argparse.ArgumentParser(description='Generate tests for tool args', allow_abbrev=False)
parser.add_argument('--json', nargs=1, default='tool-args.json', help='json file with tool args definitions.')
parser.add_argument('--quiet', action='store_true', help='Make the script quiet.')
parser.add_argument('--all', action='store_true', help='Test all parameters.')
parser.add_argument('--use-version', nargs='?', help='Use this as the version instead of the version in the definitions.')
parser.add_argument('--path', nargs='?', type=PurePath, help='Path to prepend to tool name; expected path to binaries.')
parser.add_argument('--installed', nargs='?', type=yes_or_no, const=True, default=True)
parser.add_argument('data', nargs='?', default='/panfs/pan1/sra-test/bam/GOOD_CMP_READ.csra', help='Accession or path to test with.')
parsed = parser.parse_args()


with open(parsed.json) as f:
    toolArgs = json.load(f)


index = {x['name']: i for i, x in enumerate(toolArgs)}

for tool in toolArgs:
    for i, x in enumerate(tool['parameters']):
        x['index'] = i
    tool['index'] = {x['name']: x['index'] for x in tool['parameters']}

overrides = {
    'all tools': {
        'debug': {'values': ['ARGS', 'KDB']},
        'log-level': {'values': 'fatal|sys|int|err|warn|info|debug'.split('|')},
        'skip parameters': [
            'cart',        # I don't have a cart file
            'location',
            'ngc',         # I don't have a ngc file
            'perm',        # I don't have a perm file
            'option-file'  # too complicated to test
        ],
    },
    'fasterq-dump': {
        'bases': {'values': ['A', 'C', 'G', 'T']},
        'bufsize': {'value': 1024 * 1024},
        'disk-limit': {'value': 500 * 1024 * 1024},
        'disk-limit-tmp': {'value': 500 * 1024 * 1024},
        'ncbi_error_report': {'values': 'never|error|always'.split('|')},
        'outdir': {'value': '.'},
        'qual-defline': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'seq-defline': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'size-check': {'values': ['on', 'off', 'only']},
        'stdout': {'skip-tool': True},
        'table': {'value': 'SEQUENCE'},
        'temp': {'value': "${TMPDIR:-/tmp}"},
        'data file': '${SRR000001}' # fasterq-dump doesn't like very small CSRAs
    },
    'fastq-dump': {
        'accession': {'value': 'SRR000001'},
        'defline-qual': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'defline-seq': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'dumpcs': {'values': ['A', 'C', 'G', 'T']},
        'fasta': { 'value': 75 },
        'matepair-distance': {'values': ['400-500', 'unknown']},
        'ncbi_error_report': {'values': 'never|error'.split('|')},
        'offset': { 'values': [33, 64] },
        'outdir': {'value': '.'},
        'read-filter': {'values': 'pass|reject|criteria|redacted'.split('|')},
        'spot-groups': {'value': 'FOO,BAR'},
        'table': {'value': 'SEQUENCE'},
        'skip parameters': [
            'quiet',   # conflicts with another one of fastq-dump's arguments
            'verbose'  # does not work with fastq-dump's custom args parsing
        ]
    },
    'sam-dump': {
        'matepair-distance': {'values': ['400-500', 'unknown']},
        'header-comment': {'value': "\t".join(['@CO', 'This is a comment.'])},
        'header-file': {'value': "${SAM_HDR}"},
        'ncbi_error_report': {'values': 'never|error'.split('|')},
        'qual-quant': {'value': '1:10,10:20,20:30,30:-'},
        'skip parameters': [
            'cigar-test',   # I don't know how to use this one
            'with-md-flag'  # I don't know how to use this one
        ]
    },
    'sra-pileup': {
        'table': {'values': 'p|s'.split('|')},
        'function': {'values': 'ref ref-ex count stat mismatch index varcount deletes indels'.split()},
        'minmapq': {'alias': []}, # -q alias is deprecated
        'ncbi_error_report': {'values': 'never|error'.split('|')},
        'skip parameters': [
            'schema',  # this is too complicated to make work
            'noskip'   # this one makes sra-pileup run very slow
        ]
    },
    'vdb-dump': {
        'table': {'value': 'SEQUENCE'},
        'columns': {'value': 'NAME,READ'},
        'exclude': {'value': 'CSREAD'},
        'max_length': {'value': 75},
        'ncbi_error_report': {'values': 'never|error|always'.split('|')},
        'format': {'values': "csv xml json piped tab fastq fastq1 fasta fasta1 fasta2 qual qual1".split()},
        'boolean': {'values': '1|T'.split('|')},
        'idx-range': { 'data file': '${SRR000001}', 'value': 'skey' },
        'skip parameters': [
            'schema',
            'slice',
            'filter',
            'static'  # doesn't work right
        ]
    }
}

if parsed.all:
    for key in overrides:
        if 'skip parameters' in overrides[key]:
            del overrides[key]['skip parameters']


def is_dicty(d):
    try:
        return callable(d.keys) # dict has keys
    except AttributeError:
        return false


def is_listy(l):
    try:
        return callable(l.reverse) # list has an order that can be reversed
    except AttributeError:
        return false


def is_stringy(s):
    try:
        return callable(s.upper) # strings have upper and lower case
    except AttributeError:
        return false


def resolve_override(forAll, forOne, dflt):
    if forAll and forOne:
        pass
    elif forAll:
        return forAll
    elif forOne:
        return forOne
    else:
        return dflt

    try: # try as a union of two dictionaries
        return forOne | forAll
    except TypeError:
        pass

    listy = [is_listy(forOne), is_listy(forAll)]
    if all(listy):
        return forAll + [v for v in forOne if v not in forAll]
    if listy[0]:
        return [ forAll ] + [v for v in forOne if v != forAll]
    if listy[1]:
        return forAll + ([ forOne ] if forOne not in forAll else [])

    return forAll


def get_param_property(tool, param, name, dflt):
    forAll = None
    try:
        forAll = overrides['all tools'][param][name]
        # print(f"overrides['all tools']['{param}']['{name}']", forAll)
    except KeyError:
        pass
    forTool = None
    try:
        forTool = overrides[tool][param][name]
        # print(f"overrides['{tool}']['{param}']['{name}']", forTool)
    except KeyError:
        pass

    return resolve_override(forAll, forTool, dflt)


def get_tool_property(tool, name, dflt):
    forAll = None
    try:
        forAll = overrides['all tools'][name]
    except KeyError:
        pass
    forTool = None
    try:
        forTool = overrides[tool][name]
    except KeyError:
        pass

    return resolve_override(forAll, forTool, dflt)


def parametersFor(tool):
    skip = {x: True for x in get_tool_property(tool['name'], 'skip parameters', [])}
    if skip:
        print("# Skipping some parameters:")
        for x in skip:
            print(f"#   {x}")
    return [x for x in tool['parameters'] if x['name'] not in skip]

version=None
def prepare(tool):
    global version

    # compute path
    toolName = tool['name']

    # make a variable name to hold the path that will be okay for shell script
    # replace all not isalnum with '_'
    tool['var'] = ''.join([x.upper() if x.isalnum() else '_' for x in list(toolName)])

    toolVers = tool['version']
    if version == None:
        version = toolVers
    elif version != toolVers:
        raise AssertionFailure("versions do not all match")

    versName = f'{toolName}.{version}'
    origName = f'{toolName}-orig.{version}'
    realName = origName if parsed.installed else versName

    if parsed.path:
        toolRealPath = str(parsed.path.joinpath(realName))
        toolBarePath = str(parsed.path.joinpath(toolName))
    elif parsed.installed:
        toolRealPath = "/".join(["${SRATOOLS_BINDIR}", realName])
        toolBarePath = "/".join(["${SRATOOLS_BINDIR}", toolName])
    else:
        toolRealPath = f"$(which '{realName}')"
        toolBarePath = f"$(which '{toolName}')"

    if parsed.installed:
        tool['path'] = toolRealPath
        tool['path.bare'] = toolBarePath
        tool['var.bare'] = f"{tool['var']}_BARE"
    else:
        tool['path'] = toolRealPath
        tool['var.bare'] = f"SRATOOLS"

def preamble():
    print("""#!/bin/sh

TOP_PID=$$
TEMPDIR="$(mktemp -d)"
LOGFILE="${TEMPDIR}/${0}.log"
(
    cd "${TEMPDIR}"
    prefetch SRR000001 2>${LOGFILE} 1>/dev/null \\
    && echo "@HD\tVN:1.6\tSO:unknown" > test.header.sam
) \\
|| {
    echo "Failed to prefetch SRR000001" >&2
    cat ${LOGFILE} >&2
    rm -rf ${TEMPDIR}
    exit 1
}

SRR000001="${TEMPDIR}/SRR000001"
SAM_HDR="${TEMPDIR}/test.header.sam"

trap 'rm -rf "${SAM_HDR}" "${SRR000001}"' INT QUIT TERM HUP EXIT
""")
    if not parsed.quiet:
        print("""echo "stderr is being logged to ${LOGFILE}" """)
    print("""printf '\\n\\nStarting tests at %s\\n\\n' "$(date)" >> ${LOGFILE}

run_tool () (
    EXE="${1}"
    shift

    WORKDIR="$(mktemp -d -p .)"
    cleanup () { EC=$?; rm -rf "${WORKDIR}"; exit $EC; }
    trap "false; cleanup; kill -INT $TOP_PID;" INT
    trap "false; cleanup; kill -QUIT $TOP_PID;" QUIT
    trap "false; cleanup; kill -TERM $TOP_PID;" TERM
    trap "false; cleanup; kill -HUP $TOP_PID;" HUP
    trap cleanup EXIT
    ( cd "${WORKDIR}" && "${EXE}" "${@}" )
)

test () {
    NAME="${1}"
    BARE_EXE="${2}" # This is the driver
    EXE="${3}"      # This is the driven
    DATA="${4}"
    shift 4
""")
    if not parsed.quiet:
        print("""    echo Testing "${NAME}" "${1}" "..." >&2""")

    print("""
    # verify that driver tool will accept the command line
    ( SRATOOLS_DRY_RUN=1 run_tool "${BARE_EXE}" "${@}" "${DATA}" 2>stderr.log >/dev/null ) || {
        echo "Failed in driver tool: ${NAME}" "${1}" >&2
        {
            echo "-------- in driver tool --------"
            echo "${BARE_EXE}" "${@}" "${DATA}"
            cat stderr.log
            echo "--------------------------------"
        } >> ${LOGFILE}
        return
    }
    [[ "${EXE}" ]] || return
    # verify that actual tool will accept the command line
    run_tool "${EXE}" "${@}" "${DATA}" 2>stderr.log >/dev/null || {
        echo "Failed (direct): ${NAME}" "${1}" >&2
        {
            echo "-------- direct to tool --------"
            echo "${EXE}" "${@}" "${DATA}"
            cat stderr.log
            echo "--------------------------------"
        } >> ${LOGFILE}
        return
    }
    # verify that via driver tool will accept the command line
    run_tool "${BARE_EXE}" "${@}" "${DATA}" 2>stderr.log >/dev/null || {
        echo "Failed (via driver tool): ${NAME}" "${1}" >&2
        {
            echo "-------- via driver tool -------"
            echo "${BARE_EXE}" "${@}" "${DATA}"
            cat stderr.log
            echo "--------------------------------"
        } >> ${LOGFILE}
        return
    }
}
    """)
    print(f'DATAFILE=$(realpath --canonicalize-existing "{parsed.data}")')
    if parsed.installed:
        path = str(parsed.path) if parsed.path else f"$(dirname $(which sratools.{version}))"
        print(f'SRATOOLS_BINDIR="{path}"')
    else:
        path = str(parsed.path.joinpath(f'sratools.{version}')) if parsed.path else f"$(which 'sratools.{version}')"
        print(f'SRATOOLS="{path}"')
    for tool in toolArgs:
        if parsed.installed:
            print(f'{tool["var.bare"]}="{tool["path.bare"]}"')
        print(f'{tool["var"]}="{tool["path"]}"')


def process(tool):
    toolName = tool['name']
    toolVar = '${' + tool['var'] + '}'
    toolVarBare = '${' + tool['var.bare'] + '}'

    datafile = get_tool_property(toolName, 'data file', "${DATAFILE}")
    print("")
    params = parametersFor(tool)
    print(f'# {toolName} has {len(params)} parameters to test')
    if not parsed.installed:
        print(f'SRATOOLS_IMPERSONATE="{toolName}"')

    for param in params:
        name = param['name']
        def get_property(prop, dflt):
            value = param.get(prop, dflt)
            return get_param_property(toolName, name, prop, value)

        def args():
            yield f"--{name}"
            for x in get_property('alias', []):
                yield f"-{x}"

        required = get_property('argument-required', False)
        optional = get_property('argument-optional', False)
        skipTool = get_property('skip-tool', False)
        effectiveToolVar = '' if skipTool else toolVar
        effectiveDatafile = get_property('data file', datafile)

        cmdbase = f"""test "{toolName}" "{toolVarBare}" "{effectiveToolVar}" "{effectiveDatafile}" """

        def command(arg):
            if optional or (not required):
                yield cmdbase + arg
            if optional or required:
                for value in get_property('values', [ get_property('value', '1') ]):
                    yield cmdbase + arg + f' "{value}"'

        for arg in args():
            for cmd in command(arg):
                print(cmd)

for tool in toolArgs:
    prepare(tool)

preamble()

for tool in toolArgs:
    process(tool)
