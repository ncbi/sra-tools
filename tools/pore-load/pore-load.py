#!python
"""Options:
    tmpdir:     Path to work space; this should be fast storage where lots of
                small files can be quickly created and deleted, but it probably
                doesn't need to be large, /dev/shm or an SSD would be perfect.
                The work space will be a directory created here using mkdtemp.

    output:     The destination VDB database, default is 'pore-load.out' in the
                work space.

    progress:   The update rate (in seconds) for periodic progress messages,
                0 to disable, initially set to 10 seconds if __debug__ else 0.
                Regardless, one progress message will be printed at the end of
                each input.

    help:       displays this message
    
Note:
    Input is expected to be .tar.gz files. Decompression is done by a subprocess
    if zcat or gzcat can be found in the path, otherwise it's done in-process,
    which is slower.
    
    Input is taken from stdin if no files are given. If needed, it will be
    decompressed in-process.
    
    The args parsing in this script is terribly primitive. You must type exactly
    as shown, with the '=' and no whitespace.
"""

import sys

# Environment globals
gzcat = None    # what executable to use for out-of-process decompression
files = []      # what files to operate on
tmpdir = "/tmp" # default workspace
outdir = None
removeWorkDir = True
showProgress = 10 if __debug__ else 0
usage = "Usage: {} [ --tmpdir=<path> ] [ --output=<path> ] [ --progress=<number> ] [ --help ] [file ...]\n".format(sys.argv[0])

for arg in sys.argv[1:]:
    if arg[0:2] == '--':
        if arg[2:9] == 'tmpdir=':
            tmpdir = arg[9:]
        elif arg[2:9] == 'output=':
            outdir = arg[9:]
        elif arg[2:11] == 'progress=':
            showProgress = eval(arg[11:])
        elif arg[2:] == 'help':
            sys.stderr.write(usage)
            sys.stderr.write(__doc__)
            exit(0)
        else:
            sys.stderr.write(usage)
            exit(1)
    else:
        files.append(arg)

import logging
logging.basicConfig(level=logging.DEBUG)

import os, subprocess, tempfile

if __debug__:
    import traceback

import time
import array
import tarfile
import h5py
import poretools
import GeneralWriter


def which(file):
    for path in os.environ["PATH"].split(":"):
        if os.path.exists(path + "/" + file):
                return os.path.join(path, file)
    return None

workDir = tempfile.mkdtemp(suffix = '.tmp', prefix='pore-load.', dir=tmpdir)
if outdir == None:
   outdir = workDir + '/pore-load.out'
   removeWorkDir = False

sys.stderr.write("Info: writing '{}'\n".format(outdir))
sys.stderr.write("Info: using '{}' for work space\n".format(workDir))

# check for gzcat or zcat

if gzcat == None:
    gzcat = which('gzcat')

if gzcat == None:
    gzcat = which('zcat') # gnu-ish

if gzcat and not os.path.exists(gzcat):
    gzcat = None


tbl = {
    'SEQUENCE': {
        'READ': {
            'expression': '(INSDC:dna:text)READ',
            'elem_bits': 8,
        },
        'QUALITY': {
            'expression': '(INSDC:quality:phred)QUALITY',
            'elem_bits': 8,
        },
        'SPOT_GROUP': {
            'elem_bits': 8,
        },
        'CHANNEL': {
            'elem_bits': 32,
        },
        'READ_NO': {
            'elem_bits': 32,
        },
        'READ_START': {
            'expression': '(INSDC:coord:zero)READ_START',
            'elem_bits': 32,
        },
        'READ_LENGTH': {
            'expression': '(INSDC:coord:len)READ_LENGTH',
            'elem_bits': 32,
        },
    },
    'CONSENSUS': {
        'READ': {
            'expression': '(INSDC:dna:text)READ',
            'elem_bits': 8,
        },
        'QUALITY': {
            'expression': '(INSDC:quality:phred)QUALITY',
            'elem_bits': 8,
        },
        'SPOT_GROUP': {
            'elem_bits': 8,
        },
        'CHANNEL': {
            'elem_bits': 32,
        },
        'READ_NO': {
            'elem_bits': 32,
        },
        'HIGH_QUALITY': {
            'expression': 'HIGH_QUALITY',
            'elem_bits': 8,
        },
    }
}

gw = GeneralWriter.GeneralWriter(sys.stdout
    , outdir
    , 'sra/nanopore.vschema'
    , 'NCBI:SRA:Nanopore:db'
    , tbl)

notRemoved = [] # because of errors
processCounter = 0
processStart = None

