Here is an example how to use the new fastdump-tool:

The usage is in 2 stages:
( stage 1 ) create a lookup-file from the accession
        SRRXXXXXX ---> SRRXXXXXX.lookup
        
( stage 2 ) create the final output from the accession using the lookupfile
        SRRXXXXXX + SRRXXXXXX.lookup ---> SRRXXXXXX.txt


example: accession = SRR833540

================================================================================
    stage 1:
================================================================================

(version a)
create the lookup-file in the current directory, with memory limit of 4 GB

fastdump SRR833540 -f lookup -o SRR833540.lookup -m 4G -p

This will create a number of temporary files in your current directory.
Make shure you have enough space for that. The lookup-file for this accession
(SRR833540.lookup) will be about 121 GB in size. You will need double that space
because of the temporary files. The tool will delete them after it created the
lookup-file. How much memory should you give to the tool? Look at your
available memory with 'free -h'. Give it about half as much as your free memory.
You can give it more, but that will result in memory beeing swaped and that will
result in a big slow down. One of our machines took about 500 minutes for this,
without swapping. The '-p' switch turns a percent-bar on.



(version b)
create the lookup-file in the current directory, with memory limit of 4 GB on 6 threads

fastdump SRR833540 -f lookup -o SRR833540.lookup -m 4G -e 6 -p

This will create the same output, but much faster. But now you are using 4 GB on
each of the 6 threads. You will need more than 4 GB * 6 = 24 GB, you will need
about 35 GB of memory because other parts of the tool need memory too. If you do not
have that much memory, reduce the amount of memory per thread or the number of threads
until it fits your machine. You can check how much is actually used with 
'top -u your_username'. If you specify 6 threads, you should see about 600% of
CPU utilization. If you see less than that you are limited by the speed of the
filesystem access. Make shure that SRR833540 is local on your filesystem and all
the references it uses are locally accessible too. This took us about 110 minutes.


How do you know the accession is local?

'vdb-dump SRR833540 --info'

If the path points to your local filesystem ( '/home/user/ncbi/...' etc. )
    ---> you are good to go.

If the path points to a remote url ( 'http://sra-download.ncbi...' etc. )
    ---> download the accession first, with the prefetch-tool.

How do you know that you have all references locally?

'sra-pileup SRR833540 --function ref'

This will list all references used by the accession.
If the location points to your local filesystem, your are good to go. If the location
points to a remote url, download the references with the prefetch-tool.

If after prefetch the accession is still not found locally, you have a configuration issue.

If you have a SSD available, that helps too!

================================================================================
    stage 2:
================================================================================

(version a)
create the lookup-file in the current directory into a file with percent-bar

fastdump SRR833540 -l SRR833540.lookup -o SRR833540.txt -p


(version b)
create the output on stdout ( to be piped into other tools )

fastdump SRR833540 -l SRR833540.lookup

The output will be in this text-format:
ID<tab>READ<tab>SPOTGROUP

If you want FASTQ instead, add the option '-f fastq'.


