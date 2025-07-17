#!/opt/python-2.7env/bin/python
####/opt/python-3.4/bin/python
#===========================================================================
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
#===========================================================================
#
#

"""
fastq-load.py --output=<archive path> <other options> <fastq files> | general-loader

Options:

    output:         Output archive path

    offset:         For interpretation of ascii quality (offset=33 or offset=64) or
                    indicating the presence of numerical quality (offset=0).
                    (accepts PHRED_33 and PHRED_64, too)

    quality:        Same as offset (different from latf due to requirement for '=')

    readLens:       For splitting fixed length sequence/quality from a single fastq
                    Number of values must = # of reads (comma-separated). Must be
                    consistent with number of read types specified

    readTypes:      For specifying read types using B (Biological) or T (Technical)
                    Use sequence like TBBT - no commas. Defaults to BB. Must be
                    consistent with number of values specified for read lengths.
                    If you want the read sequence to be used as the spot group or
                    part of the spot group use G (Group). Multiple reads incorporated
                    into the spot group will be concatenated with an '_' separator.

    spotGroup:      Indicates spot group to associate with all spots that are loaded
                    Overrides barcodes on deflines.

    orphanReads:    File or files contain orphan reads or fragments mixed with non-orphan
                    reads. If all files are either completely fragments or completely
                    paired (e.g. split-3 fastq-dump output) then this option is
                    unnecessary. However, for pre-split 454 this option would probably be
                    necessary.

    logOdds:        Input fastq has log odds quality (set prior to 'offset' or 'quality'
                    option)
    
    discardNames:   For when names repeat, go on position only. Specify eight line fastq
                    if that is the case. Does not work with orphanReads. Does not work
                    for split seq/qual files.
    
    useAndDiscardNames:   Too many names to store but still useful for determination of
                    read pairs. So names are used and then discarded.
                    
    ignoreNames:    Determination of pairs via names will not work but first read name
                    retained.
    
    read1PairFiles: Filenames containing read1 seqs ordered to correspond with read2 pair
                    files. Required with --orphanReads option. Files paths must still be
                    provided on the command line in addition to this option. Must be
                    specified in conjunction with read2PairFiles option. Comma-separated.
                    Also useful if ignoring/discarding names.

    read2PairFiles: Filenames containing read2 seqs. Required for --orphanReads option.
                    Include a filename from the read1 files if read2 is in the same file
                    using corresponding positions. Files paths must still be provided on
                    the command line in addition to this option. Comma-separated. Must
                    be specified in conjunction with read1PairFiles option.
                    Also useful if ignoring/discarding names.

    read1QualFiles: Filenames containing read1 quals ordered to correspond with read2 pair
                    files. Files paths must still be provided on the command line in addition
                    to this option. Must be specified in conjunction with read2QualFiles option.
                    Comma-separated. (Provide only if ignoring or discarding names)

    read2QualFiles: Filenames containing read2 quals ordered to correspond with read1 pair
                    files. Files paths must still be provided on the command line in addition
                    to this option. Must be specified in conjunction with read1QualFiles option.
                    Comma-separated. (Provide only if ignoring or discarding names)

    platform:       454, Pacbio, Illumina, ABI, etc.

    readLabels:     Rev, Fwd, Barcode, etc., comma-separated (no whitespaces)

    mixedDeflines:  Indicates mixed defline types exist in one of the fastq files.
                    Results in slower processing of deflines.

    ignLeadChars:   Set # of leading defline characters to ignore for pairing

    discardBarcodes For cases where too many barcodes exist (>30000)
    
    schema:         Set vdb schema to use during load

    z|xml-log:      XML version of stderr output

    h|help:         Displays this message

    V|version:      Displays version of fastq-load.py

You may have to provide read1 and read2 lists if pairing of files is necessary and
orphan reads exist. If both reads exist in the same file, put the same filename 
in both lists.

Cannot handle > 2 files that contain reads for a spot
(i.e. cannot handle three reads in separate files)

Log odds will be recognized if the quality offset is determined and not provided.
If offset is provided and data is log odds, then set 'logodds' option prior to
setting 'offset' or 'quality' options.

"""

import sys
import array
from enum import Enum
import GeneralWriter
import logging
import os
import re
import copy
import gzip
import bz2
import datetime
import string
import time

############################################################
# Environment globals
############################################################

filePaths = {}    # map from filename to file path
fileHandles = {}  # map from filename to file handle
fileSkip = {}     # hash of filenames that have been paired or found to be quality files
fileTypes = {}    # map from filename to file type
                  # normal, singleLine, eightLine, multiLine, multiLineEightLine,
                  # seqQual, multiLineSeqQual, fasta, multiLineFasta, nanopore?, solid?
fileReadPairs = {}   # map from filename to file that contains read pairs
filePore2D = {}      # map from nanopore template file to nanopore 2D file
fileQualPairs = {}   # map from filename to file containing qualities
fileLabels = {}      # map from filename to label for file

############################################################
# Output usage statement along with message if specified
############################################################

def usage ( message, state ):
    usageMessage =  """
Usage: fastq-load.py\n
          [ --output=<archive>        (optional) ]
          [ --offset=<number>         (optional) ]
          [ --readLens=<number(s)>    (optional) ]
          [ --readTypes=<B|T(s)>      (optional) ]
          [ --spotGroup=<string>      (optional) ]
          [ --orphanReads             (optional) ]
          [ --logOdds                 (optional) ]
          [ --ignoreNames             (optional) ]
          [ --discardNames            (optional) ]
          [ --useAndDiscardNames      (optional) ]
          [ --read1PairFiles=<string> (optional) ]
          [ --read2PairFiles=<string> (optional) ]
          [ --read1QualFiles=<string> (optional) ]
          [ --read2QualFiles=<string> (optional) ]
          [ --platform=<string>       (optional) ]
          [ --readLabels=<labels>     (optional) ]
          [ --maxErrorCount=<count>   (optional) ]
          [ --mixedDeflines           (optional) ]
          [ --ignLeadChars=<count>    (optional) ]
          [ --discardBarcodes         (optional) ]
          [ --schema=<string>         (optional) ]
          [ -z|--xml-log=<string>     (optional) ]
          [ -h|--help                 (optional) ]
          [ -V|--version              (optional) ]
          [ fastq-file(s)                        ]

"""
    if message:
        sys.stderr.write( "\n{}\n".format(message) )
    sys.stderr.write(usageMessage)
    sys.stderr.write(__doc__)
    exit(state)

############################################################
# Defline StatusWriter class
############################################################

class StatusWriter:
    """ Outputs status to stderr and optionally to an xml log file """
    def __init__(self, vers):
        self.vers = vers
        self.xmlLogHandle = None
        self.pid = os.getpid()
        
    ############################################################
    # Open xml log file if provided
    ############################################################
    
    def setXmlLog ( self, xmlLogFile ):
        xmlLogFile = xmlLogFile.strip()
        try:
            self.xmlLogHandle = open(xmlLogFile, 'w')
            self.xmlLogHandle.write("<Log>\n")
        except OSError:
            sys.exit( "\nFailed to open {} for writing\n\n".format(xmlLogFile) )

    ############################################################
    # Close Xml Log file
    ############################################################
    def closeXmlLog ( self ):
        self.xmlLogHandle.write("</Log>\n")
        self.xmlLogHandle.close()
        
    ############################################################
    # Output status message
    ############################################################
    def outputInfo ( self, message ):
        dateTime = self.getTime()
        if self.xmlLogHandle:
            self.xmlLogHandle.write('<info app="fastq-load.py" message="{}" pid="{}" timestamp="{}" version="{}"/>\n'
                                    .format(self.escape(message),self.pid,dateTime,self.vers))
            self.xmlLogHandle.flush()
        sys.stderr.write("{} fastq-load.py.{} info: {}\n".format(dateTime,self.vers,message) )
        sys.stderr.flush()

    ############################################################
    # Output status message
    ############################################################
    def outputWarning ( self, message ):
        dateTime = self.getTime()
        if self.xmlLogHandle:
            self.xmlLogHandle.write('<warning app="fastq-load.py" message="{}" pid="{}" timestamp="{}" version="{}"/>\n'
                                    .format(self.escape(message),self.pid,dateTime,self.vers))
            self.xmlLogHandle.flush()
        sys.stderr.write("{} fastq-load.py.{} warn: {}\n".format(dateTime,self.vers,message) )
        sys.stderr.flush()

    ############################################################
    # Output status message and exit
    ############################################################
    def outputErrorAndExit (self, message):
        dateTime = self.getTime()
        if self.xmlLogHandle:
            self.xmlLogHandle.write('<error app="fastq-load.py" message="{}" pid="{}" timestamp="{}" version="{}"/>\n'
                                    .format(self.escape(message),self.pid,dateTime,self.vers))
            self.xmlLogHandle.flush()
            self.closeXmlLog()
        sys.exit( "\n{} fastq-load.py.{} Error: {}\n\n".format(dateTime,self.vers,message) )

    ############################################################
    # Escape message (wrote my own instead of importing sax escape)
    ############################################################
    @staticmethod
    def escape(message):
        message = message.replace('&', '&amp;')
        message = message.replace('"', '&quot;')
        message = message.replace("'", '&apos;')
        message = message.replace('<', '&lt;')
        message = message.replace('>', '&gt;')
        return message

    ############################################################
    # Get formatted date time
    ############################################################
    @staticmethod
    def getTime():
        now = datetime.datetime.utcnow()
        now = now.replace(microsecond=0)
        return now.isoformat()

############################################################
# Define Platform and convert platform to SRA enum
############################################################

class Platform(Enum):
    """ Enumeration class for SRA platforms """
    SRA_PLATFORM_UNDEFINED         = 0
    SRA_PLATFORM_454               = 1
    SRA_PLATFORM_ILLUMINA          = 2
    SRA_PLATFORM_ABSOLID           = 3
    SRA_PLATFORM_COMPLETE_GENOMICS = 4
    SRA_PLATFORM_HELICOS           = 5
    SRA_PLATFORM_PACBIO_SMRT       = 6
    SRA_PLATFORM_ION_TORRENT       = 7
    SRA_PLATFORM_CAPILLARY         = 8
    SRA_PLATFORM_OXFORD_NANOPORE   = 9

    @classmethod
    def convertPlatformString ( cls, platformString ):
        platformString = platformString.upper()
        if ( platformString == "454" or
             platformString == "LS454" ):
            return cls.SRA_PLATFORM_454
        elif platformString == "ILLUMINA":
            return cls.SRA_PLATFORM_ILLUMINA
        elif ( platformString == "ABI" or
               platformString == "SOLID" or
               platformString == "ABSOLID" or
               platformString == "ABISOLID" ):
            return cls.SRA_PLATFORM_ABSOLID
        elif ( platformString == "PACBIO" or
               platformString == "PACBIO_SMRT"):
            return cls.SRA_PLATFORM_PACBIO_SMRT
        elif ( platformString == "CAPILLARY" or
               platformString == "SANGER" ):
            return cls.SRA_PLATFORM_CAPILLARY
        elif platformString == "NANOPORE":
            return cls.SRA_PLATFORM_OXFORD_NANOPORE
        elif platformString == "HELICOS":
            return cls.SRA_PLATFORM_HELICOS
        elif platformString == "ION_TORRENT":
            return cls.SRA_PLATFORM_ION_TORRENT
        elif ( platformString == "UNDEFINED" or
               platformString == "MIXED" ):
            return cls.SRA_PLATFORM_UNDEFINED
        else:
            return None

############################################################
# Defline class (not an enum)
############################################################

