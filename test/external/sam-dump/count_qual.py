#!/usr/bin/env python3
import sys

#this simple script counts how many quality values are "*"
#and how many of them are not

#the input is expected to be in SAM-format from STDIN

with_qual = 0
without_qual = 0

for line in sys.stdin :
	l = line.strip()
	if not l.startswith( '@' ) :
		a = l.split( '\t' )
		if len( a ) > 10 :
			if a[ 10 ] == '*' :
				without_qual += 1
			else :
				with_qual +=1
		else :
			without_qual += 1

print( f"{with_qual}\t{without_qual}" )
