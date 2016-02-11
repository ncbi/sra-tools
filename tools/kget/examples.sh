#!/bin/bash

execute()
{
    echo "------------------------------------------------------"
    echo $1
    eval $1
    echo "."
}

ACC="NC_011748.1"
URL=`srapath $ACC`
CACHEFILE="cache.dat"
PROXY="localhost:8080"

echo "example number 1: show the size of the remote file ( HEAD request )"
execute "time kget $URL --show-size"

echo "example number 2: download the remote file, no buffering, no cachefile"
echo "                  in 32k blocks, using HTTPFile, filename extracted from URL"
execute "time kget $URL"

echo "example number 3: download the remote file, wget-style: without partial requests"
echo "                  filename extracted from URL"
execute "time kget $URL --full"

echo "example number 4: download the remote file, no buffering, no cachefile"
echo "                  in 32k blocks, using HTTPFile, filename different from URL"
execute "time kget $URL some_reference.dat"

echo "example number 5: download the remote file, no buffering, no cachefile"
echo "                  in 32k blocks, using ReliableHTTPFile"
execute "time kget $URL some_reference.dat --reliable"

echo "example number 6: download the remote file, no buffering, no cachefile"
echo "                  in 128k blocks, using HTTPFile"
#blocksize of 128 kilobyte can be expressed as '128k' or '131072' or '0x020000'
execute "time kget $URL --block-size 128k"

echo "example number 7: download the remote file, no cachefile"
echo "                  in 32k blocks, wraped in a buffer-file with 128k blocks"
execute "time kget $URL --buffer 128k"

echo "example number 8: download the remote file, using a cache-file "
echo "                  ( dflt cache-blocksize of 128 k ) in 32k blocks, "
execute "rm -f $CACHEFILE"
execute "time kget $URL --cache $CACHEFILE"

echo "example number 9: download the remote file, using a cache-file"
echo "                   ( cache-blocksize of 256 k ) in 32k blocks, "
execute "rm -f $CACHEFILE"
execute "time kget $URL --cache $CACHEFILE --cache-block 256k"

echo "example number 10: download the remote file, no buffering, no cachefile"
echo "                  in 32k blocks, but requests are made in random order"
execute "time kget $URL --random"

#enable this example only after updating the PROXY-variable
#and actually having a running proxy there!
#echo "example number X: download the remote file, using a proxy"
#echo "                  in 32k blocks, "
#execute "time kget $URL --proxy $PROXY"