class Defline:
    """ Retains information parsed from fastq defline """

    ILLUMINA_NEW                = 1
    ILLUMINA_OLD                = 2
    PACBIO                      = 3
    ABSOLID                     = 4
    QIIME_ILLUMINA_NEW          = 5
    QIIME_ILLUMINA_NEW_BC       = 6
    ILLUMINA_NEW_DOUBLE         = 7
    LS454                       = 8
    UNDEFINED                   = 9
    ILLUMINA_OLD_BC_RN          = 10
    ILLUMINA_NEW_WITH_JUNK      = 11
    QIIME_ILLUMINA_OLD          = 12
    QIIME_ILLUMINA_OLD_BC       = 13
    QIIME_454                   = 14
    QIIME_454_BC                = 15
    QIIME_ILLUMINA_NEW_DBL      = 16
    QIIME_ILLUMINA_NEW_DBL_BC   = 17
    QIIME_GENERIC               = 18
    READID_BARCODE              = 19
    ILLUMINA_NEW_OLD            = 20
    NANOPORE                    = 21
    HELICOS                     = 22
    ION_TORRENT                 = 23
    SANGER_NEWBLER              = 24
    
    def __init__(self, deflineString):
        self.deflineType = None
        self.deflineStringOrig = ''
        self.deflineString = ''
        self.saveDeflineType = True
        self.name = ''
        self.platform = "NotSet"
        self.isValid = False
        self.readNum = ""
        self.filterRead = 0
        self.spotGroup = ''
        self.prefix = ''
        self.lane = ''
        self.tile = ''
        self.x = ''
        self.y = ''
        self.numDiscards = 0
        self.foundRE = None
        self.dateAndHash454 = ''
        self.region454 = ''
        self.xy454 = ''
        self.qiimeName = ''
        self.filename = ''
        self.poreRead = ''
        self.poreFile = ''
        self.channel = ''
        self.readNo = 0
        self.flowcell = ''
        self.field = ''
        self.camera = ''
        self.position = ''
        self.runId = ''
        self.row = ''
        self.column = ''
        self.dir = ''
        self.panel = ''
        self.tagType = ''
        self.suffix = ''
        self.abiTitle = ''
        self.statusWriter = None
        self.ignLeadCharsNum = None
        self.ignoredLeadChars = None

        self.illuminaNew = re.compile("[@>]([!-~]+?)(:|_)(\d+)(:|_)(\d+)(:|_)(\d+)(:|_)(\d+)(\s+|[_\|])([12345]|):([NY]):(\d+|O):?([!-~]*?)(\s+|$)")
        self.illuminaNewNoPrefix = re.compile("[@>]([!-~]*?)(:?)(\d+)(:|_)(\d+)(:|_)(\d+)(:|_)(\d+)(\s+|_)([12345]|):([NY]):(\d+|O):?([!-~]*?)(\s+|$)")
        self.illuminaNewWithJunk = re.compile("[@>]([!-~]+?)(:|_)(\d+)(:|_)(\d+)(:|_)(\d+)(:|_)(\d+)([!-~]+?\s*)([12345]|):([NY]):(\d+|O):?([!-~]*?)(\s+|$)")
        self.illuminaNewWithPeriods = re.compile("[@>]([!-~]+?)(\.)(\d+)(\.)(\d+)(\.)(\d+)(\.)(\d+)(\s+|_)([12345]|)\.([NY])\.(\d+|O)\.?([!-~]*?)(\s+|$)")
        self.illuminaNewWithUnderscores = re.compile("[@>]([!-~]+?)(_)(\d+)(_)(\d+)(_)(\d+)(_)(\d+)(\s+|_)([12345]|)_([NY])_(\d+|O)_?([!-~]*?)(\s+|$)")
        self.illuminaNewSuffix = re.compile("(#[!-~]*?|)(/[12345]|\\\\[12345])?([!-~]*?)(#[!-~]*?|)(/[12345]|\\\\[12345])?([:_\|]?)(\s+|$)")

        self.illuminaOld = None
        self.illuminaOldColon = re.compile("[@>]?([!-~]+?)(:)(\d+)(:)(\d+)(:)(-?\d+)(:)(-?\d+)_?[012]?(#[!-~]*?|)\s?(/[12345]|\\\\[12345])?(\s+|$)")
        self.illuminaOldUnderscore = re.compile("[@>]?([!-~]+?)(_)(\d+)(_)(\d+)(_)(-?\d+)(_)(-?\d+)(#[!-~]*?|)\s?(/[12345]|\\\\[12345])?(\s+|$)")
        self.illuminaOldNoPrefix = re.compile("[@>]?([!-~]*?)(:?)(\d+)(:)(\d+)(:)(-?\d+)(:)(-?\d+)(#[!-~]*?|)\s?(/[12345]|\\\\[12345])?(\s+|$)")
        self.illuminaOldWithJunk = re.compile("[@>]?([!-~]+?)(:)(\d+)(:)(\d+)(:)(-?\d+)(:)(-?\d+)(#[!-~]+)(/[12345])[!-~]+(\s+|$)")
        self.illuminaOldWithJunk2 = re.compile("[@>]?([!-~]+?)(:)(\d+)(:)(\d+)(:)(-?\d+)(:)(-?\d+[!-~]+?)(#[!-~]*|)\s?(/[12345]|\\\\[12345])?(\s+|$)")
        self.illuminaOldSuffix = re.compile("(-?\d+)([!-~]*)") # Must have '*' and not '+'. Otherwise, name for pairing is truncated by one character.

        self.illuminaOldBcRn = None
        self.illuminaOldBcRnOnly = re.compile("[@>]([!-~]+?)(#[!-~]+?)(/[12345]|\\\\[12345])(\s+|$)")
        self.illuminaOldBcOnly = re.compile("[@>]([!-~]+?)(#[!-~]+)(\s+|$)(.?)")
        self.illuminaOldRnOnly = re.compile("[@>]([!-~]+?)(/[12345]|\\\\[12345])(\s+|$)(.?)")

        self.qiimeBc = re.compile("[@>]([!-~]*).*?\s+orig_bc=[!-~]+\s+new_bc=([!-~]+)\s+bc_diffs=[01]")
        self.qiimeIlluminaNew = re.compile("[@>]([!-~]*)\s+([!-~]*?)(:|_)(\d+)(:|_)(\d+)(:|_)(\d+)(:|_)(\d+)(\s+|_|:)([12345]):([NY]):(\d+|O):?([!-~]*?)(\s+|$)")
        self.qiimeIlluminaNewPeriods = re.compile("[@>]([!-~]*)\s+([!-~]*?)(\.)(\d+)(\.)(\d+)(\.)(\d+)(\.)(\d+)(\s+|_|:)([12345])\.([NY])\.(\d+|O)\.?([!-~]*?)(\s+|$)")
        self.qiimeIlluminaNewUnderscores = re.compile("[@>]([!-~]*)\s+([!-~]*?)(_)(\d+)(_)(\d+)(_)(\d+)(_)(\d+)(\s+|_|:)([12345])_([NY])_(\d+|O)_?([!-~]*?)(\s+|$)")
        
        self.qiimeIlluminaOld = re.compile("[@>]([!-~]*)\s+([!-~]+?)(:)(\d+)(:)(\d+)(:)(-?\d+)(:)(-?\d+)(#[!-~]*?|)\s?(/[12345]|\\\\[12345])?(\s+|$)")

        self.ls454 = re.compile("[@>]([!-~]+_|)([A-Z0-9]{7})(\d{2})([A-Z0-9]{5})(/[12345])?(\s+|$)")
        self.qiime454 = re.compile("[@>]([!-~]*)\s+([A-Z0-9]{7})(\d{2})([A-Z0-9]{5})(/[12345])?(\s+|$)")

        self.pacbio = re.compile("[@>](m\d{6}_\d{6}_[!-~]+?_c\d{33}_s\d+_[pX]\d/\d+/?\d*_?\d*)(\s+|$)")

        self.nanopore = re.compile("[@>]+?(channel_)(\d+)(_read_)(\d+)([!-~]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)?(:[!-~]+?_ch\d+_file\d+_strand.fast5)?(\s+|$)")
        self.nanopore2 = re.compile("[@>]([!-~]*?ch)(\d+)(_file)(\d+)([!-~]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)(:[!-~]+?_ch\d+_file\d+_strand.fast5)?(\s+|$)")
        self.nanopore3 = re.compile("[@>]([!-~]+?_Basecall_2D[_0]*?)(_twodirections|_2d|-2D|_template|-1D|_complement|-complement|\.1C|\.1T|\.2D)[: ]([!-~]+?)[: ]([!-~]+?_ch)(\d+)(_read|_file)(\d+)(_strand\d*.fast5)(\s+|$)")

        self.helicos = re.compile("[@>](VHE-\d+)-(\d+)-(\d+)-(\d)-(\d+)(\s+|$)")

        self.ionTorrent = re.compile("[@>]([A-Z0-9]{5})(:)(\d{1,5})(:)(\d{1,5})(/[12345]|\\\\[12345]|[LR])?(\s+|$)")

        self.abSolid = re.compile("[@>]([!-~]*?)(\d+)_(\d+)_(\d+)_(F3|R3|F5-BC|BC|F5-P2|F5-RNA|F5-DNA)([!-~]*?)(\s+|$)")

        self.sangerNewbler = re.compile("[@>]([!-~]+?)\s+template=([!-~]+)\s+dir=([!-~]+)(\s+|$)")

        self.readIdBarcode = re.compile("[@>]([!-~]+)\s+read_id=([!-~]*?::|)([!-~]+)\s+barcode=([!-~]+).*(\s+|$)")

        self.undefined = re.compile("[@>]([!-~]+)(\s+|$)")

        if deflineString:
            self.parseDeflineString(deflineString)

    ############################################################
    # Determine if deflines from two files represent paired reads
    # or paired seq/qual files. If testing for paired seq/qual,
    # then checkReadNum is True. Note that name could be '0'.
    ############################################################

    @classmethod
    def isPairedDeflines( cls, defline1, defline2, sameReadNum ):

        # This case is used to determine if a seq files goes with a
        # qual file. In this case the read numbers must be the same

        if ( sameReadNum and
             defline1.name and
             defline2.name and
             ( defline1.name == defline2.name or
               ( defline1.abiTitle and defline1.name == defline1.abiTitle + "_" + defline2.name ) or
               ( defline2.abiTitle and defline2.name == defline2.abiTitle + "_" + defline1.name ) ) and
             defline1.readNum == defline2.readNum ):

            if ( defline1.tagType and
                 defline2.tagType and
                 not defline1.tagType == defline2.tagType ):
                return False
            else:
                return True

        # This case is used to determine if two files should be
        # paired together (i.e. different read numbers)

        elif ( not sameReadNum and
               defline1.name and
               defline2.name and
               defline1.name == defline2.name ):

            # Determine which defline is associated with first read

            if ( defline1.readNum and
                 defline2.readNum ):

                # Return 1 if first read associated with defline1

                if defline1.readNum < defline2.readNum:
                    return 1

                # Return 2 if first read associated with defline2

                else:
                    return 2

            # Check for nanopore template (1D) reads

            elif ( defline1.poreRead and
                   defline1.poreRead == "template" ):
                return 1

            elif ( defline2.poreRead and
                   defline2.poreRead == "template" ):
                return 2

            # Not pairing complement and 2D nanopore reads

            elif ( defline1.poreRead or
                   defline2.poreRead ):
                return False

            # Check for absolid reads

            elif ( defline1.tagType and
                   defline2.tagType ):

                if ( defline1.tagType == "F3" and
                     defline2.tagType != "F3" ):
                    return 1

                elif ( defline2.tagType == "F3" and
                       defline1.tagType != "F3" ):
                    return 2

                else:
                    return 1

            # Compare defline strings to determine order if not read numbers

            elif defline1.deflineString < defline2.deflineString:
                return 1

            elif defline1.deflineString > defline2.deflineString:
                return 2

            # Defaulting to first defline associated with first read
            # if indeterminate via readNums and defline order

            else:
                return 1
        else:
            return False

    ############################################################
    # Reset object variables
    ############################################################
    def reset(self):
        self.deflineStringOrig = ''
        self.deflineString = ''
        self.name = ''
        self.isValid = True
        self.readNum = ''
        self.filterRead = 0
        self.spotGroup = ''
        self.prefix = ''
        self.lane = ''
        self.tile = ''
        self.x = ''
        self.y = ''
        self.dateAndHash454 = ''
        self.region454 = ''
        self.xy454 = ''
        self.qiimeName = ''
        self.poreRead = ''
        self.poreFile = ''
        self.channel = ''
        self.readNo = 0
        self.runId = ''
        self.row = ''
        self.column = ''
        self.camera = ''
        self.field = ''
        self.position = ''
        self.flowcell = ''
        self.panel = ''
        self.tagType = ''
        self.suffix = ''
        self.dir = ''
        self.ignoredLeadChars = None

    ############################################################
    # Parse deflines based on cases we have seen in the past
    ############################################################
    
    def parseDeflineString(self,deflineString):

        self.reset()
        self.deflineStringOrig = deflineString
        self.deflineString = self.deflineStringOrig.strip()

        # End of file produces empty defline string

        if not self.deflineStringOrig:
            pass

        ############################################################
        # helicos
        #
        # @VHE-242383071011-15-1-0-2 (YYY034449)
        ############################################################

        elif ( self.deflineType == self.HELICOS or
               ( self.deflineType is None and
                 self.helicos.match(self.deflineString) ) ):
            
            m = self.helicos.match(self.deflineString)
            
            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.flowcell, self.channel, self.field, self.camera, self.position, endSep) = m.groups()

            # Set defline name

            self.name = self.flowcell + "-" + self.channel + "-" + self.field + "-" + self.camera + "-" + self.position

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.HELICOS
                self.platform = "HELICOS"

        ############################################################
        # ab solid
        #
        # >3_189_730_F3 (XXX001662)
        # >3_189_730_R3 (XXX001662)
        # >461_28_1048_F3 (XXX001354)
        # >349_1793_467_F3 (XXX015374)
        # >1_98_123_BC (XXX1001434)
        # >427_21_101_F3 (XXX112693)
        # >427_17_22_F5-P2 (XXX112693)
        # >2_35_407_F3 (XXX3159530)
        # >2_35_407_F5-BC (XXX3159530)
        # @1_43_495_F3 (XXX645822)
        # @1_43_495_F5-BC (XXX645822)
        # >47_15_207_F5-RNA (YYY005001)
        # >47_15_207_F5-DNA (YYY005002)
        # >1_01_1_23_192_F3 (YYY3222665)
        # >1_23_192_F3_1_01 (ZZZ3222665)
        # >427_27_224_F3 1:0003213231 (ZZZ005000)
        ############################################################

        elif ( self.deflineType == self.ABSOLID or
               ( self.deflineType is None and
                 self.abSolid.match(self.deflineString) ) ):
            
            m = self.abSolid.match(self.deflineString)
            
            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values (prefix and/or suffix may contribute to making name unique)
            
            (self.prefix, self.panel, self.x, self.y, self.tagType, self.suffix, endSep) = m.groups()

            # Set defline name

            if self.abiTitle:
                self.name = self.abiTitle + "_" + self.prefix + self.panel + "_" + self.x + "_" + self.y
            else:
                self.name = self.prefix + self.panel + "_" + self.x + "_" + self.y

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.ABSOLID
                self.platform = "ABSOLID"

        ############################################################
        # New Illumina
        #
        # @M00730:68:000000000-A2307:1:1101:14701:1383 1:N:0:1 (XXX574591)
        # @HWI-962:74:C0K69ACXX:8:2104:14888:94110 2:N:0:CCGATAT (XXX610048)
        # @HWI-ST808:130:H0B8YADXX:1:1101:1914:2223 1:N:0:NNNNNN-GGTCCA-AAAA (YYY1106612)
        # @HWI-M01380:63:000000000-A8KG4:1:1101:17932:1459 1:N:0:Alpha29 CTAGTACG|0|GTAAGGAG|0 (SRR1767413)
        # @HWI-ST959:56:D0AW4ACXX:8:1101:1233:2026 2:N:0: (XXX770604)
        # @DJB77P1:546:H8V5MADXX:2:1101:11528:3334 1:N:0:_I_GACGAC (WWW000015)
        # @HET-141-007:154:C391TACXX:6:1216:12924:76893 1:N:0 (WWW000006)
        # @DG7PMJN1:293:D12THACXX:2:1101:1161:1968_1:N:0:GATCAG (XXX998303)
        # @M01321:49:000000000-A6HWP:1:1101:17736:2216_1:N:0:1/M01321:49:000000000-A6HWP:1:1101:17736:2216_2:N:0:1 (WWW000016)
        # @MISEQ:36:000000000-A5BCL:1:1101:24982:8584;smpl=12;brcd=ACTTTCCCTCGA 1:N:0:ACTTTCCCTCGA (WWW000026)
        # @HWI-ST1234:33:D1019ACXX:2:1101:1415:2223/1 1:N:0:ATCACG (XXX1692309)
        # @aa,HWI-7001455:146:H97PVADXX:2:1101:1498:2093 1:Y:0:ACAAACGGAGTTCCGA (WWW000027)
        # @NS500234:97:HC75GBGXX:1:11101:6479:1067 1:N:0:ATTCAG+NTTCGC (WWW000028)
        # @M01388:38:000000000-A49F2:1:1101:14022:1748 1:N:0:0 (WWW000029)
        # @M00388:100:000000000-A98FW:1:1101:17578:2134 1:N:0:1|isu|119|c95|303 (WWW000030)
        # @HISEQ:191:H9BYTADXX:1:1101:1215:1719 1:N:0:TTAGGC##NGTCCG (WWW000031)
        # @HWI:1:X:1:1101:1298:2061 1:N:0: AGCGATAG (barcode is discarded) (WWW000039)
        # @8:1101:1486:2141 1:N:0:/1 (WWW000033)
        # @HS2000-1017_69:7:2203:18414:13643|2:N:O:GATCAG (WWW000045)
        # @HISEQ:258:C6E8AANXX:6:1101:1823:1979:CGAGCACA:1:N:0:CGAGCACA:NG:GT (SRR3156573)
        # @HWI-ST226:170:AB075UABXX:3:1101:1436:2127 1:N:0:GCCAAT (XXX104658)
        # @HISEQ06:187:C0WKBACXX:6:1101:1198:2254 1:N:0: (XXX788729)
        ############################################################

        elif ( self.deflineType == self.ILLUMINA_NEW or
               self.deflineType == self.ILLUMINA_NEW_DOUBLE or
               self.deflineType == self.ILLUMINA_NEW_OLD or
               ( self.deflineType is None and
                 ( self.illuminaNew.match(self.deflineString) or
                   self.illuminaNewWithJunk.match(self.deflineString) or
                   self.illuminaNewNoPrefix.match(self.deflineString) or
                   self.illuminaNewWithUnderscores.match(self.deflineString) ) ) ):

            if self.deflineType:
                m = self.illuminaNew.match(self.deflineString)
            else:
                if self.illuminaNew.match(self.deflineString):
                    m = self.illuminaNew.match(self.deflineString)
                    self.foundRE = self.illuminaNew
                elif self.illuminaNewNoPrefix.match(self.deflineString):
                    m = self.illuminaNewNoPrefix.match(self.deflineString)
                    self.foundRE = self.illuminaNewNoPrefix
                elif self.illuminaNewWithJunk.match(self.deflineString):
                    m = self.illuminaNewWithJunk.match(self.deflineString)
                    self.foundRE = self.illuminaNewWithJunk
                else:
                    m = self.illuminaNewWithUnderscores.match(self.deflineString)
                    self.foundRE = self.illuminaNewWithUnderscores

                # Must set these here because last check may not be executed
                
                if self.saveDeflineType:
                    self.illuminaNew = self.foundRE
                    self.platform = "ILLUMINA"

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid
            
            (self.prefix, sep1, self.lane, sep2, self.tile, sep3, self.x, sep4, self.y, sep5,
             self.readNum, self.filterRead, reserved, self.spotGroup, endSep ) = m.groups()

            # Capture info in sep5 as suffix

            if ( sep5 is not None and
                 len(sep5) > 2 ):
                s = self.illuminaNewSuffix.match ( sep5 )
                if s is not None:
                    ( discard1, discard2, self.suffix, discard3, discard4, discard5, discard6 ) = s.groups()

            # Value of 0 for spot group is equivalent to no spot group

            if self.spotGroup == "0":
                self.spotGroup = ''

            # Set defline name

            if self.prefix:
                self.name = self.prefix + sep1 + self.lane + sep2 + self.tile + sep3 + self.x + sep4 + self.y
            else:
                self.name = self.lane + sep2 + self.tile + sep3 + self.x + sep4 + self.y

            # Check for doubled-up defline for both reads
            # Potential for mixed double-up and fragment deflines, too.
            # So setting to self.ILLUMINA_NEW_DOUBLE on first occurrence
            # Assuming single character separator between the two names.

            if ( self.deflineType == self.ILLUMINA_NEW_DOUBLE or
                 ( not self.deflineType and
                   len(self.spotGroup) > len(self.name) and
                   re.search( re.escape(self.name), self.spotGroup) ) ):
                if (sys.version).startswith("3"):
                    start = str.find(self.spotGroup,self.name)
                else:
                    start = string.find(self.spotGroup,self.name)
                if start != -1:
                    self.spotGroup = self.spotGroup[0:start-1]
                    if self.saveDeflineType:
                        self.deflineType = self.ILLUMINA_NEW_DOUBLE

            # Check for and remove read numbers after spot group
            
            elif ( self.deflineType == self.ILLUMINA_NEW_OLD or
                   ( not self.deflineType and
                     re.search ( "/[1234]$",self.spotGroup ) ) ):
                self.spotGroup = self.spotGroup[0:len(self.spotGroup)-2]
                if self.saveDeflineType:
                    self.deflineType = self.ILLUMINA_NEW_OLD

            # Set filter value better

            if self.filterRead == 'Y':
                self.filterRead = 1
            else:
                self.filterRead = 0

            # Save defline type if not previously set (must be here)
            
            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.ILLUMINA_NEW

        ############################################################
        # Old Illumina
        #
        # @HWIEAS210R_0014:3:28:1070:11942#ATCCCG/1 (XXX799414)
        # @FCABM5A:1:1101:15563:1926#CTAGCGCT_CATTGCTT/1 (WWW000014)
        # @HWI-ST1374:1:1101:1161:2060#0/1 (XXX001101)
        # @ID57_120908_30E4FAAXX:3:1:1772:953/1 (XXX020199)
        # @HWI-EAS30_2_FC200HWAAXX_4_1_646_399^M (XXX013586)
        # @R1:1:221:197:649/1 (YYY003002)
        # @R16:8:1:0:1617#0/1^M (YYY003003)
        # @7:1:792:533 (YYY013547)
        # @7:1:164:-0 (YYY013547)
        # @HWUSI-BETA8_3:7:1:-1:14 (YYY015261)
        # @rumen9533:0:0:0:5 (YYY020795)
        # @IL10_334:1:1:4:606 (YYY036999)
        # @HWUSI-EAS517-74:3:1:1023:8178/1 ~ RGR:Uk6; (WWW000002)
        # @FCC19K2ACXX:3:1101:1485:2170#/1 (WWW000017)
        # @MB27-02-1:1101:1723:2171 /1 (WWW000003)
        # @AMS2007273_SHEN-MISEQ01:47:1:1:12958:1771:0:1#0 (WWW000008)
        # @AMS2007273_SHEN-MISEQ01:47:1:1:17538:1769:0:0#0 (WWW000008)
        # @120315_SN711_0193_BC0KW7ACXX:1:1101:1419:2074:1#0/1 (WWW000001)
        # @ATGCT_7_1101_1418_2098_1 (WWW000007)
        # HWI-ST155_0544:1:1:6804:2058#0/1 (SINGLE LINE FASTQ SO NO '@') (YYY003101)
        # @HWI-ST225:626:C2Y82ACXX:3:1208:2931:82861_1 (WWW000040)
        # >M01056:83:000000000-A7GBN:1:1108:17094:2684--W1 (WWW000019)
        # @D3LH75P1:1:1101:1054:2148:0 1:1 (WWW000032)
        # @HWI-IT879:92:5:1101:1170:2026#0/1:0 (WWW000037)
        # @HWI-ST225:626:C2Y82ACXX:3:1208:2222:82880_1 (WWW000040)
        # @FCA5PJ4:1:1101:14707:1407#GTAGTCGC_AGCTCGGT/1 (SRR2006030)
        # @SOLEXA-GA02_1:1:1:0:106 (ERR011021)
        # @HWUSI-EAS499:1:3:9:1822#0/1 (XXX000093)
        # @BILLIEHOLIDAY_1_FC20F3DAAXX:8:2:342:540 (XXX013565)
        # @BILLIEHOLIDAY_1_FC200TYAAXX_3_1_751_675 (XXX013571)
        # @HWI-EAS30_2_FC20416AAXX_7_1_116_317 (XXX013600)
        # >KN-930:1:1:653:356 (XXX014056)
        # USI-EAS50_1:6:1:392:881 (XXX014283)
        # @FC12044_91407_8_1_46_673 (XXX015015)
        # @HWI-EAS299_2_30MNAAAXX:5:1:936:1505/1 (XXX015076)
        # @HWI-EAS-249:7:1:1:443/1 (XXX037956)
        # @741:6:1:1204:10747/1 (XXX094419)
        # @HWI-EAS385_0086_FC:1:1:1239:943#0/1 (XXX488373)
        # @HWUSI-EAS613-R_0001:8:1:1020:14660#0/1 (XXX556206)
        # @ILLUMINA-D01686_0001:7:1:1028:14175#0/1 (XXX567550)
        # @1920:1:1:1504:1082/1 (XXX627950)
        # @HWI-EAS397_0013:1:1:1083:11725#0/1 (XXX651965)
        # >HWI-EAS6_4_FC2010T:1:1:80:366 (YYY001656)
        # @NUTELLA_42A08AAXX:4:001:0003:0089/1 (YYY003100)
        # @R16:8:1:0:875#0/1 (YYY014126)
        # HWI-EAS102_1_30LWPAAXX:5:1:1456:776 (YYY016872)
        # @HWI-EAS390_30VGNAAXX1:1:1:377:1113/1 (YYY020188)
        # @ID57_120908_30E4FAAXX:3:1:1772:953/1 (YYY020203)
        # HWI-EAS440_102:8:1:168:1332 (YYY029167)
        # @SNPSTER4_246_30GCDAAXX_PE:1:1:3:896/1 (YYY029194)
        # @FC42AUBAAXX:6:1:4:1280#TGACCA/1 (YYY030833)
        # @SOLEXA9:1:1:1:2005#0/1 (YYY037749)
        # @SNPSTER3_264_30JGGAAXX_PE:2:1:218:311/1 (YYY058403)
        # @HWUSI-EAS535_0001:7:1:747:14018#0/1 (YYY065453)
        # @FC42ATTAAXX:5:1:0:20481 (YYY066636)
        # @HWUSI-EAS1571_0012:8:1:1017:20197#0/1 (YYY089777)
        # @SOLEXA1_0052_FC:8:1:1508:1078#TTAGGC/1 (YYY171628)
        ############################################################

        elif ( self.deflineType == self.ILLUMINA_OLD or
               ( self.deflineType is None and
                 ( self.illuminaOldColon.match ( self.deflineString ) or
                   self.illuminaOldUnderscore.match ( self.deflineString ) or
                   self.illuminaOldNoPrefix.match ( self.deflineString ) or
                   self.illuminaOldWithJunk2.match ( self.deflineString ) ) ) ) :

            # For first time around check for need to identify appropriate separator
            # Retain defline type if desired and not set and count extra numbers in name
            # Retain appropriate pattern. Note that illumina old with junk comes first
            # because illumina old colon pattern will always match that case, too.

            if self.deflineType:
                m = self.illuminaOld.match ( self.deflineString )
            elif self.illuminaOldWithJunk.match ( self.deflineString ):
                m = self.illuminaOldWithJunk.match ( self.deflineString )
                self.foundRE = self.illuminaOldWithJunk
            elif self.illuminaOldColon.match ( self.deflineString ) :
                m = self.illuminaOldColon.match ( self.deflineString )
                self.foundRE = self.illuminaOldColon
            elif self.illuminaOldUnderscore.match ( self.deflineString ):
                m = self.illuminaOldUnderscore.match ( self.deflineString )
                self.foundRE = self.illuminaOldUnderscore
            elif self.illuminaOldWithJunk2.match ( self.deflineString ):
                m = self.illuminaOldWithJunk2.match ( self.deflineString )
                self.foundRE = self.illuminaOldWithJunk2
            else:
                m = self.illuminaOldNoPrefix.match ( self.deflineString )
                self.foundRE = self.illuminaOldNoPrefix
                    
            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Collect values from regular expression pattern
            
            (self.prefix, sep1, self.lane, sep2, self.tile, sep3, self.x, sep4, self.y,
             self.spotGroup, self.readNum, endSep) = m.groups()
            if self.readNum:
                self.readNum = self.readNum[1:]

            # Check for suffix

            s = self.illuminaOldSuffix.match ( self.y )
            if s:
                ( self.y, self.suffix ) = s.groups()
                if len(self.suffix) < 3:
                    self.suffix = None

            # Determine number of discards first time through
            
            if ( not self.deflineType and
                 self.prefix) :
                self.numDiscards = self.countExtraNumbersInIllumina(sep4)

            # Discard extra numbers (which can cause a difference between pair deflines)
            # and set defline name

            if self.numDiscards > 0:
                
                if self.numDiscards == 1:
                    self.y = self.x
                    self.x = self.tile
                    self.tile = self.lane
                    m2 = re.match("([!-~]*?)(" + sep4 + ")(\d+)$", self.prefix)
                    if m2:
                        (self.prefix,sep0,self.lane) = m2.groups()
                        self.name = self.prefix + sep0 + self.lane + sep1 + self.tile + sep2 + self.x + sep3 + self.y
                    else:
                        self.lane = self.prefix
                        self.prefix = ""
                        self.name = self.lane + sep1 + self.tile + sep2 + self.x + sep3 + self.y

                elif self.numDiscards == 2:
                    self.y = self.tile
                    self.x = self.lane
                    m2 = re.match("([!-~]*?)(" + sep4 + ")(\d+)(" + sep4 + ")(\d+)(\s+|$)",self.prefix)
                    if m2:
                        (self.prefix,sep_1,self.lane,sep0,self.tile,sepUnused) = m2.groups()
                        self.name = self.prefix + sep_1 + self.lane + sep0 + self.tile + sep1 + self.x + sep2 + self.y
                    else:
                        m2 = re.match("(\d+)(" + sep4 + ")(\d+)(\s+|$)",self.prefix)
                        if m2:
                            (self.lane,sep0,self.tile,sepUnused) = m2.groups()
                            self.prefix = ""
                            self.name = self.lane + sep0 + self.tile + sep1 + self.x + sep2 + self.y

            # If no discards and prefix exists, set name including prefix
            
            elif self.prefix:
                self.name = self.prefix + sep1 + self.lane + sep2 + self.tile + sep3 + self.x + sep4 + self.y

            # If no discards and no prefix, set name without prefix
            
            else:
                self.name = self.lane + sep2 + self.tile + sep3 + self.x + sep4 + self.y

            # Remove # from front of spot group if present
            # Value of 0 for spot group is equivalent to no spot group

            if self.spotGroup:
                self.spotGroup = self.spotGroup[1:]
            
            if self.spotGroup == "0":
                self.spotGroup = ''

            # Save defline type and regular expression (must occur after determination of numDiscards)

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.illuminaOld = self.foundRE
                self.platform = "ILLUMINA"
                self.deflineType = self.ILLUMINA_OLD

        ############################################################
        # qiime with new illumina (only first spot qiime name retained)
        #
        # @B11.13210.SIV.Barouch.Stool.250.06.8.13.12_5644 M00181:229:000000000-AAPUA:1:1101:9433:3327 1:N:0:1 orig_bc=TGACCTCCTAGA new_bc=TGACCTCCAAGA bc_diffs=1 (YYY908068)
        # @2wkRT.79_123 M00176:18:000000000-A0DK4:1:1:13923:1732 1:N:0:0 orig_bc=ATGCTAACCACG new_bc=ATGCTAACCACG bc_diffs=0 (XXX776282)
        # @ HWI-M01929:28:000000000-A6VG4:1:2109:8848:7133 1:N:0:GTGTT  orig_bc=GCTTA   new_bc=GCTTA    bc_diffs=0 (WWW000005)
        # @cp2  :1:1101:17436:1559:1:N:0:5/1_:1:1101:17436:1559:2:N:0:5/2   124 124 (WWW000020)
        # @10_194156 HWI-M02808:46:AAHRM:1:1101:17744:1823 1:N:0:ATGAGACTCCAC orig_bc=ATGAGACTCCAC new_bc=ATGAGACTCCAC bc_diffs=0 (WWW000013)
        # @AM-B-CON M02233:62:000000000-A9GLW:1:1101:15425:1859 1:N:0:111^M (WWW000024)
        # >26.04.2015.WO.Comp.S55_1 M00596.112.000000000.AHGJM.1.1101.20901.1309 1.N.0.55 (SRR3112744)
        # >PSF.1d.20_0 HISEQ:128:160215_SNL128_0128_AHJT2MBCXX:1:1101:3422:2184 1:N:0: orig_bc=TATAGCGACTACTATA new_bc=TATAGCGACTACTATA bc_diffs=0 (SRR3992252)
        # @2-796964 M01929:5:000000000-A46YE:1:1108:16489:18207 1:N:0:2 (XXX1778155)
        ############################################################

        elif ( self.deflineType == self.QIIME_ILLUMINA_NEW or
               self.deflineType == self.QIIME_ILLUMINA_NEW_BC or
               self.deflineType == self.QIIME_ILLUMINA_NEW_DBL or
               self.deflineType == self.QIIME_ILLUMINA_NEW_DBL_BC or
               ( self.deflineType is None and
                 ( self.qiimeIlluminaNew.match(self.deflineString) or
                   self.qiimeIlluminaNewPeriods.match(self.deflineString) or
                   self.qiimeIlluminaNewUnderscores.match(self.deflineString) ) ) ):
            
            if self.deflineType:
                m = self.qiimeIlluminaNew.match ( self.deflineString )
            else:
                if self.qiimeIlluminaNew.match ( self.deflineString ) :
                    m = self.qiimeIlluminaNew.match ( self.deflineString )
                    self.foundRE = self.qiimeIlluminaNew
                elif self.qiimeIlluminaNewPeriods.match ( self.deflineString ):
                    m = self.qiimeIlluminaNewPeriods.match ( self.deflineString )
                    self.foundRE = self.qiimeIlluminaNewPeriods
                else:
                    m = self.qiimeIlluminaNewUnderscores.match ( self.deflineString )
                    self.foundRE = self.qiimeIlluminaNewUnderscores
                
                # Must set here because last check may not be executed

                if self.saveDeflineType:
                    self.qiimeIlluminaNew = self.foundRE
                    self.platform = "ILLUMINA"

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.qiimeName, self.prefix, sep1, self.lane, sep2, self.tile, sep3, self.x, sep4, self.y, sep5,
             self.readNum, self.filterRead, reserved, self.spotGroup, endSep ) = m.groups()

            # Check for the presence of barcode corrections

            m_bc = None
            if ( self.deflineType == self.QIIME_ILLUMINA_NEW_BC or
                 ( not self.deflineType and
                   self.qiimeBc.match(self.deflineString) ) ) :
                m_bc = self.qiimeBc.match(self.deflineString)
                if ( self.deflineType == self.QIIME_ILLUMINA_NEW_BC and
                     m_bc is None ):
                    self.isValid = False
                    return self.isValid
                else:
                    self.spotGroup = m_bc.group(2)

            # Set defline name

            self.name = self.prefix + sep1 + self.lane + sep2 + self.tile + sep3 + self.x + sep4 + self.y

            # Check for doubled-up defline for both reads
            # Potential for mixed double-up and fragment deflines, too.
            # So setting to self.ILLUMINA_NEW_DOUBLE on first occurrence
            # Assuming single character separator between the two names.

            if ( self.deflineType == self.QIIME_ILLUMINA_NEW_DBL or
                 self.deflineType == self.QIIME_ILLUMINA_NEW_DBL_BC or
                 ( not self.deflineType and
                   len(self.spotGroup) > len(self.name) and
                   re.search( re.escape(self.name), self.spotGroup) ) ):
                if (sys.version).startswith("3"):
                    start = str.find(self.spotGroup,self.name)
                else:
                    start = string.find(self.spotGroup,self.name)
                if start != -1:
                    self.spotGroup = self.spotGroup[0:start-1]
                    if re.search( "/[12]", self.spotGroup ):
                        self.spotGroup = self.spotGroup[:len(self.spotGroup) - 2]
                    if ( not self.deflineType and
                         self.saveDeflineType ):
                        if self.deflineType == self.QIIME_ILLUMINA_NEW_BC:
                            self.deflineType = self.QIIME_ILLUMINA_NEW_DBL_BC
                        else:
                            self.deflineType = self.QIIME_ILLUMINA_NEW_DBL

            # Set filter value better

            if self.filterRead == 'Y':
                self.filterRead = 1
            else:
                self.filterRead = 0

            # Retain defline type if desired (must stay here)

            if ( not self.deflineType and
                 self.saveDeflineType ):
                if m_bc is None:
                    self.deflineType = self.QIIME_ILLUMINA_NEW
                else:
                    self.deflineType = self.QIIME_ILLUMINA_NEW_BC

        ############################################################
        # qiime with old illumina (only first spot qiime name retained)
        #
        # @B11.13210.SIV.Barouch.Stool.250.06.8.13.12_378 M00181:229:000000000-AAPUA:1:1101:19450:2192#0/1 orig_bc=TGACCTCCAAGA new_bc=TGACCTCCAAGA bc_diffs=0 (ZZZ908068)
        # @B11.13210.SIV.Barouch.Stool.250.06.8.13.12_158 M00181:229:000000000-AAPUA:1:1101:19450:2192#0/2 orig_bc=TGACCTCCAAGA new_bc=TGACCTCCAAGA bc_diffs=0 (ZZZ908068)
        ############################################################

        elif ( self.deflineType == self.QIIME_ILLUMINA_OLD or
               self.deflineType == self.QIIME_ILLUMINA_OLD_BC or
               ( self.deflineType is None and
                 self.qiimeIlluminaOld.match(self.deflineString) ) ):
            
            m = self.qiimeIlluminaOld.match(self.deflineString)
            
            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.qiimeName, self.prefix, sep1, self.lane, sep2, self.tile, sep3, self.x, sep4, self.y,
             self.spotGroup, self.readNum, endSep) = m.groups()
            if self.readNum:
                self.readNum = self.readNum[1:]
            if self.spotGroup:
                self.spotGroup = self.spotGroup[1:]

            # Check for the presence of barcode corrections

            m_bc = None
            if ( self.deflineType == self.QIIME_ILLUMINA_OLD_BC or
                 ( not self.deflineType and
                   self.qiimeBc.match(self.deflineString) ) ) :
                m_bc = self.qiimeBc.match(self.deflineString)
                if ( self.deflineType == self.QIIME_ILLUMINA_OLD_BC and
                     m_bc is None ):
                    self.isValid = False
                    return self.isValid
                else:
                    self.spotGroup = m_bc.group(2)

            # Set defline name

            self.name = self.prefix + sep1 + self.lane + sep2 + self.tile + sep3 + self.x + sep4 + self.y

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                if m_bc is None:
                    self.deflineType = self.QIIME_ILLUMINA_OLD
                else:
                    self.deflineType = self.QIIME_ILLUMINA_OLD_BC
                self.platform = "ILLUMINA"

        ############################################################
        # 454 defline
        #
        # @GG3IVWD03F5DLB length=97 xy=2404_1917 region=3 run=R_2010_05_11_11_15_22_ (XXX529889)
        # @EV5T11R03G54ZJ (YYY307780)
        # @GKW2OSF01D55D9 (ERR016499)
        # @OS-230b_GLZVSPV04JTNWT (ERR039808)
        # @HUT5UCF07H984F (SRR2035362)
        # @EM7LVYS02FOYNU/1 (WWW000042 or WWW000043)
        ############################################################

        elif ( self.deflineType == self.LS454 or
               ( self.deflineType is None and
                 self.ls454.match( self.deflineString ) ) ):

            # Capture 454 values
            
            m = self.ls454.match( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.prefix,self.dateAndHash454,self.region454,self.xy454,self.readNum,endSep) = m.groups()
            if self.readNum:
                self.readNum = self.readNum[1:]
            
            # Set name

            self.name = self.prefix + self.dateAndHash454 + self.region454 + self.xy454

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.LS454
                self.platform = "LS454"

        ############################################################
        # qiime with 454
        #
        # @T562_7000012 H29C5KU01AZBDB orig_bc=AGCTCACGTA new_bc=AGCTCACGTA bc_diffs=0 (WWW000018)
        ############################################################

        elif ( self.deflineType == self.QIIME_454 or
               self.deflineType == self.QIIME_454_BC or
               ( self.deflineType is None and
                 self.qiime454.match( self.deflineString ) ) ):

            # Capture 454 values
            
            m = self.qiime454.match( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.qiimeName,self.dateAndHash454,self.region454,self.xy454,self.readNum,endSep) = m.groups()
            if self.readNum:
                self.readNum = self.readNum[1:]

            # Extract barcode if present
            
            m_bc = None
            if ( self.deflineType == self.QIIME_454_BC or
                 ( not self.deflineType and
                   self.qiimeBc.match(self.deflineString) ) ) :
                m_bc = self.qiimeBc.match(self.deflineString)
                if ( self.deflineType == self.QIIME_454_BC and
                     m_bc is None ):
                    self.isValid = False
                    return self.isValid
                else:
                    self.spotGroup = m_bc.group(2)

            # Set name

            self.name = self.dateAndHash454 + self.region454 + self.xy454

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                if m_bc is None:
                    self.deflineType = self.QIIME_454
                else:
                    self.deflineType = self.QIIME_454_BC
                self.platform = "LS454"

        ############################################################
        # Pacbio CCS/RoIs reads or subreads
        #
        # See https://www.biostars.org/p/146048/ for field descriptions
        # Also see https://speakerdeck.com/pacbio/specifics-of-smrt-sequencing-data
        # @m120525_202528_42132_c100323432550000001523017609061234_s1_p0/43 (XXX941211)
        # @m101111_134728_richard_c000027022550000000115022502211150_s1_p0/1 (YYY075011)
        # @m110115_082846_Uni_c000000000000000000000012706400001_s3_p0/1/0_508 (SRR497981)
        # @m130727_043304_42150_c100538232550000001823086511101337_s1_p0/16/0_5273 (XXX989791)
        # @m120328_022709_00128_c100311312550000001523011808061260_s1_p0/129/0_4701 (WWW000010)
        # @m120204_011539_00128_c100220982555400000315052304111230_s2_p0/8/0_1446 (WWW000009)
        ############################################################

        elif ( self.deflineType == self.PACBIO or
               ( self.deflineType is None and
                 self.pacbio.match ( self.deflineString ) ) ):
            
            # Capture name after '@' sign

            m = self.pacbio.match( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match value
            
            self.name = m.group(1)

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.PACBIO
                self.platform = "PACBIO"

        ############################################################
        # ion torrent
        #
        # @A313D:7:49 (XXX486160)
        # @RD4FE:00027:00172 (XXX2925654)
        # @ONBWR:00329:02356/1 (WWW000044)
        # >311CX:3560:2667   length=347 (SRR547526)
        ############################################################

        elif ( self.deflineType == self.ION_TORRENT or
               ( self.deflineType is None and
                 self.ionTorrent.match ( self.deflineString ) ) ):
            
            # Capture name after '@' sign

            m = self.ionTorrent.match( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.runId, sep1, self.row, sep2, self.column, self.readNum, endSep) = m.groups()

            # Interpret readNum, if found

            if self.readNum:
                if ( self.readNum[0:1] == "/" or
                     self.readNum[0:1] == "\\" ):
                    self.readNum = self.readNum[1:]
                elif self.readNum == "L":
                    self.readNum = "1"
                elif self.readNum == "R":
                    self.readNum = "2"

            # Set defline name

            self.name = self.runId + sep1 + self.row + sep2 + self.column

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.ION_TORRENT
                self.platform = "ION_TORRENT"

        ############################################################
        # Old illumina bar code and/or read number only
        # Must occur after pacbio defline check
        #
        # @_2_#GATCAGAT/1 (WWW000004)
        # @Read_190546#BC005 length=1419 (XXX1616052)
        # @SN971:2:1101:15.80:103.70#0/1 (WWW000034)
        ############################################################
        
        elif ( self.deflineType == self.ILLUMINA_OLD_BC_RN or
               ( self.deflineType is None and
                 ( self.illuminaOldBcRnOnly.match ( self.deflineString ) or
                   self.illuminaOldBcOnly.match ( self.deflineString ) or
                   self.illuminaOldRnOnly.match ( self.deflineString ) ) ) ):
            
            if self.deflineType:
                m = self.illuminaOldBcRn.match ( self.deflineString )
            elif self.illuminaOldBcRnOnly.match ( self.deflineString ) :
                m = self.illuminaOldBcRnOnly.match ( self.deflineString )
                self.foundRE = self.illuminaOldBcRnOnly
            elif self.illuminaOldBcOnly.match ( self.deflineString ):
                m = self.illuminaOldBcOnly.match ( self.deflineString )
                self.foundRE = self.illuminaOldBcOnly
            else:
                m = self.illuminaOldRnOnly.match ( self.deflineString )
                self.foundRE = self.illuminaOldRnOnly
                    
            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Assign values

            (self.name, self.spotGroup, self.readNum, endSep) = m.groups()
            if self.spotGroup[0:1] == "#":
                self.spotGroup = self.spotGroup[1:]
                if ( self.readNum[0:1] == "/" or
                     self.readNum[0:1] == "\\" ):
                    self.readNum = self.readNum[1:]
                else:
                    self.readNum = None
            else:
                self.readNum = self.spotGroup[1:]
                self.spotGroup = None

            if self.spotGroup == "0":
                self.spotGroup = None

            # Save defline type and regular expression (must occur after determination of numDiscards)

            if ( not self.deflineType and
                 self.saveDeflineType) :
                self.illuminaOldBcRn = self.foundRE
                self.deflineType = self.ILLUMINA_OLD_BC_RN
                self.platform = "UNDEFINED"

        ############################################################
        # generic qiime
        #
        # @10317.000016458_0 orig_bc=TGCACCTCTGTC new_bc=TGCACCTCTGTC bc_diffs=0 (WWW000022)
        ############################################################

        elif ( self.deflineType == self.QIIME_GENERIC or
               ( self.deflineType is None and
                 self.qiimeBc.match( self.deflineString ) ) ):

            # Capture generic qiime values
            
            m = self.qiimeBc.match( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.name,self.spotGroup) = m.groups()

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.QIIME_GENERIC
                self.platform = "UNDEFINED"

        ############################################################
        # Nanopore/MinION fastq
        #
        # @77_2_1650_1_ch100_file0_strand_twodirections (XXX2761339)
        # @77_2_1650_1_ch100_file16_strand_twodirections:pass\77_2_1650_1_ch100_file16_strand.fast5 (SRR2761339)
        # @channel_108_read_11_twodirections:flowcell_17/LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file21_strand.fast5 (XXX637417 or YYY637417)
        # @channel_108_read_11_complement:flowcell_17/LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file21_strand.fast5 (XXX637417 or YYY637417)
        # @channel_108_read_11_template:flowcell_17/LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file21_strand.fast5 (XXX637417 or YYY637417)
        # @channel_346_read_183-1D (SRR1747417)
        # @channel_346_read_183-complement (SRR1747417)
        # @channel_346_read_183-2D (SRR1747417)
        # @ch120_file13-1D (SRR1980822)
        # @ch120_file13-2D (SRR1980822)
        # @channel_108_read_8:LomanLabz_PC_E.coli_MG1655_ONI_3058_1_ch108_file18_strand.fast5 (R-based poRe fastq requires filename, too) (WWW000025)
        # @1dc51069-f61f-45db-b624-56c857c4e2a8_Basecall_2D_000_2d oxford_PC_HG02.3attempt_0633_1_ch96_file81_strand_twodirections:CORNELL_Oxford_Nanopore/oxford_PC_HG02.3attempt_0633_1_ch96_file81_strand.fast5 (SRR2848544 - self.nanopore3)
        # @ae74c4fb-2c1d-4176-9584-3dfcc6dce41e_Basecall_2D_2d UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch93_read2620_strand NB06\UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch93_read2620_strand.fast5 (SRR5085901 - self.nanopore3)
        # @ddb7d987-73c0-4d9a-8ac0-ac0dbc462ab5_Basecall_2D_2d UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch100_read4767_strand1 NB06\UT317077_20160808_FNFAD22478_MN19846_sequencing_run_FHV_Barcoded_TakeII_88358_ch100_read4767_strand1.fast5 (SRR5085901 - self.nanopore3)
        # @channel_101_read_1.1C|1T|2D (ERR1121618 bam converted to fastq)
        ############################################################

        elif ( self.deflineType == self.NANOPORE or
               ( self.deflineType is None and
                 ( self.nanopore.match( self.deflineString ) or
                   self.nanopore2.match( self.deflineString ) or
                   self.nanopore3.match( self.deflineString ) ) ) ) :
        
            if self.deflineType:
                m = self.nanopore.match ( self.deflineString )

            elif self.nanopore.match ( self.deflineString ) :
                m = self.nanopore.match ( self.deflineString )
                self.foundRE = self.nanopore
            elif self.nanopore2.match ( self.deflineString ) :
                m = self.nanopore2.match ( self.deflineString )
                self.foundRE = self.nanopore2
            else:
                m = self.nanopore3.match ( self.deflineString )
                self.foundRE = self.nanopore3

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Assign values (note that readNum may actually be a file number which is not the same)

            poreMid = None
            if ( self.nanopore == self.nanopore3 or
                 self.foundRE == self.nanopore3 ):
                ( self.name, self.poreRead, discard, poreStart, self.channel, poreMid, self.readNo, poreEnd, endSep ) = m.groups()
                self.poreFile = poreStart + self.channel + poreMid + self.readNo + poreEnd
            else:
                ( poreStart, self.channel, poreMid, self.readNo, poreEnd, self.poreRead, self.poreFile, endSep ) = m.groups()
                self.name = poreStart + self.channel + poreMid + self.readNo + poreEnd

            # Set readNum to None if actually a file number

            if ( poreMid and
                 poreMid == "_file" ):
                self.readNo = 0

            # Process poreFile if present

            if self.poreFile:

                # Check for 'pass' or 'fail'

                if re.search( "pass(/|\\\\)", self.poreFile ):
                    self.filterRead = 0
                elif re.search( "fail(/|\\\\)", self.poreFile ):
                    self.filterRead = 1

                # Check for barcode

                b = re.search ( "(NB\d{2}|BC\d{2})(/|\\\\)", self.poreFile )
                if b:
                    (self.spotGroup,delimiter) = b.groups()
                
                # Split poreFile on '/' or '\' if present

                poreFileChunks = re.split('/|\\\\',self.poreFile)
                if len ( poreFileChunks) > 1:
                    self.poreFile = poreFileChunks.pop()

            # Check for missing poreRead (from R-based poRe fastq dump) and normalize read type

            if not self.poreRead:
                if self.filename:
                    if re.search( re.escape(".2D."), self.filename):
                        self.poreRead = "2D"
                    elif re.search( re.escape(".template."), self.filename):
                        self.poreRead = "template"
                    elif re.search( re.escape(".complement."), self.filename):
                        self.poreRead = "complement"
                    else:
                        self.statusWriter.outputErrorAndExit( "Unable to determine nanopore read type ... {}".format(self.deflineString) )
                else:
                    self.statusWriter.outputErrorAndExit( "Unable to determine nanopore read type ... {}".format(self.deflineString) )
            elif ( self.poreRead == "_twodirections" or
                   self.poreRead[1:] == "2D" or
                   self.poreRead == "_2d" ):
                self.poreRead = "2D"
            elif ( self.poreRead == "_template" or
                   self.poreRead == "-1D" or
                   self.poreRead == ".1T" ):
                self.poreRead = "template"
            else:
                self.poreRead = "complement"

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.NANOPORE
                self.nanopore = self.foundRE
                self.platform = "NANOPORE"

        ############################################################
        # read_id and barcode
        #
        # @12-Dfasci_84178 read_id=12-Dfasci::G2J4TZQ02D3VUU barcode=AAAAAATT (WWW000023)
        # @32_L3_60077 read_id=24_PPC4::HDOFHVG03GOW52 barcode=AAAAAACT (WWW000023)
        # @PF01_76 read_id=P2034:00008:00038 barcode=CTATACACT (SRR2420289)
        ############################################################

        elif ( self.deflineType == self.READID_BARCODE or
               ( self.deflineType is None and
                 self.readIdBarcode.match( self.deflineString ) ) ) :
            
            # Capture 'qiimeName', prefix, read_id, and barcode
            
            m = self.readIdBarcode.match ( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None:
                self.isValid = False
                return self.isValid

            # Get match values
            
            (self.qiimeName,self.prefix,self.name,self.spotGroup,endSep) = m.groups()
            
            # Prepend self.prefix onto self.name if it exists and not at start of self.qiimeName
            
            if ( self.prefix and
                 not re.search( re.escape(self.prefix[0:len(self.prefix)-2]), self.qiimeName ) ):
                self.name = self.prefix + self.name

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.READID_BARCODE
                self.platform = "UNDEFINED"

        ############################################################
        # Capillary/Sanger fastq with template & dir for input to newbler
        # Template is used for read/spot name; first name is discarded
        #
        # @Msex-P09-F_A01 template=Msex-P09-A01 dir=fwd library=BAC_end (SRR2762665)
        # @Msex-P09-R_A01 template=Msex-P09-A01 dir=rev library=BAC_end
        # >bac-190o01.f template=190o01 dir=f library=BACends (from https://contig.wordpress.com/2011/01/21/newbler-input-ii-sequencing-reads-from-other-platforms)
        # >bac-190o01.r template=190o01 dir=r library=BACends
        # >originalreadname_1 template=originalreadname dir=F library=somename (from https://contig.wordpress.com/2010/06/10/running-newbler-de-novo-assembly/
        # >originalreadname_2 template=originalreadname dir=R library=somename
        # >DJS045A03F template=DJS054A03 dir=F library=DJS045 trim=12-543 (from http://454.com/downloads/my454/documentation/gs-flx-plus/454SeqSys_SWManual-v2.6_PartC_May2011.pdf)
        ############################################################

        elif ( self.deflineType == self.SANGER_NEWBLER or
               ( self.deflineType is None and
                 self.sangerNewbler.match ( self.deflineString ) ) ) :

            # Capture 'qiimeName', prefix, read_id, and barcode
            
            m = self.sangerNewbler.match( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match values (template becomes the name)
            
            (localName,self.name,self.dir,endSep) = m.groups()

            # Set readNum (expected to be char)

            if self.dir[0] in ('f', 'F'):
                self.readNum = '1'
            elif self.dir[0] in ('r', 'R'):
                self.readNum = '2'
            else:
                self.statusWriter.outputErrorAndExit( "Unexpected sanger read dir value ... {}".format(self.deflineString) )

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.SANGER_NEWBLER
                self.platform = "CAPILLARY"

        ############################################################
        # Default just use non-whitespace characters after > or @
        # 
        # @MB03~zSEQ034LingCncrtPool1~0000282 (SRR869399)
        # @Lab1.3.ab1 1249 10 1068 (WWW000012)
        # @CH_BAC1_C05.ab1_extraction_2 (WWW000011)
        # @G15-D_3_1_903_603_0.81 (XXX006565)
        # >No_name^M (WWW000021)
        # @SN7001204_0288_BH97LHADXX_R_SRi_L5503_L5508:11106:1433:74165 (WWW000035)
        # @HWI-ST170:292:8:1101:1239-2176 (WWW000036)
        # @Read_1-Barcode=BC001-PIPELINE=V41 length=6487 (WWW000038)
        # @contig_2_to_3_R_inner_clone_1_9589_A11_BJ-674528_048.ab1 (SRR2064214)
        # @S3_332 (ERR1288564)
        # @sim_CFSAN001140-756880/1 (SRR3020730)
        ############################################################

        elif ( self.deflineType == self.UNDEFINED or
               ( self.deflineType is None and
                 self.undefined.match ( self.deflineString ) ) ):

            # Capture name after '@' sign

            m = self.undefined.match( self.deflineString )

            # Confirm regular expression succeeded
            
            if m is None :
                self.isValid = False
                return self.isValid

            # Get match value
            
            self.name = m.group(1)

            # Retain defline type if desired

            if ( not self.deflineType and
                 self.saveDeflineType ):
                self.deflineType = self.UNDEFINED
                self.platform = "UNDEFINED"

        else:
            self.isValid = False

        if ( self.isValid and
             self.ignLeadCharsNum ):
            self.ignoredLeadChars = self.name[0:self.ignLeadCharsNum]
            self.name = self.name[self.ignLeadCharsNum:]

        return self.isValid

        # SRA fastq output with read numbers
        # SRA fastq output without read numbers

    ############################################################
    # Count extra numbers in Illumina prefix (at most 2)
    ############################################################
    
    def countExtraNumbersInIllumina (self, sep):
        
        # Determine how many numbers at the end of self.prefix
        # separated by colons

        prefixChunks = re.split(sep,self.prefix)
        prefixChunksLen = len(prefixChunks)
        numCount = 0

        # Determine if prefix ends in one or two digits
        # delineated with 'sep'
        
        if prefixChunks[prefixChunksLen-1].isdigit():
            numCount += 1
        if ( prefixChunksLen > 1 and
             prefixChunks[prefixChunksLen-2].isdigit() ):
            numCount+=1

        # Determine how many numbers to discard (capping at 2 based on
        # what I have seen in the data)

        discardCount = 0
        if numCount > 0:
            if int(self.y) < 3:
                discardCount += 1
                if ( numCount == 2 and
                     int(self.x) < 3 ):
                    discardCount += 1

        return discardCount

############################################################
# Seq Class
############################################################

class Seq:
    """ Parses/validates sequence string """
    
    def __init__(self, seqString):
        self.seqOrig = None
        self.seq = None
        self.length = 0
        self.isValid = False
        self.isColorSpace = False
        self.isBaseSpace = False
        self.clipLeft = 0
        self.clipRight = 0
        self.csKey = None
        if (sys.version).startswith("3"):
            self.transBase1 = str.maketrans('', '', "ACTGNWSBVDHKMRY.")
            self.transBase2 = str.maketrans('', '', "ACTGNWSBVDHKMRY")
            self.transColor = str.maketrans('', '', "0123.")
        if seqString:
            self.parseSeq(seqString)

    ############################################################
    # Determine number of bases to clip from left of sequence
    ############################################################

    @classmethod
    def getClipLeft (cls, seqOrig, seq):
        if seqOrig[0] == seq[0]:
            return 0
        else:
            m = re.search ( "^([actgn.]*)", seqOrig )
            clipString = m.group(0)
            return len(clipString)
            
    ############################################################
    # Determine number of bases to clip from right of sequence
    ############################################################
    
    @classmethod
    def getClipRight (cls, seqOrig, seq):
        seqLen = len(seq)
        if seqOrig[seqLen-1] == seq[seqLen-1]:
            return 0
        else:
            m = re.search ( "([actgn.]*)$", seqOrig )
            clipString = m.group(0)
            return len(clipString)
            
    ############################################################
    # Determine if provided seqString matches only sequence characters
    ############################################################
    
    def parseSeq ( self, seqString ):
        self.seqOrig = seqString.strip()
        self.seq = self.seqOrig.upper()
        self.length = len(self.seq)
        self.isValid = False
        self.clipLeft = 0
        self.clipRight = 0
        self.csKey = None

        if self.length == 0:
            pass

        elif self.isBaseSpace:
            if (sys.version).startswith("3"):
                empty = self.seq.translate(self.transBase1)
            else:
                empty = self.seq.translate(None, "ACTGNWSBVDHKMRY.")
            if len(empty) == 0:
                self.isValid = True
                self.clipLeft = Seq.getClipLeft(self.seqOrig,self.seq)
                self.clipRight = Seq.getClipRight(self.seqOrig,self.seq)

        elif self.isColorSpace:
            if (sys.version).startswith("3"):
                empty2 = self.seq[1:].translate(self.transColor)
            else:
                empty2 = self.seq[1:].translate(None, "0123.")
            if ( len(empty2) == 0 and
                 self.seq[0:1] in "ACTG" ):
                self.isValid = True
                self.length -= 1
                self.csKey = self.seq[0:1]
                self.seq = self.seq[1:]
                self.length -= 1
                
        else:
            if (sys.version).startswith("3"):
                empty = self.seq.translate(self.transBase2)
            else:
                empty = self.seq.translate(None, "ACTGNWSBVDHKMRY")
            if len(empty) == 0:
                self.isValid = True
                self.isBaseSpace = True
                self.clipLeft = Seq.getClipLeft(self.seqOrig,self.seq)
                self.clipRight = Seq.getClipRight(self.seqOrig,self.seq)
        
            else:
                if (sys.version).startswith("3"):
                    empty2 = self.seq[1:].translate(self.transColor)
                else:
                    empty2 = self.seq[1:].translate(None, "0123.")
                if ( len(empty2) == 0 and
                     self.seq[0:1] in "ACTG" ):
                    self.isValid = True
                    self.isColorSpace = True
                    self.csKey = self.seq[0:1]
                    self.seq = self.seq[1:]
                    self.length -= 1
                
                else:

                    # Check for non-colorspace seq with dots
                    # (2nd check here to properly handle colorspace seq consisting of all dots)

                    if (sys.version).startswith("3"):
                        empty3 = self.seq.translate(self.transBase1)
                    else:
                        empty3 = self.seq.translate(None, "ACTGNWSBVDHKMRY.")
                    if len(empty3) == 0:
                        self.isValid = True
                        self.isBaseSpace = True
                        self.clipLeft = Seq.getClipLeft(self.seqOrig,self.seq)
                        self.clipRight = Seq.getClipRight(self.seqOrig,self.seq)

        return self.isValid

############################################################
# Qual Class
############################################################

class Qual:
    """ Parses/validates/characterizes quality string """

    def __init__(self, qualString, seqLen):
        self.qual = None
        self.length = 0
        self.isValid = False
        self.isNumQual = False
        self.isAscQual = False
        self.offset = 33
        self.minQual = 1000
        self.maxQual = 0
        self.minOrd = 1000
        self.maxOrd = 0
        self.seqLen = 0
        if seqLen:
            self.seqLen = seqLen
        if qualString:
            self.parseQual( qualString, seqLen )

    ############################################################
    # Determine if provided qualString matches only qual characters
    # in range or if numerical quality
    ############################################################
    
    def parseQual( self, qualString, seqLen ):
        self.qual = qualString.strip()
        self.length = 0
        self.isValid = False
        self.minQual = 1000
        self.maxQual = 0
        self.minOrd = 1000
        self.maxOrd = 0

        # isValid stays False if zero length
        
        if len(self.qual) == 0:
            return self.isValid

        # Check for numerical quality

        singleIntQual = False
        if ( ( not seqLen or seqLen == 1 ) and
             self.isInt(self.qual) and
             int(self.qual) <= 100 ):
            singleIntQual = True

        if ( self.isNumQual or
             ( not self.isAscQual and
               ( " " in self.qual or
                 singleIntQual ) ) ):
            for qualSubString in self.qual.split():
                if self.isInt(qualSubString):
                    if int(qualSubString) < self.minQual:
                        self.minQual = int(qualSubString)
                    if int(qualSubString) > self.maxQual:
                        self.maxQual = int(qualSubString)
                    self.length += 1
                    if self.maxQual > 100:
                        sys.exit( "Numerical quality is too high  ... {}".format(self.qual) )
                else:
                    self.length = 0
                    break

            if self.length != 0:
                self.isValid = True
                if not singleIntQual: # Not concluding numerical qual based on single integer quality
                    self.isNumQual = True

        # Check for ascii quality.
        # Defaulting to 33 offset for now
        # Subsequent processing will be used better establish the offset

        else:
            for c in self.qual:
                o = ord(c)
                if o < self.minOrd:
                    self.minOrd = o
                if o > self.maxOrd:
                    self.maxOrd = o

            if self.maxOrd <= 126: # no special characters present
                self.length = len(self.qual)
                self.minQual = self.minOrd - self.offset
                self.maxQual = self.maxOrd - self.offset
                self.isValid = True
                self.isAscQual = True

        return self.isValid

    ############################################################
    # Check for integer
    ############################################################

    @staticmethod
    def isInt ( qStr ):
        if qStr[0] in ('-', '+'):
            if len(qStr) > 1:
                return qStr[1:].isdigit()
            else:
                return False
        else:
            return qStr.isdigit()
    
############################################################
# FastqReader Class
############################################################

class FastqReader:
    """ Retains information read/parsed from fastq file """

    def __init__(self,filename,handle):
        handle.seek(0)
        self.filename = filename
        self.handle = handle
        self.qualHandle = None
        self.deflineCharSeq = "@"
        self.deflineCharQual = "+"
        self.deflineStringSeq = ''
        self.deflineStringQual = ''
        self.defline = Defline( None )
        self.defline.filename = self.filename
        self.deflineQual = None
        self.deflineCheck = None
        self.seqOrig = None # Using None for proper results from 'isMultiLineFasta'; Used for determination of left/right clips and eof
        self.seq = ''
        self.qualOrig = None # Using None for proper results from 'isMultiLineFastq'; Used for determination of eof
        self.qual = ''
        self.length = 0
        self.lengthQual = 0
        self.spotCount = 0
        self.eof = False
        self.csKey = ''
        self.isColorSpace = False
        self.isNumQual = False
        self.isMultiLine = False
        self.isFasta = False
        self.defaultQual = "?" # 30 using 33 offset
        self.isSplitSeqQual = False
        self.seqQualDeflineMismatch = False
        self.setClips = False
        self.clipLeft = 0
        self.clipRight = 0
        self.extraQualForCSkey = False
        self.checkSeqQual = True
        self.seqParser = Seq( '' )
        self.qualParser = Qual( '', 0 )
        self.seqLineCount = None
        self.qualLineCount = None
        self.savedDeflineString = ''
        self.savedDeflineStringSeq = ''
        self.savedDeflineStringQual = ''
        self.outputStatus = False
        self.statusWriter = None
        self.headerLineCount = self.processHeader(handle,self.defline)
        self.deflineCheck = copy.copy(self.defline) # Putting here in case abiTitle is set; Use for multiline qual

    ############################################################
    # Initialize fastq reader to reflect current understanding
    # of the data. Some sw values may not be set yet
    ############################################################
    
    def setStatus (self, status):
        self.restart()
        self.isNumQual = status.isNumQual
        self.isColorSpace = status.isColorSpace
        self.extraQualForCSkey = status.extraQualForCSkey
        self.checkSeqQual = status.checkSeqQual
        self.setClips = status.setClips
        self.statusWriter = status.statusWriter
        self.defline.statusWriter = status.statusWriter
        self.deflineCheck.statusWriter = status.statusWriter
        self.ignoreNames = status.ignoreNames
        self.discardNames = status.discardNames
        if self.deflineQual:
            self.deflineQual.statusWriter = status.statusWriter
        if status.mixedDeflines:
            self.discardDeflineType()
        if status.ignLeadCharsNum:
            self.ignoreLeadDeflineChars ( status.ignLeadCharsNum )

    ############################################################
    # Read from fastq handle
    ############################################################
    
    def read (self):

        # Process seq defline

        self.processSeqDefline()

        # Read seq
        
        self.length = self.readSeq()

        # Process qual defline

        if self.qualHandle:
            self.processQualDefline(self.qualHandle)
        else:
            self.processQualDefline(self.handle)

        # Read qual

        if self.qualHandle:
            self.lengthQual = self.readQual( self.qualHandle, self.deflineQual )
        else:
            self.lengthQual = self.readQual( self.handle, self.defline )

        # Adjust for color space

        if self.isColorSpace:
            self.adjustForCS()

        # Increment spot count and check for eol
        
        self.updateSpotCountOrEof()

    ############################################################
    # Process seq defline
    ############################################################
    
    def processSeqDefline (self):
        
        if self.savedDeflineString:
            self.deflineStringSeq = self.savedDeflineString
        elif self.savedDeflineStringSeq:
            self.deflineStringSeq = self.savedDeflineStringSeq
            self.savedDeflineStringSeq = ''
        else:
            self.deflineStringSeq = self.handle.readline().strip()

        self.defline.parseDeflineString( self.deflineStringSeq )

        if not self.defline.isValid:
            self.findValidDefline(False)

    ############################################################
    # Read seq and convert to uppercase
    ############################################################
    
    def readSeq (self):

        attemptCount = 0
        
        if self.isMultiLine:
            if ( self.isFasta or
                 self.isSplitSeqQual ):
                self.savedDeflineStringSeq = self.readMultiLineSeq()
            else:
                self.savedDeflineStringQual = self.readMultiLineSeq()
        else:
            self.seqOrig = self.handle.readline()
            
        self.seq = self.seqOrig.strip().upper()
        
        if self.savedDeflineString or self.checkSeqQual or self.setClips:
            while True:
                self.savedDeflineString = ''
                seqIsValid = self.seqParser.parseSeq( self.seqOrig.strip() )
                
                if not self.seq:
                    break
                
                elif seqIsValid:
                    if self.setClips:
                        self.clipLeft = self.seqParser.clipLeft
                        self.clipRight = self.seqParser.clipRight
                    break

                else:
                    attemptCount += 1
                    if attemptCount > 1000:
                        self.statusWriter.outputErrorAndExit("Failed to find valid seq after 1000 attempts near spot {} in file {} ... {}"
                                                        .format(self.spotCount,self.filename,self.seqOrig.strip() ))

                    if self.outputStatus:
                        self.statusWriter.outputWarning("Discarding this line in readSeq while looking for a valid complete spot near spot {} in file {} ... {}"
                                                        .format(self.spotCount,self.filename,self.seqOrig.strip() ))
                    self.findValidDefline(True)
                    
                    if self.isMultiLine:
                        if ( self.isFasta or
                             self.isSplitSeqQual ):
                            self.savedDeflineStringSeq = self.readMultiLineSeq()
                        else:
                            self.savedDeflineStringQual = self.readMultiLineSeq()
                    else:
                        self.seqOrig = self.handle.readline()
                        
                    self.seq = self.seqOrig.strip().upper()

        return len ( self.seq )

    ############################################################
    # Process qual defline string (can come from a different file)
    ############################################################
    
    def processQualDefline (self,handle):
        if self.savedDeflineStringQual:
            self.deflineStringQual = self.savedDeflineStringQual
            self.savedDeflineStringQual = ''
        else:
            self.deflineStringQual = handle.readline().strip()

    ############################################################
    # Read qual (possibly from separate file)
    ############################################################
    
    def readQual ( self, handle, defline ):
        
        if self.isMultiLine:
            # Capture defline that follows multi-line qual value
            if self.isSplitSeqQual:
                self.savedDeflineStringQual = self.readMultiLineQual( handle, defline )
            else:
                self.savedDeflineStringSeq = self.readMultiLineQual( handle, defline )
        else:
            self.qualOrig = handle.readline()

        self.qual = self.qualOrig.rstrip() # leave leading space here; just strip trailing spaces

        if self.checkSeqQual:
            while True:
                qualIsValid = self.qualParser.parseQual(self.qualOrig.strip(), self.length) # remove leading/trailing spaces here
                if ( not self.qual or
                     qualIsValid ):
                    if self.qualParser.isNumQual:
                        self.isNumQual = True # In case it's not already set
                    break

                else:
                    if self.outputStatus:
                        self.statusWriter.outputWarning("Discarding this line in readQual while looking for a valid complete spot near spot {} in file {} ... {}"
                                                        .format(self.spotCount,self.filename,self.qual) )
                    self.findValidDefline(True)
                    self.length = self.readSeq()
                    self.processQualDefline(handle)
                    
                    if self.isMultiLine:
                        if self.isSplitSeqQual:
                            self.savedDeflineStringQual = self.readMultiLineQual(handle, defline)
                        else:
                            self.savedDeflineStringSeq = self.readMultiLineQual(handle, defline)
                    else:
                        self.qualOrig = handle.readline()

                    self.qual = self.qualOrig.rstrip()

        if self.isNumQual:
            lengthQual = len ( self.qual.split() )
        else:
            lengthQual = len ( self.qual )

        if ( lengthQual != self.length and
             self.qual[0:1] == '"' and
             not self.qual[1:2] == '"' and
             self.qual[lengthQual-1] == '"' ):
            self.qual = self.qual[1:lengthQual-1]
            lengthQual -= 2

        return lengthQual

    ############################################################
    # Read seq occurring on multiple lines
    ############################################################
    
    def readMultiLineSeq (self):
        self.seqLineCount = 0
        self.seqOrig = ''
        
        while True:
            fileStringOrig = self.handle.readline()
            fileString = fileStringOrig.strip()

            # At EOF/20000 lines and did not find next defline

            if not fileStringOrig:
                return ''

            elif self.seqLineCount > 20000:
                self.statusWriter.outputErrorAndExit("Exceeded 20000 sequence lines between deflines near spot {} in file {}"
                                                     .format(self.spotCount,self.filename) )

            # Check for defline. Note that deflineCharQual and deflineCharSeq are the same for seqQual fastq.

            elif ( ( self.isFasta and
                     fileString[0:1] == self.deflineCharSeq ) or
                   fileString[0:1] == self.deflineCharQual ):
                return fileString

            # Collect seq
            
            else:
                self.seqLineCount += 1
                self.seqOrig += fileString

    ############################################################
    # Read multi-line qual (handle can be same or separate file)
    ############################################################
    
    def readMultiLineQual ( self, handle, defline ):
        self.qualLineCount = 0
        self.qualOrig = ''
        
        while True:
            fileStringOrig = handle.readline()
            fileString = fileStringOrig.rstrip()

            # At EOF/20000 lines and did not find next read defline

            if not fileStringOrig:
                return ''

            elif self.qualLineCount > 1000:
                self.statusWriter.outputErrorAndExit("Exceeded 1000 quality lines between deflines near spot {} in file {}\n"
                                                     .format(self.spotCount,self.filename))
                
            # Check for next seq or qual defline (deflineCharSeq and deflineCharQual is the same for seqQual fastq)
            # isdigit checks for numerical-only names (extra stuff on the defline will break this)
            
            elif ( ( self.deflineCharSeq == '@' and
                     not defline.saveDeflineType ) or
                   ( defline.deflineType != defline.UNDEFINED and
                     fileString[0:1] == self.deflineCharSeq and
                     self.deflineCheck.parseDeflineString(fileString) ) or
                   ( defline.deflineType == defline.UNDEFINED and
                     ( fileString[0:2] == self.deflineStringSeq[0:2] or
                       ( fileString[1:].isdigit() and
                         self.deflineStringSeq[1:].isdigit() ) ) and
                     self.deflineCheck.parseDeflineString(fileString) ) ):
                return fileString

            # Account for wrapped numerical quality

            if ( ( self.isNumQual or
                   " " in fileString ) and
                 fileString[0:1] != " " ):
                self.qualOrig += " "
                self.isNumQual = True

            # Collect qual
            
            self.qualLineCount += 1
            self.qualOrig += fileString

    ############################################################
    # Find valid defline
    ############################################################
    
    def findValidDefline (self,callProcessSeqDefline):
        count = 0
        while True:
            count += 1
            if self.outputStatus:
                self.statusWriter.outputWarning("Discarding this line in findValidDefline while looking for a valid complete spot near spot {} in file {} ... {}"
                                                .format(self.spotCount,self.filename,self.defline.deflineString) )

            deflineStringSeqOrig = self.handle.readline()
            self.deflineStringSeq = deflineStringSeqOrig.strip()
            self.defline.parseDeflineString( self.deflineStringSeq )

            if self.defline.isValid:
                self.savedDeflineString = self.deflineStringSeq
                if callProcessSeqDefline:
                    self.processSeqDefline()
                break
            elif ( count == 1000 or
                   not deflineStringSeqOrig ):
                self.statusWriter.outputErrorAndExit( "Unable to parse defline in 1000 lines or before eof ... \n\tlast line ... {}\n\tfilename ... {}\n\tspotCount ... {}"
                                                      .format( self.defline.deflineString,self.filename,str(self.spotCount) ) )

    ############################################################
    # Discard defline type if mixed deflines in a single file
    ############################################################

    def discardDeflineType (self):
        self.defline.saveDeflineType = False
        self.deflineCheck.saveDeflineType = False
        if self.deflineQual:
            self.deflineQual.saveDeflineType = False

    ############################################################
    # Ignore leading defline characters for pair determination
    ############################################################

    def ignoreLeadDeflineChars ( self, ignLeadCharsNum ):
        self.defline.ignLeadCharsNum = ignLeadCharsNum
        self.deflineCheck.ignLeadCharsNum = ignLeadCharsNum
        if self.deflineQual:
            self.deflineQual.ignLeadCharsNum = ignLeadCharsNum

    ############################################################
    # Update spotcount or eof based on quality and defline name
    ############################################################

    def updateSpotCountOrEof (self):
        if ( self.qualOrig or
             self.defline.name ):
            self.spotCount += 1
        else:
            self.eof = True

    ############################################################
    # Adjust for CS
    ############################################################

    def adjustForCS (self):
        self.csKey = self.seq[0:1]
        self.seq = self.seq[1:]
        self.length -= 1
        space = self.qual.find(' ')
        if self.extraQualForCSkey:
            if space != -1:
                self.qual = self.qual[space+1:]         
            else:
                self.qual = self.qual[1:]
            self.lengthQual -= 1
        if ( space != -1 and
             self.qual.find("-1") != -1 ):
            self.qual = self.qual.replace("-1","0")

    ############################################################
    # Start reading from front of fastq again
    ############################################################

    def restart (self):
        self.handle.seek(0)
        self.processHeader(self.handle,self.defline)
        if self.qualHandle:
            self.qualHandle.seek(0)
            self.processHeader (self.qualHandle,self.deflineQual)
            if ( self.defline.abiTitle and
                 not self.deflineQual.abiTitle ):
                self.defline.abiTitle = ''
            elif ( self.deflineQual.abiTitle
                   and not self.defline.abiTitle ):
                self.deflineQual.abiTitle = ''
        self.eof = False
        self.spotCount = 0
        self.deflineStringSeq = ''
        self.deflineStringQual = ''
        self.seqQualDeflineMismatch = False
        self.savedDeflineString = ''
        self.savedDeflineStringSeq = ''
        self.savedDeflineStringQual = ''
        self.checkSeqQual = True
        
    ############################################################
    # Skip lines at start of file beginning with '#'
    ############################################################

    @staticmethod
    def processHeader (handle,defline):
        headerLineCount = 0
        while True:
            prevPos = handle.tell()
            fileString = handle.readline()
            if fileString[0:1] != "#":
                handle.seek(prevPos)
                break
            else:
                headerLineCount+=1

                # Check for abi title
                
                if ( not defline.abiTitle and
                     re.match("# Title: ([!-~]+)",fileString) ):
                    m = re.match("# Title: ([!-~]+)",fileString)
                    defline.abiTitle = m.group(1)
                    if re.search ( "_$", defline.abiTitle ):
                        defline.abiTitle = defline.abiTitle[0:len(defline.abiTitle)-1]
					
        return headerLineCount

    ############################################################
    # Check if fastq file contains multi-line fastq
    ############################################################
    
    def isMultiLineFastq ( self ):
        self.seqLineCount = 0
        self.qualLineCount = 0

        self.restart()
        maxSeqLineCount = 0
        maxQualLineCount = 0
        while ( self.spotCount < 1001 and
                not ( self.qualOrig == "" or
                      self.seqLineCount > 20000 or
                      self.qualLineCount > 20000 ) ):
            self.read()
            if self.seqLineCount > maxSeqLineCount:
                maxSeqLineCount = self.seqLineCount
            if self.qualLineCount > maxQualLineCount:
                maxQualLineCount = self.qualLineCount
            
        self.restart()

        if ( 1 < maxSeqLineCount <= 20000 or
             1 < maxQualLineCount <= 20000 ):
            return True
             
        elif ( maxSeqLineCount > 20000 or
               maxQualLineCount > 20000 ):
            self.statusWriter.outputErrorAndExit( "Distance between fastq deflines exceeds 20000 lines in file ... {}".format(self.filename) )

        else:
            return False

############################################################
# Split seq/qual Fastq Reader (i.e. separate seq and qual files)
############################################################

class SeqQualFastqReader (FastqReader):
    """ Split seq/qual variation on FastqReader """

    def __init__(self,filename, handle, qualFilename, qualHandle):
        FastqReader.__init__(self, filename, handle)
        qualHandle.seek(0)
        self.isSplitSeqQual = True
        self.deflineCharSeq = ">"
        self.deflineCharQual = ">"
        self.qualFilename = qualFilename
        self.qualHandle = qualHandle
        self.deflineQual = Defline( '' )
        self.deflineQual.filename = self.qualFilename
        self.headerLineCountQual = self.processHeader(qualHandle,self.deflineQual)

    ############################################################
    # Process seq defline
    ############################################################
    
    def processSeqDefline (self):
        
        # Determine which string to used for defline
        
        if not self.savedDeflineStringSeq:
            self.deflineStringSeq = self.handle.readline()
            if self.deflineStringSeq:
                self.deflineStringSeq = self.deflineStringSeq.strip()
            else:
                return
        else:
            self.deflineStringSeq = self.savedDeflineStringSeq
            self.savedDeflineStringSeq = ''

        # Parse defline and confirm validity
        
        self.defline.parseDeflineString ( self.deflineStringSeq )
        if not self.defline.isValid:
            if self.outputStatus:
                self.statusWriter.outputErrorAndExit("Unable to parse defline ... filename,spotCount,deflineString\t{},{},{}"
                                                     .format(self.filename,self.spotCount,self.defline.deflineString))

    ############################################################
    # Process qual defline
    ############################################################
    
    def processQualDefline ( self, handle ):
        if self.savedDeflineStringQual:
            self.deflineStringQual= self.savedDeflineStringQual
            self.savedDeflineStringQual = ''
        else:
            self.deflineStringQual = handle.readline()

        if self.deflineStringQual: # Qual file can end early
            self.deflineStringQual = self.deflineStringQual.strip()
            self.deflineQual.parseDeflineString( self.deflineStringQual )
            if not self.deflineQual.isValid:
                testRead = handle.readline() # see if we are at EOF for quality
                if ( not testRead and
                     self.deflineStringQual in self.deflineStringSeq): # If at EOF for truncated quality file
                    return
                else:
                    self.statusWriter.outputErrorAndExit( "Unable to parse quality defline ... filename,spotCount,deflineString\t{},{},{}"
                                                          .format(self.qualFilename,self.spotCount,self.deflineQual.deflineString) )
            if ( not self.ignoreNames and
                 not self.discardNames and
                 self.defline.name != self.deflineQual.name ):
                self.seqQualDeflineMismatch = True

    ############################################################
    # Check if two handles represent seq/qual fastq pair
    ############################################################
    
    def isSeqQualFastq (self):
        self.restart()
        self.checkSeqQual = False
        self.read()
        self.restart()

        parsedSeq1 = Seq(self.seq)
        parsedQual1 = Qual(self.seq,parsedSeq1.length)
        parsedSeq2 = Seq(self.qual)
        parsedQual2 = Qual(self.qual,parsedSeq2.length)

        if Defline.isPairedDeflines ( self.defline, self.deflineQual, True ):
            if parsedSeq1.isValid and parsedQual2.isValid:
                return 1
            elif parsedSeq2.isValid and parsedQual1.isValid:
                return 2

        return False

    ############################################################
    # Update spotcount or eof based on sequence and defline name
    ############################################################

    def updateSpotCountOrEof (self):
        if ( self.defline.name and
             self.seqOrig ):
            self.spotCount += 1
        else:
            self.eof = True

############################################################
# Fasta only Fastq Reader with generated qual
############################################################

class FastaFastqReader (FastqReader):
    """ Fasta-only variation loaded as Fastq """

    def __init__(self,filename,handle):
        FastqReader.__init__(self, filename, handle)
        self.deflineCharSeq = ">"
        self.isFasta = True

    ############################################################
    # Read from fasta handle, fabricate quality
    ############################################################
    
    def read (self):

        # Process seq defline

        self.processSeqDefline()

        # Read seq

        self.length = self.readSeq()

        # Generate qual, increment spot count, and check for eol
        
        self.updateQualAndSpotCountOrEof()

        # Adjust for color space

        if self.isColorSpace:
            self.adjustForCS()

    ############################################################
    # Check if file is fasta
    ############################################################
    
    def isFastaFastq(self):
        self.restart()
        self.checkSeqQual = False
        self.read()
        parsedSeq = Seq(self.seq)
        self.read()
        parsedSeq2 = Seq(self.seq)
        self.restart()
        return parsedSeq.isValid and parsedSeq2.isValid

    ############################################################
    # Update qual/spotcount or eof 
    ############################################################

    def updateQualAndSpotCountOrEof (self):
        if ( not self.defline.name and
             not self.seqOrig ):
            self.seq = ''
            self.qualOrig = ''
            self.qual = ''
            self.eof = True
        else:
            self.qualOrig = self.defaultQual * self.length
            self.qual = self.qualOrig
            self.lengthQual = self.length
            self.spotCount += 1

    ############################################################
    # Check if fasta file contains multi-line sequence
    ############################################################
    
    def isMultiLineFasta ( self ):
        self.seqLineCount = 0
        maxSeqLineCount = 0
        
        self.restart()
        while ( self.spotCount < 1001 and
                not ( self.seqOrig == "" or
                      self.seqLineCount > 20000 ) ):
            self.read()
            if self.seqLineCount > maxSeqLineCount:
                maxSeqLineCount = self.seqLineCount
        self.restart()

        if 1 < maxSeqLineCount <= 20000:
            return True

        elif maxSeqLineCount > 20000:
            self.statusWriter.outputErrorAndExit( "Distance between fasta deflines exceeds 20000 lines in file ... {}".format(self.filename) )

        else:
            return False

############################################################
# Single Line Fastq Reader Class
############################################################

class SingleLineFastqReader (FastqReader):
    """ Single line variation on FastqReader (colon-separated fields) """

    def __init__(self,filename,handle):
        FastqReader.__init__(self, filename, handle)
        self.deflineCharSeq = ''
        self.deflineChunks = []
        self.fileString = ''
        self.delim = ":"

    ############################################################
    # Read/parse single line fastq from file
    ############################################################
    
    def read (self):

        # Read line from file

        fileStringOrig = self.handle.readline()
        self.fileString = fileStringOrig.strip()

        # Check for EOF

        if not fileStringOrig :
            self.seqOrig = ''
            self.seq = ''
            self.qualOrig = ''
            self.qual = ''
            self.defline.reset()

        # Split file line into chunks
            
        else:
            self.deflineChunks = self.fileString.split(self.delim)

            # Process qual chunk
            
            self.lengthQual = self.readQual( self.handle, None )

            # Process seq chunk
            
            self.length = self.readSeq()

            # Rebuild/parse defline string, confirming validity

            self.processSeqDefline()

        # Increment spot count and check for eol
        
        self.updateSpotCountOrEof()

    ############################################################
    # Process seq defline
    ############################################################
    
    def processSeqDefline (self):
        self.deflineStringSeq = self.delim.join(self.deflineChunks)
        self.defline.parseDeflineString( self.deflineStringSeq )
        if not self.defline.isValid:
            self.findValidDefline(False)

    ############################################################
    # Read seq and convert to uppercase
    ############################################################
    
    def readSeq (self):
        self.seqOrig = self.deflineChunks.pop()
        self.seq = self.seqOrig.upper()
        return len(self.seq)

    ############################################################
    # Read qual
    ############################################################
    
    def readQual (self,handle,defline):
        self.qualOrig = self.deflineChunks.pop()
        self.qual = self.qualOrig
        if self.isNumQual:
            return len ( self.qual.split() )
        else:
            return len ( self.qual )

    ############################################################
    # Find valid defline
    ############################################################
    
    def findValidDefline (self,callProcessSeqDefline):
        count = 0
        while True:
            count += 1
            if self.outputStatus:
                self.statusWriter.outputWarning("Discarding this line in SingleLineFastqReader findValidDefline while looking for a valid complete spot near spot {} in file {} ... {}"
                                                .format(self.spotCount,self.filename,self.fileString) )
            fileStringOrig = self.handle.readline()
            self.fileString = fileStringOrig.strip()
            self.deflineChunks = self.fileString.split( self.delim )
            self.lengthQual = self.readQual( self.handle, None )
            self.length = self.readSeq()
            self.deflineStringSeq = self.delim.join( self.deflineChunks )
            self.defline.parseDeflineString( self.deflineStringSeq )
            if self.defline.isValid:
                break
            elif ( count == 1000 or
                   not fileStringOrig ):
                self.statusWriter.outputErrorAndExit( "Unable to parse defline in 1000 lines or before eof ... \n\tlast line ... {}\n\tfilename ... {}\n\tspotCount ... {}"
                                                      .format(self.defline.deflineString,self.filename,self.spotCount) )

    ############################################################
    # Check if fastq file contains single line fastq
    ############################################################
    
    def isSingleLineFastq (self):
        self.restart()
        self.checkSeqQual = False
        self.read()
        self.restart()
        parsedSeq = Seq(self.seq)
        parsedQual = Qual( self.qual, parsedSeq.length )
        if ( parsedSeq.isValid and
             parsedQual.isValid and
             parsedSeq.length == parsedQual.length ):
            return True
        else:
            return False

############################################################
# Class for writing fastq-based spots to SRA archive
############################################################

class FastqSpotWriter():
    """ Container for collecting/writing fasta spot information to gw """

    READ_TYPE_TECHNICAL                 = 0
    READ_TYPE_BIOLOGICAL                = 1
    READ_TYPE_GROUP                     = 2

    def __init__(self):

        self.readCount = 0
        self.readCountNonGroup = 0
        self.readLengths = []
        self.readStarts = []
        self.readTypes = array.array('B', [1, 1])
        self.readFilters = array.array('B', [ 0, 0 ])
        self.platform = array.array('B', [0])
        self.platformString = ''
        
        self.clipQualityLeft = array.array('I', [0])
        self.clipQualityRight = array.array('I', [0])
        self.labels = ''
        self.firstLabelRE = ''
        self.labelStarts = array.array('I', [ 0, 0 ])
        self.labelLengths = array.array('I', [ 0, 0 ])
        self.offset = 33
        self.minQual = 1000
        self.maxQual = 0
        self.isNumQual = False
        self.orphanReads = False
        self.dumpOrphans = False
        self.dumpOrphans2D = False
        self.pairedRead1 = {}
        self.pairedRead2 = {}
        self.nanopore2Dread = {}
        self.nanopore2Donly = False
        self.read1PairFiles = None
        self.read2PairFiles = None
        self.read1QualFiles = None
        self.read2QualFiles = None
        self.spotGroup = ''
        self.spotGroupsFound = {}
        self.discardBarcodes = False
        self.ignoreNames = False
        self.discardNames = False
        self.useAndDiscardNames = False
        self.nameCount = 0
        self.isEightLine = False
        self.mixedDeflines = False
        self.setClips = False

        self.dst = None
        self.dst2D = None

        self.lengthsProvided = False
        self.variableLengthSpecified = False
        self.labelsProvided = False
        self.spotGroupProvided = False
        self.offsetProvided = False
        self.pairFilesProvided = False
        self.qualFilesProvided = False

        self.workDir = None
        self.outdir = "out.sra"
        self.finalDest = ''
        self.spotCount = 0
        
        self.isColorSpace = False       # ABI fastq rather than base-based fastq -
                                        # handled by abi-load currently; comment lines at the start
        self.extraQualForCSkey = False  # Set to True if extra qual value for cs key
        self.readColumn = 'READ'
        
        self.logOdds = False            # Indicated by presence of negative qualities
        self.changeNegOneQual = False   # '-1' only is likely used for dot or N qualities
        if (sys.version).startswith("3"):
            self.transNegOne = str.maketrans('?', '@')
        else:
            self.transNegOne = string.maketrans('?', '@')
        self.readNums = []

        self.gw = None
        self.db = 'NCBI:SRA:GenericFastq:db'
        self.schema = 'sra/generic-fastq.vschema'
        self.maxErrorCount = 100000
        self.infoSizeForSpotCount = 100000
        self.profile = False
        self.checkSeqQual = True
        self.statusWriter = None
        self.ignLeadCharsNum = None

        ############################################################
        # Define SEQUENCE table components for general loader
        # and create instance of General Writer
        ############################################################
        
        self.tbl = {
            'SEQUENCE': {
                'READ': {
                    'expression': '(INSDC:dna:text)READ',
                    'elem_bits': 8
                    },
                'CSREAD': {
                    'expression': '(INSDC:color:text)CSREAD',
                    'elem_bits': 8
                    },
                'CS_KEY': {
                    'expression': '(INSDC:dna:text)CS_KEY',
                    'elem_bits': 8
                    },
                'READ_START': {
                    'expression': '(INSDC:coord:zero)READ_START',
                    'elem_bits': 32
                    },
                'READ_LENGTH': {
                    'expression': '(INSDC:coord:len)READ_LEN',
                    'elem_bits': 32
                    },
                'READ_TYPE': {
                    'expression': '(U8)READ_TYPE',
                    'elem_bits': 8,
                    'default': array.array('B', [ 1, 1 ])
                    },
                'READ_FILTER': {
                    'expression': '(U8)READ_FILTER',
                    'elem_bits': 8,
                    'default': array.array('B', [ 0, 0 ])
                    },
                'QUALITY': {
                    'expression': '(INSDC:quality:text:phred_33)QUALITY',
                    'elem_bits': 8
                    },
                'NAME': {
                    'expression': '(ascii)NAME',
                    'elem_bits': 8,
                    'default': ' '.encode('ascii')
                    },
                'SPOT_GROUP': {
                    'elem_bits': 8,
                    'default': ''.encode('ascii')
                    },
                'CLIP_QUALITY_LEFT': {
                    'expression': '(INSDC:coord:one)CLIP_QUALITY_LEFT',
                    'elem_bits': 32,
                    'default': array.array('I', [0])
                    },
                'CLIP_QUALITY_RIGHT': {
                    'expression': '(INSDC:coord:one)CLIP_QUALITY_RIGHT',
                    'elem_bits': 32,
                    'default': array.array('I', [0])
                    },
                'LABEL': {
                    'elem_bits': 8,
                    'default': ''.encode('ascii')
                    },
                'LABEL_START': {
                    'elem_bits': 32,
                    'default': array.array('I', [ 0, 0 ])
                    },
                'LABEL_LEN': {
                    'elem_bits': 32,
                    'default': array.array('I', [ 0, 0 ])
                    },
                'PLATFORM': {
                    'expression': '(U8)PLATFORM',
                    'elem_bits': 8,
                    'default': array.array('B', [0])
                    },
                'CHANNEL': {
                    'elem_bits': 32,
                    'default': array.array('I', [0])
                    },
                'READ_NO': {
                    'expression': 'READ_NUMBER',
                    'elem_bits': 32,
                    'default': array.array('I', [0])
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
                'NAME': {
                    'expression': '(ascii)NAME',
                    'elem_bits': 8,
                    'default': ' '.encode('ascii')
                    },
                'SPOT_GROUP': {
                    'elem_bits': 8
                    },
                'CHANNEL': {
                    'elem_bits': 32
                    },
                'READ_NO': {
                    'expression': 'READ_NUMBER',
                    'elem_bits': 32,
                    'default': array.array('I', [0])
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
                    'elem_bits': 8,
                    'default': array.array('B', [1])
                    },
                'READ_FILTER': {
                    'expression': '(U8)READ_FILTER',
                    'elem_bits': 8,
                    'default': array.array('B', [0])
                    },
                'LABEL': {
                    'elem_bits': 8,
                    'default': '2D'.encode('ascii')
                    },
                'LABEL_START': {
                    'elem_bits': 32,
                    'default': array.array('I', [0])
                    },
                'LABEL_LEN': {
                    'elem_bits': 32,
                    'default': array.array('I', [2])
                    },
                }
            }

    ############################################################
    # Open general writer for output to general loader
    ############################################################
    
    def openGeneralWriter(self):

        # Address nanopore possibilities
        
        if self.platformString == "NANOPORE":
            if self.nanopore2Donly:
                self.db = 'NCBI:SRA:GenericFastqNanopore:db' # still need sequence table
            else:
                self.setReadLabels("template,complement")
                self.orphanReads = True
                self.db = 'NCBI:SRA:GenericFastqNanopore:db'
        else:
            self.removeNanoporeColumns()
            del self.tbl['CONSENSUS']

        # Address color space possibilities
            
        if self.isColorSpace:
            self.db = 'NCBI:SRA:GenericFastqAbsolid:db'
            self.removeNonColorSpaceColumns()
            self.readColumn = 'CSREAD'

        elif 'SEQUENCE' in self.tbl:
            self.removeColorSpaceColumns()

        # Address other special cases (not overlapping with nanopore and color space)

        if self.logOdds:
            self.db = 'NCBI:SRA:GenericFastqLogOdds:db'

        elif ( self.discardNames or
               self.useAndDiscardNames ):
            self.db = 'NCBI:SRA:GenericFastqNoNames:db'

        # Remove label and clip columns if necessary

        if 'SEQUENCE' in self.tbl:
            
            # Remove labels from SEQUENCE table if not provided

            if not self.labels:
                self.removeLabelColumns()

            # Remove clips from SEQUENCE table if not needed

            if not self.setClips:
                self.removeClipColumns()

        # Set destinations
        
        if 'CONSENSUS' in self.tbl:
            self.dst2D = self.tbl['CONSENSUS']
            
        if 'SEQUENCE' in self.tbl:
            self.dst = self.tbl['SEQUENCE']

        # Open writer

        self.gw = GeneralWriter.GeneralWriter( self.outdir,
                                               self.schema,
                                               self.db,
                                               'fastq-load.py',
                                               '1.0.0',
                                               self.tbl )

    ############################################################
    # Set values that are hopefully consistent across a file
    ############################################################
    
    def setUnchangingSpotValues(self,fastq1,fastq2):

        if self.platformString:
            self.dst['PLATFORM']['data'] = self.platform

        # Handle recognized nanopore data somewhat differently
        
        if self.platformString == "NANOPORE" :
            if self.spotCount == 0:
                self.dst2D['READ_START']['data'] = array.array( 'I', [ 0 ] )

        # Orphan reads trump setting constant values

        elif self.orphanReads:
            pass

        # Paired files
        
        elif fastq2 :
            if self.readTypes[0] == self.READ_TYPE_TECHNICAL:
                self.dst['READ_TYPE']['data'] = array.array( 'B', [ 0, 1 ] )
            elif self.readTypes[1] == self.READ_TYPE_TECHNICAL:
                self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1, 0 ] )
            else:
                self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1, 1 ] )

        # Fragment file
        
        elif not self.lengthsProvided :
            self.dst['READ_START']['data'] = array.array( 'I', [ 0 ] )
            self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1 ] )

        # Lengths provided
        
        else:
            self.dst['READ_START']['data'] = array.array( 'I', [] )
            self.dst['READ_LENGTH']['data'] = array.array( 'I', [] )
            self.dst['READ_TYPE']['data'] = array.array( 'B', [] )
            readIndex = -1
            readStart = 0
            for readType in self.readTypes:
                readIndex += 1
                if readType != self.READ_TYPE_GROUP:
                    self.dst['READ_START']['data'].append( readStart )
                    self.dst['READ_LENGTH']['data'].append( self.readLengths[readIndex] )
                    self.dst['READ_TYPE']['data'].append( self.readTypes[readIndex] )
                    readStart += self.readLengths[readIndex]

        # Can be called twice if fastq1 or fastq2 ends early
        # Execute the following lines only once

        if self.spotCount == 0:
            
            if self.labels :
                self.dst['LABEL']['data'] = self.labels
                self.dst['LABEL_START']['data'] = self.labelStarts
                self.dst['LABEL_LEN']['data'] = self.labelLengths
            
            if self.mixedDeflines:
                fastq1.discardDeflineType()
                if fastq2:
                    fastq2.discardDeflineType()
            
    ############################################################
    # Selects write function for handling fastq data
    ############################################################
    
    def writeSpot ( self, fastq1, fastq2, fastq3 ):
            
        # Very occasionally submitted file contains empty seq/qual for a spot.
        # Also if a mismatch exists between seq and qual, the seq and qual are
        # set to empty strings

        if ( not fastq1.seq and
             ( fastq2 is None or
               not fastq2.seq ) ):
            pass

        # Process nanopore spots

        elif self.platformString == "NANOPORE":
            if self.nanopore2Donly:
                self.write2Dread ( fastq1, None )
            elif ( self.dumpOrphans or
                   self.dumpOrphans2D ) :
                self.processOrphanNanoporeReads ( fastq1 )
            else:
                self.processNanoporeReads ( fastq1, fastq2, fastq3 )

        # Pre-split 454 would be handled here. Assumes no 454 adapter
        # is present (i.e. assuming no sub-division of sequence in each
        # pair file)

        elif self.orphanReads:
            if self.dumpOrphans:
                self.processOrphanReads ( fastq1 )
            else:
                self.processMixedReads ( fastq1, fastq2 )

        else:

            # Second fastq indicates paired fastq files

            if fastq2 :
                if ( not ( self.ignoreNames or self.discardNames ) and
                     fastq1.defline.name != fastq2.defline.name ):
                    self.statusWriter.outputErrorAndExit("Expected paired deflines but encountered name mismatch ... {},{}"
                                                         .format(fastq1.defline.name,fastq2.defline.name) )
                else:
                    self.processPairFastqSpot ( fastq1, fastq2 )

            # If fastq1 is at eof then just return

            elif fastq1.eof:
                return

            # Single file without lengths provided processed as fragments
        
            elif not self.lengthsProvided :
                self.processFragmentFastqSpot ( fastq1 )

            # Single file with lengths provided processed as multi-read single file

            else:
                self.processMultiReadSingleFastqSpot ( fastq1 )

            # Set common columns and track spotCount

            self.spotCount += 1
            self.setDstName ( fastq1, self.dst )
            self.setDstSpotGroup ( fastq1, fastq2, self.dst )
        
            # Output spotcount

            if fastq1.spotCount % self.infoSizeForSpotCount == 0:
                self.outputCount(fastq1,fastq2,False)

            # Edge case where one seq in a pair is an empty string (very sparse)

            if fastq2 :
                if not fastq1.seq:
                    self.dst['READ_TYPE']['data'] = array.array( 'B', [ 0, 1 ] )
                elif not fastq2.seq:
                    self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1, 0 ] )

            # Finally write to general writer

            self.gw.write(self.dst)

            # Recover from edge case

            if ( fastq2 and
                 ( not fastq1.seq or
                   not fastq2.seq ) ):
                self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1, 1 ] )

    ############################################################
    # Process spot from pair of fastq files
    ############################################################
    
    def processPairFastqSpot ( self, fastq1, fastq2 ):
        self.dst[self.readColumn]['data'] = (fastq1.seq + fastq2.seq).encode('ascii')
        if self.isColorSpace:
            self.dst['CS_KEY']['data'] = (fastq1.csKey + fastq2.csKey).encode('ascii')
        self.dst['READ_START']['data'] = array.array( 'I', [ 0, len(fastq1.seq) ] )
        self.dst['READ_LENGTH']['data'] = array.array( 'I', [ len(fastq1.seq), len(fastq2.seq) ] )
        
        if ( fastq1.defline.filterRead or
             fastq2.defline.filterRead ):
            self.dst['READ_FILTER']['data'] = array.array('B', [ 1, 1 ] )
        else:
            self.dst['READ_FILTER']['data'] = array.array('B', [ 0, 0 ] )

        # Put space in so subsequent split on white space works and two qual
        # values are not side-by-side
        
        if ( self.isNumQual and
             fastq1.qual and
             fastq2.qual ):
            fastq1.qual += " "
        self.setDstQual ( fastq1.qual + fastq2.qual, self.dst )

    ############################################################
    # Process fragment from single fastq file
    ############################################################
    
    def processFragmentFastqSpot ( self, fastq1 ):
        self.dst[self.readColumn]['data'] = fastq1.seq.encode('ascii')
        if self.isColorSpace:
            self.dst['CS_KEY']['data'] = fastq1.csKey.encode('ascii')
        self.dst['READ_LENGTH']['data'] = array.array( 'I', [ len(fastq1.seq) ] )
        self.dst['READ_FILTER']['data'] = array.array('B', [ fastq1.defline.filterRead ] )
        self.setDstQual ( fastq1.qual, self.dst )

    ############################################################
    # Process multiple reads from single fastq file
    # Reads lengths provided and read starts calculated already
    ############################################################
    
    def processMultiReadSingleFastqSpot ( self, fastq1 ):
        clipQualityRight = 0
        if self.readCount == self.readCountNonGroup:
            self.dst['READ']['data'] = fastq1.seq.encode('ascii')
            self.setDstQual ( fastq1.qual, self.dst )
            if self.setClips:
                clipQualityRight = len(fastq1.seq) - fastq1.clipRight
            if self.variableLengthSpecified :
                readLengthsLocal = array.array('I', [0] * self.readCount)
                readLengthsLocal[0] = self.readLengths[0]
                if self.readCount == 2:
                    readLengthsLocal[1] = len(fastq1.seq) - self.readLengths[0]
                elif self.readCount == 3:
                    readLengthsLocal[1] = self.readLengths[1]
                    readLengthsLocal[2] = len(fastq1.seq) - self.readLengths[0] - self.readLengths[1]
                elif self.readCount == 4:
                    readLengthsLocal[1] = self.readLengths[1]
                    readLengthsLocal[2] = self.readLengths[2]
                    readLengthsLocal[3] = len(fastq1.seq) - self.readLengths[0] - self.readLengths[1] - self.readLengths[2]
                self.dst['READ_LENGTH']['data'] = readLengthsLocal
        else:
            seq = ""
            qual = ""
            start = 0
            readIndex = -1
            readLengthsLocal = array.array('I', [] )
            for readType in self.readTypes:
                readIndex += 1
                if readType != self.READ_TYPE_GROUP:
                    seq += fastq1.seq[ start : start + self.readLengths[readIndex] ]
                    qual += fastq1.qual[ start : start + self.readLengths[readIndex] ] # Does not work with numerical qual :(               
                    if self.readLengths[readIndex] == 0: # last read only currently
                        readLengthsLocal.append( len ( fastq1.seq[start:] ) )
                    else:
                        readLengthsLocal.append(self.readLengths[readIndex])
                start += self.readLengths[readIndex]
            self.dst['READ']['data'] = seq.encode('ascii')
            self.setDstQual ( qual, self.dst )
            if self.setClips:
                clipQualityRight = len(seq) - fastq1.clipRight
            self.dst['READ_LENGTH']['data'] = readLengthsLocal

        self.dst['READ_FILTER']['data'] = array.array('B', [fastq1.defline.filterRead] * self.readCountNonGroup)

        if self.setClips:
            clipQualityLeft = fastq1.clipLeft + 1
            self.dst['CLIP_QUALITY_LEFT']['data'] = array.array( 'I', [ int(clipQualityLeft) ] )
            self.dst['CLIP_QUALITY_RIGHT']['data'] = array.array( 'I', [ int(clipQualityRight) ] )

    ############################################################
    # Process spots from nanopore fastq files (potential orphan reads)
    # Note that if a single file contains all three read types
    # then I need to assume 
    ############################################################
    
    def processNanoporeReads ( self, fastq1, fastq2, fastq3 ):

        if ( fastq2 and
             not fastq2.eof and
             fastq1.defline.name == fastq2.defline.name and
             fastq1.defline.poreRead != "2D" and
             fastq2.defline.poreRead != "2D"):
            self.setNanoporeColumns ( fastq1, self.dst )
            self.writeMixedPair ( fastq1, fastq2 )
            self.writeNanopore2Dread ( fastq1, fastq3 )
            
        elif ( fastq2 and
               not fastq2.eof and
               fastq3 and
               not fastq3.eof and
             fastq2.defline.name == fastq3.defline.name and
             fastq2.defline.poreRead != "2D" and
             fastq3.defline.poreRead != "2D"):
            if fastq1.defline.name != fastq2.defline.name:
                self.processUnpairedPoreRead ( fastq1, None )
            self.setNanoporeColumns ( fastq2, self.dst )
            self.writeMixedPair ( fastq2, fastq3 )
            if fastq1.defline.name == fastq2.defline.name:
                self.writeNanopore2Dread ( fastq2, fastq1 )
            else:
                self.writeNanopore2Dread ( fastq2, None )
            
        elif ( fastq3 and # unlikely but just in case
               not fastq3.eof and
               fastq1.defline.name == fastq3.defline.name and
               fastq1.defline.poreRead != "2D" and
               fastq3.defline.poreRead != "2D"):
            self.setNanoporeColumns ( fastq1, self.dst )
            self.writeMixedPair ( fastq1, fastq3 )
            self.writeNanopore2Dread ( fastq1, fastq2 )
            
        else:
            self.processUnpairedPoreRead ( fastq1, fastq3 )
            if fastq2 :
                self.processUnpairedPoreRead ( fastq2, None ) # 2D read in fastq3 handled/cached by prior call

        if fastq1.spotCount % self.infoSizeForSpotCount == 0:
            self.outputCount(fastq1,fastq2,True)

    ############################################################
    # If a read is unpaired then check for prior encounter with
    # the mate
    ############################################################
    
    def processUnpairedPoreRead (self, fastq, fastq2D ):

        if ( fastq.defline.poreRead == "2D" or
             ( not fastq.defline.name in self.pairedRead1 and
               not fastq.defline.name in self.pairedRead2 ) ):
            self.addToPoreReadHash( fastq )
            if ( fastq2D and
                 not fastq2D.eof ):
                self.addToPoreReadHash( fastq2D )

        else:
            self.setNanoporeColumns ( fastq, self.dst )
            if fastq.defline.name in self.pairedRead2:
                self.writeMixedRead1 ( fastq )              
            else:
                self.writeMixedRead2 ( fastq )
            self.writeNanopore2Dread ( fastq, fastq2D )

    ############################################################
    # Process orphan reads from nanopore fastq files (probably
    # template only)
    ############################################################
    
    def processOrphanNanoporeReads ( self, fastq1 ):

        if ( self.dumpOrphans and
             not fastq1.defline.poreRead == "2D" and
             ( fastq1.defline.name in self.pairedRead1 or
               fastq1.defline.name in self.pairedRead2 ) ) :
            self.setNanoporeColumns ( fastq1, self.dst )
            self.writeMixedOrphan ( fastq1 )
            self.writeNanopore2Dread ( fastq1, None )
        elif ( self.dumpOrphans2D and
               fastq1.defline.poreRead == "2D" and
               fastq1.defline.name in self.nanopore2Dread ) :
            self.writeFakeTemplateRead ( fastq1 )
            self.writeNanopore2Dread ( fastq1, None )
            
        if fastq1.spotCount % self.infoSizeForSpotCount == 0:
            self.outputCount(fastq1,None,True)
        
    ############################################################
    # Write actual or fake 2D read
    ############################################################
    
    def writeNanopore2Dread ( self, fastq, fastq2D ):

        readWritten = False
        if ( fastq2D and
             not fastq2D.eof ):
            
            if ( fastq2D.defline.poreRead != "2D" or
                 fastq.defline.name != fastq2D.defline.name):
                self.processUnpairedPoreRead ( fastq2D, None )
            else:
                self.write2Dread ( fastq2D, None )
                readWritten = True

        if not readWritten:
            read2D = None
            if fastq.defline.name in self.nanopore2Dread:
                read2D = self.nanopore2Dread [ fastq.defline.name ]
                
            if read2D:
                self.write2Dread ( fastq, read2D )
                del self.nanopore2Dread [ fastq.defline.name ]
            else:
                self.writeFake2Dread ( fastq )
    
    ############################################################
    # Write nanopore 2D reads to CONSENSUS table using either
    # values read only from 2D read file or combination from
    # template/complement file and save 2D read data
    ############################################################
    
    def write2Dread (self, fastq, read2D):
        
        self.setNanoporeColumns ( fastq, self.dst2D )
        self.dst2D['READ_FILTER']['data'] = array.array('B', [ fastq.defline.filterRead ] )
        self.dst2D['READ_TYPE']['data'] = array.array( 'B', [ 1 ] )
        if read2D:
            self.dst2D['READ']['data'] = read2D['seq'].encode('ascii')
            self.dst2D['READ_LENGTH']['data'] = array.array( 'I', [ len(read2D['seq']) ] )
            self.setDstQual ( read2D['qual'], self.dst2D )
        else:
            self.dst2D['READ']['data'] = fastq.seq.encode('ascii')
            self.dst2D['READ_LENGTH']['data'] = array.array( 'I', [ len(fastq.seq) ] )
            self.setDstQual ( fastq.qual, self.dst2D )
        self.setDstName ( fastq, self.dst2D )
        self.setDstSpotGroup ( fastq, None, self.dst2D )
        self.gw.write( self.dst2D )
        if self.nanopore2Donly:
            self.spotCount += 1
            if fastq.spotCount % self.infoSizeForSpotCount == 0:
                self.outputCount(fastq,None,False )

    ############################################################
    # Write technical nanopore template read to SEQUENCE table using
    # 2D fastq read values
    ############################################################
    
    def writeFakeTemplateRead (self, fastq):
        
        self.setNanoporeColumns ( fastq, self.dst )
        self.dst['READ_LENGTH']['data'] = array.array( 'I', [0] )
        self.dst['READ_FILTER']['data'] = array.array('B', [ fastq.defline.filterRead ] )
        self.dst['READ_TYPE']['data'] = array.array( 'B', [ 0 ] )
        self.dst['READ']['data'] = ''.encode('ascii')
        self.setDstQual ( '', self.dst )
        self.setDstName ( fastq, self.dst )
        self.setDstSpotGroup ( fastq, None, self.dst )
        self.gw.write( self.dst )

    ############################################################
    # Write technical nanopore 2D read to CONSENSUS table using
    # fastq read results from template or complement read
    ############################################################
    
    def writeFake2Dread (self, fastq):
        
        self.setNanoporeColumns ( fastq, self.dst2D )
        self.dst2D['READ_LENGTH']['data'] = array.array( 'I', [0] )
        self.dst2D['READ_FILTER']['data'] = array.array('B', [ fastq.defline.filterRead ] )
        self.dst2D['READ_TYPE']['data'] = array.array( 'B', [ 0 ] )
        self.dst2D['READ']['data'] = ''.encode('ascii')
        self.setDstQual ( '', self.dst2D )
        self.setDstName ( fastq, self.dst2D )
        self.setDstSpotGroup ( fastq, None, self.dst2D )
        self.gw.write( self.dst2D )

    ############################################################
    # Process spots from mixed fastq files (potential orphan reads)
    ############################################################
    
    def processMixedReads ( self, fastq1, fastq2 ):
        
        if ( fastq2 and
             fastq1.defline.name == fastq2.defline.name ):
            self.writeMixedPair ( fastq1, fastq2 )
        else:
            self.processUnpairedRead ( fastq1 )
            if fastq2 :
                self.processUnpairedRead ( fastq2 )

        if fastq1.spotCount % self.infoSizeForSpotCount == 0:
            self.outputCount (fastq1,fastq2,True)

    ############################################################
    # Write spot when read matches up with previously encountered reads
    # Otherwise retain/hash the read
    ############################################################
    
    def processUnpairedRead ( self, fastq ):
        
        if fastq.defline.name in self.pairedRead2:
            self.writeMixedRead1 ( fastq )
        elif fastq.defline.name in self.pairedRead1:
            self.writeMixedRead2 ( fastq )
        else:
            self.addToPairedReadHash ( fastq )
    
    ############################################################
    # Write spot where read1 and read2 in fastq
    ############################################################
    
    def writeMixedPair ( self, fastq1, fastq2 ):
        if ( ( fastq1.defline.readNum and
               fastq1.defline.readNum != "1" and
               fastq2.defline.readNum == "1" ) or
             ( fastq1.defline.poreRead and
               fastq1.defline.poreRead == "complement" and
               fastq2.defline.poreRead == "template" ) ):
            save = fastq1
            fastq1 = fastq2
            fastq2 = save
        self.processPairFastqSpot ( fastq1, fastq2 )
        self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1, 1 ] )
        self.setDstName ( fastq1, self.dst )
        self.setDstSpotGroup ( fastq1, None, self.dst )
        self.gw.write(self.dst)

    ############################################################
    # Write spot where read1 in fastq and read2 in hash
    ############################################################
    
    def writeMixedRead1 ( self, fastq1 ):
        read2 = self.pairedRead2 [ fastq1.defline.name ]
        del self.pairedRead2 [ fastq1.defline.name ]
        self.dst[self.readColumn]['data'] = (fastq1.seq + read2['seq']).encode('ascii')
        if self.isColorSpace:
            self.dst['CS_KEY']['data'] = (fastq1.csKey + read2['csKey']).encode('ascii')
        self.dst['READ_START']['data'] = array.array( 'I', [ 0, len(fastq1.seq) ] )
        self.dst['READ_LENGTH']['data'] = array.array( 'I', [ len(fastq1.seq), len(read2['seq']) ] )
        if ( fastq1.defline.filterRead or
             read2['filterRead'] ):
            self.dst['READ_FILTER']['data'] = array.array('B', [ 1, 1 ] )
        else:
            self.dst['READ_FILTER']['data'] = array.array('B', [ 0, 0 ] )

        if ( self.isNumQual and
             fastq1.qual and
             read2['qual'] ):
            fastq1.qual += " "
        self.setDstQual ( fastq1.qual + read2['qual'], self.dst )
        
        self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1, 1 ] )
        self.setDstName ( fastq1, self.dst )
        self.setDstSpotGroup ( fastq1, None, self.dst )
        self.gw.write(self.dst)

    ############################################################
    # Write spot where read2 in fastq and read1 in hash
    ############################################################
    
    def writeMixedRead2 ( self, fastq2 ):
        read1 = self.pairedRead1 [ fastq2.defline.name ]
        del self.pairedRead1 [ fastq2.defline.name ]
        self.dst[self.readColumn]['data'] = (read1['seq'] + fastq2.seq ).encode('ascii')
        if self.isColorSpace:
            self.dst['CS_KEY']['data'] = ( read1['csKey'] + fastq2.csKey ).encode('ascii')
        self.dst['READ_START']['data'] = array.array( 'I', [ 0, len(read1['seq']) ] )
        self.dst['READ_LENGTH']['data'] = array.array( 'I', [ len(read1['seq']), len(fastq2.seq) ] )
        if ( read1['filterRead'] or
             fastq2.defline.filterRead ):
            self.dst['READ_FILTER']['data'] = array.array('B', [ 1, 1 ] )
        else:
            self.dst['READ_FILTER']['data'] = array.array('B', [ 0, 0 ] )

        if ( self.isNumQual and
             read1['qual'] and
             fastq2.qual ):
             read1['qual'] += " "
            
        self.setDstQual ( read1['qual'] + fastq2.qual, self.dst )
        
        self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1, 1 ] )

        # qiime name can vary between read1 and read2 (read1 name is used)
        
        if fastq2.defline.qiimeName:
            fastq2.defline.qiimeName = read1['qiimeName']

        if fastq2.defline.suffix:
            fastq2.defline.suffix = read1['suffix']

        self.setDstName ( fastq2, self.dst )
        self.setDstSpotGroup ( fastq2, None, self.dst )
        self.gw.write(self.dst)

    ############################################################
    # Write spot where read in fastq is an orphan
    ############################################################
    
    def writeMixedOrphan ( self, fastq ):
        self.processFragmentFastqSpot ( fastq )
        self.dst['READ_START']['data'] = array.array( 'I', [ 0 ] )
        self.dst['READ_TYPE']['data'] = array.array( 'B', [ 1 ] )
        self.setDstName ( fastq, self.dst )
        self.setDstSpotGroup ( fastq, None, self.dst )
        self.gw.write(self.dst)

    ############################################################
    # Populate paired read hashes
    ############################################################
    
    def addToPairedReadHash ( self, fastq ):
        if self.isColorSpace:
            readValues = { 'seq':fastq.seq, 'qual':fastq.qual, 'filterRead':fastq.defline.filterRead, 'csKey':fastq.csKey }
        else:
            readValues = { 'seq':fastq.seq, 'qual':fastq.qual, 'filterRead':fastq.defline.filterRead }

        # qiime name can vary between read1 and read2
        
        if fastq.defline.qiimeName:
            readValues['qiimeName'] = fastq.defline.qiimeName
            
        if fastq.defline.suffix:
            readValues['suffix'] = fastq.defline.suffix

        if ( ( fastq.defline.readNum and
               fastq.defline.readNum == "1") or
             ( fastq.defline.poreRead and
               fastq.defline.poreRead == "template") or
             ( fastq.defline.tagType and
               fastq.defline.tagType == "F3") ):
            self.pairedRead1[fastq.defline.name] = readValues
        else:
            self.pairedRead2[fastq.defline.name] = readValues

    ############################################################
    # Populate paired read hashes
    ############################################################
    
    def addToPoreReadHash ( self, fastq ):
        if ( fastq and
             not fastq.eof):
            if fastq.defline.poreRead != "2D":
                self.addToPairedReadHash ( fastq )
            elif not fastq.defline.name in self.nanopore2Dread :
                readValues = { 'seq':fastq.seq, 'qual':fastq.qual, 'filterRead':fastq.defline.filterRead }
                self.nanopore2Dread[fastq.defline.name] = readValues

    ############################################################
    # Dump leftover orphan reads
    # Not sure if it would be quicker to del entries in each hash
    # when found
    ############################################################
    
    def processOrphanReads ( self, fastq1 ):
        if ( fastq1.defline.name in self.pairedRead1 or
             fastq1.defline.name in self.pairedRead2 ):
            self.writeMixedOrphan ( fastq1 )
        if fastq1.spotCount % self.infoSizeForSpotCount == 0:
            self.outputCount(fastq1,None,True)

    ############################################################
    # Set name to be written in archive
    # Keeping qiimeName separate because readNum may be present
    # in the qiimeName and I have no way to remove it. But I can
    # remove it from what occurs after the qiimeName and use that
    # for comparing read names.
    ############################################################
            
    def setDstName ( self, fastq, dst ):
        self.nameCount += 1
        if ( self.discardNames or
             self.useAndDiscardNames ):
            pass
        else:
            name = fastq.defline.name
            if ( self.ignoreNames and
                 self.firstLabelRE ):
                m = re.search( self.firstLabelRE, name )
                if m:
                    name = name[:m.start()]
            if fastq.defline.qiimeName :
                name = fastq.defline.qiimeName + "_" + name
            if fastq.defline.ignoredLeadChars :
                name = fastq.defline.ignoredLeadChars + name
            if fastq.defline.suffix :
                name = name + fastq.defline.suffix
            dst['NAME']['data'] = name.encode('ascii')

    ############################################################
    # Set spot group in destination array
    # The bar code may not provide the final spot group that is
    # indicated in the run xml data block.
    ############################################################
    
    def setDstSpotGroup ( self, fastq, fastq2, dst ):
        spotGroup = ''
        if not self.discardBarcodes:
            if self.spotGroupProvided:
                spotGroup = self.spotGroup
            elif ( self.lengthsProvided and
                   self.readCount != self.readCountNonGroup ):
                start = 0
                readIndex = -1
                for readType in self.readTypes:
                    readIndex += 1
                    if readType == self.READ_TYPE_GROUP:
                        if spotGroup:
                            spotGroup += "_"
                        spotGroup += fastq.seq[ start : start + self.readLengths[readIndex] ]
                    start += self.readLengths[readIndex]
            elif ( fastq.defline.spotGroup and
                   not self.discardNames ):
                spotGroup = fastq.defline.spotGroup

        dst['SPOT_GROUP']['data'] = spotGroup.encode('ascii')
        if ( spotGroup and
             not self.spotGroupProvided and
             not spotGroup in self.spotGroupsFound ):
            self.spotGroupsFound[spotGroup] = 1
            if len ( self.spotGroupsFound ) > 30000:
                self.statusWriter.outputErrorAndExit( "Over 30000 unique spot groups were found within this submission. Please specify '--discardBarcodes'" )

    ############################################################
    # Set quality in destination array
    ############################################################
    
    def setDstQual ( self, qualString, dst ):
        if self.isNumQual:
            qualVals = self.getNumQualArray ( qualString )
            dst['QUALITY']['data'] = qualVals
        else:
            if self.changeNegOneQual:
                qualString = qualString.translate(self.transNegOne)
            dst['QUALITY']['data'] = qualString.encode('ascii')

    ############################################################
    # Set quality offset
    ############################################################
    
    def getNumQualArray ( self, qualString ):

        qualValStrings = qualString.split()
        qualIndex = -1
        
        if self.logOdds:
            qualVals = array.array('b', [0] * len(qualValStrings) )
        else:
            qualVals = array.array('B', [0] * len(qualValStrings) )

        for qualValString in qualValStrings:
            qualIndex += 1
            qualVal = int(qualValString)

            # Handle case where dots or Ns are sometimes assigned '-1' for quality
            
            if ( self.changeNegOneQual and
                 qualVal == -1 ):
                qualVal = 0
                
            qualVals[qualIndex] = qualVal

        return qualVals

    ############################################################
    # Set quality offset
    ############################################################
    
    def setQualityOffset ( self, offset, provided ):

        if ( offset == "PHRED_0" or
             offset == "0" ) :
            offset = 0
        elif ( offset == "PHRED_33" or
               offset == "33" ) :
            offset = 33
        elif ( offset == "PHRED_64" or
               offset == "64" ) :
            offset = 64
        
        if ( offset != 0 and
             offset != 33 and
             offset != 64 ):
            self.statusWriter.outputErrorAndExit( "Invalid offset determined or specified. Allowed values are 0 (numerical quality), 33, or 64 ... {}"
                                                  .format(offset) )
        else:
            self.offset = offset

            if self.offset == 64:
                if self.logOdds:
                    self.tbl['SEQUENCE']['QUALITY']['expression'] = '(INSDC:quality:text:log_odds_64)QUALITY'
                else:
                    self.tbl['SEQUENCE']['QUALITY']['expression'] = '(INSDC:quality:text:phred_64)QUALITY'

            elif self.offset == 0:
                if self.logOdds:
                    self.tbl['SEQUENCE']['QUALITY']['expression'] = '(INSDC:quality:log_odds)QUALITY'
                else:
                    self.tbl['SEQUENCE']['QUALITY']['expression'] = '(INSDC:quality:phred)QUALITY'
                self.isNumQual = True

            elif ( self.offset == 33 and
                   self.logOdds ):
                self.statusWriter.outputErrorAndExit( "\nCombining 33 offset with log odds is not yet supported" )
    
            if provided :
                self.offsetProvided = True

    ############################################################
    # Set read lengths from provided comma-separated list of lengths
    ############################################################
    
    def setReadLengths (self,readLengthString):
        readLengthString = readLengthString.strip()
        readLens = readLengthString.split(",")
        readLenCount = len(readLens)
        if self.readCount != 0:
            if readLenCount != self.readCount:
                self.statusWriter.outputErrorAndExit( "\nRead count disagreement between provided read lengths, {}, and prior argument read count, {}"
                                                      .format(str(readLenCount),str(self.readCount)))
        else:
            self.readCount = readLenCount

        readIndex = -1
        readStart = 0
        self.readLengths = array.array('I', [0] * self.readCount)
        self.readStarts = array.array('I', [0] * self.readCount)
        for readLen in readLens:
            readLen = readLen.strip()
            if readLen.isdigit():
                readIndex += 1
                self.readLengths[readIndex] = int(readLen)
                if self.readLengths[readIndex] == 0 :
                    self.variableLengthSpecified = True
                self.readStarts[readIndex] = readStart
                readStart += int(readLen)
            else:
                self.statusWriter.outputErrorAndExit( "Non-integer length specified ... {}".format(readLen))
        self.lengthsProvided = True

    ############################################################
    # Set read types from provided read type string
    ############################################################
    
    def setReadTypes (self,readTypeString):
        readTypeString = readTypeString.strip()
        if (sys.version).startswith("3"):
            transType = str.maketrans('', '', "BTG")
            empty = readTypeString.translate(transType)
        else:
            empty = readTypeString.translate(None, "BTG")
        if len(empty) != 0:
            self.statusWriter.outputErrorAndExit( "Invalid read type specified (only B, T, or G allowed) ... {}".format(readTypeString) )

        else:
            typeCount = len(readTypeString)
            if self.readCount != 0:
                if typeCount != self.readCount:
                    self.statusWriter.outputErrorAndExit( "Read count disagreement between provided read types, {}, and prior argument read count, {}"
                                                          .format(typeCount,self.readCount))
            else:
                self.readCount = typeCount

            readIndex = -1
            self.readTypes = array.array('B', [0] * self.readCount)
            for readType in readTypeString:
                readIndex += 1
                if readType == "G":
                    self.readTypes[readIndex] = self.READ_TYPE_GROUP
                else:
                    self.readCountNonGroup += 1
                    if readType == "T":
                        self.readTypes[readIndex] = self.READ_TYPE_TECHNICAL
                    else:
                        self.readTypes[readIndex] = self.READ_TYPE_BIOLOGICAL

    ############################################################
    # Set read labels variables from provided read label string
    ############################################################
    
    def setReadLabels (self,readLabelString):
        readLabelString = readLabelString.strip()
        labels = readLabelString.split(",")
        labelCount = len(labels)
        if self.readCount != 0:
            if labelCount != self.readCount:
                self.statusWriter.outputErrorAndExit( "Read count disagreement between provided read labels, {}, and prior argument read count, {}"
                                                      .format(labelCount,self.readCount))
        else:
            self.readCount = labelCount

        readIndex = -1
        labelStart = 0
        labelString = ''
        self.firstLabelRE = re.escape ( labels[0] )
        self.firstLabelRE = self.firstLabelRE + "$"
        self.labels = ''
        self.labelStarts = array.array('I', [0] * self.readCount)
        self.labelLengths = array.array('I', [0] * self.readCount)
        for label in labels:
            readIndex += 1
            label = label.strip()
            labelString += label
            self.labelStarts[readIndex] = labelStart
            self.labelLengths[readIndex] = len(label)
            labelStart += len(label)
        self.labels = labelString.encode('ascii')
        self.labelsProvided = True

    ############################################################
    # Set spot group (need to add check for spaces/tabs)
    ############################################################
    
    def setSpotGroup (self,spotGroupString):
        self.spotGroup = spotGroupString.strip()
        self.spotGroupProvided = True

    ############################################################
    # Set platform
    ############################################################
    
    def setPlatform (self, platformString ):
        platformString.strip()
        platform = Platform.convertPlatformString(platformString)
        if platform is None :
            self.statusWriter.outputErrorAndExit( "\nUnrecognized platform was specified ... {}".format(platformString) )
        else:
            self.platform = array.array('B', [platform.value] )
            self.platformString = platformString
            if platform == Platform.SRA_PLATFORM_ILLUMINA:
                self.db = 'NCBI:SRA:Illumina:db'

    ############################################################
    # Ignore leading characters on deflines for pairing
    ############################################################
    
    def setIgnLeadChars ( self, ignLeadCharsNum ):
        self.ignLeadCharsNum = int(ignLeadCharsNum) - 1

    ############################################################
    # Ignore names and remove NAME column value
    ############################################################
    
    def setIgnoreNames ( self ):
        self.ignoreNames = True
        self.mixedDeflines = True

    ############################################################
    # Discard names and remove NAME column value
    ############################################################
    
    def setDiscardNames ( self ):
        self.discardNames = True
        self.mixedDeflines = True
        del self.tbl['SEQUENCE']['NAME']

    ############################################################
    # Discard names and remove NAME column value
    ############################################################
    
    def setUseAndDiscardNames ( self ):
        self.useAndDiscardNames = True
        del self.tbl['SEQUENCE']['NAME']

    ############################################################
    # Discard barcodes because too many
    ############################################################
    
    def setDiscardBarcodes ( self ):
        self.discardBarcodes = True

    ############################################################
    # Set alternative schema to use
    ############################################################
    
    def setSchema ( self, schema ):
        self.schema = schema

    ############################################################
    # Remove clip columns if not being used
    ############################################################

    def removeClipColumns ( self ):
        del self.tbl['SEQUENCE']['CLIP_QUALITY_LEFT']
        del self.tbl['SEQUENCE']['CLIP_QUALITY_RIGHT']

    ############################################################
    # Remove label columns if not being used
    ############################################################

    def removeLabelColumns ( self ):
        del self.tbl['SEQUENCE']['LABEL']
        del self.tbl['SEQUENCE']['LABEL_START']
        del self.tbl['SEQUENCE']['LABEL_LEN']

    ############################################################
    # Remove nanopore columns if not being used
    ############################################################

    def removeNanoporeColumns ( self ):
        del self.tbl['SEQUENCE']['READ_NO']
        del self.tbl['SEQUENCE']['CHANNEL']

    ############################################################
    # Remove absolid columns if not being used
    ############################################################

    def removeColorSpaceColumns ( self ):
        del self.tbl['SEQUENCE']['CSREAD']
        del self.tbl['SEQUENCE']['CS_KEY']

    ############################################################
    # Remove non-absolid columns if not being used
    ############################################################

    def removeNonColorSpaceColumns ( self ):
        del self.tbl['SEQUENCE']['READ']

    ############################################################
    # Set nanopore columns READ_NUMBER and CHANNEL
    ############################################################

    @staticmethod
    def setNanoporeColumns ( fastq, dst ):
        dst['READ_NO']['data'] = array.array( 'I', [ int(fastq.defline.readNo) ] )
        dst['CHANNEL']['data'] = array.array( 'I', [ int(fastq.defline.channel) ] )

    ############################################################
    # Outputs spot count
    ############################################################

    def outputCount ( self, fastq, fastq2, readCount ):
        if ( fastq2 and
             fastq.filename != fastq2.filename ):
            if fastq.spotCount == fastq2.spotCount:
                if readCount:
                    self.statusWriter.outputInfo("Reading spot number {} from {} and {}".format(fastq.spotCount,fastq.filename,fastq2.filename) )
                else:
                    self.statusWriter.outputInfo("Wrote approx. spot number {} from {} and {}".format(fastq.spotCount,fastq.filename,fastq2.filename) )
            else:
                if readCount:
                    self.statusWriter.outputInfo("Reading spot number {} from {}, spot number {} from {}"
                                                 .format(fastq.spotCount,fastq.filename,fastq2.spotCount,fastq2.filename) )
                else:
                    self.statusWriter.outputInfo("Wrote approx. spot number {} from {}, approx. spot number {} from {}"
                                                 .format(fastq.spotCount,fastq.filename,fastq2.spotCount,fastq2.filename) )
        else:
            if readCount:
                self.statusWriter.outputInfo("Reading spot number {} from {}".format(fastq.spotCount,fastq.filename) )
            else:
                self.statusWriter.outputInfo("Wrote approx. spot number {} from {}".format(fastq.spotCount,fastq.filename) )

