import os
import os.path
import shutil
import filecmp

## Deleting file
##
def deleteFile ( path ):
    print ( "# Deleting file [" + path + "]" )
    if os.path.exists ( path ):
        os.remove ( path )

def deleteDir ( path ):
    print ( "# Deleting directory [" + path + "]" )
    if os.path.exists ( path ):
        shutil.rmtree ( path )

## Checks if file exists and it's file is correct
## Checks if file exists and it's file is correct
##
def checkFileSize ( path, size ):
    print ( "# Checking size (" + str ( size ) + ") for file [" + path + "]" );
    if not os.path.exists ( path ):
        raise Exception ( "File does not exists [" + path + "]" )

    new_size = os.stat ( path ).st_size
    if new_size != size:
        raise Exception ( "Invalid file size " + str ( new_size ) + " for file [" + path + "] had to " + str ( size ) )

## Creates file
##
def createFile ( path, size ):
    print ( "# Writing " + str ( size ) + " bytes to file [" + path + "]" )
    deleteFile ( path )
    file = open ( path, "w" )
    arr="0123456789abcef"
    arr_size = len ( arr )
    acc_size = 0
    while ( acc_size < size ):
        size_to_write = size - acc_size
        if arr_size < size_to_write :
            size_to_write = arr_size
        file.write ( arr [ : size_to_write ] ) 
        acc_size += size_to_write;
    file.close ()

## Copies file
##
def copyFile ( src, dst ):
    print ( "# Copy file [" + src + "] to [" + dst + "]" )
    deleteFile ( dst )
    shutil.copyfile ( src, dst )

## Compare files
##
def compareFiles ( file1, file2 ):
    print ( "# Compareing file [" + file1 + "] and [" + file2 + "]" )
    filecmp.cmp ( file1, file2 )

## Creates directory
##
def createDirectory ( path ):
    print ( "# Creating directory [" + path + "]" )
    os.makedirs ( path )

## Listing directory content
##
def listDirectory ( path, sep = "" ):
    if not len ( sep ):
        print ( "# Listing of directory [" + path + "]" )
    if os.path.exists ( path ):
        for name in os.listdir ( path ):
            new_path = path + os.sep + name
            if os.path.isdir ( new_path ):
                print ( sep + "[" + name + "][/]")
                listDirectory ( new_path, sep + "   " )
            elif os.path.isfile ( new_path ):
                print ( sep + "[" + name + "]")



