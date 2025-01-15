# General Writer to drive VDB General Loader

from sys import stdout, stderr
from struct import Struct, calcsize, pack

_headerFmt = Struct("8s 4I")
_simpleEventFmt = Struct("I")
_dataEventFmt = Struct("2I")
_writeStdOut = stdout.buffer.write
_makeDataEvent = _dataEventFmt.pack


def _padding(length: int) -> str:
    return f" {4 - (length & 0x03)}x" if length & 0x03 > 0 else ''


def _paddedFormat(fmt):
    return fmt + _padding(calcsize(fmt))


def _makeHeader():
    return _headerFmt.pack("NCBIgnld".encode('ascii'),
                           1, 2, _headerFmt.size, 0)


def _makeSimpleEvent(eid):
    """ used for { evt_end_stream, evt_open_stream, evt_next_row } """
    return _simpleEventFmt.pack(eid)


def _make1StringEvent(eid, str1):
    """ used for { evt_errmsg, evt_remote_path, evt_new_table, evt_software_name } """
    len1 = len(str1)
    fmt = _paddedFormat(f"I 1I {len1}s")
    return pack(fmt, eid, len1, str1)


def _make2StringEvent(eid, str1, str2):
    """ used for { evt_use_schema, evt_metadata_node } """
    len1 = len(str1)
    len2 = len(str2)
    fmt = _paddedFormat(f"I 2I {len1}s {len2}s")
    return pack(fmt, eid, len1, len2, str1, str2)


def _makeColumnEvent(colid, tblid, bits, name):
    """ used for { evt_new_column } """
    lenn = len(name)
    fmt = _paddedFormat(f"I 3I {lenn}s")
    return pack(fmt, colid, tblid, bits, lenn, name)