############################################################
# Process command line arguments
############################################################

def processArguments():

    if len(sys.argv[1:]) == 0 :
        usage( None , 0 )

    for arg in sys.argv[1:]:
        if arg[0:1] == '-':
            if arg[2:9] == 'offset=':
                sw.setQualityOffset ( arg[9:], True )
            elif arg[2:9] == 'output=':
                sw.outdir = arg[9:]
            elif arg[2:10] == 'quality=':
                sw.setQualityOffset ( arg[10:], True )
            elif arg[2:11] == 'readLens=':
                sw.setReadLengths ( arg[11:] )
            elif arg[2:12] == 'readTypes=':
                sw.setReadTypes ( arg[12:] )
            elif arg[2:12] == 'spotGroup=':
                sw.setSpotGroup ( arg[12:] )
            elif arg[2:] == 'orphanReads':
                sw.orphanReads = True
            elif arg[2:] == 'logOdds':
                sw.logOdds = True
            elif arg[2:] == 'ignoreNames':
                sw.setIgnoreNames()
            elif arg[2:] == 'discardNames':
                sw.setDiscardNames()
            elif arg[2:17] == 'read1PairFiles=':
                sw.read1PairFiles = arg[17:]
            elif arg[2:17] == 'read2PairFiles=':
                sw.read2PairFiles = arg[17:]
            elif arg[2:17] == 'read1QualFiles=':
                sw.read1QualFiles = arg[17:]
            elif arg[2:17] == 'read2QualFiles=':
                sw.read2QualFiles = arg[17:]
            elif arg[2:] == 'profile':
                sw.profile = True
            elif arg[2:11] == 'platform=':
                sw.setPlatform ( arg[11:] )
            elif arg[2:13] == 'readLabels=':
                sw.setReadLabels ( arg[13:] )
            elif arg[2:16] == 'maxErrorCount=':
                sw.maxErrorCount = int ( arg[16:] )
            elif arg[2:] == 'mixedDeflines':
                sw.mixedDeflines = True
            elif arg[2:9] == 'schema=':
                sw.setSchema ( arg[9:] )
            elif arg[2:10] == 'xml-log=':
                statusWriter.setXmlLog( arg[10:] )
            elif arg[2:15] == 'ignLeadChars=':
                sw.setIgnLeadChars( arg[15:] )
            elif arg[2:] == 'discardBarcodes':
                sw.setDiscardBarcodes()
            elif arg[2:] == 'useAndDiscardNames':
                sw.setUseAndDiscardNames()
            elif arg[1:2] == 'z=':
                statusWriter.setXmlLog( arg[2:] )
            elif ( arg[2:] == 'help' or
                   arg[1:] == 'h'):
                usage(None,0)
            elif ( arg[2:] == 'version' or
                   arg[1:] == 'V'):
                sys.stderr.write("\nfastq-load.py.{}\n\n".format(version))
                exit(0)
            else:
                usage( "Unrecognized option ... " + arg, 1 )
        else:
            if os.path.isdir(arg):
                for fastq in os.listdir(arg):
                    path = arg + "/" + fastq
                    if os.path.isfile(path):
                        filePaths[fastq] = path
            else:
                filePaths[os.path.basename(arg)] = arg

    # Ensure read lengths and types are consistent

    if ( sw.lengthsProvided and
         ( len(sw.readLengths) != len(sw.readTypes) or
           ( sw.labelsProvided and
             ( len(sw.readLengths) != len(sw.labelLengths) ) ) ) ):
        usage( "Read types, read lengths, and read labels (if provided)\nmust have the same number of values", 1 )

    # Ensure not orphan reads and ignore names

    if ( ( sw.ignoreNames or
           sw.discardNames ) and
         sw.orphanReads ):
        usage( "Cannot specify ignoreNames|discardNames and orphanReads at the same time", 1)

    # Ensure fastq file paths were provided

    if len(filePaths) == 0 :
        usage( "No fastq files were specified", 1 )

    # Setup logging and work directory

    logging.basicConfig(level=logging.DEBUG)

