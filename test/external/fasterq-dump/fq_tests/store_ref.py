#! /usr/bin/env python3

import sys, sqlite3

def perform( db, key, value ) :
	con = sqlite3.connect( db )
	cur = con.cursor()
	# Create table
	cur.execute( "CREATE TABLE IF NOT EXISTS value_tbl ( key_col TEXT NOT NULL PRIMARY KEY, value_col TEXT )" )
	# Insert a row of data
	cur.execute( f"INSERT OR REPLACE INTO value_tbl VALUES ( ?, ? )", ( key, value ) )
	# Save (commit) the changes
	con.commit()
	# We can also close the connection if we are done with it.
	# Just be sure any changes have been committed or they will be lost.
	con.close()

if __name__ == '__main__':
	if len( sys.argv ) > 3 :
		db = sys.argv[ 1 ]
		key = sys.argv[ 2 ]
		value = sys.argv[ 3 ]
		print( f"storing value:'{value}' in key:'{key}' in db:'{db}'" )
		try :
			perform( db, key, value )
			sys.exit( 0 )
		except sqlite3.Error as e:
			print( "An error occurred:", e.args[ 0 ] )
			sys.exit( 3 )
	else :
		print( "usage: store_ref.py database key value" )
		sys.exit( 3 )
