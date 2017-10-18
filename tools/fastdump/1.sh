acc="SRR341578"
numthreads="-e 6"
scratch="-t /dev/shm/raetzw_tmp"
outfile="/dev/shm/raetzw_out/$acc.fastq"
out="-o $outfile"
mem_limit="-m 50M"
cur_cache="-c 5M"

# -p ... show progress-bar
# -f ... force to overwrite output-file
# -x ... print details of actions
# -s ... split fastq
# -e n      ... number of threads to use
# -t xxx    ... path for temp. files
# -o xxx    ... output-file
# -m n      ... memory-limit for merge-sort
# -c n      ... cursor-cache per thread

md5="66b97c245a2c605a8854967b454d789e"

rm -rf $outfile
time fastdump $acc -p -f -x -s $numthreads $mem_limit $cur_cache $scratch $out
#totalview fastdump -a $acc -e $numthreads -x -p --split -t $scratch -o $out -f
md5sum $outfile
echo $md5
rm -rf $outfile
