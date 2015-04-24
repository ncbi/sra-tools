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


class GeneralWriter:

    GW_SIGNATURE        = "NCBIgnld".encode('ascii')
    GW_GOOD_ENDIAN      = 1
    GW_CURRENT_VERSION  = 1

    evt_bad_event       = 0
    evt_end_stream      = (1 << 24) + evt_bad_event
    evt_new_table       = (1 << 24) + evt_end_stream
    evt_new_column      = (1 << 24) + evt_new_table
    evt_open_stream     = (1 << 24) + evt_new_column
    evt_cell_default    = (1 << 24) + evt_open_stream
    evt_cell_data       = (1 << 24) + evt_cell_default
    evt_next_row        = (1 << 24) + evt_cell_data
    evt_errmsg          = (1 << 24) + evt_next_row

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
                    self._writeColumnData(c['_columnId'], c['elem_bits'], len(data), data)
                except:
                    sys.stderr.write("failed to write column #{}\n".format(c['_columnId']))
                    raise
            tableId = c['_tableId']
        self._writeNextRow(tableId)

   
    def _writeHeader(self):
        fmt = _paddedFormat("8s 5I %ds %ds %ds" % (
            len(self.remoteDb) + 1,
            len(self.schemaFileName) + 1,
            len(self.schemaDbSpec) + 1))
        data = struct.pack(fmt,
            self.GW_SIGNATURE,
            self.GW_GOOD_ENDIAN,
            self.GW_CURRENT_VERSION,
            len(self.remoteDb),
            len(self.schemaFileName),
            len(self.schemaDbSpec),
            self.remoteDb,
            self.schemaFileName,
            self.schemaDbSpec)
        os.write(sys.stdout.fileno(), data)


    def _writeEndStream(self):
        os.write(sys.stdout.fileno(), struct.pack("I", self.evt_end_stream))


    def _writeNewTable(self, tableId, table):
        fmt = _paddedFormat("I I %ds" % (len(table) + 1))
        os.write(sys.stdout.fileno(), struct.pack(fmt, tableId + self.evt_new_table, len(table), table))


    def _writeNewColumn(self, columnId, tableId, column):
        fmt = _paddedFormat("I I I %ds" % (len(column) + 1))
        os.write(sys.stdout.fileno(), struct.pack(fmt, columnId + self.evt_new_column, tableId, len(column), column))


    def _writeOpenStream(self):
        os.write(sys.stdout.fileno(), struct.pack("I", self.evt_open_stream))
        
    
    def _writeColumnDefault(self, columnId, bits, count, data):
        l = os.write(sys.stdout.fileno(), struct.pack("I I I", columnId + self.evt_cell_default, bits, count))
        l = (l + os.write(sys.stdout.fileno(), data)) % 4
        if l != 0:
            os.write(sys.stdout.fileno(), struct.pack("%dx" % (4 - l)))


    def _writeColumnData(self, columnId, bits, count, data):
        l = os.write(sys.stdout.fileno(), struct.pack("I I I", columnId + self.evt_cell_data, bits, count))
        l = (l + os.write(sys.stdout.fileno(), data)) % 4
        if l != 0:
            os.write(sys.stdout.fileno(), struct.pack("%dx" % (4 - l)))


    def _writeNextRow(self, tableId):
        os.write(sys.stdout.fileno(), struct.pack("I", tableId + self.evt_next_row))
        
    
    def __init__(self, writer, fileName, schemaFileName, schemaDbSpec, tbl):
        """ Construct a General Writer object
    
            writer may be None if no actual output is desired
                else writer is expected to have a callable write attribute
            
            fileName is a string with name of the database that will be created

            schemaFileName is a string with the path to the file containing the schema

            schemaDbSpec is a string with the schema name of the database that will be created
        """
        
        self.remoteDb = fileName.encode('utf-8')
        self.schemaFileName = schemaFileName.encode('utf-8')
        self.schemaDbSpec = schemaDbSpec.encode('ascii')

        self._writeHeader()

        tableId = 0
        columnId = 0
        for t in tbl:
            tableId = tableId + 1
            self._writeNewTable(tableId, t.encode('ascii'))
            cols = tbl[t]
            for c in cols:
                columnId = columnId + 1
                cols[c]['_tableId'] = tableId
                cols[c]['_columnId'] = columnId
                expression = cols[c]['expression'] if 'expression' in cols[c] else c
                self._writeNewColumn(columnId, tableId, expression.encode('ascii'))

        self._writeOpenStream()
        for t in tbl.values():
            for c in t.values():
                if 'default' in c:
                    try:
                        self._writeColumnDefault(c['_columnId'], c['elem_bits'], len(c['default']), c['default'])
                    except:
                        sys.stderr.write("failed to set default for %s\n" % c)
                        raise


    def __del__(self):
        try: self._writeEndStream()
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
    gw = GeneralWriter(None, sys.argv[1], sys.argv[2], sys.argv[3], spec)
    
    spec['SEQUENCE']['READ_LENGTH']['data'] = array.array('I', [ 1, 2 ])
    gw.write(spec['SEQUENCE'])
    gw.write(spec['SEQUENCE'])

    gw = None

