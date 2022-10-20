#!python3
import sys
import argparse
from pathlib import PurePath
import json

parser = argparse.ArgumentParser(description='Generate tests for tool args', allow_abbrev=False)
parser.add_argument('--json', nargs=1, default='tool-args.json', help='json file with tool args definitions.')
parser.add_argument('--barename', action='store_true', help='Use the bare tool name without version or -orig.')
parser.add_argument('--use-version', nargs='?', help='Use this as the version instead of the version in the definitions.')
parser.add_argument('--path', nargs='?', type=PurePath, help='Path to prepend to tool name; expected path to binaries.')
parser.add_argument('data', nargs='?', default='/panfs/pan1/sra-test/bam/GOOD_CMP_READ.csra', help='Accession or path to test with.')
parsed = parser.parse_args()

with open(parsed.json) as f:
    toolArgs = json.load(f)

overrides = {
    'all tools': {
        'debug': {'values': ['ARGS', 'KDB']},
        'log-level': {'values': 'fatal|sys|int|err|warn|info|debug'.split('|')},
        'ncbi_error_report': {'values': 'never|error|always'.split('|')},
        'skip parameters': """
            cart
            location
            ngc
            perm
            option-file
        """.split(),
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
        'data file': 'SRR000001'
    },
    'fastq-dump': {
        'accession': {'value': 'SRR000001'},
        'defline-qual': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'defline-seq': {'values': ['$ac.$si.$ri', '$sg.$sn.$ri']},
        'dumpcs': {'values': ['A', 'C', 'G', 'T']},
        'fasta': { 'value': 75 },
        'matepair-distance': {'values': ['400-500', 'unknown']},
        'offset': { 'value': 64 },
        'outdir': {'value': '.'},
        'read-filter': {'values': 'pass|reject|criteria|redacted'.split('|')},
        'spot-groups': {'value': 'FOO,BAR'},
        'table': {'value': 'SEQUENCE'},
        'skip parameters': [ 'quiet' ]
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
        'skip parameters': ['schema']
    },
    'vdb-dump': {
        'table': {'value': 'SEQUENCE'},
        'columns': {'value': 'NAME,READ'},
        'exclude': {'value': 'CSREAD'},
        'max_length': {'value': 75},
        'format': {'values': "csv xml json piped tab fastq fastq1 fasta fasta1 fasta2 qual qual1".split()},
        'boolean': {'values': '1|T'.split('|')},
        'skip parameters': ['schema', 'slice', 'filter']
    }
}

for tool in toolArgs:
    toolName = tool['name']
    toolVersion = parsed.use_version if parsed.use_version else tool['version']
    toolFullName = toolName if parsed.barename else f'{toolName}-orig.{toolVersion}'
    if parsed.path:
        toolFullName = str(parsed.path.joinpath(toolFullName))
    toolName = "".join(['_' if ord(x) < ord('0') or ord(x) > ord('z') or (ord(x) > ord('9') and ord(x) < ord('A')) or (ord(x) > ord('Z') and ord(x) < ord('a')) else x.upper() for x in list(toolName)])
    tool['var'] = toolName
    tool['path'] = toolFullName

print("""
#!/bin/sh

LOGFILE="${PWD}/${0}.log"
echo "stderr is being logged to ${LOGFILE}"

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
        # echo "${EXE}" "${@}" "${DATA}" >&2
        echo Testing "${NAME}" "${1}" "..." >&2
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
print(f'DATAFILE="{parsed.data}"')
for tool in toolArgs:
    print(f'{tool["var"]}="{tool["path"]}"')

for tool in toolArgs:
    toolName = tool['name']
    toolPath = tool['path']
    toolVar = '${' + tool['var'] + '}'

    override = overrides.get(toolName, {}).copy()
    override.update(overrides['all tools'])
    datafile = override.get('data file', '${DATAFILE}')
    skip = {x: True for x in overrides['all tools']['skip parameters'] + override.get('skip parameters', [])}

    print("")
    print(f'# {toolName} has {len(tool["parameters"])} parameters to test')
    for param in tool['parameters']:
        name = param['name']
        try:
            if skip[name]:
                continue
        except KeyError:
            pass

        fixes = override.get(name, {})
        def param_fixed(prop_name, dflt=None):
            return fixes.get(prop_name, param.get(prop_name, dflt))

        required = param_fixed('argument-required', False)
        optional = param_fixed('argument-optional', False)

        def command(arg):
            if optional or (not required):
                yield f"""run_tool "{toolName}" "{toolVar}" "{datafile}" {arg}"""
            if optional or required:
                for value in param_fixed('values', [ param_fixed('value', '1') ]):
                    yield f"""run_tool "{toolName}" "{toolVar}" "{datafile}" {arg} "{value}" """

        def args():
            yield f"--{name}"
            for x in param_fixed('alias', []):
                yield f"-{x}"

        for arg in args():
            for cmd in command(arg):
                print(cmd)
