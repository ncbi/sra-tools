import sys

inputfilename = "./lookup.csv"
inputfile = open( inputfilename, "r" )
for line in inputfile :
    c = line.strip().split( "," )
    if len( c ) == 6 :
        refname = c[ 0 ].replace( "'", "" )
        refpos = c[ 1 ]
        dels = c[ 2 ]
        bases = c[ 3 ].replace( "'", "" )
        rs = c[ 4 ]
        flags = c[ 5 ] 
        print( "%s,%s,%s,%s,%s,%s"%( refname, refpos, dels, bases, rs, flags ) )
