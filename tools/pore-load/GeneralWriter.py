# General Writer to drive VDB General Loader

import sys
import os
import struct
import array
    
def _paddedFormat(fmt):
    l = struct.calcsize(fmt)
    if l % 4 != 0:
        fmt += (" %dx" % (4 - l % 4))
    return fmt


def _makeHeader():
    fmt = "8s 4I"
    size = struct.calcsize(fmt)
    return struct.pack(fmt, "NCBIgnld".encode('ascii'), 1, 1, size, 0)


def _makeSimpleEvent(eid):
    """ used for { evt_end_stream, evt_open_stream, evt_next_row } """
    return struct.pack("I", eid)


def _make1StringEvent(eid, str1):
    """ used for { evt_errmsg, evt_remote_path, evt_new_table } """
    fmt = _paddedFormat("I 1I {}s".format(len(str1)))
    return struct.pack(fmt, eid, len(str1), str1)


def _make2StringEvent(eid, str1, str2):
    """ used for { evt_use_schema } """
    fmt = _paddedFormat("I 2I {}s {}s".format(len(str1), len(str2)))
    return struct.pack(fmt, eid, len(str1), len(str2), str1, str2)


def _makeColumnEvent(colid, tblid, bits, name):
    """ used for { evt_new_column } """
    fmt = _paddedFormat("I 3I {}s".format(len(name)))
    return struct.pack(fmt, colid, tblid, bits, len(name), name)


def _makeDataEvent(colid, count):
    """ used for { evt_cell_default, evt_cell_data } """
    return struct.pack("2I", colid, count)


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


    def write(self, spec):
        tableId = -1
        for c in spec.values():
            if 'data' in c:
                data = c['data']
                try:
                    data = data()
                except:
                    pass
                try:
                    self._writeColumnData(c['_columnId'], len(data), data)
                except:
                    sys.stderr.write("failed to write column #{}\n".format(c['_columnId']))
                    raise
            tableId = c['_tableId']
        self._writeNextRow(tableId)

    @classmethod
    def _writeHeader(cls, remoteDb, schemaFileName, schemaDbSpec):
        os.write(sys.stdout.fileno(), _makeHeader())
        os.write(sys.stdout.fileno(), _make1StringEvent(cls.evt_remote_path, remoteDb))
        os.write(sys.stdout.fileno(), _make2StringEvent(cls.evt_use_schema, schemaFileName, schemaDbSpec))


    @classmethod
    def _writeEndStream(cls):
        os.write(sys.stdout.fileno(), _makeSimpleEvent(cls.evt_end_stream))


    @classmethod
    def _writeNewTable(cls, tableId, table):
        os.write(sys.stdout.fileno(), _make1StringEvent(cls.evt_new_table + tableId, table))


    @classmethod
    def _writeNewColumn(cls, columnId, tableId, bits, column):
        os.write(sys.stdout.fileno(), _makeColumnEvent(cls.evt_new_column + columnId, tableId, bits, column))


    @classmethod
    def _writeOpenStream(cls):
        os.write(sys.stdout.fileno(), _makeSimpleEvent(cls.evt_open_stream))
        
    
    @classmethod
    def _writeColumnDefault(cls, colId, count, data):
        l = os.write(sys.stdout.fileno(), _makeDataEvent(cls.evt_cell_default + colId, count))
        l = (l + os.write(sys.stdout.fileno(), data)) % 4
        if l != 0:
            os.write(sys.stdout.fileno(), struct.pack("%dx" % (4 - l)))


    @classmethod
    def _writeColumnData(cls, colId, count, data):
        l = os.write(sys.stdout.fileno(), _makeDataEvent(cls.evt_cell_data + colId, count))
        l = (l + os.write(sys.stdout.fileno(), data)) % 4
        if l != 0:
            os.write(sys.stdout.fileno(), struct.pack("%dx" % (4 - l)))


    @classmethod
    def _writeNextRow(cls, tableId):
        os.write(sys.stdout.fileno(), _makeSimpleEvent(cls.evt_next_row + tableId))
        
    
    def __init__(self, fileName, schemaFileName, schemaDbSpec, tbl):
        """ Construct a General Writer object
    
            writer may be None if no actual output is desired
                else writer is expected to have a callable write attribute
            
            fileName is a string with name of the database that will be created

            schemaFileName is a string with the path to the file containing the schema

            schemaDbSpec is a string with the schema name of the database that will be created
        """
        
        GeneralWriter._writeHeader(fileName.encode('utf-8')
            , schemaFileName.encode('utf-8')
            , schemaDbSpec.encode('ascii'))

        tableId = 0
        columnId = 0
        for t in tbl:
            tableId = tableId + 1
            GeneralWriter._writeNewTable(tableId, t.encode('ascii'))
            cols = tbl[t]
            for c in cols:
                columnId = columnId + 1
                cols[c]['_tableId'] = tableId
                cols[c]['_columnId'] = columnId
                expression = cols[c]['expression'] if 'expression' in cols[c] else c
                bits = cols[c]['elem_bits']
                GeneralWriter._writeNewColumn(columnId, tableId, bits, expression.encode('ascii'))

        GeneralWriter._writeOpenStream()
        for t in tbl.values():
            for c in t.values():
                if 'default' in c:
                    try:
                        GeneralWriter._writeColumnDefault(c['_columnId'], len(c['default']), c['default'])
                    except:
                        sys.stderr.write("failed to set default for %s\n" % c)
                        raise


    def __del__(self):
        try: GeneralWriter._writeEndStream()
        except: pass


if __name__ == "__main__":
    readData = [ "ACGT", "GTAACGT" ]
    row = 0
    
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
                'default': array.array('I', [ 0 ])
            },
            'LABEL_LENGTH': {
                'elem_bits': 32,
                'default': array.array('I', [ 2 ])
            }
        },
    }
    gw = GeneralWriter(sys.argv[1], sys.argv[2], sys.argv[3], spec)
    
    spec['SEQUENCE']['READ_LENGTH']['data'] = array.array('I', [ 1, 2 ])
    gw.write(spec['SEQUENCE'])
    gw.write(spec['SEQUENCE'])

    gw = None

