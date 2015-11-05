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


workDir = tempfile.mkdtemp(suffix='.tmp', prefix='pore-load.', dir=tmpdir)
if outdir == None:
    outdir = workDir + '/pore-load.out'
    removeWorkDir = False

sys.stderr.write("Info: writing '{}'\n".format(outdir))
sys.stderr.write("Info: using '{}' for work space\n".format(workDir))

tbl = {
    'SEQUENCE': {
        'READ': {
            'expression': '(INSDC:dna:text)READ',
            'elem_bits': 8,
        },
        'QUALITY': {
            'expression': '(INSDC:quality:text:phred_33)QUALITY',
            'elem_bits': 8,
        },
        'SPOT_GROUP': {
            'elem_bits': 8,
        },
        'CHANNEL': {
            'elem_bits': 32,
        },
        'READ_NO': {
            'expression': 'READ_NUMBER',
            'elem_bits': 32,
        },
        'READ_START': {
            'expression': '(INSDC:coord:zero)READ_START',
            'elem_bits': 32,
        },
        'READ_LENGTH': {
            'expression': '(INSDC:coord:len)READ_LEN',
            'elem_bits': 32,
        },
        'READ_TYPE': {
            'expression': '(U8)READ_TYPE',
            'elem_bits': 8
        },
    },
    'CONSENSUS': {
        'READ': {
            'expression': '(INSDC:dna:text)READ',
            'elem_bits': 8,
        },
        'QUALITY': {
            'expression': '(INSDC:quality:text:phred_33)QUALITY',
            'elem_bits': 8,
        },
        'SPOT_GROUP': {
            'elem_bits': 8,
        },
        'CHANNEL': {
            'elem_bits': 32,
        },
        'READ_NO': {
            'expression': 'READ_NUMBER',
            'elem_bits': 32,
        },
        'HIGH_QUALITY': {
            'expression': 'HIGH_QUALITY',
            'elem_bits': 8,
        },
        'READ_START': {
            'expression': '(INSDC:coord:zero)READ_START',
            'elem_bits': 32,
            'default': array.array('I', [0])
        },
        'READ_LENGTH': {
            'expression': '(INSDC:coord:len)READ_LEN',
            'elem_bits': 32,
        },
        'READ_TYPE': {
            'expression': '(U8)READ_TYPE',
            'elem_bits': 8
        },
    }
}

gw = GeneralWriter.GeneralWriter(
      outdir
    , 'sra/nanopore.vschema'
    , 'NCBI:SRA:Nanopore:db'
    , 'pore-load.py'
    , '1.0.0'
    , tbl)


class FastQData:
    """ To hold FastQ data """
    
    # pylint: disable=too-many-instance-attributes
    # pylint: disable=too-many-arguments
    # what an idiotic warning
    
    def __init__(self, source
        , readLength
        , sequence
        , quality
        , sequence_2d
        , quality_2d
        , channel
        , readno
        , isHighQuality
        ):
        self.source = source
        self.readLength = readLength
        self.sequence = sequence
        self.quality = quality
        self.sequence_2d = sequence_2d
        self.quality_2d = quality_2d
        self.channel = channel
        self.readno = readno
        self.isHighQuality = isHighQuality
    
    def write(self):
        """ Write FASTQ data to the General Writer
            Writes 2D reads to CONSENSUS
            Writes other reads to SEQUENCE
        """
        if self.sequence_2d != None:
            tbl['CONSENSUS']['HIGH_QUALITY']['data'] = array.array('b', [ 1 if self.isHighQuality else 0 ])
            tbl['CONSENSUS']['READ_LENGTH' ]['data'] = array.array('I', [len(self.sequence_2d)])
            tbl['CONSENSUS']['READ_TYPE'   ]['data'] = array.array('B', [1])
            tbl['CONSENSUS']['READ'        ]['data'] = self.sequence_2d.encode('ascii')
            tbl['CONSENSUS']['QUALITY'     ]['data'] = self.quality_2d.encode('ascii')
        else:
            tbl['CONSENSUS']['HIGH_QUALITY']['data'] = array.array('b', [False])
            tbl['CONSENSUS']['READ_LENGTH' ]['data'] = array.array('I', [0])
            tbl['CONSENSUS']['READ_TYPE'   ]['data'] = array.array('B', [0])
            tbl['CONSENSUS']['READ'        ]['data'] = ''.encode('ascii')
            tbl['CONSENSUS']['QUALITY'     ]['data'] = ''.encode('ascii')

        tbl['SEQUENCE']['READ_START' ]['data'] = array.array('I', [ 0, self.readLength[0] ])
        tbl['SEQUENCE']['READ_LENGTH']['data'] = self.readLength
        tbl['SEQUENCE']['READ_TYPE'  ]['data'] = array.array('B', map((lambda length: 1 if length > 0 else 0), self.readLength))
        tbl['SEQUENCE']['READ'       ]['data'] = self.sequence.encode('ascii')
        tbl['SEQUENCE']['QUALITY'    ]['data'] = self.quality.encode('ascii')
        tbl['SEQUENCE']['CHANNEL'    ]['data'] = array.array('I', [self.channel])
        tbl['SEQUENCE']['READ_NO'    ]['data'] = array.array('I', [self.readno])
        spotGroup = os.path.basename(self.source)
        try:
            at = spotGroup.rindex("_ch{}_".format(self.channel))
            spotGroup = spotGroup[0:at]
        except:
            pass
        tbl['SEQUENCE']['SPOT_GROUP']['data'] = spotGroup

        tbl['CONSENSUS']['CHANNEL'   ]['data'] = tbl['SEQUENCE']['CHANNEL'   ]['data']
        tbl['CONSENSUS']['READ_NO'   ]['data'] = tbl['SEQUENCE']['READ_NO'   ]['data']
        tbl['CONSENSUS']['SPOT_GROUP']['data'] = tbl['SEQUENCE']['SPOT_GROUP']['data']

        gw.write(tbl['SEQUENCE' ])
        gw.write(tbl['CONSENSUS'])


    @classmethod
    def ReadFast5Data(cls, fname):
        """ Read the FASTQ data from the fast5 file

            This is segregated into its own function in order to catch errors
            that poretools itself doesn't catch, e.g. malformed HDF5
        """
        f5 = poretools.Fast5File(fname)
        try:
            channel = int(f5.get_channel_number())
            readno = int(f5.get_read_number())
            sequence_2d = None
            quality_2d = None
            hiQ = False

            if f5.has_2D():
                twoD = f5.get_fastqs("2D")[0]
                sequence_2d = twoD.seq
                quality_2d = twoD.qual
                hiQ = f5.is_high_quality()

            fwd = f5.get_fastqs("fwd")[0]
            rev = f5.get_fastqs("rev")[0]
            if fwd != None or rev != None:
                return FastQData(fname
                    , array.array('I', [ len(fwd.seq) if fwd != None else 0, len(rev.seq) if rev != None else 0 ])
                    , (fwd.seq  if fwd != None else '') + (rev.seq  if rev != None else '')
                    , (fwd.qual if fwd != None else '') + (rev.qual if rev != None else '')
                    , sequence_2d, quality_2d
                    , channel, readno, hiQ)
                        
            return None
        except:
            errMsg = "pore-tools reported an unspecific error while reading '{}'".format(os.path.basename(fname))
            gw.errorMessage(errMsg)
            sys.stderr.write(errMsg+"\n")
            if __debug__:
                traceback.print_exc()
            return None
        finally:
            f5.close()