class GeneralWriter:
    # pylint: disable=too-few-public-methods

    evt_bad_event       = 0

    evt_errmsg          = (1 << 24) + evt_bad_event
    evt_end_stream      = (1 << 24) + evt_errmsg

    evt_remote_path     = (1 << 24) + evt_end_stream
    evt_use_schema      = (1 << 24) + evt_remote_path
    evt_new_table       = (1 << 24) + evt_use_schema
    evt_new_column      = (1 << 24) + evt_new_table
    evt_open_stream     = (1 << 24) + evt_new_column

    evt_cell_default    = (1 << 24) + evt_open_stream
    evt_cell_data       = (1 << 24) + evt_cell_default
    evt_next_row        = (1 << 24) + evt_cell_data

    evt_move_ahead      = (1 << 24) + evt_next_row      # this one is not used here
    evt_errmsg2         = (1 << 24) + evt_move_ahead    # this one is not used here
    evt_remote_path2    = (1 << 24) + evt_errmsg2       # this one is not used here
    evt_use_schema2     = (1 << 24) + evt_remote_path2  # this one is not used here
    evt_new_table2      = (1 << 24) + evt_use_schema2   # this one is not used here
    evt_cell_default2   = (1 << 24) + evt_new_table2    # this one is not used here
    evt_cell_data2      = (1 << 24) + evt_cell_default2 # this one is not used here
    evt_empty_default   = (1 << 24) + evt_cell_data2    # this one is not used here

    # BEGIN VERSION 2 MESSAGES
    evt_software_name      = (1 << 24) + evt_empty_default
    evt_db_metadata_node   = (1 << 24) + evt_software_name
    evt_tbl_metadata_node  = (1 << 24) + evt_db_metadata_node
    evt_col_metadata_node  = (1 << 24) + evt_tbl_metadata_node
    evt_db_metadata_node2  = (1 << 24) + evt_col_metadata_node
    evt_tbl_metadata_node2 = (1 << 24) + evt_db_metadata_node2
    evt_col_metadata_node2 = (1 << 24) + evt_tbl_metadata_node2

    evt_add_mbr_db         = (1 << 24) + evt_col_metadata_node2
    evt_add_mbr_tbl        = (1 << 24) + evt_add_mbr_db

    evt_logmsg             = (1 << 24) + evt_add_mbr_tbl
    evt_progmsg            = (1 << 24) + evt_logmsg

    def errorMessage(self, message):
        _writeStdOut(_make1StringEvent(self.evt_errmsg, message.encode('utf-8')))

    def logMessage(self, message):
        _writeStdOut(_make1StringEvent(self.evt_logmsg, message.encode('utf-8')))

    def write(self, spec):
        tableId = spec['_tableId']
        for k in spec:
            if k[0] == '_':
                continue

            c = spec[k]
            if 'data' in c:
                data = c['data']
                if callable(data):
                    data = data()

                try:
                    self._writeColumnData(c['_columnId'], len(data), data)
                except Exception as e:
                    stderr.write(f"failed to write column #{c['_columnId']} {k}: {e}\n")
                    raise e
        self._writeNextRow(tableId)

    @classmethod
    def _writeHeader(cls, remoteDb, schemaFileName, schemaDbSpec):
        _writeStdOut(_makeHeader())
        _writeStdOut(_make1StringEvent(cls.evt_remote_path, remoteDb))
        _writeStdOut(_make2StringEvent(cls.evt_use_schema, schemaFileName, schemaDbSpec))

    @classmethod
    def _writeSoftwareName(cls, name, version): # name is any string, version is like "2.1.5"
        _writeStdOut(_make2StringEvent(cls.evt_software_name, name, version))

    @classmethod
    def _writeEndStream(cls):
        _writeStdOut(_makeSimpleEvent(cls.evt_end_stream))

    @classmethod
    def _writeNewTable(cls, tableId, table):
        _writeStdOut(_make1StringEvent(cls.evt_new_table + tableId, table))

    @classmethod
    def _writeNewColumn(cls, columnId, tableId, bits, column):
        _writeStdOut(_makeColumnEvent(cls.evt_new_column + columnId, tableId, bits, column))

    @classmethod
    def _writeOpenStream(cls):
        _writeStdOut(_makeSimpleEvent(cls.evt_open_stream))

    @classmethod
    def _writeColumnDefault(cls, colId, count, data):
        l = _writeStdOut(_makeDataEvent(cls.evt_cell_default + colId, count))
        l += _writeStdOut(data)
        _writeStdOut(pack(_padding(l)))

    @classmethod
    def _writeDbMetadata(cls, dbId, nodeName, nodeValue):
        _writeStdOut(_make2StringEvent(cls.evt_db_metadata_node + dbId, nodeName, nodeValue))

    @classmethod
    def _writeTableMetadata(cls, tblId, nodeName, nodeValue):
        _writeStdOut(_make2StringEvent(cls.evt_tbl_metadata_node + tblId, nodeName, nodeValue))

    @classmethod
    def _writeColumnMetadata(cls, colId, nodeName, nodeValue):
        _writeStdOut(_make2StringEvent(cls.evt_col_metadata_node + colId, nodeName, nodeValue))

    @classmethod
    def _writeColumnData(cls, colId, count, data):
        # hottest point; called rows*columns times
        l = _writeStdOut(_makeDataEvent(cls.evt_cell_data + colId, count))
        l += _writeStdOut(data)
        _writeStdOut(pack(_padding(l)))

    @classmethod
    def _writeNextRow(cls, tableId):
        # called once per row
        _writeStdOut(_makeSimpleEvent(cls.evt_next_row + tableId))

    def writeDbMetadata(self, nodeName, nodeValue):
        """ this only supports writing to the default database """
        GeneralWriter._writeDbMetadata(0, nodeName.encode('ascii'), nodeValue.encode('utf-8'))

    def writeTableMetadata(self, table, nodeName, nodeValue):
        GeneralWriter._writeTableMetadata(table['_tableId'], nodeName.encode('ascii'), nodeValue.encode('utf-8'))

    def writeColumnMetadata(self, column, nodeName, nodeValue):
        GeneralWriter._writeColumnMetadata(column['_columnId'], nodeName.encode('ascii'), nodeValue.encode('utf-8'))

    def __init__(self, fileName, schemaFileName, schemaDbSpec, softwareName, versionString, tbl):
        """ Construct a General Writer object

            writer may be None if no actual output is desired
                else writer is expected to have a callable write attribute

            fileName is a string with name of the database that will be created

            schemaFileName is a string with the path to the file containing the schema

            schemaDbSpec is a string with the schema name of the database that will be created

            softwareName is a string

            versionString is a three-part number like "2.1.5"
        """

        GeneralWriter._writeHeader(fileName.encode('utf-8')
            , schemaFileName.encode('utf-8')
            , schemaDbSpec.encode('ascii'))

        GeneralWriter._writeSoftwareName(softwareName.encode('utf-8'), versionString.encode('ascii'))

        tableId = 0
        columnId = 0
        for t in tbl:
            tableId += 1
            GeneralWriter._writeNewTable(tableId, t.encode('ascii'))
            cols = tbl[t]
            for c in cols:
                columnId = columnId + 1
                cols[c]['_columnId'] = columnId
                expression = cols[c]['expression'] if 'expression' in cols[c] else c
                bits = cols[c]['elem_bits']
                GeneralWriter._writeNewColumn(columnId, tableId, bits, expression.encode('ascii'))
            tbl[t]['_tableId'] = tableId

        GeneralWriter._writeOpenStream()
        for t in tbl.values():
            for c in t.values():
                try:
                    if 'default' in c:
                        try:
                            GeneralWriter._writeColumnDefault(c['_columnId'], len(c['default']), c['default'])
                        except:
                            stderr.write("failed to set default for %s\n" % c)
                            raise
                except TypeError:
                    pass

    def __del__(self):
        try: GeneralWriter._writeEndStream()
        except: pass