############################################################
# Process provided file lists
############################################################

def processPairAndQualLists ():
    # Process read1 and read2 pair filename lists if provided
    # (only necessary for orphan reads or for ignoring/discarding names)
    
    if ( sw.read1PairFiles and
         sw.read2PairFiles ):
        
        sw.pairFilesProvided = True
        
        for (filename1,filename2) in zip(sw.read1PairFiles.split(","),sw.read2PairFiles.split(",")):
            if ( not filename1 or
                 filename1 == "-"):
                statusWriter.outputInfo("Assuming this file is a fragment file ... ".format(filename2 ) )
            elif ( not filename2 or
                   filename2 == "-" ):
                statusWriter.outputInfo("Assuming this file is a fragment file ... ".format(filename1 ) )
            else:
                fileReadPairs[filename1] = filename2

    elif ( sw.read1PairFiles or
           sw.read2PairFiles ):
        usage ( "Providing only read1 or read2 pair file lists is not valid", 1)

    # Process read 1 seq and qual filename lists if provided

    if ( sw.read1PairFiles and
         sw.read1QualFiles ):
        
        sw.qualFilesProvided = True
        for (filename1,filename2) in zip(sw.read1PairFiles.split(","),sw.read1QualFiles.split(",")):
            if ( not filename1 or
                 filename1 == "-" ):
                continue
            elif ( not filename2 or
                   filename2 == "-" ):
                usage ( "Filename does not have a qual file even though read1QualFiles provided ... {}".format(filename1), 1)
            else:
                refineSeqQual (filename1, fileHandles[filename1], filename2, fileHandles[filename2] )

    # Process read 2 seq and qual filename lists if provided

    if ( sw.read2PairFiles and
         sw.read2QualFiles ):
        
        sw.qualFilesProvided = True
        for (filename1,filename2) in zip(sw.read2PairFiles.split(","),sw.read2QualFiles.split(",")):
            if ( not filename1 or
                 filename1 == "-" ):
                continue
            elif ( not filename2 or
                   filename2 == "-" ):
                usage ( "Filename does not have a qual file even though read2QualFiles provided ... {}".format(filename1), 1)
            else:
                refineSeqQual (filename1, fileHandles[filename1], filename2, fileHandles[filename2] )