class FastQData:
    def __init__(self, source):
        self.is2D = False
        self.source = source
    
    def write(self):
        """ Write FASTQ data to the General Writer
            Writes 2D reads to CONSENSUS
            Writes other reads to SEQUENCE
        """
        dst = tbl['SEQUENCE']
        if self.is2D:
            dst = tbl['CONSENSUS']
            dst['HIGH_QUALITY']['data'] = array.array('b',
                [ 1 if self.isHighQuality else 0 ])
        else:
            dst['READ_START' ]['data'] = array.array('I',
                [ 0, self.readLength[0] ])
            dst['READ_LENGTH']['data'] = self.readLength
        
        dst['READ'   ]['data'] = self.sequence.encode('ascii')
        dst['QUALITY']['data'] = self.quality.encode('ascii')
        dst['CHANNEL']['data'] = array.array('I', [ self.channel ])
        dst['READ_NO']['data'] = array.array('I', [ self.readno ])
        spotGroup = os.path.basename(self.source)
        try:
            at = spotGroup.rindex("_ch{}_".format(self.channel))
            dst['SPOT_GROUP']['data'] = spotGroup[0:at]
        except:
            dst['SPOT_GROUP']['data'] = spotGroup
    
        gw.write(dst)


def ReadFast5Data(fname):
    """ Read the FASTQ data from the fast5 file

        This is segregated into its own function in order to catch errors
        that poretools itself doesn't catch, e.g. malformed HDF5
    """
    f5 = poretools.Fast5File(fname)
    try:
        rslt = FastQData(fname)
        rslt.channel = f5.get_channel_number()
        rslt.readno = f5.get_read_number()
    
        if f5.has_2D():
            twoD = f5.get_fastqs("2D")[0]
            rslt.is2D = True
            rslt.sequence = twoD.seq
            rslt.quality = twoD.qual
            rslt.isHighQuality = f5.is_high_quality()
        else:
            fwd = f5.get_fastqs("fwd")[0]
            rev = f5.get_fastqs("rev")[0]
            rslt.readLength = array.array('I', [ len(fwd.seq) if fwd != None else 0, len(rev.seq) if rev != None else 0 ])
            rslt.sequence = (fwd.seq  if fwd != None else '') + (rev.seq  if rev != None else '')
            rslt.quality  = (fwd.qual if fwd != None else '') + (rev.qual if rev != None else '')

        if rslt.sequence != '':
            return rslt
        else:
            return None
    except:
        sys.stderr.write("Error: reading '{}'\n".format(os.path.basename(fname)))
        if __debug__:
            traceback.print_exc()
        return None
    finally:
        f5.close()


def ProcessFast5(fname):
    """ Read the FASTQ data from the fast5 file

        Write it to the General Writer
    """
    data = ReadFast5Data(fname)
    if data:
        data.write()
        data = None
        return True
    else:
        return False


def isHDF5(fname):
    """ Try to open it using h5py
        If it works, assume it's an HDF5 file

        This is needed because sometimes the .fast5 files
        are actually HTML containing an error message
    """
    try:
        with h5py.File(fname, 'r'):
            return True
    except IOError:
        return False

nextReport = (lastReport + showProgress) if showProgress > 0 else None

def ExtractAndProcess(file, source):
    """ Extract file to the working directory and process it
    """
    fname = os.path.join(workDir, source)
    with open(fname, 'wb') as output:
        output.write(file.read())

    keep = False
    if not isHDF5(fname):
        sys.stderr.write("Warning: skipping '{}': not an HDF5 file.\n".
            format(source))
    elif not ProcessFast5(fname):
        keep = __debug__
    
    if keep:
        notRemoved.append(source)
    else:
        os.remove(fname)
    
    global processCounter
    global nextReport
    
    processCounter = processCounter + 1
    if nextReport and time.clock() >= nextReport:
        nextReport = nextReport + showProgress
        sys.stderr.write("Progress: processed {} spots; {} per sec.\n".
            format(processCounter, processCounter/(time.clock() - processStart)))


def ProcessTar(tar):
    """ Extract and process all fast5 files
    """
    global processStart
    processStart = time.clock()
    
    for file in tar:
        if file.name.endswith('.fast5'):
            input = tar.extractfile(file)
            try:
                ExtractAndProcess(input, os.path.basename(file.name))
            finally:
                input.close()
    
    elapsed = time.clock() - processStart
    sys.stderr.write("Progress: processed {} spots in {} secs, {} per sec.\n".
        format(processCounter, elapsed, processCounter/elapsed))


if len(files) == 0:
    sys.stderr.write("Info: processing stdin\n")
    with tarfile.open(mode='r|', fileobj=sys.stdin) as tar:
        ProcessTar(tar)

for file in files:
    sys.stderr.write("Info: processing {}\n".format(file))
    if gzcat == None:
        with tarfile.open(file) as tar:
            ProcessTar(tar)
    else:
        p = subprocess.Popen([gzcat, file], stdout=subprocess.PIPE)
        with tarfile.open(mode='r|', fileobj=p.stdout) as tar:
            ProcessTar(tar)
        p.stdout.close()
        p.wait()

gw = None # close stream and flush

if len(notRemoved) != 0:
    removeWorkDir = False
    sys.stderr.write("Info: these files caused errors and were not removed "
        "from the work space to aid in debugging:\n"
        )
    for file in notRemoved:
        sys.stderr.write("Info:\t{}\n".format(file))

if removeWorkDir:
    os.rmdir(workDir)
