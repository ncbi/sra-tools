-------------------------------------------------------------------------------
        fasterq-dump - a faster fastq-dump
-------------------------------------------------------------------------------

The fasterq-dump tool uses temporary files and multi-threading to speed up the
extraction of FASTQ from SRA-accessions. If a minimal commandline is given:

$fasterq-dump SRR000001

The tool produces an output file called SRR000001.fastq in the current
directory. The tool will also create a directory named 'fast.tmp' in the
current directory and use it for temporary files. After the extraction is
finished, this directory and its content will be deleted. This temporary
directory will use approximately the size of the final output-file. If you do
not have enough space in your current directory for the output-file and the
temporary files, the tool will fail.

The location and name of the output-file can be changed:

$fasterq-dump SRR000001 -o /big_hdd/SRR000001.fastq

Notice that path and filename has to be given! If parts of the output-path
do not exist, they will be created. If the output-file already exists, the
tool will not overwrite it, but fail instead. If you want an already existing
output-file to be overwritten, use the force option '-f'.

The location of the temporary directory can be changed too:

$fasterq-dump SRR000001 -o /big_hdd/SRR000001.fastq -t /scratch

Now the temporary files will be created in the '/scratch' directory. These
temporary files will be deleted on finish, but the directory itself will not
be deleted. If the temporary directory does not exist, it will be created.

It is helpfull for the speed-up, if the output-path and the scratch-path are
on different file-systems. For instance it is a good idea to point the temporary
directory to a SSD if available or a RAM-disk like '/dev/shm' if enough RAM
is available.

Another factor is the number of threads. If no option is given (as above) the
tool uses 6 threads for its work. If you have more CPU cores it might help to
increase this number. The option to do this is for instance '-e 8' to increase
the thread-count to 8. However even if you have a computer with much more
CPU cores, increasing the thread count can lead to diminishing returns, because
you exhaust the I/O - bandwidth. You can test your speed by measuring how long
it takes to convert a small accession, like this:

$time fasterq-dump  SRR000001 -t /dev/shm
$time fasterq-dump  SRR000001 -t /dev/shm -e 8
$time fasterq-dump  SRR000001 -t /dev/shm -e 10

Dont forget to repeat the first command at least 2 times, to prime
the filesystem cache.

The tool can create different formats:

(1) FASTQ               ... the spots are not split,
                            for each spot - 4 lines of FASTQ are written
                            into one output-file
                            default ( no option neccessary )

(2) FASTQ split spot    ... the spots are split into fragments,
                            for each fragments - 4 lines of FASTQ are written
                            into one output-file
                            --split-spot ( -s )
                            
(3) FASTQ split file    ... the spots are split into fragments,
                            for each fragments - 4 lines of FASTQ are written
                            each n-th fragment into a different file
                            --split-file ( -S )
                            
(4) FASTQ split 3       ... the spots are split into fragments,
                            for each fragments - 4 lines of FASTQ are written
                            for spots having 2 fragments, the fragments are
                            written into the .1 and .2 files
                            spots having only 1 fragment, the fragment is
                            written into the .3 file
                            --split-3 ( -3 )