############################################################
# Determine if eight line fastq
############################################################

def isEightLineOrSameNameFastq (filename, handle, fileType):
    if fileType == "multiLine":
        fastq1 = FastqReader (filename,handle)
        fastq1.isMultiLine = True
        fastq2 = FastqReader (filename,handle)
        fastq2.isMultiLine = True
    elif ( fileType == "seqQual" or
           fileType == "fasta" ):
        fastq1 = FastaFastqReader (filename,handle)
        fastq2 = FastaFastqReader (filename,handle)
    elif ( fileType == "multiLineSeqQual" or
           fileType == "multiLineFasta" ):
        fastq1 = FastaFastqReader (filename,handle)
        fastq1.isMultiLine = True
        fastq2 = FastaFastqReader (filename,handle)
        fastq2.isMultiLine = True
    else:
        fastq1 = FastqReader (filename,handle)
        fastq2 = FastqReader (filename,handle)

    if sw.ignLeadCharsNum:
        fastq1.ignoreLeadDeflineChars ( sw.ignLeadCharsNum )
        fastq2.ignoreLeadDeflineChars ( sw.ignLeadCharsNum )

    spotNames = {}
    spotCount = 0
    isEightLine = False
    fastq1.setStatus(sw)
    fastq2.setStatus(sw)
    while True:
        spotCount += 1
        fastq1.read()
        if fastq1.defline.name not in spotNames:
            spotNames[fastq1.defline.name] = 0
        spotNames[fastq1.defline.name] += 1
        fastq2.read()
        if fastq2.defline.name not in spotNames:
            spotNames[fastq2.defline.name] = 0
        spotNames[fastq2.defline.name] += 1
        if (not isEightLine and
            Defline.isPairedDeflines ( fastq1.defline, fastq2.defline, False ) ):
            if (fastq1.defline.poreRead and # Look for 2D read, too
                ( fastq1.defline.poreRead == "2D" or
                  fastq2.defline.poreRead == "2D" ) ):
                filePore2D[filename] = filename
                sw.orphanReads = True
            isEightLine = True
        elif ( isEightLine and
               not filename in filePore2D and
               fastq1.defline.poreRead and  # Look for 2D read, too
               ( fastq1.defline.poreRead == "2D" or
                 fastq2.defline.poreRead == "2D" ) ):
                filePore2D[filename] = filename
                sw.orphanReads = True

        if (spotCount == 10000 or
            fastq1.eof):
            break

    for name in spotNames:
        if spotNames[name] > 10:
            statusWriter.outputWarning("Discarding spot names because file {} has repeated names (e.g. '{}'). Paired files must be specified."
                                       .format(filename,name) )
            isEightLine = False
            sw.setDiscardNames()
            break

    return isEightLine