def ProcessFast5(fname):
    """ Read the FASTQ data from the fast5 file

        Write it to the General Writer
    """
    data = FastQData.ReadFast5Data(fname)
    if data:
        data.write()
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


notRemoved = [] # because of errors

def ExtractAndProcess(f, source):
    """ Extract file to the working directory and process it
    """
    fname = os.path.join(workDir, source)
    with open(fname, 'wb') as output:
        output.write(f.read())

    keep = False
    if not isHDF5(fname):
        errMsg = "Warning: skipping '{}': not an HDF5 file.".format(source)
        gw.errorMessage(errMsg)
        sys.stderr.write(errMsg+"\n")
    elif not ProcessFast5(fname):
        keep = __debug__
    
    if keep:
        notRemoved.append(source)
    else:
        os.remove(fname)
    

processCounter = 0
processStart = time.clock()
nextReport = (processStart + showProgress) if showProgress > 0 else None

def ProcessTar(tar):
    """ Extract and process all fast5 files
    """
    global processCounter
    global nextReport

    for f in tar:
        if f.name.endswith('.fast5'):
            i = tar.extractfile(f)
            try:
                ExtractAndProcess(i, os.path.basename(f.name))
            finally:
                i.close()

            processCounter = processCounter + 1
            now = time.clock()
            if nextReport and now >= nextReport:
                nextReport = nextReport + showProgress
                sys.stderr.write("Progress: processed {} spots; {} per sec.\n".
                    format(processCounter, processCounter/(now - processStart)))
    
    elapsed = time.clock() - processStart
    sys.stderr.write("Progress: processed {} spots in {} secs, {} per sec.\n".
        format(processCounter, elapsed, processCounter/elapsed))


def which(f):
    PATH = os.environ["PATH"].split(":")
    for fullname in map((lambda p: os.path.join(p, f)), PATH):
        if os.path.exists(fullname):
            return fullname
    return None


def main():
    # check for gzcat or zcat

    gzcat = which('gzcat')
    if gzcat == None:
        gzcat = which('zcat') # gnu-ish
    if gzcat and not os.path.exists(gzcat):
        gzcat = None

    if len(files) == 0:
        sys.stderr.write("Info: processing stdin\n")
        with tarfile.open(mode='r|', fileobj=sys.stdin) as tar:
            ProcessTar(tar)

    for f in files:
        sys.stderr.write("Info: processing {}\n".format(f))
        if gzcat == None:
            with tarfile.open(f) as tar:
                ProcessTar(tar)
        else:
            p = subprocess.Popen([gzcat, f], stdout=subprocess.PIPE)
            with tarfile.open(mode='r|', fileobj=p.stdout) as tar:
                ProcessTar(tar)
            p.stdout.close()
            p.wait()


def cleanup():
    rmd = removeWorkDir
    if len(notRemoved) != 0:
        rmd = False
        sys.stderr.write("Info: these files caused errors and were not removed "
            "from the work space to aid in debugging:\n"
            )
        for f in notRemoved:
            sys.stderr.write("Info:\t{}\n".format(f))

    if rmd:
        os.rmdir(workDir)


main()
gw = None # close stream and flush
cleanup()
