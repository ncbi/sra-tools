acc="SRR341578"
numthreads="6"
scratch="/dev/shm/raetzw_tmp"
out="/dev/shm/raetzw_out/$acc.fastq"

md5="66b97c245a2c605a8854967b454d789e"

rm -rf $out
time fastdump $acc -e $numthreads -x -p --split -t $scratch -o $out -f
#totalview fastdump -a $acc -e $numthreads -x -p --split -t $scratch -o $out -f
md5sum $out
echo $md5