############################################################
# Check seqQual files for multiline, eightline
############################################################

def refineSeqQual ( filename, handle, filename2, handle2 ):
    fileSkip[filename2] = 1
    fileQualPairs[filename] = filename2
    fastq = SeqQualFastqReader (filename,handle,filename2,handle2)
    fastq.setStatus(sw)
    fastq.isMultiLine = True
    if fastq.isMultiLineFastq():
        fileTypes[filename] = "multiLineSeqQual"
    else:
        fileTypes[filename] = "seqQual"

    # Check for '8 line' seq qual

    if ( sw.isEightLine or
         ( not ( sw.ignoreNames or sw.discardNames ) and
           isEightLineOrSameNameFastq ( filename, handle, fileTypes[filename] ) ) ):
        fileReadPairs[filename] = filename
        if fileTypes[filename] == "multiLineSeqQual":
            fileTypes[filename] = "multiLineEightLineSeqQual"
        else:
            fileTypes[filename] = "eightLineSeqQual"

    statusWriter.outputInfo("File {} is identified as {}".format(filename,fileTypes[filename] ) )
    statusWriter.outputInfo("File {} is identified as {} (seq file is {})".format(filename2,fileTypes[filename],filename) )

############################################################
# Check fasta files for multiline, eightline
############################################################

