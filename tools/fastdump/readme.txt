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

Dont forget to repeat the commands at least 2 times, to exclude other influences
like caching or network load.

To detect how many cpu-cores your machine has:

on Linux:   $nproc --alll
on Mac:     $/usr/sbin/sysctl -n hw.ncpu

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
                            for spots having only 1 fragment, the fragment is
                            written into the .3 file
                            --split-3 ( -3 )

It is possible that you exhaust the space at your filesystem while converting
large accessions. This can happen with this tool more often because it uses
additional scratch-space to increase speed. It is a good idea to perform some
simple checks before you perform the conversion. First you should know how big
an accession is. Let us use the accession SRR341578 as an example:

$vdb-dump --info SRR341578

will give you a lot of information about this accession. The important line is
the 3rd one: 'size   : 932,308,473'. After running the tool without any other
options you will have a fastq-file in your current directory: SRR341578.fastq.
Its size will be 3,634,513,876. In this case we have inflated the accession by
a factor of ~ 3.9. But that is not all, the tool will need aproximately the same
amout as scratch-space. As a rule of thumb you should have about 8x ... 10x the
size of the accession available on your filesystem. How do you know how much
space is available? Just run this command on linux or mac:

$df -h .

Under the 4th column ( 'Avail' ), you see the amount of space you have available.

Filesystem                   Size  Used Avail Use% Mounted on
server:/vol/export/user       20G   15G  5.9G  71% /home/user

This user has only 5.9 Gigabyte available. In this case there is not enough
space available in its home directory. Either try to delete files, or perform
the conversion to a different location with more space.

If you want to use for instance RAM as scratch-space:

$df -h /dev/shm

If you have enough space there, run the tool:
$fasterq-dump SRR341578 -t /dev/shm

In order to give you some information about the progress of the conversion
there is a progress-bar that can be activated.

$fasterq-dump SRR341578 -t /dev/shm -p

The conversion happens in multiple steps, depending on the internal type of
the accession. You will see either 2 or 3 progressbars after each other.
The full output with progress-bars for a cSRA-accession like SRR341578 looks
like this:

lookup :|-------------------------------------------------- 100.00%
merge  : 13255208
join   :|-------------------------------------------------- 100.00%
concat :|-------------------------------------------------- 100.00%
spots read          : 6,143,624
fragments read      : 12,287,248
fragments written   : 12,287,248

for a flat table like SRR000001 it looks like this:

join   :|-------------------------------------------------- 100.00%
concat :|-------------------------------------------------- 100.00%
spots read          : 470,985
fragments read      : 470,985
fragments written   : 470,985


