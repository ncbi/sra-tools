#!python

# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

"""Options:
    tmpdir:     Path to work space; this should be fast storage where lots of
                small files can be quickly created and deleted, but it probably
                doesn't need to be large, /dev/shm or an SSD would be perfect.
                The work space will be a directory created here using mkdtemp.

    output:     The destination VDB database, default is 'pore-load.out' in the
                work space.

    progress:   The update rate (in seconds) for periodic progress messages,
                0 to disable, initially set to 0.
                Regardless, one progress message will be printed at the end of
                each input.

    error:      error response, can't be one of
                keep:   keep the offending file in the scratch space
                report: report the error but don't keep the file
                silent: don't report or keep the file

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
error = None
removeWorkDir = True
showProgress = 0
usage = "Usage: {} [ --tmpdir=<path> ] [ --output=<path> ] [ --progress=<number> ] [ --error=<keep|report|silent> [ --help ] [file ...]\n".format(sys.argv[0])

for arg in sys.argv[1:]:
    if arg[0:2] == '--':
        if arg[2:9] == 'tmpdir=':
            tmpdir = arg[9:]
        elif arg[2:9] == 'output=':
            outdir = arg[9:]
        elif arg[2:11] == 'progress=':
            showProgress = eval(arg[11:])
        elif arg[2:8] == 'error=':
            error = arg[8:]
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

if error == None or error == 'silent' or error == 'report' or error == 'keep':
    pass
else:
    sys.stderr.write(usage)
    exit(1)

saveSysPath = sys.path
sys.path.append(os.path.abspath("../../shared/python"))
import GeneralWriter
sys.path = saveSysPath

import time
import array
import tarfile
import h5py
import poretools


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


def safeHas2D(f5):
    try:
        if f5.has_2D():
            return True
    except:
        pass
    return False

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
            if f5.get_channel_number() == None:
                logMsg = "Info:\tchannel number missing. skipping file '{}'".format(os.path.basename(fname))
                gw.logMessage(logMsg)
                return None

            channel = int(f5.get_channel_number())
            try:
                readno = int(f5.get_read_number())
            except:
                readno = 0
            sequence_2d = None
            quality_2d = None
            hiQ = False

            if safeHas2D(f5):
                twoD = f5.get_fastqs("2D")[0]
                sequence_2d = twoD.seq
                quality_2d = twoD.qual
                hiQ = f5.is_high_quality()

            fwd = f5.get_fastqs("fwd")[0]
            rev = f5.get_fastqs("rev")[0]
            if fwd != None or rev != None or (sequence_2d != None and quality_2d != None):
                return FastQData(fname
                    , array.array('I', [ len(fwd.seq) if fwd != None else 0, len(rev.seq) if rev != None else 0 ])
                    , (fwd.seq  if fwd != None else '') + (rev.seq  if rev != None else '')
                    , (fwd.qual if fwd != None else '') + (rev.qual if rev != None else '')
                    , sequence_2d, quality_2d
                    , channel, readno, hiQ)
                        
            #errMsg = "pore-tools did not read any sequence data from '{}'".format(os.path.basename(fname))
            #gw.errorMessage(errMsg)
            #sys.stderr.write(errMsg+"\n")
            return None
        except:
            e = sys.exc_info()[0]
            errMsg = "{} while reading '{}'".format(e, os.path.basename(fname))
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
    if showProgress > 0:
        sys.stderr.write("Info: processing {}\n".format(fname))
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


def ExtractToTemporary(tar, file):
    extracted = tar.extractfile(file)
    if extracted:
        tmpname = os.path.join(workDir, os.path.basename(file.name))
        try:
            with open(tmpname, 'wb') as output:
                output.write(extracted.read())
            return tmpname
        except:
            pass
    return None


def ProcessTar(tar):
    """ Extract and process all fast5 files
    """
    processCounter = 0
    processStart = time.clock()
    nextReport = (processStart + showProgress) if showProgress else None
    results = dict()

    for f in tar:
        result = None
        name = f.name
        if f.isfile() and name.endswith('.fast5'):
            dir = os.path.dirname(name)
            if dir not in results:
                results[dir] = { 'good': [], 'fail': [] }
            tmpname = ExtractToTemporary(tar, f)
            if tmpname:
                try:
                    if isHDF5(tmpname):
                        result = False
                        if ProcessFast5(tmpname):
                            result = True
                finally:
                    os.remove(tmpname)

            if result == True:
                results[dir]['good'].append(name)
            else:
                results[dir]['fail'].append(name)

            now = time.clock()
            processCounter += 1
            if showProgress > 0 and now >= nextReport:
                nextReport = now + showProgress
                sys.stderr.write("Progress: processed {} files; {:0.1f} per sec.\n".
                                 format(processCounter, processCounter/(now - processStart)))

    elapsed = time.clock() - processStart
    sys.stderr.write("Progress: processed {} files in {} secs, {:0.1f} per sec.\n".
                     format(processCounter, elapsed, processCounter/elapsed))

    for f in results:
        resultGood = len(results[f]['good'])
        resultFail = len(results[f]['fail'])
        total = resultGood + resultFail
        sys.stderr.write("{}: pass: {} ({:4.1f}%)\tfail: {} ({:4.1f}%)\n".format(f, resultGood, 100.0 * resultGood / total, resultFail, 100.0 * resultFail / total))


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
    os.rmdir(workDir)


main()
gw = None # close stream and flush
cleanup()