def refineFasta ( filename, handle ):
    fastq = FastaFastqReader (filename,handle)
    fastq.setStatus(sw)
    fastq.isMultiLine = True
    if fastq.isMultiLineFasta():
        fileTypes[filename] = "multiLineFasta"
    else:
        fileTypes[filename] = "fasta"

    # Check for '8 line' fasta

    if ( sw.isEightLine or
         ( not ( sw.ignoreNames or sw.discardNames ) and
           isEightLineOrSameNameFastq ( filename, handle, fileTypes[filename] ) ) ):
        fileReadPairs[filename] = filename
        if fileTypes[filename] == "multiLineFasta":
            fileTypes[filename] = "multiLineEightLineFasta"
        else:
            fileTypes[filename] = "eightLineFasta"
                            
    statusWriter.outputInfo("File {} is identified as {}".format(filename,fileTypes[filename]) )

############################################################
# Check fasta files for multiline, eightline
############################################################

def refineFastq ( filename, handle ):
    fastq = FastqReader ( filename, handle )
    fastq.setStatus(sw)
    fastq.isMultiLine = True
    if fastq.isMultiLineFastq():
        if ( sw.isEightLine or
             ( not ( sw.ignoreNames or sw.discardNames) and
               isEightLineOrSameNameFastq ( filename, handle, "multiLine" ) ) ):
            fileReadPairs[filename] = filename
            fileTypes[filename] = "multiLineEightLine"
        else:
            fileTypes[filename] = "multiLine"
    elif ( sw.isEightLine or
           ( not ( sw.ignoreNames or sw.discardNames) and
             isEightLineOrSameNameFastq ( filename, handle, False ) ) ):
        fileReadPairs[filename] = filename
        fileTypes[filename] = "eightLine"
    else:
        fileTypes[filename] = "normal"

    statusWriter.outputInfo("File {} is identified as {}".format(filename, fileTypes[filename]) )

############################################################
# Determine file types
# Check for separate seq/qual files and single/eight line fastq
# Also check for fasta only file (i.e. no corresponding qual file)
# Check for multi-line seq/qual
############################################################

def setFileTypes ():
    startFileNum = 0
    for filename in sorted(fileHandles):
        startFileNum += 1
        if ( filename in fileSkip or
             filename in fileQualPairs ):
            continue
        defline = Defline('')
        handle = fileHandles[filename]
        FastqReader.processHeader(handle,defline)
        file1DeflineString = handle.readline().strip()
        deflineChar = file1DeflineString[0:1]

        # Check for 'single line' fastq

        if ( deflineChar != "@" and
             deflineChar != ">" ):
            fastq = SingleLineFastqReader (filename,handle)
            if fastq.isSingleLineFastq():
                fileTypes[filename] = "singleLine"
                statusWriter.outputInfo("File {} is identified as single line fastq".format(filename) )
            else:
                statusWriter.outputErrorAndExit( "Defline without > or @ is not recognizable as single line fastq ... {}"
                                                 .format(file1DeflineString) )

        # Check for '8 line' (8-line with ignored/discarded names must be user specified)
        # and/or 'multi-line' fastq

        elif deflineChar == "@":
            refineFastq (filename, handle)

        # Check for separate seq/qual files (deflineChar == ">")
        # Also check for fasta only file

        else:
            fileNum = 0
            for filename2 in sorted(fileHandles):
                fileNum += 1

                if ( fileNum <= startFileNum or
                     filename2 in fileQualPairs or
                     filename2 in fileSkip ):
                    continue
                else:
                    
                    # Look for matching seq/qual file in remaining files
                    
                    defline2 = Defline('')
                    handle2 = fileHandles[filename2]
                    FastqReader.processHeader(handle2,defline2)
                    file2DeflineString = handle2.readline().strip()
                    deflineChar2 = file2DeflineString[0:1]
                    if deflineChar2 != ">":
                        handle2.seek(0)
                        continue
                    else:
                        fastq = SeqQualFastqReader (filename,handle,filename2,handle2)
                        fastq.setStatus(sw)
                        isSeqQualFastq = fastq.isSeqQualFastq()
                        if isSeqQualFastq == 1:
                            refineSeqQual(filename,handle,filename2,handle2)
                        elif isSeqQualFastq == 2:
                            refineSeqQual(filename2,handle2,filename,handle)
                    handle2.seek(0)

            if ( filename not in fileTypes and
                 filename not in fileSkip and
                 filename not in fileQualPairs ):
                fastq = FastaFastqReader (filename,handle)
                fastq.setStatus(sw)
                if fastq.isFastaFastq():
                    refineFasta ( filename, handle )

                else:
                    parsedQual = Qual(fastq.seq, 0)
                    if parsedQual.isValid :
                        statusWriter.outputErrorAndExit( "Unable to associate seq file with this qual file ... {}".format(filename) )
                    else:
                        statusWriter.outputErrorAndExit( "File with '>' defline character is unrecognized as containing sequence or quality ... {}"
                                                         .format(filename) )

        handle.seek(0)

############################################################
# Based on fileType, return fastq instance for provided filename
# Types are normal, singleLine, eightLine, seqQual, fasta.
############################################################

def getFastqInstance (filename):
    reader = None
    if ( fileTypes[filename] == "normal" or
         fileTypes[filename] == "eightLine" ):
        reader = FastqReader (filename,fileHandles[filename])
    elif ( fileTypes[filename] == "multiLine" or
           fileTypes[filename] == "multiLineEightLine" ):
        reader = FastqReader (filename,fileHandles[filename])
        reader.isMultiLine = True
    elif ( fileTypes[filename] == "seqQual" or
           fileTypes[filename] == "eightLineSeqQual" ):
        filenameQual = fileQualPairs[filename]
        reader = SeqQualFastqReader (filename,fileHandles[filename],filenameQual,fileHandles[filenameQual])
    elif ( fileTypes[filename] == "multiLineSeqQual" or
           fileTypes[filename] == "multiLineEightLineSeqQual" ):
        filenameQual = fileQualPairs[filename]
        reader = SeqQualFastqReader (filename,fileHandles[filename],filenameQual,fileHandles[filenameQual])
        reader.isMultiLine = True
    elif ( fileTypes[filename] == "fasta" or
           fileTypes[filename] == "eightLineFasta" ):
        reader = FastaFastqReader (filename,fileHandles[filename])
    elif ( fileTypes[filename] == "multiLineFasta" or
           fileTypes[filename] == "multiLineEightLineFasta") :
        reader = FastaFastqReader (filename,fileHandles[filename])
        reader.isMultiLine = True
    elif fileTypes[filename] == "singleLine":
        reader = SingleLineFastqReader (filename,fileHandles[filename])
    else:
        statusWriter.outputErrorAndExit( "Unable to determine file type for ... {}".format(filename) )

    reader.setStatus(sw)

    return reader

############################################################
# Pair up files containing read pairs
############################################################

