#!python3
import sys

if __name__ != '__main__':
    sys.exit(0)
if sys.platform != 'linux' and sys.platform != 'darwin' and sys.platform != 'freebsd14':
    print( f"wrong platform: {sys.platform}" )
    sys.exit(0)

import os
import random
import re

try:
    vdb_path = os.environ['VDB_PATH']
except KeyError:
    vdb_path = os.path.abspath('../../../../../ncbi-vdb')

py_vdb_path = os.path.join(vdb_path, 'py_vdb')
#print(py_vdb_path);sys.exit(0)

schema_include_path = os.path.join(vdb_path, 'interfaces')
#print(schema_include_path);sys.exit(0)

saveSysPath = sys.path
sys.path.append(py_vdb_path)
from vdb import *
sys.path = saveSysPath

output = 'test-data' if len(sys.argv) < 2 else sys.argv[1]

def findLibrary(what):
    paths = []
    try:
        paths = os.environ['VDB_LIBRARY_PATH'].split(':')
    except:
        pass

    for path in paths:
        results = []
        for file in os.listdir(path):
            if re.search(what, file) == None:
                continue
            if os.path.isfile(os.path.join(path, file)):
                results.append(file)
        if len(results) != 0:
            return os.path.realpath(os.path.join(path, sorted(results)[-1]))

    print('fatal: Can not find ncbi-wvdb dynamic library, try setting VDB_LIBRARY_PATH')
    exit(1)

def randomRead(length:int):
    y = []
    while len(y) <= length:
        y += ['A', 'C', 'G', 'T']
    # y is ACGT repeated enough times that there are at least length elements
    return ''.join(random.sample(y, k=length))

def phred33(x:int):
    return chr(x+33)

def randomQuality(length:int):
    y = []
    # some values will <20, but never enough to have a chance to trigger any
    # of the other rules, regardless of how they are arranged
    while len(y) * 4 <= length and len(y) < 9:
        y.append(random.randrange(2, 20))
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
schema.AddIncludePath(schema_include_path)
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
    good = ''
    if name.startswith('good'):
        cols['READ_FILTER'].write([ 0, 0 ])
        name = name[4:]
    else:
        cols['READ_FILTER'].write([ 3, 3 ])
        name = name[3:]
    if name.startswith('good'):
        good = True
        name = name[4:]
    else:
        good = False
        name = name[3:]
    if name.startswith('lq'):
        type = 'low'
        length = int(name[2:])
    elif name.startswith('f'):
        type = 'front'
        length = int(name[1:])
    elif name.startswith('b'):
        type = 'back'
        length = int(name[1:])
    else:
        type = 'good'
        length = int(name)

    cols['READ_START'].write([0, length])
    cols['READ_LEN'].write([length, length])
    cols['READ'].write(randomRead(length) + randomRead(length))
    cols['QUALITY'].write(randomQuality(length) + randomQuality(length))

    curs.CommitRow()
    curs.CloseRow()
    return True

good = all(map(writeTestData, [
    'goodgood75',
    'goodgood50',
    'badbad75'  ,
    'badbad50'  
]))

cols = None
curs.Commit()
curs = None

assert good
print(f'info: successfully generated test data to \'{output}\'')
