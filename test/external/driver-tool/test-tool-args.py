#!python3
import sys
import argparse
from pathlib import PurePath
import json

parser = argparse.ArgumentParser(description='Generate tests for tool args', allow_abbrev=False)
parser.add_argument('--json', nargs=1, default='tool-args.json', help='json file with tool args definitions.')
parser.add_argument('--barename', action='store_true', help='Use the bare tool name without version or -orig.')
parser.add_argument('--quiet', action='store_true', help='Make the script quiet.')
parser.add_argument('--all', action='store_true', help='Test all parameters.')
parser.add_argument('--use-version', nargs='?', help='Use this as the version instead of the version in the definitions.')
parser.add_argument('--path', nargs='?', type=PurePath, help='Path to prepend to tool name; expected path to binaries.')
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
        'ncbi_error_report': {'values': 'never|error|always'.split('|')},
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
        'outdir': {'value': '.'},
        'qual-defline': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'seq-defline': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'size-check': {'values': ['on', 'off', 'only']},
        'table': {'value': 'SEQUENCE'},
        'temp': {'value': "${TMPDIR:-/tmp}"},
        'data file': 'SRR000001' # fasterq-dump doesn't like very small CSRAs
    },
    'fastq-dump': {
        'accession': {'value': 'SRR000001'},
        'defline-qual': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'defline-seq': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'dumpcs': {'values': ['A', 'C', 'G', 'T']},
        'fasta': { 'value': 75 },
        'matepair-distance': {'values': ['400-500', 'unknown']},
        'offset': { 'value': [33, 64] },
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
        'header-file': {'value': "test.header.sam"},
        'qual-quant': {'value': '1:10,10:20,20:30,30:-'}
    },
    'sra-pileup': {
        'table': {'values': 'p|s'.split('|')},
        'function': {'values': 'ref ref-ex count stat mismatch index varcount deletes indels'.split()},
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
        'format': {'values': "csv xml json piped tab fastq fastq1 fasta fasta1 fasta2 qual qual1".split()},
        'boolean': {'values': '1|T'.split('|')},
        'idx-range': { 'data file': 'SRR000001', 'value': 'skey' },
        'skip parameters': ['schema', 'slice', 'filter']
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


def prepare(tool):
    # compute path
    toolName = tool['name']
    if parsed.barename:
        tool['path'] = f"""$(which '{toolName}')"""
    else:
        toolVersion = parsed.use_version if parsed.use_version else tool['version']
        toolFullName = f'{toolName}-orig.{toolVersion}'
        if parsed.path:
            tool['path'] = str(parsed.path.joinpath(toolFullName))
        else:
            tool['path'] = f"""$(which '{toolFullName}')"""

    # make a variable name to hold the path that will be okay for shell script
    # replace all not isalnum with '_'
    tool['var'] = ''.join([x.upper() if x.isalnum() else '_' for x in list(toolName)])


def printPreamble():
    print("""#!/bin/sh

LOGFILE="${PWD}/${0}.log"
""")
    if not parsed.quiet:
        print("""echo "stderr is being logged to ${LOGFILE}" """)
    print("""printf '\\n\\nStarting tests at %s\\n\\n' "$(date)" >> ${LOGFILE}

run_tool () {
    WORKDIR=`mktemp -d -p .`
    (
        cd ${WORKDIR}
        echo "@HD\tVN:1.6\tSO:unknown" > test.header.sam
        NAME="${1}"
        EXE="${2}"
        DATA="${3}"
        shift
        shift
        shift
""")
    if not parsed.quiet:
        print("""echo Testing "${NAME}" "${1}" "..." >&2""")
    print("""
        "${EXE}" "${@}" "${DATA}" 2>stderr.log >/dev/null || {
             echo "Failed: ${NAME}" "${1}" >&2
             {
                echo "--------"
                echo "${EXE}" "${@}" "${DATA}"
                cat stderr.log
                echo "--------"
            } >> ${LOGFILE}
        }
    )
    rm -rf ${WORKDIR}
}
    """)
    print(f'DATAFILE=$(realpath --canonicalize-existing "{parsed.data}")')
    for tool in toolArgs:
        print(f'{tool["var"]}="{tool["path"]}"')


def process(tool):
    toolName = tool['name']
    toolPath = tool['path']
    toolVar = '${' + tool['var'] + '}'

    datafile = get_tool_property(toolName, 'data file', "${DATAFILE}")
    print("")
    params = parametersFor(tool)
    print(f'# {toolName} has {len(params)} parameters to test')

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

        cmdbase = f"""run_tool "{toolName}" "{toolVar}" "{datafile}" """

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

printPreamble()

for tool in toolArgs:
    process(tool)