def setFilePairs ():

    if sw.ignoreNames or sw.discardNames:
        return

    startFileNum = 0
    for filename1 in sorted(filePaths):
        startFileNum += 1
        if ( filename1 in fileSkip or
             filename1 in fileReadPairs or
             filename1 in filePore2D ):
            continue
        fastq1 = getFastqInstance(filename1)
        fastq1.read()
        fileNum = 0

        # Search for corresponding pair file in the rest of the files

        for filename2 in sorted(fileHandles):
            fileNum += 1
            if ( fileNum <= startFileNum or
                 filename2 in fileSkip ):
                continue
            else:
                fastq2 = getFastqInstance(filename2)
                fastq2.read()
                
                isPairedDeflines = Defline.isPairedDeflines ( fastq1.defline, fastq2.defline, False )

                # Ensure filename with first read (if specified) is captured in fileReadPairs

                if isPairedDeflines:
                    
                    processPairedDeflines ( filename1, filename2, fastq1.defline, fastq2.defline, isPairedDeflines )
                    
                    if ( ( filename1 in fileReadPairs and
                           ( not fastq1.defline.poreRead or
                             filename1 in filePore2D ) ) or
                         filename2 in fileReadPairs or
                         filename2 in filePore2D ):
                        break

    # Check for pairs with orphan reads (if any unpaired files still exist)
    # Also checks for eight line fastq instances with orphans

    startFileNum = 0
    for filename1 in sorted(filePaths):
        startFileNum += 1
        if ( filename1 in fileSkip or
             filename1 in fileReadPairs or
             filename1 in filePore2D ):
            continue
        fastq1 = getFastqInstance(filename1)
        file1Reads = {}
        read1Count = 0
        while True:
            fastq1.read()
            if fastq1.eof:
                break
            read1Count+=1
            if fastq1.defline.name in file1Reads:
                sw.orphanReads = True
                statusWriter.outputInfo("File {} is eight line fastq with orphan reads".format(filename1) )
                fileReadPairs[filename1] = filename1
                
                if fileTypes[filename1] == "normal":
                    fileTypes[filename1] = "eightLine"
                elif fileTypes[filename1] == "seqQual":
                    fileTypes[filename1] = "eightLineSeqQual"
                elif fileTypes[filename1] == "fasta":
                    fileTypes[filename1] = "eightLineFasta"
                elif fileTypes[filename1] == "multiLine":
                    fileTypes[filename1] = "multiLineEightLine"
                elif fileTypes[filename1] == "multiLineSeqQual":
                    fileTypes[filename1] = "multiLineEightLineSeqQual"
                elif fileTypes[filename1] == "multiLineFasta":
                    fileTypes[filename1] = "multiLineEightLineFasta"
                elif ( fileTypes[filename1] == "eightLine" or
                       fileTypes[filename1] == "eightLineSeqQual" or
                       fileTypes[filename1] == "eightLineFasta" or
                       fileTypes[filename1] == "multiLineEightLine" or
                       fileTypes[filename1] == "multiLineEightLineSeqQual" or
                       fileTypes[filename1] == "multiLineEightLineFasta" ):
                    pass
                else:
                    statusWriter.outputErrorAndExit ("Unsupported eight line fastq file with orphan reads ... {}".format(filename1) )
                
                if fastq1.defline.poreRead:
                    defline1 = Defline(file1Reads[fastq1.defline.name])
                    if ( defline1.poreRead == "2D" or
                         fastq1.defline.poreRead == "2D" ):
                        filePore2D[filename1] = filename1

                if ( filename1 in fileReadPairs and
                     ( not fastq1.defline.poreRead or
                       filename1 in filePore2D ) ):
                     break
            else:
                file1Reads[fastq1.defline.name] = fastq1.deflineStringSeq
                
            if read1Count == 50000:
                break

        if ( fileTypes[filename1] == "eightLine" or
             fileTypes[filename1] == "eightLineSeqQual" or
             fileTypes[filename1] == "eightLineFasta" or
             fileTypes[filename1] == "multiLineEightLine" or
             fileTypes[filename1] == "multiLineEightLineSeqQual" or
             fileTypes[filename1] == "multiLineEightLineFasta" ):
            continue
        
        else:
            fileNum = 0

            # Search for corresponding pair file in the rest of the files (allowing for orphans)

            for filename2 in sorted(fileHandles):
                fileNum += 1
                if ( fileNum <= startFileNum or
                     filename2 in fileSkip ):
                    continue
                else:
                    read2Count = 0
                    fastq2 = getFastqInstance(filename2)
                    while True:
                        fastq2.read()
                        if fastq2.eof:
                            break
                        read2Count += 1
                        if fastq2.defline.name in file1Reads:

                            # Reset fastq1 defline object to state for retained fastq1.deflineStringSeq

                            fastq1.defline.parseDeflineString(file1Reads[fastq2.defline.name])

                            # Determine which file has lesser read (e.g. read1 vs read2)

                            isPairedDeflines = Defline.isPairedDeflines ( fastq1.defline, fastq2.defline, False )

                            # Use 'isPairedDeflines' values to retain lesser read

                            if isPairedDeflines:
                                sw.orphanReads = True
                                statusWriter.outputInfo("Files {} and {} are paired with orphan reads".format(filename1,filename2) )
                                processPairedDeflines ( filename1, filename2, fastq1.defline, fastq2.defline, isPairedDeflines )
                                if ( ( filename1 in fileReadPairs and
                                       ( not fastq1.defline.poreRead or
                                         filename1 in filePore2D ) ) or
                                     filename2 in fileReadPairs or
                                     filename2 in filePore2D ):
                                    break
                        if read2Count == 50000:
                            break
                if ( ( filename1 in fileReadPairs and
                       ( not fastq1.defline.poreRead or
                         filename1 in filePore2D ) ) or
                     filename2 in fileReadPairs or
                     filename2 in filePore2D ):
                    break

    # Check for nanopore 2D files still not paired (if any unpaired 2D files still exist)
    # Addressing case where template/complement reads in one file and 2D reads in another

    startFileNum = 0
    for filename1 in sorted(filePaths):
        startFileNum += 1
        if ( filename1 in fileSkip or
             filename1 in fileReadPairs or
             filename1 in filePore2D ):
            continue
        fastq1 = getFastqInstance(filename1)
        file1Reads = {}
        read1Count = 0
        while True:
            fastq1.read()
            if ( fastq1.eof or
                 not fastq1.defline.poreRead or
                 ( fastq1.defline.poreRead and
                   not fastq1.defline.poreRead == "2D" ) ):
                break
            read1Count+=1
            file1Reads[fastq1.defline.name] = fastq1.deflineStringSeq
            if read1Count == 50000:
                break
        if ( not fastq1.defline.poreRead or
             ( fastq1.defline.poreRead and
               not fastq1.defline.poreRead == "2D" ) ):
            continue
        else:
            for filename2 in sorted(fileHandles):
                if ( filename2 == filename1 or
                     filename2 in fileSkip or
                     filename2 in filePore2D ):
                    continue
                else:
                    read2Count = 0
                    fastq2 = getFastqInstance(filename2)
                    while True:
                        fastq2.read()
                        if fastq2.eof:
                            break
                        read2Count += 1
                        if fastq2.defline.name in file1Reads:
                            fastq1.defline.parseDeflineString ( file1Reads[fastq2.defline.name] )
                            isPairedDeflines = Defline.isPairedDeflines ( fastq1.defline, fastq2.defline, False )
                            if isPairedDeflines:
                                sw.orphanReads = True
                                processPairedDeflines ( filename1, filename2, fastq1.defline, fastq2.defline, isPairedDeflines )
                                if filename2 in filePore2D:
                                    break
                        if read2Count == 50000:
                            break
                if filename2 in filePore2D:
                    break

############################################################
def processPairedDeflines ( filename1, filename2, defline1, defline2, isPairedDeflines):
    
    # Check for nanopore 2D

    if defline1.poreRead:

        if isPairedDeflines == 1:
            fileSkip[filename2] = 1
            if defline2.poreRead == "complement":
                fileReadPairs[filename1] = filename2
            elif defline2.poreRead == "2D":
                filePore2D[filename1] = filename2
            else:
                statusWriter.outputErrorAndExit("Unrecognized poreRead type2 ... {}".format(defline2.poreRead) )
        else:
            fileSkip[filename1] = 1
            if defline1.poreRead == "complement":
                fileReadPairs[filename2] = filename1
            elif defline1.poreRead == "2D":
                filePore2D[filename2] = filename1
            else:
                statusWriter.outputErrorAndExit ("Unrecognized poreRead type1 ... {}".format(defline1.poreRead) )

    # Process paired files other than nanopore
                
    else:

        if isPairedDeflines == 1:
            fileReadPairs[filename1] = filename2
            fileSkip[filename2] = 1
        else:
            fileReadPairs[filename2] = filename1
            fileSkip[filename1] = 1

        # Check for consistent file types (may be an issue with multiLine type
        # paired with non-multiLine type)

        type1 = fileTypes[filename1]
        type2 = fileTypes[filename2]
        if type1 != type2:
            statusWriter.outputErrorAndExit( "Paired files have different file types ... {} is {}, {} is {}".format(filename1,type1,filename2,type2) )

############################################################
# Check for nanopore files that contain all three read types that were
# not captured by the eight-line fastq check or 2D only nanopore files
# Also check for color space files to handle them appropriately
############################################################

def checkForColorSpaceOrNanoporeFiles():

    abLabelSet = False

    for filename in sorted(filePaths):
        if filename in fileSkip:
            continue
        fastq = getFastqInstance(filename)
        fastq.read()

        # Check for presence of absolid reads
        
        if fastq.defline.panel:
            sw.setPlatform("ABSOLID")
            parsedSeq = Seq(fastq.seq)
            parsedQual = Qual(fastq.qual,parsedSeq.length)
            if parsedSeq.isColorSpace:
                sw.isColorSpace = True
                if parsedQual.length == parsedSeq.length + 1:
                    sw.extraQualForCSkey = True

            fileLabels[filename] = fastq.defline.tagType
            if ( fileTypes[filename] == "eightLine" or
                 fileTypes[filename] == "eightLineSeqQual" or
                 fileTypes[filename] == "eightLineFasta" or
                 fileTypes[filename] == "multiLineEightLine" or
                 fileTypes[filename] == "multiLineEightLineSeqQual" or
                 fileTypes[filename] == "multiLineEightLineFasta"):
                fastq.read()
                fileLabels[filename] += "," + fastq.defline.tagType
            elif filename in fileReadPairs:
                pairFilename = fileReadPairs[filename]
                fastq2 = getFastqInstance(pairFilename)
                fastq2.read()
                fileLabels[filename] += "," + fastq2.defline.tagType
                if fastq2.defline.tagType == "BC":
                    sw.readTypes[1] = sw.READ_TYPE_TECHNICAL
            if not abLabelSet:
                sw.setReadLabels(fileLabels[filename])
                abLabelSet = True

        # Check for presence of nanopore reads
                    
        elif fastq.defline.poreRead:
            if sw.platformString:
                if sw.platformString != "NANOPORE":
                    statusWriter.outputErrorAndExit("Unable to mix nanopore reads with other platforms")
            else:
                sw.setPlatform("NANOPORE")
                
            if ( filename in fileReadPairs or
                 filename in filePore2D ):
                pass
            else:
                foundTemplate = False
                foundComplement = False
                found2D = False
                readCount = 0
                while True:
                    readCount += 1
                    if ( not fastq.qualOrig and
                         not fastq.defline.name ):
                        break
                    elif fastq.defline.poreRead == "template":
                        foundTemplate = True
                    elif fastq.defline.poreRead == "complement":
                        foundComplement = True
                    else:
                        found2D = True

                    if readCount == 1000:
                        break
                    fastq.read()

                if ( foundTemplate and
                     foundComplement):
                    fileReadPairs[filename] = filename
                if ( foundTemplate and
                     found2D):
                    filePore2D[filename] = filename
                if ( not ( foundTemplate or
                           foundComplement ) ):
                    sw.nanopore2Donly = True

############################################################
# Update qual range for determination of min and max qual
# Not setting clip status yet
############################################################

def updateQualRangeAndClipStatus ( fastq1, fastq2 ):
    
    qual = Qual('',0)
    if ( not fastq1.qualOrig or
         ( fastq2 and
           not fastq2.qualOrig) ):
        return
    
    if fastq2:
        if ( fastq1.qual and
             fastq2.qual and
             ( " " in fastq1.qual or
               ( fastq1.length == 1 and qual.isInt(fastq1.qual) ) ) and
             ( " " in fastq2.qual or
               ( fastq2.length == 1 and qual.isInt(fastq2.qual) ) ) and
             ( len(fastq1.qual) > 1 or
               len(fastq2.qual) > 1 ) ):
            combinedQual = fastq1.qual + " " + fastq2.qual
        else:
            combinedQual = fastq1.qual + fastq2.qual
        qual.parseQual( combinedQual, ( fastq1.length + fastq2.length ) )
    else:
        qual.parseQual( fastq1.qual, fastq1.length )

    if qual.minQual < sw.minQual:
        sw.minQual = qual.minQual
    if qual.maxQual > sw.maxQual:
        sw.maxQual = qual.maxQual

    if ( sw.isNumQual and
         not qual.isNumQual and
         qual.length > 1 ):
        if fastq2:
            statusWriter.outputErrorAndExit( "Possible mixed numerical and non-numerical quality  ... {}, {}, {}, {}".format(fastq1.defline.name, fastq1.filename, fastq2.defline.name, fastq2.filename) )
        else:
            statusWriter.outputErrorAndExit( "Possible mixed numerical and non-numerical quality  ... {}, {}".format(fastq1.defline.name, fastq1.filename) )
    elif qual.isNumQual:
        sw.isNumQual = qual.isNumQual

    if ( ( not sw.setClips ) and
         sw.lengthsProvided and
         ( fastq1.seq != fastq1.seqOrig.strip() or
           ( fastq2 and
             fastq2.seq != fastq2.seqOrig.strip() ) ) ):
        if fastq2:
            statusWriter.outputInfo("Setting left and right clips based on {},{} or {},{}".format(fastq1.defline.name, fastq1.filename, fastq2.defline.name, fastq2.filename) )
        else:
            statusWriter.outputInfo("Setting left and right clips based on {},{}".format(fastq1.defline.name, fastq1.filename) )
        sw.setClips = True

############################################################
# Ensure lengths of seq and qual are the same
############################################################

def checkFastqLengths ( fastq ):
    if fastq.eof:
        return False
    else:
        if fastq.length != fastq.lengthQual:

            fastq.deflineCheck.deflineType = fastq.defline.deflineType
            fastq.deflineCheck.platform = fastq.defline.platform

            # Looks for absence of quality
            
            if ( fastq.qual[0:2] == fastq.defline.deflineString[0:2] and
                 fastq.deflineCheck.parseDeflineString(fastq.qual) ):
                fastq.savedDeflineString = fastq.qual
                if sw.isNumQual:
                    fastq.qual = "30"
                    fastq.qual = " 30" * (fastq.length - 1)
                else:
                    fastq.qual = "?" * fastq.length
                statusWriter.outputWarning("Qual not present for spot {} in {} - created qual containing all ?s"
                                           .format(fastq.defline.name,fastq.filename) )

            # Addresses seq length > qual length
            
            elif fastq.length > fastq.lengthQual:
                delta = fastq.length - fastq.lengthQual
                if sw.isNumQual:
                    fastq.qual += " 30" * delta
                else:
                    fastq.qual += "?" * delta
                statusWriter.outputWarning("Qual length < seq length for spot {} in {} - extended qual to seq length with ?s"
                                           .format(fastq.defline.name,fastq.filename) )

            # Addresses qual length > seq length
            
            elif fastq.length < fastq.lengthQual:
                if sw.isNumQual:
                    qualChunks = fastq.qual.split()
                    delim = " "
                    fastq.qual = delim.join(qualChunks[0:fastq.length])
                else:
                    fastq.qual = fastq.qual[0:fastq.length]
                statusWriter.outputWarning("Qual length > seq length for spot {} in {} - truncated qual to seq length"
                                           .format(fastq.defline.name,fastq.filename) )
                
            fastq.lengthQual = fastq.length
            return True
        else:
            return False

############################################################
# Read a fastq file or a fastq file pair
############################################################

def processFastqSpots ( determineOffsetAndClip, fastq1, fastq2, fastq3 ):

    errorCount = 0

    # Read from fastq files

    while True:
                
        if not fastq1.eof:
            fastq1.read()
            if ( fastq1.qualHandle and
                 fastq1.seqQualDeflineMismatch ):
                statusWriter.outputErrorAndExit( "Mismatch between seq and qual deflines ... filename,spotCount,deflineStringSeq,deflineStringQual\t{}\t{}\t{}\t{}"
                                                 .format(fastq1.filename,fastq1.spotCount,fastq1.deflineStringSeq,fastq1.deflineStringQual))

        if ( fastq2 and
             not fastq2.eof ):
            fastq2.read()
            if ( fastq2.qualHandle and
                 fastq2.seqQualDeflineMismatch ):
                statusWriter.outputErrorAndExit( "Mismatch between seq and qual deflines ... filename,spotCount,deflineStringSeq,deflineStringQual\t{}\t{}\t{}\t{}"
                                                 .format(fastq2.filename,fastq2.spotCount,fastq2.deflineStringSeq,fastq2.deflineStringQual))

        # Nanopore 2D fastq - Assuming this will end sooner than fastq1 or fastq2
        # Could all be the same file, too.
        
        if ( fastq3 and
             not fastq3.eof ):
            fastq3.read()

        # Exit if at end of fastq1 and fastq2 (if it exists)
        # Possible to have empty quality so checking spot name, too.
                
        if ( fastq1.eof and
             ( fastq2 is None or
               fastq2.eof ) ):
            break

        # Accumulate extent of values for offset determination

        elif determineOffsetAndClip:
            updateQualRangeAndClipStatus ( fastq1, fastq2 )
            if fastq1.spotCount == 100000:
                break

        # Write spot for archive generation
        # Process extras in one file as fragments

        else:

            # Check for emergence of orphan reads (due to corruption for example)
            
            if ( not ( sw.ignoreNames or sw.discardNames ) and
                 not sw.orphanReads and
                 fastq1 and
                 fastq2 and
                 ( not fastq1.eof ) and
                 ( not fastq2.eof ) and
                 fastq1.defline.name != fastq2.defline.name ):
                sw.orphanReads = True
                if fastq1.filename == fastq2.filename:
                    statusWriter.outputInfo("File {} contains paired reads with orphan reads".format(fastq1.filename) )
                else:
                    statusWriter.outputInfo("File {} and {} are paired with orphan reads".format(fastq1.filename,fastq2.filename) )


            # If fastq2 and at end of fastq2, set fastq2 to None
            # Set new unchanging values
                    
            if ( fastq2 and
                 fastq2.eof ):
                fastq2 = None
                sw.setUnchangingSpotValues ( fastq1, fastq2 )

            # If fastq2 and at end of fastq1, and set fastq1 to fastq2
            # and fastq2 to None. Set new unchanging values
                    
            elif ( fastq2 and
                   fastq1.eof ):
                fastq1 = fastq2
                fastq2 = None
                sw.setUnchangingSpotValues ( fastq1, fastq2 )

            # If mismatch between seq and qual length,
            # then set both to empty strings for now

            if checkFastqLengths ( fastq1 ):
                errorCount += 1

            if ( fastq2 and
                 checkFastqLengths ( fastq2 ) ):
                errorCount += 1
                    
            if ( fastq3 and
                 checkFastqLengths ( fastq3 ) ):
                errorCount += 1
            
            if errorCount > sw.maxErrorCount:
                statusWriter.outputErrorAndExit( "Too many errors occurred" )
            
            # Write spot to archive

            sw.writeSpot ( fastq1, fastq2, fastq3 )

############################################################
# Process files (determine offset if specified)
############################################################

def processFiles (determineOffsetAndClip):

    # Open spot writer for actual archive generation

    if not determineOffsetAndClip:
        sw.openGeneralWriter()

    # Process each file or file pair

    for filename1 in sorted(filePaths):
        
        if filename1 in fileSkip:
            continue
        
        else:

            # Get fastq instances
            
            fastq1 = getFastqInstance(filename1)
            if not determineOffsetAndClip:
                fastq1.read()
                statusWriter.outputInfo("File {} platform based on defline is {}".format(filename1,fastq1.defline.platform) )
                fastq1.restart()
                fastq1.outputStatus = True
            
            fastq2 = None
            fastq3 = None
            filename2 = ""

            if filename1 in fileReadPairs:
                filename2 = fileReadPairs[filename1]
                fastq2 = getFastqInstance(filename2)
                if not determineOffsetAndClip:
                    statusWriter.outputInfo("File {} platform based on defline is {}".format(filename2,fastq1.defline.platform) )
                    fastq2.outputStatus = True

            if filename1 in filePore2D:
                filename3 = filePore2D[filename1]
                fastq3 = getFastqInstance(filename3)
                if not determineOffsetAndClip:
                    statusWriter.outputInfo("File {} platform based on defline is {}".format(filename3,fastq1.defline.platform) )
                    fastq3.outputStatus = True
                
            # Reset fastq1 and set unchanging values for some optimization for archive generation

            if not determineOffsetAndClip:
                sw.setUnchangingSpotValues(fastq1,fastq2)

            # Read fastq files

            processFastqSpots ( determineOffsetAndClip, fastq1, fastq2, fastq3 )

            # Output orphan spots (for now, traversing the files again
            # to be consistent with the order of latf-load). I could
            # just dump the orphan reads collected by the sw object.

            if ( sw.orphanReads and
                 not determineOffsetAndClip ):
                
                # Read fastq files again (keeping order consistent with latf-load)
                # Possibility of orphan 2D reads exists

                sw.dumpOrphans = True
                fastq1.restart()
                processFastqSpots ( False, fastq1, None , None )
                
                if ( fastq2 and
                     not fastq1.filename == fastq2.filename ) :
                    fastq2.restart()
                    processFastqSpots ( False, fastq2, None , None )

                # Reset dumpOrphans and read hashes for case where multiple
                # mixed files/pairs are processed (may not be the correct
                # approach)
                
                sw.dumpOrphans = False
                sw.pairedRead1 = {}
                sw.pairedRead2 = {}

                if sw.platformString == "NANOPORE":
                    
                    sw.dumpOrphans2D = True
                    fastq1.restart()
                    processFastqSpots ( False, fastq1, None , None )
                    
                    if ( fastq3  and
                         not fastq1.filename == fastq3.filename ) :
                        fastq3.restart()
                        processFastqSpots ( False, fastq3, None , None )
                        
                    sw.dumpOrphans2D = False
                    sw.nanopore2Dread = {}

            # Indicate quality range for file

            if determineOffsetAndClip:
                if ( not filename2 or
                     filename1 == filename2 ):
                    statusWriter.outputInfo( "Qual range using 0 or 33 offset for file {} is {},{}"
                                             .format(filename1,sw.minQual,sw.maxQual) )
                else :
                    statusWriter.outputInfo( "Qual range using 0 or 33 offset for files {} and {} is {},{}"
                                             .format(filename1,filename2,sw.minQual,sw.maxQual) )

            # Indicate end of files for archive generation

            elif ( not filename2 or
                   filename1 == filename2 ):
                statusWriter.outputInfo( "End of file {}".format(filename1) )
               
            else:
                statusWriter.outputInfo( "End of files {} and {}".format(filename1,filename2) )

    if determineOffsetAndClip:
        if sw.isNumQual:

            # Take into account color space

            if ( sw.isColorSpace and
                 sw.minQual == -1 ):
                sw.minQual = 0
            
            # Set log odds first if necessary (setQualityOffset dependency)
            # '-1' only is likely used for dot or N qualities
            
            if sw.minQual < -1:
                sw.logOdds = True
                statusWriter.outputInfo( "Logodds quality type" )
            elif sw.minQual == -1:
                sw.changeNegOneQual = True

            # Set offset to 0

            sw.setQualityOffset ( 0, False )
            statusWriter.outputInfo( "Quality values are numeric" )
            
        elif ( ( sw.minQual > 25 and
                 sw.maxQual > 45 ) ) :

            # Set log odds first if necessary (setQualityOffset dependency)
            # '-1' only is likely used for dot or N qualities
            
            if ( sw.minQual + 33 - 64 ) < -1 :
                sw.logOdds = True
                statusWriter.outputInfo( "Logodds quality type2" )
            elif ( sw.minQual + 33 - 64 ) == -1:
                sw.changeNegOneQual = True

            # Set offset to 64

            sw.setQualityOffset ( 64, False )
            statusWriter.outputInfo( "Quality values are in an ascii form with an offset of 64" )
            
        else:
            sw.setQualityOffset ( 33, False )
            statusWriter.outputInfo( "Quality values are in an ascii form with an offset of 33" )

    else:
        sw.gw = None # close stream and flush

############################################################
# Generate archive from provided fastq files
############################################################

def generateArchive():

    # Open files to be processed

    for filename in filePaths:
        if filename.endswith(".gz"):
            fileHandles[filename] = gzip.open ( filePaths[filename], 'rt' )
        elif filename.endswith(".bz2"):
            fileHandles[filename] = bz2.BZ2File( filePaths[filename] )
        else:
            fileHandles[filename] = open ( filePaths[filename] )

    # Process file lists if provided

    if sw.read1PairFiles:
        processPairAndQualLists()

    # Set file types
    # (normal, singleLine, eightLine, multiLine, multiLineEightLine,
    #  seqQual, multiLineSeqQual, fasta, multiLineFasta, solid)

    setFileTypes()

    # Pair up files containing read pairs (unless pairs were provided)

    if not sw.pairFilesProvided:
        setFilePairs()

    # Check provided pair files
    
    else:
        for filename1 in fileReadPairs:
            filename2 = fileReadPairs[filename1]
            if filename1 != filename2:
                fileSkip[filename2] = 1
                type1 = fileTypes[filename1]
                type2 = fileTypes[filename2]
                if type1 != type2:
                    statusWriter.outputErrorAndExit( "Paired files have different file types ... {} is {}, {} is {}"
                                                     .format(filename1,type1,filename2,type2) )

    # Check for properly characterized absolid or nanopore files

    checkForColorSpaceOrNanoporeFiles()

    # Prescan to determine quality offset/logodds/etc. if not provided

    if not sw.offsetProvided :
        processFiles ( True )

    # Read/Parse/Write fastq data

    processFiles ( False )

############################################################
# Execute generic fastq loader
############################################################

version = "1.0.0"
profile = False
sys.stdout = os.fdopen(sys.stdout.fileno(), 'wb')
sw = FastqSpotWriter()
statusWriter = StatusWriter(version)
sw.statusWriter = statusWriter
processArguments()

if sw.profile:
    import cProfile
    import pstats
    pr = cProfile.Profile()
    pr.enable()
    generateArchive()
    pr.disable()
    ps = pstats.Stats(pr, stream=sys.stderr).sort_stats('cumulative')
    ps.print_stats()
else:
    generateArchive()

# Wait for general-loader to finish

pid = os.getpid()
loaderPid = pid+1
loaderPidPath = "/proc/" + str(loaderPid) + "/cmdline"
while True:
    time.sleep(5)
    if not os.path.exists(loaderPidPath):
        break

while True:
    time.sleep(5)
    if ( not os.path.exists( sw.outdir ) or
         time.time() - os.path.getmtime(sw.outdir) > 20 ):
        break

# Output load complete indication

statusWriter.outputInfo("Load complete")
if statusWriter.xmlLogHandle:
    statusWriter.closeXmlLog()