if __name__ == "__main__":
    row = 0

    def performance():
        from array import array
        import random

        def gen_phred_range():
            for c in range(3, 41):
                yield chr(33+c)
        phred_range = list(gen_phred_range())
        reads = list([''.join(random.choices(('A', 'C', 'G', 'T'), k=300))] * 50000)
        quals = list([''.join(random.choices(phred_range, k=300))] * 50000)

        def getRead():
            return random.choice(reads).encode('ascii')

        def getQual():
            return random.choice(quals).encode('ascii')

        spec = {
            'SEQUENCE': {
                'READ': {
                    'expression': '(INSDC:dna:text)READ',
                    'elem_bits': 8,
                    'data': getRead
                },
                'QUALITY': {
                    'expression': '(INSDC:quality:phred)QUALITY',
                    'elem_bits': 8,
                    'data': getQual
                },
                'LABEL': {
                    'elem_bits': 8,
                    'default': 'templatecomplement'.encode('ascii')
                },
                'LABEL_START': {
                    'elem_bits': 32,
                },
                'LABEL_LENGTH': {
                    'elem_bits': 32,
                },
                'READ_START': {
                    'elem_bits': 32,
                },
                'READ_LENGTH': {
                    'elem_bits': 32,
                },
            },
        }
        gw = GeneralWriter('file name', 'test/bogus.vschema', 'Test:BOGUS:tbl', 'GeneralWriter-test', '1.0.0', spec)

        spec['SEQUENCE']['READ_LENGTH']['data'] = array('I', [150, 150])
        for _ in range(5_000_000):
            gw.write(spec['SEQUENCE'])

        gw = None


    def sanity():
        from array import array

        readData = [ "ACGT", "GTAACGT" ]

        def getRead():
            global row
            cur = row
            row = row + 1
            return readData[cur].encode('ascii')

        spec = {
            'SEQUENCE': {
                'READ': {
                    'expression': '(INSDC:dna:text)READ',
                    'elem_bits': 8,
                    'data': getRead
                },
                'QUALITY': {
                    'expression': '(INSDC:quality:phred)QUALITY',
                    'elem_bits': 8,
                },
                'LABEL': {
                    'elem_bits': 8,
                    'default': 'templatecomplement'.encode('ascii')
                },
                'LABEL_START': {
                    'elem_bits': 32,
                },
                'LABEL_LENGTH': {
                    'elem_bits': 32,
                },
                'READ_START': {
                    'elem_bits': 32,
                },
                'READ_LENGTH': {
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
                'LABEL': {
                    'elem_bits': 8,
                    'default': '2DFull'.encode('ascii')
                },
                'LABEL_START': {
                    'elem_bits': 32,
                    'default': array('I', [ 0 ])
                },
                'LABEL_LENGTH': {
                    'elem_bits': 32,
                    'default': array('I', [ 2 ])
                }
            },
        }
        gw = GeneralWriter('file name', 'test/bogus.vschema', 'Test:BOGUS:tbl', 'GeneralWriter-test', '1.0.0', spec)

        spec['SEQUENCE']['READ_LENGTH']['data'] = array('I', [ 1, 2 ])
        gw.write(spec['SEQUENCE'])
        gw.write(spec['SEQUENCE'])

        gw.logMessage('log message')
        gw.errorMessage('error message')

        gw = None

    performance()
