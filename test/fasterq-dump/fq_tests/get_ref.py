#! /usr/bin/env python3

import sys, os.path, sqlite3

def perform( db, key ) :
	con = sqlite3.connect( db )
	cur = con.cursor()
	# Create table ( in case it does not exist yet )
	cur.execute( "CREATE TABLE IF NOT EXISTS value_tbl ( key_col TEXT NOT NULL PRIMARY KEY, value_col TEXT )" )
	con.commit()
	# Get the value of a key
	rows = cur.execute( f"SELECT value_col FROM value_tbl WHERE key_col = ?", ( key, ) ).fetchall()
	if len( rows ) > 0 :
		print( rows[ 0 ][ 0 ] )

if __name__ == '__main__':
	sys.tracebacklimit = 0
	if len( sys.argv ) > 2 :
		db = sys.argv[ 1 ]
		key = sys.argv[ 2 ]
		try :
			perform( db, key )
			sys.exit( 0 )
		except sqlite3.Error as e:
			print( "An error occurred:", e.args[ 0 ] )
			sys.exit( 3 )
	else :
		print( "usage: get_ref.py database key" )
		sys.exit( 3 )
