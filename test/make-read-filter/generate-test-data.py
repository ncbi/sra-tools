#!python3
import sys

if __name__ != '__main__':
    sys.exit(0)
if sys.platform != 'linux' and sys.platform != 'darwin':
    sys.exit(0)

import os
import random
import re

saveSysPath = sys.path
sys.path.append(os.path.abspath('../../../ncbi-vdb/py_vdb'))
from vdb import *
sys.path = saveSysPath

output = 'test-data' if len(sys.argv) < 2 else sys.argv[1]

def findLibrary(what):
    paths = []
    try:
        paths = os.environ['LD_LIBRARY_PATH'].split(':')
    except:
        pass

    if len(paths) == 0:
        try:
            outdir = os.path.realpath(os.path.expanduser(os.path.join('~', 'ncbi-outdir', 'ncbi-vdb', 'mac' if sys.platform == 'darwin' else sys.platform)))
            if not os.path.exists(outdir):
                raise 'no output directory'
            if os.path.exists(os.path.join(outdir, 'gcc')):
                outdir = os.path.join(outdir, 'gcc')
            elif os.path.exists(os.path.join(outdir, 'clang')):
                outdir = os.path.join(outdir, 'clang')
            else:
                raise 'no compiler directory'
            if os.path.exists(os.path.join(outdir, 'x86_64', 'dbg', 'lib')):
                outdir = os.path.join(outdir, 'x86_64', 'dbg', 'lib')
            elif os.path.exists(os.path.join(outdir, 'x86_64', 'rel', 'lib')):
                outdir = os.path.join(outdir, 'x86_64', 'rel', 'lib')
            else:
                raise 'no build directory'
            paths.append(outdir)
        except:
            raise

    for path in paths:
        results = []
        for file in os.listdir(path):
            if re.search(what, file) == None:
                continue
            if os.path.isfile(os.path.join(path, file)):
                results.append(file)
        if len(results) != 0:
            return os.path.realpath(os.path.join(path, sorted(results)[-1]))

    print('fatal: Can not find ncbi-wvdb dynamic library, try adding the path to LD_LIBRARY_PATH')
    exit(1)

def randomRead(length:int):
    y = []
    while len(y) <= length:
        y += ['A', 'C', 'G', 'T']
    # y is ACGT repeated enough times that there are at least length elements
    return ''.join(random.sample(y, k=length))

def phred33(x:int):
    return chr(x+33)

# NOTE: Rules for filtering
#  Quote:
#      Reads that have more than half of quality score values <20 will be
#      flagged ‘reject’.
#      Reads that begin or end with a run of more than 10 quality scores <20
#      are also flagged ‘reject’.
#
def randomQuality(length:int, type:str):
    y = []
    if type == 'low':
        while len(y) * 2 <= length:
            y.append(random.randrange(2, 21))
        while len(y) < length:
            y.append(random.randrange(20,41))
        random.shuffle(y)
    elif type == 'front':
        while len(y) < 10:
            y.append(random.randrange(2, 21))
        while len(y) < length:
            y.append(random.randrange(20,41))
    elif type == 'back':
        while len(y) + 10 < length:
            y.append(random.randrange(20,41))
        while len(y) < length:
            y.append(random.randrange(2, 21))
    else:
        # some values will <20, but never enough to have a chance to trigger any
        # of the other rules, regardless of how they are arranged
        while len(y) * 4 <= length and len(y) < 9:
            y.append(random.randrange(2, 21))
        while len(y) < length:
            y.append(random.randrange(20,41))
        random.shuffle(y)
    return ''.join(map(phred33, y))[0:length]


cursDef = {
     'READ_FILTER': {
         'expression': '(U8)READ_FILTER',
     },
     'READ_TYPE': {
         'expression': '(U8)READ_TYPE',
         'default': [1, 1],
     },
     'NAME': {
         'expression': '(ascii)NAME',
     },
     'READ_START': {
         'expression': '(INSDC:coord:zero)READ_START',
     },
     'READ_LEN': {
         'expression': '(INSDC:coord:len)READ_LEN',
     },
     'READ': {
         'expression': '(INSDC:dna:text)READ',
     },
     'QUALITY': {
         'expression': '(INSDC:quality:text:phred_33)QUALITY',
     },
}

wvdb = findLibrary(r'ncbi-wvdb')
#print(wvdb);sys.exit(0)
mgr = manager(OpenMode.Write, wvdb)
print('info: loaded VDBManager', mgr.Version(), 'from', wvdb)
schema = mgr.MakeSchema()
schema.AddIncludePath(os.path.abspath('../../../ncbi-vdb/interfaces'))
schema.ParseFile('sra/generic-fastq.vschema')
db = mgr.CreateDB(schema, 'NCBI:SRA:GenericFastq:db', output)
schema = None
mgr = None
tbl = db.CreateTable('SEQUENCE')
db = None
curs = tbl.CreateCursor(OpenMode.Write)
tbl = None
cols = {}
for name, define in cursDef.items():
    cols[name] = curs.AddColumn(define['expression'])
curs.Open()
for col in cols.values():
    col._update()
for name, define in cursDef.items():
    if 'default' in define:
        cols[name].set_default(define['default'])

def writeTestData(name:str):
    curs.OpenRow()

    cols['NAME'].write(name)
    type = ''
    length = ''
    if name.startswith('good'):
        cols['READ_FILTER'].write([ 0, 0 ])
        name = name[4:]
    else:
        cols['READ_FILTER'].write([ 1, 1 ])
        name = name[3:]
    if name.startswith('good'):
        type = 'good'
        length = int(name[4:])
    elif name.startswith('badlq'):
        type = 'low'
        length = int(name[5:])
    elif name.startswith('badf'):
        type = 'front'
        length = int(name[4:])
    elif name.startswith('badb'):
        type = 'back'
        length = int(name[4:])
    else:
        assert "unreachable"

    cols['READ_START'].write([0, length])
    cols['READ_LEN'].write([length, length])
    cols['READ'].write(randomRead(length) + randomRead(length))
    cols['QUALITY'].write(randomQuality(length, type) + randomQuality(length, type))

    curs.CommitRow()
    curs.CloseRow()
    return True

good = all(map(writeTestData, [
    'goodgood75' ,
    'goodgood50' ,
    'goodbadlq75',
    'goodbadlq50',
    'goodbadf75' ,
    'goodbadf50' ,
    'goodbadb75' ,
    'goodbadb50' ,
    'badgood75'  ,
    'badgood50'  ,
    'badbadlq75' ,
    'badbadlq50' ,
    'badbadf75'  ,
    'badbadf50'  ,
    'badbadb75'  ,
    'badbadb50'  ,
]))

cols = None
curs.Commit()
curs = None

assert good
print(f'info: successfully generated test data to \'{output}\'')
