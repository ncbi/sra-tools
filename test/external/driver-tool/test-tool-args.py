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
parser.add_argument('--json', nargs='?', type=argparse.FileType('r'), default=sys.stdin, help='json file with tool args definitions.')
parser.add_argument('--quiet', action='store_true', help='Make the script quiet.')
parser.add_argument('--all', action='store_true', help='Test all parameters.')
parser.add_argument('--use-version', nargs='?', dest='version', help='Use this as the version instead of the version in the definitions.')
parser.add_argument('--path', nargs='?', type=PurePath, help='Path to prepend to tool name; expected path to binaries.')
parser.add_argument('--installed', nargs='?', type=yes_or_no, const=True, default=None)
parser.add_argument('data', nargs='?', default='/panfs/pan1/sra-test/bam/GOOD_CMP_READ.csra', help='Accession or path to test with.')
args = parser.parse_args()

have_installed = False if args.installed == None else True
if not have_installed:
    args.installed = False if args.path else True

toolArgs = json.load(args.json)

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

if args.all:
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


def effectiveVersion():
    versions = frozenset([x['version'] for x in toolArgs])
    if len(versions) != 1:
        raise AssertionFailure(f"versions do not all match, got {versions}")

    version = next(iter(versions))  # get some element of the set (since there is only one)
    if args.version and version != args.version:
        raise AssertionFailure(f"versions do not all match requested version {args.version}, got {version}")

    return version


def quoteForShell(s):
    return f"'{s}" if '$' in str(s) else f'"{s}"'

version = effectiveVersion()


def prepare(tool):
    # compute path
    toolName = tool['name']

    # make a variable name to hold the path that will be okay for shell script
    # replace all not isalnum with '_'
    tool['var'] = ''.join([x.upper() if x.isalnum() else '_' for x in list(toolName)])
    versName = f'{toolName}.{version}'
    origName = f'{toolName}-orig.{version}'
    realName = origName if args.installed else versName

    toolRealPath = realName
    toolBarePath = toolName

    if args.installed:
        tool['path'] = toolRealPath
        tool['path.bare'] = toolBarePath
        tool['var.bare'] = f"{tool['var']}_BARE"
    else:
        tool['path'] = toolName
        tool['var.bare'] = f"SRATOOLS"


def preamble():
    print("#!/bin/sh\n")

    if not have_installed:
        print(f"# assuming{' ' if args.installed else ' not '}installed")

    if args.path:
        print(f'PATH="{args.path}":'+'${PATH}')

    if not args.installed:
        print('SRATOOLS="sratools"')

    for tool in toolArgs:
        if args.installed:
            print(f'{tool["var.bare"]}="{tool["path.bare"]}"')
        print(f'{tool["var"]}="{tool["path"]}"')

    print(f'\nDATAFILE="{args.data}"')

    print("""
TEMPDIR="$(mktemp -d)"
cleanup () {
    trap - INT QUIT TERM HUP EXIT
    rm -rf "${TEMPDIR}"
    kill -TERM $(jobs -p) 2>/dev/null || true
}
trap 'exit' INT QUIT TERM HUP
trap cleanup EXIT

LOGFILE="${LOGFILE:-${TEMPDIR}/test-all-args.log}"
SUCCESS='YES'

run_tool () {
    EXE="${1}"
    shift

    WORKDIR="$(mktemp -d -p ${TEMPDIR})"
    (
        cd "${WORKDIR}" && \\
        "${EXE}" "${@}" >/dev/null
    ) && \\
    rm -rf "${WORKDIR}"
}

test () {
    NAME="${1}"
    BARE_EXE="${2}" # This is the driver
    EXE="${3}"      # This is the driven
    DATA="${4}"
    shift 4

    # verify that driver tool will accept the command line
    ( SRATOOLS_DRY_RUN=1 run_tool "${BARE_EXE}" "${@}" "${DATA}" 2>stderr.log ) || {
        echo "Failed in driver tool: ${NAME} ${1}" >&2
        {
            echo "-------- in driver tool --------"
            echo "${BARE_EXE}" "${@}" "${DATA}"
            cat stderr.log
            echo "--------------------------------"
        } >> ${LOGFILE}
        SUCCESS=''
        return
    }

    # done if testing driver only
    [[ "${EXE}" ]] || return

    # verify that actual tool will accept the command line
    run_tool "${EXE}" "${@}" "${DATA}" 2>stderr.log || {
        echo "Failed (direct): ${NAME} ${1}" >&2
        {
            echo "-------- direct to tool --------"
            echo "${EXE}" "${@}" "${DATA}"
            cat stderr.log
            echo "--------------------------------"
        } >> ${LOGFILE}
        SUCCESS=''
        return
    }

    # verify that via driver tool will accept the command line
    run_tool "${BARE_EXE}" "${@}" "${DATA}" 2>stderr.log || {
        echo "Failed (via driver tool): ${NAME} ${1}" >&2
        {
            echo "-------- via driver tool -------"
            echo "${BARE_EXE}" "${@}" "${DATA}"
            cat stderr.log
            echo "--------------------------------"
        } >> ${LOGFILE}
        SUCCESS=''
        return
    }
}

which sratools 2>&1 >/dev/null || {
    echo no sratools in ${PATH} >&2
    exit 1
}

SRR000001="${TEMPDIR}/SRR000001"

SAM_HDR="${TEMPDIR}/test.header.sam"
printf '@HD\\tVN:1.6\\tSO:unknown\\n' > ${SAM_HDR}

printf '\\n\\nStarting tests at %s\\n\\n' "$(date)" >> ${LOGFILE}

(
    cd "${TEMPDIR}"
    prefetch SRR000001 2>prefetch.log 1>/dev/null || {
        echo "Failed to prefetch SRR000001" >&2
        cat prefetch.log >&2
        exit 1
    }
) || {
    trap - INT QUIT TERM HUP EXIT
    rm -rf "${TEMPDIR}"
    exit 1
}

[[ -e "${DATAFILE}" ]] || {
    echo no data file "${DATAFILE}" >&2
    exit 1
}
DATAFILE="$( cd $(dirname "${DATAFILE}"); pwd -P )"
""")

    if not args.quiet:
        print('echo "logging to ${LOGFILE}"')


def process(tool):
    toolName = tool['name']
    toolVar = '${' + tool['var'] + '}'
    toolVarBare = '${' + tool['var.bare'] + '}'

    datafile = get_tool_property(toolName, 'data file', "${DATAFILE}")
    print("")
    params = parametersFor(tool)
    print(f'# {toolName} has {len(params)} parameters to test')
    if not args.installed:
        print(f'SRATOOLS_IMPERSONATE="{toolName}"')

    for param in params:
        name = param['name']
        if args.quiet:
            print(f'\n# Testing {toolName} --{name} ...')
        else:
            print(f'\necho "Testing {toolName} --{name} ..." >&2')

        def get_property(prop, dflt):
            value = param.get(prop, dflt)
            return get_param_property(toolName, name, prop, value)

        def all_args():
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
                yield arg
            if optional or required:
                for value in get_property('values', [ get_property('value', '1') ]):
                    yield arg + ' ' + quoteForShell(value)

        for arg in all_args():
            for cmd in command(arg):
                print(cmdbase + cmd)

for tool in toolArgs:
    prepare(tool)

preamble()

for tool in toolArgs:
    process(tool)

print("""
[[ "${SUCCESS}" ]] && {
    echo "All tests passed" >&2
    exit 0
}
echo "Some tests failed" >&2
cat ${LOGFILE} >&2
exit 1
""")
