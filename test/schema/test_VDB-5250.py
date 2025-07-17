#!python3
import sys

if __name__ != '__main__':
    sys.exit(0)
if sys.platform == 'linux':
    platforms = ('linux', )
    def libname(what):
        return f"lib{what}.so"
elif sys.platform == 'darwin':
    platforms = ('mac', )
    def libname(what):
        return f"lib{what}.dylib"
else:
    sys.exit(0)

import os

saveSysPath = sys.path
sys.path.append(os.path.abspath(os.environ['VDB_SRCDIR'] + '/py_vdb'))
from vdb import *
sys.path = saveSysPath

def allStandardBuildLocations(product: str, leaf: str):
    try:
        home = os.environ['HOME']
    except KeyError:
        return

    path = [home, 'ncbi-outdir', product]
    for osname in platforms:
        path.append(osname)
        for ccname in ('clang', 'gcc', ):
            path.append(ccname)
            for arch in ('x86_64', 'i386', ):
                path.append(arch)
                for btype in ('dbg', 'rel', ):
                    yield os.path.join(*path, btype, leaf)
                del path[-1]
            del path[-1]
        del path[-1]

def findLibrary(what):
    paths = []
    try:
        seen = {}
        for path in os.environ['VDB_LIBRARY_PATH'].split(':'):
            if path in seen: continue
            seen[path] = True
            if os.path.isdir(path):
                paths.append(path)
    except KeyError:
        paths = [x for x in allStandardBuildLocations('ncbi-vdb', 'lib') if os.path.isdir(x)]

    for path in paths:
        results = []
        fullpath = os.path.join(path, libname(what))
        if os.path.isfile(fullpath):
            return os.path.realpath(fullpath)

    print(f'fatal: Can not find {what} dynamic library, try setting VDB_LIBRARY_PATH')
    exit(1)


vdb = findLibrary('ncbi-wvdb')

mgr = manager(OpenMode.Write, vdb)
# print('info: loaded VDBManager', mgr.Version(), 'from', vdb)
mgr.InitLogging('test_VDB-5250.py')
mgr.SetLogLevel(5)

schema = mgr.MakeSchema()
schema.AddIncludePath(os.path.abspath(os.environ['VDB_SRCDIR'] + '/interfaces'))
schema.AddIncludePath(os.path.abspath('../../libs/schema'))
schema.ParseFile('sra/454.vschema')
db = mgr.CreateDB(schema, 'NCBI:SRA:_454_:db', "test_VDB-5250.db")
schema = None
mgr = None
tbl = db.CreateTable('SEQUENCE')
db = None
curs = tbl.CreateCursor(OpenMode.Write)
tbl = None
cols = {}

cursDef = {
    'READ': {},
    'RAW_NAME': {},
}

for name, define in cursDef.items():
    cols[name] = curs.AddColumn(define.get('expression', name))
curs.Open()

for col in cols.values():
    col._update()
for name, define in cursDef.items():
    try:
        cols[name].set_default(define['default'])
    except KeyError:
        pass


def writeTestData(values: list):
    curs.OpenRow()

    cols['READ'].write(values[0])
    cols['RAW_NAME'].write(values[1])

    curs.CommitRow()
    curs.CloseRow()
    return True

data = (
    ('CTCAGGCAAAGACGCAGCGAAATCGTCATAACTCTCAGCAGGACCACCAGTCTTCTCAACCACAACCTCCCTTTTCTTTTCATCAACCTTAAAGATGACACAAATTCACTACTCTTTCATCCCTTTAAATCCTTCATACATTTTCCATGTCTTTCAGAGGTCTCAGCCGGCCAAATGCTTCATCTGGCATGGGTGTTGAT',
     'CL100048465L2C001R015_402436',
    ),
    ('CTTAGATATGTCGGGGGTGAAACACGGATTATTAGTATAAGAAAGGACATAAGTTGGCCGGAGCTTATGCAGAAAATATTATCGATCTATAATGAAACTCGCGAGTTTCATTATAGATCGATAATATTTTCTGCATAAGCTCCGGCCAACTTATGTCCTTTCTTATACTAATAATCCGTGTTTCACCCCCGGCATATCTA',
     'CL100048465L2C001R001_5',
    ),
)

good = all(map(writeTestData, data))

cols = None
if good: curs.Commit()
curs = None

assert good
