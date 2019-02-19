import os
import sys
import subprocess
import hashlib
import datetime

'''---------------------------------------------------------------------
    calls "vdb-dump ACCESSION --info"
    extracts from the output the line that starts with "remote : ...."
    returns the remote url or None
---------------------------------------------------------------------'''
def get_remote_url( acc ):
    cmd = "vdb-dump %s --info"%( acc )
    try:
        lines = subprocess.check_output( cmd, shell = True ).split( "\n" )
        for line in lines:
            try:
                colon = line.index( ":" )
                if line[ :colon ].strip() == "remote" :
                    url = line[ colon+1: ].strip()
                    if url.startswith( "http:" ) or url.startswith( "https:" ) :
                        return url
            except:
                pass
        for line in lines:
            try:
                colon = line.index( ":" )
                if line[ :colon ].strip() == "path" :
                    url = line[ colon+1: ].strip()
                    if url.startswith( "http:" ) or url.startswith( "https:" ) :
                        return url
            except:
                pass
    except:
        pass
    return None

'''---------------------------------------------------------------------
    calls "kget URL --show-size"
    extracts from the output the line that starts with "file-size : ...."
    returns the value as int or 0
---------------------------------------------------------------------'''
def kget_remote_size( url ): 
    cmd = "kget %s --show-size"%( url )
    try:
        lines = subprocess.check_output( cmd, shell = True ).split( "\n" )
        for line in lines:
            try:
                eq = line.index( "=" )
                if line[ :eq ].strip() == "file-size" :
                    return int( line[ eq+1: ].strip() )
            except:
                pass
    except:
        pass
    return 0

'''---------------------------------------------------------------------
    helper functions to create a md5 or sha256 hash from a file
    ( this way we do not depend on the existence of a md5sum-binary )
---------------------------------------------------------------------'''
def hashfile( afile, hasher, blocksize=65536 ) :
    buf = afile.read( blocksize )
    while len( buf ) > 0 :
        hasher.update( buf )
        buf = afile.read( blocksize )
    return hasher.hexdigest()

def md5( fname ) :
    return hashfile( open( fname, 'rb' ), hashlib.md5() )

def sha256( fname ) :
    return hashfile( open( fname, 'rb' ), hashlib.sha256() )

    
'''---------------------------------------------------------------------
    calls "kget URL"
---------------------------------------------------------------------'''
def kget_download_partial( url, acc ):
    try:
        os.remove( acc )
    except:
        pass

    cmd = "kget %s"%( url )
    try:
        subprocess.check_output( cmd, shell = True )
        return md5( acc )
    except:
        return None

'''---------------------------------------------------------------------
    calls "kget URL --full"
---------------------------------------------------------------------'''
def kget_download_full( url, acc ):
    try:
        os.remove( acc )
    except:
        pass
    cmd = "kget %s --full"%( url )
    try:
        subprocess.check_output( cmd, shell = True )
        return md5( acc )
    except:
        return None


'''---------------------------------------------------------------------
    the expected values
---------------------------------------------------------------------'''
ACC = "NC_011748.1"
EXP_SIZE = 1313200
EXP_MD5 = "cdb959a48206fd335f766e6637993a78"


'''---------------------------------------------------------------------
    main...
---------------------------------------------------------------------'''
print "-" * 80
print "we test download of accession '%s'"%( ACC )

URL = get_remote_url( ACC )
if URL == None :
    print "cannot resolve accession '%s'"%( ACC )
    sys.exit( -1 )

print "'%s' is resolved into '%s'"%( ACC, URL )

remote_size = kget_remote_size( URL )
if remote_size != EXP_SIZE :
    print "size (%d) differs from expected size(%d)"%( remote_size, EXP_SIZE )
    sys.exit( -1 )
else :
    print "size as expected = %d"%( remote_size )

t_start = datetime.datetime.now()
remote_md5 = kget_download_partial( URL, ACC )
t_partial = datetime.datetime.now() - t_start;
if remote_md5 == None :
    print "error downloading '%s'"%( URL )
    sys.exit( -1 )

if remote_md5 != EXP_MD5 :
    print "md5 diff: expected (%s) vs remote (%s)"%( EXP_MD5, remote_md5 )
    sys.exit( -1 )
else :
    print "partial donwload ok in %d ms"%( t_partial.microseconds)

t_start = datetime.datetime.now()
remote_md5 = kget_download_full( URL, ACC )
t_full = datetime.datetime.now() - t_start;
if remote_md5 == None :
    print "error downloading '%s'"%( URL )
    sys.exit( -1 )

if remote_md5 != EXP_MD5 :
    print "md5 diff: expected (%s) vs remote (%s)"%( EXP_MD5, remote_md5 )
    sys.exit( -1 )
else :
    print "full donwload ok in %d ms"%( t_full.microseconds )

'''---------------------------------------------------------------------
if t_full >= t_partial :
    print "timing problem: full download should be faster than partial download"
    sys.exit( -1 )
else :
    print "timing ok: full download is faster than partial download"
---------------------------------------------------------------------'''

try:
    os.remove( ACC )
except:
    pass

print "-" * 80
