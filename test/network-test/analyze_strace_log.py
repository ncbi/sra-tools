import os, sys

def extract_port( value ) :
    res = 0
    open_par = value.find( '(' )
    close_par = value.rfind( ')' )
    if close_par > open_par :
        try :
            res = int( value[ open_par + 1 : close_par ] )
        except :
            self.socket = 0
    
class logline :
    def __init__( self, line, line_nr ) :
        self.line = line
        self.line_nr = line_nr
        self.valid = 0
        a = line.strip().split( "=" )
        if len( a ) > 1 :
            try :
                b = a[ len( a ) - 1 ].strip().split( " " )
                self.result = int( b[ 0 ].strip() )
                open_par = line.find( '(' )
                if open_par > 0 :
                    self.name = line[ 0 : open_par ]
                    close_par = line.rfind( ')' )
                    if close_par > open_par :
                        tmp = line[ open_par + 1 : close_par ].split( "," )
                        self.params = [ ( x.strip() ) for x in tmp ]
                        if len( self.params ) > 0 :
                            try :
                                self.socket = int( self.params[ 0 ] )
                            except :
                                self.socket = 0
                            self.valid = 1
            except :
                pass

    def __str__( self ) :
        if self.valid > 0 :
            return "#%d : valid %d, name '%s', result %d, socket %d"%( self.line_nr, self.valid, self.name, self.result, self.socket )
        return "#%d invalid: '%s'"%( self.line )
    
class connection :
    def __init__( self, socket ) :
        self.socket = socket
        self.status = 0
        self.receive_lines = 0
        self.send_lines = 0
        print "open socket #%d"%( socket )
        
    def handle( self, ll ) :
        if ll.name == 'connect' :
            self.connect( ll )
        elif ll.name == 'sendto' :
            self.sendto( ll )
        elif ll.name == 'recvfrom' :
            self.recvfrom( ll )
        elif ll.name == 'shutdown' :
            self.shutdown( ll )

    def connect( self, ll ) :
        print "#%d connect: '%s'"%( self.socket, ll.params )
        
    def sendto( self, ll ) :
        if self.send_lines == 0 :
            print "#%d send: '%s'"%( self.socket, ll.params[ 1 ] )
        self.send_lines += 1
        
    def recvfrom( self, ll ) :
        if self.receive_lines == 0 :
            print "#%d receive: '%s'"%( self.socket, ll.params[ 1 ] )
        self.receive_lines += 1

    def shutdown( self, ll ) :
        self.status = -1
        

def make_connection( line ) :
    res = None
    rest_of_line = line[6:]
    a = rest_of_line.strip().split( "=" )
    b = a[ 0 ].strip()[1:-1].split( "," )
    socket_id = int( a[ 1 ].strip() )
    if b[ 0 ] == 'PF_INET' :
        res = connection( int( a[ 1 ].strip() ) )
    return res

def extract_id( line ) :
    res = line.find( '(' )
    if res < 0 :
        return res
    comma = line.find( ',' )
    if comma < 0 :
        return comma
    tmp = line[ res + 1 : comma ]
    return int( tmp.strip() )


def analyze( logfilename ) :
    print "analyzing file: '" + logfilename + "'"
    connections = {}
    f = open( logfilename )
    line_nr = 0
    for line in iter( f ) :
        ll = logline( line, line_nr )
        if ll.valid > 0 :
            if ll.name == 'socket' :
                if ll.params[ 0 ] == 'PF_INET' :
                    s = ll.result
                    connections[ s ] = connection( s )
            else :
                try :
                    s = connections[ ll.socket ]
                    s.handle( ll )
                    if s.status < 0 :
                        del connections[ ll.socket ]
                        print "closed #%d"%( ll.socket )
                except :
                    pass
        line_nr += 1
    f.close

if __name__ == '__main__':
    if len( sys.argv ) > 1 :
        analyze( sys.argv[ 1 ] )
    else :
        analyze( 'strace.log' )
