#!/bin/bash

progname=`basename $0`
dir=$1
[ ! -d "$dir" ] && { echo "${progname}: Input directory '${dir}' does not exist (error:1)"; exit 1; }
cd  $dir || exit 1
bam_param=`cat bam_param`
bam_param="${bam_param%\\n}" # chomp


is_cram=0

bam_list=(`ls -S *.bam 2>/dev/null`)
if [ ${#bam_list[@]} -eq 0 ]; then
  cram_list=(`ls -S *.cram 2>/dev/null`)
  if [ ${#cram_list[@]} -eq 0 ]; then
      echo "No Input files"
      exit 1
  else
    cram=${cram_list[0]}
    is_cram=1   
  fi
else  
  for bam in ${bam_list[*]}
  do
    bams="${bams} ${bam}"
  done
fi


if [[ -f "refseq.fa" && ! -f "refseq.seqnames.fa" ]]; then
  `ln -s refseq.fa refseq.seqnames.fa` || exit 1
fi


output="output.new"
if [ -d "$output" ]; then
  [ -d "$output/vdb.out" ] && rm -r ${output}/vdb.out
  dt=$(date '+%d_%m_%Y_%H:%M:%S');
  mv ${output} "${output}.${dt}" || { echo "Failed to rename output directory"; exit 1; }
fi
  mkdir ${output} || { echo "Test directory already exists"; exit 1; }

cmd_prefix="LD_PRELOAD=../libjemalloc.so.2 /usr/bin/time -f '%E, %M' -o ${output}/time.log"  

if [ $is_cram -eq 1 ]; then
  LD_PRELOAD=../libjemalloc.so.2 /usr/bin/time -f '%E, %M' -o ${output}/time.log samtools view -h ${cram} | ../bam-load.3.0.1 ${bam_param} --extra-logging -o ${output}/vdb.out /dev/stdin 1> ${output}/new.stdout.log 2> ${output}/new.stderr.log
else
  LD_PRELOAD=../libjemalloc.so.2 /usr/bin/time -f '%E, %M' -o ${output}/time.log ../bam-load.3.0.1 ${bam_param} --extra-logging -o ${output}/vdb.out ${bams} 1> ${output}/new.stdout.log 2> ${output}/new.stderr.log
fi

../sra-stat.2.9.1 --xml --statistics ${output}/vdb.out 1>${output}/new.stats.stdout 2>${output}/new.stats.stderr
../stat_diff.sh sra.stats.stdout ${output}/new.stats.stdout
cd ..

exit 0
